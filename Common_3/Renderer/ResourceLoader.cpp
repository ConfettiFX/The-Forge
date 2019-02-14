/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include "IRenderer.h"
#include "ResourceLoader.h"
#include "../OS/Interfaces/ILogManager.h"
#include "../OS/Interfaces/IMemoryManager.h"
#include "../OS/Interfaces/IThread.h"

//this is needed for unix as PATH_MAX is defined instead of MAX_PATH
#ifndef _WIN32
//linux needs limits.h for PATH_MAX
#ifdef __linux__
#include <limits.h>
#endif
#if defined(__ANDROID__)
#include <shaderc/shaderc.h>
#endif
#define MAX_PATH PATH_MAX
#endif

// buffer functions
#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture);
extern void removeTexture(Renderer* pRenderer, Texture* p_texture);
extern void cmdUpdateBuffer(Cmd* p_cmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* p_src_buffer, Buffer* p_buffer);
extern void cmdUpdateSubresources(
	Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate,
	uint64_t intermediateOffset, Texture* pTexture);
extern const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer);
#endif
/************************************************************************/
/************************************************************************/

//////////////////////////////////////////////////////////////////////////
// Resource CopyEngine Structures
//////////////////////////////////////////////////////////////////////////
typedef struct MappedMemoryRange
{
	void*    pData;
	Buffer*  pBuffer;
	uint64_t mOffset;
	uint64_t mSize;
} MappedMemoryRange;

typedef struct ResourceSet
{
	Fence* pFence;
	Cmd* pCmd;
	tinystl::vector<Buffer*> mBuffers;
} CopyResourceSet;

#define NUM_RESOURCE_SETS 2

//Synchronization?
typedef struct CopyEngine
{
	Queue* pQueue;
	CmdPool* pCmdPool;
	ResourceSet resourceSets[NUM_RESOURCE_SETS];
	uint64_t bufferSize;
	uint64_t allocatedSpace;
	bool isRecording;
} CopyEngine;

//////////////////////////////////////////////////////////////////////////
// Resource Loader Internal Functions
//////////////////////////////////////////////////////////////////////////
static void setupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size)
{
	QueueDesc desc = { QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, CMD_POOL_COPY, nodeIndex };
	addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

	addCmdPool(pRenderer, pCopyEngine->pQueue, false, &pCopyEngine->pCmdPool);

	for (auto& resourceSet : pCopyEngine->resourceSets)
	{
		addFence(pRenderer, &resourceSet.pFence);

		addCmd(pCopyEngine->pCmdPool, false, &resourceSet.pCmd);

		resourceSet.mBuffers.resize(1);
		BufferDesc bufferDesc = {};
		bufferDesc.mSize = size;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.mNodeIndex = nodeIndex;
		addBuffer(pRenderer, &bufferDesc, &resourceSet.mBuffers.back());
	}

	pCopyEngine->bufferSize = size;
	pCopyEngine->allocatedSpace = 0;
	pCopyEngine->isRecording = false;
}

static void cleanupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
	for (auto& resourceSet : pCopyEngine->resourceSets)
	{
		for (size_t i = 0; i < resourceSet.mBuffers.size(); ++i)
			removeBuffer(pRenderer, resourceSet.mBuffers[i]);

		removeCmd(pCopyEngine->pCmdPool, resourceSet.pCmd);

		removeFence(pRenderer, resourceSet.pFence);
	}
	
	removeCmdPool(pRenderer, pCopyEngine->pCmdPool);

	removeQueue(pCopyEngine->pQueue);
}

static void waitCopyEngineSet(CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	waitForFences(pCopyEngine->pQueue, 1, &resourceSet.pFence, false);
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	for (size_t i = 1; i < resourceSet.mBuffers.size(); ++i)
		removeBuffer(pRenderer, resourceSet.mBuffers[i]);
	resourceSet.mBuffers.resize(1);
	pCopyEngine->allocatedSpace = 0;
	pCopyEngine->isRecording = false;
}

static Cmd* aquireCmd(CopyEngine* pCopyEngine, size_t activeSet)
{
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	if (!pCopyEngine->isRecording)
	{
		beginCmd(resourceSet.pCmd);
		pCopyEngine->isRecording = true;
	}
	return resourceSet.pCmd;
}

static void streamerFlush(CopyEngine* pCopyEngine, size_t activeSet)
{
	if (pCopyEngine->isRecording)
	{
		ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
		endCmd(resourceSet.pCmd);
		queueSubmit(pCopyEngine->pQueue, 1, &resourceSet.pCmd, resourceSet.pFence, 0, 0, 0, 0);
		pCopyEngine->isRecording = false;
	}
}

static void finish(CopyEngine* pCopyEngine, size_t activeSet)
{
	streamerFlush(pCopyEngine, activeSet);
	// NOTE: should be last submitted set, not previous;
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	waitForFences(pCopyEngine->pQueue, 1, &resourceSet.pFence, false);
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, uint64_t memoryRequirement, uint32_t alignment)
{
	uint64_t offset = pCopyEngine->allocatedSpace;
	if (alignment != 0)
	{
		offset = round_up_64(offset, alignment);
	}

	CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[activeSet];
	uint64_t size = pResourceSet->mBuffers.back()->mDesc.mSize;
	bool memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
	if (!memoryAvailable)
	{
		// Try creating a temporary staging buffer which we will clean up after resource is uploaded
		Buffer* tempStagingBuffer = NULL;
		BufferDesc desc = {};
		desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		desc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		desc.mSize = round_up_64(memoryRequirement, pCopyEngine->bufferSize);
		desc.mNodeIndex = pResourceSet->mBuffers.back()->mDesc.mNodeIndex;
		addBuffer(pRenderer, &desc, &tempStagingBuffer);

		if (tempStagingBuffer)
		{
			pResourceSet->mBuffers.emplace_back(tempStagingBuffer);
			offset = 0;
		}
		else
		{
			LOGERRORF("Failed to allocate memory (%llu) for resource", memoryRequirement);
			return { NULL };
		}
	}

	Buffer* buffer = pResourceSet->mBuffers.back();
#if defined(DIRECT3D11)
	// TODO: do done once, unmap before queue submit
	mapBuffer(pRenderer, buffer, nullptr);
#endif
	void* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
	pCopyEngine->allocatedSpace = offset + memoryRequirement;
	return { pDstData, buffer, offset, memoryRequirement };
}

static ResourceState util_determine_resource_start_state(DescriptorType usage)
{
	ResourceState state = RESOURCE_STATE_UNDEFINED;
	if (usage & DESCRIPTOR_TYPE_RW_TEXTURE)
		return RESOURCE_STATE_UNORDERED_ACCESS;
	else if (usage & DESCRIPTOR_TYPE_TEXTURE)
		return RESOURCE_STATE_SHADER_RESOURCE;
	return state;
}

