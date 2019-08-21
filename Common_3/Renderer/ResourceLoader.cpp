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

#define IMAGE_CLASS_ALLOWED

// this is needed for the CopyEngine on XBox
#ifdef _DURANGO
#include "../../Xbox/CommonXBOXOne_3/OS/XBoxPrivateHeaders.h"
#endif

#include "../ThirdParty/OpenSource/EASTL/deque.h"

#include "IRenderer.h"
#include "ResourceLoader.h"
#include "../OS/Interfaces/ILog.h"
#include "../OS/Interfaces/IThread.h"
#include "../OS/Image/Image.h"

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
#include "../OS/Interfaces/IMemory.h"

// buffer functions
#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture);
extern void removeTexture(Renderer* pRenderer, Texture* p_texture);
extern void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
extern void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, SubresourceDataDesc* pSubresourceDesc);
extern const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer);
#endif
/************************************************************************/
/************************************************************************/

//////////////////////////////////////////////////////////////////////////
// Internal TextureUpdateDesc
// Used internally as to not expose Image class in the public interface
//////////////////////////////////////////////////////////////////////////
typedef struct TextureUpdateDescInternal
{
	Texture* pTexture;
	Image*   pImage;
	bool     mFreeImage;
} TextureUpdateDescInternal;

//////////////////////////////////////////////////////////////////////////
// Resource CopyEngine Structures
//////////////////////////////////////////////////////////////////////////
typedef struct MappedMemoryRange
{
	uint8_t* pData;
	Buffer*  pBuffer;
	uint64_t mOffset;
	uint64_t mSize;
} MappedMemoryRange;

typedef struct ResourceSet
{
	Fence*  pFence;
#ifdef _DURANGO
	DmaCmd* pCmd;
#else
	Cmd*    pCmd;
#endif
	Buffer* mBuffer;
} CopyResourceSet;

enum
{
	DEFAULT_BUFFER_SIZE = 16ull<<20,
	DEFAULT_BUFFER_COUNT = 2u,
	DEFAULT_TIMESLICE_MS = 4u,
	MAX_BUFFER_COUNT = 8u,
};

//Synchronization?
typedef struct CopyEngine
{
	Queue*      pQueue;
	CmdPool*    pCmdPool;
	ResourceSet* resourceSets;
	uint64_t    bufferSize;
	uint64_t    allocatedSpace;
	uint32_t    bufferCount;
	bool        isRecording;
} CopyEngine;

//////////////////////////////////////////////////////////////////////////
// Resource Loader Internal Functions
//////////////////////////////////////////////////////////////////////////
static void setupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size, uint32_t bufferCount)
{
	QueueDesc desc = { QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, CMD_POOL_COPY, nodeIndex };
	addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

	addCmdPool(pRenderer, pCopyEngine->pQueue, false, &pCopyEngine->pCmdPool);

	const uint32_t maxBlockSize = 32;
	uint64_t       minUploadSize = pCopyEngine->pQueue->mUploadGranularity.mWidth * pCopyEngine->pQueue->mUploadGranularity.mHeight *
							 pCopyEngine->pQueue->mUploadGranularity.mDepth * maxBlockSize;
	size = max(size, minUploadSize);

	pCopyEngine->resourceSets = (ResourceSet*)conf_malloc(sizeof(ResourceSet)*bufferCount);
	for (uint32_t i=0;  i < bufferCount; ++i)
	{
		ResourceSet& resourceSet = pCopyEngine->resourceSets[i];
		addFence(pRenderer, &resourceSet.pFence);

		addCmd(pCopyEngine->pCmdPool, false, &resourceSet.pCmd);

		BufferDesc bufferDesc = {};
		bufferDesc.mSize = size;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.mNodeIndex = nodeIndex;
		addBuffer(pRenderer, &bufferDesc, &resourceSet.mBuffer);
	}

	pCopyEngine->bufferSize = size;
	pCopyEngine->bufferCount = bufferCount;
	pCopyEngine->allocatedSpace = 0;
	pCopyEngine->isRecording = false;
}

static void cleanupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
	for (uint32_t i = 0; i < pCopyEngine->bufferCount; ++i)
	{
		ResourceSet& resourceSet = pCopyEngine->resourceSets[i];
		removeBuffer(pRenderer, resourceSet.mBuffer);

		removeCmd(pCopyEngine->pCmdPool, resourceSet.pCmd);

		removeFence(pRenderer, resourceSet.pFence);
	}
	
	conf_free(pCopyEngine->resourceSets);

	removeCmdPool(pRenderer, pCopyEngine->pCmdPool);

	removeQueue(pCopyEngine->pQueue);
}

static void waitCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	waitForFences(pRenderer, 1, &resourceSet.pFence);
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	pCopyEngine->allocatedSpace = 0;
	pCopyEngine->isRecording = false;
}

