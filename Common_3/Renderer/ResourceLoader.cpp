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

#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#define TINYKTX_IMPLEMENTATION
#include "../ThirdParty/OpenSource/tinyktx/tinyktx.h"

#include "../ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

#define CGLTF_IMPLEMENTATION
#include "../ThirdParty/OpenSource/cgltf/cgltf.h"

#include "IRenderer.h"
#include "IResourceLoader.h"
#include "../OS/Interfaces/ILog.h"
#include "../OS/Interfaces/IThread.h"

#if defined(__ANDROID__)
#include <shaderc/shaderc.h>
#endif

#include "../OS/Core/TextureContainers.h"

#include "../OS/Interfaces/IMemory.h"

#ifdef NX64
#include "../ThirdParty/OpenSource/murmurhash3/MurmurHash3_32.h"
#endif

struct SubresourceDataDesc
{
	uint64_t                           mSrcOffset;
	uint32_t                           mMipLevel;
	uint32_t                           mArrayLayer;
#if defined(DIRECT3D11) || defined(METAL) || defined(VULKAN)
	uint32_t                           mRowPitch;
	uint32_t                           mSlicePitch;
#endif
};

#define MIP_REDUCE(s, mip) (max(1u, (uint32_t)((s) >> (mip))))

enum
{
	MAPPED_RANGE_FLAG_UNMAP_BUFFER = (1 << 0),
	MAPPED_RANGE_FLAG_TEMP_BUFFER = (1 << 1),
};

extern void addBuffer(Renderer* pRenderer, const BufferDesc* desc, Buffer** pp_buffer);
extern void removeBuffer(Renderer* pRenderer, Buffer* p_buffer);
extern void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
extern void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
extern void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pp_texture);
extern void addVirtualTexture(Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData);
extern void removeTexture(Renderer* pRenderer, Texture* p_texture);

extern void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
extern void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc);
/************************************************************************/
/************************************************************************/

// Xbox, Orbis, Prospero, iOS have unified memory
// so we dont need a command buffer to upload linear data
// A simple memcpy suffices since the GPU memory is marked as CPU write combine
#if defined(XBOX) || defined(ORBIS) || defined(PROSPERO) || defined(TARGET_IOS)
#define UMA 1
#else
#define UMA 0
#endif

#define MAX_FRAMES 3U

#ifdef DIRECT3D11
Mutex gContextLock;
#endif

ResourceLoaderDesc gDefaultResourceLoaderDesc = { 8ull << 20, 2 };
/************************************************************************/
// Surface Utils
/************************************************************************/
static inline ResourceState util_determine_resource_start_state(bool uav)
{
	if (uav)
		return RESOURCE_STATE_UNORDERED_ACCESS;
	else
		return RESOURCE_STATE_SHADER_RESOURCE;
}

static inline ResourceState util_determine_resource_start_state(const BufferDesc* pDesc)
{
	// Host visible (Upload Heap)
	if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
	{
		return RESOURCE_STATE_GENERIC_READ;
	}
	// Device Local (Default Heap)
	else if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		DescriptorType usage = (DescriptorType)pDesc->mDescriptors;

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

static inline constexpr ShaderSemantic util_cgltf_attrib_type_to_semantic(cgltf_attribute_type type, uint32_t index)
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

static inline constexpr TinyImageFormat util_cgltf_type_to_image_format(cgltf_type type, cgltf_component_type compType)
{
	switch (type)
	{
	case cgltf_type_scalar:
		if (cgltf_component_type_r_8 == compType)
			return TinyImageFormat_R8_SINT;
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

static inline uint16_t util_float_to_half(float val)
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

static inline void util_pack_float2_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	struct f2 { float x; float y; };
	f2* f = (f2*)src;
	for (uint32_t e = 0; e < count; ++e)
	{
		*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = (
			(util_float_to_half(f[e].x) & 0x0000FFFF) | ((util_float_to_half(f[e].y) << 16) & 0xFFFF0000));
	}
}

static inline uint32_t util_float2_to_unorm2x16(const float* v)
{
	uint32_t x = (uint32_t)round(clamp(v[0], 0, 1) * 65535.0f);
	uint32_t y = (uint32_t)round(clamp(v[1], 0, 1) * 65535.0f);
	return ((uint32_t)0x0000FFFF & x) | ((y << 16) & (uint32_t)0xFFFF0000);
}

#define OCT_WRAP(v, w) ((1.0f - abs((w))) * ((v) >= 0.0f ? 1.0f : -1.0f))

static inline void util_pack_float3_direction_to_half2(uint32_t count, uint32_t stride, uint32_t offset, const uint8_t* src, uint8_t* dst)
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
			*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = util_float2_to_unorm2x16(&enc.x);
		}
		else
		{
			*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) = 0;
		}
	}
}
/************************************************************************/
// Internal Structures
/************************************************************************/
typedef void(*PreMipStepFn)(FileStream* pStream, uint32_t mip);

typedef struct TextureUpdateDescInternal
{
	Texture*          pTexture;
	FileStream        mStream;
	MappedMemoryRange mRange;
	uint32_t          mBaseMipLevel;
	uint32_t          mMipLevels;
	uint32_t          mBaseArrayLayer;
	uint32_t          mLayerCount;
	PreMipStepFn      pPreMipFunc;
	bool              mMipsAfterSlice;
} TextureUpdateDescInternal;

typedef struct CopyResourceSet
{
#ifndef DIRECT3D11
	Fence*                 pFence;
#endif
	Cmd*                   pCmd;
	CmdPool*               pCmdPool;
	Buffer*                mBuffer;
	uint64_t               mAllocatedSpace;

	/// Buffers created in case we ran out of space in the original staging buffer
	/// Will be cleaned up after the fence for this set is complete
	eastl::vector<Buffer*> mTempBuffers;
} CopyResourceSet;

//Synchronization?
typedef struct CopyEngine
{
	Queue*           pQueue;
	CopyResourceSet* resourceSets;
	uint64_t         bufferSize;
	uint32_t         bufferCount;
	bool             isRecording;
} CopyEngine;

typedef enum UpdateRequestType
{
	UPDATE_REQUEST_UPDATE_BUFFER,
	UPDATE_REQUEST_UPDATE_TEXTURE,
	UPDATE_REQUEST_BUFFER_BARRIER,
	UPDATE_REQUEST_TEXTURE_BARRIER,
	UPDATE_REQUEST_LOAD_TEXTURE,
	UPDATE_REQUEST_LOAD_GEOMETRY,
	UPDATE_REQUEST_INVALID,
} UpdateRequestType;

typedef enum UploadFunctionResult
{
	UPLOAD_FUNCTION_RESULT_COMPLETED,
	UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL,
	UPLOAD_FUNCTION_RESULT_INVALID_REQUEST
} UploadFunctionResult;