static ResourceState util_determine_resource_start_state(const BufferDesc* pBuffer)
{
	// Host visible (Upload Heap)
	if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
	{
		return RESOURCE_STATE_GENERIC_READ;
	}
	// Device Local (Default Heap)
	else if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		DescriptorType usage = pBuffer->mDescriptors;

		// Try to limit number of states used overall to avoid sync complexities
		if (usage & DESCRIPTOR_TYPE_RW_BUFFER)
			return RESOURCE_STATE_UNORDERED_ACCESS;
		if ((usage & DESCRIPTOR_TYPE_VERTEX_BUFFER) || (usage & DESCRIPTOR_TYPE_UNIFORM_BUFFER))
			return RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (usage & DESCRIPTOR_TYPE_INDEX_BUFFER)
			return RESOURCE_STATE_INDEX_BUFFER;
		if ((usage & DESCRIPTOR_TYPE_BUFFER))
			return RESOURCE_STATE_SHADER_RESOURCE;

		return RESOURCE_STATE_COMMON;
	}
	// Host Cached (Readback Heap)
	else
	{
		return RESOURCE_STATE_COPY_DEST;
	}
}

struct UploadTextureParams
{
	uint32_t w, h;
	uint32_t stridePitch;
	uint32_t numStrides;
};

#ifndef METAL
static void calculate_upload_texture_params(ImageFormat::Enum fmt, uint32_t w, uint32_t h, UploadTextureParams& params)
{
	switch (ImageFormat::GetBlockSize(fmt))
	{
	case ImageFormat::BLOCK_SIZE_4x4:
		params.w = (w + 3) & 0xFFFFFFFC;
		params.stridePitch = (params.w >> 2) * ImageFormat::GetBytesPerBlock(fmt);
		params.h = (h + 3) & 0xFFFFFFFC;
		params.numStrides = params.h >> 2;
		break;
	case ImageFormat::BLOCK_SIZE_4x8:
		params.w = (w + 7) & 0xFFFFFFF8;
		params.stridePitch = (params.w >> 3) * ImageFormat::GetBytesPerBlock(fmt);
		params.h = (h + 3) & 0xFFFFFFFC;
		params.numStrides = params.h >> 2;
		break;
	default:
		params.w = w;
		params.h = h;
		params.stridePitch = params.w * ImageFormat::GetBytesPerPixel(fmt);
		params.numStrides = h;
		break;
	};
}
#endif

static void updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, TextureUpdateDesc* pTextureUpdate)
{
#if defined(METAL)
	const Image& img = *pTextureUpdate->pImage;
	Texture*     pTexture = pTextureUpdate->pTexture;
	Cmd*         pCmd = aquireCmd(pCopyEngine, activeSet);

	ASSERT(pTexture);

	uint32_t          textureAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
	MappedMemoryRange range = allocateStagingMemory(pRenderer, pCopyEngine, activeSet, pTexture->mTextureSize, textureAlignment);

	ASSERT(pTexture);

	// create source subres data structs
	SubresourceDataDesc  texData[1024];
	SubresourceDataDesc* dest = texData;
	uint                 nSlices = img.IsCube() ? 6 : 1;

	for (uint32_t n = 0; n < img.GetArrayCount(); ++n)
	{
		for (uint32_t k = 0; k < nSlices; ++k)
		{
			for (uint32_t i = 0; i < img.GetMipMapCount(); ++i)
			{
				uint32_t pitch, slicePitch;
				if (ImageFormat::IsCompressedFormat(img.getFormat()))
				{
					pitch = ((img.GetWidth(i) + 3) >> 2) * ImageFormat::GetBytesPerBlock(img.getFormat());
					slicePitch = pitch * ((img.GetHeight(i) + 3) >> 2);
				}
				else
				{
					pitch = img.GetWidth(i) * ImageFormat::GetBytesPerPixel(img.getFormat());
					slicePitch = pitch * img.GetHeight(i);
				}

				dest->pData = img.GetPixels(i, n) + k * slicePitch;
				dest->mRowPitch = pitch;
				dest->mSlicePitch = slicePitch;
				++dest;
			}
		}
	}

	// calculate number of subresources
	int numSubresources = (int)(dest - texData);
	cmdUpdateSubresources(pCmd, 0, numSubresources, texData, range.pBuffer, range.mOffset, pTexture);
#else
	ASSERT(pCopyEngine->pQueue->mQueueDesc.mNodeIndex == pTextureUpdate->pTexture->mDesc.mNodeIndex);
	bool         applyBarrieers = pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12;
	const Image& img = *pTextureUpdate->pImage;
	Texture*     pTexture = pTextureUpdate->pTexture;
	Cmd*         pCmd = aquireCmd(pCopyEngine, activeSet);

	ASSERT(pTexture);

	uint32_t  textureAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
	uint32_t  textureRowAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureRowAlignment;

	uint32_t          nSlices = img.IsCube() ? 6 : 1;
	uint32_t          arrayCount = img.GetArrayCount() * nSlices;
	uint32_t          numSubresources = arrayCount * img.GetMipMapCount();
	MappedMemoryRange range = allocateStagingMemory(
		pRenderer, pCopyEngine, activeSet, pTexture->mTextureSize + textureAlignment * numSubresources, textureAlignment);
	// create source subres data structs
	SubresourceDataDesc  texData[1024];
	SubresourceDataDesc* dest = texData;

	uint32_t offset = 0;
	for (uint32_t i = 0; i < img.GetMipMapCount(); ++i)
	{
		UploadTextureParams uploadParams;
		calculate_upload_texture_params(img.getFormat(), img.GetWidth(i), img.GetHeight(i), uploadParams);
		uint32_t bufferStridePitch = round_up(uploadParams.stridePitch, textureRowAlignment);
		uint32_t slicePitch = uploadParams.stridePitch * uploadParams.numStrides;

		for (uint32_t j = 0; j < arrayCount; ++j)
		{
			dest->mArrayLayer = j /*n * nSlices + k*/;
			dest->mMipLevel = i;
			dest->mBufferOffset = range.mOffset + offset;
			dest->mWidth = uploadParams.w;
			dest->mHeight = uploadParams.h;
			dest->mDepth = img.GetDepth(i);
			dest->mRowPitch = bufferStridePitch;
			dest->mSlicePitch = bufferStridePitch * uploadParams.numStrides;
			++dest;

			uint32_t n = j / nSlices;
			uint32_t k = j - n * nSlices;
			uint8_t* pSrcData = (uint8_t*)img.GetPixels(i, n) + k * slicePitch;
			uint8_t* pSrcDataEnd = pSrcData + slicePitch * img.GetDepth(i);
			while (pSrcData < pSrcDataEnd)
			{
				memcpy((uint8_t*)range.pData + offset, pSrcData, uploadParams.stridePitch);
				pSrcData += uploadParams.stridePitch;
				offset += bufferStridePitch;
			}
			offset = round_up(offset, textureAlignment);
		}
	}

#if defined(DIRECT3D11)
	unmapBuffer(pRenderer, range.pBuffer);
#endif

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
	if (applyBarrieers)
	{
		TextureBarrier preCopyBarrier = { pTexture, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier, false);
	}

	cmdUpdateSubresources(pCmd, 0, numSubresources, texData, range.pBuffer, 0, pTexture);

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
	if (applyBarrieers)
	{
		TextureBarrier postCopyBarrier = { pTexture, util_determine_resource_start_state(pTexture->mDesc.mDescriptors) };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier, true);
	}