#ifdef _DURANGO
static DmaCmd* aquireCmd(CopyEngine* pCopyEngine, size_t activeSet)
{
	ResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	if (!pCopyEngine->isRecording)
	{
		beginCmd(resourceSet.pCmd);
		pCopyEngine->isRecording = true;
	}
	return resourceSet.pCmd;
}
#else
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
#endif

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

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, uint64_t memoryRequirement, uint32_t alignment)
{
	uint64_t offset = pCopyEngine->allocatedSpace;
	if (alignment != 0)
	{
		offset = round_up_64(offset, alignment);
	}

	CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[activeSet];
	uint64_t size = pResourceSet->mBuffer->mDesc.mSize;
	bool memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
	if (memoryAvailable)
	{
		Buffer* buffer = pResourceSet->mBuffer;
#if defined(DIRECT3D11)
		// TODO: do done once, unmap before queue submit
	mapBuffer(pRenderer, buffer, NULL);
#endif
		uint8_t* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
		pCopyEngine->allocatedSpace = offset + memoryRequirement;
		return { pDstData, buffer, offset, memoryRequirement };
	}

	return { nullptr, nullptr, 0, 0};
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

typedef enum UpdateRequestType
{
	UPDATE_REQUEST_UPDATE_BUFFER,
	UPDATE_REQUEST_UPDATE_TEXTURE,
	UPDATE_REQUEST_UPDATE_RESOURCE_STATE,
	UPDATE_REQUEST_INVALID,
} UpdateRequestType;

typedef struct UpdateRequest
{
	UpdateRequest() : mType(UPDATE_REQUEST_INVALID) {}
	UpdateRequest(BufferUpdateDesc& buffer) : mType(UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	UpdateRequest(TextureUpdateDescInternal& texture) : mType(UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
	UpdateRequest(Buffer* buf) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE) { buffer = buf; texture = NULL; }
	UpdateRequest(Texture* tex) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE) { texture = tex; buffer = NULL; }
	UpdateRequestType mType;
	SyncToken mToken = 0;
	union
	{
		BufferUpdateDesc bufUpdateDesc;
		TextureUpdateDescInternal texUpdateDesc;
		struct { Buffer* buffer; Texture* texture; };
	};
} UpdateRequest;

typedef struct UpdateState
{
	UpdateState(): UpdateState(UpdateRequest())
	{
	}
	UpdateState(const UpdateRequest& request): mRequest(request), mMipLevel(0), mArrayLayer(0), mOffset({ 0, 0, 0 }), mSize(0)
	{
	}

	UpdateRequest mRequest;
	uint32_t      mMipLevel;
	uint32_t      mArrayLayer;
	uint3         mOffset;
	uint64_t      mSize;
} UpdateState;

class ResourceLoader
{
public:
	Renderer* pRenderer;

	ResourceLoaderDesc mDesc;

	volatile int mRun;
	ThreadDesc   mThreadDesc;
	ThreadHandle mThread;

	Mutex mQueueMutex;
	ConditionVariable mQueueCond;
	Mutex mTokenMutex;
	ConditionVariable mTokenCond;
	eastl::deque <UpdateRequest> mRequestQueue[MAX_GPUS];

	tfrg_atomic64_t mTokenCompleted;
	tfrg_atomic64_t mTokenCounter;

	static Image* AllocImage()
	{
		return conf_new(Image);
	}

	static Image* CreateImage(const ImageFormat::Enum fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData)
	{
		Image* pImage = AllocImage();
		pImage->Create(fmt, w, h, d, mipMapCount, arraySize, rawData);
		return pImage;
	}

	static Image* CreateImage(const char* origFileName, memoryAllocationFunc pAllocator, void* pUserData, FSRoot root)
	{
		Image* pImage = AllocImage();
		if (!pImage->LoadFromFile(origFileName, pAllocator, pUserData, root))
		{
			DestroyImage(pImage);
			return NULL;
		}

		return pImage;
	}

	static Image* CreateImage(uint8_t const* mem, uint32_t size, char const* extension, memoryAllocationFunc pAllocator, void* pUserData)
	{
		Image* pImage = AllocImage();
		if (!pImage->LoadFromMemory(mem, size, extension, pAllocator, pUserData))
		{
			DestroyImage(pImage);
			return NULL;
		}

		return pImage;
	}

	static void DestroyImage(Image* pImage)
	{
		pImage->Destroy();
		conf_delete(pImage);
	}
};

uint32 SplitBitsWith0(uint32 x)
{
	x &= 0x0000ffff;
	x = (x ^ (x << 8)) & 0x00ff00ff;
	x = (x ^ (x << 4)) & 0x0f0f0f0f;
	x = (x ^ (x << 2)) & 0x33333333;
	x = (x ^ (x << 1)) & 0x55555555;
	return x;
}

uint32 EncodeMorton(uint32 x, uint32 y)
{
	return (SplitBitsWith0(y) << 1) + SplitBitsWith0(x);
}

// TODO: test and fix
void copyUploadRectZCurve(uint8_t* pDstData, uint8_t* pSrcData, Region3D uploadRegion, uint3 srcPitches, uint3 dstPitches)
{
	uint32_t offset = 0;
	for (uint32_t z = uploadRegion.mZOffset; z < uploadRegion.mZOffset+uploadRegion.mDepth; ++z)
	{
		for (uint32_t y = uploadRegion.mYOffset; y < uploadRegion.mYOffset+uploadRegion.mHeight; ++y)
		{
			for (uint32_t x = uploadRegion.mXOffset; x < uploadRegion.mXOffset+uploadRegion.mWidth; ++x)
			{
				uint32_t blockOffset = EncodeMorton(y, x);
				memcpy(pDstData + offset, pSrcData + blockOffset * srcPitches.x, srcPitches.x);
				offset += dstPitches.x;
			}
			offset = round_up(offset, dstPitches.y);
		}
		pSrcData += srcPitches.z;
	}
}

void copyUploadRect(uint8_t* pDstData, uint8_t* pSrcData, Region3D uploadRegion, uint3 srcPitches, uint3 dstPitches)
{
	uint32_t srcOffset = uploadRegion.mZOffset * srcPitches.z + uploadRegion.mYOffset * srcPitches.y +
						 uploadRegion.mXOffset * srcPitches.x;
	uint32_t numSlices = uploadRegion.mHeight * uploadRegion.mDepth;
	uint32_t pitch = uploadRegion.mWidth * srcPitches.x;
	pSrcData += srcOffset;
	for (uint32_t s = 0; s < numSlices; ++s)
	{
		memcpy(pDstData, pSrcData, pitch);
		pSrcData += srcPitches.y;
		pDstData += dstPitches.y;
	}
}

uint3 calculateUploadRect(uint64_t mem, uint3 pitches, uint3 offset, uint3 extent, uint3 granularity)
{
	uint3 scaler{granularity.x*granularity.y*granularity.z, granularity.y*granularity.z, granularity.z};
	pitches *= scaler;
	uint3 leftover = extent-offset;
	uint32_t numSlices = (uint32_t)min<uint64_t>((mem / pitches.z) * granularity.z, leftover.z);
	// advance by slices
	if (offset.x == 0 && offset.y == 0 && numSlices > 0)
	{
		return { extent.x, extent.y, numSlices };
	}

	// advance by strides
	numSlices = min(leftover.z, granularity.z);
	uint32_t numStrides = (uint32_t)min<uint64_t>((mem / pitches.y) * granularity.y, leftover.y);
	if (offset.x == 0 && numStrides > 0)
	{
		return { extent.x, numStrides, numSlices };
	}

	numStrides = min(leftover.y, granularity.y);
	// advance by blocks
	uint32_t numBlocks = (uint32_t)min<uint64_t>((mem / pitches.x) * granularity.x, leftover.x);
	return { numBlocks, numStrides, numSlices };
}

Region3D calculateUploadRegion(uint3 offset, uint3 extent, uint3 uploadBlock, uint3 pxImageDim)
{
	uint3 regionOffset = offset * uploadBlock;
	uint3 regionSize = min(extent * uploadBlock, pxImageDim);
	return { regionOffset.x, regionOffset.y, regionOffset.z, regionSize.x, regionSize.y, regionSize.z };
}

static bool updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pTextureUpdate)
{
	TextureUpdateDescInternal& texUpdateDesc = pTextureUpdate.mRequest.texUpdateDesc;
	ASSERT(pCopyEngine->pQueue->mQueueDesc.mNodeIndex == texUpdateDesc.pTexture->mDesc.mNodeIndex);
	bool         applyBarrieers = pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12 || pRenderer->mSettings.mApi == RENDERER_API_METAL;
	const Image& img = *texUpdateDesc.pImage;
	Texture*     pTexture = texUpdateDesc.pTexture;

#ifdef _DURANGO
	DmaCmd*      pCmd = aquireCmd(pCopyEngine, activeSet);
#else
	Cmd*         pCmd = aquireCmd(pCopyEngine, activeSet);
#endif

	ASSERT(pTexture);

	uint32_t  textureAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
	uint32_t  textureRowAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureRowAlignment;

	uint32_t          nSlices = img.IsCube() ? 6 : 1;
	uint32_t          arrayCount = img.GetArrayCount() * nSlices;

	// TODO: move to Image
	bool isSwizzledZCurve = !img.IsLinearLayout();

	uint32_t i = pTextureUpdate.mMipLevel;
	uint32_t j = pTextureUpdate.mArrayLayer;
	uint3 uploadOffset = pTextureUpdate.mOffset;

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
	if (applyBarrieers && (uploadOffset.x == 0) && (uploadOffset.y == 0) && (uploadOffset.z == 0))
	{
		TextureBarrier preCopyBarrier = { pTexture, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier, false);
	}

	ImageFormat::Enum fmt = img.GetFormat();
	Extent3D          uploadGran = pCopyEngine->pQueue->mUploadGranularity;
	const uint32_t    blockSize = ImageFormat::GetBytesPerBlock(fmt);
	const uint32      pxPerRow = max<uint32_t>(round_down(textureRowAlignment / blockSize, uploadGran.mWidth), uploadGran.mWidth);
	const uint3       queueGranularity = { pxPerRow, uploadGran.mHeight, uploadGran.mDepth };
	const uint3       pxBlockDim = ImageFormat::GetBlockSize(fmt);

	for (; i < pTexture->mDesc.mMipLevels; ++i)
	{
		uint3    pxImageDim{ img.GetWidth(i), img.GetHeight(i), img.GetDepth(i) };
		uint3    uploadExtent{ (pxImageDim + pxBlockDim - uint3(1)) / pxBlockDim };
		uint3    granularity{ min(queueGranularity, uploadExtent) };
		uint32_t srcPitchY{ blockSize * uploadExtent.x };
		uint32_t dstPitchY{ round_up(srcPitchY, textureRowAlignment) };
		uint3    srcPitches{ blockSize, srcPitchY, srcPitchY * uploadExtent.y };
		uint3    dstPitches{ blockSize, dstPitchY, dstPitchY * uploadExtent.y };

		ASSERT(uploadOffset.x < uploadExtent.x || uploadOffset.y < uploadExtent.y || uploadOffset.z < uploadExtent.z);

		for (; j < arrayCount; ++j)
		{
			uint64_t spaceAvailable{ round_down_64(pCopyEngine->bufferSize - pCopyEngine->allocatedSpace, textureRowAlignment) };
			uint3    uploadRectExtent{ calculateUploadRect(spaceAvailable, dstPitches, uploadOffset, uploadExtent, granularity) };
			uint32_t uploadPitchY{ round_up(uploadRectExtent.x * dstPitches.x, textureRowAlignment) };
			uint3    uploadPitches{ blockSize, uploadPitchY, uploadPitchY * uploadRectExtent.y };

			ASSERT(
				uploadOffset.x + uploadRectExtent.x <= uploadExtent.x || uploadOffset.y + uploadRectExtent.y <= uploadExtent.y ||
				uploadOffset.z + uploadRectExtent.z <= uploadExtent.z);

			if (uploadRectExtent.x == 0)
			{
				pTextureUpdate.mMipLevel = i;
				pTextureUpdate.mArrayLayer = j;
				pTextureUpdate.mOffset = uploadOffset;
				return false;
			}

			MappedMemoryRange range =
				allocateStagingMemory(pRenderer, pCopyEngine, activeSet, uploadRectExtent.z * uploadPitches.z, textureAlignment);
			// TODO: should not happed, resolve, simplify
			//ASSERT(range.pData);
			if (!range.pData)
			{
				pTextureUpdate.mMipLevel = i;
				pTextureUpdate.mArrayLayer = j;
				pTextureUpdate.mOffset = uploadOffset;
				return false;
			}

			SubresourceDataDesc  texData;
			texData.mArrayLayer = j /*n * nSlices + k*/;
			texData.mMipLevel = i;
			texData.mBufferOffset = range.mOffset;
			texData.mRegion = calculateUploadRegion(uploadOffset, uploadRectExtent, pxBlockDim, pxImageDim);
			texData.mRowPitch = uploadPitches.y;
			texData.mSlicePitch = uploadPitches.z;

			uint32_t n = j / nSlices;
			uint32_t k = j - n * nSlices;
			uint8_t* pSrcData = (uint8_t*)img.GetPixels(i, n) + k * srcPitches.z;

			Region3D uploadRegion{ uploadOffset.x,     uploadOffset.y,     uploadOffset.z,
								   uploadRectExtent.x, uploadRectExtent.y, uploadRectExtent.z };

			if (isSwizzledZCurve)
				copyUploadRectZCurve(range.pData, pSrcData, uploadRegion, srcPitches, uploadPitches);
			else
				copyUploadRect(range.pData, pSrcData, uploadRegion, srcPitches, uploadPitches);

#if defined(DIRECT3D11)
			unmapBuffer(pRenderer, range.pBuffer);
#endif

			cmdUpdateSubresource(pCmd, pTexture, pCopyEngine->resourceSets[activeSet].mBuffer, &texData);

			uploadOffset.x += uploadRectExtent.x;
			uploadOffset.y += (uploadOffset.x < uploadExtent.x) ? 0 : uploadRectExtent.y;
			uploadOffset.z += (uploadOffset.y < uploadExtent.y) ? 0 : uploadRectExtent.z;

			uploadOffset.x = uploadOffset.x % uploadExtent.x;
			uploadOffset.y = uploadOffset.y % uploadExtent.y;
			uploadOffset.z = uploadOffset.z % uploadExtent.z;

			if (uploadOffset.x != 0 || uploadOffset.y != 0 || uploadOffset.z != 0)
			{
				pTextureUpdate.mMipLevel = i;
				pTextureUpdate.mArrayLayer = j;
				pTextureUpdate.mOffset = uploadOffset;
				return false;
			}
		}
		j = 0;
		ASSERT(uploadOffset.x == 0 && uploadOffset.y == 0 && uploadOffset.z == 0);
	}

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
	if (applyBarrieers)
	{
		TextureBarrier postCopyBarrier = { pTexture, util_determine_resource_start_state(pTexture->mDesc.mDescriptors) };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier, true);
	}
	else
	{
		pTexture->mCurrentState = util_determine_resource_start_state(pTexture->mDesc.mDescriptors);
	}
	
	if (texUpdateDesc.mFreeImage)
	{
		ResourceLoader::DestroyImage(texUpdateDesc.pImage);
	}

	return true;
}