struct UpdateRequest
{
	UpdateRequest(const BufferUpdateDesc& buffer) :           mType(UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	UpdateRequest(const TextureLoadDesc& texture) :           mType(UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
	UpdateRequest(const TextureUpdateDescInternal& texture) : mType(UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
	UpdateRequest(const GeometryLoadDesc& geom) :             mType(UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
	UpdateRequest(const BufferBarrier& barrier) :             mType(UPDATE_REQUEST_BUFFER_BARRIER), bufferBarrier(barrier) {}
	UpdateRequest(const TextureBarrier& barrier) :            mType(UPDATE_REQUEST_TEXTURE_BARRIER), textureBarrier(barrier) {}

	UpdateRequestType             mType = UPDATE_REQUEST_INVALID;
	uint64_t                      mWaitIndex = 0;
	Buffer*                       pUploadBuffer = NULL;
	union
	{
		BufferUpdateDesc          bufUpdateDesc;
		TextureUpdateDescInternal texUpdateDesc;
		TextureLoadDesc           texLoadDesc;
		GeometryLoadDesc          geomLoadDesc;
		BufferBarrier             bufferBarrier;
		TextureBarrier            textureBarrier;
	};
};

struct ResourceLoader
{
	Renderer*                    pRenderer;

	ResourceLoaderDesc           mDesc;

	volatile int                 mRun;
	ThreadDesc                   mThreadDesc;
	ThreadHandle                 mThread;

	Mutex                        mQueueMutex;
	ConditionVariable            mQueueCond;
	Mutex                        mTokenMutex;
	ConditionVariable            mTokenCond;
	eastl::vector<UpdateRequest> mRequestQueue[MAX_LINKED_GPUS];

	tfrg_atomic64_t              mTokenCompleted;
	tfrg_atomic64_t              mTokenCounter;

	SyncToken                    mCurrentTokenState[MAX_FRAMES];

	CopyEngine                   pCopyEngines[MAX_LINKED_GPUS];
	uint32_t                     mNextSet;
	uint32_t                     mSubmittedSets;

#if defined(NX64)
	ThreadTypeNX                 mThreadType;
	void*                        mThreadStackPtr;
#endif
};

static ResourceLoader* pResourceLoader = NULL;

static uint32_t util_get_texture_row_alignment(Renderer* pRenderer)
{
	return max(1u, pRenderer->pActiveGpuSettings->mUploadBufferTextureRowAlignment);
}

static uint32_t util_get_texture_subresource_alignment(Renderer* pRenderer, TinyImageFormat fmt = TinyImageFormat_UNDEFINED)
{
	uint32_t blockSize = max(1u, TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
	uint32_t alignment = round_up(pRenderer->pActiveGpuSettings->mUploadBufferTextureAlignment, blockSize);
	return round_up(alignment, util_get_texture_row_alignment(pRenderer));
}
/************************************************************************/
// Internal Functions
/************************************************************************/
/// Return a new staging buffer
static MappedMemoryRange allocateUploadMemory(Renderer* pRenderer, uint64_t memoryRequirement, uint32_t alignment)
{
#if defined(DIRECT3D11)
	// There is no such thing as staging buffer in D3D11
	// To keep code paths unified in update functions, we allocate space for a dummy buffer and the system memory for pCpuMappedAddress
	Buffer* buffer = (Buffer*)tf_memalign(alignof(Buffer), sizeof(Buffer) + (size_t)memoryRequirement);
	*buffer = {};
	buffer->pCpuMappedAddress = buffer + 1;
	buffer->mSize = memoryRequirement;
#else
	//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
	Buffer*    buffer = {};
	BufferDesc bufferDesc = {};
	bufferDesc.mSize = memoryRequirement;
	bufferDesc.mAlignment = alignment;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	addBuffer(pRenderer, &bufferDesc, &buffer);
#endif
	return { (uint8_t*)buffer->pCpuMappedAddress, buffer, 0, memoryRequirement };
}

static void setupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size, uint32_t bufferCount)
{
	QueueDesc desc = { QUEUE_TYPE_TRANSFER, QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, nodeIndex };
	addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

	const uint64_t maxBlockSize = 32;
	size = max(size, maxBlockSize);

	pCopyEngine->resourceSets = (CopyResourceSet*)tf_malloc(sizeof(CopyResourceSet)*bufferCount);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		tf_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
#ifndef DIRECT3D11
		addFence(pRenderer, &resourceSet.pFence);
#endif
		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = pCopyEngine->pQueue;
		addCmdPool(pRenderer, &cmdPoolDesc, &resourceSet.pCmdPool);

		CmdDesc cmdDesc = {};
		cmdDesc.pPool = resourceSet.pCmdPool;
		addCmd(pRenderer, &cmdDesc, &resourceSet.pCmd);

		resourceSet.mBuffer = allocateUploadMemory(pRenderer, size, util_get_texture_subresource_alignment(pRenderer)).pBuffer;
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
		removeCmdPool(pRenderer, resourceSet.pCmdPool);
#ifndef DIRECT3D11
		removeFence(pRenderer, resourceSet.pFence);
#endif
		if (!resourceSet.mTempBuffers.empty())
			LOGF(eINFO, "Was not cleaned up %d", i);
		for (Buffer*& buffer : resourceSet.mTempBuffers)
		{
			removeBuffer(pRenderer, buffer);
		}
		pCopyEngine->resourceSets[i].mTempBuffers.set_capacity(0);
	}

	tf_free(pCopyEngine->resourceSets);

	removeQueue(pRenderer, pCopyEngine->pQueue);
}

static bool waitCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, bool wait)
{
	ASSERT(!pCopyEngine->isRecording);
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	bool             completed = true;
#ifndef DIRECT3D11
	FenceStatus status;
	getFenceStatus(pRenderer, resourceSet.pFence, &status);
	completed = status != FENCE_STATUS_INCOMPLETE;
	if (wait && !completed)
	{
		waitForFences(pRenderer, 1, &resourceSet.pFence);
	}
#else
	UNREF_PARAM(pRenderer);
	UNREF_PARAM(pCopyEngine);
	UNREF_PARAM(activeSet);
#endif
	return completed;
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	pCopyEngine->resourceSets[activeSet].mAllocatedSpace = 0;
	pCopyEngine->isRecording = false;

	for (Buffer*& buffer : pCopyEngine->resourceSets[activeSet].mTempBuffers)
	{
		removeBuffer(pRenderer, buffer);
	}
	pCopyEngine->resourceSets[activeSet].mTempBuffers.clear();
}

static Cmd* acquireCmd(CopyEngine* pCopyEngine, size_t activeSet)
{
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	if (!pCopyEngine->isRecording)
	{
		resetCmdPool(pResourceLoader->pRenderer, resourceSet.pCmdPool);
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
#ifndef DIRECT3D11
		submitDesc.pSignalFence = resourceSet.pFence;
#endif
		{
#if defined(DIRECT3D11)
			MutexLock lock(gContextLock);
#endif
			queueSubmit(pCopyEngine->pQueue, &submitDesc);
		}
		pCopyEngine->isRecording = false;
	}
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(uint64_t memoryRequirement, uint32_t alignment)
{
	// Use the copy engine for GPU 0.
	CopyEngine* pCopyEngine = &pResourceLoader->pCopyEngines[0];

	uint64_t offset = pCopyEngine->resourceSets[pResourceLoader->mNextSet].mAllocatedSpace;
	if (alignment != 0)
	{
		offset = round_up_64(offset, alignment);
	}

	CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[pResourceLoader->mNextSet];
	uint64_t size = (uint64_t)pResourceSet->mBuffer->mSize;
	bool memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
	if (memoryAvailable && pResourceSet->mBuffer->pCpuMappedAddress)
	{
		Buffer* buffer = pResourceSet->mBuffer;
		ASSERT(buffer->pCpuMappedAddress);
		uint8_t* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
		pCopyEngine->resourceSets[pResourceLoader->mNextSet].mAllocatedSpace = offset + memoryRequirement;
		return { pDstData, buffer, offset, memoryRequirement };
	}
	else
	{
		if (pCopyEngine->bufferSize < memoryRequirement)
		{
			MappedMemoryRange range = allocateUploadMemory(pResourceLoader->pRenderer, memoryRequirement, alignment);
			//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
			pResourceSet->mTempBuffers.emplace_back(range.pBuffer);
			return range;
		}
	}

	MappedMemoryRange range = allocateUploadMemory(pResourceLoader->pRenderer, memoryRequirement, alignment);
	//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
	pResourceSet->mTempBuffers.emplace_back(range.pBuffer);
	return range;
}

static void freeAllUploadMemory()
{
	for (size_t i = 0; i < MAX_LINKED_GPUS; ++i)
	{
		for (UpdateRequest& request : pResourceLoader->mRequestQueue[i])
		{
			if (request.pUploadBuffer)
			{
				removeBuffer(pResourceLoader->pRenderer, request.pUploadBuffer);
			}
		}
	}
}

static UploadFunctionResult updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const TextureUpdateDescInternal& texUpdateDesc)
{
	// When this call comes from updateResource, staging buffer data is already filled
	// All that is left to do is record and execute the Copy commands
	bool dataAlreadyFilled = texUpdateDesc.mRange.pBuffer ? true : false;
	Texture* texture = texUpdateDesc.pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
	FileStream stream = texUpdateDesc.mStream;
	Cmd* cmd = acquireCmd(pCopyEngine, activeSet);

	ASSERT(pCopyEngine->pQueue->mNodeIndex == texUpdateDesc.pTexture->mNodeIndex);

	const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
	const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
	const uint64_t requiredSize = util_get_surface_size(fmt, texture->mWidth, texture->mHeight, texture->mDepth,
		rowAlignment,
		sliceAlignment,
		texUpdateDesc.mBaseMipLevel, texUpdateDesc.mMipLevels,
		texUpdateDesc.mBaseArrayLayer, texUpdateDesc.mLayerCount);

#if defined(VULKAN)
	TextureBarrier barrier = { texture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
#endif

	MappedMemoryRange upload = dataAlreadyFilled ? texUpdateDesc.mRange : allocateStagingMemory(requiredSize, sliceAlignment);
	uint64_t offset = 0;

	// #TODO: Investigate - fsRead crashes if we pass the upload buffer mapped address. Allocating temporary buffer as a workaround. Does NX support loading from disk to GPU shared memory?
#ifdef NX64
	void* nxTempBuffer = NULL;
	if (!dataAlreadyFilled)
	{
		size_t remainingBytes = fsGetStreamFileSize(&stream) - fsGetStreamSeekPosition(&stream);
		nxTempBuffer = tf_malloc(remainingBytes);
		ssize_t bytesRead = fsReadFromStream(&stream, nxTempBuffer, remainingBytes);
		if (bytesRead != remainingBytes)
		{
			fsCloseStream(&stream);
			tf_free(nxTempBuffer);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		fsCloseStream(&stream);
		fsOpenStreamFromMemory(nxTempBuffer, remainingBytes, FM_READ_BINARY, true, &stream);
	}
#endif

	if (!upload.pData)
	{
		return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
	}

	uint32_t firstStart = texUpdateDesc.mMipsAfterSlice ? texUpdateDesc.mBaseMipLevel : texUpdateDesc.mBaseArrayLayer;
	uint32_t firstEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels) : (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount);
	uint32_t secondStart = texUpdateDesc.mMipsAfterSlice ? texUpdateDesc.mBaseArrayLayer : texUpdateDesc.mBaseMipLevel;
	uint32_t secondEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount) : (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels);

	for (uint32_t p = 0; p < 1; ++p)
	{
		for (uint32_t j = firstStart; j < firstEnd; ++j)
		{
			if (texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMipFunc)
			{
				texUpdateDesc.pPreMipFunc(&stream, j);
			}

			for (uint32_t i = secondStart; i < secondEnd; ++i)
			{
				if (!texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMipFunc)
				{
					texUpdateDesc.pPreMipFunc(&stream, i);
				}

				uint32_t mip = texUpdateDesc.mMipsAfterSlice ? j : i;
				uint32_t layer = texUpdateDesc.mMipsAfterSlice ? i : j;

				uint32_t w = MIP_REDUCE(texture->mWidth, mip);
				uint32_t h = MIP_REDUCE(texture->mHeight, mip);
				uint32_t d = MIP_REDUCE(texture->mDepth, mip);

				uint32_t numBytes = 0;
				uint32_t rowBytes = 0;
				uint32_t numRows = 0;

				bool ret = util_get_surface_info(w, h, fmt, &numBytes, &rowBytes, &numRows);
				if (!ret)
				{
					return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
				}

				uint32_t subRowPitch = round_up(rowBytes, rowAlignment);
				uint32_t subSlicePitch = round_up(subRowPitch * numRows, sliceAlignment);
				uint32_t subNumRows = numRows;
				uint32_t subDepth = d;
				uint32_t subRowSize = rowBytes;
				uint8_t* data = upload.pData + offset;

				if (!dataAlreadyFilled)
				{
					for (uint32_t z = 0; z < subDepth; ++z)
					{
						uint8_t* dstData = data + subSlicePitch * z;
						for (uint32_t r = 0; r < subNumRows; ++r)
						{
							ssize_t bytesRead = fsReadFromStream(&stream, dstData + r * subRowPitch, subRowSize);
							if (bytesRead != subRowSize)
							{
								return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
							}
						}
					}
				}
				SubresourceDataDesc subresourceDesc = {};
				subresourceDesc.mArrayLayer = layer;
				subresourceDesc.mMipLevel = mip;
				subresourceDesc.mSrcOffset = upload.mOffset + offset;
#if defined(DIRECT3D11) || defined(METAL) || defined(VULKAN)
				subresourceDesc.mRowPitch = subRowPitch;
				subresourceDesc.mSlicePitch = subSlicePitch;
#endif
				cmdUpdateSubresource(cmd, texture, upload.pBuffer, &subresourceDesc);
				offset += subDepth * subSlicePitch;
			}
		}
	}

#if defined(VULKAN)
	barrier = { texture, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
#endif

	if (stream.pIO)
	{
		fsCloseStream(&stream);
	}

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const UpdateRequest& pTextureUpdate)
{
	const TextureLoadDesc* pTextureDesc = &pTextureUpdate.texLoadDesc;

	if (pTextureDesc->pFileName)
	{
		FileStream stream = {};
		char fileName[FS_MAX_PATH] = {};
		bool success = false;

		TextureUpdateDescInternal updateDesc = {};
		TextureContainerType container = pTextureDesc->mContainer;
		static const char* extensions[] = { NULL, "dds", "ktx", "gnf", "basis", "svt" };

		if (TEXTURE_CONTAINER_DEFAULT == container)
		{
#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
			container = TEXTURE_CONTAINER_KTX;
#elif defined(_WINDOWS) || defined(XBOX) || defined(__APPLE__) || defined(__linux__)
			container = TEXTURE_CONTAINER_DDS;
#elif defined(ORBIS) || defined(PROSPERO)
			container = TEXTURE_CONTAINER_GNF;
#endif
		}

		TextureDesc textureDesc = {};
		textureDesc.pName = pTextureDesc->pFileName;

		// Validate that we have found the file format now
		ASSERT(container != TEXTURE_CONTAINER_DEFAULT);
		if (TEXTURE_CONTAINER_DEFAULT == container)
		{
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		fsAppendPathExtension(pTextureDesc->pFileName, extensions[container], fileName);

		switch (container)
		{
		case TEXTURE_CONTAINER_DDS:
		{
#if defined(XBOX)
			success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream);
			uint32_t res = 1;
			if (success)
			{
				extern uint32_t loadXDDSTexture(Renderer* pRenderer, FileStream* stream, const char* name, TextureCreationFlags flags, Texture** ppTexture);
				res = loadXDDSTexture(pRenderer, &stream, fileName, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
				fsCloseStream(&stream);
			}

			return res ? UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : UPLOAD_FUNCTION_RESULT_COMPLETED;
#else
			success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream);
			if (success)
			{
				success = loadDDSTextureDesc(&stream, &textureDesc);
			}
#endif
			break;
		}
		case TEXTURE_CONTAINER_KTX:
		{
			success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream);
			if (success)
			{
				success = loadKTXTextureDesc(&stream, &textureDesc);
				updateDesc.mMipsAfterSlice = true;
				// KTX stores mip size before the mip data
				// This function gets called to skip the mip size so we read the mip data
				updateDesc.pPreMipFunc = [](FileStream* pStream, uint32_t)
				{
					uint32_t mipSize = 0;
					fsReadFromStream(pStream, &mipSize, sizeof(mipSize));
				};
			}
			break;
		}
		case TEXTURE_CONTAINER_BASIS:
		{
			void* data = NULL;
			uint32_t dataSize = 0;
			success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream);
			if (success)
			{
				success = loadBASISTextureDesc(&stream, &textureDesc, &data, &dataSize);
				if (success)
				{
					fsCloseStream(&stream);
					fsOpenStreamFromMemory(data, dataSize, FM_READ_BINARY, true, &stream);
				}
			}
			break;
		}
		case TEXTURE_CONTAINER_GNF:
		{
#if defined(ORBIS) || defined(PROSPERO)
			success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream);
			uint32_t res = 1;
			if (success)
			{
				extern uint32_t loadGnfTexture(Renderer* pRenderer, FileStream* stream, const char* name, TextureCreationFlags flags, Texture** ppTexture);
				res = loadGnfTexture(pRenderer, &stream, fileName, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
				fsCloseStream(&stream);
			}

			return res ? UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : UPLOAD_FUNCTION_RESULT_COMPLETED;
#endif
		}
		default:
			break;
		}

		if (success)
		{
			textureDesc.mStartState = RESOURCE_STATE_COMMON;
			textureDesc.mFlags |= pTextureDesc->mCreationFlag;
			textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;
			addTexture(pRenderer, &textureDesc, pTextureDesc->ppTexture);

			updateDesc.mStream = stream;
			updateDesc.pTexture = *pTextureDesc->ppTexture;
			updateDesc.mBaseMipLevel = 0;
			updateDesc.mMipLevels = textureDesc.mMipLevels;
			updateDesc.mBaseArrayLayer = 0;
			updateDesc.mLayerCount = textureDesc.mArraySize;

			return updateTexture(pRenderer, pCopyEngine, activeSet, updateDesc);
		}
		/************************************************************************/
		// Sparse Tetxtures
		/************************************************************************/
#if defined(DIRECT3D12) || defined(VULKAN)
		if (TEXTURE_CONTAINER_SVT == container)
		{
			if (fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, &stream))
			{
				success = loadSVTTextureDesc(&stream, &textureDesc);
				if (success)
				{
					ssize_t dataSize = fsGetStreamFileSize(&stream) - fsGetStreamSeekPosition(&stream);
					void* data = tf_malloc(dataSize);
					fsReadFromStream(&stream, data, dataSize);

					textureDesc.mStartState = RESOURCE_STATE_COPY_DEST;
					textureDesc.mFlags |= pTextureDesc->mCreationFlag;
					textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;
					addVirtualTexture(acquireCmd(pCopyEngine, activeSet), &textureDesc, pTextureDesc->ppTexture, data);
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
					visDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
					visDesc.mDesc.pName = "Vis Buffer for Sparse Texture";
					visDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mVisibility;
					addResource(&visDesc, NULL);

					BufferLoadDesc prevVisDesc = {};
					prevVisDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
					prevVisDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					prevVisDesc.mDesc.mStructStride = sizeof(uint);
					prevVisDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
					prevVisDesc.mDesc.mSize = prevVisDesc.mDesc.mStructStride * prevVisDesc.mDesc.mElementCount;
					prevVisDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
					prevVisDesc.mDesc.pName = "Prev Vis Buffer for Sparse Texture";
					prevVisDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mPrevVisibility;
					addResource(&prevVisDesc, NULL);

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
					alivePageDesc.mDesc.pName = "Alive pages buffer for Sparse Texture";
					alivePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mAlivePage;
					addResource(&alivePageDesc, NULL);

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
					removePageDesc.mDesc.pName = "Remove pages buffer for Sparse Texture";
					removePageDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mRemovePage;
					addResource(&removePageDesc, NULL);

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
					pageCountsDesc.mDesc.pName = "Page count buffer for Sparse Texture";
					pageCountsDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mPageCounts;
					addResource(&pageCountsDesc, NULL);

					fsCloseStream(&stream);

					return UPLOAD_FUNCTION_RESULT_COMPLETED;
				}
			}

			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
#endif
	}

	return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}

static UploadFunctionResult updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const BufferUpdateDesc& bufUpdateDesc)
{
	ASSERT(pCopyEngine->pQueue->mNodeIndex == bufUpdateDesc.pBuffer->mNodeIndex);
	Buffer* pBuffer = bufUpdateDesc.pBuffer;
	ASSERT(pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

	Cmd* pCmd = acquireCmd(pCopyEngine, activeSet);

	MappedMemoryRange range = bufUpdateDesc.mInternal.mMappedRange;
	cmdUpdateBuffer(pCmd, pBuffer, bufUpdateDesc.mDstOffset, range.pBuffer, range.mOffset, range.mSize);

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadGeometry(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateRequest& pGeometryLoad)
{
	GeometryLoadDesc* pDesc = &pGeometryLoad.geomLoadDesc;

	char iext[FS_MAX_PATH] = { 0 };
	fsGetPathExtension(pDesc->pFileName, iext);

	// Geometry in gltf container
	if (iext[0] != 0 && (stricmp(iext, "gltf") == 0 || stricmp(iext, "glb") == 0))
	{
		FileStream file = {};
		if (!fsOpenStreamFromPath(RD_MESHES, pDesc->pFileName, FM_READ_BINARY, &file))
		{
			LOGF(eERROR, "Failed to open gltf file %s", pDesc->pFileName);
			ASSERT(false);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		ssize_t fileSize = fsGetStreamFileSize(&file);
		void* fileData = tf_malloc(fileSize);
		cgltf_result result = cgltf_result_invalid_gltf;

		fsReadFromStream(&file, fileData, fileSize);

		cgltf_options options = {};
		cgltf_data* data = NULL;
		options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
		options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
		result = cgltf_parse(&options, fileData, fileSize, &data);
		fsCloseStream(&file);

		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to parse gltf file %s with error %u", pDesc->pFileName, (uint32_t)result);
			ASSERT(false);
			tf_free(fileData);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

#if defined(FORGE_DEBUG)
		result = cgltf_validate(data);
		if (cgltf_result_success != result)
		{
			LOGF(eWARNING, "GLTF validation finished with error %u for file %s", (uint32_t)result, pDesc->pFileName);
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
				char parent[FS_MAX_PATH] = { 0 };
				fsGetParentPath(pDesc->pFileName, parent);
				char path[FS_MAX_PATH] = { 0 };
				fsAppendPathComponent(parent, uri, path);
				FileStream fs = {};
				if (fsOpenStreamFromPath(RD_MESHES, path, FM_READ_BINARY, &fs))
				{
					ASSERT(fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[i].size);
					data->buffers[i].data = tf_malloc(data->buffers[i].size);
					fsReadFromStream(&fs, data->buffers[i].data, data->buffers[i].size);
				}
				fsCloseStream(&fs);
			}
		}

		result = cgltf_load_buffers(&options, data, pDesc->pFileName);
		if (cgltf_result_success != result)
		{
			LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", pDesc->pFileName, (uint32_t)result);
			ASSERT(false);
			tf_free(fileData);
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
					vertexAttribs[util_cgltf_attrib_type_to_semantic(prim->attributes[i].type, prim->attributes[i].index)] = &prim->attributes[i];
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
			const TinyImageFormat srcFormat = util_cgltf_type_to_image_format(cgltfAttr->data->type, cgltfAttr->data->component_type);
			const TinyImageFormat dstFormat = attr->mFormat == TinyImageFormat_UNDEFINED ? srcFormat : attr->mFormat;

			if (dstFormat != srcFormat)
			{
				// Select appropriate packing function which will be used when filling the vertex buffer
				switch (cgltfAttr->type)
				{
				case cgltf_attribute_type_texcoord:
				{
					if (sizeof(uint32_t) == dstFormatSize && sizeof(float[2]) == srcFormatSize)
						vertexPacking[attr->mSemantic] = util_pack_float2_to_half2;
					// #TODO: Add more variations if needed
					break;
				}
				case cgltf_attribute_type_normal:
				case cgltf_attribute_type_tangent:
				{
					if (sizeof(uint32_t) == dstFormatSize && (sizeof(float[3]) == srcFormatSize || sizeof(float[4]) == srcFormatSize))
						vertexPacking[attr->mSemantic] = util_pack_float3_direction_to_half2;
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

		Geometry* geom = (Geometry*)tf_calloc(1, totalSize);
		ASSERT(geom);

		geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);
		geom->pInverseBindPoses = (mat4*)((uint8_t*)geom->pDrawArgs + round_up(drawCount * sizeof(*geom->pDrawArgs), 16));
		geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(jointCount * sizeof(*geom->pInverseBindPoses), 16));

		uint32_t shadowSize = 0;
		if (pDesc->mFlags & GEOMETRY_LOAD_FLAG_SHADOWED)
		{
			shadowSize += (uint32_t)vertexAttribs[SEMANTIC_POSITION]->data->stride * vertexCount;
			shadowSize += indexCount * indexStride;

			geom->pShadow = (Geometry::ShadowData*)tf_calloc(1, sizeof(Geometry::ShadowData) + shadowSize);
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
		indexUpdateDesc.mInternal.mMappedRange = { (uint8_t*)geom->pIndexBuffer->pCpuMappedAddress };
#else
		indexUpdateDesc.mInternal.mMappedRange = allocateStagingMemory(indexUpdateDesc.mSize, RESOURCE_BUFFER_ALIGNMENT);
#endif
		indexUpdateDesc.pMappedData = indexUpdateDesc.mInternal.mMappedRange.pData;

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
			vertexUpdateDesc[i].mInternal.mMappedRange = { (uint8_t*)geom->pVertexBuffers[bufferCounter]->pCpuMappedAddress, 0 };
#else
			vertexUpdateDesc[i].mInternal.mMappedRange = allocateStagingMemory(vertexUpdateDesc[i].mSize, RESOURCE_BUFFER_ALIGNMENT);
#endif
			vertexUpdateDesc[i].pMappedData = vertexUpdateDesc[i].mInternal.mMappedRange.pData;
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
					uint32_t index = util_cgltf_attrib_type_to_semantic(attr->type, attr->index);

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
		uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, indexUpdateDesc);

		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (vertexUpdateDesc[i].pMappedData)
			{
				uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, vertexUpdateDesc[i]);
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
				jsmntok_t* tokens = (jsmntok_t*)tf_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
				jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
				ASSERT(tokens[0].size == skin->joints_count + 1);
				cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses, skin->joints_count * sizeof(float[16]) / sizeof(float));
				for (uint32_t r = 0; r < skin->joints_count; ++r)
					geom->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
				tf_free(tokens);
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

		tf_free(pDesc->pVertexLayout);

		*pDesc->ppGeometry = geom;

		return uploadResult;
	}

	return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}
/************************************************************************/
// Internal Resource Loader Implementation
/************************************************************************/
static bool areTasksAvailable(ResourceLoader* pLoader)
{
	for (size_t i = 0; i < MAX_LINKED_GPUS; ++i)
	{
		if (!pLoader->mRequestQueue[i].empty())
		{
			return true;
		}
	}

	return false;
}

static void streamerThreadFunc(void* pThreadData)
{
	ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
	ASSERT(pLoader);

	uint32_t linkedGPUCount = pLoader->pRenderer->mLinkedNodeCount;

	SyncToken      maxToken = {};

	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();

		// Check for pending tokens
		// Safe to use mTokenCounter as we are inside critical section
		bool allTokensSignaled = (pLoader->mTokenCompleted == tfrg_atomic64_load_relaxed(&pLoader->mTokenCounter));

		while (!areTasksAvailable(pLoader) && allTokensSignaled && pLoader->mRun)
		{
			// Sleep until someone adds an update request to the queue
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex);
		}

		pLoader->mQueueMutex.Release();

		pLoader->mNextSet = (pLoader->mNextSet + 1) % pLoader->mDesc.mBufferCount;
		for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
		{
			waitCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet, true);
			resetCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
		}

		// Signal pending tokens from previous frames
		pLoader->mTokenMutex.Acquire();
		tfrg_atomic64_store_release(&pLoader->mTokenCompleted, pLoader->mCurrentTokenState[pLoader->mNextSet]);
		pLoader->mTokenMutex.Release();
		pLoader->mTokenCond.WakeAll();

		for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
		{
			uint64_t completionMask = 0;

			pLoader->mQueueMutex.Acquire();

			eastl::vector<UpdateRequest>& requestQueue = pLoader->mRequestQueue[nodeIndex];
			CopyEngine& copyEngine = pLoader->pCopyEngines[nodeIndex];

			if (!requestQueue.size())
			{
				pLoader->mQueueMutex.Release();
				continue;
			}

			eastl::vector<UpdateRequest> activeQueue;
			eastl::swap(requestQueue, activeQueue);
			pLoader->mQueueMutex.Release();

			size_t requestCount = activeQueue.size();

			for (size_t j = 0; j < requestCount; ++j)
			{
				UpdateRequest updateState = activeQueue[j];

				UploadFunctionResult result = UPLOAD_FUNCTION_RESULT_COMPLETED;
				switch (updateState.mType)
				{
				case UPDATE_REQUEST_UPDATE_BUFFER:
					result = updateBuffer(pLoader->pRenderer, &copyEngine, pLoader->mNextSet, updateState.bufUpdateDesc);
					break;
				case UPDATE_REQUEST_UPDATE_TEXTURE:
					result = updateTexture(pLoader->pRenderer, &copyEngine, pLoader->mNextSet, updateState.texUpdateDesc);
					break;
				case UPDATE_REQUEST_BUFFER_BARRIER:
					cmdResourceBarrier(acquireCmd(&copyEngine, pLoader->mNextSet), 1, &updateState.bufferBarrier, 0, NULL, 0, NULL);
					result = UPLOAD_FUNCTION_RESULT_COMPLETED;
					break;
				case UPDATE_REQUEST_TEXTURE_BARRIER:
					cmdResourceBarrier(acquireCmd(&copyEngine, pLoader->mNextSet), 0, NULL, 1, &updateState.textureBarrier, 0, NULL);
					result = UPLOAD_FUNCTION_RESULT_COMPLETED;
					break;
				case UPDATE_REQUEST_LOAD_TEXTURE:
					result = loadTexture(pLoader->pRenderer, &copyEngine, pLoader->mNextSet, updateState);
					break;
				case UPDATE_REQUEST_LOAD_GEOMETRY:
					result = loadGeometry(pLoader->pRenderer, &copyEngine, pLoader->mNextSet, updateState);
					break;
				case UPDATE_REQUEST_INVALID:
					break;
				}

				if (updateState.pUploadBuffer)
				{
					CopyResourceSet& resourceSet = copyEngine.resourceSets[pLoader->mNextSet];
					resourceSet.mTempBuffers.push_back(updateState.pUploadBuffer);
				}

				bool completed = result == UPLOAD_FUNCTION_RESULT_COMPLETED || result == UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

				completionMask |= completed << nodeIndex;

				if (updateState.mWaitIndex && completed)
				{
					ASSERT(maxToken < updateState.mWaitIndex);
					maxToken = updateState.mWaitIndex;
				}

				ASSERT(result != UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL);
			}

			if (completionMask != 0)
			{
				for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
				{
					streamerFlush(&pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
				}
			}
		}

		SyncToken nextToken = max(maxToken, getLastTokenCompleted());
		pLoader->mCurrentTokenState[pLoader->mNextSet] = nextToken;
	}

	for (uint32_t nodeIndex = 0; nodeIndex < linkedGPUCount; ++nodeIndex)
	{
		streamerFlush(&pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
#ifndef DIRECT3D11
		waitQueueIdle(pLoader->pCopyEngines[nodeIndex].pQueue);
#endif
		cleanupCopyEngine(pLoader->pRenderer, &pLoader->pCopyEngines[nodeIndex]);
	}

	freeAllUploadMemory();
}

static void addResourceLoader(Renderer* pRenderer, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
{
	ResourceLoader* pLoader = tf_new(ResourceLoader);
	pLoader->pRenderer = pRenderer;

	pLoader->mRun = true;
	pLoader->mDesc = pDesc ? *pDesc : gDefaultResourceLoaderDesc;

	pLoader->mQueueMutex.Init();
	pLoader->mTokenMutex.Init();
	pLoader->mQueueCond.Init();
	pLoader->mTokenCond.Init();

	pLoader->mTokenCounter = 0;
	pLoader->mTokenCompleted = 0;

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
	pLoader->mThreadDesc.pThreadName = "ResourceLoaderTask";
	pLoader->mThreadDesc.preferredCore = 1;
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

	tf_delete(pLoader);
}

static void queueBufferUpdate(ResourceLoader* pLoader, BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	uint32_t nodeIndex = pBufferUpdate->pBuffer->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pBufferUpdate));
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mRequestQueue[nodeIndex].back().pUploadBuffer =
		(pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pBufferUpdate->mInternal.mMappedRange.pBuffer
																					   : NULL;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueTextureLoad(ResourceLoader* pLoader, TextureLoadDesc* pTextureUpdate, SyncToken* token)
{
	uint32_t nodeIndex = pTextureUpdate->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueGeometryLoad(ResourceLoader* pLoader, GeometryLoadDesc* pGeometryLoad, SyncToken* token)
{
	uint32_t nodeIndex = pGeometryLoad->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pGeometryLoad));
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueTextureUpdate(ResourceLoader* pLoader, TextureUpdateDescInternal* pTextureUpdate, SyncToken* token)
{
	ASSERT(pTextureUpdate->mRange.pBuffer);

	uint32_t nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mRequestQueue[nodeIndex].back().pUploadBuffer =
		(pTextureUpdate->mRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pTextureUpdate->mRange.pBuffer : NULL;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueBufferBarrier(ResourceLoader* pLoader, Buffer* pBuffer, ResourceState state, SyncToken* token)
{
	uint32_t nodeIndex = pBuffer->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest{ BufferBarrier{ pBuffer, RESOURCE_STATE_UNDEFINED, state } });
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueTextureBarrier(ResourceLoader* pLoader, Texture* pTexture, ResourceState state, SyncToken* token)
{
	uint32_t nodeIndex = pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	pLoader->mRequestQueue[nodeIndex].emplace_back(UpdateRequest{ TextureBarrier{ pTexture, RESOURCE_STATE_UNDEFINED, state } });
	pLoader->mRequestQueue[nodeIndex].back().mWaitIndex = t;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
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
/************************************************************************/
// Resource Loader Interfae Implementation
/************************************************************************/
void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc)
{
#ifdef DIRECT3D11
	gContextLock.Init();
#endif
	addResourceLoader(pRenderer, pDesc, &pResourceLoader);
}

void exitResourceLoaderInterface(Renderer* pRenderer)
{
	removeResourceLoader(pResourceLoader);
#ifdef DIRECT3D11
	gContextLock.Destroy();
#endif
}

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token)
{
	uint64_t stagingBufferSize = pResourceLoader->pCopyEngines[0].bufferSize;
	bool update = pBufferDesc->pData || pBufferDesc->mForceReset;

	ASSERT(stagingBufferSize > 0);
	if (RESOURCE_MEMORY_USAGE_GPU_ONLY == pBufferDesc->mDesc.mMemoryUsage && !pBufferDesc->mDesc.mStartState && !update)
	{
		pBufferDesc->mDesc.mStartState = util_determine_resource_start_state(&pBufferDesc->mDesc);
		LOGF(eWARNING, "Buffer start state not provided. Determined the start state as (%u) based on the provided BufferDesc",
			(uint32_t)pBufferDesc->mDesc.mStartState);
	}

	if (pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY && update)
	{
		pBufferDesc->mDesc.mStartState = RESOURCE_STATE_COMMON;
	}
	addBuffer(pResourceLoader->pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

	if (update)
	{
		if (pBufferDesc->mDesc.mSize > stagingBufferSize &&
			pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
		{
			// The data is too large for a single staging buffer copy, so perform it in stages.

			// Save the data parameter so we can restore it later.
			const void* data = pBufferDesc->pData;

			BufferUpdateDesc updateDesc = {};
			updateDesc.pBuffer = *pBufferDesc->ppBuffer;
			for (uint64_t offset = 0; offset < pBufferDesc->mDesc.mSize; offset += stagingBufferSize)
			{
				size_t chunkSize = (size_t)min(stagingBufferSize, pBufferDesc->mDesc.mSize - offset);
				updateDesc.mSize = chunkSize;
				updateDesc.mDstOffset = offset;
				beginUpdateResource(&updateDesc);
				if (pBufferDesc->mForceReset)
				{
					memset(updateDesc.pMappedData, 0, chunkSize);
				}
				else
				{
					memcpy(updateDesc.pMappedData, (char*)data + offset, chunkSize);
				}
				endUpdateResource(&updateDesc, token);
			}
		}
		else
		{
			BufferUpdateDesc updateDesc = {};
			updateDesc.pBuffer = *pBufferDesc->ppBuffer;
			beginUpdateResource(&updateDesc);
			if (pBufferDesc->mForceReset)
			{
				memset(updateDesc.pMappedData, 0, (size_t)pBufferDesc->mDesc.mSize);
			}
			else
			{
				memcpy(updateDesc.pMappedData, pBufferDesc->pData, (size_t)pBufferDesc->mDesc.mSize);
			}
			endUpdateResource(&updateDesc, token);
		}
	}
	else
	{
		// Transition GPU buffer to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mApi == RENDERER_API_VULKAN &&
			pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pBufferDesc->mDesc.mStartState != RESOURCE_STATE_UNDEFINED && pBufferDesc->mDesc.mStartState != RESOURCE_STATE_COMMON))
			queueBufferBarrier(pResourceLoader, *pBufferDesc->ppBuffer, pBufferDesc->mDesc.mStartState, token);
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token)
{
	ASSERT(pTextureDesc->ppTexture);

	if (!pTextureDesc->pFileName && pTextureDesc->pDesc)
	{
		ASSERT(pTextureDesc->pDesc->mStartState);

		// If texture is supposed to be filled later (UAV / Update later / ...) proceed with the mStartState provided by the user in the texture description
		addTexture(pResourceLoader->pRenderer, pTextureDesc->pDesc, pTextureDesc->ppTexture);

		// Transition texture to desired state for Vulkan since all Vulkan resources are created in undefined state
		if (pResourceLoader->pRenderer->mApi == RENDERER_API_VULKAN)
		{
			ResourceState startState = pTextureDesc->pDesc->mStartState;
			// Check whether this is required (user specified a state other than undefined / common)
			if (startState == RESOURCE_STATE_UNDEFINED || startState == RESOURCE_STATE_COMMON)
			{
				startState = util_determine_resource_start_state(pTextureDesc->pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE);
			}
			queueTextureBarrier(pResourceLoader, *pTextureDesc->ppTexture, startState, token);
		}
	}
	else
	{
		TextureLoadDesc updateDesc = *pTextureDesc;
		queueTextureLoad(pResourceLoader, &updateDesc, token);
	}
}

void addResource(GeometryLoadDesc* pDesc, SyncToken* token)
{
	ASSERT(pDesc->ppGeometry);

	GeometryLoadDesc updateDesc = *pDesc;
	updateDesc.pFileName = pDesc->pFileName;
	updateDesc.pVertexLayout = (VertexLayout*)tf_calloc(1, sizeof(VertexLayout));
	memcpy(updateDesc.pVertexLayout, pDesc->pVertexLayout, sizeof(VertexLayout));
	queueGeometryLoad(pResourceLoader, &updateDesc, token);
}

void removeResource(Buffer* pBuffer)
{
	removeBuffer(pResourceLoader->pRenderer, pBuffer);
}

void removeResource(Texture* pTexture)
{
	removeTexture(pResourceLoader->pRenderer, pTexture);
}

void removeResource(Geometry* pGeom)
{
	removeResource(pGeom->pIndexBuffer);

	for (uint32_t i = 0; i < pGeom->mVertexBufferCount; ++i)
		removeResource(pGeom->pVertexBuffers[i]);

	tf_free(pGeom);
}

void beginUpdateResource(BufferUpdateDesc* pBufferUpdate)
{
	Buffer* pBuffer = pBufferUpdate->pBuffer;
	ASSERT(pBuffer);

	uint64_t size = pBufferUpdate->mSize > 0 ? pBufferUpdate->mSize : (pBufferUpdate->pBuffer->mSize - pBufferUpdate->mDstOffset);
	ASSERT(pBufferUpdate->mDstOffset + size <= pBuffer->mSize);

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (UMA || memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		bool map = !pBuffer->pCpuMappedAddress;
		if (map)
		{
			mapBuffer(pResourceLoader->pRenderer, pBuffer, NULL);
		}

		pBufferUpdate->mInternal.mMappedRange = { (uint8_t*)pBuffer->pCpuMappedAddress + pBufferUpdate->mDstOffset, pBuffer };
		pBufferUpdate->pMappedData = pBufferUpdate->mInternal.mMappedRange.pData;
		pBufferUpdate->mInternal.mMappedRange.mFlags = map ? MAPPED_RANGE_FLAG_UNMAP_BUFFER : 0;
	}
	else
	{
		// We need to use a staging buffer.
		MappedMemoryRange range = allocateUploadMemory(pResourceLoader->pRenderer, size, RESOURCE_BUFFER_ALIGNMENT);
		pBufferUpdate->pMappedData = range.pData;

		pBufferUpdate->mInternal.mMappedRange = range;
		pBufferUpdate->mInternal.mMappedRange.mFlags = MAPPED_RANGE_FLAG_TEMP_BUFFER;
	}
}

void endUpdateResource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	if (pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_UNMAP_BUFFER)
	{
		unmapBuffer(pResourceLoader->pRenderer, pBufferUpdate->pBuffer);
	}

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (!UMA && memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		queueBufferUpdate(pResourceLoader, pBufferUpdate, token);
	}

	// Restore the state to before the beginUpdateResource call.
	pBufferUpdate->pMappedData = NULL;
	pBufferUpdate->mInternal = {};
}

void beginUpdateResource(TextureUpdateDesc* pTextureUpdate)
{
	const Texture* texture = pTextureUpdate->pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
	const uint32_t alignment = util_get_texture_subresource_alignment(pResourceLoader->pRenderer, fmt);

	bool success = util_get_surface_info(
		MIP_REDUCE(texture->mWidth, pTextureUpdate->mMipLevel),
		MIP_REDUCE(texture->mHeight, pTextureUpdate->mMipLevel),
		fmt,
		&pTextureUpdate->mSrcSliceStride,
		&pTextureUpdate->mSrcRowStride,
		&pTextureUpdate->mRowCount);
	ASSERT(success);
	UNREF_PARAM(success);

	pTextureUpdate->mDstRowStride = round_up(pTextureUpdate->mSrcRowStride, util_get_texture_row_alignment(pResourceLoader->pRenderer));
	pTextureUpdate->mDstSliceStride = round_up(pTextureUpdate->mDstRowStride * pTextureUpdate->mRowCount, alignment);

	const ssize_t requiredSize = round_up(
		MIP_REDUCE(texture->mDepth, pTextureUpdate->mMipLevel) * pTextureUpdate->mDstRowStride * pTextureUpdate->mRowCount,
		alignment);

	// We need to use a staging buffer.
	pTextureUpdate->mInternal.mMappedRange = allocateUploadMemory(pResourceLoader->pRenderer, requiredSize, alignment);
	pTextureUpdate->mInternal.mMappedRange.mFlags = MAPPED_RANGE_FLAG_TEMP_BUFFER;
	pTextureUpdate->pMappedData = pTextureUpdate->mInternal.mMappedRange.pData;
}

void endUpdateResource(TextureUpdateDesc* pTextureUpdate, SyncToken* token)
{
	TextureUpdateDescInternal desc = {};
	desc.pTexture = pTextureUpdate->pTexture;
	desc.mRange = pTextureUpdate->mInternal.mMappedRange;
	desc.mBaseMipLevel = pTextureUpdate->mMipLevel;
	desc.mMipLevels = 1;
	desc.mBaseArrayLayer = pTextureUpdate->mArrayLayer;
	desc.mLayerCount = 1;
	queueTextureUpdate(pResourceLoader, &desc, token);

	// Restore the state to before the beginUpdateResource call.
	pTextureUpdate->pMappedData = NULL;
	pTextureUpdate->mInternal = {};
}

SyncToken getLastTokenCompleted()
{
	return tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted);
}

bool isTokenCompleted(const SyncToken* token)
{
	return *token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted);
}

void waitForToken(const SyncToken* token)
{
	waitForToken(pResourceLoader, token);
}

bool allResourceLoadsCompleted()
{
	SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
	return token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted);
}

void waitForAllResourceLoads()
{
	SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
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
	default: break;
	}

	ASSERT(false);
	return static_cast<shaderc_shader_kind>(-1);
}
#endif

