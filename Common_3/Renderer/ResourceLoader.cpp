/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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


#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "../ThirdParty/OpenSource/EASTL/deque.h"

#define CGLTF_IMPLEMENTATION
#include "../ThirdParty/OpenSource/cgltf/cgltf.h"

#include "IRenderer.h"
#include "IResourceLoader.h"
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

#ifdef NX64
#include "../ThirdParty/OpenSource/murmurhash3/MurmurHash3_32.h"
#endif

extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture);
extern void addVirtualTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData);
extern void removeTexture(Renderer* pRenderer, Texture* p_texture);
extern void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
extern void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, SubresourceDataDesc* pSubresourceDesc);
extern void updateVirtualTexture(Renderer* pRenderer, Queue* pQueue, Texture* pTexture);
/************************************************************************/
/************************************************************************/

// Durango, Orbis, and iOS have unified memory
// so we dont need a command buffer to upload data
// We can get away with a memcpy since the GPU memory is marked as CPU write combine
#if defined(_DURANGO) || defined(ORBIS) || defined(TARGET_IOS)
#define UMA 1
#else
#define UMA 0
#endif

#define MAX_FRAMES 3U
//////////////////////////////////////////////////////////////////////////
// Internal TextureUpdateDesc
// Used internally as to not expose Image class in the public interface
//////////////////////////////////////////////////////////////////////////
typedef struct TextureLoadDescInternal
{
	Texture**   ppTexture;

	/// Load texture from disk
	Path* 		pFilePath;
	uint32_t    mNodeIndex;

	/// Load texture from binary data (with header)
	BinaryImageData mBinaryImageData;

	// Following is ignored if pDesc != NULL.  pDesc->mFlags will be considered instead.
	TextureCreationFlags mCreationFlag;
} TextureLoadDescInternal;

typedef struct TextureUpdateDescInternal
{
	Texture* pTexture;
	RawImageData mRawImageData;
	MappedMemoryRange mStagingAllocation;
	Image* pImage;
} TextureUpdateDescInternal;
//////////////////////////////////////////////////////////////////////////
// Resource CopyEngine Structures
//////////////////////////////////////////////////////////////////////////

typedef struct CopyResourceSet
{
	Fence*   pFence;
	Cmd*     pCmd;
	Buffer*  mBuffer;
	uint64_t allocatedSpace;

	/// Buffers created in case we ran out of space in the original staging buffer
	/// Will be cleaned up after the fence for this set is complete
	eastl::vector<Buffer*> mTempBuffers;
} CopyResourceSet;

//Synchronization?
typedef struct CopyEngine
{
	Queue*           pQueue;
	CmdPool*         pCmdPool;
	CopyResourceSet* resourceSets;
	uint64_t         bufferSize;
	uint32_t         bufferCount;
	bool             isRecording;
} CopyEngine;


typedef enum UpdateRequestType
{
	UPDATE_REQUEST_UPDATE_BUFFER,
	UPDATE_REQUEST_UPDATE_TEXTURE,
	UPDATE_REQUEST_UPDATE_RESOURCE_STATE,
	UPDATE_REQUEST_LOAD_TEXTURE,
	UPDATE_REQUEST_LOAD_GEOMETRY,
	UPDATE_REQUEST_INVALID,
} UpdateRequestType;

typedef enum UploadFunctionResult {
	UPLOAD_FUNCTION_RESULT_COMPLETED,
	UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL,
	UPLOAD_FUNCTION_RESULT_INVALID_REQUEST
} UploadFunctionResult;


typedef struct UpdateRequest
{
	UpdateRequest() : mType(UPDATE_REQUEST_INVALID) {}
	UpdateRequest(BufferUpdateDesc& buffer) : mType(UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	UpdateRequest(TextureLoadDescInternal& texture) : mType(UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
	UpdateRequest(TextureUpdateDescInternal& texture) : mType(UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
	UpdateRequest(GeometryLoadDesc& geom) : mType(UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
	UpdateRequest(Buffer* buf) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE) { buffer = buf; texture = NULL; }
	UpdateRequest(Texture* tex) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE) { texture = tex; buffer = NULL; }
	UpdateRequestType mType;
	uint64_t mWaitIndex = 0;
	union
	{
		BufferUpdateDesc bufUpdateDesc;
		TextureUpdateDescInternal texUpdateDesc;
		TextureLoadDescInternal texLoadDesc;
		GeometryLoadDesc geomLoadDesc;
		struct { Buffer* buffer; Texture* texture; };
	};
} UpdateRequest;

typedef struct UpdateState
{
	UpdateState() : UpdateState(UpdateRequest())
	{
	}
	UpdateState(const UpdateRequest& request) : mRequest(request), mMipLevel(0), mArrayLayer(0), mOffset({ 0, 0, 0 }), mSize(0)
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
	Mutex mStagingBufferMutex;
	eastl::deque <UpdateRequest> mActiveQueue;
	eastl::deque <UpdateRequest> mRequestQueue[MAX_GPUS][LOAD_PRIORITY_COUNT];

	tfrg_atomic64_t mTokenCompleted[LOAD_PRIORITY_COUNT];
	tfrg_atomic64_t mTokenCounter[LOAD_PRIORITY_COUNT];

	SyncToken mCurrentTokenState[MAX_FRAMES];

	CopyEngine pCopyEngines[MAX_GPUS];
	size_t mActiveSetIndex;

#if defined(NX64)
	ThreadTypeNX mThreadType;
	void* mThreadStackPtr;
#endif

	static void InitImageClass()
	{
		Image::Init();
	}

	static void ExitImageClass()
	{
		Image::Exit();
	}

	static Image* AllocImage()
	{
		return conf_new(Image);
	}

	static uint32_t GetTextureRowAlignment(Renderer* pRenderer)
	{
		return max(1u, pRenderer->pActiveGpuSettings->mUploadBufferTextureRowAlignment);
	}

	static uint32_t GetSubtextureAlignment(Renderer* pRenderer)
	{
		return max(GetTextureRowAlignment(pRenderer), pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment);
	}

	static size_t GetImageSize(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, uint rowAlignment, uint subtextureAlignment)
	{
		Image image;
		image.Create(fmt, w, h, d, mipMapCount, arraySize, NULL, rowAlignment, subtextureAlignment);
		return image.GetSizeInBytes();
	}

	static Image* CreateImage(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData, uint rowAlignment, uint subtextureAlignment)
	{
		Image* pImage = AllocImage();
		pImage->Create(fmt, w, h, d, mipMapCount, arraySize, rawData, rowAlignment, subtextureAlignment);
		return pImage;
	}

	static ImageLoadingResult CreateImage(const Path* filePath, memoryAllocationFunc pAllocator, void* pUserData, uint rowAlignment, uint subtextureAlignment, Image** pOutImage)
	{
		Image* pImage = AllocImage();

		ImageLoadingResult result = pImage->LoadFromFile(filePath, pAllocator, pUserData, rowAlignment, subtextureAlignment);

		if (result != IMAGE_LOADING_RESULT_SUCCESS)
		{
			if (result == IMAGE_LOADING_RESULT_DECODING_FAILED)
			{
				LOGF(LogLevel::eERROR, "Decoding failed for image at path %s; texture will be NULL.", fsGetPathAsNativeString(filePath));
			}

			DestroyImage(pImage);
		}
		else
		{
			*pOutImage = pImage;
		}

		return result;
	}

	static ImageLoadingResult CreateImage(void const* mem, uint32_t size, char const* extension, memoryAllocationFunc pAllocator, void* pUserData, uint rowAlignment, uint subtextureAlignment, Image** pOutImage)
	{
		FileStream* stream = fsOpenReadOnlyMemory(mem, size);

		Image* pImage = AllocImage();
		ImageLoadingResult result = pImage->LoadFromStream(stream, extension, pAllocator, pUserData, rowAlignment, subtextureAlignment);

		fsCloseStream(stream);

		if (result != IMAGE_LOADING_RESULT_SUCCESS)
		{
			if (result == IMAGE_LOADING_RESULT_DECODING_FAILED)
			{
				LOGF(LogLevel::eERROR, "Decoding failed for image from memory %p; texture will be NULL.", mem);
			}

			DestroyImage(pImage);
		}
		else
		{
			*pOutImage = pImage;
		}

		return result;
	}

	static void DestroyImage(Image* pImage)
	{
		pImage->Destroy();
		conf_delete(pImage);
	}
};

static ResourceLoader* pResourceLoader = NULL;

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

static inline SyncToken max(const SyncToken& a, const SyncToken& b)
{
	SyncToken result = {};
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		result.mWaitIndex[i] = max(a.mWaitIndex[i], b.mWaitIndex[i]);
	}
	return result;
}
//////////////////////////////////////////////////////////////////////////
// Resource Loader Internal Functions
//////////////////////////////////////////////////////////////////////////
static void setupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size, uint32_t bufferCount)
{
	QueueDesc desc = { QUEUE_TYPE_TRANSFER, QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, nodeIndex };
	addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

	CmdPoolDesc cmdPoolDesc = {};
	cmdPoolDesc.pQueue = pCopyEngine->pQueue;
	addCmdPool(pRenderer, &cmdPoolDesc, &pCopyEngine->pCmdPool);

	const uint32_t maxBlockSize = 32;
	uint64_t       minUploadSize = pCopyEngine->pQueue->mUploadGranularity.mWidth * pCopyEngine->pQueue->mUploadGranularity.mHeight *
		pCopyEngine->pQueue->mUploadGranularity.mDepth * maxBlockSize;
	size = max(size, minUploadSize);

	pCopyEngine->resourceSets = (CopyResourceSet*)conf_malloc(sizeof(CopyResourceSet)*bufferCount);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		conf_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
		addFence(pRenderer, &resourceSet.pFence);

		CmdDesc cmdDesc = {};
		cmdDesc.pPool = pCopyEngine->pCmdPool;
		addCmd(pRenderer, &cmdDesc, &resourceSet.pCmd);

		BufferDesc bufferDesc = {};
		bufferDesc.mSize = size;
#ifdef ORBIS
		bufferDesc.mAlignment = pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
#endif
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.mNodeIndex = nodeIndex;
		addBuffer(pRenderer, &bufferDesc, &resourceSet.mBuffer);
	}

	pCopyEngine->bufferSize = size;
	pCopyEngine->bufferCount = bufferCount;
	pCopyEngine->isRecording = false;
}

static void cleanupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
	for (uint32_t i = 0; i < pCopyEngine->bufferCount; ++i)
	{
		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
		removeBuffer(pRenderer, resourceSet.mBuffer);

		removeCmd(pRenderer, resourceSet.pCmd);

		removeFence(pRenderer, resourceSet.pFence);

		pCopyEngine->resourceSets[i].mTempBuffers.set_capacity(0);
	}

	conf_free(pCopyEngine->resourceSets);

	removeCmdPool(pRenderer, pCopyEngine->pCmdPool);

	removeQueue(pRenderer, pCopyEngine->pQueue);
}

static void waitCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	waitForFences(pRenderer, 1, &resourceSet.pFence);
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	pCopyEngine->resourceSets[activeSet].allocatedSpace = 0;
	pCopyEngine->isRecording = false;

#if defined(DIRECT3D11)
	Buffer* pStagingBuffer = pCopyEngine->resourceSets[activeSet].mBuffer;
	if (!pStagingBuffer->pCpuMappedAddress)
		mapBuffer(pResourceLoader->pRenderer, pCopyEngine->resourceSets[activeSet].mBuffer, NULL);
#endif

	for (Buffer*& buffer : pCopyEngine->resourceSets[activeSet].mTempBuffers)
		removeBuffer(pRenderer, buffer);
	pCopyEngine->resourceSets[activeSet].mTempBuffers.clear();
}

static Cmd* acquireCmd(CopyEngine* pCopyEngine, size_t activeSet)
{
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
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
		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
		endCmd(resourceSet.pCmd);
		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &resourceSet.pCmd;
		submitDesc.pSignalFence = resourceSet.pFence;

#if defined(DIRECT3D11)
		unmapBuffer(pResourceLoader->pRenderer, resourceSet.mBuffer);
#endif
		queueSubmit(pCopyEngine->pQueue, &submitDesc);
		pCopyEngine->isRecording = false;
	}
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(uint64_t memoryRequirement, uint32_t alignment, bool waitForSpace)
{
	// Use the copy engine for GPU 0.
	CopyEngine* pCopyEngine = &pResourceLoader->pCopyEngines[0];

	uint64_t offset = pCopyEngine->resourceSets[pResourceLoader->mActiveSetIndex].allocatedSpace;
	if (alignment != 0)
	{
		offset = round_up_64(offset, alignment);
	}

	CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[pResourceLoader->mActiveSetIndex];
	uint64_t size = (uint64_t)pResourceSet->mBuffer->mSize;
	bool memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
	if (memoryAvailable)
	{
		Buffer* buffer = pResourceSet->mBuffer;
		ASSERT(buffer->pCpuMappedAddress);
		uint8_t* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
		pCopyEngine->resourceSets[pResourceLoader->mActiveSetIndex].allocatedSpace = offset + memoryRequirement;
		return { pDstData, buffer, offset, memoryRequirement };
	}
	else
	{
		if (pCopyEngine->bufferSize < memoryRequirement)
		{
			LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
			Buffer* buffer = {};
			BufferDesc bufferDesc = {};
			bufferDesc.mSize = memoryRequirement;
#ifdef ORBIS
			bufferDesc.mAlignment = pResourceLoader->pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
#endif
			bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
			bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			addBuffer(pResourceLoader->pRenderer, &bufferDesc, &buffer);
#if defined(DIRECT3D11)
			mapBuffer(pResourceLoader->pRenderer, buffer, NULL);
#endif
			pResourceSet->mTempBuffers.emplace_back(buffer);
			return { (uint8_t*)buffer->pCpuMappedAddress, pResourceSet->mTempBuffers.back(), 0, memoryRequirement };
		}
		if (waitForSpace)
		{
			// We're not operating on the streaming thread, so we can wait until there's space available.
			pResourceLoader->mStagingBufferMutex.Release();
			waitForAllResourceLoads();
			pResourceLoader->mStagingBufferMutex.Acquire();
			return allocateStagingMemory(memoryRequirement, alignment, waitForSpace);
		}
	}

	LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
	Buffer* buffer = {};
	BufferDesc bufferDesc = {};
	bufferDesc.mSize = memoryRequirement;
#ifdef ORBIS
	bufferDesc.mAlignment = pResourceLoader->pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment;
#endif
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addBuffer(pResourceLoader->pRenderer, &bufferDesc, &buffer);
#if defined(DIRECT3D11)
	mapBuffer(pResourceLoader->pRenderer, buffer, NULL);
#endif
	pResourceSet->mTempBuffers.emplace_back(buffer);
	return { (uint8_t*)buffer->pCpuMappedAddress, pResourceSet->mTempBuffers.back(), 0, memoryRequirement };
}

