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

// buffer functions
extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);

extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange = NULL);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);

// texture functions
extern void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture);
extern void removeTexture(Renderer* pRenderer, Texture*p_texture);

extern void mapTexture(Renderer* pRenderer, Texture* pTexture);
extern void unmapTexture(Renderer* pRenderer, Texture* pTexture);

extern void cmdUpdateBuffer(Cmd* p_cmd, uint64_t srcOffset, uint64_t dstOffset, uint64_t size, Buffer* p_src_buffer, Buffer* p_buffer);
extern void cmdUpdateSubresources(Cmd* pCmd, uint32_t startSubresource, uint32_t numSubresources, SubresourceDataDesc* pSubresources, Buffer* pIntermediate, uint64_t intermediateOffset, Texture* pTexture);
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

	Cmd* pCopyCmd;
	Cmd* pBatchCopyCmd;
	CmdPool* pCopyCmdPool;

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

	addCmdPool(pLoader->pRenderer, pCopyQueue, false, &pLoader->pCopyCmdPool);
	addCmd(pLoader->pCopyCmdPool, false, &pLoader->pCopyCmd);
	addCmd(pLoader->pCopyCmdPool, false, &pLoader->pBatchCopyCmd);

	*ppLoader = pLoader;
}

static void cleanupResourceLoader(ResourceLoader* pLoader)
{
	for (uint32_t i = 0; i < pLoader->mTempStagingBuffers.getCount(); ++i)
		removeBuffer(pLoader->pRenderer, pLoader->mTempStagingBuffers[i]);

	pLoader->mTempStagingBuffers.clear();
}

static void removeResourceLoader (ResourceLoader* pLoader)
{
	removeBuffer (pLoader->pRenderer, pLoader->pStagingBuffer);
	cleanupResourceLoader(pLoader);

	removeCmd(pLoader->pCopyCmdPool, pLoader->pCopyCmd);
	removeCmd(pLoader->pCopyCmdPool, pLoader->pBatchCopyCmd);
	removeCmdPool(pLoader->pRenderer, pLoader->pCopyCmdPool);

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

static void cmdLoadBuffer(BufferLoadDesc* pBufferDesc, ResourceLoader* pLoader)
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

			cmdUpdateBuffer(pLoader->pCopyCmd, range.mOffset, pBuffer->mPositionInHeap, range.mSize, range.pBuffer, pBuffer);
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
				mapBuffer(pLoader->pRenderer, pBuffer);
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
		cmdResourceBarrier(pLoader->pCopyCmd, 1, bufferBarriers, 0, NULL, false);
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

static void upload_texture_data(TextureLoadDesc* pTextureFileDesc, const Image& img, ResourceLoader* pLoader)
{
	TextureType textureType = TEXTURE_TYPE_2D;
	if (img.Is3D())
		textureType = TEXTURE_TYPE_3D;
	else if (img.IsCube())
		textureType = TEXTURE_TYPE_CUBE;

	TextureDesc desc = { };
	desc.mType = textureType;
	desc.mFlags = TEXTURE_CREATION_FLAG_NONE;
	desc.mWidth = img.GetWidth();
	desc.mHeight = img.GetHeight();
	desc.mDepth = img.GetDepth();
	desc.mBaseArrayLayer = 0;
	desc.mArraySize = img.GetArrayCount();
	desc.mBaseMipLevel = 0;
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

	addTexture(pLoader->pRenderer, &desc, pTextureFileDesc->ppTexture);
	Texture* pTexture = *pTextureFileDesc->ppTexture;
	ASSERT(pTexture);

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
#if defined(VULKAN) || defined(_DURANGO)
	TextureBarrier barrier = { pTexture, RESOURCE_STATE_COPY_DEST };
	cmdResourceBarrier(pLoader->pCopyCmd, 0, NULL, 1, &barrier, false);
#endif
	MappedMemoryRange range = consumeResourceLoaderMemory(pTexture->mTextureSize, RESOURCE_TEXTURE_ALIGNMENT, pLoader);

	// create source subres data structs
	SubresourceDataDesc texData[1024];
	SubresourceDataDesc *dest = texData;
	uint nSlices = img.IsCube() ? 6 : 1;

#if defined(DIRECT3D12) || defined(METAL)
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
#else
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
				memcpy((uint8_t*)range.pData + offset, pSrcData, img.GetMipMappedSize(i, 1));
				offset += img.GetMipMappedSize(i, 1);
			}
		}
	}