#if defined(VULKAN)
#if defined(__ANDROID__)
// Android:
// Use shaderc to compile glsl to spirV
void vk_compileShader(
	Renderer* pRenderer, ShaderStage stage, uint32_t codeSize, const char* code, const char* outFile, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	// compile into spir-V shader
	shaderc_compiler_t           compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t	 options = shaderc_compile_options_initialize();
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderc_compile_options_add_macro_definition(options, pMacros[i].definition, strlen(pMacros[i].definition),
			pMacros[i].value, strlen(pMacros[i].value));
	}

	eastl::string android_definition = "TARGET_ANDROID";
	shaderc_compile_options_add_macro_definition(options, android_definition.c_str(), android_definition.size(), "1", 1);

	shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

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
	pOut->mByteCodeSize = shaderc_result_get_length(spvShader);
	pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
	memcpy(pOut->pByteCode, shaderc_result_get_bytes(spvShader), pOut->mByteCodeSize);

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
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, const char* outFile, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	eastl::string commandLine;
	char filePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, filePath);
	char outFilePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), outFile, outFilePath);
	
	// If there is a config file located in the shader source directory use it to specify the limits
	FileStream confStream = {};
	if (fsOpenStreamFromPath(RD_SHADER_SOURCES, "config.conf", FM_READ, &confStream))
	{
		fsCloseStream(&confStream);
		char configFilePath[FS_MAX_PATH] = { 0 };
		fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), "config.conf", configFilePath);

		// Add command to compile from Vulkan GLSL to Spirv
		commandLine.append_sprintf(
			"\"%s\" -V \"%s\" -o \"%s\"",
			configFilePath,
			filePath,
			outFilePath);
	}
	else
	{
		commandLine.append_sprintf("-V \"%s\" -o \"%s\"",
			filePath,
			outFilePath);
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

	char logFileName[FS_MAX_PATH] = { 0 };
	fsGetPathFileName(outFile, logFileName);
	strcat(logFileName, "_compile.log");
	char logFilePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), logFileName, logFilePath);

	if (systemRun(glslangValidator.c_str(), args, 1, logFilePath) == 0)
	{
		FileStream fh = {};
		bool success = fsOpenStreamFromPath(RD_SHADER_BINARIES, outFile, FM_READ_BINARY, &fh);
		//Check if the File Handle exists
		ASSERT(success);
		pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&fh);
		pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
		fsReadFromStream(&fh, pOut->pByteCode, pOut->mByteCodeSize);
		fsCloseStream(&fh);
	}
	else
	{
		FileStream fh = {};
		// If for some reason the error file could not be created just log error msg
		if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, logFileName, FM_READ_BINARY, &fh))
		{
			LOGF(LogLevel::eERROR, "Failed to compile shader %s", filePath);
		}
		else
		{
			size_t size = fsGetStreamFileSize(&fh);
			if (size)
			{
				char* errorLog = (char*)tf_malloc(size + 1);
				errorLog[size] = 0;
				fsReadFromStream(&fh, errorLog, size);
				LOGF(LogLevel::eERROR, "Failed to compile shader %s with error\n%s", filePath, errorLog);
			}
			fsCloseStream(&fh);
		}
	}
}
#endif
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(
	Renderer* pRenderer, const char* fileName, const char* outFile, uint32_t macroCount, ShaderMacro* pMacros,
	BinaryShaderStageDesc* pOut, const char* /*pEntryPoint*/)
{
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, filePath);
	char outFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), outFile, outFilePath);
	char intermediateFilePath[FS_MAX_PATH] = {};
	fsAppendPathExtension(outFilePath, "air", intermediateFilePath);

	const char *xcrun = "/usr/bin/xcrun";
	eastl::vector<eastl::string> args;
	eastl::string tmpArg = "";

	// Compile the source into a temporary .air file.
	args.push_back("-sdk");
	args.push_back("macosx");
	args.push_back("metal");
	args.push_back("-c");
	args.push_back(filePath);
	args.push_back("-o");
	args.push_back(intermediateFilePath);

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
		args.push_back(intermediateFilePath);
		args.push_back("-o");
		tmpArg = eastl::string().sprintf(
			""
			"%s"
			"",
			outFilePath);
		args.push_back(tmpArg);

		cArgs.clear();
		for (eastl::string& arg : args) {
			cArgs.push_back(arg.c_str());
		}

		if (systemRun(xcrun, &cArgs[0], cArgs.size(), NULL) == 0)
		{
			// Remove the temp .air file.
			const char *nativePath = intermediateFilePath;
			systemRun("rm", &nativePath, 1, NULL);

			// Store the compiled bytecode.
			FileStream fHandle = {};
			if(fsOpenStreamFromPath(RD_SHADER_BINARIES, outFile, FM_READ_BINARY, &fHandle))
			{
			pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&fHandle);
			pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
			fsReadFromStream(&fHandle, pOut->pByteCode, pOut->mByteCodeSize);
			fsCloseStream(&fHandle);
			}
		}
		else
		{
			LOGF(eERROR, "Failed to assemble shader's %s .metallib file", outFile);
		}
	}
	else
	{
		LOGF(eERROR, "Failed to compile shader %s", filePath);
	}
}
#endif
#if (defined(DIRECT3D12) || defined(DIRECT3D11))
extern void compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(ORBIS)
extern bool orbis_compileShader(
	Renderer* pRenderer,
	ShaderStage stage, ShaderStage allStages,
	const char* srcFileName,
	const char* outFileName,
	uint32_t macroCount, ShaderMacro* pMacros,
	BinaryShaderStageDesc* pOut,
	const char* pEntryPoint);