#endif
	
	if (pTextureUpdate->freeImage)
	{
		pTextureUpdate->pImage->Destroy();
		conf_delete(pTextureUpdate->pImage);
	}
}

static void updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, BufferUpdateDesc* pBufferUpdate)
{
	ASSERT(pCopyEngine->pQueue->mQueueDesc.mNodeIndex == pBufferUpdate->pBuffer->mDesc.mNodeIndex);
	Buffer* pBuffer = pBufferUpdate->pBuffer;
	// TODO: remove uniform buffer alignment?
	const uint64_t bufferSize = (pBufferUpdate->mSize > 0) ? pBufferUpdate->mSize : pBuffer->mDesc.mSize;
	const uint64_t alignment = pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER ? pRenderer->pActiveGpuSettings->mUniformBufferAlignment : 1;
	const uint64_t offset = round_up_64(pBufferUpdate->mDstOffset, alignment);

	void* pSrcBufferAddress = NULL;
	if (pBufferUpdate->pData)
		//calculate address based on progress
		pSrcBufferAddress = (uint8_t*)(pBufferUpdate->pData) + pBufferUpdate->mSrcOffset;

	ASSERT(pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);
	Cmd* pCmd = aquireCmd(pCopyEngine, activeSet);
	//calculate remaining size
	MappedMemoryRange range = allocateStagingMemory(pRenderer, pCopyEngine, activeSet, bufferSize, RESOURCE_BUFFER_ALIGNMENT);
	//calculate upload size
	//calculate partial offset
	//update progress
	ASSERT(range.pData);

	if (pSrcBufferAddress)
		memcpy(range.pData, pSrcBufferAddress, range.mSize);
	else
		memset(range.pData, 0, range.mSize);

	cmdUpdateBuffer(pCmd, range.mOffset, pBuffer->mPositionInHeap + offset,
		bufferSize, range.pBuffer, pBuffer);
#if defined(DIRECT3D11)
	unmapBuffer(pRenderer, range.pBuffer);
#endif

	ResourceState state = util_determine_resource_start_state(&pBuffer->mDesc);
#ifdef _DURANGO
	// XBox One needs explicit resource transitions
	BufferBarrier bufferBarriers[] = { { pBuffer, state } };
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#else
	// Resource will automatically transition so just set the next state without a barrier
	pBuffer->mCurrentState = state;
#endif
}

//////////////////////////////////////////////////////////////////////////
// Resource Loader Globals
//////////////////////////////////////////////////////////////////////////
typedef enum StreamerRequestType
{
	STREAMER_REQUEST_UPDATE_BUFFER,
	STREAMER_REQUEST_UPDATE_TEXTURE,
	STREAMER_REQUEST_INVALID,
} StreamerRequestType;

typedef struct StreamerRequest
{
	StreamerRequest() : mType(STREAMER_REQUEST_INVALID) {}
	StreamerRequest(BufferUpdateDesc& buffer) : mType(STREAMER_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	StreamerRequest(TextureUpdateDesc& texture) : mType(STREAMER_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}

	StreamerRequestType mType;
	SyncToken mToken = 0;
	union
	{
		BufferUpdateDesc bufUpdateDesc;
		TextureUpdateDesc texUpdateDesc;
	};
} StreamerRequest;

//////////////////////////////////////////////////////////////////////////
// Resource Loader Implementation
//////////////////////////////////////////////////////////////////////////
typedef struct ResourceLoader
{
	Renderer* pRenderer;

	uint64_t    mSize;
	CopyEngine* pCopyEngines[MAX_GPUS];

	volatile int mRun;
	WorkItem mWorkItem;
	ThreadHandle mThread;

	Mutex mQueueMutex;
	ConditionVariable mQueueCond;
	tinystl::vector <StreamerRequest> mRequestQueue;

	tfrg_atomic64_t mTokenCompleted;
	tfrg_atomic64_t mTokenCounter;
} ResourceLoader;

static CopyEngine* getCopyEngine(ResourceLoader* pLoader, uint32_t nodeIndex)
{
	CopyEngine*& pCopyEngine = pLoader->pCopyEngines[nodeIndex];
	if (!pCopyEngine)
	{
		pCopyEngine = conf_new<CopyEngine>();
		setupCopyEngine(pLoader->pRenderer, pCopyEngine, nodeIndex, pLoader->mSize);
	}
	return pCopyEngine;
}

static void streamerThreadFunc(void* pThreadData)
{
#define TIME_SLICE_DURATION_MS 16

	ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
	ASSERT(pLoader);

	unsigned nextTimeslot = getSystemTime() + TIME_SLICE_DURATION_MS;
	SyncToken maxToken[NUM_RESOURCE_SETS] = { 0 };
	size_t activeSet = 0;
	StreamerRequest request;
	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();
		while (pLoader->mRun && pLoader->mRequestQueue.empty() && getSystemTime() < nextTimeslot)
		{
			unsigned time = getSystemTime();
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex, nextTimeslot - time);
		}
		pLoader->mQueueMutex.Release();

		pLoader->mQueueMutex.Acquire();
		if (!pLoader->mRequestQueue.empty())
		{
			request = pLoader->mRequestQueue.front();
			pLoader->mRequestQueue.erase(pLoader->mRequestQueue.begin());
		}
		else
		{
			request = StreamerRequest();
		}
		pLoader->mQueueMutex.Release();

		if (request.mToken)
		{
			ASSERT(maxToken[activeSet] < request.mToken);
			maxToken[activeSet] = request.mToken;
		}
		switch (request.mType)
		{
		case STREAMER_REQUEST_UPDATE_BUFFER:
			updateBuffer(
				pLoader->pRenderer, getCopyEngine(pLoader, request.bufUpdateDesc.pBuffer->mDesc.mNodeIndex), activeSet,
				&request.bufUpdateDesc);
			break;
		case STREAMER_REQUEST_UPDATE_TEXTURE:
			updateTexture(
				pLoader->pRenderer, getCopyEngine(pLoader, request.texUpdateDesc.pTexture->mDesc.mNodeIndex), activeSet,
				&request.texUpdateDesc);
			break;
		default:break;
		}

		if (getSystemTime() > nextTimeslot)
		{
			for (size_t i = 0; i < MAX_GPUS; ++i)
			{
				if (pLoader->pCopyEngines[i])
					streamerFlush(pLoader->pCopyEngines[i], activeSet);
			}
			activeSet = (activeSet + 1) % NUM_RESOURCE_SETS;
			for (size_t i = 0; i < MAX_GPUS; ++i)
			{
				if (pLoader->pCopyEngines[i])
				{
					waitCopyEngineSet(pLoader->pCopyEngines[i], activeSet);
					resetCopyEngineSet(pLoader->pRenderer, pLoader->pCopyEngines[i], activeSet);
				}
			}
			SyncToken nextToken = maxToken[activeSet];
			SyncToken prevToken = tfrg_atomic64_load_relaxed(&pLoader->mTokenCompleted);
			// As the only writer atomicity is preserved
			tfrg_atomic64_store_release(&pLoader->mTokenCompleted, nextToken > prevToken ? nextToken : prevToken);
			nextTimeslot = getSystemTime() + TIME_SLICE_DURATION_MS;
		}

	}
	for (size_t i = 0; i < MAX_GPUS; ++i)
	{
		if (pLoader->pCopyEngines[i])
			finish(pLoader->pCopyEngines[i], activeSet);
	}
}