static bool updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pBufferUpdate)
{
	BufferUpdateDesc& bufUpdateDesc = pBufferUpdate.mRequest.bufUpdateDesc;
	ASSERT(pCopyEngine->pQueue->mQueueDesc.mNodeIndex == bufUpdateDesc.pBuffer->mDesc.mNodeIndex);
	Buffer* pBuffer = bufUpdateDesc.pBuffer;
	ASSERT(pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

	// TODO: remove uniform buffer alignment?
	const uint64_t bufferSize = (bufUpdateDesc.mSize > 0) ? bufUpdateDesc.mSize : pBuffer->mDesc.mSize;
	const uint64_t alignment = pBuffer->mDesc.mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER ? pRenderer->pActiveGpuSettings->mUniformBufferAlignment : 1;
	const uint64_t offset = round_up_64(bufUpdateDesc.mDstOffset, alignment) + pBufferUpdate.mSize;
	uint64_t       spaceAvailable = round_down_64(pCopyEngine->bufferSize - pCopyEngine->allocatedSpace, RESOURCE_BUFFER_ALIGNMENT);

	if (spaceAvailable < RESOURCE_BUFFER_ALIGNMENT)
		return false;

	uint64_t dataToCopy = min(spaceAvailable, bufferSize - pBufferUpdate.mSize);

#ifdef _DURANGO
	DmaCmd* pCmd = aquireCmd(pCopyEngine, activeSet);
#else
	Cmd* pCmd = aquireCmd(pCopyEngine, activeSet);
#endif
	
	MappedMemoryRange range = allocateStagingMemory(pRenderer, pCopyEngine, activeSet, dataToCopy, RESOURCE_BUFFER_ALIGNMENT);

	// TODO: should not happed, resolve, simplify
	//ASSERT(range.pData);
	if (!range.pData)
		return false;

	void* pSrcBufferAddress = NULL;
	if (bufUpdateDesc.pData)
		pSrcBufferAddress = (uint8_t*)(bufUpdateDesc.pData) + (bufUpdateDesc.mSrcOffset + pBufferUpdate.mSize);

	if (pSrcBufferAddress)
		memcpy(range.pData, pSrcBufferAddress, dataToCopy);
	else
		memset(range.pData, 0, dataToCopy);

	cmdUpdateBuffer(pCmd, pBuffer, offset, range.pBuffer, range.mOffset, dataToCopy);
#if defined(DIRECT3D11)
	unmapBuffer(pRenderer, range.pBuffer);
#endif

	pBufferUpdate.mSize += dataToCopy;

	if (pBufferUpdate.mSize != bufferSize)
	{
		return false;
	}

	ResourceState state = util_determine_resource_start_state(&pBuffer->mDesc);
#ifdef _DURANGO
	// XBox One needs explicit resource transitions
	BufferBarrier bufferBarriers[] = { { pBuffer, state } };
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, false);
#else
	// Resource will automatically transition so just set the next state without a barrier
	pBuffer->mCurrentState = state;
#endif

	return true;
}

