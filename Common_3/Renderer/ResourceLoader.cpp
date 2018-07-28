/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

//this is needed for unix as PATH_MAX is defined intead of MAX_PATH
#ifndef _WIN32
//linux needs limits.h for PATH_MAX
#ifdef LINUX
#include<limits.h> 
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
extern void removeTexture(Renderer* pRenderer, Texture*p_texture);
extern void cmdUpdateBuffer(Cmd* p_cmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* p_src_buffer, Buffer* p_buffer);
extern void cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture);
extern const RendererShaderDefinesDesc get_renderer_shaderdefines(Renderer* pRenderer);
#endif
//////////////////////////////////////////////////////////////////////////
// Resource Loader Defines
//////////////////////////////////////////////////////////////////////////
#define RESOURCE_BUFFER_ALIGNMENT 4U
#if defined(DIRECT3D12)
#define RESOURCE_TEXTURE_ALIGNMENT 512U
#else
#define RESOURCE_TEXTURE_ALIGNMENT 16U
#endif

#define MAX_LOAD_THREADS 3U
#define MAX_COPY_THREADS 1U
//////////////////////////////////////////////////////////////////////////
// Resource Loader Structures
//////////////////////////////////////////////////////////////////////////
typedef struct MappedMemoryRange
{
	void* pData;
	Buffer* pBuffer;
	uint64_t mOffset;
	uint64_t mSize;
} MappedMemoryRange;

typedef struct ResourceLoader
{
	Renderer* pRenderer;
	Buffer* pStagingBuffer;
	uint64_t mCurrentPos;

	Cmd* pCopyCmd[MAX_GPUS];
	Cmd* pBatchCopyCmd[MAX_GPUS];
	CmdPool* pCopyCmdPool[MAX_GPUS];

	tinystl::vector<Buffer*> mTempStagingBuffers;
    
    bool mOpen = false;
} ResourceLoader;

typedef struct ResourceThread
{
	Renderer* pRenderer;
	ResourceLoader* pLoader;
	WorkItem* pItem;
	uint64_t mMemoryBudget;
} ResourceThread;
//////////////////////////////////////////////////////////////////////////
// Resource Loader Internal Functions
//////////////////////////////////////////////////////////////////////////
static void addResourceLoader (Renderer* pRenderer, uint64_t mSize, ResourceLoader** ppLoader, Queue* pCopyQueue)
{
	ResourceLoader* pLoader = (ResourceLoader*)conf_calloc(1, sizeof(*pLoader));
	pLoader->pRenderer = pRenderer;

	BufferDesc bufferDesc = {};
	bufferDesc.mUsage = BUFFER_USAGE_UPLOAD;
	bufferDesc.mSize = mSize;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addBuffer (pRenderer, &bufferDesc, &pLoader->pStagingBuffer);

	addCmdPool(pLoader->pRenderer, pCopyQueue, false, &pLoader->pCopyCmdPool[0]);
	addCmd(pLoader->pCopyCmdPool[0], false, &pLoader->pCopyCmd[0]);
	addCmd(pLoader->pCopyCmdPool[0], false, &pLoader->pBatchCopyCmd[0]);

	*ppLoader = pLoader;
}

static void cleanupResourceLoader(ResourceLoader* pLoader)
{
	for (uint32_t i = 0; i < (uint32_t)pLoader->mTempStagingBuffers.size(); ++i)
		removeBuffer(pLoader->pRenderer, pLoader->mTempStagingBuffers[i]);

	pLoader->mTempStagingBuffers.clear();
}

static void removeResourceLoader(ResourceLoader* pLoader)
{
	removeBuffer(pLoader->pRenderer, pLoader->pStagingBuffer);
	cleanupResourceLoader(pLoader);

	for (uint32_t i = 0; i < MAX_GPUS; ++i)
	{
		if (i == 0)
			removeCmd(pLoader->pCopyCmdPool[i], pLoader->pBatchCopyCmd[i]);

		if (pLoader->pCopyCmdPool[i])
		{
			removeCmd(pLoader->pCopyCmdPool[i], pLoader->pCopyCmd[i]);
			removeCmdPool(pLoader->pRenderer, pLoader->pCopyCmdPool[i]);
		}
	}

	pLoader->mTempStagingBuffers.~vector();

	conf_free(pLoader);
}

static ResourceState util_determine_resource_start_state(TextureUsage usage)
{
	ResourceState state = RESOURCE_STATE_UNDEFINED;
	if (usage & TEXTURE_USAGE_UNORDERED_ACCESS)
		return RESOURCE_STATE_UNORDERED_ACCESS;
	else if (usage & TEXTURE_USAGE_SAMPLED_IMAGE)
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
		BufferUsage usage = pBuffer->mUsage;

		// Try to limit number of states used overall to avoid sync complexities
		if (usage & BUFFER_USAGE_STORAGE_UAV)
			return RESOURCE_STATE_UNORDERED_ACCESS;
		if ((usage & BUFFER_USAGE_VERTEX) || (usage & BUFFER_USAGE_UNIFORM))
			return RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if (usage & BUFFER_USAGE_INDEX)
			return RESOURCE_STATE_INDEX_BUFFER;
		if ((usage & BUFFER_USAGE_STORAGE_SRV))
			return RESOURCE_STATE_SHADER_RESOURCE;

		return RESOURCE_STATE_COMMON;
	}
	// Host Cached (Readback Heap)
	else
	{
		return RESOURCE_STATE_COPY_DEST;
	}
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the loader ran out of memory
static MappedMemoryRange consumeResourceLoaderMemory(uint64_t memoryRequirement, uint32_t alignment, ResourceLoader* pLoader)
{
	if (alignment != 0 && pLoader->mCurrentPos % alignment != 0)
		pLoader->mCurrentPos = round_up_64(pLoader->mCurrentPos, alignment);

	if (memoryRequirement > pLoader->pStagingBuffer->mDesc.mSize ||
		pLoader->mCurrentPos + memoryRequirement > pLoader->pStagingBuffer->mDesc.mSize)
	{
		// Try creating a temporary staging buffer which we will clean up after resource is uploaded
		Buffer* tempStagingBuffer = NULL;
		BufferDesc desc = {};
		desc.mUsage = BUFFER_USAGE_UPLOAD;
		desc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		desc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		desc.mSize = memoryRequirement;
		addBuffer(pLoader->pRenderer, &desc, &tempStagingBuffer);

		if (tempStagingBuffer)
		{
			pLoader->mTempStagingBuffers.emplace_back(tempStagingBuffer);
			return { tempStagingBuffer->pCpuMappedAddress, tempStagingBuffer, 0, memoryRequirement };
		}
		else
		{
			LOGERRORF("Failed to allocate memory (%llu) for resource", memoryRequirement);
			return { NULL };
		}
	}

	void* pDstData = (uint8_t*) pLoader->pStagingBuffer->pCpuMappedAddress + pLoader->mCurrentPos;
	uint64_t currentOffset = pLoader->mCurrentPos;
	pLoader->mCurrentPos += memoryRequirement;
	return { pDstData, pLoader->pStagingBuffer, currentOffset, memoryRequirement };
}