static void addResourceLoader(Renderer* pRenderer, uint64_t size, ResourceLoader** ppLoader)
{
	ResourceLoader* pLoader = conf_new<ResourceLoader>();
	pLoader->pRenderer = pRenderer;

	pLoader->mRun = true;
	pLoader->mSize = size;

	pLoader->mWorkItem.pFunc = streamerThreadFunc;
	pLoader->mWorkItem.pData = pLoader;
	pLoader->mWorkItem.mPriority = 0;
	pLoader->mWorkItem.mCompleted = false;

	pLoader->mThread = create_thread(&pLoader->mWorkItem);

	*ppLoader = pLoader;
}

static void removeResourceLoader(ResourceLoader* pLoader)
{
	pLoader->mRun = false;
	pLoader->mQueueCond.Set();
	destroy_thread(pLoader->mThread);

	for (size_t i = 0; i < MAX_GPUS; ++i)
	{
		if (pLoader->pCopyEngines[i])
		{
			cleanupCopyEngine(pLoader->pRenderer, pLoader->pCopyEngines[i]);
			conf_delete(pLoader->pCopyEngines[i]);
		}
	}

	conf_delete(pLoader);
}

static void updateCPUbuffer(Renderer* pRenderer, BufferUpdateDesc* pBufferUpdate)
{
	Buffer* pBuffer = pBufferUpdate->pBuffer;

	ASSERT(
		pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU);

	bool map = !pBuffer->pCpuMappedAddress;
	if (map)
	{
		mapBuffer(pRenderer, pBuffer, NULL);
	}

	const uint64_t bufferSize = (pBufferUpdate->mSize > 0) ? pBufferUpdate->mSize : pBuffer->mDesc.mSize;
	// TODO: remove???
	const uint64_t alignment =
		pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER ? pRenderer->pActiveGpuSettings->mUniformBufferAlignment : 1;
	const uint64_t offset = round_up_64(pBufferUpdate->mDstOffset, alignment);
	void*          pDstBufferAddress = (uint8_t*)(pBuffer->pCpuMappedAddress) + offset;

	if (pBufferUpdate->pData)
	{
		uint8_t* pSrcBufferAddress = (uint8_t*)(pBufferUpdate->pData) + pBufferUpdate->mSrcOffset;
		memcpy(pDstBufferAddress, pSrcBufferAddress, bufferSize);
	}
	else
	{
		memset(pDstBufferAddress, 0, bufferSize);
	}

	if (map)
	{
		unmapBuffer(pRenderer, pBuffer);
	}
}

static void queueResourceUpdate(ResourceLoader* pLoader, BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue.emplace_back(StreamerRequest(*pBufferUpdate));
	pLoader->mRequestQueue.back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.Set();
	if (token) *token = t;
}

static void queueResourceUpdate(ResourceLoader* pLoader, TextureUpdateDesc* pTextureUpdate, SyncToken* token)
{
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue.emplace_back(StreamerRequest(*pTextureUpdate));
	pLoader->mRequestQueue.back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.Set();
	if (token) *token = t;
}

static bool isTokenCompleted(ResourceLoader* pLoader, SyncToken token)
{
	bool completed = tfrg_atomic64_load_acquire(&pLoader->mTokenCompleted) >= token;
	return completed;
}

//////////////////////////////////////////////////////////////////////////
// Resource Loader Implementation
//////////////////////////////////////////////////////////////////////////
static ResourceLoader* pResourceLoader = NULL;

void initResourceLoaderInterface(Renderer* pRenderer, uint64_t memoryBudget, bool useThreads)
{
	addResourceLoader(pRenderer, memoryBudget, &pResourceLoader);
}

void removeResourceLoaderInterface(Renderer* pRenderer)
{
	removeResourceLoader(pResourceLoader);
}

void addResource(BufferLoadDesc* pBufferDesc, bool batch)
{
	SyncToken token = 0;
	addResource(pBufferDesc, &token);
	if (!batch) waitTokenCompleted(token);
}