static ResourceState util_determine_resource_start_state(bool uav)
{
	if (uav)
		return RESOURCE_STATE_UNORDERED_ACCESS;
	else
		return RESOURCE_STATE_SHADER_RESOURCE;
}

static ResourceState util_determine_resource_start_state(const Buffer* pBuffer)
{
	// Host visible (Upload Heap)
	if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
	{
		return RESOURCE_STATE_GENERIC_READ;
	}
	// Device Local (Default Heap)
	else if (pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		DescriptorType usage = (DescriptorType)pBuffer->mDescriptors;

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

// TODO: test and fix
void copyUploadRectZCurve(uint8_t* pDstData, uint8_t* pSrcData, Region3D uploadRegion, uint3 pitches)
{
	uint32_t offset = 0;
	for (uint32_t z = uploadRegion.mZOffset; z < uploadRegion.mZOffset + uploadRegion.mDepth; ++z)
	{
		for (uint32_t y = uploadRegion.mYOffset; y < uploadRegion.mYOffset + uploadRegion.mHeight; ++y)
		{
			for (uint32_t x = uploadRegion.mXOffset; x < uploadRegion.mXOffset + uploadRegion.mWidth; ++x)
			{
				uint32_t blockOffset = EncodeMorton(y, x);
				memcpy(pDstData + offset, pSrcData + blockOffset * pitches.x, pitches.x);
				offset += pitches.x;
			}
			offset = round_up(offset, pitches.y);
		}
		pSrcData += pitches.z;
	}
}

uint3 calculateUploadRect(uint3 pitches, uint3 offset, uint3 extent, uint3 granularity)
{
	uint3 scaler{ granularity.x*granularity.y*granularity.z, granularity.y*granularity.z, granularity.z };
	pitches *= scaler;
	uint3 leftover = extent - offset;
	uint32_t numSlices = (uint32_t)leftover.z;
	// advance by slices
	if (offset.x == 0 && offset.y == 0 && numSlices > 0)
	{
		return { extent.x, extent.y, numSlices };
	}

	// advance by strides
	numSlices = min(leftover.z, granularity.z);
	uint32_t numStrides = (uint32_t)leftover.y;
	if (offset.x == 0 && numStrides > 0)
	{
		return { extent.x, numStrides, numSlices };
	}

	numStrides = min(leftover.y, granularity.y);
	// advance by blocks
	uint32_t numBlocks = (uint32_t)leftover.x;
	return { numBlocks, numStrides, numSlices };
}

Region3D calculateUploadRegion(uint3 offset, uint3 extent, uint3 uploadBlock, uint3 pxImageDim)
{
	uint3 regionOffset = offset * uploadBlock;
	uint3 regionSize = min<uint3>(extent * uploadBlock, pxImageDim);
	return { regionOffset.x, regionOffset.y, regionOffset.z, regionSize.x, regionSize.y, regionSize.z };
}

uint64_t GetMipMappedSizeUpTo(uint3 dims, uint32_t nMipMapLevels, int32_t slices, TinyImageFormat format, uint subtextureAlignment)
{
	uint32_t w = dims.x;
	uint32_t h = dims.y;
	uint32_t d = dims.z;

	uint64_t size = 0;
	for (uint32_t i = 0; i < nMipMapLevels; ++i)
	{
		uint64_t bx = TinyImageFormat_WidthOfBlock(format);
		uint64_t by = TinyImageFormat_HeightOfBlock(format);
		uint64_t bz = TinyImageFormat_DepthOfBlock(format);

		uint64_t tmpsize = ((w + bx - 1) / bx) * ((h + by - 1) / by) * ((d + bz - 1) / bz);
		tmpsize *= slices;
		size += round_up_64(tmpsize * TinyImageFormat_BitSizeOfBlock(format) / 8, subtextureAlignment);

		w >>= 1;
		h >>= 1;
		d >>= 1;
		if (w + h + d == 0)
			break;
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;
		if (d == 0)
			d = 1;
	}
	return size;
}

#if defined(ORBIS)
extern "C"
UMAAllocation alloc_gpu_mem(Renderer* pRenderer, ResourceMemoryUsage memUsage, uint32_t size, uint32_t align);
#endif

static void* allocateTextureStagingMemory(Image* pImage, uint64_t byteCount, uint64_t alignment, void* pUserData)
{
#if defined(ORBIS)
	// Allocate directly into GPU memory rather than staging memory.
	UMAAllocation* outAllocation = (UMAAllocation*)pUserData;

	UMAAllocation allocation = alloc_gpu_mem(pResourceLoader->pRenderer, RESOURCE_MEMORY_USAGE_GPU_ONLY, (uint32_t)byteCount, (uint32_t)alignment);
	*outAllocation = allocation;
	return allocation.pData;
#else
	MappedMemoryRange* outRange = (MappedMemoryRange*)pUserData;

	MappedMemoryRange mappedRange = allocateStagingMemory(byteCount, (uint32_t)alignment, /* waitForSpace = */ false);
	*outRange = mappedRange;

	return mappedRange.pData;
#endif
}

static UploadFunctionResult updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pTextureUpdate)
{
	TextureUpdateDescInternal& texUpdateDesc = pTextureUpdate.mRequest.texUpdateDesc;
	ASSERT(pCopyEngine->pQueue->mNodeIndex == texUpdateDesc.pTexture->mNodeIndex);
	bool         applyBarriers = pRenderer->mApi == RENDERER_API_VULKAN || pRenderer->mApi == RENDERER_API_XBOX_D3D12 || pRenderer->mApi == RENDERER_API_METAL;

	uint32_t  textureAlignment = ResourceLoader::GetSubtextureAlignment(pRenderer);
	uint32_t  textureRowAlignment = ResourceLoader::GetTextureRowAlignment(pRenderer);

	bool ownsImage = false;
	Image* pImage = texUpdateDesc.pImage;
	if (!pImage)
	{
		pImage = ResourceLoader::CreateImage(texUpdateDesc.mRawImageData.mFormat, texUpdateDesc.mRawImageData.mWidth, texUpdateDesc.mRawImageData.mHeight,
			texUpdateDesc.mRawImageData.mDepth, texUpdateDesc.mRawImageData.mMipLevels, texUpdateDesc.mRawImageData.mArraySize,
			texUpdateDesc.mRawImageData.pRawData, textureRowAlignment, textureAlignment);
		pImage->SetMipsAfterSlices(texUpdateDesc.mRawImageData.mMipsAfterSlices);

		if (!texUpdateDesc.mStagingAllocation.pData)
		{
			ResourceLoader::DestroyImage(pImage);
			return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL; // We weren't able to allocate enough space in the staging buffer.
		}

		ownsImage = true;
	}

	Texture*     pTexture = texUpdateDesc.pTexture;

	Cmd*         pCmd = acquireCmd(pCopyEngine, activeSet);

	ASSERT(pTexture);
	//ASSERT(texUpdateDesc.mStagingAllocation.pBuffer == pCopyEngine->resourceSets[activeSet].mBuffer);

	// TODO: move to Image
	bool isSwizzledZCurve = !pImage->IsLinearLayout();

	uint32_t i = pTextureUpdate.mMipLevel;
	uint32_t j = pTextureUpdate.mArrayLayer;
	uint3 uploadOffset = pTextureUpdate.mOffset;

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
	if (applyBarriers && (uploadOffset.x == 0) && (uploadOffset.y == 0) && (uploadOffset.z == 0))
	{
		TextureBarrier preCopyBarrier = { pTexture, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier, 0, NULL);
	}
	Extent3D          uploadGran = pCopyEngine->pQueue->mUploadGranularity;

	TinyImageFormat fmt = pImage->GetFormat();
	uint32_t blockSize;
	uint3 pxBlockDim;
	uint32_t	nSlices;
	uint32_t	arrayCount;

	blockSize = TinyImageFormat_BitSizeOfBlock(fmt) / 8;
	pxBlockDim = { TinyImageFormat_WidthOfBlock(fmt),
								 TinyImageFormat_HeightOfBlock(fmt),
								 TinyImageFormat_DepthOfBlock(fmt) };
	nSlices = pImage->IsCube() ? 6 : 1;
	arrayCount = pImage->GetArrayCount() * nSlices;

	const uint32 pxPerRow = max<uint32_t>(round_down(textureRowAlignment / blockSize, uploadGran.mWidth), uploadGran.mWidth);
	const uint3 queueGranularity = { pxPerRow, uploadGran.mHeight, uploadGran.mDepth };
	const uint3 fullSizeDim = { pImage->GetWidth(), pImage->GetHeight(), pImage->GetDepth() };

	for (; i < pTexture->mMipLevels; ++i)
	{
		uint3 const pxImageDim{ pImage->GetWidth(i), pImage->GetHeight(i), pImage->GetDepth(i) };
		uint3    uploadExtent{ (pxImageDim + pxBlockDim - uint3(1)) / pxBlockDim };
		uint3    granularity{ min<uint3>(queueGranularity, uploadExtent) };
		uint32_t srcPitchY{ pImage->GetBytesPerRow(i) };

		uint3    pitches{ blockSize, srcPitchY, srcPitchY * uploadExtent.y };

		ASSERT(uploadOffset.x < uploadExtent.x || uploadOffset.y < uploadExtent.y || uploadOffset.z < uploadExtent.z);

		for (; j < arrayCount; ++j)
		{
			uint3    uploadRectExtent{ calculateUploadRect(pitches, uploadOffset, uploadExtent, granularity) };
			uint32_t uploadPitchY{ pImage->GetBytesPerRow(i) };
			uint3    uploadPitches{ blockSize, uploadPitchY, uploadPitchY * uploadRectExtent.y };
			ASSERT(uploadPitches.x <= pitches.x && uploadPitches.y <= pitches.y && uploadPitches.z <= pitches.z);

			ASSERT(
				uploadOffset.x + uploadRectExtent.x <= uploadExtent.x || uploadOffset.y + uploadRectExtent.y <= uploadExtent.y ||
				uploadOffset.z + uploadRectExtent.z <= uploadExtent.z);

			if (uploadRectExtent.x == 0)
			{
				pTextureUpdate.mMipLevel = i;
				pTextureUpdate.mArrayLayer = j;
				pTextureUpdate.mOffset = uploadOffset;
				return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}

			Region3D uploadRegion{ uploadOffset.x,     uploadOffset.y,     uploadOffset.z,
				uploadRectExtent.x, uploadRectExtent.y, uploadRectExtent.z };

			SubresourceDataDesc  texData;
			texData.mArrayLayer = j /*n * nSlices + k*/;
			texData.mMipLevel = i;
			texData.mRegion = calculateUploadRegion(uploadOffset, uploadRectExtent, pxBlockDim, pxImageDim);
			texData.mRowPitch = uploadPitches.y;
			texData.mSlicePitch = uploadPitches.z;

			if (isSwizzledZCurve)
			{
				uint8_t* pSrcData = NULL;
				// there are two common formats for how slices and mipmaps are laid out in
				// either a slice is just another dimension (the 4th) that doesn't undergo
				// mip map reduction
				// so images the top level is just w * h * d * s in size
				// a mipmap level is w >> mml * h >> mml * d >> mml * s in size
				// or DDS style where each slice is mipmapped as a seperate image
				if (pImage->AreMipsAfterSlices()) {
					pSrcData = (uint8_t *)pImage->GetPixels() +
						GetMipMappedSizeUpTo(fullSizeDim, i, arrayCount, fmt, textureAlignment) +
						j * pitches.z;
				}
				else {
					uint32_t n = j / nSlices;
					uint32_t k = j - n * nSlices;
					pSrcData = (uint8_t *)pImage->GetPixels(i, n) + k * pitches.z;
				}
				ASSERT(pSrcData);

				MappedMemoryRange range =
					allocateStagingMemory(uploadRectExtent.z * uploadPitches.z, textureAlignment, /* waitForSpace = */ false);

				if (!range.pData)
				{
					if (ownsImage) ResourceLoader::DestroyImage(pImage);
					return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
				}
				copyUploadRectZCurve(range.pData, pSrcData, uploadRegion, pitches);
				texData.mBufferOffset = range.mOffset;
			}
			else
			{
				uint64_t srcOffset = (uint64_t)(pImage->GetPixels(i, j) - pImage->GetPixels());
				ASSERT(srcOffset < texUpdateDesc.mStagingAllocation.mSize);
				texData.mBufferOffset = texUpdateDesc.mStagingAllocation.mOffset + srcOffset;
			}

			ASSERT(texData.mBufferOffset % textureAlignment == 0);

			cmdUpdateSubresource(pCmd, pTexture, texUpdateDesc.mStagingAllocation.pBuffer, &texData);

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
				return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
			}
		}
		j = 0;
		ASSERT(uploadOffset.x == 0 && uploadOffset.y == 0 && uploadOffset.z == 0);
	}

	// Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
	if (applyBarriers)
	{
		TextureBarrier postCopyBarrier = { pTexture, util_determine_resource_start_state(pTexture->mUav) };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier, 0, NULL);
	}
	else
	{
		pTexture->mCurrentState = util_determine_resource_start_state(pTexture->mUav);
	}

	if (ownsImage)
	{
		ResourceLoader::DestroyImage(pImage);
	}

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pTextureUpdate)
{
	TextureLoadDescInternal* pTextureDesc = &pTextureUpdate.mRequest.texLoadDesc;

	uint32_t  textureAlignment = ResourceLoader::GetSubtextureAlignment(pRenderer);
	uint32_t  textureRowAlignment = ResourceLoader::GetTextureRowAlignment(pRenderer);

#if defined(ORBIS)
	UMAAllocation allocation = {};
#else
	MappedMemoryRange allocation = {};
#endif

	Image* pImage = NULL;
	if (pTextureDesc->pFilePath)
	{
#if !defined(METAL) && !defined(DIRECT3D11)
		PathComponent component = fsGetPathExtension(pTextureDesc->pFilePath);

		bool isSparseVirtualTexture = (component.length > 0 && strcmp(component.buffer, "svt") == 0) ? true : false;

		if (isSparseVirtualTexture)
		{
			ImageLoadingResult result = ResourceLoader::CreateImage(pTextureDesc->pFilePath, NULL, NULL, 1, 1, &pImage);

			if (result != IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
				fsFreePath(pTextureDesc->pFilePath);

			if (result == IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
				return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
			else if (result != IMAGE_LOADING_RESULT_SUCCESS)
				return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

			TextureDesc SVTDesc = {};
			SVTDesc.mWidth = pImage->GetWidth();
			SVTDesc.mHeight = pImage->GetHeight();
			SVTDesc.mDepth = pImage->GetDepth();
			SVTDesc.mFlags = TEXTURE_CREATION_FLAG_NONE;
			SVTDesc.mFormat = pImage->GetFormat();
			SVTDesc.mHostVisible = false;
			SVTDesc.mMipLevels = pImage->GetMipMapCount();
			SVTDesc.mSampleCount = SAMPLE_COUNT_1;
			//SVTDesc.mStartState = RESOURCE_STATE_COMMON;
			SVTDesc.mStartState = RESOURCE_STATE_COPY_DEST;
			SVTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;

			addVirtualTexture(pResourceLoader->pRenderer, &SVTDesc, pTextureDesc->ppTexture, pImage->GetPixels());

			ResourceLoader::DestroyImage(pImage);

			/************************************************************************/
			// Create visibility buffer
			/************************************************************************/

			eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)(*pTextureDesc->ppTexture)->pSvt->pPages;

			if (pPageTable == NULL)
				return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

			BufferLoadDesc visDesc = {};
			visDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			visDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			visDesc.mDesc.mStructStride = sizeof(uint);
			visDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			visDesc.mDesc.mSize = visDesc.mDesc.mStructStride * visDesc.mDesc.mElementCount;
			visDesc.mDesc.pDebugName = L"Vis Buffer for Sparse Texture";
			visDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mVisibility;
			addResource(&visDesc, NULL, LOAD_PRIORITY_NORMAL);

			BufferLoadDesc prevVisDesc = {};
			prevVisDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			prevVisDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			prevVisDesc.mDesc.mStructStride = sizeof(uint);
			prevVisDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			prevVisDesc.mDesc.mSize = prevVisDesc.mDesc.mStructStride * prevVisDesc.mDesc.mElementCount;
			prevVisDesc.mDesc.pDebugName = L"Prev Vis Buffer for Sparse Texture";
			prevVisDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mPrevVisibility;
			addResource(&prevVisDesc, NULL, LOAD_PRIORITY_NORMAL);

			BufferLoadDesc alivePageDesc = {};
			alivePageDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			alivePageDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(DIRECT3D12)
			alivePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#elif defined(VULKAN)
			alivePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#else
			alivePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
			alivePageDesc.mDesc.mStructStride = sizeof(uint);
			alivePageDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			alivePageDesc.mDesc.mSize = alivePageDesc.mDesc.mStructStride * alivePageDesc.mDesc.mElementCount;
			alivePageDesc.mDesc.pDebugName = L"Alive pages buffer for Sparse Texture";
			alivePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mAlivePage;
			addResource(&alivePageDesc, NULL, LOAD_PRIORITY_NORMAL);

			BufferLoadDesc removePageDesc = {};
			removePageDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			removePageDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(DIRECT3D12)
			removePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#elif defined(VULKAN)
			removePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#else
			removePageDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
			removePageDesc.mDesc.mStructStride = sizeof(uint);
			removePageDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			removePageDesc.mDesc.mSize = removePageDesc.mDesc.mStructStride * removePageDesc.mDesc.mElementCount;
			removePageDesc.mDesc.pDebugName = L"Remove pages buffer for Sparse Texture";
			removePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mRemovePage;
			addResource(&removePageDesc, NULL, LOAD_PRIORITY_NORMAL);

			BufferLoadDesc pageCountsDesc = {};
			pageCountsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			pageCountsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(DIRECT3D12)
			pageCountsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#elif defined(VULKAN)
			pageCountsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#else
			pageCountsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
			pageCountsDesc.mDesc.mStructStride = sizeof(uint);
			pageCountsDesc.mDesc.mElementCount = 4;
			pageCountsDesc.mDesc.mSize = pageCountsDesc.mDesc.mStructStride * pageCountsDesc.mDesc.mElementCount;
			pageCountsDesc.mDesc.pDebugName = L"Page count buffer for Sparse Texture";
			pageCountsDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mPageCounts;
			addResource(&pageCountsDesc, NULL, LOAD_PRIORITY_NORMAL);

			return UPLOAD_FUNCTION_RESULT_COMPLETED;
		}
#endif

		ImageLoadingResult result = ResourceLoader::CreateImage(pTextureDesc->pFilePath, allocateTextureStagingMemory, &allocation, textureRowAlignment, textureAlignment, &pImage);

		if (result != IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
			fsFreePath(pTextureDesc->pFilePath);

		if (result == IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
			return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
		else if (result != IMAGE_LOADING_RESULT_SUCCESS)
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

		if (!pImage)
		{
			return allocation.pData == NULL ? UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL : UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
	}
	else if (pTextureDesc->mBinaryImageData.pBinaryData)
	{
		ImageLoadingResult result = ResourceLoader::CreateImage(pTextureDesc->mBinaryImageData.pBinaryData, (uint32_t)pTextureDesc->mBinaryImageData.mSize, pTextureDesc->mBinaryImageData.pExtension, allocateTextureStagingMemory, &allocation, textureRowAlignment, textureAlignment, &pImage);
		if (result == IMAGE_LOADING_RESULT_ALLOCATION_FAILED)
			return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
		else if (result != IMAGE_LOADING_RESULT_SUCCESS)
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
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

	if (pTextureDesc->mCreationFlag & TEXTURE_CREATION_FLAG_SRGB)
	{
		// Set the format to be an sRGB format.
		uint64_t formatBits = desc.mFormat;
		formatBits &= ~((uint64_t)(TinyImageFormat_PACK_TYPE_REQUIRED_BITS - 1) << TinyImageFormat_PACK_TYPE_SHIFT);
		formatBits |= (uint64_t)TinyImageFormat_PACK_TYPE_SRGB << TinyImageFormat_PACK_TYPE_SHIFT;
		desc.mFormat = (TinyImageFormat)formatBits;
	}

	desc.mClearValue = ClearValue();
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
#if defined(ORBIS)
	desc.pNativeHandle = allocation.pAllocationInfo;
#else
	desc.pNativeHandle = NULL;
#endif
	desc.mHostVisible = false;
	desc.mNodeIndex = pTextureDesc->mNodeIndex;

	if (pImage->IsCube())
	{
		desc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
		desc.mArraySize *= 6;
	}

	wchar_t debugName[MAX_PATH] = {};
	desc.pDebugName = debugName;

	if (const Path *path = pImage->GetPath()) {
		PathComponent fileName = fsGetPathFileName(path);
		mbstowcs(debugName, fileName.buffer, min((size_t)MAX_PATH, fileName.length));
	}

	addTexture(pResourceLoader->pRenderer, &desc, pTextureDesc->ppTexture);

#if defined(ORBIS)
	UploadFunctionResult result = UPLOAD_FUNCTION_RESULT_COMPLETED;
#else

	TextureUpdateDescInternal updateDesc = { *pTextureDesc->ppTexture, {}, allocation, pImage };
	UpdateRequest updateRequest(updateDesc);
	UpdateState updateState = updateRequest;
	UploadFunctionResult result = updateTexture(pRenderer, pCopyEngine, activeSet, updateState);
#endif

	ResourceLoader::DestroyImage(pImage);

	return result;
}

static inline constexpr ShaderSemantic cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
{
	switch (type)
	{
	case cgltf_attribute_type_position: return SEMANTIC_POSITION;
	case cgltf_attribute_type_normal: return SEMANTIC_NORMAL;
	case cgltf_attribute_type_tangent: return SEMANTIC_TANGENT;
	case cgltf_attribute_type_color: return SEMANTIC_COLOR;
	case cgltf_attribute_type_joints: return SEMANTIC_JOINTS;
	case cgltf_attribute_type_weights: return SEMANTIC_WEIGHTS;
	case cgltf_attribute_type_texcoord:
		return (ShaderSemantic)(SEMANTIC_TEXCOORD0 + index);
	default:
		return SEMANTIC_TEXCOORD0;
	}
}

static inline constexpr TinyImageFormat cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
{
	switch (type)
	{
	case cgltf_type_scalar:
		if (cgltf_component_type_r_8 == compType)
			return TinyImageFormat_R8_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R8_UINT;
		else if (cgltf_component_type_r_16 == compType)
			return TinyImageFormat_R16_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R16_UINT;
		else if (cgltf_component_type_r_32f == compType)
			return TinyImageFormat_R32_SFLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return TinyImageFormat_R32_UINT;
	case cgltf_type_vec2:
		if (cgltf_component_type_r_8 == compType)
			return TinyImageFormat_R8G8_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R8G8_UINT;
		else if (cgltf_component_type_r_16 == compType)
			return TinyImageFormat_R16G16_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R16G16_UINT;
		else if (cgltf_component_type_r_32f == compType)
			return TinyImageFormat_R32G32_SFLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return TinyImageFormat_R32G32_UINT;
	case cgltf_type_vec3:
		if (cgltf_component_type_r_8 == compType)
			return TinyImageFormat_R8G8B8_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R8G8B8_UINT;
		else if (cgltf_component_type_r_16 == compType)
			return TinyImageFormat_R16G16B16_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R16G16B16_UINT;
		else if (cgltf_component_type_r_32f == compType)
			return TinyImageFormat_R32G32B32_SFLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return TinyImageFormat_R32G32B32_UINT;
	case cgltf_type_vec4:
		if (cgltf_component_type_r_8 == compType)
			return TinyImageFormat_R8G8B8A8_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R8G8B8A8_UINT;
		else if (cgltf_component_type_r_16 == compType)
			return TinyImageFormat_R16G16B16A16_SINT;
		else if (cgltf_component_type_r_16u == compType)
			return TinyImageFormat_R16G16B16A16_UINT;
		else if (cgltf_component_type_r_32f == compType)
			return TinyImageFormat_R32G32B32A32_SFLOAT;
		else if (cgltf_component_type_r_32u == compType)
			return TinyImageFormat_R32G32B32A32_UINT;
		// #NOTE: Not applicable to vertex formats
	case cgltf_type_mat2:
	case cgltf_type_mat3:
	case cgltf_type_mat4:
	default:
		return TinyImageFormat_UNDEFINED;
	}
}

#define F16_EXPONENT_BITS 0x1F
#define F16_EXPONENT_SHIFT 10
#define F16_EXPONENT_BIAS 15
#define F16_MANTISSA_BITS 0x3ff
#define F16_MANTISSA_SHIFT (23 - F16_EXPONENT_SHIFT)
#define F16_MAX_EXPONENT (F16_EXPONENT_BITS << F16_EXPONENT_SHIFT)

static inline uint16_t float_to_half(float val)
{
	uint32_t           f32 = (*(uint32_t*)&val);
	uint16_t           f16 = 0;
	/* Decode IEEE 754 little-endian 32-bit floating-point value */
	int sign = (f32 >> 16) & 0x8000;
	/* Map exponent to the range [-127,128] */
	int exponent = ((f32 >> 23) & 0xff) - 127;
	int mantissa = f32 & 0x007fffff;
	if (exponent == 128)
	{ /* Infinity or NaN */
		f16 = (uint16_t)(sign | F16_MAX_EXPONENT);
		if (mantissa)
			f16 |= (mantissa & F16_MANTISSA_BITS);
	}
	else if (exponent > 15)
	{ /* Overflow - flush to Infinity */
		f16 = (unsigned short)(sign | F16_MAX_EXPONENT);
	}
	else if (exponent > -15)
	{ /* Representable value */
		exponent += F16_EXPONENT_BIAS;
		mantissa >>= F16_MANTISSA_SHIFT;
		f16 = (unsigned short)(sign | exponent << F16_EXPONENT_SHIFT | mantissa);
	}
	else
	{
		f16 = (unsigned short)sign;
	}
	return f16;
}

static inline void pack_float2_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	struct f2 { float x; float y; };
	f2* f = (f2*)src;
	for (uint32_t e = 0; e < count; ++e)
	{
		*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = (
			(float_to_half(f[e].x) & 0x0000FFFF) | ((float_to_half(f[e].y) << 16) & 0xFFFF0000));
	}
}

static inline uint32_t float2_to_unorm2x16(const float* v)
{
	uint32_t x = (uint32_t)round(clamp(v[0], 0, 1) * 65535.0f);
	uint32_t y = (uint32_t)round(clamp(v[1], 0, 1) * 65535.0f);
	return ((uint32_t)0x0000FFFF & x) | ((y << 16) & (uint32_t)0xFFFF0000);
}

#define OCT_WRAP(v, w) ((1.0f - abs((w))) * ((v) >= 0.0f ? 1.0f : -1.0f))

static inline void pack_float3_direction_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	struct f3 { float x; float y; float z; };
	for (uint32_t e = 0; e < count; ++e)
	{
		f3 f = *(f3*)(src + e * stride);
		float absLength = (abs(f.x) + abs(f.y) + abs(f.z));
		f3 enc = {};
		if (absLength)
		{
			enc.x = f.x / absLength;
			enc.y = f.y / absLength;
			enc.z = f.z / absLength;
			if (enc.z < 0)
			{
				float oldX = enc.x;
				enc.x = OCT_WRAP(enc.x, enc.y);
				enc.y = OCT_WRAP(enc.y, oldX);
			}
			enc.x = enc.x * 0.5f + 0.5f;
			enc.y = enc.y * 0.5f + 0.5f;
			*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = float2_to_unorm2x16(&enc.x);
		}
		else
		{
			*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = 0;
		}
	}
}

static UploadFunctionResult updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pBufferUpdate)
{
	BufferUpdateDesc& bufUpdateDesc = pBufferUpdate.mRequest.bufUpdateDesc;
	ASSERT(pCopyEngine->pQueue->mNodeIndex == bufUpdateDesc.pBuffer->mNodeIndex);
	Buffer* pBuffer = bufUpdateDesc.pBuffer;
	ASSERT(pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

	Cmd* pCmd = acquireCmd(pCopyEngine, activeSet);

	MappedMemoryRange range = bufUpdateDesc.mInternalData.mMappedRange;
	cmdUpdateBuffer(pCmd, pBuffer, bufUpdateDesc.mDstOffset, range.pBuffer, range.mOffset, range.mSize);

	ResourceState state = util_determine_resource_start_state(pBuffer);
#ifdef _DURANGO
	// Xbox One needs explicit resource transitions
	BufferBarrier bufferBarriers[] = { { pBuffer, state } };
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL, 0, NULL);
#else
	// Resource will automatically transition so just set the next state without a barrier
	pBuffer->mCurrentState = state;
#endif

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult updateResourceState(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pUpdate)
{
	bool applyBarriers = pRenderer->mApi == RENDERER_API_VULKAN;
	if (applyBarriers)
	{
		Cmd* pCmd = acquireCmd(pCopyEngine, activeSet);
		if (pUpdate.mRequest.buffer)
		{
			BufferBarrier barrier = { pUpdate.mRequest.buffer, (ResourceState)pUpdate.mRequest.buffer->mStartState };
			cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
		}
		else if (pUpdate.mRequest.texture)
		{
			TextureBarrier barrier = { pUpdate.mRequest.texture, (ResourceState)pUpdate.mRequest.texture->mStartState };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, 0, NULL);
		}
		else
		{
			ASSERT(0 && "Invalid params");
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
	}

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadGeometry(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pGeometryLoad)
{
	GeometryLoadDesc* pDesc = &pGeometryLoad.mRequest.geomLoadDesc;

	const char* iext = fsGetPathExtension(pDesc->pFilePath).buffer;

	// Geometry in gltf container
	if (iext && (stricmp(iext, "gltf") == 0 || stricmp(iext, "glb") == 0))
	{
		FileStream* file = fsOpenFile(pDesc->pFilePath, FM_READ_BINARY);
		if (!file)
		{
			LOGF(eERROR, "Failed to open gltf file %s", fsGetPathFileName(pDesc->pFilePath).buffer);
			ASSERT(false);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		ssize_t fileSize = fsGetStreamFileSize(file);
		void* fileData = conf_malloc(fileSize);
		cgltf_result result = cgltf_result_invalid_gltf;

		fsReadFromStream(file, fileData, fileSize);

		cgltf_options options = {};
		cgltf_data* data = NULL;
		options.memory_alloc = [](void* user, cgltf_size size) { return conf_malloc(size); };
		options.memory_free = [](void* user, void* ptr) { conf_free(ptr); };
		result = cgltf_parse(&options, fileData, fileSize, &data);
		fsCloseStream(file);

		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to parse gltf file %s with error %u", fsGetPathFileName(pDesc->pFilePath).buffer, (uint32_t)result);
			ASSERT(false);
			conf_free(fileData);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

#ifdef _DEBUG
		result = cgltf_validate(data);
		if (cgltf_result_success != result)
		{
			LOGF(eWARNING, "GLTF validation finished with error %u for file %s", (uint32_t)result, fsGetPathFileName(pDesc->pFilePath).buffer);
		}
#endif

		// Load buffers located in separate files (.bin) using our file system
		for (uint32_t i = 0; i < data->buffers_count; ++i)
		{
			const char* uri = data->buffers[i].uri;

			if (!uri || data->buffers[i].data)
			{
				continue;
			}

			if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
			{
				Path* parent = fsCopyParentPath(pDesc->pFilePath);
				Path* path = fsAppendPathComponent(parent, uri);
				FileStream* fs = fsOpenFile(path, FM_READ_BINARY);
				if (fs)
				{
					ASSERT(fsGetStreamFileSize(fs) >= (ssize_t)data->buffers[i].size);

					data->buffers[i].data = conf_malloc(data->buffers[i].size);
					fsReadFromStream(fs, data->buffers[i].data, data->buffers[i].size);
				}
				fsCloseStream(fs);
				fsFreePath(path);
				fsFreePath(parent);
			}
		}

		result = cgltf_load_buffers(&options, data, fsGetPathAsNativeString(pDesc->pFilePath));
		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", fsGetPathFileName(pDesc->pFilePath).buffer, (uint32_t)result);
			ASSERT(false);
			conf_free(fileData);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		typedef void (*PackingFunction)(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst);

		uint32_t vertexStrides[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t vertexAttribCount[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t vertexOffsets[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t vertexBindings[SEMANTIC_TEXCOORD9 + 1] = {};
		cgltf_attribute* vertexAttribs[SEMANTIC_TEXCOORD9 + 1] = {};
		PackingFunction vertexPacking[SEMANTIC_TEXCOORD9 + 1] = {};
		for (uint32_t i = 0; i < SEMANTIC_TEXCOORD9 + 1; ++i)
			vertexOffsets[i] = UINT_MAX;

		uint32_t indexCount = 0;
		uint32_t vertexCount = 0;
		uint32_t drawCount = 0;
		uint32_t jointCount = 0;
		uint32_t vertexBufferCount = 0;

		// Find number of traditional draw calls required to draw this piece of geometry
		// Find total index count, total vertex count
		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
			{
				const cgltf_primitive* prim = &data->meshes[i].primitives[p];
				indexCount += (uint32_t)prim->indices->count;
				vertexCount += (uint32_t)prim->attributes->data->count;
				++drawCount;

				for (uint32_t i = 0; i < prim->attributes_count; ++i)
					vertexAttribs[cgltf_attrib_type_to_semantic(prim->attributes[i].type, prim->attributes[i].index)] = &prim->attributes[i];
			}
		}

		// Determine vertex stride for each binding
		for (uint32_t i = 0; i < pDesc->pVertexLayout->mAttribCount; ++i)
		{
			const VertexAttrib* attr = &pDesc->pVertexLayout->mAttribs[i];
			const cgltf_attribute* cgltfAttr = vertexAttribs[attr->mSemantic];
			ASSERT(cgltfAttr);

			const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock(attr->mFormat) >> 3;
			const uint32_t srcFormatSize = (uint32_t)cgltfAttr->data->stride;

			vertexStrides[attr->mBinding] += dstFormatSize ? dstFormatSize : srcFormatSize;
			vertexOffsets[attr->mSemantic] = attr->mOffset;
			vertexBindings[attr->mSemantic] = attr->mBinding;
			++vertexAttribCount[attr->mBinding];

			// Compare vertex attrib format to the gltf attrib type
			// Select a packing function if dst format is packed version
			// Texcoords - Pack float2 to half2
			// Directions - Pack float3 to float2 to unorm2x16 (Normal, Tangent)
			// Position - No packing yet
			const TinyImageFormat srcFormat = cgltf_type_to_image_format(cgltfAttr->data->type, cgltfAttr->data->component_type);
			const TinyImageFormat dstFormat = attr->mFormat == TinyImageFormat_UNDEFINED ? srcFormat : attr->mFormat;

			if (dstFormat != srcFormat)
			{
				// Select appropriate packing function which will be used when filling the vertex buffer
				switch (cgltfAttr->type)
				{
				case cgltf_attribute_type_texcoord:
				{
					if (sizeof(uint32_t) == dstFormatSize && sizeof(float[2]) == srcFormatSize)
						vertexPacking[attr->mSemantic] = pack_float2_to_half2;
					// #TODO: Add more variations if needed
					break;
				}
				case cgltf_attribute_type_normal:
				case cgltf_attribute_type_tangent:
				{
					if (sizeof(uint32_t) == dstFormatSize && (sizeof(float[3]) == srcFormatSize || sizeof(float[4]) == srcFormatSize))
						vertexPacking[attr->mSemantic] = pack_float3_direction_to_half2;
					// #TODO: Add more variations if needed
					break;
				}
				default:
					break;
				}
			}
		}

		// Determine number of vertex buffers needed based on number of unique bindings found
		// For each unique binding the vertex stride will be non zero
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
			if (vertexStrides[i])
				++vertexBufferCount;

		for (uint32_t i = 0; i < data->skins_count; ++i)
			jointCount += (uint32_t)data->skins[i].joints_count;

		// Determine index stride
		// This depends on vertex count rather than the stride specified in gltf
		// since gltf assumes we have index buffer per primitive which is non optimal
		const uint32_t indexStride = vertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t);

		uint32_t totalSize = 0;
		totalSize += round_up(sizeof(Geometry), 16);
		totalSize += round_up(drawCount * sizeof(IndirectDrawIndexArguments), 16);
		totalSize += round_up(jointCount * sizeof(mat4), 16);
		totalSize += round_up(jointCount * sizeof(uint32_t), 16);

		Geometry* geom = (Geometry*)conf_calloc(1, totalSize);
		ASSERT(geom);

		geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);
		geom->pInverseBindPoses = (mat4*)((uint8_t*)geom->pDrawArgs + round_up(drawCount * sizeof(*geom->pDrawArgs), 16));
		geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(jointCount * sizeof(*geom->pInverseBindPoses), 16));

		uint32_t shadowSize = 0;
		if (pDesc->mFlags & GEOMETRY_LOAD_FLAG_SHADOWED)
		{
			shadowSize += (uint32_t)vertexAttribs[SEMANTIC_POSITION]->data->stride * vertexCount;
			shadowSize += indexCount * indexStride;

			geom->pShadow = (Geometry::ShadowData*)conf_calloc(1, sizeof(Geometry::ShadowData) + shadowSize);
			geom->pShadow->pIndices = geom->pShadow + 1;
			geom->pShadow->pAttributes[SEMANTIC_POSITION] = (uint8_t*)geom->pShadow->pIndices + (indexCount * indexStride);
			// #TODO: Add more if needed
		}

		geom->mVertexBufferCount = vertexBufferCount;
		geom->mDrawArgCount = drawCount;
		geom->mIndexCount = indexCount;
		geom->mVertexCount = vertexCount;
		geom->mIndexType = (sizeof(uint16_t) == indexStride) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32;
		geom->mJointCount = jointCount;

		// Allocate buffer memory
		const bool structuredBuffers = (pDesc->mFlags & GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS);

		// Index buffer
		BufferDesc indexBufferDesc = {};
		indexBufferDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER |
			(structuredBuffers ?
			(DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER) :
				(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
		indexBufferDesc.mSize = indexStride * indexCount;
		indexBufferDesc.mElementCount = indexBufferDesc.mSize / (structuredBuffers ? indexStride : sizeof(uint32_t));
		indexBufferDesc.mStructStride = indexStride;
		indexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		addBuffer(pRenderer, &indexBufferDesc, &geom->pIndexBuffer);

		BufferUpdateDesc indexUpdateDesc = {};
		BufferUpdateDesc vertexUpdateDesc[MAX_VERTEX_BINDINGS] = {};

		indexUpdateDesc.mSize = indexCount * indexStride;
		indexUpdateDesc.pBuffer = geom->pIndexBuffer;
#if UMA
		indexUpdateDesc.mInternalData.mMappedRange = { (uint8_t*)geom->pIndexBuffer->pCpuMappedAddress };
#else
		indexUpdateDesc.mInternalData.mMappedRange = allocateStagingMemory(indexUpdateDesc.mSize, RESOURCE_BUFFER_ALIGNMENT, false);
#endif
		indexUpdateDesc.pMappedData = indexUpdateDesc.mInternalData.mMappedRange.pData;

		uint32_t bufferCounter = 0;
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (!vertexStrides[i])
				continue;

			BufferDesc vertexBufferDesc = {};
			vertexBufferDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER |
				(structuredBuffers ?
				(DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER) :
					(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
			vertexBufferDesc.mSize = vertexStrides[i] * vertexCount;
			vertexBufferDesc.mElementCount = vertexBufferDesc.mSize / (structuredBuffers ? vertexStrides[i] : sizeof(uint32_t));
			vertexBufferDesc.mStructStride = vertexStrides[i];
			vertexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			addBuffer(pRenderer, &vertexBufferDesc, &geom->pVertexBuffers[bufferCounter]);

			geom->mVertexStrides[bufferCounter] = vertexStrides[i];

			vertexUpdateDesc[i].pBuffer = geom->pVertexBuffers[bufferCounter];
			vertexUpdateDesc[i].mSize = vertexBufferDesc.mSize;
#if UMA
			vertexUpdateDesc[i].mInternalData.mMappedRange = { (uint8_t*)geom->pVertexBuffers[bufferCounter]->pCpuMappedAddress, 0 };
#else
			vertexUpdateDesc[i].mInternalData.mMappedRange = allocateStagingMemory(vertexUpdateDesc[i].mSize, RESOURCE_BUFFER_ALIGNMENT, false);
#endif
			vertexUpdateDesc[i].pMappedData = vertexUpdateDesc[i].mInternalData.mMappedRange.pData;
			++bufferCounter;
		}

		indexCount = 0;
		vertexCount = 0;
		drawCount = 0;

		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
			{
				const cgltf_primitive* prim = &data->meshes[i].primitives[p];
				/************************************************************************/
				// Fill index buffer for this primitive
				/************************************************************************/
				if (sizeof(uint16_t) == indexStride)
				{
					uint16_t* dst = (uint16_t*)indexUpdateDesc.pMappedData;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
				}
				else
				{
					uint32_t* dst = (uint32_t*)indexUpdateDesc.pMappedData;
					for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
						dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
				}
				/************************************************************************/
				// Fill vertex buffers for this primitive
				/************************************************************************/
				for (uint32_t a = 0; a < prim->attributes_count; ++a)
				{
					cgltf_attribute* attr = &prim->attributes[a];
					uint32_t index = cgltf_attrib_type_to_semantic(attr->type, attr->index);

					if (vertexOffsets[index] != UINT_MAX)
					{
						const uint32_t binding = vertexBindings[index];
						const uint32_t offset = vertexOffsets[index];
						const uint32_t stride = vertexStrides[binding];
						const uint8_t* src = (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;

						// If this vertex attribute is not interleaved with any other attribute use fast path instead of copying one by one
						// In this case a simple memcpy will be enough to transfer the data to the buffer
						if (1 == vertexAttribCount[binding])
						{
							uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
							if (vertexPacking[index])
								vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, 0, src, dst);
							else
								memcpy(dst, src, attr->data->count * attr->data->stride);
						}
						else
						{
							uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
							// Loop through all vertices copying into the correct place in the vertex buffer
							// Example:
							// [ POSITION | NORMAL | TEXCOORD ] => [ 0 | 12 | 24 ], [ 32 | 44 | 52 ], ... (vertex stride of 32 => 12 + 12 + 8)
							if (vertexPacking[index])
								vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, offset, src, dst);
							else
								for (uint32_t e = 0; e < attr->data->count; ++e)
									memcpy(dst + e * stride + offset, src + e * attr->data->stride, attr->data->stride);
						}
					}
				}
				/************************************************************************/
				// Fill draw arguments for this primitive
				/************************************************************************/
				geom->pDrawArgs[drawCount].mIndexCount = (uint32_t)prim->indices->count;
				geom->pDrawArgs[drawCount].mInstanceCount = 1;
				geom->pDrawArgs[drawCount].mStartIndex = indexCount;
				geom->pDrawArgs[drawCount].mStartInstance = 0;
				// Since we already offset indices when creating the index buffer, vertex offset will be zero
				// With this approach, we can draw everything in one draw call or use the traditional draw per subset without the
				// need for changing shader code
				geom->pDrawArgs[drawCount].mVertexOffset = 0;

				indexCount += (uint32_t)prim->indices->count;
				vertexCount += (uint32_t)prim->attributes->data->count;
				++drawCount;
			}
		}

		UploadFunctionResult uploadResult = UPLOAD_FUNCTION_RESULT_COMPLETED;
#if !UMA
		UpdateRequest updateRequest(indexUpdateDesc);
		UpdateState updateState = updateRequest;
		uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, updateState);

		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (vertexUpdateDesc[i].pMappedData)
			{
				UpdateRequest updateRequest(vertexUpdateDesc[i]);
				UpdateState updateState = updateRequest;
				uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, updateState);
			}
		}
#endif

		// Load the remap joint indices generated in the offline process
		uint32_t remapCount = 0;
		for (uint32_t i = 0; i < data->skins_count; ++i)
		{
			const cgltf_skin* skin = &data->skins[i];
			uint32_t extrasSize = (uint32_t)(skin->extras.end_offset - skin->extras.start_offset);
			if (extrasSize)
			{
				const char* jointRemaps = (const char*)data->json + skin->extras.start_offset;
				jsmn_parser parser = {};
				jsmntok_t* tokens = (jsmntok_t*)alloca((skin->joints_count + 1) * sizeof(jsmntok_t));
				jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
				ASSERT(tokens[0].size == skin->joints_count + 1);
				cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses, skin->joints_count * sizeof(float[16]) / sizeof(float));
				for (uint32_t r = 0; r < skin->joints_count; ++r)
					geom->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
			}

			remapCount += (uint32_t)skin->joints_count;
		}

		// Load the tressfx specific data generated in the offline process
		if (stricmp(data->asset.generator, "tressfx") == 0)
		{
			// { "mVertexCountPerStrand" : "16", "mGuideCountPerStrand" : "3456" }
			uint32_t extrasSize = (uint32_t)(data->asset.extras.end_offset - data->asset.extras.start_offset);
			const char* json = data->json + data->asset.extras.start_offset;
			jsmn_parser parser = {};
			jsmntok_t tokens[5] = {};
			jsmn_parse(&parser, (const char*)json, extrasSize, tokens, 5);
			geom->mHair.mVertexCountPerStrand = atoi(json + tokens[2].start);
			geom->mHair.mGuideCountPerStrand = atoi(json + tokens[4].start);
		}

		if (pDesc->mFlags & GEOMETRY_LOAD_FLAG_SHADOWED)
		{
			indexCount = 0;
			vertexCount = 0;

			for (uint32_t i = 0; i < data->meshes_count; ++i)
			{
				for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
				{
					const cgltf_primitive* prim = &data->meshes[i].primitives[p];
					/************************************************************************/
					// Fill index buffer for this primitive
					/************************************************************************/
					if (sizeof(uint16_t) == indexStride)
					{
						uint16_t* dst = (uint16_t*)geom->pShadow->pIndices;
						for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
							dst[indexCount + idx] = vertexCount + (uint16_t)cgltf_accessor_read_index(prim->indices, idx);
					}
					else
					{
						uint32_t* dst = (uint32_t*)geom->pShadow->pIndices;
						for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
							dst[indexCount + idx] = vertexCount + (uint32_t)cgltf_accessor_read_index(prim->indices, idx);
					}

					for (uint32_t a = 0; a < prim->attributes_count; ++a)
					{
						cgltf_attribute* attr = &prim->attributes[a];
						if (cgltf_attribute_type_position == attr->type)
						{
							const uint8_t* src = (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
					}

					indexCount += (uint32_t)prim->indices->count;
					vertexCount += (uint32_t)prim->attributes->data->count;
				}
			}
		}

		data->file_data = fileData;
		cgltf_free(data);

		fsFreePath((Path*)pDesc->pFilePath);
		conf_free(pDesc->pVertexLayout);

		*pDesc->ppGeometry = geom;

		return uploadResult;
	}

	return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}
//////////////////////////////////////////////////////////////////////////
// Resource Loader Implementation
//////////////////////////////////////////////////////////////////////////
static bool allQueuesEmpty(ResourceLoader* pLoader)
{
	for (size_t i = 0; i < MAX_GPUS; ++i)
	{
		for (size_t priority = 0; priority < LOAD_PRIORITY_COUNT; priority += 1)
		{
			if (!pLoader->mRequestQueue[i][priority].empty())
			{
				return false;
			}
		}
	}
	return true;
}

static void streamerThreadFunc(void* pThreadData)
{
	ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
	ASSERT(pLoader);

	uint32_t linkedGPUCount = pLoader->pRenderer->mLinkedNodeCount;

	const uint32_t allUploadsCompleted = (1 << linkedGPUCount) - 1;
	uint32_t       completionMask = allUploadsCompleted;

#if defined(DIRECT3D11)
	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		Buffer* pStagingBuffer = pLoader->pCopyEngines[i].resourceSets[pLoader->mActiveSetIndex].mBuffer;
		mapBuffer(pLoader->pRenderer, pStagingBuffer, NULL);
	}
#endif

	SyncToken maxToken = {};

	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();
		while (allQueuesEmpty(pLoader) && pLoader->mRun)
		{
			// Empty queue
			// Signal all tokens before going into condition variable sleep
			for (uint32_t i = 0; i < linkedGPUCount; ++i)
				for (uint32_t b = 0; b < pLoader->mDesc.mBufferCount; ++b)
					waitCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[i], b);

			// As the only writer atomicity is preserved
			pLoader->mTokenMutex.Acquire();
			for (size_t i = 0; i < LOAD_PRIORITY_COUNT; ++i)
			{
				tfrg_atomic64_store_release(&pLoader->mTokenCompleted[i], tfrg_atomic64_load_relaxed(&pLoader->mTokenCounter[i]));
			}
			pLoader->mTokenMutex.Release();
			pLoader->mTokenCond.WakeAll();

			// Sleep until someone adds an update request to the queue
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex);
		}
		pLoader->mQueueMutex.Release();

		pLoader->mStagingBufferMutex.Acquire();

		for (uint32_t i = 1; i < linkedGPUCount; ++i)
		{
			// Copy from the staging buffer for GPU 0 to the staging buffer for all other GPUs.
			Buffer* srcBuffer = pLoader->pCopyEngines[0].resourceSets[pLoader->mActiveSetIndex].mBuffer;
			Buffer* destBuffer = pLoader->pCopyEngines[i].resourceSets[pLoader->mActiveSetIndex].mBuffer;
			memcpy(destBuffer->pCpuMappedAddress, srcBuffer->pCpuMappedAddress, pLoader->pCopyEngines[0].resourceSets[pResourceLoader->mActiveSetIndex].allocatedSpace);

			pLoader->pCopyEngines[i].resourceSets[pLoader->mActiveSetIndex].mTempBuffers.resize(
				pLoader->pCopyEngines[0].resourceSets[pLoader->mActiveSetIndex].mTempBuffers.size());

			for (uint32_t j = 0; j < (uint32_t)pLoader->pCopyEngines[0].resourceSets[pLoader->mActiveSetIndex].mTempBuffers.size(); ++j)
			{
				// Copy from the staging buffer for GPU 0 to the staging buffer for all other GPUs.
				Buffer* srcBuffer = pLoader->pCopyEngines[0].resourceSets[pLoader->mActiveSetIndex].mTempBuffers[j];
				Buffer* destBuffer = pLoader->pCopyEngines[i].resourceSets[pLoader->mActiveSetIndex].mTempBuffers[j];
				memcpy(destBuffer->pCpuMappedAddress, srcBuffer->pCpuMappedAddress, pLoader->pCopyEngines[0].resourceSets[pResourceLoader->mActiveSetIndex].allocatedSpace);
			}
		}

		for (uint32_t i = 0; i < linkedGPUCount; ++i)
		{
			for (uint32_t priority = 0; priority < LOAD_PRIORITY_COUNT; priority += 1)
			{
				pLoader->mQueueMutex.Acquire();
				if (priority == LOAD_PRIORITY_UPDATE)
				{
					eastl::swap(pLoader->mActiveQueue, pLoader->mRequestQueue[i][priority]);
					pLoader->mRequestQueue[i][priority].clear();
				}
				else
				{
					// For the file loading queues, we only process one request at a time (unlike the update queue,
					// which has already written into the staging memory and therefore needs to be processed immediately.)
					pLoader->mActiveQueue.clear();
					if (!pLoader->mRequestQueue[i][priority].empty())
					{
						pLoader->mActiveQueue.push_back(pLoader->mRequestQueue[i][priority].front());
					}
				}
				pLoader->mQueueMutex.Release();

				size_t requestCount = pLoader->mActiveQueue.size();
				for (size_t j = 0; j < requestCount; j += 1)
				{
					UpdateState updateState = pLoader->mActiveQueue[j];

					UploadFunctionResult result = UPLOAD_FUNCTION_RESULT_COMPLETED;
					switch (updateState.mRequest.mType)
					{
					case UPDATE_REQUEST_UPDATE_BUFFER:
						result = updateBuffer(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex, updateState);
						break;
					case UPDATE_REQUEST_UPDATE_TEXTURE:
						result = updateTexture(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex, updateState);
						break;
					case UPDATE_REQUEST_UPDATE_RESOURCE_STATE:
						result = updateResourceState(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex, updateState);
						break;
					case UPDATE_REQUEST_LOAD_TEXTURE:
						result = loadTexture(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex, updateState);
						break;
					case UPDATE_REQUEST_LOAD_GEOMETRY:
						result = loadGeometry(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex, updateState);
					case UPDATE_REQUEST_INVALID:
						break;
					}

					bool completed = result == UPLOAD_FUNCTION_RESULT_COMPLETED || result == UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

					completionMask |= completed << i;

					if (updateState.mRequest.mWaitIndex && completed)
					{
						// It's a queue, so items need to be processed in order for the SyncToken to work, but we're also inserting retries.
						// We _have_ to execute everything with a staging buffer attached in the same frame since otherwise the activeSetIndex changes.

						ASSERT(maxToken.mWaitIndex[priority] < updateState.mRequest.mWaitIndex);
						maxToken.mWaitIndex[priority] = updateState.mRequest.mWaitIndex;
					}

					ASSERT(result != UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL || priority != LOAD_PRIORITY_UPDATE);

					if (priority != LOAD_PRIORITY_UPDATE)
					{
						if (result != UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL)
						{
							pLoader->mQueueMutex.Acquire();
							pLoader->mRequestQueue[i][priority].pop_front(); // We successfully processed the front item.
							pLoader->mQueueMutex.Release();

							break; // Don't process any more items.
						}
					}
				}
			}
		}

		if (completionMask != 0)
		{
			for (uint32_t i = 0; i < linkedGPUCount; ++i)
			{
				streamerFlush(&pLoader->pCopyEngines[i], pLoader->mActiveSetIndex);
			}

			SyncToken nextToken = max(maxToken, getLastTokenCompleted());
			for (size_t i = 0; i < LOAD_PRIORITY_COUNT; ++i)
			{
				pLoader->mCurrentTokenState[pLoader->mActiveSetIndex].mWaitIndex[i] = nextToken.mWaitIndex[i];
			}

			pLoader->mActiveSetIndex = (pLoader->mActiveSetIndex + 1) % pLoader->mDesc.mBufferCount;
			for (uint32_t i = 0; i < linkedGPUCount; ++i)
			{
				waitCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex);
				resetCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mActiveSetIndex);
			}

			// As the only writer atomicity is preserved
			pLoader->mTokenMutex.Acquire();
			for (size_t i = 0; i < LOAD_PRIORITY_COUNT; ++i)
			{
				tfrg_atomic64_store_release(&pLoader->mTokenCompleted[i], pLoader->mCurrentTokenState[pLoader->mActiveSetIndex].mWaitIndex[i]);
			}
			pLoader->mTokenMutex.Release();

			pLoader->mTokenCond.WakeAll();
		}

		pLoader->mStagingBufferMutex.Release();
	}

	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		streamerFlush(&pLoader->pCopyEngines[i], pLoader->mActiveSetIndex);
		waitQueueIdle(pLoader->pCopyEngines[i].pQueue);
		cleanupCopyEngine(pLoader->pRenderer, &pLoader->pCopyEngines[i]);
	}
}