/// 
static MappedMemoryRange consumeResourceUpdateMemory(uint64_t memoryRequirement, uint32_t alignment, ResourceLoader* pLoader)
{
	if (alignment != 0 && pLoader->mCurrentPos % alignment != 0)
		pLoader->mCurrentPos = round_up_64(pLoader->mCurrentPos, alignment);

	if (memoryRequirement > pLoader->pStagingBuffer->mDesc.mSize)
	{
		LOGERRORF("ResourceLoader::updateResource Out of memory");
		return { 0 };
	}

	if (pLoader->mCurrentPos + memoryRequirement > pLoader->pStagingBuffer->mDesc.mSize)
	{
		//LOGINFO ("ResourceLoader::Reached end of staging buffer. Reseting staging buffer");
		pLoader->mCurrentPos = 0;
	}

	void* pDstData = (uint8_t*)pLoader->pStagingBuffer->pCpuMappedAddress + pLoader->mCurrentPos;
	uint64_t currentOffset = pLoader->mCurrentPos;
	pLoader->mCurrentPos += memoryRequirement;
	return{ pDstData, pLoader->pStagingBuffer, currentOffset, memoryRequirement };
}

static void cmdLoadBuffer(Cmd* pCmd, BufferLoadDesc* pBufferDesc, ResourceLoader* pLoader)
{
	ASSERT (pBufferDesc->ppBuffer);

	if (pBufferDesc->pData || pBufferDesc->mForceReset)
	{
		pBufferDesc->mDesc.mStartState = RESOURCE_STATE_COMMON;
		addBuffer(pLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

		Buffer* pBuffer = *pBufferDesc->ppBuffer;
		ASSERT(pBuffer);

		if (pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
		{
			MappedMemoryRange range = consumeResourceLoaderMemory(pBufferDesc->mDesc.mSize, RESOURCE_BUFFER_ALIGNMENT, pLoader);
			ASSERT(range.pData);
			ASSERT(range.pBuffer);

			if (pBufferDesc->pData)
				memcpy(range.pData, pBufferDesc->pData, pBuffer->mDesc.mSize);
			else
				memset(range.pData, NULL, pBuffer->mDesc.mSize);

			cmdUpdateBuffer(pCmd, range.mOffset, pBuffer->mPositionInHeap, range.mSize, range.pBuffer, pBuffer);
		}
		else
		{
			if (pBufferDesc->mDesc.mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
			{
				if (pBufferDesc->pData)
					memcpy(pBuffer->pCpuMappedAddress, pBufferDesc->pData, pBuffer->mDesc.mSize);
				else
					memset(pBuffer->pCpuMappedAddress, NULL, pBuffer->mDesc.mSize);
			}
			else
			{
				mapBuffer(pLoader->pRenderer, pBuffer, NULL);
				if (pBufferDesc->pData)
					memcpy(pBuffer->pCpuMappedAddress, pBufferDesc->pData, pBuffer->mDesc.mSize);
				else
					memset(pBuffer->pCpuMappedAddress, NULL, pBuffer->mDesc.mSize);
				unmapBuffer(pLoader->pRenderer, pBuffer);
			}
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
	}
	else
	{
		pBufferDesc->mDesc.mStartState = util_determine_resource_start_state(&pBufferDesc->mDesc);
		addBuffer(pLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);
	}
}

static void* imageLoadAllocationFunc(Image* pImage, uint64_t memoryRequirement, void* pUserData)
{
  UNREF_PARAM(pImage);
	ResourceLoader* pLoader = (ResourceLoader*) pUserData;
	ASSERT (pLoader);

	MappedMemoryRange range = consumeResourceLoaderMemory(memoryRequirement, 0, pLoader);
	return range.pData;
}

static void cmd_upload_texture_data(Cmd* pCmd, ResourceLoader* pLoader, Texture* pTexture, const Image& img)
{
	ASSERT(pTexture);

	MappedMemoryRange range = consumeResourceLoaderMemory(pTexture->mTextureSize, RESOURCE_TEXTURE_ALIGNMENT, pLoader);

	// create source subres data structs
	SubresourceDataDesc texData[1024];
	SubresourceDataDesc *dest = texData;
	uint nSlices = img.IsCube() ? 6 : 1;

#if defined(DIRECT3D12) || defined(METAL)
	if (pCmd->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12 || pCmd->pRenderer->mSettings.mApi == RENDERER_API_D3D12 || pCmd->pRenderer->mSettings.mApi == RENDERER_API_METAL)
	{
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
	}
#endif
#if defined(VULKAN)
	if (pCmd->pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
	{
		uint offset = 0;
		for (uint i = 0; i < img.GetMipMapCount(); ++i)
		{
			for (uint k = 0; k < nSlices; ++k)
			{
				uint pitch, slicePitch, dataSize;
				if (ImageFormat::IsCompressedFormat(img.getFormat()))
				{
					dataSize = ImageFormat::GetBytesPerBlock(img.getFormat());
					pitch = ((img.GetWidth(i) + 3) >> 2);
					slicePitch = ((img.GetHeight(i) + 3) >> 2);
				}
				else
				{
					dataSize = ImageFormat::GetBytesPerPixel(img.getFormat());
					pitch = img.GetWidth(i);
					slicePitch = img.GetHeight(i);
				}

				dest->mMipLevel = i;
				dest->mArrayLayer = k;
				dest->mBufferOffset = range.mOffset + offset;
				dest->mWidth = img.GetWidth(i);
				dest->mHeight = img.GetHeight(i);
				dest->mDepth = img.GetDepth(i);
				dest->mArraySize = img.GetArrayCount();
				++dest;

				for (uint n = 0; n < img.GetArrayCount(); ++n)
				{
					uint8_t* pSrcData = (uint8_t*)img.GetPixels(i, n) + k * slicePitch * pitch * dataSize;
					memcpy((uint8_t*)range.pData + offset, pSrcData, (img.GetMipMappedSize(i, 1) / nSlices));
					offset += (img.GetMipMappedSize(i, 1) / nSlices);
				}
			}
		}
	}
#endif

	// calculate number of subresources
	int numSubresources = (int)(dest - texData);
	cmdUpdateSubresources(pCmd, 0, numSubresources, texData, range.pBuffer, range.mOffset, pTexture);
}

static void cmdLoadTextureFile(Cmd* pCmd, TextureLoadDesc* pTextureFileDesc, ResourceLoader* pLoader)
{
	ASSERT (pTextureFileDesc->ppTexture);

	Image img;
	//uint64_t pos = pLoader->mCurrentPos; //unused
	bool res = img.loadImage(pTextureFileDesc->pFilename, pTextureFileDesc->mUseMipmaps, imageLoadAllocationFunc, pLoader, pTextureFileDesc->mRoot);
	if (res)
	{
		TextureType textureType = TEXTURE_TYPE_2D;
		if (img.Is3D())
			textureType = TEXTURE_TYPE_3D;
		else if (img.IsCube())
			textureType = TEXTURE_TYPE_CUBE;

		TextureDesc desc = {};
		desc.mType = textureType;
		desc.mFlags = TEXTURE_CREATION_FLAG_NONE;
		desc.mWidth = img.GetWidth();
		desc.mHeight = img.GetHeight();
		desc.mDepth = img.GetDepth();
		desc.mArraySize = img.GetArrayCount();
		desc.mMipLevels = img.GetMipMapCount();
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mSampleQuality = 0;
		desc.mFormat = img.getFormat();
		desc.mClearValue = ClearValue();
		desc.mUsage = TEXTURE_USAGE_SAMPLED_IMAGE;
		desc.mStartState = RESOURCE_STATE_COMMON;
		desc.pNativeHandle = NULL;
		desc.mSrgb = pTextureFileDesc->mSrgb;
		desc.mHostVisible = false;
		desc.mNodeIndex = pTextureFileDesc->mNodeIndex;

		wchar_t debugName[MAX_PATH] = {};
		String filename = FileSystem::GetFileNameAndExtension(img.GetName());
		mbstowcs(debugName, filename.c_str(), min((size_t)MAX_PATH, filename.size()));
		desc.pDebugName = debugName;

		addTexture(pLoader->pRenderer, &desc, pTextureFileDesc->ppTexture);

		// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
		if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
		{
			TextureBarrier preCopyBarrier = { *pTextureFileDesc->ppTexture, RESOURCE_STATE_COPY_DEST };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier, false);
		}

		cmd_upload_texture_data(pCmd, pLoader, *pTextureFileDesc->ppTexture, img);

		// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
		if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
		{
			TextureBarrier postCopyBarrier = { *pTextureFileDesc->ppTexture, util_determine_resource_start_state(desc.mUsage) };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier, true);
		}
	}
	else
	{
		*pTextureFileDesc->ppTexture = NULL;
	}

	img.Destroy();
}

static void cmdLoadTextureImage(Cmd* pCmd, TextureLoadDesc* pTextureImage, ResourceLoader* pLoader)
{
	ASSERT(pTextureImage->ppTexture);
	Image& img = *pTextureImage->pImage;

	TextureType textureType = TEXTURE_TYPE_2D;
	if (img.Is3D())
		textureType = TEXTURE_TYPE_3D;
	else if (img.IsCube())
		textureType = TEXTURE_TYPE_CUBE;

	TextureDesc desc = {};
	desc.mType = textureType;
	desc.mFlags = TEXTURE_CREATION_FLAG_NONE;
	desc.mWidth = img.GetWidth();
	desc.mHeight = img.GetHeight();
	desc.mDepth = img.GetDepth();
	desc.mArraySize = img.GetArrayCount();
	desc.mMipLevels = img.GetMipMapCount();
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mSampleQuality = 0;
	desc.mFormat = img.getFormat();
	desc.mClearValue = ClearValue();
	desc.mUsage = TEXTURE_USAGE_SAMPLED_IMAGE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	desc.pNativeHandle = NULL;
	desc.mHostVisible = false;
	desc.mSrgb = pTextureImage->mSrgb;
	desc.mNodeIndex = pTextureImage->mNodeIndex;

	wchar_t debugName[MAX_PATH] = {};
	String filename = FileSystem::GetFileNameAndExtension(img.GetName());
	mbstowcs(debugName, filename.c_str(), min((size_t)MAX_PATH, filename.size()));
	desc.pDebugName = debugName;

	addTexture(pLoader->pRenderer, &desc, pTextureImage->ppTexture);

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
	if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		TextureBarrier preCopyBarrier = { *pTextureImage->ppTexture, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier, false);
	}

	cmd_upload_texture_data(pCmd, pLoader, *pTextureImage->ppTexture, *pTextureImage->pImage);

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
	if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		TextureBarrier postCopyBarrier = { *pTextureImage->ppTexture, util_determine_resource_start_state(desc.mUsage) };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier, true);
	}
}