static bool updateResourceState(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pUpdate)
{
	bool applyBarriers = pRenderer->mSettings.mApi == RENDERER_API_VULKAN;
	if (applyBarriers)
	{
#ifdef _DURANGO
		DmaCmd* pCmd = aquireCmd(pCopyEngine, activeSet);
#else
		Cmd* pCmd = aquireCmd(pCopyEngine, activeSet);
#endif
		if (pUpdate.mRequest.buffer)
		{
			BufferBarrier barrier = { pUpdate.mRequest.buffer, pUpdate.mRequest.buffer->mDesc.mStartState };
			cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, true);
		}
		else if (pUpdate.mRequest.texture)
		{
			TextureBarrier barrier = { pUpdate.mRequest.texture, pUpdate.mRequest.texture->mDesc.mStartState };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, true);
		}
		else
		{
			ASSERT(0 && "Invalid params");
			return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////
// Resource Loader Implementation
//////////////////////////////////////////////////////////////////////////
static bool allQueuesEmpty(ResourceLoader* pLoader)
{
	for (size_t i = 0; i < MAX_GPUS; ++i)
	{
		if (!pLoader->mRequestQueue[i].empty())
		{
			return false;
		}
	}
	return true;
}

static void streamerThreadFunc(void* pThreadData)
{
	ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
	ASSERT(pLoader);

	uint32_t linkedGPUCount = pLoader->pRenderer->mLinkedNodeCount;
	CopyEngine pCopyEngines[MAX_GPUS];
	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		setupCopyEngine(pLoader->pRenderer, &pCopyEngines[i], i, pLoader->mDesc.mBufferSize, pLoader->mDesc.mBufferCount);
	}

	const uint32_t allUploadsCompleted = (1 << linkedGPUCount) - 1;
	uint32_t       completionMask = allUploadsCompleted;
	UpdateState updateState[MAX_GPUS];

	unsigned nextTimeslot = getSystemTime() + pLoader->mDesc.mTimesliceMs;
	SyncToken maxToken[MAX_BUFFER_COUNT] = { 0 };
	size_t activeSet = 0;
	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();
		while (pLoader->mRun && (completionMask == allUploadsCompleted) && allQueuesEmpty(pLoader) && getSystemTime() < nextTimeslot)
		{
			unsigned time = getSystemTime();
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex, nextTimeslot - time);
		}
		pLoader->mQueueMutex.Release();

		for (uint32_t i = 0; i < linkedGPUCount; ++i)
		{
			pLoader->mQueueMutex.Acquire();
			const uint32_t mask = 1 << i;
			if (completionMask & mask)
			{
				if (!pLoader->mRequestQueue[i].empty())
				{
					updateState[i] = pLoader->mRequestQueue[i].front();
					pLoader->mRequestQueue[i].pop_front();
					completionMask &= ~mask;
				}
				else
				{
					updateState[i] = UpdateRequest();
				}
			}

			pLoader->mQueueMutex.Release();

			bool completed = true;
			switch (updateState[i].mRequest.mType)
			{
				case UPDATE_REQUEST_UPDATE_BUFFER:
					completed = updateBuffer(pLoader->pRenderer, &pCopyEngines[i], activeSet, updateState[i]);
					break;
				case UPDATE_REQUEST_UPDATE_TEXTURE:
					completed = updateTexture(pLoader->pRenderer, &pCopyEngines[i], activeSet, updateState[i]);
					break;
				case UPDATE_REQUEST_UPDATE_RESOURCE_STATE:
					completed = updateResourceState(pLoader->pRenderer, &pCopyEngines[i], activeSet, updateState[i]);
					break;
				default: break;
			}
			completionMask |= completed << i;
			if (updateState[i].mRequest.mToken && completed)
			{
				ASSERT(maxToken[activeSet] < updateState[i].mRequest.mToken);
				maxToken[activeSet] = updateState[i].mRequest.mToken;
			}
		}
		
		if (getSystemTime() > nextTimeslot || completionMask == 0)
		{
			for (uint32_t i = 0; i < linkedGPUCount; ++i)
			{
				streamerFlush(&pCopyEngines[i], activeSet);
			}
			
			activeSet = (activeSet + 1) % pLoader->mDesc.mBufferCount;
			for (uint32_t i = 0; i < linkedGPUCount; ++i)
			{
				waitCopyEngineSet(pLoader->pRenderer, &pCopyEngines[i], activeSet);
				resetCopyEngineSet(pLoader->pRenderer, &pCopyEngines[i], activeSet);
			}
			
			SyncToken nextToken = maxToken[activeSet];
			SyncToken prevToken = tfrg_atomic64_load_relaxed(&pLoader->mTokenCompleted);
			// As the only writer atomicity is preserved
			pLoader->mTokenMutex.Acquire();
			tfrg_atomic64_store_release(&pLoader->mTokenCompleted, nextToken > prevToken ? nextToken : prevToken);
			pLoader->mTokenMutex.Release();
			pLoader->mTokenCond.WakeAll();
			nextTimeslot = getSystemTime() + pLoader->mDesc.mTimesliceMs;
		}

	}

	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		streamerFlush(&pCopyEngines[i], activeSet);
		waitQueueIdle(pCopyEngines[i].pQueue);
		cleanupCopyEngine(pLoader->pRenderer, &pCopyEngines[i]);
	}
}