#endif
#if defined(PROSPERO)
extern bool prospero_compileShader(
	Renderer* pRenderer,
	ShaderStage stage, ShaderStage allStages,
	const char* srcFileName,
	const char* outFileName,
	uint32_t macroCount, ShaderMacro* pMacros,
	BinaryShaderStageDesc* pOut,
	const char* pEntryPoint);
#endif

static eastl::string fsReadFromStreamSTLLine(FileStream* stream)
{
	eastl::string result;

	while (!fsStreamAtEnd(stream))
	{
		char nextChar = 0;
		fsReadFromStream(stream, &nextChar, sizeof(nextChar));
		if (nextChar == 0 || nextChar == '\n')
		{
			break;
		}
		if (nextChar == '\r')
		{
			char newLine = 0;
			fsReadFromStream(stream, &newLine, sizeof(newLine));
			if (newLine == '\n')
			{
				break;
			}
			else
			{
				// We're not looking at a "\r\n" sequence, so add the '\r' to the buffer.
				fsSeekStream(stream, SBO_CURRENT_POSITION, -1);
			}
		}
		result.push_back(nextChar);
	}

	return result;
}

// Function to generate the timestamp of this shader source file considering all include file timestamp
#if !defined(NX64)
static bool process_source_file(const char* pAppName, FileStream* original, const char* filePath, FileStream* file, time_t& outTimeStamp, eastl::string& outCode)
{
	// If the source if a non-packaged file, store the timestamp
	if (file)
	{
		time_t fileTimeStamp = fsGetLastModifiedTime(RD_SHADER_SOURCES, filePath);

		if (fileTimeStamp > outTimeStamp)
			outTimeStamp = fileTimeStamp;
	}
	else
	{
		return true; // The source file is missing, but we may still be able to use the shader binary.
	}

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


			// open the include file
			FileStream fHandle = {};
			char includePath[FS_MAX_PATH] = {};
			{
				char parentPath[FS_MAX_PATH] = {};
				fsGetParentPath(filePath, parentPath);
				fsAppendPathComponent(parentPath, fileName.c_str(), includePath);
			}
			if (!fsOpenStreamFromPath(RD_SHADER_SOURCES, includePath, FM_READ_BINARY, &fHandle))
			{
				LOGF(LogLevel::eERROR, "Cannot open #include file: %s", includePath);
				continue;
			}

			// Add the include file into the current code recursively
			if (!process_source_file(pAppName, original, includePath, &fHandle, outTimeStamp, outCode))
			{
				fsCloseStream(&fHandle);
				return false;
			}

			fsCloseStream(&fHandle);
		}

