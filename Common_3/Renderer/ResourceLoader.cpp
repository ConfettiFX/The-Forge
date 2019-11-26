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
#include "../../Xbox/Common_3/Renderer/XBoxPrivateHeaders.h"
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

	static Image* CreateImage(const TinyImageFormat fmt, const int w, const int h, const int d, const int mipMapCount, const int arraySize, const unsigned char* rawData)
	{
		Image* pImage = AllocImage();
		pImage->Create(fmt, w, h, d, mipMapCount, arraySize, rawData);
		return pImage;
	}

	static Image* CreateImage(const Path* filePath, memoryAllocationFunc pAllocator, void* pUserData)
	{
		Image* pImage = AllocImage();
        
		if (!pImage->LoadFromFile(filePath, pAllocator, pUserData))
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
	uint3 regionSize = min<uint3>(extent * uploadBlock, pxImageDim);
	return { regionOffset.x, regionOffset.y, regionOffset.z, regionSize.x, regionSize.y, regionSize.z };
}

uint64_t GetMipMappedSizeUpto( uint3 dims, uint32_t nMipMapLevels, int32_t slices, TinyImageFormat format)
{
	uint32_t w = dims.x;
	uint32_t h = dims.y;
	uint32_t d = dims.z;

	uint64_t size = 0;
	for(uint32_t i = 0; i < nMipMapLevels;++i)
	{
		uint64_t bx = TinyImageFormat_WidthOfBlock(format);
		uint64_t by = TinyImageFormat_HeightOfBlock(format);
		uint64_t bz = TinyImageFormat_DepthOfBlock(format);

		uint64_t tmpsize = ((w + bx - 1) / bx) * ((h + by - 1) / by) * ((d + bz - 1) / bz);
		tmpsize *= slices;
		size += tmpsize;

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
	size = size * TinyImageFormat_BitSizeOfBlock(format) / 8;
	return size;
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


	// TODO: move to Image
	bool isSwizzledZCurve = !img.IsLinearLayout();

	uint32_t i = pTextureUpdate.mMipLevel;
	uint32_t j = pTextureUpdate.mArrayLayer;
	uint3 uploadOffset = pTextureUpdate.mOffset;

	// Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
	if (applyBarrieers && (uploadOffset.x == 0) && (uploadOffset.y == 0) && (uploadOffset.z == 0))
	{
		TextureBarrier preCopyBarrier = { pTexture, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier);
	}
	Extent3D          uploadGran = pCopyEngine->pQueue->mUploadGranularity;

	TinyImageFormat fmt = img.GetFormat();
	uint32_t blockSize;
	uint3 pxBlockDim;
	uint32_t	nSlices;
	uint32_t	arrayCount;

	blockSize = TinyImageFormat_BitSizeOfBlock(fmt) / 8;
	pxBlockDim = { TinyImageFormat_WidthOfBlock(fmt),
								 TinyImageFormat_HeightOfBlock(fmt),
								 TinyImageFormat_DepthOfBlock(fmt) };
	nSlices = img.IsCube() ? 6 : 1;
	arrayCount = img.GetArrayCount() * nSlices;

	const uint32 pxPerRow = max<uint32_t>(round_down(textureRowAlignment / blockSize, uploadGran.mWidth), uploadGran.mWidth);
	const uint3 queueGranularity = {pxPerRow, uploadGran.mHeight, uploadGran.mDepth};
	const uint3 fullSizeDim = {img.GetWidth(), img.GetHeight(), img.GetDepth()};

	for (; i < pTexture->mDesc.mMipLevels; ++i)
	{
		uint3 const pxImageDim{ img.GetWidth(i), img.GetHeight(i), img.GetDepth(i) };
		uint3    uploadExtent{ (pxImageDim + pxBlockDim - uint3(1)) / pxBlockDim };
		uint3    granularity{ min<uint3>(queueGranularity, uploadExtent) };
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

			uint8_t* pSrcData;
			// there are two common formats for how slices and mipmaps are laid out in
			// either a slice is just another dimension (the 4th) that doesn't undergo
			// mip map reduction
			// so images the top level is just w * h * d * s in size
			// a mipmap level is w >> mml * h >> mml * d >> mml * s in size
			// or DDS style where each slice is mipmapped as a seperate image
			if(img.AreMipsAfterSlices()) {
				pSrcData = (uint8_t *) img.GetPixels() +
						GetMipMappedSizeUpto(fullSizeDim, i, arrayCount, fmt) +
						j * srcPitches.z;
			} else {
				uint32_t n = j / nSlices;
				uint32_t k = j - n * nSlices;
				pSrcData = (uint8_t *) img.GetPixels(i, n) + k * srcPitches.z;
			}

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
		cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier);
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
	cmdResourceBarrier(pCmd, 1, bufferBarriers, 0, NULL);
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
			cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL);
		}
		else if (pUpdate.mRequest.texture)
		{
			TextureBarrier barrier = { pUpdate.mRequest.texture, pUpdate.mRequest.texture->mDesc.mStartState };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier);
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
			unsigned nextSlot = min(nextTimeslot - time, pLoader->mDesc.mTimesliceMs);
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex, nextSlot);
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

	pLoader->mQueueMutex.Init();
	pLoader->mTokenMutex.Init();
	pLoader->mQueueCond.Init();
	pLoader->mTokenCond.Init();
	
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
	pLoader->mQueueCond.Destroy();
	pLoader->mTokenCond.Destroy();
	pLoader->mQueueMutex.Destroy();
	pLoader->mTokenMutex.Destroy();
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

	ResourceLoader::InitImageClass();
}