ResourceLoaderDesc gDefaultResourceLoaderDesc = { 32ull << 20, 2 };

static void addResourceLoader(Renderer* pRenderer, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
{
	ResourceLoader* pLoader = conf_new(ResourceLoader);
	pLoader->pRenderer = pRenderer;

	pLoader->mRun = true;
	pLoader->mDesc = pDesc ? *pDesc : gDefaultResourceLoaderDesc;

	pLoader->mQueueMutex.Init();
	pLoader->mTokenMutex.Init();
	pLoader->mQueueCond.Init();
	pLoader->mTokenCond.Init();
	pLoader->mStagingBufferMutex.Init();

	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		tfrg_atomic64_store_release(&pLoader->mTokenCounter[i], 0);
		tfrg_atomic64_store_release(&pLoader->mTokenCompleted[i], 0);
	}

	uint32_t linkedGPUCount = pLoader->pRenderer->mLinkedNodeCount;
	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		setupCopyEngine(pLoader->pRenderer, &pLoader->pCopyEngines[i], i, pLoader->mDesc.mBufferSize, pLoader->mDesc.mBufferCount);
	}

	pLoader->mThreadDesc.pFunc = streamerThreadFunc;
	pLoader->mThreadDesc.pData = pLoader;

#if defined(NX64)
	pLoader->mThreadDesc.pThreadStack = aligned_alloc(THREAD_STACK_ALIGNMENT_NX, ALIGNED_THREAD_STACK_SIZE_NX);
	pLoader->mThreadDesc.hThread = &pLoader->mThreadType;