void addResource(TextureLoadDesc* pTextureDesc, bool batch)
{
	SyncToken token = 0;
	addResource(pTextureDesc, &token);
	if (!batch) waitTokenCompleted(token);
}

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token)
{
	ASSERT(pBufferDesc->ppBuffer);

	bool update = pBufferDesc->pData || pBufferDesc->mForceReset;

	pBufferDesc->mDesc.mStartState = update ? RESOURCE_STATE_COMMON : util_determine_resource_start_state(&pBufferDesc->mDesc);
	addBuffer(pResourceLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

	if (update)
	{
		BufferUpdateDesc bufferUpdate(*pBufferDesc->ppBuffer, pBufferDesc->pData);
		updateResource(&bufferUpdate, token);
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token)
{
	ASSERT(pTextureDesc->ppTexture);

	bool freeImage = false;
	if (pTextureDesc->pFilename)
	{
		pTextureDesc->pImage = conf_new<Image>();
		if (!pTextureDesc->pImage->loadImage(pTextureDesc->pFilename, pTextureDesc->mUseMipmaps, nullptr, nullptr, pTextureDesc->mRoot))
		{
			conf_delete(pTextureDesc->pImage);
			return;
		}
		freeImage = true;
	}
	else if (!pTextureDesc->pFilename && !pTextureDesc->pImage)
	{
		pTextureDesc->pDesc->mStartState = util_determine_resource_start_state(pTextureDesc->pDesc->mDescriptors);
		addTexture(pResourceLoader->pRenderer, pTextureDesc->pDesc, pTextureDesc->ppTexture);
		// TODO: what about barriers???
		// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
		//if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
		//{
		//	TextureBarrier barrier = { *pEmptyTexture->ppTexture, pEmptyTexture->pDesc->mStartState };
		//	cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, true);
		//}
		return;
	}

	Image& img = *pTextureDesc->pImage;

	TextureDesc desc = {};
	desc.mFlags = pTextureDesc->mCreationFlag;
	desc.mWidth = img.GetWidth();
	desc.mHeight = img.GetHeight();
	desc.mDepth = max(1U, img.GetDepth());
	desc.mArraySize = img.GetArrayCount();
	desc.mMipLevels = img.GetMipMapCount();
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mSampleQuality = 0;
	desc.mFormat = img.getFormat();
	desc.mClearValue = ClearValue();
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COPY_DEST;
	desc.pNativeHandle = NULL;
	desc.mHostVisible = false;
	desc.mSrgb = pTextureDesc->mSrgb;
	desc.mNodeIndex = pTextureDesc->mNodeIndex;

	if (img.IsCube())
	{
		desc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
		desc.mArraySize *= 6;
	}

	wchar_t         debugName[MAX_PATH] = {};
	tinystl::string filename = FileSystem::GetFileNameAndExtension(img.GetName());
	mbstowcs(debugName, filename.c_str(), min((size_t)MAX_PATH, filename.size()));
	desc.pDebugName = debugName;

	addTexture(pResourceLoader->pRenderer, &desc, pTextureDesc->ppTexture);

	TextureUpdateDesc updateDesc = { *pTextureDesc->ppTexture, pTextureDesc->pImage, freeImage };
	updateResource(&updateDesc, token);
}

void updateResource(BufferUpdateDesc* pBufferUpdate, bool batch)
{
	SyncToken token = 0;
	updateResource(pBufferUpdate, &token);
	if (!batch) waitTokenCompleted(token);
}

void updateResource(TextureUpdateDesc* pTextureUpdate, bool batch)
{
	SyncToken token = 0;
	updateResource(pTextureUpdate, &token);
	if (!batch) waitTokenCompleted(token);
}

void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources)
{
	SyncToken token = 0;
	updateResources(resourceCount, pResources, &token);
	waitTokenCompleted(token);
}

void updateResource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	if (pBufferUpdate->pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY ||
		pBufferUpdate->pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
	{
		queueResourceUpdate(pResourceLoader, pBufferUpdate, token);
	}
	else
	{
		updateCPUbuffer(pResourceLoader->pRenderer, pBufferUpdate);
	}
}

void updateResource(TextureUpdateDesc* pTextureUpdate, SyncToken* token)
{
	queueResourceUpdate(pResourceLoader, pTextureUpdate, token);
}

void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources, SyncToken* token)
{
	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		if (pResources[i].mType == RESOURCE_TYPE_BUFFER)
		{
			updateResource(&pResources[i].buf, token);
		}
		else
		{
			updateResource(&pResources[i].tex, token);
		}
	}
}

void removeResource(Texture* pTexture)
{
	removeTexture(pResourceLoader->pRenderer, pTexture);
}

void removeResource(Buffer* pBuffer)
{
	removeBuffer(pResourceLoader->pRenderer, pBuffer);
}

bool isTokenCompleted(SyncToken token)
{
	return isTokenCompleted(pResourceLoader, token);
}

void waitTokenCompleted(SyncToken token)
{
	// TODO: replace busy loop with wait
	while (!isTokenCompleted(token))
		continue;
}

void waitBatchCompleted()
{
	SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
	waitTokenCompleted(token);
}

void flushResourceUpdates()
{
	waitBatchCompleted();
}

void finishResourceLoading()
{
	waitBatchCompleted();
}

/************************************************************************/
// Shader loading
/************************************************************************/
#if defined(__ANDROID__)
// Translate Vulkan Shader Type to shaderc shader type
shaderc_shader_kind getShadercShaderType(ShaderStage type)
{
	switch (type)
	{
		case ShaderStage::SHADER_STAGE_VERT: return shaderc_glsl_vertex_shader;
		case ShaderStage::SHADER_STAGE_FRAG: return shaderc_glsl_fragment_shader;
		case ShaderStage::SHADER_STAGE_TESC: return shaderc_glsl_tess_control_shader;
		case ShaderStage::SHADER_STAGE_TESE: return shaderc_glsl_tess_evaluation_shader;
		case ShaderStage::SHADER_STAGE_GEOM: return shaderc_glsl_geometry_shader;
		case ShaderStage::SHADER_STAGE_COMP: return shaderc_glsl_compute_shader;
		default: ASSERT(0); abort();
	}
	return static_cast<shaderc_shader_kind>(-1);
}
#endif