static void cmdLoadEmptyTexture(Cmd* pCmd, TextureLoadDesc* pEmptyTexture, ResourceLoader* pLoader)
{
	ASSERT(pEmptyTexture->ppTexture);
	pEmptyTexture->pDesc->mStartState = util_determine_resource_start_state(pEmptyTexture->pDesc->mUsage);
	addTexture(pLoader->pRenderer, pEmptyTexture->pDesc, pEmptyTexture->ppTexture);

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
	if (pLoader->pRenderer->mSettings.mApi == RENDERER_API_VULKAN || pLoader->pRenderer->mSettings.mApi == RENDERER_API_XBOX_D3D12)
	{
		TextureBarrier barrier = { *pEmptyTexture->ppTexture, pEmptyTexture->pDesc->mStartState };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, true);
	}
}

static void cmdLoadResource(Cmd* pCmd, ResourceLoadDesc* pResourceLoadDesc, ResourceLoader* pLoader)
{
	switch (pResourceLoadDesc->mType)
	{
	case RESOURCE_TYPE_BUFFER:
		cmdLoadBuffer(pCmd, &pResourceLoadDesc->buf, pLoader);
		break;
	case RESOURCE_TYPE_TEXTURE:
		if (pResourceLoadDesc->tex.pFilename)
			cmdLoadTextureFile(pCmd, &pResourceLoadDesc->tex, pLoader);
		else if (pResourceLoadDesc->tex.pImage)
			cmdLoadTextureImage(pCmd, &pResourceLoadDesc->tex, pLoader);
		else
			cmdLoadEmptyTexture(pCmd, &pResourceLoadDesc->tex, pLoader);
		break;
	default:
		break;
	}
}