#endif

	// calculate number of subresources
	int numSubresources = (int)(dest - texData);
	cmdUpdateSubresources(pLoader->pCopyCmd, 0, numSubresources, texData, range.pBuffer, range.mOffset, pTexture);

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
#if defined(VULKAN) || defined(_DURANGO)
	barrier = { pTexture, util_determine_resource_start_state(pTexture->mDesc.mUsage) };
	cmdResourceBarrier(pLoader->pCopyCmd, 0, NULL, 1, &barrier, true);
#endif
}

static void cmdLoadTextureFile(TextureLoadDesc* pTextureFileDesc, ResourceLoader* pLoader)
{
	ASSERT (pTextureFileDesc->ppTexture);

	Image img;
	//uint64_t pos = pLoader->mCurrentPos; //unused
	bool res = img.loadImage(pTextureFileDesc->pFilename, pTextureFileDesc->mUseMipmaps, imageLoadAllocationFunc, pLoader, pTextureFileDesc->mRoot);
	if (res)
	{
		upload_texture_data(pTextureFileDesc, img, pLoader);
	}
	img.Destroy();
}

static void cmdLoadTextureImage(TextureLoadDesc* pTextureImage, ResourceLoader* pLoader)
{
	ASSERT(pTextureImage->ppTexture);
	upload_texture_data(pTextureImage, *pTextureImage->pImage, pLoader);
}

static void cmdLoadEmptyTexture(TextureLoadDesc* pEmptyTexture, ResourceLoader* pLoader)
{
	ASSERT(pEmptyTexture->ppTexture);
	pEmptyTexture->pDesc->mStartState = util_determine_resource_start_state(pEmptyTexture->pDesc->mUsage);
	addTexture(pLoader->pRenderer, pEmptyTexture->pDesc, pEmptyTexture->ppTexture);

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
#if defined(VULKAN) || defined(_DURANGO)
	TextureBarrier barrier = { *pEmptyTexture->ppTexture, pEmptyTexture->pDesc->mStartState };
	cmdResourceBarrier(pLoader->pCopyCmd, 0, NULL, 1, &barrier, true);
#endif
}