#endif

	pLoader->mThread = create_thread(&pLoader->mThreadDesc);

	*ppLoader = pLoader;
}

static void removeResourceLoader(ResourceLoader* pLoader)
{
	pLoader->mRun = false;
	pLoader->mQueueCond.WakeOne();
	destroy_thread(pLoader->mThread);
	pLoader->mQueueCond.Destroy();
	pLoader->mTokenCond.Destroy();
	pLoader->mQueueMutex.Destroy();
	pLoader->mTokenMutex.Destroy();
	pLoader->mStagingBufferMutex.Destroy();

	conf_delete(pLoader);
}

static void queueResourceUpdate(ResourceLoader* pLoader, BufferUpdateDesc* pBufferUpdate, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pBufferUpdate->pBuffer->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(*pBufferUpdate));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, TextureLoadDescInternal* pTextureUpdate, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pTextureUpdate->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, GeometryLoadDesc* pGeometryLoad, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pGeometryLoad->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(*pGeometryLoad));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, TextureUpdateDescInternal* pTextureUpdate, SyncToken* token, LoadPriority priority)
{
	ASSERT(pTextureUpdate->mStagingAllocation.pData);

	uint32_t nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, Buffer* pBuffer, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pBuffer->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(pBuffer));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, Texture* pTexture, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(pTexture));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static SyncToken getLastTokenCompleted(ResourceLoader* pLoader)
{
	SyncToken result = {};
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		result.mWaitIndex[i] = tfrg_atomic64_load_acquire(&pLoader->mTokenCompleted[i]);
	}
	return result;
}