void removeResourceLoaderInterface(Renderer* pRenderer)
{
	ResourceLoader::ExitImageClass();

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
	
	if (pTextureDesc->pFilePath)
	{
		pImage = ResourceLoader::CreateImage(pTextureDesc->pFilePath, NULL, NULL);
#if !defined(METAL) && !defined(DIRECT3D11)
		PathComponent component = fsGetPathExtension(pTextureDesc->pFilePath);
		
		bool isSparseVirtualTexture = (component.length > 0 && strcmp(component.buffer, "svt") == 0) ? true : false;

		if (isSparseVirtualTexture)
		{
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
			pTextureDesc->pDesc = &SVTDesc;

			addVirtualTexture(pResourceLoader->pRenderer, pTextureDesc->pDesc, pTextureDesc->ppTexture, pImage->GetPixels());

			conf_free(pImage->GetPixels());
			conf_free(pImage);

			/************************************************************************/
			// Create visibility buffer
			/************************************************************************/

			eastl::vector<VirtualTexturePage>* pPageTable = (eastl::vector<VirtualTexturePage>*)(*pTextureDesc->ppTexture)->pPages;

			if(pPageTable == NULL)
				return;

			BufferLoadDesc visDesc = {};
			visDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			visDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			visDesc.mDesc.mStructStride = sizeof(uint);
			visDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			visDesc.mDesc.mSize = visDesc.mDesc.mStructStride * visDesc.mDesc.mElementCount;
			visDesc.mDesc.pDebugName = L"Vis Buffer for Sparse Texture";
			visDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mVisibility;			
			addResource(&visDesc);

			BufferLoadDesc prevVisDesc = {};
			prevVisDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			prevVisDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			prevVisDesc.mDesc.mStructStride = sizeof(uint);
			prevVisDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
			prevVisDesc.mDesc.mSize = prevVisDesc.mDesc.mStructStride * prevVisDesc.mDesc.mElementCount;
			prevVisDesc.mDesc.pDebugName = L"Prev Vis Buffer for Sparse Texture";
			prevVisDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mPrevVisibility;
			addResource(&prevVisDesc);

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
			alivePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mAlivePage;
			addResource(&alivePageDesc);

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
			removePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mRemovePage;
			addResource(&removePageDesc);

			BufferLoadDesc alivePageCountDesc = {};
			alivePageCountDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			alivePageCountDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(DIRECT3D12)
			alivePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#elif defined(VULKAN)
			alivePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#else
			alivePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
			alivePageCountDesc.mDesc.mStructStride = sizeof(uint);
			alivePageCountDesc.mDesc.mElementCount = 4;
			alivePageCountDesc.mDesc.mSize = alivePageCountDesc.mDesc.mStructStride * alivePageCountDesc.mDesc.mElementCount;
			alivePageCountDesc.mDesc.pDebugName = L"Alive page count buffer for Sparse Texture";
			alivePageCountDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mAlivePageCount;
			addResource(&alivePageCountDesc);

			BufferLoadDesc removePageCountDesc = {};
			removePageCountDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
			removePageCountDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(DIRECT3D12)
			removePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#elif defined(VULKAN)
			removePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#else
			removePageCountDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
			removePageCountDesc.mDesc.mStructStride = sizeof(uint);
			removePageCountDesc.mDesc.mElementCount = 4;
			removePageCountDesc.mDesc.mSize = removePageCountDesc.mDesc.mStructStride * removePageCountDesc.mDesc.mElementCount;
			removePageCountDesc.mDesc.pDebugName = L"Remove page count buffer for Sparse Texture";
			removePageCountDesc.ppBuffer = &(*pTextureDesc->ppTexture)->mRemovePageCount;
			addResource(&removePageCountDesc);

			return;
		}
		else
		{
			if (!pImage)
			{
				return;
			}			
		}
#else
		if (!pImage)
		{
			return;
		}
#endif
		freeImage = true;
	}
	else if (!pTextureDesc->pFilePath && !pTextureDesc->pRawImageData && !pTextureDesc->pBinaryImageData && pTextureDesc->pDesc)
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
		pImage->SetMipsAfterSlices(pTextureDesc->pRawImageData->mMipsAfterSlices);
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
		pImage->SetMipsAfterSlices(pTextureUpdate->pRawImageData->mMipsAfterSlices);			
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
			LOGF( LogLevel::eERROR, "Failed to compile shader %s", fsGetPathAsNativeString(filePath));
		}
		else
		{
			eastl::string errorLog = fsReadFromStreamSTLString(fh);
			LOGF( LogLevel::eERROR, "Failed to compile shader %s with error\n%s", fsGetPathAsNativeString(filePath), errorLog.c_str());
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

// Function to generate the timestamp of this shader source file considering all include file timestamp
static bool process_source_file(FileStream* original, const Path* filePath, FileStream* file, time_t& outTimeStamp, eastl::string& outCode)
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
				return false;
			}

			// Add the include file into the current code recursively
			if (!process_source_file(original, includeFilePath, fHandle, outTimeStamp, outCode))
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
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const Path* filePath, uint32_t macroCount,
	ShaderMacro* pMacros, eastl::vector<char>& byteCode,
	const char* pEntryPoint)
{
	eastl::string code;
	time_t          timeStamp = 0;

#ifndef METAL
	FileStream* sourceFileStream = fsOpenFile(filePath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (!process_source_file(sourceFileStream, filePath, sourceFileStream, timeStamp, code))
	{
        fsCloseStream(sourceFileStream);
		return false;
    }
#else
	PathHandle metalShaderPath = fsAppendPathExtension(filePath, "metal");
	FileStream* sourceFileStream = fsOpenFile(metalShaderPath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (!process_source_file(sourceFileStream, metalShaderPath, sourceFileStream, timeStamp, code))
	{
        fsCloseStream(sourceFileStream);
		return false;
    }
#endif

	PathComponent directoryPath, fileName, extension;
    fsGetPathComponents(filePath, &directoryPath, &fileName, &extension);
	eastl::string shaderDefines;
	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
	}

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

    eastl::string binaryShaderComponent = fsPathComponentToString(fileName) +
									 eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + fsPathComponentToString(extension) +
									 eastl::string().sprintf("%u", target) + ".bin";
    
    PathHandle binaryShaderPath = fsCopyPathInResourceDirectory(RD_SHADER_BINARIES, binaryShaderComponent.c_str());

	// Shader source is newer than binary
	if (!check_for_byte_code(binaryShaderPath, timeStamp, byteCode))
	{
        if (!sourceFileStream)
        {
            LOGF(eERROR, "No source shader or precompiled binary present for file %s", fileName);
            fsCloseStream(sourceFileStream);
            return false;
        }
        
		if (pRenderer->mSettings.mApi == RENDERER_API_METAL || pRenderer->mSettings.mApi == RENDERER_API_VULKAN)
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
    else if ((stricmp(extension.buffer, "rgen") == 0)    ||
             (stricmp(extension.buffer, "rmiss") == 0)    ||
             (stricmp(extension.buffer, "rchit") == 0)    ||
             (stricmp(extension.buffer, "rint") == 0)    ||
             (stricmp(extension.buffer, "rahit") == 0)    ||
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
	const Path* filePath, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
    const PathComponent extension = fsGetPathExtension(filePath);
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
    else if ((stricmp(extension.buffer, "rgen") == 0)    ||
             (stricmp(extension.buffer, "rmiss") == 0)    ||
             (stricmp(extension.buffer, "rchit") == 0)    ||
             (stricmp(extension.buffer, "rint") == 0)    ||
             (stricmp(extension.buffer, "rahit") == 0)    ||
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
	if (pDesc->mTarget > pRenderer->mSettings.mShaderTarget)
	{
		eastl::string error = eastl::string().sprintf("Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)pDesc->mTarget, (uint32_t)pRenderer->mSettings.mShaderTarget);
		LOGF( LogLevel::eERROR, error.c_str());
		return;
	}

#ifndef TARGET_IOS
    
	BinaryShaderDesc      binaryDesc = {};
	eastl::vector<char> byteCodes[SHADER_STAGE_COUNT] = {};
#if defined(METAL)
	char* pSources[SHADER_STAGE_COUNT] = {};
#endif
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
			if (find_shader_stage(filePath, &binaryDesc, &pStage, &stage))
			{
				const uint32_t macroCount = pDesc->mStages[i].mMacroCount + pRenderer->mBuiltinShaderDefinesCount;
				eastl::vector<ShaderMacro> macros(macroCount);
				for (uint32_t macro = 0; macro < pRenderer->mBuiltinShaderDefinesCount; ++macro)
					macros[macro] = pRenderer->pBuiltinShaderDefines[macro];
				for (uint32_t macro = 0; macro < pDesc->mStages[i].mMacroCount; ++macro)
					macros[pRenderer->mBuiltinShaderDefinesCount + macro] = pDesc->mStages[i].pMacros[macro];

				if (!load_shader_stage_byte_code(
						pRenderer, pDesc->mTarget, stage, filePath, macroCount, macros.data(),
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
                process_source_file(fh, metalFilePath, fh, timestamp, codes[i]);
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

#if !defined(METAL) && !defined(DIRECT3D11)
void updateVirtualTexture(Renderer* pRenderer, Queue* pQueue, TextureUpdateDesc* pTextureUpdate)
{
	updateVirtualTexture(pRenderer, pQueue, pTextureUpdate->pTexture);
}
#endif