#if defined(TARGET_IOS) || defined(ANDROID)
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
#endif

// Loads the bytecode from file if the binary shader file is newer than the source
bool check_for_byte_code(Renderer* pRenderer, const char* binaryShaderPath, time_t sourceTimeStamp, BinaryShaderStageDesc* pOut)
{
	// If source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
	// than source
	time_t dstTimeStamp = fsGetLastModifiedTime(RD_SHADER_BINARIES, binaryShaderPath);
	if (!sourceTimeStamp || (dstTimeStamp < sourceTimeStamp))
		return false;

	FileStream fh = {};
	if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, binaryShaderPath, FM_READ_BINARY, &fh))
	{
		LOGF(LogLevel::eERROR, "'%s' is not a valid shader bytecode file", binaryShaderPath);
		return false;
	}

	if (!fsGetStreamFileSize(&fh))
	{
		fsCloseStream(&fh);
		return false;
	}

#if defined(PROSPERO)
	extern void prospero_loadByteCode(Renderer*, FileStream*, BinaryShaderStageDesc*);
	prospero_loadByteCode(pRenderer, &fh, pOut);
#else
	ssize_t size = fsGetStreamFileSize(&fh);
	pOut->mByteCodeSize = (uint32_t)size;
	pOut->pByteCode = tf_memalign(256, size);
	fsReadFromStream(&fh, (void*)pOut->pByteCode, size);