static bool isTokenCompleted(ResourceLoader* pLoader, const SyncToken* token)
{
	bool completed = true;
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		completed = completed && token->mWaitIndex[i] <= tfrg_atomic64_load_acquire(&pLoader->mTokenCompleted[i]);
	}
	return completed;
}

static void waitForToken(ResourceLoader* pLoader, const SyncToken* token)
{
	pLoader->mTokenMutex.Acquire();
	while (!isTokenCompleted(token))
	{
		pLoader->mTokenCond.Wait(pLoader->mTokenMutex);
	}
	pLoader->mTokenMutex.Release();
}

void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc)
{
	addResourceLoader(pRenderer, pDesc, &pResourceLoader);

	ResourceLoader::InitImageClass();
}

void exitResourceLoaderInterface(Renderer* pRenderer)
{
	ResourceLoader::ExitImageClass();

	removeResourceLoader(pResourceLoader);
}

static uint64_t getMaximumStagingAllocationSize()
{
	return pResourceLoader->pCopyEngines[0].bufferSize;
}

static void beginAddBuffer(BufferLoadDesc* pBufferDesc)
{
	ASSERT(pBufferDesc->ppBuffer);

	bool update = !pBufferDesc->mSkipUpload || pBufferDesc->mForceReset;

	pBufferDesc->mDesc.mStartState = update ? RESOURCE_STATE_COMMON : pBufferDesc->mDesc.mStartState;
	addBuffer(pResourceLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

	if (update)
	{
		BufferUpdateDesc bufferUpdate = { *pBufferDesc->ppBuffer };
		bufferUpdate.mSize = pBufferDesc->mDesc.mSize;
		beginUpdateResource(&bufferUpdate);
		memcpy(bufferUpdate.pMappedData, pBufferDesc->pData, bufferUpdate.mSize);
		pBufferDesc->mInternalData = bufferUpdate.mInternalData;

		if (pBufferDesc->mForceReset)
		{
			memset(bufferUpdate.pMappedData, 0, bufferUpdate.mSize);
		}
	}
}

static void endAddBuffer(BufferLoadDesc* pBufferDesc, SyncToken* token, LoadPriority priority)
{
	ASSERT(pBufferDesc->ppBuffer);

	bool update = pBufferDesc->mInternalData.mMappedRange.pData;

	if (update)
	{
		BufferUpdateDesc bufferUpdate = { *pBufferDesc->ppBuffer };
		bufferUpdate.mSize = pBufferDesc->mDesc.mSize;
		bufferUpdate.pMappedData = pBufferDesc->mInternalData.mMappedRange.pData;
		bufferUpdate.mInternalData = pBufferDesc->mInternalData;
		endUpdateResource(&bufferUpdate, token);

		pBufferDesc->mInternalData = {};
	}
	else
	{
		// Transition GPU buffer to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mApi == RENDERER_API_VULKAN &&
			pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pBufferDesc->mDesc.mStartState != RESOURCE_STATE_UNDEFINED && pBufferDesc->mDesc.mStartState != RESOURCE_STATE_COMMON))
			queueResourceUpdate(pResourceLoader, *pBufferDesc->ppBuffer, token, priority);
	}
}