static void cmdUpdateResource(Cmd* pCmd, BufferUpdateDesc* pBufferUpdate, ResourceLoader* pLoader)
{
	Buffer* pBuffer = pBufferUpdate->pBuffer;
    const uint64_t bufferSize = (pBufferUpdate->mSize > 0) ? pBufferUpdate->mSize : pBuffer->mDesc.mSize;
	const uint64_t alignment = pBuffer->mDesc.mUsage & BUFFER_USAGE_UNIFORM ? pLoader->pRenderer->pActiveGpuSettings->mUniformBufferAlignment : 16;
	const uint64_t offset = round_up_64(pBufferUpdate->mDstOffset, alignment);

    void* pDstBufferAddress = (uint8_t*)(pBuffer->pCpuMappedAddress) + offset;
    
	void* pSrcBufferAddress = NULL;
    if (pBufferUpdate->pData)
        pSrcBufferAddress = (uint8_t*)(pBufferUpdate->pData) + pBufferUpdate->mSrcOffset;

	// If buffer is mapped on CPU memory, just do a memcpy
	if (pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
	{
		if (pBuffer->mDesc.mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		{
			if (pSrcBufferAddress)
				memcpy(pDstBufferAddress, pSrcBufferAddress, bufferSize);
			else
				memset(pDstBufferAddress, NULL, bufferSize);
		}
		else
		{
			if (!pBuffer->pCpuMappedAddress)
				mapBuffer(pLoader->pRenderer, pBuffer, NULL);

			if (pSrcBufferAddress)
				memcpy(pDstBufferAddress, pSrcBufferAddress, bufferSize);
			else
				memset(pDstBufferAddress, NULL, bufferSize);
		}
	}
	// If buffer is only in Device Local memory, stage an update from the pre-allocated staging buffer
	else
	{
		MappedMemoryRange range = consumeResourceUpdateMemory(bufferSize, RESOURCE_BUFFER_ALIGNMENT, pLoader);
		ASSERT(range.pData);

        if (pSrcBufferAddress)
            memcpy(range.pData, pSrcBufferAddress, range.mSize);
		else
			memset(range.pData, NULL, range.mSize);

		cmdUpdateBuffer(pCmd, range.mOffset, pBuffer->mPositionInHeap + offset, 
            bufferSize, range.pBuffer, pBuffer);
	}
}

static void cmdUpdateResource(Cmd* pCmd, TextureUpdateDesc* pTextureUpdate, ResourceLoader* pLoader)
{
	cmd_upload_texture_data(pCmd, pLoader, pTextureUpdate->pTexture, *pTextureUpdate->pImage);
}

void cmdUpdateResource(Cmd* pCmd, ResourceUpdateDesc* pResourceUpdate, ResourceLoader* pLoader)
{
	switch (pResourceUpdate->mType)
	{
	case RESOURCE_TYPE_BUFFER:
		cmdUpdateResource(pCmd, &pResourceUpdate->buf, pLoader);
		break;
	case RESOURCE_TYPE_TEXTURE:
		cmdUpdateResource(pCmd, &pResourceUpdate->tex, pLoader);
		break;
	default:
		break;
	}
}
//////////////////////////////////////////////////////////////////////////
// Resource Loader Globals
//////////////////////////////////////////////////////////////////////////
static Queue* pCopyQueue[MAX_GPUS] = { NULL };
static Fence* pWaitFence[MAX_GPUS] = { NULL };

static ResourceLoader* pMainResourceLoader = NULL;
static ThreadPool* pThreadPool = NULL;
static tinystl::vector <ResourceThread*> gResourceThreads;
static tinystl::vector <ResourceLoadDesc*> gResourceQueue;
static Mutex gResourceQueueMutex;
static bool gFinishLoading = false;
static bool gUseThreads = false;
//////////////////////////////////////////////////////////////////////////
// Resource Loader Implementation
//////////////////////////////////////////////////////////////////////////
static void loadThread(void* pThreadData)
{
	ResourceThread* pThread = (ResourceThread*) pThreadData;
	ASSERT (pThread);

	ResourceLoader* pLoader = pThread->pLoader;

	Cmd* pCmd = pLoader->pCopyCmd[0];
	beginCmd(pCmd);

	while (true)
	{
		if (gFinishLoading)
			break;

		gResourceQueueMutex.Acquire();
		if (!gResourceQueue.empty())
		{
			ResourceLoadDesc* item = gResourceQueue.front();
			gResourceQueue.erase(gResourceQueue.begin());
			gResourceQueueMutex.Release();
			cmdLoadResource(pCmd, item, pLoader);
			if (item->mType == RESOURCE_TYPE_TEXTURE && item->tex.pFilename)
				conf_free((char*)item->tex.pFilename);
			conf_free(item);
		}
		else
		{
			gResourceQueueMutex.Release();
			Thread::Sleep (0);
		}
	}

	endCmd(pCmd);
}

void initResourceLoaderInterface(Renderer* pRenderer, uint64_t memoryBudget, bool useThreads)
{
	uint32_t numCores = Thread::GetNumCPUCores();

	gUseThreads = useThreads && numCores > 1;
	// Threaded loading will wreak havoc w/ DX11 so disable it

#if defined(DIRECT3D11) || defined(TARGET_IOS)
	gUseThreads = false;
#endif

	memset(pCopyQueue, 0, sizeof(pCopyQueue));
	memset(pWaitFence, 0, sizeof(pWaitFence));

	QueueDesc desc = { QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, CMD_POOL_COPY };
	addQueue(pRenderer, &desc, &pCopyQueue[0]);
	addFence(pRenderer, &pWaitFence[0]);

	if (gUseThreads)
	{
		uint32_t numLoaders = min (MAX_LOAD_THREADS, numCores - 1);

		pThreadPool = conf_placement_new<ThreadPool>(conf_calloc(1, sizeof(ThreadPool)));
		pThreadPool->CreateThreads(numLoaders);

		for (unsigned i = 0; i < numLoaders; ++i)
		{
			WorkItem* pItem = (WorkItem*)conf_calloc(1, sizeof(*pItem));

			pItem->pFunc = loadThread;

			ResourceThread* thread = (ResourceThread*)conf_calloc(1, sizeof(*thread));

			addResourceLoader(pRenderer, memoryBudget, &thread->pLoader, pCopyQueue[0]);

			thread->pRenderer = pRenderer;
			// Consider worst case budget for each thread
			thread->mMemoryBudget = memoryBudget;
			thread->pItem = pItem;
			gResourceThreads.push_back(thread);

			pItem->pData = gResourceThreads [i];

			pThreadPool->AddWorkItem (pItem);
		}
	}

	addResourceLoader(pRenderer, memoryBudget, &pMainResourceLoader, pCopyQueue[0]);
}

void removeResourceLoaderInterface(Renderer* pRenderer)
{
	gResourceThreads.clear();
	removeResourceLoader(pMainResourceLoader);

	for (uint32_t i = 0; i < MAX_GPUS; ++i)
	{
		if (pCopyQueue[i])
			removeQueue(pCopyQueue[i]);
		if (pWaitFence[i])
			removeFence(pRenderer, pWaitFence[i]);
	}
}

void addResource(ResourceLoadDesc* pResourceLoadDesc, bool threaded /* = false */)
{
	if (threaded && !gUseThreads)
	{
		LOGWARNING("Threaded Option specified for loading but no threads were created in initResourceLoaderInterface - Using single threaded loading");
	}

	if (!threaded || !gUseThreads)
	{
		uint32_t nodeIndex = 0;
		if (pResourceLoadDesc->mType == RESOURCE_TYPE_BUFFER)
		{
			nodeIndex = pResourceLoadDesc->buf.mDesc.mNodeIndex;
		}
		else if (pResourceLoadDesc->mType == RESOURCE_TYPE_TEXTURE)
		{
			if (pResourceLoadDesc->tex.pFilename || pResourceLoadDesc->tex.pImage)
				nodeIndex = pResourceLoadDesc->tex.mNodeIndex;
			else
				nodeIndex = pResourceLoadDesc->tex.pDesc->mNodeIndex;
		}

		gResourceQueueMutex.Acquire();
		if (!pCopyQueue[nodeIndex])
		{
			QueueDesc queueDesc = {};
			queueDesc.mType = CMD_POOL_COPY;
			queueDesc.mNodeIndex = nodeIndex;
			addQueue(pMainResourceLoader->pRenderer, &queueDesc, &pCopyQueue[nodeIndex]);
			addCmdPool(pMainResourceLoader->pRenderer, pCopyQueue[nodeIndex], false, &pMainResourceLoader->pCopyCmdPool[nodeIndex]);
			addCmd(pMainResourceLoader->pCopyCmdPool[nodeIndex], false, &pMainResourceLoader->pCopyCmd[nodeIndex]);
		}
		if (!pWaitFence[nodeIndex])
			addFence(pMainResourceLoader->pRenderer, &pWaitFence[nodeIndex]);

		Queue* pQueue = pCopyQueue[nodeIndex];
		Cmd* pCmd = pMainResourceLoader->pCopyCmd[nodeIndex];
		Fence* pFence = pWaitFence[nodeIndex];
		beginCmd(pCmd);
		cmdLoadResource(pCmd, pResourceLoadDesc, pMainResourceLoader);
		endCmd(pCmd);

		queueSubmit(pQueue, 1, &pCmd, pFence, 0, 0, 0, 0);
		waitForFences(pQueue, 1, &pFence, false);
		cleanupResourceLoader(pMainResourceLoader);

		pMainResourceLoader->mCurrentPos = 0;
		gResourceQueueMutex.Release();
	}
	else
	{
		ResourceLoadDesc* pResource = (ResourceLoadDesc*)conf_malloc(sizeof(*pResourceLoadDesc));
		memcpy(pResource, pResourceLoadDesc, sizeof(*pResourceLoadDesc));
		gResourceQueueMutex.Acquire();
		gResourceQueue.emplace_back(pResource);
		gResourceQueueMutex.Release();
	}
}

void addResource(BufferLoadDesc* pBuffer, bool threaded)
{
	ResourceLoadDesc resourceDesc = *pBuffer;
	addResource(&resourceDesc, threaded);
}

void addResource(TextureLoadDesc* pTexture, bool threaded)
{
	if (threaded && !gUseThreads)
	{
		LOGWARNING("Threaded Option specified for loading but no threads were created in initResourceLoaderInterface - Using single threaded loading");
	}

	if (!threaded || !gUseThreads)
	{
		ResourceLoadDesc res = { *pTexture };
		addResource(&res, threaded);
	}
	else
	{
		ResourceLoadDesc* pResource = (ResourceLoadDesc*)conf_malloc(sizeof(ResourceLoadDesc));
		pResource->mType = RESOURCE_TYPE_TEXTURE;
		pResource->tex = *pTexture;
		if (pTexture->pFilename)
		{
			pResource->tex.pFilename = (char*)conf_calloc(strlen(pTexture->pFilename) + 1, sizeof(char));
			memcpy((char*)pResource->tex.pFilename, pTexture->pFilename, strlen(pTexture->pFilename));
		}
		gResourceQueueMutex.Acquire();
		gResourceQueue.emplace_back(pResource);
		gResourceQueueMutex.Release();
	}
}

void updateResource(BufferUpdateDesc* pBufferUpdate, bool batch /* = false*/)
{
	if (pBufferUpdate->pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBufferUpdate->pBuffer->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
	{
        if (!batch)
        {
            // TODO : Make updating resources thread safe. This lock is a temporary solution
            MutexLock lock(gResourceQueueMutex);

			Cmd* pCmd = pMainResourceLoader->pCopyCmd[0];

            beginCmd(pCmd);
            cmdUpdateResource(pCmd, pBufferUpdate, pMainResourceLoader);
            endCmd(pCmd);

            queueSubmit(pCopyQueue[0], 1, &pCmd, pWaitFence[0], 0, 0, 0, 0);
            waitForFences(pCopyQueue[0], 1, &pWaitFence[0], false);
			cleanupResourceLoader(pMainResourceLoader);
        }
        else
        {
			Cmd* pCmd = pMainResourceLoader->pBatchCopyCmd[0];

            if (!pMainResourceLoader->mOpen)
            {
                beginCmd(pCmd);
                pMainResourceLoader->mOpen = true;
            }
            
            cmdUpdateResource(pCmd, pBufferUpdate, pMainResourceLoader);
        }
	}
	else
	{
		cmdUpdateResource(NULL, pBufferUpdate, pMainResourceLoader);
	}
}

void updateResource(TextureUpdateDesc* pTextureUpdate, bool batch)
{
	if (!batch)
	{
		// TODO : Make updating resources thread safe. This lock is a temporary solution
		MutexLock lock(gResourceQueueMutex);

		Cmd* pCmd = pMainResourceLoader->pCopyCmd[0];

		beginCmd(pCmd);
		cmdUpdateResource(pCmd, pTextureUpdate, pMainResourceLoader);
		endCmd(pCmd);

		queueSubmit(pCopyQueue[0], 1, &pCmd, pWaitFence[0], 0, 0, 0, 0);
		waitForFences(pCopyQueue[0], 1, &pWaitFence[0], false);
		cleanupResourceLoader(pMainResourceLoader);
	}
	else
	{
		Cmd* pCmd = pMainResourceLoader->pBatchCopyCmd[0];

		if (!pMainResourceLoader->mOpen)
		{
			beginCmd(pCmd);
			pMainResourceLoader->mOpen = true;
		}

		cmdUpdateResource(pCmd, pTextureUpdate, pMainResourceLoader);
	}
}

void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources)
{
	Cmd* pCmd = pMainResourceLoader->pCopyCmd[0];

	beginCmd(pCmd);

	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		cmdUpdateResource(pCmd, &pResources[i], pMainResourceLoader);
	}

	endCmd(pCmd);

	queueSubmit(pCopyQueue[0], 1, &pCmd, pWaitFence[0], 0, 0, 0, 0);
	waitForFences(pCopyQueue[0], 1, &pWaitFence[0], false);
	cleanupResourceLoader(pMainResourceLoader);
}

void flushResourceUpdates()
{
    if (pMainResourceLoader->mOpen)
    {
        endCmd(pMainResourceLoader->pBatchCopyCmd[0]);
        
        queueSubmit(pCopyQueue[0], 1, &pMainResourceLoader->pBatchCopyCmd[0], pWaitFence[0], 0, 0, 0, 0);
        waitForFences(pCopyQueue[0], 1, &pWaitFence[0], false);

		cleanupResourceLoader(pMainResourceLoader);
        pMainResourceLoader->mOpen = false;
    }
}

void removeResource(Texture* pTexture)
{
	removeTexture(pMainResourceLoader->pRenderer, pTexture);
}

void removeResource(Buffer* pBuffer)
{
	removeBuffer(pMainResourceLoader->pRenderer, pBuffer);
}

void finishResourceLoading()
{
	if ((uint32_t)gResourceThreads.size())
	{
        while (!gResourceQueue.empty ())
            Thread::Sleep (0);
        
        gFinishLoading = true;
        
        // Wait till all resources are loaded
        while (!pThreadPool->IsCompleted(0))
            Thread::Sleep(0);
        
		Cmd* pCmds[MAX_LOAD_THREADS + 1];
		for (uint32_t i = 0; i < (uint32_t)gResourceThreads.size(); ++i)
		{
			pCmds[i] = gResourceThreads[i]->pLoader->pCopyCmd[0];
		}

		pCmds[(uint32_t)gResourceThreads.size()] = pMainResourceLoader->pCopyCmd[0];

		queueSubmit(pCopyQueue[0], (uint32_t)gResourceThreads.size(), pCmds, pWaitFence[0], 0, 0, 0, 0);
		waitForFences(pCopyQueue[0], 1, &pWaitFence[0], false);
		cleanupResourceLoader(pMainResourceLoader);

		for (uint32_t i = 0; i < (uint32_t)gResourceThreads.size(); ++i)
			removeResourceLoader(gResourceThreads[i]->pLoader);

		pThreadPool->~ThreadPool();
        conf_free(pThreadPool);

        for (unsigned i = 0; i < (uint32_t)gResourceThreads.size(); ++i)
		{
			conf_free(gResourceThreads[i]->pItem);
			conf_free(gResourceThreads[i]);
		}
	}

	gResourceThreads.clear();
}
/************************************************************************/
// Shader loading
/************************************************************************/
#if defined(VULKAN)
// Vulkan has no builtin functions to compile source to spirv
// So we call the glslangValidator tool located inside VulkanSDK on user machine to compile the glsl code to spirv
// This code is not added to Vulkan.cpp since it calls no Vulkan specific functions
void vk_compileShader(Renderer* pRenderer, const String& fileName, const String& outFile, uint32_t macroCount, ShaderMacro* pMacros, tinystl::vector<char>* pByteCode)
{
	if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
		FileSystem::CreateDir(FileSystem::GetPath(outFile));

	String commandLine;
	tinystl::vector<String> args;
	String configFileName;

	// If there is a config file located in the shader source directory use it to specify the limits
	if (FileSystem::FileExists(FileSystem::GetPath(fileName) + "/config.conf", FSRoot::FSR_Absolute))
	{
		configFileName = FileSystem::GetPath(fileName) + "/config.conf";
		// Add command to compile from Vulkan GLSL to Spirv
		commandLine += String::format("\"%s\" -V \"%s\" -o \"%s\"", configFileName.size() ? configFileName.c_str() : "", fileName.c_str(), outFile.c_str());
	}
	else
	{
		commandLine += String::format("-V \"%s\" -o \"%s\"", fileName.c_str(), outFile.c_str());
	}

	//commandLine += " \"-D" + String("VULKAN") + "=" + "1" + "\"";

	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		commandLine += " \"-D" + pMacros[i].definition + "=" + pMacros[i].value + "\"";
	}
	args.push_back(commandLine);

	String glslangValidator = getenv("VULKAN_SDK");
	glslangValidator += "/bin/glslangValidator";
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
		errorFile.Open(outFile + "_compile.log", FM_Read, FSR_Absolute);
		// If for some reason the error file could not be created just log error msg
		if (!errorFile.IsOpen())
		{
			ErrorMsg("Failed to compile shader %s", fileName);
		}
		else
		{
			String errorLog = errorFile.ReadText();
			errorFile.Close();
			ErrorMsg("Failed to compile shader %s with error\n%s", fileName.c_str(), errorLog.c_str());
			errorFile.Close();
		}
	}
}
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(Renderer* pRenderer, const String& fileName, const String& outFile, uint32_t macroCount, ShaderMacro* pMacros, tinystl::vector<char>* pByteCode)
{
    if (!FileSystem::DirExists(FileSystem::GetPath(outFile)))
        FileSystem::CreateDir(FileSystem::GetPath(outFile));
    
    String xcrun = "/usr/bin/xcrun";
    String intermediateFile = outFile + ".air";
    tinystl::vector<String> args;
    String tmpArg = "";
    
    // Compile the source into a temporary .air file.
    args.push_back("-sdk");
    args.push_back("macosx");
    args.push_back("metal");
    args.push_back("-c");
    tmpArg = String::format("""%s""", fileName.c_str());
    args.push_back(tmpArg);
    args.push_back("-o");
    args.push_back(intermediateFile.c_str());
    args.push_back("-D");
    args.push_back("MTL_SHADER=1"); // Add MTL_SHADER macro to differentiate structs in headers shared by app/shader code.
    // Add user defined macros to the command line
    for (uint32_t i = 0; i < macroCount; ++i)
    {
        args.push_back("-D");
        args.push_back(pMacros[i].definition + "=" + pMacros[i].value);
    }
    if(FileSystem::SystemRun(xcrun, args, "") == 0)
    {
        // Create a .metallib file from the .air file.
        args.clear();
        tmpArg = "";
        args.push_back("-sdk");
        args.push_back("macosx");
        args.push_back("metallib");
        args.push_back(intermediateFile.c_str());
        args.push_back("-o");
        tmpArg = String::format("""%s""", outFile.c_str());
        args.push_back(tmpArg);
        if(FileSystem::SystemRun(xcrun, args, "") == 0)
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
        else ErrorMsg("Failed to assemble shader's %s .metallib file", fileName.c_str());
    }
    else ErrorMsg("Failed to compile shader %s", fileName.c_str());
}
#endif
#if defined(DIRECT3D12) && !defined(ENABLE_RENDERER_RUNTIME_SWITCH)
extern void compileShader(Renderer* pRenderer, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code, uint32_t macroCount, ShaderMacro* pMacros, void*(*allocator)(size_t a), uint32_t* pByteCodeSize, char** ppByteCode);
#endif