#endif
	fsCloseStream(&fh);

	return true;
}

// Saves bytecode to a file
bool save_byte_code(const char* binaryShaderPath, char* byteCode, uint32_t byteCodeSize)
{
	if (!byteCodeSize)
		return false;

	FileStream fh = {};

	if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, binaryShaderPath, FM_WRITE_BINARY, &fh))
		return false;

	fsWriteToStream(&fh, byteCode, byteCodeSize);
	fsCloseStream(&fh);

	return true;
}

bool load_shader_stage_byte_code(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const ShaderStageLoadDesc& loadDesc, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut)
{
	UNREF_PARAM(loadDesc.mFlags);

	eastl::string code;
#if !defined(NX64)
	time_t          timeStamp = 0;
#endif

#if !defined(METAL) && !defined(NX64)
	FileStream sourceFileStream = {};
	bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, loadDesc.pFileName, FM_READ_BINARY, &sourceFileStream);
	ASSERT(sourceExists);

	if (!process_source_file(pRenderer->pName, &sourceFileStream, loadDesc.pFileName, &sourceFileStream, timeStamp, code))
	{
		fsCloseStream(&sourceFileStream);
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

	char hashStringBuffer[14];
	sprintf(&hashStringBuffer[0], "%zu.%s", (size_t)hash, "spv");

	char nxShaderPath[FS_MAX_PATH] = {};
	fsAppendPathExtension(loadDesc.pFileName, hashStringBuffer, nxShaderPath);
	FileStream sourceFileStream = {};
	bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, nxShaderPath, FM_READ_BINARY, &sourceFileStream);
	ASSERT(sourceExists);

	if (sourceExists)
	{
		pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&sourceFileStream);
		pOut->pByteCode = tf_malloc(pOut->mByteCodeSize);
		fsReadFromStream(&sourceFileStream, pOut->pByteCode, pOut->mByteCodeSize);

		LOGF(LogLevel::eINFO, "Shader loaded: '%s' with macro string '%s'", nxShaderPath, shaderDefines.c_str());
		fsCloseStream(&sourceFileStream);
		return true;
	}
	else
	{
		LOGF(LogLevel::eERROR, "Failed to load shader: '%s' with macro string '%s'", nxShaderPath, shaderDefines.c_str());
		return false;
	}