#if defined(VULKAN)
#if defined(__ANDROID__)
// Android:
// Use shaderc to compile glsl to spirV
//@todo add support to macros!!
void vk_compileShader(
	Renderer* pRenderer, ShaderStage stage, uint32_t codeSize, const char* code, const tinystl::string& outFile, uint32_t macroCount,
	ShaderMacro* pMacros, tinystl::vector<char>* pByteCode)
{
	// compile into spir-V shader
	shaderc_compiler_t           compiler = shaderc_compiler_initialize();
	shaderc_compilation_result_t spvShader =
		shaderc_compile_into_spv(compiler, code, codeSize, getShadercShaderType(stage), "shaderc_error", "main", nullptr);
	if (shaderc_result_get_compilation_status(spvShader) != shaderc_compilation_status_success)
	{
        int errno = shaderc_result_get_compilation_status(spvShader);
		LOGERRORF("Shader compiling failed! with status %i", errno);
		abort();
	}

	// Resize the byteCode block based on the compiled shader size
	pByteCode->resize(shaderc_result_get_length(spvShader));
	memcpy(pByteCode->data(), shaderc_result_get_bytes(spvShader), pByteCode->size());

	// Release resources
	shaderc_result_release(spvShader);
	shaderc_compiler_release(compiler);
}
#else
// PC:
// Vulkan has no builtin functions to compile source to spirv
// So we call the glslangValidator tool located inside VulkanSDK on user machine to compile the glsl code to spirv
// This code is not added to Vulkan.cpp since it calls no Vulkan specific functions
void vk_compileShader(
	Renderer* pRenderer, ShaderTarget target, const tinystl::string& fileName, const tinystl::string& outFile, uint32_t macroCount,
	ShaderMacro* pMacros, tinystl::vector<char>* pByteCode)
{
	if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
		FileSystem::CreateDir(FileSystem::GetPath(outFile));

	tinystl::string                  commandLine;
	tinystl::vector<tinystl::string> args;
	tinystl::string                  configFileName;

	// If there is a config file located in the shader source directory use it to specify the limits
	if (FileSystem::FileExists(FileSystem::GetPath(fileName) + "/config.conf", FSRoot::FSR_Absolute))
	{
		configFileName = FileSystem::GetPath(fileName) + "/config.conf";
		// Add command to compile from Vulkan GLSL to Spirv
		commandLine += tinystl::string::format(
			"\"%s\" -V \"%s\" -o \"%s\"", configFileName.size() ? configFileName.c_str() : "", fileName.c_str(), outFile.c_str());
	}
	else
	{
		commandLine += tinystl::string::format("-V \"%s\" -o \"%s\"", fileName.c_str(), outFile.c_str());
	}

	if (target >= shader_target_6_0)
		commandLine += " --target-env vulkan1.1 ";
		//commandLine += " \"-D" + tinystl::string("VULKAN") + "=" + "1" + "\"";

		// Add platform macro
#ifdef _WINDOWS
	commandLine += " \"-D WINDOWS\"";
#elif defined(__ANDROID__)
	commandLine += " \"-D ANDROID\"";
#elif defined(__linux__)
	commandLine += " \"-D LINUX\"";
#endif

	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		commandLine += " \"-D" + pMacros[i].definition + "=" + pMacros[i].value + "\"";
	}
	args.push_back(commandLine);

	tinystl::string glslangValidator = getenv("VULKAN_SDK");
	if (glslangValidator.size())
		glslangValidator += "/bin/glslangValidator";
	else
		glslangValidator = "/usr/bin/glslangValidator";
	if (FileSystem::SystemRun(glslangValidator, args, outFile + "_compile.log") == 0)
	{
		File file = {};
		file.Open(outFile, FileMode::FM_ReadBinary, FSRoot::FSR_Absolute);
		ASSERT(file.IsOpen());
		pByteCode->resize(file.GetSize());
		memcpy(pByteCode->data(), file.ReadText().c_str(), pByteCode->size());
		file.Close();
	}
	else
	{
		File errorFile = {};
		errorFile.Open(outFile + "_compile.log", FM_ReadBinary, FSR_Absolute);
		// If for some reason the error file could not be created just log error msg
		if (!errorFile.IsOpen())
		{
			ErrorMsg("Failed to compile shader %s", fileName.c_str());
		}
		else
		{
			tinystl::string errorLog = errorFile.ReadText();
			errorFile.Close();
			ErrorMsg("Failed to compile shader %s with error\n%s", fileName.c_str(), errorLog.c_str());
			errorFile.Close();
		}
	}
}
#endif
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(
	Renderer* pRenderer, const tinystl::string& fileName, const tinystl::string& outFile, uint32_t macroCount, ShaderMacro* pMacros,
	tinystl::vector<char>* pByteCode)
{
	if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
		FileSystem::CreateDir(FileSystem::GetPath(outFile));

	tinystl::string xcrun = "/usr/bin/xcrun";
	tinystl::string intermediateFile = outFile + ".air";
	tinystl::vector<tinystl::string> args;
	tinystl::string tmpArg = "";

	// Compile the source into a temporary .air file.
	args.push_back("-sdk");
	args.push_back("macosx");
	args.push_back("metal");
	args.push_back("-c");
	tmpArg = tinystl::string::format(
		""
		"%s"
		"",
		fileName.c_str());
	args.push_back(tmpArg);
	args.push_back("-o");
	args.push_back(intermediateFile.c_str());

	//enable the 2 below for shader debugging on xcode 10.0
	//args.push_back("-MO");
	//args.push_back("-gline-tables-only");
	args.push_back("-D");
	args.push_back("MTL_SHADER=1");    // Add MTL_SHADER macro to differentiate structs in headers shared by app/shader code.
	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		args.push_back("-D");
		args.push_back(pMacros[i].definition + "=" + pMacros[i].value);
	}
	if (FileSystem::SystemRun(xcrun, args, "") == 0)
	{
		// Create a .metallib file from the .air file.
		args.clear();
		tmpArg = "";
		args.push_back("-sdk");
		args.push_back("macosx");
		args.push_back("metallib");
		args.push_back(intermediateFile.c_str());
		args.push_back("-o");
		tmpArg = tinystl::string::format(
			""
			"%s"
			"",
			outFile.c_str());
		args.push_back(tmpArg);
		if (FileSystem::SystemRun(xcrun, args, "") == 0)
		{
			// Remove the temp .air file.
			args.clear();
			args.push_back(intermediateFile.c_str());
			FileSystem::SystemRun("rm", args, "");

			// Store the compiled bytecode.
			File file = {};
			file.Open(outFile, FileMode::FM_ReadBinary, FSRoot::FSR_Absolute);
			ASSERT(file.IsOpen());
			pByteCode->resize(file.GetSize());
			memcpy(pByteCode->data(), file.ReadText().c_str(), pByteCode->size());
			file.Close();
		}
		else
			ErrorMsg("Failed to assemble shader's %s .metallib file", fileName.c_str());
	}
	else
		ErrorMsg("Failed to compile shader %s", fileName.c_str());
}
#endif
#if (defined(DIRECT3D12) || defined(DIRECT3D11)) && !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
extern void compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode);
#endif