static void addResourceLoader(Renderer* pRenderer, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
{
	ResourceLoader* pLoader = conf_new(ResourceLoader);
	pLoader->pRenderer = pRenderer;

	pLoader->mRun = true;
	pLoader->mDesc = pDesc ? *pDesc : ResourceLoaderDesc{ DEFAULT_BUFFER_SIZE, DEFAULT_BUFFER_COUNT, DEFAULT_TIMESLICE_MS };

	pLoader->mThreadDesc.pFunc = streamerThreadFunc;
	pLoader->mThreadDesc.pData = pLoader;

	pLoader->mThread = create_thread(&pLoader->mThreadDesc);

	*ppLoader = pLoader;
}

static void removeResourceLoader(ResourceLoader* pLoader)
{
	pLoader->mRun = false;
	pLoader->mQueueCond.WakeOne();
	destroy_thread(pLoader->mThread);

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
	uint32_t nodeIndex = pBufferUpdate->pBuffer->mDesc.mNodeIndex;
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pBufferUpdate));
	pLoader->mRequestQueue[nodeIndex].back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = t;
}

static void queueResourceUpdate(ResourceLoader* pLoader, TextureUpdateDescInternal* pTextureUpdate, SyncToken* token)
{
	uint32_t nodeIndex = pTextureUpdate->pTexture->mDesc.mNodeIndex;
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex].back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = t;
}

static void queueResourceUpdate(ResourceLoader* pLoader, Buffer* pBuffer, SyncToken* token)
{
	uint32_t nodeIndex = pBuffer->mDesc.mNodeIndex;
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(pBuffer));
	pLoader->mRequestQueue[nodeIndex].back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = t;
}

static void queueResourceUpdate(ResourceLoader* pLoader, Texture* pTexture, SyncToken* token)
{
	uint32_t nodeIndex = pTexture->mDesc.mNodeIndex;
	pLoader->mQueueMutex.Acquire();
	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;
	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(pTexture));
	pLoader->mRequestQueue[nodeIndex].back().mToken = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = t;
}

static bool isTokenCompleted(ResourceLoader* pLoader, SyncToken token)
{
	bool completed = tfrg_atomic64_load_acquire(&pLoader->mTokenCompleted) >= token;
	return completed;
}

static void waitTokenCompleted(ResourceLoader* pLoader, SyncToken token)
{
	pLoader->mTokenMutex.Acquire();
	while (!isTokenCompleted(token))
	{
		pLoader->mTokenCond.Wait(pLoader->mTokenMutex);
	}
	pLoader->mTokenMutex.Release();
}

static ResourceLoader* pResourceLoader = NULL;