#else
	char metalShaderPath[FS_MAX_PATH] = {};
	fsAppendPathExtension(loadDesc.pFileName, "metal", metalShaderPath);
	FileStream sourceFileStream = {};
	bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, metalShaderPath, FM_READ_BINARY, &sourceFileStream);
	ASSERT(sourceExists);
	if (!process_source_file(pRenderer->pName, &sourceFileStream, metalShaderPath, &sourceFileStream, timeStamp, code))
	{
		fsCloseStream(&sourceFileStream);
		return false;
	}
#endif

#ifndef NX64
	eastl::string shaderDefines;
	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderDefines += (eastl::string(pMacros[i].definition) + pMacros[i].value);
	}
#ifdef _DEBUG
	shaderDefines += "_DEBUG";
#else
	shaderDefines += "NDEBUG";
#endif

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
	char extension[FS_MAX_PATH] = { 0 };
	fsGetPathExtension(loadDesc.pFileName, extension);
	char fileName[FS_MAX_PATH] = { 0 };
	fsGetPathFileName(loadDesc.pFileName, fileName);
	eastl::string appName(pRenderer->pName);

#ifdef __linux__
	appName.make_lower();
	appName = appName != pRenderer->pName ? appName : appName + "_";
#endif

	eastl::string binaryShaderComponent = fileName +
		eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + extension +
		eastl::string().sprintf("%u", target) +
#ifdef DIRECT3D11
		eastl::string().sprintf("%u", pRenderer->mFeatureLevel) +