// Function to generate the timestamp of this shader source file considering all include file timestamp
static bool process_source_file(File* original, File* file, uint32_t& outTimeStamp, tinystl::string& outCode)
{
	// If the source if a non-packaged file, store the timestamp
	if (file)
	{
		tinystl::string fullName = file->GetName();
		unsigned        fileTimeStamp = FileSystem::GetLastModifiedTime(fullName);
		if (fileTimeStamp > outTimeStamp)
			outTimeStamp = fileTimeStamp;
	}

	const tinystl::string pIncludeDirective = "#include";
	while (!file->IsEof())
	{
		tinystl::string line = file->ReadLine();
		uint32_t        filePos = line.find(pIncludeDirective, 0);
		const uint      commentPosCpp = line.find("//", 0);
		const uint      commentPosC = line.find("/*", 0);

		// if we have an "#include \"" in our current line
		const bool bLineHasIncludeDirective = filePos != tinystl::string::npos;
		const bool bLineIsCommentedOut = (commentPosCpp != tinystl::string::npos && commentPosCpp < filePos) ||
										 (commentPosC != tinystl::string::npos && commentPosC < filePos);

		if (bLineHasIncludeDirective && !bLineIsCommentedOut)
		{
			// get the include file name
			uint32_t        currentPos = filePos + (uint32_t)strlen(pIncludeDirective);
			tinystl::string fileName;
			while (line.at(currentPos++) == ' ')
				;    // skip empty spaces
			if (currentPos >= (uint32_t)line.size())
				continue;
			if (line.at(currentPos - 1) != '\"')
				continue;
			else
			{
				// read char by char until we have the include file name
				while (line.at(currentPos) != '\"')
				{
					fileName.push_back(line.at(currentPos));
					++currentPos;
				}
			}

			// get the include file path
			tinystl::string includeFileName = FileSystem::GetPath(file->GetName()) + fileName;
			if (FileSystem::GetFileName(includeFileName).at(0) == '<')    // disregard bracketsauthop
				continue;

			// open the include file
			File includeFile = {};
			includeFile.Open(includeFileName, FM_ReadBinary, FSR_Absolute);
			if (!includeFile.IsOpen())
			{
				LOGERRORF("Cannot open #include file: %s", includeFileName.c_str());
				return false;
			}

			// Add the include file into the current code recursively
			if (!process_source_file(original, &includeFile, outTimeStamp, outCode))
			{
				includeFile.Close();
				return false;
			}

			includeFile.Close();
		}

#ifdef TARGET_IOS
		// iOS doesn't have support for resolving user header includes in shader code
		// when compiling with shader source using Metal runtime.
		// https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/FunctionsandLibraries.html
		//
		// Here we write out the contents of the header include into the original source
		// where its included from -- we're expanding the headers as the pre-processor
		// would do.
		//
		//const bool bAreWeProcessingAnIncludedHeader = file != original;
		if (!bLineHasIncludeDirective)
		{
			outCode += line + "\n";
		}
#else
		// Simply write out the current line if we are not in a header file
		const bool bAreWeProcessingTheShaderSource = file == original;
		if (bAreWeProcessingTheShaderSource)
		{
			outCode += line + "\n";
		}
#endif
	}

	return true;
}

// Loads the bytecode from file if the binary shader file is newer than the source
bool check_for_byte_code(const tinystl::string& binaryShaderName, uint32_t sourceTimeStamp, tinystl::vector<char>& byteCode)
{
	if (!FileSystem::FileExists(binaryShaderName, FSR_Absolute))
		return false;

	// If source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
	// than source
	if (sourceTimeStamp && FileSystem::GetLastModifiedTime(binaryShaderName) < sourceTimeStamp)
		return false;

	File file = {};
	file.Open(binaryShaderName, FM_ReadBinary, FSR_Absolute);
	if (!file.IsOpen())
	{
		LOGERROR(binaryShaderName + " is not a valid shader bytecode file");
		return false;
	}

	byteCode.resize(file.GetSize());
	memcpy(byteCode.data(), file.ReadText().c_str(), byteCode.size());
	return true;
}

// Saves bytecode to a file
bool save_byte_code(const tinystl::string& binaryShaderName, const tinystl::vector<char>& byteCode)
{
	tinystl::string path = FileSystem::GetPath(binaryShaderName);
	if (!FileSystem::DirExists(path))
		FileSystem::CreateDir(path);

	File outFile = {};
	outFile.Open(binaryShaderName, FM_WriteBinary, FSR_Absolute);

	if (!outFile.IsOpen())
		return false;

	outFile.Write(byteCode.data(), (uint32_t)byteCode.size() * sizeof(char));
	outFile.Close();

	return true;
}

bool load_shader_stage_byte_code(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, FSRoot root, uint32_t macroCount,
	ShaderMacro* pMacros, uint32_t rendererMacroCount, ShaderMacro* pRendererMacros, tinystl::vector<char>& byteCode)
{
	File            shaderSource = {};
	tinystl::string code;
	uint32_t        timeStamp = 0;

#ifndef METAL
	const char* shaderName = fileName;
#else
	// Metal shader files need to have the .metal extension.
	tinystl::string metalShaderName = tinystl::string(fileName) + ".metal";
	const char* shaderName = metalShaderName.c_str();
#endif

	shaderSource.Open(shaderName, FM_ReadBinary, root);
	ASSERT(shaderSource.IsOpen());

	if (!process_source_file(&shaderSource, &shaderSource, timeStamp, code))
		return false;

	tinystl::string name, extension, path;
	FileSystem::SplitPath(fileName, &path, &name, &extension);
	tinystl::string shaderDefines;
	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (pMacros[i].definition + pMacros[i].value);
	}
	// Apply renderer specified macros
	for (uint32_t i = 0; i < rendererMacroCount; ++i)
	{
		shaderDefines += (pRendererMacros[i].definition + pRendererMacros[i].value);
	}

#if 0    //#ifdef _DURANGO
	// Using Durango application data storage requires appmanifest(from application) changes.
	tinystl::string binaryShaderName = FileSystem::GetAppPreferencesDir(NULL,NULL) + "/" + pRenderer->pName + "/CompiledShadersBinary/" +
		FileSystem::GetFileName(fileName) + tinystl::string::format("_%zu", tinystl::hash(shaderDefines)) + extension + ".bin";
#else
	tinystl::string rendererApi;
	switch (pRenderer->mSettings.mApi)
	{
		case RENDERER_API_D3D12:
		case RENDERER_API_XBOX_D3D12: rendererApi = "D3D12"; break;
		case RENDERER_API_D3D11: rendererApi = "D3D11"; break;
		case RENDERER_API_VULKAN: rendererApi = "Vulkan"; break;
		case RENDERER_API_METAL: rendererApi = "Metal"; break;
		default: break;
	}

	tinystl::string appName(pRenderer->pName);
#ifdef __linux__

	tinystl::string lowerStr = appName.to_lower();
	appName = lowerStr != pRenderer->pName ? lowerStr : lowerStr + "_";
#endif

	tinystl::string binaryShaderName = FileSystem::GetProgramDir() + "/" + appName + tinystl::string("/Shaders/") + rendererApi +
									   tinystl::string("/CompiledShadersBinary/") + FileSystem::GetFileName(fileName) +
									   tinystl::string::format("_%zu", tinystl::hash(shaderDefines)) + extension +
									   tinystl::string::format("%u", (uint32_t)target) + ".bin";