static void beginAddTexture(TextureLoadDesc* pTextureDesc)
{
	ASSERT(pTextureDesc->ppTexture);

	if (pTextureDesc->pRawImageData)
	{
		TextureDesc textureDesc = {};
		if (pTextureDesc->pDesc != NULL)
		{
			textureDesc = *pTextureDesc->pDesc;
		}
		else
		{
			textureDesc.mFormat = pTextureDesc->pRawImageData->mFormat;
			textureDesc.mWidth = pTextureDesc->pRawImageData->mWidth;
			textureDesc.mHeight = pTextureDesc->pRawImageData->mHeight;
			textureDesc.mDepth = pTextureDesc->pRawImageData->mDepth;
			textureDesc.mMipLevels = pTextureDesc->pRawImageData->mMipLevels;
			textureDesc.mArraySize = pTextureDesc->pRawImageData->mArraySize;
			textureDesc.mFlags = pTextureDesc->mCreationFlag;
			textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
			textureDesc.mStartState = RESOURCE_STATE_COMMON;
			textureDesc.mSampleCount = SAMPLE_COUNT_1;
			textureDesc.mSampleQuality = 0;
			textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;
		}

		addTexture(pResourceLoader->pRenderer, &textureDesc, pTextureDesc->ppTexture);

		TextureUpdateDesc updateDesc = {};
		updateDesc.pTexture = *pTextureDesc->ppTexture;
		updateDesc.pRawImageData = pTextureDesc->pRawImageData;
		beginUpdateResource(&updateDesc);
		pTextureDesc->mInternalData.mMappedRange = updateDesc.mInternalData.mMappedRange;
	}
}

static void endAddTexture(TextureLoadDesc* pTextureDesc, SyncToken* token, LoadPriority priority)
{
	ASSERT(pTextureDesc->ppTexture);

	if (!pTextureDesc->pFilePath && !pTextureDesc->pRawImageData && !pTextureDesc->pBinaryImageData && pTextureDesc->pDesc)
	{
		// If texture is supposed to be filled later (UAV / Update later / ...) proceed with the mStartState provided by the user in the texture description
		addTexture(pResourceLoader->pRenderer, pTextureDesc->pDesc, pTextureDesc->ppTexture);

		// Transition texture to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mApi == RENDERER_API_VULKAN &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pTextureDesc->pDesc->mStartState != RESOURCE_STATE_UNDEFINED && pTextureDesc->pDesc->mStartState != RESOURCE_STATE_COMMON))
			queueResourceUpdate(pResourceLoader, *pTextureDesc->ppTexture, token, priority);
		return;
	}
	else if (pTextureDesc->pRawImageData)
	{
		TextureUpdateDesc updateDesc = {};
		updateDesc.pTexture = *pTextureDesc->ppTexture;
		updateDesc.pRawImageData = pTextureDesc->pRawImageData;
		updateDesc.pMappedData = pTextureDesc->mInternalData.mMappedRange.pData;
		updateDesc.mInternalData.mMappedRange = pTextureDesc->mInternalData.mMappedRange;
		endUpdateResource(&updateDesc, token);

		pTextureDesc->mInternalData.mMappedRange = {};
	}
	else
	{
		TextureLoadDescInternal updateDesc = {};
		updateDesc.ppTexture = pTextureDesc->ppTexture;
		updateDesc.pFilePath = fsCopyPath(pTextureDesc->pFilePath);
		updateDesc.mNodeIndex = pTextureDesc->mNodeIndex;
		if (pTextureDesc->pBinaryImageData)
			updateDesc.mBinaryImageData = *pTextureDesc->pBinaryImageData;
		updateDesc.mCreationFlag = pTextureDesc->mCreationFlag;
		queueResourceUpdate(pResourceLoader, &updateDesc, token, priority);
	}
}

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token, LoadPriority priority)
{
	uint64_t stagingBufferSize = getMaximumStagingAllocationSize();
	ASSERT(stagingBufferSize > 0);

	if (pBufferDesc->pData && pBufferDesc->mDesc.mSize > stagingBufferSize && pBufferDesc->mDesc.mMemoryUsage != RESOURCE_MEMORY_USAGE_CPU_ONLY)
	{
		// The data is too large for a single staging buffer copy, so perform it in stages.

		// Save the data parameter so we can restore it later.
		const void* data = pBufferDesc->pData;

		pBufferDesc->mSkipUpload = true;

		beginAddBuffer(pBufferDesc);
		endAddBuffer(pBufferDesc, token, priority);

		BufferUpdateDesc updateDesc = {};
		updateDesc.pBuffer = *pBufferDesc->ppBuffer;
		for (uint64_t offset = 0; offset < pBufferDesc->mDesc.mSize; offset += stagingBufferSize)
		{
			size_t chunkSize = min(stagingBufferSize, pBufferDesc->mDesc.mSize - offset);
			updateDesc.mSize = chunkSize;
			updateDesc.mDstOffset = offset;
			beginUpdateResource(&updateDesc);
			memcpy(updateDesc.pMappedData, (char*)data + offset, chunkSize);
			endUpdateResource(&updateDesc, token);
		}
	}
	else
	{
		pBufferDesc->mSkipUpload = pBufferDesc->pData == NULL && !pBufferDesc->mForceReset;

		beginAddBuffer(pBufferDesc);
		endAddBuffer(pBufferDesc, token, priority);
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token, LoadPriority priority)
{
	beginAddTexture(pTextureDesc);
	endAddTexture(pTextureDesc, token, priority);
}

void addResource(GeometryLoadDesc* pDesc, SyncToken* token, LoadPriority priority)
{
	ASSERT(pDesc->ppGeometry);

	GeometryLoadDesc updateDesc = *pDesc;
	updateDesc.pFilePath = fsCopyPath(pDesc->pFilePath);
	updateDesc.pVertexLayout = (VertexLayout*)conf_calloc(1, sizeof(VertexLayout));
	memcpy(updateDesc.pVertexLayout, pDesc->pVertexLayout, sizeof(VertexLayout));
	queueResourceUpdate(pResourceLoader, &updateDesc, token, priority);
}

void removeResource(Texture* pTexture)
{
	removeTexture(pResourceLoader->pRenderer, pTexture);
}

void removeResource(Buffer* pBuffer)
{
	removeBuffer(pResourceLoader->pRenderer, pBuffer);
}

void removeResource(Geometry* pGeom)
{
	removeResource(pGeom->pIndexBuffer);

	for (uint32_t i = 0; i < pGeom->mVertexBufferCount; ++i)
		removeResource(pGeom->pVertexBuffers[i]);

	conf_free(pGeom);
}

void beginUpdateResource(BufferUpdateDesc* pBufferUpdate)
{
	Buffer* pBuffer = pBufferUpdate->pBuffer;
	ASSERT(pBuffer);

	uint64_t size = pBufferUpdate->mSize > 0 ? pBufferUpdate->mSize : (pBufferUpdate->pBuffer->mSize - pBufferUpdate->mDstOffset);
	ASSERT(pBufferUpdate->mDstOffset + size <= pBuffer->mSize);

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (UMA || memoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || memoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU || memoryUsage == RESOURCE_MEMORY_USAGE_UNKNOWN)
	{
		// We can directly provide the buffer's CPU-accessible address.
		bool map = !pBuffer->pCpuMappedAddress;

		if (map)
		{
			mapBuffer(pResourceLoader->pRenderer, pBuffer, NULL);
		}

		pBufferUpdate->mInternalData.mMappedRange = { (uint8_t*)pBuffer->pCpuMappedAddress + pBufferUpdate->mDstOffset, pBuffer };
		pBufferUpdate->pMappedData = pBufferUpdate->mInternalData.mMappedRange.pData;
		pBufferUpdate->mInternalData.mBufferNeedsUnmap = map;
	}
	else
	{
		// We need to use a staging buffer.
		pResourceLoader->mStagingBufferMutex.Acquire();
		MappedMemoryRange range = allocateStagingMemory(size, RESOURCE_BUFFER_ALIGNMENT, /* waitForSpace = */ true);
		pBufferUpdate->pMappedData = range.pData;

		pBufferUpdate->mInternalData.mMappedRange = range;
		pBufferUpdate->mInternalData.mBufferNeedsUnmap = false;
	}
}

void beginUpdateResource(TextureUpdateDesc* pTextureUpdate)
{
	if (!pTextureUpdate->pRawImageData)
	{
		return;
	}

	RawImageData& rawData = *pTextureUpdate->pRawImageData;
	const uint8_t* sourceData = rawData.pRawData;

	uint32_t textureAlignment = ResourceLoader::GetSubtextureAlignment(pResourceLoader->pRenderer);
	uint32_t textureRowAlignment = ResourceLoader::GetTextureRowAlignment(pResourceLoader->pRenderer);
	size_t requiredSize = ResourceLoader::GetImageSize(rawData.mFormat, rawData.mWidth, rawData.mHeight, rawData.mDepth, rawData.mMipLevels, rawData.mArraySize, textureRowAlignment, textureAlignment);

	pResourceLoader->mStagingBufferMutex.Acquire();
	MappedMemoryRange range = allocateStagingMemory(requiredSize, textureAlignment, /* waitForSpace = */ true);
	pTextureUpdate->pMappedData = range.pData;
	pTextureUpdate->mInternalData.mMappedRange = range;

	uint32_t sourceRowStride = rawData.mRowStride;
	if (sourceRowStride == 0)
	{
		uint32_t bytesPerBlock = TinyImageFormat_BitSizeOfBlock(rawData.mFormat) / 8;
		uint32_t blocksPerRow = (rawData.mWidth + TinyImageFormat_WidthOfBlock(rawData.mFormat) - 1) / TinyImageFormat_WidthOfBlock(rawData.mFormat);
		sourceRowStride = blocksPerRow * bytesPerBlock;
	}

	rawData.mRowStride = round_up(sourceRowStride, textureRowAlignment);

	if (sourceData)
	{
		if (rawData.mRowStride == sourceRowStride)
		{
			memcpy(pTextureUpdate->pMappedData, sourceData, requiredSize);
		}
		else
		{
			size_t rowCount = (rawData.mHeight + TinyImageFormat_HeightOfBlock(rawData.mFormat) - 1) / TinyImageFormat_HeightOfBlock(rawData.mFormat);
			for (size_t row = 0; row < rowCount; row += 1)
			{
				const uint8_t* src = sourceData + row * sourceRowStride;
				uint8_t* dst = (uint8_t*)pTextureUpdate->pMappedData + row * rawData.mRowStride;
				memcpy(dst, src, min(sourceRowStride, rawData.mRowStride));
			}
		}
	}
}

void endUpdateResource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	if (pBufferUpdate->mInternalData.mBufferNeedsUnmap)
	{
		unmapBuffer(pResourceLoader->pRenderer, pBufferUpdate->pBuffer);
	}

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (!UMA && (memoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU || memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY))
	{
		queueResourceUpdate(pResourceLoader, pBufferUpdate, token, LOAD_PRIORITY_UPDATE);
		// We need to hold the staging buffer mutex until after enqueuing the update to ensure the active set doesn't change in between allocating the staging memory and submitting to the queue.
		pResourceLoader->mStagingBufferMutex.Release();
	}

	// Restore the state to before the beginUpdateResource call.
	pBufferUpdate->pMappedData = NULL;
	pBufferUpdate->mInternalData = {};
}