void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc)
{
	addResourceLoader(pRenderer, pDesc, &pResourceLoader);
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

	pBufferDesc->mDesc.mStartState = update ? RESOURCE_STATE_COMMON : pBufferDesc->mDesc.mStartState;
	addBuffer(pResourceLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

	if (update)
	{
		BufferUpdateDesc bufferUpdate(*pBufferDesc->ppBuffer, pBufferDesc->pData);
        bufferUpdate.mSize = pBufferDesc->mDesc.mSize;
		updateResource(&bufferUpdate, token);
	}
	else
	{
		// Transition GPU buffer to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN &&
			pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pBufferDesc->mDesc.mStartState != RESOURCE_STATE_UNDEFINED && pBufferDesc->mDesc.mStartState != RESOURCE_STATE_COMMON))
			queueResourceUpdate(pResourceLoader, *pBufferDesc->ppBuffer, token);
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token)
{
	ASSERT(pTextureDesc->ppTexture);

	bool freeImage = false;
	Image* pImage = NULL;
	if (pTextureDesc->pFilename)
	{
		pImage = ResourceLoader::CreateImage(pTextureDesc->pFilename, NULL, NULL, pTextureDesc->mRoot);
		if (!pImage)
		{
			return;
		}
		freeImage = true;
	}
	else if (!pTextureDesc->pFilename && !pTextureDesc->pRawImageData && !pTextureDesc->pBinaryImageData && pTextureDesc->pDesc)
	{
		// If texture is supposed to be filled later (UAV / Update later / ...) proceed with the mStartState provided by the user in the texture description
		addTexture(pResourceLoader->pRenderer, pTextureDesc->pDesc, pTextureDesc->ppTexture);

		// Transition texture to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pTextureDesc->pDesc->mStartState != RESOURCE_STATE_UNDEFINED && pTextureDesc->pDesc->mStartState != RESOURCE_STATE_COMMON))
			queueResourceUpdate(pResourceLoader, *pTextureDesc->ppTexture, token);
		return;
	}
	else if (pTextureDesc->pRawImageData && !pTextureDesc->pBinaryImageData)
	{
		pImage = ResourceLoader::CreateImage(pTextureDesc->pRawImageData->mFormat, pTextureDesc->pRawImageData->mWidth, pTextureDesc->pRawImageData->mHeight, pTextureDesc->pRawImageData->mDepth, pTextureDesc->pRawImageData->mMipLevels, pTextureDesc->pRawImageData->mArraySize, pTextureDesc->pRawImageData->pRawData);
		freeImage = true;
	}
	else if (pTextureDesc->pBinaryImageData)
	{
		pImage = ResourceLoader::CreateImage(pTextureDesc->pBinaryImageData->pBinaryData, pTextureDesc->pBinaryImageData->mSize, pTextureDesc->pBinaryImageData->pExtension, NULL, NULL);
#ifdef _DEBUG
		bool success = pImage;
#endif
#ifdef _DEBUG
		ASSERT(success);
#endif
		freeImage = true;
	}
	else
		ASSERT(0 && "Invalid params");

	TextureDesc desc = {};
	desc.mFlags = pTextureDesc->mCreationFlag;
	desc.mWidth = pImage->GetWidth();
	desc.mHeight = pImage->GetHeight();
	desc.mDepth = max(1U, pImage->GetDepth());
	desc.mArraySize = pImage->GetArrayCount();

	desc.mMipLevels = pImage->GetMipMapCount();
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mSampleQuality = 0;
	desc.mFormat = pImage->GetFormat();
	desc.mClearValue = ClearValue();
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	desc.pNativeHandle = NULL;
	desc.mHostVisible = false;
	desc.mSrgb = pImage->IsSrgb() || pTextureDesc->mSrgb;
	desc.mNodeIndex = pTextureDesc->mNodeIndex;

	if (pImage->IsCube())
	{
		desc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
		desc.mArraySize *= 6;
	}

	wchar_t         debugName[MAX_PATH] = {};
	eastl::string filename = FileSystem::GetFileNameAndExtension(pImage->GetName());
	mbstowcs(debugName, filename.c_str(), min((size_t)MAX_PATH, filename.size()));
	desc.pDebugName = debugName;

	addTexture(pResourceLoader->pRenderer, &desc, pTextureDesc->ppTexture);

	TextureUpdateDescInternal updateDesc = { *pTextureDesc->ppTexture, pImage, freeImage };
	queueResourceUpdate(pResourceLoader, &updateDesc, token);
}

void updateResource(BufferUpdateDesc* pBufferUpdate, bool batch)
{
	SyncToken token = 0;
	updateResource(pBufferUpdate, &token);
#if defined(DIRECT3D11)
	batch = false;
#endif
	if (!batch) waitTokenCompleted(token);
}

void updateResource(TextureUpdateDesc* pTextureUpdate, bool batch)
{
	SyncToken token = 0;
	updateResource(pTextureUpdate, &token);
#if defined(DIRECT3D11)
	batch = false;
#endif
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
	if (pBufferUpdate->pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		SyncToken updateToken;
		queueResourceUpdate(pResourceLoader, pBufferUpdate, &updateToken);
#if defined(DIRECT3D11)
		waitTokenCompleted(updateToken);
#endif
		if (token) *token = updateToken;
	}
	else
	{
		updateCPUbuffer(pResourceLoader->pRenderer, pBufferUpdate);
	}
}

void updateResource(TextureUpdateDesc* pTextureUpdate, SyncToken* token)
{	
	TextureUpdateDescInternal desc;
	desc.pTexture = pTextureUpdate->pTexture;
	if (pTextureUpdate->pRawImageData)
	{
		Image* pImage = ResourceLoader::CreateImage(pTextureUpdate->pRawImageData->mFormat, pTextureUpdate->pRawImageData->mWidth, pTextureUpdate->pRawImageData->mHeight,
			pTextureUpdate->pRawImageData->mDepth, pTextureUpdate->pRawImageData->mMipLevels, pTextureUpdate->pRawImageData->mArraySize,
			pTextureUpdate->pRawImageData->pRawData);
		desc.mFreeImage = true;
		desc.pImage = pImage;
	}
	else
	{
		ASSERT(false && "TextureUpdateDesc::pRawImageData cannot be NULL");
		return;
	}

	SyncToken updateToken;
	queueResourceUpdate(pResourceLoader, &desc, &updateToken);
#if defined(DIRECT3D11)
	waitTokenCompleted(updateToken);
#endif
	if (token) *token = updateToken;
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
	waitTokenCompleted(pResourceLoader, token);
}

bool isBatchCompleted()
{
	SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
	return isTokenCompleted(pResourceLoader, token);
}