#endif

	// Shader source is newer than binary
	if (!check_for_byte_code(binaryShaderName, timeStamp, byteCode))
	{
		if (pRenderer->mSettings.mApi == RENDERER_API_METAL || pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
#if defined(__ANDROID__)
			vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderName, macroCount, pMacros, &byteCode);
#else
			vk_compileShader(pRenderer, target, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode);
#endif
#elif defined(METAL)
			mtl_compileShader(pRenderer, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode);
#endif
		}
		else
		{
#if defined(DIRECT3D12) || defined(DIRECT3D11)
			char*    pByteCode = NULL;
			uint32_t byteCodeSize = 0;
			compileShader(
				pRenderer, target, stage, shaderSource.GetName(), (uint32_t)code.size(), code.c_str(), macroCount, pMacros, conf_malloc,
				&byteCodeSize, &pByteCode);
			byteCode.resize(byteCodeSize);
			memcpy(byteCode.data(), pByteCode, byteCodeSize);
			conf_free(pByteCode);
			if (!save_byte_code(binaryShaderName, byteCode))
			{
				const char* shaderName = shaderSource.GetName();
				LOGWARNINGF("Failed to save byte code for file %s", shaderName);
			}
#endif
		}
		if (!byteCode.size())
		{
			ErrorMsg("Error while generating bytecode for shader %s", fileName);
			shaderSource.Close();
			return false;
		}
	}

	shaderSource.Close();
	return true;
}
#ifdef TARGET_IOS
bool find_shader_stage(const tinystl::string& fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	tinystl::string ext = FileSystem::GetExtension(fileName);
	if (ext == ".vert")
	{
		*pOutStage = &pDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (ext == ".frag")
	{
		*pOutStage = &pDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
#ifndef METAL
#if !defined(METAL)
	else if (ext == ".tesc")
	{
		*pOutStage = &pDesc->mHull;
		*pStage = SHADER_STAGE_HULL;
	}
	else if (ext == ".tese")
	{
		*pOutStage = &pDesc->mDomain;
		*pStage = SHADER_STAGE_DOMN;
	}
	else if (ext == ".geom")
	{
		*pOutStage = &pDesc->mGeom;
		*pStage = SHADER_STAGE_GEOM;
	}
#endif
#endif
	else if (ext == ".comp")
	{
		*pOutStage = &pDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else
	{
		return false;
	}

	return true;
}
#else
bool find_shader_stage(
	const tinystl::string& fileName, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	tinystl::string ext = FileSystem::GetExtension(fileName);
	if (ext == ".vert")
	{
		*pOutStage = &pBinaryDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (ext == ".frag")
	{
		*pOutStage = &pBinaryDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
#ifndef METAL
#if !defined(METAL)
	else if (ext == ".tesc")
	{
		*pOutStage = &pBinaryDesc->mHull;
		*pStage = SHADER_STAGE_HULL;
	}
	else if (ext == ".tese")
	{
		*pOutStage = &pBinaryDesc->mDomain;
		*pStage = SHADER_STAGE_DOMN;
	}
	else if (ext == ".geom")
	{
		*pOutStage = &pBinaryDesc->mGeom;
		*pStage = SHADER_STAGE_GEOM;
	}
#endif
#endif
	else if (ext == ".comp")
	{
		*pOutStage = &pBinaryDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else
	{
		return false;
	}

	return true;
}
#endif
void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** ppShader)
{
#ifndef TARGET_IOS
	BinaryShaderDesc      binaryDesc = {};
	tinystl::vector<char> byteCodes[SHADER_STAGE_COUNT] = {};
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		const RendererShaderDefinesDesc rendererDefinesDesc = get_renderer_shaderdefines(pRenderer);

		if (pDesc->mStages[i].mFileName.size() != 0)
		{
			tinystl::string filename = pDesc->mStages[i].mFileName;
			if (pDesc->mStages[i].mRoot != FSR_SrcShaders)
				filename = FileSystem::GetRootPath(FSR_SrcShaders) + filename;

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			if (find_shader_stage(filename, &binaryDesc, &pStage, &stage))
			{
				if (!load_shader_stage_byte_code(
						pRenderer, pDesc->mTarget, stage, filename, pDesc->mStages[i].mRoot, pDesc->mStages[i].mMacroCount,
						pDesc->mStages[i].pMacros, rendererDefinesDesc.rendererShaderDefinesCnt, rendererDefinesDesc.rendererShaderDefines,
						byteCodes[i]))
					return;

				binaryDesc.mStages |= stage;
				pStage->pByteCode = byteCodes[i].data();
				pStage->mByteCodeSize = (uint32_t)byteCodes[i].size();
#if defined(METAL)
				pStage->mEntryPoint = "stageMain";
				// In metal, we need the shader source for our reflection system.
				File metalFile = {};
				metalFile.Open(filename + ".metal", FM_ReadBinary, pDesc->mStages[i].mRoot);
				pStage->mSource = metalFile.ReadText();
				metalFile.Close();
#endif
			}
		}
	}

	addShaderBinary(pRenderer, &binaryDesc, ppShader);
#else
	// Binary shaders are not supported on iOS.
	ShaderDesc desc = {};
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		const RendererShaderDefinesDesc rendererDefinesDesc = get_renderer_shaderdefines(pRenderer);

		if (pDesc->mStages[i].mFileName.size() > 0)
		{
			tinystl::string filename = pDesc->mStages[i].mFileName;
			if (pDesc->mStages[i].mRoot != FSR_SrcShaders)
				filename = FileSystem::GetRootPath(FSR_SrcShaders) + filename;

			ShaderStage stage;
			ShaderStageDesc* pStage = NULL;
			if (find_shader_stage(filename, &desc, &pStage, &stage))
			{
				File shaderSource = {};
				shaderSource.Open(filename + ".metal", FM_ReadBinary, pDesc->mStages[i].mRoot);
				ASSERT(shaderSource.IsOpen());

				pStage->mName = pDesc->mStages[i].mFileName;
				uint timestamp = 0;
				process_source_file(&shaderSource, &shaderSource, timestamp, pStage->mCode);
				pStage->mEntryPoint = "stageMain";
				// Apply user specified shader macros
				for (uint32_t j = 0; j < pDesc->mStages[i].mMacroCount; j++)
				{
					pStage->mMacros.push_back(pDesc->mStages[i].pMacros[j]);
				}
				// Apply renderer specified shader macros
				for (uint32_t j = 0; j < rendererDefinesDesc.rendererShaderDefinesCnt; j++)
				{
					pStage->mMacros.push_back(rendererDefinesDesc.rendererShaderDefines[j]);
				}
				shaderSource.Close();
				desc.mStages |= stage;
			}
		}
	}

	addShader(pRenderer, &desc, ppShader);
#endif
}