void endUpdateResource(TextureUpdateDesc* pTextureUpdate, SyncToken* token)
{
	TextureUpdateDescInternal desc = {};
	desc.pTexture = pTextureUpdate->pTexture;
	desc.mStagingAllocation = pTextureUpdate->mInternalData.mMappedRange;

	if (pTextureUpdate->pRawImageData)
	{
		desc.mRawImageData = *pTextureUpdate->pRawImageData;
	}
	else
	{
		ASSERT(false && "TextureUpdateDesc::pRawImageData cannot be NULL");
		return;
	}

	queueResourceUpdate(pResourceLoader, &desc, token, LOAD_PRIORITY_UPDATE);
	// We need to hold the staging buffer mutex until after enqueuing the update to ensure the active set doesn't change in between allocating the staging memory and submitting to the queue.
	pResourceLoader->mStagingBufferMutex.Release();

	// Restore the state to before the beginUpdateResource call.
	pTextureUpdate->pMappedData = NULL;
	pTextureUpdate->mInternalData = {};
}

SyncToken getLastTokenCompleted()
{
	SyncToken emptyToken = {};
	return pResourceLoader ? getLastTokenCompleted(pResourceLoader) : emptyToken;
}

bool isTokenCompleted(const SyncToken* token)
{
	if (!pResourceLoader) { return false; }
	return isTokenCompleted(pResourceLoader, token);
}

void waitForToken(const SyncToken* token)
{
	waitForToken(pResourceLoader, token);
}

bool allResourceLoadsCompleted()
{
	SyncToken token = {};
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		token.mWaitIndex[i] = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter[i]);
	}
	return isTokenCompleted(pResourceLoader, &token);
}

void waitForAllResourceLoads()
{
	SyncToken token = {};
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		token.mWaitIndex[i] = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter[i]);
	}
	waitForToken(pResourceLoader, &token);
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
	Renderer* pRenderer, ShaderStage stage, uint32_t codeSize, const char* code, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>* pByteCode, const char* pEntryPoint)
{
	// compile into spir-V shader
	shaderc_compiler_t           compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t	 options = shaderc_compile_options_initialize();
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderc_compile_options_add_macro_definition(options, pMacros[i].definition, strlen(pMacros[i].definition),
			pMacros[i].value, strlen(pMacros[i].value));
	}
	shaderc_compilation_result_t spvShader =
		shaderc_compile_into_spv(compiler, code, codeSize, getShadercShaderType(stage), "shaderc_error", pEntryPoint ? pEntryPoint : "main", options);
	shaderc_compilation_status spvStatus = shaderc_result_get_compilation_status(spvShader);
	if (spvStatus != shaderc_compilation_status_success)
	{
		const char* errorMessage = shaderc_result_get_error_message(spvShader);
		LOGF(LogLevel::eERROR, "Shader compiling failed! with status %s", errorMessage);
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
	Renderer* pRenderer, ShaderTarget target, const Path* filePath, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>* pByteCode, const char* pEntryPoint)
{
	PathHandle parentDirectory = fsCopyParentPath(outFilePath);
	if (!fsFileExists(parentDirectory))
		fsCreateDirectory(parentDirectory);

	eastl::string                  commandLine;

	// If there is a config file located in the shader source directory use it to specify the limits
	PathHandle configFilePath = fsAppendPathComponent(filePath, "config.conf");
	if (fsFileExists(configFilePath))
	{
		// Add command to compile from Vulkan GLSL to Spirv
		commandLine.append_sprintf(
			"\"%s\" -V \"%s\" -o \"%s\"",
			fsGetPathAsNativeString(configFilePath),
			fsGetPathAsNativeString(filePath),
			fsGetPathAsNativeString(outFilePath));
	}
	else
	{
		commandLine.append_sprintf("-V \"%s\" -o \"%s\"",
			fsGetPathAsNativeString(filePath),
			fsGetPathAsNativeString(outFilePath));
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
		commandLine += " \"-D" + eastl::string(pMacros[i].definition) + "=" + pMacros[i].value + "\"";
	}

	eastl::string glslangValidator = getenv("VULKAN_SDK");
	if (glslangValidator.size())
		glslangValidator += "/bin/glslangValidator";
	else
		glslangValidator = "/usr/bin/glslangValidator";

	const char* args[1] = { commandLine.c_str() };

	eastl::string fileName = fsPathComponentToString(fsGetPathFileName(outFilePath));
	eastl::string logFileName = fileName + "_compile.log";
	PathHandle logFilePath = fsAppendPathComponent(parentDirectory, logFileName.c_str());
	if (fsFileExists(logFilePath))
	{
		fsDeleteFile(logFilePath);
	}
	LOGF(LogLevel::eINFO, "args: ", args);
	if (systemRun(glslangValidator.c_str(), args, 1, logFilePath) == 0)
	{
		FileStream* fh = fsOpenFile(outFilePath, FM_READ_BINARY);
		//Check if the File Handle exists
		ASSERT(fh);
		pByteCode->resize(fsGetStreamFileSize(fh));
		fsReadFromStream(fh, pByteCode->data(), pByteCode->size());
		fsCloseStream(fh);
	}
	else
	{
		FileStream* fh = fsOpenFile(logFilePath, FM_READ_BINARY);
		// If for some reason the error file could not be created just log error msg
		if (!fh)
		{
			LOGF(LogLevel::eERROR, "Failed to compile shader %s", fsGetPathAsNativeString(filePath));
		}
		else
		{
			eastl::string errorLog = fsReadFromStreamSTLString(fh);
			LOGF(LogLevel::eERROR, "Failed to compile shader %s with error\n%s", fsGetPathAsNativeString(filePath), errorLog.c_str());
		}
		fsCloseStream(fh);
	}
}
#endif
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(
	Renderer* pRenderer, const Path* sourcePath, const Path* outFilePath, uint32_t macroCount, ShaderMacro* pMacros,
	eastl::vector<char>* pByteCode, const char* /*pEntryPoint*/)
{

	PathHandle outFileDirectory = fsCopyParentPath(outFilePath);
	if (!fsFileExists(outFileDirectory))
	{
		fsCreateDirectory(outFileDirectory);
	}

	PathHandle intermediateFile = fsAppendPathExtension(outFilePath, "air");

	const char *xcrun = "/usr/bin/xcrun";
	eastl::vector<eastl::string> args;
	eastl::string tmpArg = "";

	// Compile the source into a temporary .air file.
	args.push_back("-sdk");
	args.push_back("macosx");
	args.push_back("metal");
	args.push_back("-c");
	args.push_back(fsGetPathAsNativeString(sourcePath));
	args.push_back("-o");
	args.push_back(fsGetPathAsNativeString(intermediateFile));

	//enable the 2 below for shader debugging on xcode
	//args.push_back("-MO");
	//args.push_back("-gline-tables-only");
	args.push_back("-D");
	args.push_back("MTL_SHADER=1");    // Add MTL_SHADER macro to differentiate structs in headers shared by app/shader code.

	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		args.push_back("-D");
		args.push_back(eastl::string(pMacros[i].definition) + "=" + pMacros[i].value);
	}

	eastl::vector<const char*> cArgs;
	for (eastl::string& arg : args) {
		cArgs.push_back(arg.c_str());
	}

	if (systemRun(xcrun, &cArgs[0], cArgs.size(), NULL) == 0)
	{
		// Create a .metallib file from the .air file.
		args.clear();
		tmpArg = "";
		args.push_back("-sdk");
		args.push_back("macosx");
		args.push_back("metallib");
		args.push_back(fsGetPathAsNativeString(intermediateFile));
		args.push_back("-o");
		tmpArg = eastl::string().sprintf(
			""
			"%s"
			"",
			fsGetPathAsNativeString(outFilePath));
		args.push_back(tmpArg);

		cArgs.clear();
		for (eastl::string& arg : args) {
			cArgs.push_back(arg.c_str());
		}

		if (systemRun(xcrun, &cArgs[0], cArgs.size(), NULL) == 0)
		{
			// Remove the temp .air file.
			const char *nativePath = fsGetPathAsNativeString(intermediateFile);
			systemRun("rm", &nativePath, 1, NULL);

			// Store the compiled bytecode.
			FileStream* fHandle = fsOpenFile(outFilePath, FM_READ_BINARY);

			ASSERT(fHandle);
			pByteCode->resize(fsGetStreamFileSize(fHandle));
			fsReadFromStream(fHandle, pByteCode->data(), pByteCode->size());
			fsCloseStream(fHandle);
		}
		else
		{
			LOGF(eERROR, "Failed to assemble shader's %s .metallib file", fsGetPathFileName(outFilePath));
		}
	}
	else
	{
		LOGF(eERROR, "Failed to compile shader %s", fsGetPathFileName(outFilePath));
	}
}
#endif
#if (defined(DIRECT3D12) || defined(DIRECT3D11))
extern void compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const Path* filePath, uint32_t codeSize, const char* code,
	uint32_t macroCount, ShaderMacro* pMacros, void* (*allocator)(size_t a, const char *f, int l, const char *sf), uint32_t* pByteCodeSize, char** ppByteCode, const char* pEntryPoint);
#endif
#if defined(ORBIS)
extern void orbis_copyInclude(const char* appName, FileStream* fHandle, PathHandle& includeFilePath);
extern void orbis_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const Path* filePath, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>* pByteCode, const char* pEntryPoint);
#endif