static void cmdLoadResource(ResourceLoadDesc* pResourceLoadDesc, ResourceLoader* pLoader)
{
	switch (pResourceLoadDesc->mType)
	{
	case RESOURCE_TYPE_BUFFER:
		cmdLoadBuffer (&pResourceLoadDesc->buf, pLoader);
		break;
	case RESOURCE_TYPE_TEXTURE:
		if (pResourceLoadDesc->tex.pFilename)
			cmdLoadTextureFile(&pResourceLoadDesc->tex, pLoader);
		else if (pResourceLoadDesc->tex.pImage)
			cmdLoadTextureImage(&pResourceLoadDesc->tex, pLoader);
		else
			cmdLoadEmptyTexture(&pResourceLoadDesc->tex, pLoader);
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
				mapBuffer(pLoader->pRenderer, pBuffer);

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

void cmdUpdateResource(Cmd* pCmd, ResourceUpdateDesc* pResourceUpdate, ResourceLoader* pLoader)
{
	switch (pResourceUpdate->mType)
	{
	case RESOURCE_TYPE_BUFFER:
		cmdUpdateResource(pCmd, &pResourceUpdate->buf, pLoader);
		break;
	case RESOURCE_TYPE_TEXTURE:
		break;
	default:
		break;
	}
}
//////////////////////////////////////////////////////////////////////////
// Resource Loader Globals
//////////////////////////////////////////////////////////////////////////
static Queue* pCopyQueue = NULL;
static Fence* pWaitFence = NULL;

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

	beginCmd (pLoader->pCopyCmd);

	while (true)
	{
		if (gFinishLoading)
			break;

		gResourceQueueMutex.Acquire();
		if (!gResourceQueue.empty())
		{
			ResourceLoadDesc* item = gResourceQueue.front();
			gResourceQueue.remove(0);
			gResourceQueueMutex.Release();
			cmdLoadResource (item, pLoader);
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

	endCmd(pLoader->pCopyCmd);
}

void initResourceLoaderInterface(Renderer* pRenderer, uint64_t memoryBudget, bool useThreads)
{
	uint32_t numCores = Thread::GetNumCPUCores();

	gUseThreads = useThreads && numCores > 1;

	QueueDesc desc = { QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, CMD_POOL_COPY };
	addQueue(pRenderer, &desc, &pCopyQueue);
	addFence(pRenderer, &pWaitFence);

	if (useThreads)
	{
		uint32_t numLoaders = min (MAX_LOAD_THREADS, numCores - 1);

		pThreadPool = conf_placement_new<ThreadPool>(conf_calloc(1, sizeof(ThreadPool)));
		pThreadPool->CreateThreads(numLoaders);

		for (unsigned i = 0; i < numLoaders; ++i)
		{
			WorkItem* pItem = (WorkItem*)conf_calloc(1, sizeof(*pItem));

			pItem->pFunc = loadThread;

			ResourceThread* thread = (ResourceThread*)conf_calloc(1, sizeof(*thread));

			addResourceLoader (pRenderer, memoryBudget, &thread->pLoader, pCopyQueue);

			thread->pRenderer = pRenderer;
			// Consider worst case budget for each thread
			thread->mMemoryBudget = memoryBudget;
			thread->pItem = pItem;
			gResourceThreads.push_back (thread);

			pItem->pData = gResourceThreads [i];

			pThreadPool->AddWorkItem (pItem);
		}
	}

	addResourceLoader(pRenderer, memoryBudget, &pMainResourceLoader, pCopyQueue);
}

void removeResourceLoaderInterface(Renderer* pRenderer)
{
	removeResourceLoader(pMainResourceLoader);

	removeQueue(pCopyQueue);
	removeFence(pRenderer, pWaitFence);
}

void addResource(ResourceLoadDesc* pResourceLoadDesc, bool threaded /* = false */)
{
	if (threaded && !gUseThreads)
	{
		LOGWARNING("Threaded Option specified for loading but no threads were created in initResourceLoaderInterface - Using single threaded loading");
	}

	if (!threaded || !gUseThreads)
	{
		gResourceQueueMutex.Acquire();
		beginCmd(pMainResourceLoader->pCopyCmd);

		cmdLoadResource(pResourceLoadDesc, pMainResourceLoader);

		endCmd (pMainResourceLoader->pCopyCmd);

		queueSubmit(pCopyQueue, 1, &pMainResourceLoader->pCopyCmd, pWaitFence, 0, 0, 0, 0);
		waitForFences(pCopyQueue, 1, &pWaitFence);
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
		ResourceLoadDesc resourceLoadDesc = *pTexture;
		gResourceQueueMutex.Acquire();
		beginCmd(pMainResourceLoader->pCopyCmd);

		cmdLoadResource(&resourceLoadDesc, pMainResourceLoader);

		endCmd(pMainResourceLoader->pCopyCmd);

		queueSubmit(pCopyQueue, 1, &pMainResourceLoader->pCopyCmd, pWaitFence, 0, 0, 0, 0);
		waitForFences(pCopyQueue, 1, &pWaitFence);
		cleanupResourceLoader(pMainResourceLoader);

		pMainResourceLoader->mCurrentPos = 0;
		gResourceQueueMutex.Release();
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

void addResources(uint32_t resourceCount, ResourceLoadDesc* pResources, bool threaded /* = false */)
{
	if (threaded && !gUseThreads)
	{
		LOGWARNING("Threaded Option specified for loading but no threads were created in initResourceLoaderInterface - Using single threaded loading");
	}

	if (!threaded || !gUseThreads)
	{
		gResourceQueueMutex.Acquire();
		beginCmd(pMainResourceLoader->pCopyCmd);

		for (uint32_t i = 0; i < resourceCount; ++i)
		{
			cmdLoadResource(&pResources[i], pMainResourceLoader);
		}

		endCmd(pMainResourceLoader->pCopyCmd);

		queueSubmit(pCopyQueue, 1, &pMainResourceLoader->pCopyCmd, pWaitFence, 0, 0, 0, 0);
		waitForFences(pCopyQueue, 1, &pWaitFence);
		cleanupResourceLoader(pMainResourceLoader);

		pMainResourceLoader->mCurrentPos = 0;
		gResourceQueueMutex.Release();
	}
	else
	{
		gResourceQueueMutex.Acquire();
		for (uint32_t i = 0; i < resourceCount; ++i)
		{
			ResourceLoadDesc* pResource = (ResourceLoadDesc*)conf_malloc(sizeof(ResourceLoadDesc));
			if (pResources[i].mType == RESOURCE_TYPE_TEXTURE && pResources[i].tex.pFilename)
			{
				pResource->mType = RESOURCE_TYPE_TEXTURE;
				pResource->tex = pResources[i].tex;
				pResource->tex.pFilename = (char*)conf_calloc(strlen(pResources[i].tex.pFilename) + 1, sizeof(char));
				memcpy((char*)pResource->tex.pFilename, pResources[i].tex.pFilename, strlen(pResources[i].tex.pFilename));
			}
			else
			{
				memcpy(pResource, &pResources[i], sizeof(ResourceLoadDesc));
			}
			gResourceQueue.emplace_back(pResource);
		}
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

			Cmd* pCmd = pMainResourceLoader->pCopyCmd;

            beginCmd(pCmd);
            cmdUpdateResource(pCmd, pBufferUpdate, pMainResourceLoader);
            endCmd(pCmd);

            queueSubmit(pCopyQueue, 1, &pCmd, pWaitFence, 0, 0, 0, 0);
            waitForFences(pCopyQueue, 1, &pWaitFence);
			cleanupResourceLoader(pMainResourceLoader);
        }
        else
        {
			Cmd* pCmd = pMainResourceLoader->pBatchCopyCmd;

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

void updateResources(uint32_t resourceCount, ResourceUpdateDesc* pResources)
{
	Cmd* pCmd = pMainResourceLoader->pCopyCmd;

	beginCmd(pCmd);

	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		cmdUpdateResource(pCmd, &pResources[i], pMainResourceLoader);
	}

	endCmd(pCmd);

	queueSubmit(pCopyQueue, 1, &pCmd, pWaitFence, 0, 0, 0, 0);
	waitForFences(pCopyQueue, 1, &pWaitFence);
	cleanupResourceLoader(pMainResourceLoader);
}

void flushResourceUpdates()
{
    if (pMainResourceLoader->mOpen)
    {
        endCmd(pMainResourceLoader->pBatchCopyCmd);
        
        queueSubmit(pCopyQueue, 1, &pMainResourceLoader->pBatchCopyCmd, pWaitFence, 0, 0, 0, 0);
        waitForFences(pCopyQueue, 1, &pWaitFence);

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
	if (gResourceThreads.getCount())
	{
        while (!gResourceQueue.empty ())
            Thread::Sleep (0);
        
        gFinishLoading = true;
        
        // Wait till all resources are loaded
        while (!pThreadPool->IsCompleted(0))
            Thread::Sleep(0);
        
		Cmd* pCmds[MAX_LOAD_THREADS + 1];
		for (uint32_t i = 0; i < gResourceThreads.getCount(); ++i)
		{
			pCmds[i] = gResourceThreads[i]->pLoader->pCopyCmd;
		}

		pCmds[gResourceThreads.getCount()] = pMainResourceLoader->pCopyCmd;

		queueSubmit(pCopyQueue, gResourceThreads.getCount(), pCmds, pWaitFence, 0, 0, 0, 0);
		waitForFences(pCopyQueue, 1, &pWaitFence);
		cleanupResourceLoader(pMainResourceLoader);

		for (uint32_t i = 0; i < gResourceThreads.getCount(); ++i)
			removeResourceLoader(gResourceThreads[i]->pLoader);

		pThreadPool->~ThreadPool();
        conf_free(pThreadPool);

        for (unsigned i = 0; i < gResourceThreads.getCount(); ++i)
		{
			conf_free(gResourceThreads[i]->pItem);
			conf_free(gResourceThreads[i]);
        }
	}
}
