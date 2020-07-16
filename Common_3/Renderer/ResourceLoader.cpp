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
#include "../ThirdParty/OpenSource/EASTL/deque.h"

#define TINYKTX_IMPLEMENTATION
#include "../ThirdParty/OpenSource/tinyktx/tinyktx.h"

#include "../ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

#define CGLTF_IMPLEMENTATION
#include "../ThirdParty/OpenSource/cgltf/cgltf.h"

#include "IRenderer.h"
#include "IResourceLoader.h"
#include "../OS/Interfaces/ILog.h"
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
extern void updateVirtualTexture(Renderer* pRenderer, Queue* pQueue, Texture* pTexture);

/************************************************************************/
/************************************************************************/

// Durango, Orbis, and iOS have unified memory
// so we dont need a command buffer to upload data
// We can get away with a memcpy since the GPU memory is marked as CPU write combine
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

static inline ResourceState util_determine_resource_start_state(const Buffer* pBuffer)
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
typedef void(*PreMipStepFn)(FileStream* stream, uint32_t mip);

typedef struct TextureUpdateDescInternal
{
	Texture*          pTexture;
	FileStream*       pStream;
	MappedMemoryRange mRange;
	uint32_t          mBaseMipLevel;
	uint32_t          mMipLevels;
	uint32_t          mBaseArrayLayer;
	uint32_t          mLayerCount;
	bool              mMipsAfterSlice;

	PreMipStepFn      pPreMip;
} TextureUpdateDescInternal;