#endif
		".bin";

	// Shader source is newer than binary
	if (!check_for_byte_code(pRenderer, binaryShaderComponent.c_str(), timeStamp, pOut))
	{
		if (!sourceExists)
		{
			LOGF(eERROR, "No source shader or precompiled binary present for file %s", fileName);
			return false;
		}

#if defined(ORBIS)
		orbis_compileShader(pRenderer,
			stage, allStages,
			loadDesc.pFileName,
			binaryShaderComponent.c_str(),
			macroCount, pMacros,
			pOut,
			loadDesc.pEntryPointName);
#elif defined(PROSPERO)
		prospero_compileShader(pRenderer,
			stage, allStages,
			loadDesc.pFileName,
			binaryShaderComponent.c_str(),
			macroCount, pMacros,
			pOut,
			loadDesc.pEntryPointName);
#else
		if (pRenderer->mApi == RENDERER_API_METAL || pRenderer->mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
#if defined(__ANDROID__)
			vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.pEntryPointName);
			if (!save_byte_code(binaryShaderComponent.c_str(), (char*)(pOut->pByteCode), pOut->mByteCodeSize))
			{
				LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
			}
#else
			vk_compileShader(pRenderer, target, stage, loadDesc.pFileName, binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.pEntryPointName);
#endif
#elif defined(METAL)
			mtl_compileShader(pRenderer, metalShaderPath, binaryShaderComponent.c_str(), macroCount, pMacros, pOut, loadDesc.pEntryPointName);
#endif
		}
		else
		{
#if defined(DIRECT3D12) || defined(DIRECT3D11)
			compileShader(
				pRenderer, target, stage, loadDesc.pFileName, (uint32_t)code.size(), code.c_str(),
				loadDesc.mFlags & SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID,
				macroCount, pMacros,
				pOut, loadDesc.pEntryPointName);

			if (!save_byte_code(binaryShaderComponent.c_str(), (char*)(pOut->pByteCode), pOut->mByteCodeSize))
			{
				LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
			}
#endif
		}
		if (!pOut->pByteCode)
		{
			LOGF(eERROR, "Error while generating bytecode for shader %s", loadDesc.pFileName);
			fsCloseStream(&sourceFileStream);
			ASSERT(false);
			return false;
		}
#endif
	}
#else
#endif

	fsCloseStream(&sourceFileStream);
	return true;
}
#ifdef TARGET_IOS
bool find_shader_stage(const char* fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	char extension[FS_MAX_PATH] = {0};
	fsGetPathExtension(fileName, extension);
	if (stricmp(extension, "vert") == 0)
	{
		*pOutStage = &pDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (stricmp(extension, "frag") == 0)
	{
		*pOutStage = &pDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
	else if (stricmp(extension, "comp") == 0)
	{
		*pOutStage = &pDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else if ((stricmp(extension, "rgen") == 0) ||
		(stricmp(extension, "rmiss") == 0) ||
		(stricmp(extension, "rchit") == 0) ||
		(stricmp(extension, "rint") == 0) ||
		(stricmp(extension, "rahit") == 0) ||
		(stricmp(extension, "rcall") == 0))
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
	const char* extension, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	if (stricmp(extension, "vert") == 0)
	{
		*pOutStage = &pBinaryDesc->mVert;
		*pStage = SHADER_STAGE_VERT;
	}
	else if (stricmp(extension, "frag") == 0)
	{
		*pOutStage = &pBinaryDesc->mFrag;
		*pStage = SHADER_STAGE_FRAG;
	}
#ifndef METAL
	else if (stricmp(extension, "tesc") == 0)
	{
		*pOutStage = &pBinaryDesc->mHull;
		*pStage = SHADER_STAGE_HULL;
	}
	else if (stricmp(extension, "tese") == 0)
	{
		*pOutStage = &pBinaryDesc->mDomain;
		*pStage = SHADER_STAGE_DOMN;
	}
	else if (stricmp(extension, "geom") == 0)
	{
		*pOutStage = &pBinaryDesc->mGeom;
		*pStage = SHADER_STAGE_GEOM;
	}
#endif
	else if (stricmp(extension, "comp") == 0)
	{
		*pOutStage = &pBinaryDesc->mComp;
		*pStage = SHADER_STAGE_COMP;
	}
	else if ((stricmp(extension, "rgen") == 0) ||
		(stricmp(extension, "rmiss") == 0) ||
		(stricmp(extension, "rchit") == 0) ||
		(stricmp(extension, "rint") == 0) ||
		(stricmp(extension, "rahit") == 0) ||
		(stricmp(extension, "rcall") == 0))
	{
		*pOutStage = &pBinaryDesc->mComp;
#ifndef METAL
		*pStage = SHADER_STAGE_RAYTRACING;
#else
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
#ifndef DIRECT3D11
	if ((uint32_t)pDesc->mTarget > pRenderer->mShaderTarget)
	{
		eastl::string error = eastl::string().sprintf("Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)pDesc->mTarget, (uint32_t)pRenderer->mShaderTarget);
		LOGF(LogLevel::eERROR, error.c_str());
		return;
	}
#endif

#ifndef TARGET_IOS
	BinaryShaderDesc      binaryDesc = {};
#if defined(METAL)
	char* pSources[SHADER_STAGE_COUNT] = {};
#endif

	ShaderStage stages = SHADER_STAGE_NONE;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName) != 0)
		{
			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			char ext[FS_MAX_PATH] = { 0 };
			fsGetPathExtension(pDesc->mStages[i].pFileName, ext);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
				stages |= stage;
		}
	}
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName) != 0)
		{
			const char* fileName = pDesc->mStages[i].pFileName;

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			char ext[FS_MAX_PATH] = { 0 };
			fsGetPathExtension(fileName, ext);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
			{
				const uint32_t macroCount = pDesc->mStages[i].mMacroCount + pRenderer->mBuiltinShaderDefinesCount;
				eastl::vector<ShaderMacro> macros(macroCount);
				for (uint32_t macro = 0; macro < pRenderer->mBuiltinShaderDefinesCount; ++macro)
					macros[macro] = pRenderer->pBuiltinShaderDefines[macro];
				for (uint32_t macro = 0; macro < pDesc->mStages[i].mMacroCount; ++macro)
					macros[pRenderer->mBuiltinShaderDefinesCount + macro] = pDesc->mStages[i].pMacros[macro];

				if (!load_shader_stage_byte_code(
					pRenderer, pDesc->mTarget, stage, stages, pDesc->mStages[i], macroCount, macros.data(),
					pStage))
					return;

				binaryDesc.mStages |= stage;
#if defined(METAL)
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "stageMain";

				char metalFileName[FS_MAX_PATH] = {0};
				fsAppendPathExtension(fileName, "metal", metalFileName);

				FileStream fh = {};
				fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, &fh);
				size_t metalFileSize = fsGetStreamFileSize(&fh);
				pSources[i] = (char*)tf_malloc(metalFileSize + 1);
				pStage->pSource = pSources[i];
				pStage->mSourceSize = (uint32_t)metalFileSize;
				fsReadFromStream(&fh, pSources[i], metalFileSize);
				pSources[i][metalFileSize] = 0; // Ensure the shader text is null-terminated
				fsCloseStream(&fh);
#elif !defined(ORBIS) && !defined(PROSPERO)
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "main";
#endif
			}
		}
	}

#if defined(PROSPERO)
	binaryDesc.mOwnByteCode = true;
#endif

	addShaderBinary(pRenderer, &binaryDesc, ppShader);

#if defined(METAL)
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pSources[i])
		{
			tf_free(pSources[i]);
		}
	}
#endif
#if !defined(PROSPERO)
	if (binaryDesc.mStages & SHADER_STAGE_VERT)
		tf_free(binaryDesc.mVert.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_FRAG)
		tf_free(binaryDesc.mFrag.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_COMP)
		tf_free(binaryDesc.mComp.pByteCode);
#if !defined(METAL)
	if (binaryDesc.mStages & SHADER_STAGE_TESC)
		tf_free(binaryDesc.mHull.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_TESE)
		tf_free(binaryDesc.mDomain.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_GEOM)
		tf_free(binaryDesc.mGeom.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_RAYTRACING)
		tf_free(binaryDesc.mComp.pByteCode);
#endif
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
			ShaderStage stage;
			ShaderStageDesc* pStage = NULL;
			if (find_shader_stage(pDesc->mStages[i].pFileName, &desc, &pStage, &stage))
			{
				char metalFileName[FS_MAX_PATH] = {0};
				fsAppendPathExtension(pDesc->mStages[i].pFileName, "metal", metalFileName);
				FileStream fh = {};
				bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, &fh);
				ASSERT(sourceExists);

				pStage->pName = pDesc->mStages[i].pFileName;
				time_t timestamp = 0;
				process_source_file(pRenderer->pName, &fh, metalFileName, &fh, timestamp, codes[i]);
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
				fsCloseStream(&fh);
				desc.mStages |= stage;
			}
		}
	}

	addShader(pRenderer, &desc, ppShader);
#endif
}
/************************************************************************/
// Pipeline cache save, load
/************************************************************************/
void addPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	FileStream stream = {};
	bool success = fsOpenStreamFromPath(RD_PIPELINE_CACHE, pDesc->pFileName, FM_READ_BINARY, &stream);
	ssize_t dataSize = 0;
	void* data = NULL;
	if (success)
	{
		dataSize = fsGetStreamFileSize(&stream);
		data = NULL;
		if (dataSize)
		{
			data = tf_malloc(dataSize);
			fsReadFromStream(&stream, data, dataSize);
		}

		fsCloseStream(&stream);
	}

	PipelineCacheDesc desc = {};
	desc.mFlags = pDesc->mFlags;
	desc.pData = data;
	desc.mSize = dataSize;
	addPipelineCache(pRenderer, &desc, ppPipelineCache);

	if (data)
	{
		tf_free(data);
	}
#endif
}

void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	FileStream stream = {};
	if (fsOpenStreamFromPath(RD_PIPELINE_CACHE, pDesc->pFileName, FM_WRITE_BINARY, &stream))
	{
		size_t dataSize = 0;
		getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, NULL);
		if (dataSize)
		{
			void* data = tf_malloc(dataSize);
			getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, data);
			fsWriteToStream(&stream, data, dataSize);
			tf_free(data);
		}

		fsCloseStream(&stream);
	}
#endif
}
/************************************************************************/
/************************************************************************/