void waitBatchCompleted()
{
	SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
	waitTokenCompleted(pResourceLoader, token);
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
void vk_compileShader(
	Renderer* pRenderer, ShaderStage stage, uint32_t codeSize, const char* code, const eastl::string& outFile, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>* pByteCode, const char* pEntryPoint)
{
	// compile into spir-V shader
	shaderc_compiler_t           compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t	 options = shaderc_compile_options_initialize();
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderc_compile_options_add_macro_definition(options, pMacros[i].definition.c_str(), pMacros[i].definition.length(),
			pMacros[i].value.c_str(), pMacros[i].value.length());
	}
	shaderc_compilation_result_t spvShader =
		shaderc_compile_into_spv(compiler, code, codeSize, getShadercShaderType(stage), "shaderc_error", pEntryPoint ? pEntryPoint : "main", options);
	if (shaderc_result_get_compilation_status(spvShader) != shaderc_compilation_status_success)
	{
		LOGF(LogLevel::eERROR, "Shader compiling failed! with status");
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
	Renderer* pRenderer, ShaderTarget target, const eastl::string& fileName, const eastl::string& outFile, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>* pByteCode, const char* pEntryPoint)
{
	if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
		FileSystem::CreateDir(FileSystem::GetPath(outFile));

	eastl::string                  commandLine;
	eastl::vector<eastl::string> args;
	eastl::string                  configFileName;

	// If there is a config file located in the shader source directory use it to specify the limits
	if (FileSystem::FileExists(FileSystem::GetPath(fileName) + "/config.conf", FSRoot::FSR_Absolute))
	{
		configFileName = FileSystem::GetPath(fileName) + "/config.conf";
		// Add command to compile from Vulkan GLSL to Spirv
		commandLine.append_sprintf(
			"\"%s\" -V \"%s\" -o \"%s\"", configFileName.size() ? configFileName.c_str() : "", fileName.c_str(), outFile.c_str());
	}
	else
	{
		commandLine.append_sprintf("-V \"%s\" -o \"%s\"", fileName.c_str(), outFile.c_str());
	}

	if (target >= shader_target_6_0)
		commandLine += " --target-env vulkan1.1 ";
		//commandLine += " \"-D" + eastl::string("VULKAN") + "=" + "1" + "\"";

	if (pEntryPoint != NULL)
		commandLine.append_sprintf(" -e %s", pEntryPoint);

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

	eastl::string glslangValidator = getenv("VULKAN_SDK");
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
			eastl::string errorLog = errorFile.ReadText();
			ErrorMsg("Failed to compile shader %s with error\n%s", fileName.c_str(), errorLog.c_str());
		}
		errorFile.Close();
	}
}
#endif
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(
	Renderer* pRenderer, const eastl::string& fileName, const eastl::string& outFile, uint32_t macroCount, ShaderMacro* pMacros,
	eastl::vector<char>* pByteCode, const char* /*pEntryPoint*/)
{
	if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
		FileSystem::CreateDir(FileSystem::GetPath(outFile));

	eastl::string xcrun = "/usr/bin/xcrun";
	eastl::string intermediateFile = outFile + ".air";
	eastl::vector<eastl::string> args;
	eastl::string tmpArg = "";

	// Compile the source into a temporary .air file.
	args.push_back("-sdk");
	args.push_back("macosx");
	args.push_back("metal");
	args.push_back("-c");
	tmpArg = eastl::string().sprintf(
		""
		"%s"
		"",
		fileName.c_str());
	args.push_back(tmpArg);
	args.push_back("-o");
	args.push_back(intermediateFile.c_str());

	//enable the 2 below for shader debugging on xcode
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
		tmpArg = eastl::string().sprintf(
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
	uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a, const char *f, int l, const char *sf), uint32_t* pByteCodeSize, char** ppByteCode, const char* pEntryPoint);
#endif