// Function to generate the timestamp of this shader source file considering all include file timestamp
static bool process_source_file(File* original, File* file, uint32_t& outTimeStamp, String& outCode)
{
	// If the source if a non-packaged file, store the timestamp
	if (file)
	{
		String fullName = file->GetName();
		unsigned fileTimeStamp = FileSystem::GetLastModifiedTime(fullName);
		if (fileTimeStamp > outTimeStamp)
			outTimeStamp = fileTimeStamp;
	}

	while (!file->IsEof())
	{
		String line = file->ReadLine();

		if (line.find("#include \"", 0) != String::npos)
		{
			String includeFileName = FileSystem::GetPath(file->GetName()) + line.substring(9).replaced("\"", "").trimmed();
			File includeFile = {};
			includeFile.Open(includeFileName, FM_ReadBinary, FSR_Absolute);
			if (!includeFile.IsOpen())
				return false;

			// Add the include file into the current code recursively
			if (!process_source_file(original, &includeFile, outTimeStamp, outCode))
			{
				includeFile.Close();
				return false;
			}

			includeFile.Close();
		}

		if (file == original)
		{
			outCode += line;
			outCode += "\n";
		}
	}

	return true;
}

// Loads the bytecode from file if the binary shader file is newer than the source
bool check_for_byte_code(const String& binaryShaderName, uint32_t sourceTimeStamp, tinystl::vector<char>& byteCode)
{
	if (!FileSystem::FileExists(binaryShaderName, FSR_Absolute))
		return false;

	// If source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
	// than source
	if (sourceTimeStamp && FileSystem::GetLastModifiedTime(binaryShaderName) < sourceTimeStamp)
		return false;

	File file = {}; file.Open(binaryShaderName, FM_ReadBinary, FSR_Absolute);
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
bool save_byte_code(const String& binaryShaderName, const tinystl::vector<char>& byteCode)
{
	String path = FileSystem::GetPath(binaryShaderName);
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

bool load_shader_stage_byte_code(Renderer* pRenderer, ShaderStage stage, const char* fileName, FSRoot root, uint32_t macroCount, ShaderMacro* pMacros, uint32_t rendererMacroCount, ShaderMacro* pRendererMacros, tinystl::vector<char>& byteCode)
{
	File shaderSource = {};
	String code;
	uint32_t timeStamp = 0;
    
#ifndef METAL
    const char* shaderName = fileName;
#else
    // Metal shader files need to have the .metal extension.
    String metalShaderName = String(fileName) + ".metal";
    const char* shaderName = metalShaderName.c_str();
#endif

	shaderSource.Open(shaderName, FM_ReadBinary, root);
	ASSERT(shaderSource.IsOpen());

	if (!process_source_file(&shaderSource, &shaderSource, timeStamp, code))
		return false;

	String name, extension, path;
	FileSystem::SplitPath(fileName, &path, &name, &extension);
	String shaderDefines;
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

#if 0 //#ifdef _DURANGO
	// Using Durango application data storage requires appmanifest(from application) changes.
	String binaryShaderName = FileSystem::GetAppPreferencesDir(NULL,NULL) + "/" + pRenderer->pName + "/CompiledShadersBinary/" +
		FileSystem::GetFileName(fileName) + String::format("_%zu", tinystl::hash(shaderDefines)) + extension + ".bin";
#else
	String rendererApi;
	switch (pRenderer->mSettings.mApi)
	{
	case RENDERER_API_D3D12:
	case RENDERER_API_XBOX_D3D12:
		rendererApi = "PCDX12";
		break;
	case RENDERER_API_VULKAN:
#if defined(_WIN32)
		rendererApi = "PCVulkan";
#elif defined(__linux__)
		rendererApi = "LINUXVulkan";
#endif
		break;
	case RENDERER_API_METAL:
		rendererApi = "OSXMetal";
		break;
	default:
		break;
	}

	String appName(pRenderer->pName);
#ifdef LINUX

	String lowerStr = appName.to_lower();
	appName = lowerStr!=pRenderer->pName ? lowerStr : lowerStr+"_";
#endif
	
	String binaryShaderName = FileSystem::GetProgramDir() + "/" + appName + String("/") + rendererApi + String("/CompiledShadersBinary/") +
		FileSystem::GetFileName(fileName) + String::format("_%zu", tinystl::hash(shaderDefines)) + extension + ".bin";
#endif

	// Shader source is newer than binary
	if (!check_for_byte_code(binaryShaderName, timeStamp, byteCode))
	{
		if (pRenderer->mSettings.mApi == RENDERER_API_METAL || pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
			vk_compileShader(pRenderer, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode);
#elif defined(METAL)
			mtl_compileShader(pRenderer, shaderSource.GetName(), binaryShaderName, macroCount, pMacros, &byteCode);
#endif
		}
		else
		{
#if defined(DIRECT3D12)
			char* pByteCode = NULL;
			uint32_t byteCodeSize = 0;
			compileShader(pRenderer, stage, shaderSource.GetName(), (uint32_t)code.size(), code.c_str(), macroCount, pMacros, conf_malloc, &byteCodeSize, &pByteCode);
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
bool find_shader_stage(const String& fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
    String ext = FileSystem::GetExtension(fileName);
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
bool find_shader_stage(const String& fileName, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	String ext = FileSystem::GetExtension(fileName);
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
	BinaryShaderDesc binaryDesc = {};
	tinystl::vector<char> byteCodes[SHADER_STAGE_COUNT] = {};
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		const RendererShaderDefinesDesc rendererDefinesDesc = get_renderer_shaderdefines(pRenderer);
		
		if (pDesc->mStages[i].mFileName.size() != 0)
		{
			ShaderStage stage;
			BinaryShaderStageDesc* pStage = NULL;
			if (find_shader_stage(pDesc->mStages[i].mFileName, &binaryDesc, &pStage, &stage))
			{
				if (!load_shader_stage_byte_code(pRenderer, stage, pDesc->mStages[i].mFileName, pDesc->mStages[i].mRoot, 
												 pDesc->mStages[i].mMacroCount, pDesc->mStages[i].pMacros, 
												 rendererDefinesDesc.rendererShaderDefinesCnt, rendererDefinesDesc.rendererShaderDefines,
												 byteCodes[i]))
					return;

				binaryDesc.mStages |= stage;
				pStage->pByteCode = byteCodes[i].data();
				pStage->mByteCodeSize = (uint32_t)byteCodes[i].size();
#if defined(METAL)
				pStage->mEntryPoint = "stageMain";
				// In metal, we need the shader source for our reflection system.
				File metalFile = {};
				metalFile.Open(pDesc->mStages[i].mFileName + ".metal", FM_Read, pDesc->mStages[i].mRoot);
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
			ShaderStage stage;
			ShaderStageDesc* pStage = NULL;
			if (find_shader_stage(pDesc->mStages[i].mFileName, &desc, &pStage, &stage))
			{
				File shaderSource = {};
				shaderSource.Open(pDesc->mStages[i].mFileName + ".metal", FM_ReadBinary, FSR_Absolute);
				ASSERT(shaderSource.IsOpen());
			
				pStage->mName = pDesc->mStages[i].mFileName;
				pStage->mCode = shaderSource.ReadText();
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
/************************************************************************/
/************************************************************************/