// Function to generate the timestamp of this shader source file considering all include file timestamp
static bool process_source_file(const char* pAppName, FileStream* original, const Path* filePath, FileStream* file, time_t& outTimeStamp, eastl::string& outCode)
{
	// If the source if a non-packaged file, store the timestamp
	if (file)
	{
		time_t fileTimeStamp = fsGetLastModifiedTime(filePath);

		if (fileTimeStamp > outTimeStamp)
			outTimeStamp = fileTimeStamp;
	}
	else
	{
		return true; // The source file is missing, but we may still be able to use the shader binary.
	}

	PathHandle fileDirectory = fsCopyParentPath(filePath);

	const eastl::string pIncludeDirective = "#include";
	while (!fsStreamAtEnd(file))
	{
		eastl::string line = fsReadFromStreamSTLLine(file);

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
			//TODO: Remove Comments

			if (fileName.at(0) == '<')    // disregard bracketsauthop
				continue;

			PathHandle includeFilePath = fsAppendPathComponent(fileDirectory, fileName.c_str());

			// open the include file
			FileStream* fHandle = fsOpenFile(includeFilePath, FM_READ_BINARY);
			if (!fHandle)
			{
				LOGF(LogLevel::eERROR, "Cannot open #include file: %s", fsGetPathAsNativeString(filePath));
				continue;
			}

#if defined(ORBIS)
			orbis_copyInclude(pAppName, fHandle, includeFilePath);
#endif

			// Add the include file into the current code recursively
			if (!process_source_file(pAppName, original, includeFilePath, fHandle, outTimeStamp, outCode))
			{
				fsCloseStream(fHandle);
				return false;
			}

			fsCloseStream(fHandle);
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
bool check_for_byte_code(const Path* binaryShaderPath, time_t sourceTimeStamp, eastl::vector<char>& byteCode)
{
	if (!fsFileExists(binaryShaderPath))
		return false;

	// If source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
	// than source
	if (sourceTimeStamp && fsGetLastModifiedTime(binaryShaderPath) < sourceTimeStamp)
		return false;

	FileStream* fh = fsOpenFile(binaryShaderPath, FM_READ_BINARY);
	if (!fh)
	{
		LOGF(LogLevel::eERROR, (eastl::string(fsGetPathAsNativeString(binaryShaderPath)) + " is not a valid shader bytecode file").c_str());
		return false;
	}

	ssize_t size = fsGetStreamFileSize(fh);
	byteCode.resize(fsGetStreamFileSize(fh));
	fsReadFromStream(fh, byteCode.data(), size);
	fsCloseStream(fh);

	return true;
}

// Saves bytecode to a file
bool save_byte_code(const Path* binaryShaderPath, const eastl::vector<char>& byteCode)
{
	PathHandle parentDirectory = fsCopyParentPath(binaryShaderPath);
	if (!fsFileExists(parentDirectory))
	{
		fsCreateDirectory(parentDirectory);
	}

	FileStream* fh = fsOpenFile(binaryShaderPath, FM_WRITE_BINARY);

	if (!fh)
		return false;

	fsWriteToStream(fh, byteCode.data(), byteCode.size() * sizeof(char));
	fsCloseStream(fh);

	return true;
}

bool load_shader_stage_byte_code(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const Path* filePath, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>& byteCode,
	const char* pEntryPoint)
{
	eastl::string code;
	time_t          timeStamp = 0;

#if !defined(METAL) && !defined(NX64)
	FileStream* sourceFileStream = fsOpenFile(filePath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (!process_source_file(pRenderer->pName, sourceFileStream, filePath, sourceFileStream, timeStamp, code))
	{
		fsCloseStream(sourceFileStream);
		return false;
	}
#elif defined(NX64)
	eastl::string shaderDefines;
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
	}

	uint32_t hash = 0;
	MurmurHash3_x86_32(shaderDefines.c_str(), shaderDefines.size(), 0, &hash);

	char hashStringBuffer[10];
	sprintf(&hashStringBuffer[0], "%zu", hash);

	PathHandle nxShaderPath = fsAppendPathExtension(filePath, hashStringBuffer);
	nxShaderPath = fsAppendPathExtension(nxShaderPath, "spv");
	FileStream* sourceFileStream = fsOpenFile(nxShaderPath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (sourceFileStream)
	{
		byteCode.resize(fsGetStreamFileSize(sourceFileStream));
		fsReadFromStream(sourceFileStream, byteCode.data(), byteCode.size());

		LOGF(LogLevel::eINFO, "Shader loaded: '%s' with macro string '%s'", fsGetPathAsNativeString(nxShaderPath), shaderDefines.c_str());
	}
	else
	{
		LOGF(LogLevel::eERROR, "Failed to load shader: '%s' with macro string '%s'", fsGetPathAsNativeString(nxShaderPath), shaderDefines.c_str());
		return false;
	}

#else
	PathHandle metalShaderPath = fsAppendPathExtension(filePath, "metal");
	FileStream* sourceFileStream = fsOpenFile(metalShaderPath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (!process_source_file(pRenderer->pName, sourceFileStream, metalShaderPath, sourceFileStream, timeStamp, code))
	{
		fsCloseStream(sourceFileStream);
		return false;
	}
#endif

#ifndef NX64
	PathComponent directoryPath, fileName, extension;
	fsGetPathComponents(filePath, &directoryPath, &fileName, &extension);
	eastl::string shaderDefines;
	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
	}

	eastl::string rendererApi;
	switch (pRenderer->mApi)
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

#if defined(ORBIS)
	eastl::string binaryShaderComponent = "/host/%temp%/" + appName + "/" + fsPathComponentToString(fileName) +
		eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + fsPathComponentToString(extension) +
		eastl::string().sprintf("%u", target) + ".sb";
	PathHandle binaryShaderPath = fsCreatePath(fsGetSystemFileSystem(), binaryShaderComponent.c_str());
#else
	eastl::string binaryShaderComponent = fsPathComponentToString(fileName) +
		eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + fsPathComponentToString(extension) +
		eastl::string().sprintf("%u", target) + ".bin";

	PathHandle binaryShaderPath = fsCopyPathInResourceDirectory(RD_SHADER_BINARIES, binaryShaderComponent.c_str());
#endif

	// Shader source is newer than binary
	if (!check_for_byte_code(binaryShaderPath, timeStamp, byteCode))
	{
		if (!sourceFileStream)
		{
			LOGF(eERROR, "No source shader or precompiled binary present for file %s", fileName);
			fsCloseStream(sourceFileStream);
			return false;
		}

		if (pRenderer->mApi == RENDERER_API_METAL || pRenderer->mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
#if defined(__ANDROID__)
			vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderPath, macroCount, pMacros, &byteCode, pEntryPoint);
#else
			vk_compileShader(pRenderer, target, filePath, binaryShaderPath, macroCount, pMacros, &byteCode, pEntryPoint);
#endif
#elif defined(METAL)
			mtl_compileShader(pRenderer, metalShaderPath, binaryShaderPath, macroCount, pMacros, &byteCode, pEntryPoint);
#endif
		}
#if defined(ORBIS)
		else if (pRenderer->mApi == RENDERER_API_ORBIS)
		{
			orbis_compileShader(pRenderer, target, stage, allStages, filePath, binaryShaderPath, macroCount, pMacros, &byteCode, pEntryPoint);
		}
#endif
		else
		{
#if defined(DIRECT3D12) || defined(DIRECT3D11)
			char*    pByteCode = NULL;
			uint32_t byteCodeSize = 0;

			compileShader(
				pRenderer, target, stage, filePath, (uint32_t)code.size(), code.c_str(), macroCount, pMacros, conf_malloc_internal,
				&byteCodeSize, &pByteCode, pEntryPoint);
			byteCode.resize(byteCodeSize);

			memcpy(byteCode.data(), pByteCode, byteCodeSize);
			conf_free(pByteCode);
			if (!save_byte_code(binaryShaderPath, byteCode))
			{
				const char* shaderName = fsGetPathFileName(filePath).buffer;
				LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", shaderName);
			}
#endif
		}
		if (!byteCode.size())
		{
			LOGF(eERROR, "Error while generating bytecode for shader %s", fileName);
			fsCloseStream(sourceFileStream);
			return false;
		}
	}
#else
#endif

	fsCloseStream(sourceFileStream);
	return true;
}
#ifdef TARGET_IOS
bool find_shader_stage(const Path* filePath, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	const PathComponent extension = fsGetPathExtension(filePath);
	if (stricmp(extension.buffer, "vert") == 0)
	{
		*pOutStage = &pDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (stricmp(extension.buffer, "frag") == 0)
	{
		*pOutStage = &pDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
	else if (stricmp(extension.buffer, "comp") == 0)
	{
		*pOutStage = &pDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else if ((stricmp(extension.buffer, "rgen") == 0) ||
		(stricmp(extension.buffer, "rmiss") == 0) ||
		(stricmp(extension.buffer, "rchit") == 0) ||
		(stricmp(extension.buffer, "rint") == 0) ||
		(stricmp(extension.buffer, "rahit") == 0) ||
		(stricmp(extension.buffer, "rcall") == 0))
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
	const PathComponent& extension, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	if (stricmp(extension.buffer, "vert") == 0)
	{
		*pOutStage = &pBinaryDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (stricmp(extension.buffer, "frag") == 0)
	{
		*pOutStage = &pBinaryDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
#ifndef METAL
	else if (stricmp(extension.buffer, "tesc") == 0)
	{
		*pOutStage = &pBinaryDesc->mHull;
		*pStage = SHADER_STAGE_HULL;
	}
	else if (stricmp(extension.buffer, "tese") == 0)
	{
		*pOutStage = &pBinaryDesc->mDomain;
		*pStage = SHADER_STAGE_DOMN;
	}
	else if (stricmp(extension.buffer, "geom") == 0)
	{
		*pOutStage = &pBinaryDesc->mGeom;
		*pStage = SHADER_STAGE_GEOM;
	}
#endif
	else if (stricmp(extension.buffer, "comp") == 0)
	{
		*pOutStage = &pBinaryDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else if ((stricmp(extension.buffer, "rgen") == 0) ||
		(stricmp(extension.buffer, "rmiss") == 0) ||
		(stricmp(extension.buffer, "rchit") == 0) ||
		(stricmp(extension.buffer, "rint") == 0) ||
		(stricmp(extension.buffer, "rahit") == 0) ||
		(stricmp(extension.buffer, "rcall") == 0))
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
	if ((uint32_t)pDesc->mTarget > pRenderer->mShaderTarget)
	{
		eastl::string error = eastl::string().sprintf("Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)pDesc->mTarget, (uint32_t)pRenderer->mShaderTarget);
		LOGF(LogLevel::eERROR, error.c_str());
		return;
	}

#ifndef TARGET_IOS

	BinaryShaderDesc      binaryDesc = {};
	eastl::vector<char> byteCodes[SHADER_STAGE_COUNT] = {};
#if defined(METAL)
	char* pSources[SHADER_STAGE_COUNT] = {};
#endif

	ShaderStage stages = SHADER_STAGE_NONE;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName) != 0)
		{
			ResourceDirectory resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsCopyPathForResourceDirectory(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirectory(RD_SHADER_SOURCES));
			}

			PathHandle filePath = fsAppendPathComponent(resourceDirBasePath, pDesc->mStages[i].pFileName);

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			PathComponent          ext = fsGetPathExtension(filePath);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
				stages |= stage;
		}
	}
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName) != 0)
		{
			ResourceDirectory resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsCopyPathForResourceDirectory(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirectory(RD_SHADER_SOURCES));
			}

			PathHandle filePath = fsAppendPathComponent(resourceDirBasePath, pDesc->mStages[i].pFileName);

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			PathComponent          ext = fsGetPathExtension(filePath);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
			{
				const uint32_t macroCount = pDesc->mStages[i].mMacroCount + pRenderer->mBuiltinShaderDefinesCount;
				eastl::vector<ShaderMacro> macros(macroCount);
				for (uint32_t macro = 0; macro < pRenderer->mBuiltinShaderDefinesCount; ++macro)
					macros[macro] = pRenderer->pBuiltinShaderDefines[macro];
				for (uint32_t macro = 0; macro < pDesc->mStages[i].mMacroCount; ++macro)
					macros[pRenderer->mBuiltinShaderDefinesCount + macro] = pDesc->mStages[i].pMacros[macro];

				if (!load_shader_stage_byte_code(
					pRenderer, pDesc->mTarget, stage, stages, filePath, macroCount, macros.data(),
					byteCodes[i], pDesc->mStages[i].pEntryPointName))
					return;

				binaryDesc.mStages |= stage;
				pStage->pByteCode = byteCodes[i].data();
				pStage->mByteCodeSize = (uint32_t)byteCodes[i].size();
#if defined(METAL)
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "stageMain";

				PathHandle metalFilePath = fsAppendPathExtension(filePath, "metal");

				FileStream* fh = fsOpenFile(metalFilePath, FM_READ_BINARY);
				size_t metalFileSize = fsGetStreamFileSize(fh);
				pSources[i] = (char*)conf_malloc(metalFileSize + 1);
				pStage->pSource = pSources[i];
				pStage->mSourceSize = (uint32_t)metalFileSize;
				fsReadFromStream(fh, pSources[i], metalFileSize);
				pSources[i][metalFileSize] = 0; // Ensure the shader text is null-terminated
				fsCloseStream(fh);
#else
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "main";
#endif
			}
		}
	}

	addShaderBinary(pRenderer, &binaryDesc, ppShader);

#if defined(METAL)
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pSources[i])
			conf_free(pSources[i]);
	}
#endif
#else
	// Binary shaders are not supported on iOS.
	ShaderDesc desc = {};
	eastl::string codes[SHADER_STAGE_COUNT] = {};
	ShaderMacro* pMacros[SHADER_STAGE_COUNT] = {};
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName))
		{
			ResourceDirectory resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsCopyPathForResourceDirectory(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirectory(RD_SHADER_SOURCES));
			}

			PathHandle filePath = fsAppendPathComponent(resourceDirBasePath, pDesc->mStages[i].pFileName);

			ShaderStage stage;
			ShaderStageDesc* pStage = NULL;
			if (find_shader_stage(filePath, &desc, &pStage, &stage))
			{
				PathHandle metalFilePath = fsAppendPathExtension(filePath, "metal");
				FileStream* fh = fsOpenFile(metalFilePath, FM_READ_BINARY);
				ASSERT(fh);

				pStage->pName = pDesc->mStages[i].pFileName;
				time_t timestamp = 0;
				process_source_file(pRenderer->pName, fh, metalFilePath, fh, timestamp, codes[i]);
				pStage->pCode = codes[i].c_str();
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "stageMain";
				// Apply user specified shader macros
				pStage->mMacroCount = pDesc->mStages[i].mMacroCount + pRenderer->mBuiltinShaderDefinesCount;
				pMacros[i] = (ShaderMacro*)alloca(pStage->mMacroCount * sizeof(ShaderMacro));
				pStage->pMacros = pMacros[i];
				for (uint32_t j = 0; j < pDesc->mStages[i].mMacroCount; j++)
					pMacros[i][j] = pDesc->mStages[i].pMacros[j];
				// Apply renderer specified shader macros
				for (uint32_t j = 0; j < pRenderer->mBuiltinShaderDefinesCount; j++)
				{
					pMacros[i][pDesc->mStages[i].mMacroCount + j] = pRenderer->pBuiltinShaderDefines[j];
				}
				fsCloseStream(fh);
				desc.mStages |= stage;
			}
		}
	}

	addShader(pRenderer, &desc, ppShader);
#endif
}