// Function to generate the timestamp of this shader source file considering all include file timestamp
static bool process_source_file(File* original, File* file, time_t& outTimeStamp, eastl::string& outCode)
{
	// If the source if a non-packaged file, store the timestamp
	if (file)
	{
		eastl::string fullName = file->GetName();
		time_t          fileTimeStamp = FileSystem::GetLastModifiedTime(fullName);
		if (fileTimeStamp > outTimeStamp)
			outTimeStamp = fileTimeStamp;
	}

	const eastl::string pIncludeDirective = "#include";
	while (!file->IsEof())
	{
		eastl::string line = file->ReadLine();
		size_t        filePos = line.find(pIncludeDirective, 0);
		const size_t  commentPosCpp = line.find("//", 0);
		const size_t  commentPosC = line.find("/*", 0);

		// if we have an "#include \"" in our current line
		const bool bLineHasIncludeDirective = filePos != eastl::string::npos;
		const bool bLineIsCommentedOut = (commentPosCpp != eastl::string::npos && commentPosCpp < filePos) ||
										 (commentPosC != eastl::string::npos && commentPosC < filePos);

		if (bLineHasIncludeDirective && !bLineIsCommentedOut)
		{
			// get the include file name
			size_t        currentPos = filePos + pIncludeDirective.length();
			eastl::string fileName;
			while (line.at(currentPos++) == ' ')
				;    // skip empty spaces
			if (currentPos >= line.size())
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
			eastl::string includeFileName = FileSystem::GetPath(file->GetName()) + fileName;
			if (FileSystem::GetFileName(includeFileName).at(0) == '<')    // disregard bracketsauthop
				continue;

			// open the include file
			File includeFile = {};
			includeFile.Open(includeFileName, FM_ReadBinary, FSR_Absolute);
			if (!includeFile.IsOpen())
			{
				LOGF(LogLevel::eERROR, "Cannot open #include file: %s", includeFileName.c_str());
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
bool check_for_byte_code(const eastl::string& binaryShaderName, time_t sourceTimeStamp, eastl::vector<char>& byteCode)
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
		LOGF(LogLevel::eERROR, (binaryShaderName + " is not a valid shader bytecode file").c_str());
		return false;
	}

	byteCode.resize(file.GetSize());
	memcpy(byteCode.data(), file.ReadText().c_str(), byteCode.size());
	return true;
}

// Saves bytecode to a file
bool save_byte_code(const eastl::string& binaryShaderName, const eastl::vector<char>& byteCode)
{
	eastl::string path = FileSystem::GetPath(binaryShaderName);
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
	ShaderMacro* pMacros, eastl::vector<char>& byteCode,
	const char* pEntryPoint)
{
	File            shaderSource = {};
	eastl::string code;
	time_t          timeStamp = 0;

#ifndef METAL
	const char* shaderName = fileName;
#else
	// Metal shader files need to have the .metal extension.
	eastl::string metalShaderName = eastl::string(fileName) + ".metal";
	const char* shaderName = metalShaderName.c_str();
#endif

	shaderSource.Open(shaderName, FM_ReadBinary, root);
	ASSERT(shaderSource.IsOpen());

	if (!process_source_file(&shaderSource, &shaderSource, timeStamp, code))
		return false;

	eastl::string name, extension, path;
	FileSystem::SplitPath(fileName, &path, &name, &extension);
	eastl::string shaderDefines;
	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (pMacros[i].definition + pMacros[i].value);
	}

#if 0    //#ifdef _DURANGO
	// Using Durango application data storage requires appmanifest(from application) changes.
	eastl::string binaryShaderName = FileSystem::GetAppPreferencesDir(NULL,NULL) + "/" + pRenderer->pName + "/CompiledShadersBinary/" +
		FileSystem::GetFileName(fileName) + eastl::string().sprintf("_%zu", eastl::hash(shaderDefines)) + extension + ".bin";
#else
	eastl::string rendererApi;
	switch (pRenderer->mSettings.mApi)
	{
		case RENDERER_API_D3D12:
		case RENDERER_API_XBOX_D3D12: rendererApi = "D3D12"; break;
		case RENDERER_API_D3D11: rendererApi = "D3D11"; break;
		case RENDERER_API_VULKAN: rendererApi = "Vulkan"; break;
		case RENDERER_API_METAL: rendererApi = "Metal"; break;
		default: break;
	}

	eastl::string appName(pRenderer->pName);

#ifdef __linux__
	appName.make_lower();
	appName = appName != pRenderer->pName ? appName : appName + "_";
#endif

	eastl::string binaryShaderName = FileSystem::GetProgramDir() + "/" + appName + eastl::string("/Shaders/") + rendererApi +
									 eastl::string().sprintf("/CompiledShadersBinary/") + FileSystem::GetFileName(fileName) +
									 eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + extension +
									 eastl::string().sprintf("%u", target) + ".bin";
#endif

	// Shader source is newer than binary
	if (!check_for_byte_code(binaryShaderName, timeStamp, byteCode))
	{
		if (pRenderer->mSettings.mApi == RENDERER_API_METAL || pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
#if defined(__ANDROID__)
			vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderName, macroCount, pMacros, &byteCode, pEntryPoint);
#else
			vk_compileShader(pRenderer, target, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode, pEntryPoint);
#endif
#elif defined(METAL)
			mtl_compileShader(pRenderer, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode, pEntryPoint);
#endif
		}
		else
		{
#if defined(DIRECT3D12) || defined(DIRECT3D11)
			char*    pByteCode = NULL;
			uint32_t byteCodeSize = 0;
			compileShader(
				pRenderer, target, stage, shaderSource.GetName().c_str(), (uint32_t)code.size(), code.c_str(), macroCount, pMacros, conf_malloc_internal,
				&byteCodeSize, &pByteCode, pEntryPoint);
			byteCode.resize(byteCodeSize);
			memcpy(byteCode.data(), pByteCode, byteCodeSize);
			conf_free(pByteCode);
			if (!save_byte_code(binaryShaderName, byteCode))
			{
				const char* shaderName = shaderSource.GetName().c_str();
				LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", shaderName);
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
bool find_shader_stage(const eastl::string& fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	eastl::string ext = FileSystem::GetExtension(fileName);
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
	else if (ext == ".comp")
	{
		*pOutStage = &pDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
    else if ((ext == ".rgen")    ||
             (ext == ".rmiss")    ||
             (ext == ".rchit")    ||
             (ext == ".rint")    ||
             (ext == ".rahit")    ||
             (ext == ".rcall"))
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
	const eastl::string& fileName, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	eastl::string ext = FileSystem::GetExtension(fileName);
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
	else if (ext == ".comp")
	{
		*pOutStage = &pBinaryDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
    else if ((ext == ".rgen")    ||
             (ext == ".rmiss")    ||
             (ext == ".rchit")    ||
             (ext == ".rint")    ||
             (ext == ".rahit")    ||
             (ext == ".rcall"))
    {
#ifndef METAL
        *pOutStage = &pBinaryDesc->mComp;
        *pStage = SHADER_STAGE_RAYTRACING;
#else
        *pOutStage = &pBinaryDesc->mComp;
        *pStage = SHADER_STAGE_COMP;
#endif
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
	if (pDesc->mTarget > pRenderer->mSettings.mShaderTarget)
	{
		eastl::string error = eastl::string().sprintf("Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)pDesc->mTarget, (uint32_t)pRenderer->mSettings.mShaderTarget);
		LOGF(eERROR, error.c_str());
		_FailedAssert(__FILE__, __LINE__, error.c_str());
		return;
	}

#ifndef TARGET_IOS
	BinaryShaderDesc      binaryDesc = {};
	eastl::vector<char> byteCodes[SHADER_STAGE_COUNT] = {};
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		const RendererShaderDefinesDesc rendererDefinesDesc = get_renderer_shaderdefines(pRenderer);

		if (pDesc->mStages[i].mFileName.size() != 0)
		{
			eastl::string filename = pDesc->mStages[i].mFileName;
			// For absolute path, ignore the shader root path
			if (pDesc->mStages[i].mRoot != FSR_SrcShaders && pDesc->mStages[i].mRoot != FSR_Absolute)
				filename = FileSystem::GetRootPath(FSR_SrcShaders) + filename;

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			if (find_shader_stage(filename, &binaryDesc, &pStage, &stage))
			{
				const uint32_t macroCount = pDesc->mStages[i].mMacroCount + rendererDefinesDesc.rendererShaderDefinesCnt;
				eastl::vector<ShaderMacro> macros(macroCount);
				for (uint32_t macro = 0; macro < rendererDefinesDesc.rendererShaderDefinesCnt; ++macro)
					macros[macro] = rendererDefinesDesc.rendererShaderDefines[macro];
				for (uint32_t macro = 0; macro < pDesc->mStages[i].mMacroCount; ++macro)
					macros[rendererDefinesDesc.rendererShaderDefinesCnt + macro] = pDesc->mStages[i].pMacros[macro];

				if (!load_shader_stage_byte_code(
						pRenderer, pDesc->mTarget, stage, filename.c_str(), pDesc->mStages[i].mRoot, macroCount, macros.data(),
						byteCodes[i], pDesc->mStages[i].mEntryPointName))
					return;

				binaryDesc.mStages |= stage;
				pStage->pByteCode = byteCodes[i].data();
				pStage->mByteCodeSize = (uint32_t)byteCodes[i].size();
#if defined(METAL)
                if (pDesc->mStages[i].mEntryPointName)
                    pStage->mEntryPoint = pDesc->mStages[i].mEntryPointName;
                else
                    pStage->mEntryPoint = "stageMain";
				// In metal, we need the shader source for our reflection system.
				File metalFile = {};
				metalFile.Open(filename + ".metal", FM_ReadBinary, pDesc->mStages[i].mRoot);
				pStage->mSource = metalFile.ReadText();
				metalFile.Close();
#else
				if (pDesc->mStages[i].mEntryPointName)
					pStage->mEntryPoint = pDesc->mStages[i].mEntryPointName;
				else
					pStage->mEntryPoint = "main";
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
			eastl::string filename = pDesc->mStages[i].mFileName;
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
				time_t timestamp = 0;
				process_source_file(&shaderSource, &shaderSource, timestamp, pStage->mCode);
                if (pDesc->mStages[i].mEntryPointName)
                    pStage->mEntryPoint = pDesc->mStages[i].mEntryPointName;
                else
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