typedef struct CopyResourceSet
{
#ifndef DIRECT3D11
	Fence*   pFence;
#endif
	Cmd*     pCmd;
	CmdPool* pCmdPool;
	Buffer*  mBuffer;
	uint64_t mAllocatedSpace;

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
	UPDATE_REQUEST_UPDATE_RESOURCE_STATE,
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

typedef struct UpdateRequest
{
	UpdateRequest() : mType(UPDATE_REQUEST_INVALID) {}
	UpdateRequest(BufferUpdateDesc& buffer) : mType(UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	UpdateRequest(TextureLoadDesc& texture) : mType(UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
	UpdateRequest(TextureUpdateDescInternal& texture) : mType(UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
	UpdateRequest(GeometryLoadDesc& geom) : mType(UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
	UpdateRequest(Buffer* buf, ResourceState state) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE), buffer(buf), texture(NULL), startState(state) {}
	UpdateRequest(Texture* tex, ResourceState state) : mType(UPDATE_REQUEST_UPDATE_RESOURCE_STATE), buffer(NULL), texture(tex), startState(state) {}
	UpdateRequestType mType;
	uint64_t          mWaitIndex = 0;
	Buffer*           mUploadBuffer = NULL;
	union
	{
		BufferUpdateDesc bufUpdateDesc;
		TextureUpdateDescInternal texUpdateDesc;
		TextureLoadDesc texLoadDesc;
		GeometryLoadDesc geomLoadDesc;
		struct { Buffer* buffer; Texture* texture; ResourceState startState; };
	};
} UpdateRequest;

typedef struct UpdateState
{
	UpdateState() : UpdateState(UpdateRequest())
	{
	}
	UpdateState(const UpdateRequest& request) : mRequest(request)
	{
	}

	UpdateRequest mRequest;
} UpdateState;

struct ResourceLoader
{
	Renderer* pRenderer;

	ResourceLoaderDesc mDesc;

	volatile int mRun;
	ThreadDesc   mThreadDesc;
	ThreadHandle mThread;

	Mutex mQueueMutex;
	ConditionVariable mQueueCond;
	Mutex mTokenMutex;
	ConditionVariable mTokenCond;
	eastl::deque <UpdateRequest> mActiveQueue;
	eastl::deque <UpdateRequest> mRequestQueue[MAX_LINKED_GPUS][LOAD_PRIORITY_COUNT];

	tfrg_atomic64_t mTokenCompleted[LOAD_PRIORITY_COUNT];
	tfrg_atomic64_t mTokenCounter[LOAD_PRIORITY_COUNT];

	SyncToken mCurrentTokenState[MAX_FRAMES];

	CopyEngine pCopyEngines[MAX_LINKED_GPUS];
	uint32_t   mNextSet;
	uint32_t   mSubmittedSets;
	tfrg_atomic32_t mAsleep;
	tfrg_atomic32_t mSignalTokens;

#if defined(NX64)
	ThreadTypeNX mThreadType;
	void* mThreadStackPtr;
#endif
};

static ResourceLoader* pResourceLoader = NULL;

static inline SyncToken max(const SyncToken& a, const SyncToken& b)
{
	SyncToken result = {};
	for (size_t i = 0; i < LOAD_PRIORITY_COUNT; i += 1)
	{
		result.mWaitIndex[i] = max(a.mWaitIndex[i], b.mWaitIndex[i]);
	}
	return result;
}

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
	Buffer* buffer = (Buffer*)conf_memalign(alignof(Buffer), sizeof(Buffer) + memoryRequirement);
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

	pCopyEngine->resourceSets = (CopyResourceSet*)conf_malloc(sizeof(CopyResourceSet)*bufferCount);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		conf_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

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

	conf_free(pCopyEngine->resourceSets);

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
		for (size_t p = 0; p < LOAD_PRIORITY_COUNT; ++p)
		{
			for (UpdateRequest& request : pResourceLoader->mRequestQueue[i][p])
			{
				if (request.mUploadBuffer)
					removeBuffer(pResourceLoader->pRenderer, request.mUploadBuffer);
			}
		}
	}
}

static UploadFunctionResult updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const UpdateState& pTextureUpdate)
{
	const TextureUpdateDescInternal& texUpdateDesc = pTextureUpdate.mRequest.texUpdateDesc;
	// When this call comes from updateResource, staging buffer data is already filled
	// All that is left to do is record and execute the Copy commands
	bool dataAlreadyFilled = texUpdateDesc.mRange.pBuffer ? true : false;
	Texture* texture = texUpdateDesc.pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
	FileStream* stream = texUpdateDesc.pStream;
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
	TextureBarrier barrier = { texture, RESOURCE_STATE_COPY_DEST };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
#endif

	MappedMemoryRange upload = dataAlreadyFilled ? texUpdateDesc.mRange : allocateStagingMemory(requiredSize, sliceAlignment);
	uint64_t offset = 0;

	// #TODO: Investigate - fsRead crashes if we pass the upload buffer mapped address. Allocating temporary buffer as a workaround. Does NX support loading from disk to GPU shared memory?
#ifdef NX64
	ssize_t nxTempBufferSize = fsGetStreamFileSize(stream) - fsGetStreamSeekPosition(stream);
	void* nxTempBuffer = NULL;
	if (!dataAlreadyFilled)
	{
		nxTempBuffer = conf_malloc(nxTempBufferSize);
		ssize_t bytesRead = fsReadFromStream(stream, nxTempBuffer, nxTempBufferSize);
		if (bytesRead != nxTempBufferSize)
		{
			fsCloseStream(stream);
			conf_free(nxTempBuffer);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		fsCloseStream(stream);
		stream = fsOpenReadOnlyMemory(nxTempBuffer, nxTempBufferSize, true);
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
			if (texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMip)
			{
				texUpdateDesc.pPreMip(stream, j);
			}

			for (uint32_t i = secondStart; i < secondEnd; ++i)
			{
				if (!texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMip)
				{
					texUpdateDesc.pPreMip(stream, i);
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
				uint64_t subRowSize = rowBytes;
				uint8_t* data = upload.pData + offset;

				if (!dataAlreadyFilled)
				{
					for (uint32_t z = 0; z < subDepth; ++z)
					{
						uint8_t* dstData = data + subSlicePitch * z;
						for (uint32_t r = 0; r < subNumRows; ++r)
						{
							ssize_t bytesRead = fsReadFromStream(stream, dstData + r * subRowPitch, subRowSize);
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
	barrier = { texture, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
#endif

	fsCloseStream(stream);

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pTextureUpdate)
{
	TextureLoadDesc* pTextureDesc = &pTextureUpdate.mRequest.texLoadDesc;

	if (pTextureDesc->pFilePath)
	{
		FileStream* stream = NULL;
		PathHandle path = fsCopyPath(pTextureDesc->pFilePath);

		const char* extension = fsGetPathExtension(path).buffer;
		bool success = false;

		TextureUpdateDescInternal updateDesc = {};

		if (extension && !fsFileExists(path))
		{
			extension = NULL;
		}

		if (!extension)
		{
#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
			extension = "ktx";
#elif defined(_WINDOWS) || defined(XBOX) || defined(__APPLE__) || defined(__linux__)
			extension = "dds";
#elif defined(ORBIS) || defined(PROSPERO)
			extension = "gnf";
#endif
			path = fsAppendPathExtension(pTextureDesc->pFilePath, extension);
		}

		fsFreePath((Path*)pTextureDesc->pFilePath);

		PathComponent fileName = fsGetPathFileName(path);
		TextureDesc textureDesc = {};
		textureDesc.pName = fileName.buffer;

		if (extension)
		{
#if defined(ORBIS) || defined(XBOX) || defined(PROSPERO)
#if defined(ORBIS) || defined(PROSPERO)
#define LOAD_DIRECT loadGnfTexture
#elif defined(XBOX)
#define LOAD_DIRECT loadXDDSTexture
#endif
			stream = fsOpenFile(path, FM_READ_BINARY);
			uint32_t res = 1;
			if (stream)
			{
				extern uint32_t LOAD_DIRECT(Renderer* pRenderer, FileStream* stream, const char* name, TextureCreationFlags flags, Texture** ppTexture);
				res = LOAD_DIRECT(pRenderer, stream, fsGetPathFileName(path).buffer, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
				fsCloseStream(stream);
			}

			if (!res)
			{
				return UPLOAD_FUNCTION_RESULT_COMPLETED;
			}
#endif

			if (!stricmp(extension, "dds"))
			{
				stream = fsOpenFile(path, FM_READ_BINARY);
				success = loadDDSTextureDesc(stream, &textureDesc);
			}
			else if (!stricmp(extension, "ktx"))
			{
				stream = fsOpenFile(path, FM_READ_BINARY);
				success = loadKTXTextureDesc(stream, &textureDesc);
				updateDesc.mMipsAfterSlice = true;
				// KTX stores mip size before the mip data
				// This function gets called to skip the mip size so we read the mip data
				updateDesc.pPreMip = [](FileStream* pStream, uint32_t)
				{
					fsReadFromStreamUInt32(pStream);
				};
			}
			else if (!stricmp(extension, "basis"))
			{
				void* data = NULL;
				uint32_t dataSize = 0;
				stream = fsOpenFile(path, FM_READ_BINARY);
				success = loadBASISTextureDesc(stream, &textureDesc, &data, &dataSize);
				if (success)
				{
					fsCloseStream(stream);
					stream = fsOpenReadOnlyMemory(data, dataSize, true);
				}
			}
		}

		if (success)
		{
			textureDesc.mStartState = RESOURCE_STATE_COMMON;
			textureDesc.mFlags |= pTextureDesc->mCreationFlag;
			textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;
			addTexture(pRenderer, &textureDesc, pTextureDesc->ppTexture);

			updateDesc.pStream = stream;
			updateDesc.pTexture = *pTextureDesc->ppTexture;
			updateDesc.mBaseMipLevel = 0;
			updateDesc.mMipLevels = textureDesc.mMipLevels;
			updateDesc.mBaseArrayLayer = 0;
			updateDesc.mLayerCount = textureDesc.mArraySize;

			return updateTexture(pRenderer, pCopyEngine, activeSet, UpdateRequest(updateDesc));
		}
		/************************************************************************/
		// Sparse Tetxtures
		/************************************************************************/
#if defined(DIRECT3D12) || defined(VULKAN)
		if (!stricmp(extension, "svt"))
		{
			stream = fsOpenFile(path, FM_READ_BINARY);
			success = loadSVTTextureDesc(stream, &textureDesc);
			if (success)
			{
				ssize_t dataSize = fsGetStreamFileSize(stream) - fsGetStreamSeekPosition(stream);
				void* data = conf_malloc(dataSize);
				fsReadFromStream(stream, data, dataSize);

				textureDesc.mStartState = RESOURCE_STATE_COMMON;
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
				visDesc.mDesc.pName = "Vis Buffer for Sparse Texture";
				visDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mVisibility;
				addResource(&visDesc, NULL, LOAD_PRIORITY_NORMAL);

				BufferLoadDesc prevVisDesc = {};
				prevVisDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
				prevVisDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				prevVisDesc.mDesc.mStructStride = sizeof(uint);
				prevVisDesc.mDesc.mElementCount = (uint64_t)pPageTable->size();
				prevVisDesc.mDesc.mSize = prevVisDesc.mDesc.mStructStride * prevVisDesc.mDesc.mElementCount;
				prevVisDesc.mDesc.pName = "Prev Vis Buffer for Sparse Texture";
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
				alivePageDesc.mDesc.pName = "Alive pages buffer for Sparse Texture";
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
				removePageDesc.mDesc.pName = "Remove pages buffer for Sparse Texture";
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
				pageCountsDesc.mDesc.pName = "Page count buffer for Sparse Texture";
				pageCountsDesc.ppBuffer = &(*pTextureDesc->ppTexture)->pSvt->mPageCounts;
				addResource(&pageCountsDesc, NULL, LOAD_PRIORITY_NORMAL);

				fsCloseStream(stream);

				return UPLOAD_FUNCTION_RESULT_COMPLETED;
			}

			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
#endif
	}

	return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}

static UploadFunctionResult updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateState& pBufferUpdate)
{
	BufferUpdateDesc& bufUpdateDesc = pBufferUpdate.mRequest.bufUpdateDesc;
	ASSERT(pCopyEngine->pQueue->mNodeIndex == bufUpdateDesc.pBuffer->mNodeIndex);
	Buffer* pBuffer = bufUpdateDesc.pBuffer;
	ASSERT(pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

	Cmd* pCmd = acquireCmd(pCopyEngine, activeSet);

	MappedMemoryRange range = bufUpdateDesc.mInternal.mMappedRange;
	cmdUpdateBuffer(pCmd, pBuffer, bufUpdateDesc.mDstOffset, range.pBuffer, range.mOffset, range.mSize);

	ResourceState state = util_determine_resource_start_state(pBuffer);
#if defined(XBOX)
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
			BufferBarrier barrier = { pUpdate.mRequest.buffer, (ResourceState)pUpdate.mRequest.startState };
			cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
		}
		else if (pUpdate.mRequest.texture)
		{
			TextureBarrier barrier = { pUpdate.mRequest.texture, (ResourceState)pUpdate.mRequest.startState };
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

#if defined(FORGE_DEBUG)
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
				Path* parent = fsGetParentPath(pDesc->pFilePath);
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
				jsmntok_t* tokens = (jsmntok_t*)conf_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
				jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
				ASSERT(tokens[0].size == skin->joints_count + 1);
				cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses, skin->joints_count * sizeof(float[16]) / sizeof(float));
				for (uint32_t r = 0; r < skin->joints_count; ++r)
					geom->pJointRemaps[remapCount + r] = atoi(jointRemaps + tokens[1 + r].start);
				conf_free(tokens);
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
/************************************************************************/
// Internal Resource Loader Implementation
/************************************************************************/
static bool areTasksAvailable(ResourceLoader* pLoader)
{
	for (size_t i = 0; i < MAX_LINKED_GPUS; ++i)
	{
		for (size_t priority = 0; priority < LOAD_PRIORITY_COUNT; priority += 1)
		{
			if (!pLoader->mRequestQueue[i][priority].empty())
			{
				return true;
			}
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
	uint64_t       completionMask = 0;

	while (pLoader->mRun)
	{
		pLoader->mQueueMutex.Acquire();
		while (!areTasksAvailable(pLoader) && !tfrg_atomic32_load_relaxed(&pLoader->mSignalTokens) && pLoader->mRun)
		{
			// Set the asleep flag.
			// This way the querying threads can check whether resource loader needs to be woken up to signal the pending tokens
			tfrg_atomic32_store_relaxed(&pLoader->mAsleep, 1);
			// Sleep until someone adds an update request to the queue
			pLoader->mQueueCond.Wait(pLoader->mQueueMutex);
		}
		pLoader->mQueueMutex.Release();

		pLoader->mNextSet = (pLoader->mNextSet + 1) % pLoader->mDesc.mBufferCount;
		for (uint32_t i = 0; i < linkedGPUCount; ++i)
		{
			waitCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, true);
			resetCopyEngineSet(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet);
		}

		// Signal pending tokens from previous frames
		pLoader->mTokenMutex.Acquire();
		for (size_t i = 0; i < LOAD_PRIORITY_COUNT; ++i)
		{
			tfrg_atomic64_store_release(&pLoader->mTokenCompleted[i], pLoader->mCurrentTokenState[pLoader->mNextSet].mWaitIndex[i]);
		}
		pLoader->mTokenMutex.Release();
		pLoader->mTokenCond.WakeAll();

		// Reset the flags
		tfrg_atomic32_store_relaxed(&pLoader->mAsleep, 0);
		tfrg_atomic32_store_relaxed(&pLoader->mSignalTokens, 0);

		for (uint32_t i = 0; i < linkedGPUCount; ++i)
		{
			for (uint32_t priority = 0; priority < LOAD_PRIORITY_COUNT; ++priority)
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
						result = updateBuffer(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_UPDATE_TEXTURE:
						result = updateTexture(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_UPDATE_RESOURCE_STATE:
						result = updateResourceState(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_LOAD_TEXTURE:
						result = loadTexture(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_LOAD_GEOMETRY:
						result = loadGeometry(pLoader->pRenderer, &pLoader->pCopyEngines[i], pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_INVALID:
						break;
					}

					if (updateState.mRequest.mUploadBuffer)
					{
						CopyResourceSet& resourceSet = pLoader->pCopyEngines[i].resourceSets[pLoader->mNextSet];
						resourceSet.mTempBuffers.push_back(updateState.mRequest.mUploadBuffer);
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
				streamerFlush(&pLoader->pCopyEngines[i], pLoader->mNextSet);
			}
		}

		SyncToken nextToken = max(maxToken, getLastTokenCompleted());
		for (size_t i = 0; i < LOAD_PRIORITY_COUNT; ++i)
		{
			pLoader->mCurrentTokenState[pLoader->mNextSet].mWaitIndex[i] = nextToken.mWaitIndex[i];
		}

		completionMask = 0;
	}

	for (uint32_t i = 0; i < linkedGPUCount; ++i)
	{
		streamerFlush(&pLoader->pCopyEngines[i], pLoader->mNextSet);
#ifndef DIRECT3D11
		waitQueueIdle(pLoader->pCopyEngines[i].pQueue);
#endif
		cleanupCopyEngine(pLoader->pRenderer, &pLoader->pCopyEngines[i]);
	}

	freeAllUploadMemory();
}

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
	pLoader->mRequestQueue[nodeIndex][priority].back().mUploadBuffer =
		(pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pBufferUpdate->mInternal.mMappedRange.pBuffer
																					   : NULL;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, TextureLoadDesc* pTextureUpdate, SyncToken* token, LoadPriority priority)
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
	ASSERT(pTextureUpdate->pStream || pTextureUpdate->mRange.pBuffer);

	uint32_t nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(*pTextureUpdate));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mRequestQueue[nodeIndex][priority].back().mUploadBuffer = (pTextureUpdate->mRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pTextureUpdate->mRange.pBuffer : NULL;
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, Buffer* pBuffer, ResourceState state, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pBuffer->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(pBuffer, state));
	pLoader->mRequestQueue[nodeIndex][priority].back().mWaitIndex = t.mWaitIndex[priority];
	pLoader->mQueueMutex.Release();
	pLoader->mQueueCond.WakeOne();
	if (token) *token = max(t, *token);
}

static void queueResourceUpdate(ResourceLoader* pLoader, Texture* pTexture, ResourceState state, SyncToken* token, LoadPriority priority)
{
	uint32_t nodeIndex = pTexture->mNodeIndex;
	pLoader->mQueueMutex.Acquire();

	SyncToken t = {};
	t.mWaitIndex[priority] = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter[priority], 1) + 1;

	pLoader->mRequestQueue[nodeIndex][priority].emplace_back(UpdateRequest(pTexture, state));
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
	// If the loader thread is in condition variable sleep wake it so it can signal our token
	if (tfrg_atomic32_load_relaxed(&pLoader->mAsleep))
	{
		// By setting this flag, we make the loader thread break out of the while loop to signal the tokens
		tfrg_atomic32_store_relaxed(&pLoader->mSignalTokens, 1);
		pLoader->mQueueCond.WakeOne();
	}

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
		pLoader->mTokenCond.Wait(pLoader->mTokenMutex, 0);
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

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token, LoadPriority priority)
{
	uint64_t stagingBufferSize = pResourceLoader->pCopyEngines[0].bufferSize;
	ASSERT(stagingBufferSize > 0);

	bool update = pBufferDesc->pData || pBufferDesc->mForceReset;

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
				memset(updateDesc.pMappedData, 0, pBufferDesc->mDesc.mSize);
			}
			else
			{
				memcpy(updateDesc.pMappedData, pBufferDesc->pData, pBufferDesc->mDesc.mSize);
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
			queueResourceUpdate(pResourceLoader, *pBufferDesc->ppBuffer, pBufferDesc->mDesc.mStartState, token, priority);
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token, LoadPriority priority)
{
	ASSERT(pTextureDesc->ppTexture);

	if (!pTextureDesc->pFilePath && pTextureDesc->pDesc)
	{
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
			queueResourceUpdate(pResourceLoader, *pTextureDesc->ppTexture, startState, token, priority);
		}
	}
	else
	{
		TextureLoadDesc updateDesc = {};
		updateDesc.ppTexture = pTextureDesc->ppTexture;
		updateDesc.pFilePath = fsCopyPath(pTextureDesc->pFilePath);
		updateDesc.mNodeIndex = pTextureDesc->mNodeIndex;
		updateDesc.mCreationFlag = pTextureDesc->mCreationFlag;
		queueResourceUpdate(pResourceLoader, &updateDesc, token, priority);
	}
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
	if (UMA || memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		bool map;
		{
#ifdef DIRECT3D11
			MutexLock lock(gContextLock);
#endif
			// We can directly provide the buffer's CPU-accessible address.
			map = !pBuffer->pCpuMappedAddress;
			if (map)
			{
				mapBuffer(pResourceLoader->pRenderer, pBuffer, NULL);
			}
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

void endUpdateResource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	if (pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_UNMAP_BUFFER)
	{
#ifdef DIRECT3D11
		MutexLock lock(gContextLock);
#endif
		unmapBuffer(pResourceLoader->pRenderer, pBufferUpdate->pBuffer);
	}

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (!UMA && memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		queueResourceUpdate(pResourceLoader, pBufferUpdate, token, LOAD_PRIORITY_UPDATE);
	}

	// Restore the state to before the beginUpdateResource call.
	pBufferUpdate->pMappedData = NULL;
	pBufferUpdate->mInternal = {};
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
	queueResourceUpdate(pResourceLoader, &desc, token, LOAD_PRIORITY_UPDATE);

	// Restore the state to before the beginUpdateResource call.
	pTextureUpdate->pMappedData = NULL;
	pTextureUpdate->mInternal = {};
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
	pOut->pByteCode = conf_malloc(pOut->mByteCodeSize);
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
	Renderer* pRenderer, ShaderTarget target, const Path* filePath, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint)
{
	// TODO: Remove this if default directory creation on Shader/binary location is done
	FileStream* fs = fsOpenFile(outFilePath, FM_WRITE_BINARY);
	fsCloseStream(fs);

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
	PathHandle parentDirectory = fsGetParentPath(outFilePath);
	PathHandle logFilePath = fsAppendPathComponent(parentDirectory, logFileName.c_str());
	if (fsFileExists(logFilePath))
	{
		fsDeleteFile(logFilePath);
	}

	if (systemRun(glslangValidator.c_str(), args, 1, logFilePath) == 0)
	{
		FileStream* fh = fsOpenFile(outFilePath, FM_READ_BINARY);
		//Check if the File Handle exists
		ASSERT(fh);
		pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(fh);
		pOut->pByteCode = conf_malloc(pOut->mByteCodeSize);
		fsReadFromStream(fh, pOut->pByteCode, pOut->mByteCodeSize);
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
	BinaryShaderStageDesc* pOut, const char* /*pEntryPoint*/)
{
	// TODO: Remove this if default directory creation on Shader/binary location is done
	FileStream* fHandle = fsOpenFile(outFilePath, FM_WRITE_BINARY);
	fsCloseStream(fHandle);

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
			pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(fHandle);
			pOut->pByteCode = conf_malloc(pOut->mByteCodeSize);
			fsReadFromStream(fHandle, pOut->pByteCode, pOut->mByteCodeSize);
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
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(ORBIS)
extern void orbis_copyInclude(const char* appName, FileStream* fHandle, PathHandle& includeFilePath);
extern void orbis_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const Path* filePath, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(PROSPERO)
extern void prospero_copyInclude(const char* appName, FileStream* fHandle, PathHandle& includeFilePath);
extern void prospero_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const Path* filePath, const Path* outFilePath, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
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

	PathHandle fileDirectory = fsGetParentPath(filePath);

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
#elif defined(PROSPERO)
			prospero_copyInclude(pAppName, fHandle, includeFilePath);
#endif

			// Add the include file into the current code recursively
			if (!process_source_file(pAppName, original, includeFilePath, fHandle, outTimeStamp, outCode))
			{
				fsCloseStream(fHandle);
				return false;
			}

			fsCloseStream(fHandle);
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

// Loads the bytecode from file if the binary shader file is newer than the source
bool check_for_byte_code(Renderer* pRenderer, const Path* binaryShaderPath, time_t sourceTimeStamp, BinaryShaderStageDesc* pOut)
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

#if defined(PROSPERO)
	extern void prospero_loadByteCode(Renderer*, FileStream*, BinaryShaderStageDesc*);
	prospero_loadByteCode(pRenderer, fh, pOut);
#else
	ssize_t size = fsGetStreamFileSize(fh);
	pOut->mByteCodeSize = (uint32_t)size;
	pOut->pByteCode = conf_memalign(256, size);
	fsReadFromStream(fh, (void*)pOut->pByteCode, size);
#endif
	fsCloseStream(fh);

	return true;
}

// Saves bytecode to a file
bool save_byte_code(const Path* binaryShaderPath, char* byteCode, uint32_t byteCodeSize)
{
	if (!byteCodeSize)
		return false;

	FileStream* fh = fsOpenFile(binaryShaderPath, FM_WRITE_BINARY);

	if (!fh)
		return false;

	fsWriteToStream(fh, byteCode, byteCodeSize);
	fsCloseStream(fh);

	return true;
}

bool load_shader_stage_byte_code(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const Path* filePath, ShaderStageLoadFlags flags, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut,
	const char* pEntryPoint)
{
	UNREF_PARAM(flags);

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
	sprintf(&hashStringBuffer[0], "%zu", (size_t)hash);

	PathHandle nxShaderPath = fsAppendPathExtension(filePath, hashStringBuffer);
	nxShaderPath = fsAppendPathExtension(nxShaderPath, "spv");
	FileStream* sourceFileStream = fsOpenFile(nxShaderPath, FM_READ_BINARY);
	ASSERT(sourceFileStream);

	if (sourceFileStream)
	{
		pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(sourceFileStream);
		pOut->pByteCode = conf_malloc(pOut->mByteCodeSize);
		fsReadFromStream(sourceFileStream, pOut->pByteCode, pOut->mByteCodeSize);

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

	eastl::string appName(pRenderer->pName);

#ifdef __linux__
	appName.make_lower();
	appName = appName != pRenderer->pName ? appName : appName + "_";
#endif

#if defined(ORBIS) || defined(PROSPERO)
	eastl::string binaryShaderComponent = pDevShaderBinaryDirectory + appName + "/" + fsPathComponentToString(fileName) +
		eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + fsPathComponentToString(extension) +
		eastl::string().sprintf("%u", target) + ".sb";
	PathHandle binaryShaderPath = fsCreatePath(fsGetSystemFileSystem(), binaryShaderComponent.c_str());
#else
	eastl::string binaryShaderComponent = fsPathComponentToString(fileName) +
		eastl::string().sprintf("_%zu", eastl::string_hash<eastl::string>()(shaderDefines)) + fsPathComponentToString(extension) +
		eastl::string().sprintf("%u", target) +
#ifdef DIRECT3D11
		eastl::string().sprintf("%u", pRenderer->mFeatureLevel) +
#endif
		".bin";

	PathHandle binaryShaderPath = fsGetPathInResourceDirEnum(RD_SHADER_BINARIES, binaryShaderComponent.c_str());
#endif

	// Shader source is newer than binary
	if (!check_for_byte_code(pRenderer, binaryShaderPath, timeStamp, pOut))
	{
		if (!sourceFileStream)
		{
			LOGF(eERROR, "No source shader or precompiled binary present for file %s", fileName.buffer);
			fsCloseStream(sourceFileStream);
			return false;
		}

#if defined(ORBIS)
		orbis_compileShader(pRenderer, target, stage, allStages, filePath, binaryShaderPath, macroCount, pMacros, pOut, pEntryPoint);
#elif defined(PROSPERO)
		prospero_compileShader(pRenderer, target, stage, allStages, filePath, binaryShaderPath, macroCount, pMacros, pOut, pEntryPoint);
#else
		if (pRenderer->mApi == RENDERER_API_METAL || pRenderer->mApi == RENDERER_API_VULKAN)
		{
#if defined(VULKAN)
#if defined(__ANDROID__)
			vk_compileShader(pRenderer, stage, (uint32_t)code.size(), code.c_str(), binaryShaderPath, macroCount, pMacros, pOut, pEntryPoint);
#else
			vk_compileShader(pRenderer, target, filePath, binaryShaderPath, macroCount, pMacros, pOut, pEntryPoint);
#endif
#elif defined(METAL)
			mtl_compileShader(pRenderer, metalShaderPath, binaryShaderPath, macroCount, pMacros, pOut, pEntryPoint);
#endif
		}
		else
		{
#if defined(DIRECT3D12) || defined(DIRECT3D11)
			compileShader(
				pRenderer, target, stage, filePath, (uint32_t)code.size(), code.c_str(),
				flags & SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID,
				macroCount, pMacros,
				pOut, pEntryPoint);

			if (!save_byte_code(binaryShaderPath, (char*)(pOut->pByteCode), pOut->mByteCodeSize))
			{
				const char* shaderName = fsGetPathFileName(filePath).buffer;
				LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", shaderName);
			}
#endif
		}
		if (!pOut->pByteCode)
		{
			LOGF(eERROR, "Error while generating bytecode for shader %s", fileName.buffer);
			fsCloseStream(sourceFileStream);
			ASSERT(false);
			return false;
		}
#endif
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
			ResourceDirEnum resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsGetResourceDirEnumPath(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirEnum(RD_SHADER_SOURCES));
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
			ResourceDirEnum resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsGetResourceDirEnumPath(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirEnum(RD_SHADER_SOURCES));
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
					pRenderer, pDesc->mTarget, stage, stages, filePath, pDesc->mStages[i].mFlags, macroCount, macros.data(),
					pStage, pDesc->mStages[i].pEntryPointName))
					return;

				binaryDesc.mStages |= stage;
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
			conf_free(pSources[i]);
		}
	}
#endif
#if !defined(PROSPERO)
	if (binaryDesc.mStages & SHADER_STAGE_VERT)
		conf_free(binaryDesc.mVert.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_FRAG)
		conf_free(binaryDesc.mFrag.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_COMP)
		conf_free(binaryDesc.mComp.pByteCode);
#if !defined(METAL)
	if (binaryDesc.mStages & SHADER_STAGE_TESC)
		conf_free(binaryDesc.mHull.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_TESE)
		conf_free(binaryDesc.mDomain.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_GEOM)
		conf_free(binaryDesc.mGeom.pByteCode);
	if (binaryDesc.mStages & SHADER_STAGE_RAYTRACING)
		conf_free(binaryDesc.mComp.pByteCode);
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
			ResourceDirEnum resourceDir = pDesc->mStages[i].mRoot;

			PathHandle resourceDirBasePath = fsGetResourceDirEnumPath(resourceDir);

			if (resourceDir != RD_SHADER_SOURCES && resourceDir != RD_ROOT) {
				resourceDirBasePath = fsAppendPathComponent(resourceDirBasePath, fsGetDefaultRelativePathForResourceDirEnum(RD_SHADER_SOURCES));
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
/************************************************************************/
// Pipeline cache save, load
/************************************************************************/
void addPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	FileStream* stream = fsOpenFile(pDesc->pPath, FM_READ_BINARY);
	ssize_t dataSize = 0;
	void* data = NULL;
	if (stream)
	{
		dataSize = fsGetStreamFileSize(stream);
		data = NULL;
		if (dataSize)
		{
			data = conf_malloc(dataSize);
			fsReadFromStream(stream, data, dataSize);
		}

		fsCloseStream(stream);
	}

	PipelineCacheDesc desc = {};
	desc.mFlags = pDesc->mFlags;
	desc.pData = data;
	desc.mSize = dataSize;
	addPipelineCache(pRenderer, &desc, ppPipelineCache);

	if (data)
	{
		conf_free(data);
	}
#endif
}

void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, const Path* pPath)
{
#if defined(DIRECT3D12) || defined(VULKAN)
	FileStream* stream = fsOpenFile(pPath, FM_WRITE_BINARY);
	if (stream)
	{
		size_t dataSize = 0;
		getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, NULL);
		if (dataSize)
		{
			void* data = conf_malloc(dataSize);
			getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, data);
			fsWriteToStream(stream, data, dataSize);
			conf_free(data);
		}

		fsCloseStream(stream);
	}
#endif
}
/************************************************************************/
/************************************************************************/
