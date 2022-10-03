/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Graphics/GraphicsConfig.h"

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#if defined(ENABLE_MESHOPTIMIZER)
#include "ThirdParty/OpenSource/meshoptimizer/src/meshoptimizer.h"
#endif

#define TINYKTX_IMPLEMENTATION
#include "ThirdParty/OpenSource/tinyktx/tinyktx.h"

#define TINYDDS_IMPLEMENTATION
#include "ThirdParty/OpenSource/tinydds/tinydds.h"

#include "../../Utilities/ThirdParty/OpenSource/basis_universal/transcoder/basisu_transcoder.h"

#define CGLTF_IMPLEMENTATION
#include "ThirdParty/OpenSource/cgltf/cgltf.h"

#include "../../Graphics/Interfaces/IGraphics.h"
#include "Interfaces/IResourceLoader.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"

#if defined(__ANDROID__) && defined(VULKAN)
#include <shaderc/shaderc.h>
#endif

#if defined(GLES)
#include "../../Graphics/OpenGLES/GLESContextCreator.h"
#endif

#include "TextureContainers.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifdef NX64
#include "../../Utilities/ThirdParty/OpenSource/murmurhash3/MurmurHash3_32.h"
#endif

struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
#if defined(DIRECT3D11) || defined(METAL) || defined(VULKAN)
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
#endif
};

#define MIP_REDUCE(s, mip) (max(1u, (uint32_t)((s) >> (mip))))

enum
{
	MAPPED_RANGE_FLAG_UNMAP_BUFFER = (1 << 0),
	MAPPED_RANGE_FLAG_TEMP_BUFFER = (1 << 1),
};

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(
	void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)

extern RendererApi gSelectedRendererApi;

/************************************************************************/
/************************************************************************/

// Xbox, Orbis, Prospero, iOS have unified memory
// so we dont need a command buffer to upload linear data
// A simple memcpy suffices since the GPU memory is marked as CPU write combine
#if defined(XBOX) || defined(ORBIS) || defined(PROSPERO) || defined(NX64)
static FORGE_CONSTEXPR const bool gUma = true;
#elif defined(ANDROID)
#if defined(USE_MULTIPLE_RENDER_APIS)
// Cant determine at compile time since we can be running GLES or VK. Not using UMA path for non VK
static bool gUma = false;
#elif defined(VULKAN)
static FORGE_CONSTEXPR const bool gUma = true;
#else
static FORGE_CONSTEXPR const bool gUma = false;
#endif
#else
static FORGE_CONSTEXPR const bool gUma = false;
#endif

#define MAX_FRAMES 3U

ResourceLoaderDesc gDefaultResourceLoaderDesc = { 8ull << 20, 2, false };
/************************************************************************/
// Surface Utils
/************************************************************************/
#if defined(VULKAN)
static inline ResourceState util_determine_resource_start_state(bool uav)
{
	if (uav)
		return RESOURCE_STATE_UNORDERED_ACCESS;
	else
		return RESOURCE_STATE_SHADER_RESOURCE;
}
#endif

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

static uint32_t gltfAttributeStride(const cgltf_attribute* const* attributes, ShaderSemantic semantic)
{
	const cgltf_size stride = attributes[semantic] ? attributes[semantic]->data->stride : 0;
	ASSERT(stride < UINT32_MAX);
	if (stride > UINT32_MAX)
	{
		LOGF(eERROR, "ShaderSemantic stride of this gltf_attribute is too big to store in uint32, value will be truncated");
		return UINT32_MAX;
	}
	return (uint32_t)stride;
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
		case cgltf_attribute_type_texcoord: return (ShaderSemantic)(SEMANTIC_TEXCOORD0 + index);
		default: return SEMANTIC_TEXCOORD0;
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
			else if (cgltf_component_type_r_8u == compType)
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
		default: return TinyImageFormat_UNDEFINED;
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
	uint32_t f32 = (*(uint32_t*)&val);
	uint16_t f16 = 0;
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

static inline void util_pack_float2_to_half2(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	struct f2
	{
		float x;
		float y;
	};
	
	ASSERT(srcStride == sizeof(f2));
	ASSERT(dstStride == sizeof(uint32_t));

	f2* f = (f2*)src;
	for (uint32_t e = 0; e < count; ++e)
	{
		*(uint32_t*)(dst + e * sizeof(uint32_t) + offset) =
			((util_float_to_half(f[e].x) & 0x0000FFFF) | ((util_float_to_half(f[e].y) << 16) & 0xFFFF0000));
	}
}

static inline uint32_t util_float2_to_unorm2x16(const float* v)
{
	uint32_t x = (uint32_t)round(clamp(v[0], 0, 1) * 65535.0f);
	uint32_t y = (uint32_t)round(clamp(v[1], 0, 1) * 65535.0f);
	return ((uint32_t)0x0000FFFF & x) | ((y << 16) & (uint32_t)0xFFFF0000);
}

#define OCT_WRAP(v, w) ((1.0f - abs((w))) * ((v) >= 0.0f ? 1.0f : -1.0f))

static inline void util_pack_float3_direction_to_half2(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	struct f3
	{
		float x;
		float y;
		float z;
	};
	ASSERT(dstStride == sizeof(uint32_t));
	for (uint32_t e = 0; e < count; ++e)
	{
		f3    f = *(f3*)(src + e * srcStride);
		float absLength = (abs(f.x) + abs(f.y) + abs(f.z));
		f3    enc = {};
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
			*(uint32_t*)(dst + e * dstStride + offset) = util_float2_to_unorm2x16(&enc.x);
		}
		else
		{
			*(uint32_t*)(dst + e * dstStride + offset) = 0;
		}
	}
}

static inline void util_unpack_uint8_to_uint16_joints(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst)
{
	ASSERT(srcStride == 4 && "Expecting stride of 4 (sizeof(uint8_t) * 4 joints)");

	for (uint32_t e = 0; e < count; ++e)
	{
		const uint8_t* srcBase = (const uint8_t*)(src + e * srcStride);
		uint16_t* dstBase = (uint16_t*)(dst + e * dstStride + offset);
		dstBase[0] = srcBase[0];
		dstBase[1] = srcBase[1];
		dstBase[2] = srcBase[2];
		dstBase[3] = srcBase[3];
	}
}

/************************************************************************/
// Internal Structures
/************************************************************************/
typedef void (*PreMipStepFn)(FileStream* pStream, uint32_t mip);

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
	Fence*   pFence = NULL;
	Cmd*     pCmd = NULL;
	CmdPool* pCmdPool = NULL;
	Buffer*  mBuffer = NULL;
	uint64_t mAllocatedSpace = 0;

	/// Buffers created in case we ran out of space in the original staging buffer
	/// Will be cleaned up after the fence for this set is complete
	/// stb_ds array of Buffer*
	Buffer** mTempBuffers = NULL;

	Semaphore* pCopyCompletedSemaphore = NULL;
} CopyResourceSet;

//Synchronization?
typedef struct CopyEngine
{
	Queue*           pQueue;
	CopyResourceSet* resourceSets;
	uint64_t         bufferSize;
	uint32_t         bufferCount;
	/// Node index in linked GPU mode, Renderer index in unlinked mode
	uint32_t         nodeIndex;
	bool             isRecording;
	Semaphore*       pLastCompletedSemaphore;

	/// For reading back GPU generated textures, we need to ensure writes have completed before performing the copy.
	/// stb_ds array of Semaphore*
	Semaphore** mWaitSemaphores;

} CopyEngine;

typedef enum UpdateRequestType
{
	UPDATE_REQUEST_UPDATE_BUFFER,
	UPDATE_REQUEST_UPDATE_TEXTURE,
	UPDATE_REQUEST_BUFFER_BARRIER,
	UPDATE_REQUEST_TEXTURE_BARRIER,
	UPDATE_REQUEST_LOAD_TEXTURE,
	UPDATE_REQUEST_LOAD_GEOMETRY,
	UPDATE_REQUEST_COPY_TEXTURE,
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
	UpdateRequest(const BufferUpdateDesc& buffer): mType(UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer) {}
	UpdateRequest(const TextureLoadDesc& texture): mType(UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
	UpdateRequest(const TextureUpdateDescInternal& texture): mType(UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture) {}
	UpdateRequest(const GeometryLoadDesc& geom): mType(UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
	UpdateRequest(const BufferBarrier& barrier): mType(UPDATE_REQUEST_BUFFER_BARRIER), bufferBarrier(barrier) {}
	UpdateRequest(const TextureBarrier& barrier): mType(UPDATE_REQUEST_TEXTURE_BARRIER), textureBarrier(barrier) {}
	UpdateRequest(const TextureCopyDesc& texture): mType(UPDATE_REQUEST_COPY_TEXTURE), texCopyDesc(texture) {}

	UpdateRequestType mType = UPDATE_REQUEST_INVALID;
	uint64_t          mWaitIndex = 0;
	Buffer*           pUploadBuffer = NULL;
	union
	{
		BufferUpdateDesc          bufUpdateDesc;
		TextureUpdateDescInternal texUpdateDesc;
		TextureLoadDesc           texLoadDesc;
		GeometryLoadDesc          geomLoadDesc;
		BufferBarrier             bufferBarrier;
		TextureBarrier            textureBarrier;
		TextureCopyDesc           texCopyDesc;
	};
};

struct ShaderByteCodeBuffer
{
	// Make sure we don't stack overflow
#if defined(NX64)
	static FORGE_CONSTEXPR const uint32_t kStackSize = THREAD_STACK_SIZE_NX / 2;
#elif defined(ORBIS)
	static FORGE_CONSTEXPR const uint32_t kStackSize = THREAD_STACK_SIZE_ORBIS / 2;
#else
	static FORGE_CONSTEXPR const uint32_t kStackSize = 128u * TF_KB;
#endif

	// Stack memory, no need to deallocate it. Used first, if a shader is too big we allocate heap memory
	void* pStackMemory;
	uint32_t mStackUsed;
};

struct ResourceLoader
{
	Renderer* ppRenderers[MAX_MULTIPLE_GPUS];
	uint32_t mGpuCount;

	ResourceLoaderDesc mDesc;

	volatile int mRun;
	ThreadHandle mThread;

	Mutex                        mQueueMutex;
	ConditionVariable            mQueueCond;
	Mutex                        mTokenMutex;
	ConditionVariable            mTokenCond;
	// array of stb_ds arrays
	UpdateRequest* mRequestQueue[MAX_MULTIPLE_GPUS];

	tfrg_atomic64_t mTokenCompleted;
	tfrg_atomic64_t mTokenSubmitted;
	tfrg_atomic64_t mTokenCounter;

	Mutex mSemaphoreMutex;

	SyncToken mCurrentTokenState[MAX_FRAMES];

	CopyEngine pCopyEngines[MAX_MULTIPLE_GPUS];
	uint32_t   mNextSet;
	uint32_t   mSubmittedSets;
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

#if !defined(PROSPERO)
static void* allocShaderByteCode(ShaderByteCodeBuffer* pShaderByteCodeBuffer, uint32_t alignment, uint32_t size, const char* filename)
{
	ASSERT(pShaderByteCodeBuffer && pShaderByteCodeBuffer->pStackMemory);
	ASSERT(alignment > 0);

	void* pBufferStart = (char*)pShaderByteCodeBuffer->pStackMemory + pShaderByteCodeBuffer->mStackUsed;

	uintptr_t offset = alignment - ((uintptr_t)pBufferStart % alignment);
	if (offset == alignment)
		offset = 0;

	void* pOutMemory = NULL;
	if (size + offset <= pShaderByteCodeBuffer->kStackSize - pShaderByteCodeBuffer->mStackUsed)
	{
		pShaderByteCodeBuffer->mStackUsed += (uint32_t)(size + offset);
		pOutMemory = (char*)pBufferStart + offset;
	}
	else
	{
		LOGF(eINFO, "Loading shader bytecode in heap memory (%s - %u bytes) (Stack total size: %u, Free size: %u)", filename, (uint32_t)(size + offset), pShaderByteCodeBuffer->kStackSize, pShaderByteCodeBuffer->kStackSize - pShaderByteCodeBuffer->mStackUsed);
		pOutMemory = tf_memalign(alignment, size);
	}

	ASSERT(((uintptr_t)pOutMemory % alignment) == 0);
	return pOutMemory;
}

static void freeShaderByteCode(ShaderByteCodeBuffer* pShaderByteCodeBuffer, BinaryShaderDesc* pBinaryShaderDesc)
{
	ASSERT(pShaderByteCodeBuffer && pBinaryShaderDesc);

	// Free bytecode if it's not allocated on the buffer
#define FREE_BYTECODE_IF_ON_HEAP(stage) \
	if(pShaderByteCodeBuffer->pStackMemory > stage.pByteCode || ((char*)pShaderByteCodeBuffer->pStackMemory + pShaderByteCodeBuffer->kStackSize) < stage.pByteCode) \
		tf_free(stage.pByteCode)

	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mVert);
	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mFrag);
	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mGeom);
	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mHull);
	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mDomain);
	FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mComp);

#undef FREE_BYTECODE_IF_ON_HEAP
}
#else
static void freeShaderByteCode(ShaderByteCodeBuffer*, BinaryShaderDesc*) {}
#endif

/************************************************************************/
// Internal Functions
/************************************************************************/
/// Return a new staging buffer
static MappedMemoryRange allocateUploadMemory(Renderer* pRenderer, uint64_t memoryRequirement, uint32_t alignment)
{
	Buffer* buffer;
#if defined(DIRECT3D11)
	if (gSelectedRendererApi == RENDERER_API_D3D11)
	{
		// There is no such thing as staging buffer in D3D11
		// To keep code paths unified in update functions, we allocate space for a dummy buffer and the system memory for pCpuMappedAddress
		buffer = (Buffer*)tf_memalign(alignof(Buffer), sizeof(Buffer) + (size_t)memoryRequirement);
		*buffer = {};
		buffer->pCpuMappedAddress = buffer + 1;
		buffer->mSize = memoryRequirement;
	}
	else
#endif
#if defined(GLES)
		if (gSelectedRendererApi == RENDERER_API_GLES)
	{
		// There is no such thing as staging buffer in D3D11
		// To keep code paths unified in update functions, we allocate space for a dummy buffer and the system memory for pCpuMappedAddress
		buffer = (Buffer*)tf_memalign(alignof(Buffer), sizeof(Buffer) + (size_t)memoryRequirement);
		*buffer = {};
		buffer->pCpuMappedAddress = buffer + 1;
		buffer->mSize = memoryRequirement;
	}
	else
#endif
	{
		//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
		buffer = {};
		BufferDesc bufferDesc = {};
		bufferDesc.mSize = memoryRequirement;
		bufferDesc.mAlignment = alignment;
		bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
		bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferDesc.mNodeIndex = pRenderer->mUnlinkedRendererIndex;
		addBuffer(pRenderer, &bufferDesc, &buffer);
	}
	return { (uint8_t*)buffer->pCpuMappedAddress, buffer, 0, memoryRequirement };
}

static void setupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine, uint32_t nodeIndex, uint64_t size, uint32_t bufferCount)
{
	QueueDesc desc = { QUEUE_TYPE_TRANSFER, QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, nodeIndex };
	addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

	const uint64_t maxBlockSize = 32;
	size = max(size, maxBlockSize);

	pCopyEngine->resourceSets = (CopyResourceSet*)tf_malloc(sizeof(CopyResourceSet) * bufferCount);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{
		tf_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
#if defined(DIRECT3D11)
		if (gSelectedRendererApi != RENDERER_API_D3D11)
#endif
			addFence(pRenderer, &resourceSet.pFence);

		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = pCopyEngine->pQueue;
		addCmdPool(pRenderer, &cmdPoolDesc, &resourceSet.pCmdPool);

		CmdDesc cmdDesc = {};
		cmdDesc.pPool = resourceSet.pCmdPool;
		addCmd(pRenderer, &cmdDesc, &resourceSet.pCmd);

		addSemaphore(pRenderer, &resourceSet.pCopyCompletedSemaphore);

		resourceSet.mBuffer = allocateUploadMemory(pRenderer, size, util_get_texture_subresource_alignment(pRenderer)).pBuffer;
	}

	pCopyEngine->bufferSize = size;
	pCopyEngine->bufferCount = bufferCount;
	pCopyEngine->nodeIndex = nodeIndex;
	pCopyEngine->isRecording = false;
	pCopyEngine->pLastCompletedSemaphore = NULL;
}

static void cleanupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
	for (uint32_t i = 0; i < pCopyEngine->bufferCount; ++i)
	{
		CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
		removeBuffer(pRenderer, resourceSet.mBuffer);

		removeSemaphore(pRenderer, resourceSet.pCopyCompletedSemaphore);

		removeCmd(pRenderer, resourceSet.pCmd);
		removeCmdPool(pRenderer, resourceSet.pCmdPool);
#if defined(DIRECT3D11)
		if (gSelectedRendererApi != RENDERER_API_D3D11)
#endif
			removeFence(pRenderer, resourceSet.pFence);

		if (arrlen(resourceSet.mTempBuffers))
			LOGF(eINFO, "Was not cleaned up %d", i);
		for (ptrdiff_t i = 0; i < arrlen(resourceSet.mTempBuffers); ++i)
		{
			removeBuffer(pRenderer, resourceSet.mTempBuffers[i]);
		}
		arrfree(resourceSet.mTempBuffers);
	}

	tf_free(pCopyEngine->resourceSets);
	arrfree(pCopyEngine->mWaitSemaphores);

	removeQueue(pRenderer, pCopyEngine->pQueue);
}

static bool waitCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, bool wait)
{
	ASSERT(!pCopyEngine->isRecording);
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	bool             completed = true;
#if defined(DIRECT3D11)
	if (gSelectedRendererApi != RENDERER_API_D3D11)
	{
#endif
		FenceStatus status;
		getFenceStatus(pRenderer, resourceSet.pFence, &status);
		completed = status != FENCE_STATUS_INCOMPLETE;
		if (wait && !completed)
		{
			waitForFences(pRenderer, 1, &resourceSet.pFence);
		}
#if defined(DIRECT3D11)
	}
	else
	{
		UNREF_PARAM(pRenderer);
		UNREF_PARAM(pCopyEngine);
		UNREF_PARAM(activeSet);
	}
#endif
	return completed;
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet)
{
	ASSERT(!pCopyEngine->isRecording);
	pCopyEngine->resourceSets[activeSet].mAllocatedSpace = 0;
	pCopyEngine->isRecording = false;

	Buffer** tempBuffers = pCopyEngine->resourceSets[activeSet].mTempBuffers;
	for (ptrdiff_t i = 0; i < arrlen(tempBuffers); ++i)
	{
		removeBuffer(pRenderer, tempBuffers[i]);
	}
	arrsetlen(tempBuffers, 0);
}

static Cmd* acquireCmd(CopyEngine* pCopyEngine, size_t activeSet)
{
	CopyResourceSet& resourceSet = pCopyEngine->resourceSets[activeSet];
	if (!pCopyEngine->isRecording)
	{
		resetCmdPool(pResourceLoader->ppRenderers[pCopyEngine->nodeIndex], resourceSet.pCmdPool);
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
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.ppSignalSemaphores = &resourceSet.pCopyCompletedSemaphore;
		submitDesc.pSignalFence = resourceSet.pFence;
		if (arrlen(pCopyEngine->mWaitSemaphores))
		{
			submitDesc.mWaitSemaphoreCount = (uint32_t)arrlen(pCopyEngine->mWaitSemaphores);
			submitDesc.ppWaitSemaphores = &pCopyEngine->mWaitSemaphores[0];
			arrsetlen(pCopyEngine->mWaitSemaphores, 0);
		}
		{
			queueSubmit(pCopyEngine->pQueue, &submitDesc);
		}
		pCopyEngine->isRecording = false;
	}
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(uint64_t memoryRequirement, uint32_t alignment, uint32_t nodeIndex)
{
	CopyEngine* pCopyEngine = &pResourceLoader->pCopyEngines[nodeIndex];

	uint64_t offset = pCopyEngine->resourceSets[pResourceLoader->mNextSet].mAllocatedSpace;
	if (alignment != 0)
	{
		offset = round_up_64(offset, alignment);
	}

	CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[pResourceLoader->mNextSet];
	uint64_t         size = (uint64_t)pResourceSet->mBuffer->mSize;
	bool             memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
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
			MappedMemoryRange range = allocateUploadMemory(pResourceLoader->ppRenderers[nodeIndex], memoryRequirement, alignment);
			//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
			arrpush(pResourceSet->mTempBuffers, range.pBuffer);
			return range;
		}
	}

	MappedMemoryRange range = allocateUploadMemory(pResourceLoader->ppRenderers[nodeIndex], memoryRequirement, alignment);
	//LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu", memoryRequirement, size);
	arrpush(pResourceSet->mTempBuffers, range.pBuffer);
	return range;
}

static void freeAllUploadMemory()
{
	for (size_t i = 0; i < MAX_MULTIPLE_GPUS; ++i)
	{
		for (ptrdiff_t j = 0; j < arrlen(pResourceLoader->mRequestQueue[i]); ++j)
		{
			UpdateRequest* pRequest = &pResourceLoader->mRequestQueue[i][j];
			if (pRequest->pUploadBuffer)
			{
				removeBuffer(pResourceLoader->ppRenderers[i], pRequest->pUploadBuffer);
			}
		}
	}
}

static UploadFunctionResult
	updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const TextureUpdateDescInternal& texUpdateDesc)
{
	// When this call comes from updateResource, staging buffer data is already filled
	// All that is left to do is record and execute the Copy commands
	bool                  dataAlreadyFilled = texUpdateDesc.mRange.pBuffer ? true : false;
	Texture*              texture = texUpdateDesc.pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
	FileStream            stream = texUpdateDesc.mStream;
	Cmd*                  cmd = acquireCmd(pCopyEngine, activeSet);

	ASSERT(pCopyEngine->pQueue->mNodeIndex == texUpdateDesc.pTexture->mNodeIndex);

	const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
	const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
	const uint64_t requiredSize = util_get_surface_size(
		fmt, texture->mWidth, texture->mHeight, texture->mDepth, rowAlignment, sliceAlignment, texUpdateDesc.mBaseMipLevel,
		texUpdateDesc.mMipLevels, texUpdateDesc.mBaseArrayLayer, texUpdateDesc.mLayerCount);

#if defined(VULKAN)
	TextureBarrier barrier;
	if (gSelectedRendererApi == RENDERER_API_VULKAN)
	{
		barrier = { texture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif
#if defined(SCARLETT)
	{
		// Workaround for Scarlett common state promotion issue.
		// Texture state is manually transitioned since textures don't properly decay to Common State after they are promoted.
		TextureBarrier barrier = { texture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif

	MappedMemoryRange upload = dataAlreadyFilled ? texUpdateDesc.mRange : allocateStagingMemory(requiredSize, sliceAlignment, texture->mNodeIndex);
	uint64_t          offset = 0;

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
	uint32_t firstEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels)
													  : (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount);
	uint32_t secondStart = texUpdateDesc.mMipsAfterSlice ? texUpdateDesc.mBaseArrayLayer : texUpdateDesc.mBaseMipLevel;
	uint32_t secondEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount)
													   : (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels);

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
	if (gSelectedRendererApi == RENDERER_API_VULKAN)
	{
		barrier = { texture, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif
#if defined(SCARLETT)
	{
		// Workaround for Scarlett common state promotion issue.
		// Texture state is manually transitioned since textures don't properly decay to Common State after they are promoted.
		TextureBarrier barrier = { texture, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_COMMON };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
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

	ASSERT((((pTextureDesc->mCreationFlag & TEXTURE_CREATION_FLAG_SRGB) == 0) ||
		    (pTextureDesc->pFileName != NULL)) &&
	       "Only textures loaded from file can have TEXTURE_CREATION_FLAG_SRGB. "
	       "Please change format of the provided texture if you need srgb format."
	);

	if (pTextureDesc->pFileName)
	{
		FileStream stream = {};
		char       fileName[FS_MAX_PATH] = {};
		bool       success = false;

		TextureUpdateDescInternal updateDesc = {};
		TextureContainerType      container = pTextureDesc->mContainer;
		static const char*        extensions[] = { NULL, "dds", "ktx", "gnf", "basis", "svt" };

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
		textureDesc.mFlags |= pTextureDesc->mCreationFlag;

		// Validate that we have found the file format now
		ASSERT(container != TEXTURE_CONTAINER_DEFAULT);    //-V547
		if (TEXTURE_CONTAINER_DEFAULT == container)        //-V547
		{
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		fsAppendPathExtension(pTextureDesc->pFileName, extensions[container], fileName);

		switch (container)
		{
			case TEXTURE_CONTAINER_DDS:
			{
#if defined(XBOX)
				success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream);
				uint32_t res = 1;
				if (success)
				{
					extern uint32_t loadXDDSTexture(
						Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags, Texture** ppTexture);
					res = loadXDDSTexture(pRenderer, &stream, fileName, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
					fsCloseStream(&stream);
				}

				return res ? UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : UPLOAD_FUNCTION_RESULT_COMPLETED;
#else
				success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream);
				if (success)
				{
					success = loadDDSTextureDesc(&stream, &textureDesc);
				}
#endif
				break;
			}
			case TEXTURE_CONTAINER_KTX:
			{
				success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream);
				if (success)
				{
					success = loadKTXTextureDesc(&stream, &textureDesc);
					updateDesc.mMipsAfterSlice = true;
					// KTX stores mip size before the mip data
					// This function gets called to skip the mip size so we read the mip data
					updateDesc.pPreMipFunc = [](FileStream* pStream, uint32_t) {
						uint32_t mipSize = 0;
						fsReadFromStream(pStream, &mipSize, sizeof(mipSize));
					};
				}
				break;
			}
			case TEXTURE_CONTAINER_BASIS:
			{
				void*    data = NULL;
				uint32_t dataSize = 0;
				success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream);
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
				success = fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream);
				uint32_t res = 1;
				if (success)
				{
					extern uint32_t loadGnfTexture(
						Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags, Texture** ppTexture);
					res = loadGnfTexture(pRenderer, &stream, fileName, pTextureDesc->mCreationFlag, pTextureDesc->ppTexture);
					fsCloseStream(&stream);
				}

				return res ? UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : UPLOAD_FUNCTION_RESULT_COMPLETED;
#endif
			}
			default: break;
		}

		if (success)
		{
			textureDesc.mStartState = RESOURCE_STATE_COMMON;
			textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;

			if (pTextureDesc->mCreationFlag & TEXTURE_CREATION_FLAG_SRGB)
			{
				TinyImageFormat srgbFormat = TinyImageFormat_ToSRGB(textureDesc.mFormat);
				if (srgbFormat != TinyImageFormat_UNDEFINED)
					textureDesc.mFormat = srgbFormat;
				else
				{
					LOGF(eWARNING,
						"Trying to load '%s' image using SRGB profile. "
						"But image has '%s' format, which doesn't have SRGB counterpart.",
						pTextureDesc->pFileName, TinyImageFormat_Name(textureDesc.mFormat));
				}
			}

#if defined(VULKAN)
			if (NULL != pTextureDesc->pDesc)
				textureDesc.pVkSamplerYcbcrConversionInfo = pTextureDesc->pDesc->pVkSamplerYcbcrConversionInfo;
#endif
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
		// Sparse Textures
		/************************************************************************/
#if defined(DIRECT3D12) || defined(VULKAN)
		if (TEXTURE_CONTAINER_SVT == container)
		{
			if (fsOpenStreamFromPath(RD_TEXTURES, fileName, FM_READ_BINARY, pTextureDesc->pFilePassword, &stream))
			{
				success = loadSVTTextureDesc(&stream, &textureDesc);
				if (success)
				{
					ssize_t dataSize = fsGetStreamFileSize(&stream) - fsGetStreamSeekPosition(&stream);
					void*   data = tf_malloc(dataSize);
					fsReadFromStream(&stream, data, dataSize);

					textureDesc.mStartState = RESOURCE_STATE_COPY_DEST;
					textureDesc.mFlags |= pTextureDesc->mCreationFlag;
					textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;
					if (pTextureDesc->mCreationFlag & TEXTURE_CREATION_FLAG_SRGB)
					{
						TinyImageFormat srgbFormat = TinyImageFormat_ToSRGB(textureDesc.mFormat);
						if (srgbFormat != TinyImageFormat_UNDEFINED)
							textureDesc.mFormat = srgbFormat;
						else
						{
							LOGF(eWARNING,
								"Trying to load '%s' image using SRGB profile. "
								"But image has '%s' format, which doesn't have SRGB counterpart.",
								pTextureDesc->pFileName, TinyImageFormat_Name(textureDesc.mFormat));
						}
					}

					addVirtualTexture(acquireCmd(pCopyEngine, activeSet), &textureDesc, pTextureDesc->ppTexture, data);

					fsCloseStream(&stream);

					if ((*pTextureDesc->ppTexture)->pSvt->mVirtualPageTotalCount == 0)
						return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

					return UPLOAD_FUNCTION_RESULT_COMPLETED;
				}
			}

			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
#endif
	}

	return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}

static UploadFunctionResult
	updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, const BufferUpdateDesc& bufUpdateDesc)
{
	ASSERT(pCopyEngine->pQueue->mNodeIndex == bufUpdateDesc.pBuffer->mNodeIndex);
	Buffer* pBuffer = bufUpdateDesc.pBuffer;
	ASSERT(pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY || pBuffer->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_TO_CPU);

	Cmd* pCmd = acquireCmd(pCopyEngine, activeSet);

	MappedMemoryRange range = bufUpdateDesc.mInternal.mMappedRange;
	cmdUpdateBuffer(pCmd, pBuffer, bufUpdateDesc.mDstOffset, range.pBuffer, range.mOffset, range.mSize);

	return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

#if defined(ENABLE_MESHOPTIMIZER)
static void 
geomOptimize(Geometry* geom, MeshOptimizerFlags optimizationFlags, BufferUpdateDesc indexUpdateDesc, BufferUpdateDesc* vertexUpdateDesc, uint32_t positionBinding, void* positionPointer)
{
	if (optimizationFlags == MESH_OPTIMIZATION_FLAG_OFF || !geom)
		return;

	size_t optimizerScratchSize = 128 * 1024 * 1024;
	size_t remapSize = (geom->mVertexCount * sizeof(uint32_t));
	size_t totalScratchSize = remapSize + optimizerScratchSize;

	//remap + optimizer scratch
	uint32_t* scratchMemory = (uint32_t*)tf_malloc(totalScratchSize);

	uint32_t* remap = scratchMemory;

	void* optimizerScratch = &scratchMemory[geom->mVertexCount];
	meshopt_SetScratchMemory(optimizerScratchSize, optimizerScratch);

	meshopt_Stream streams[MAX_VERTEX_BINDINGS];
	for (size_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
	{
		if (!geom->mVertexStrides[i])
			continue;

		streams[i].data = vertexUpdateDesc[i].pMappedData;
		streams[i].size = geom->mVertexStrides[i];
		streams[i].stride = geom->mVertexStrides[i];
	}

	//generating remap & new vertex/index sets
	if (geom->mIndexType == INDEX_TYPE_UINT16)
	{
		meshopt_generateVertexRemapMulti(remap, (uint16_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount, streams, geom->mVertexBufferCount);
		meshopt_remapIndexBuffer((uint16_t*)indexUpdateDesc.pMappedData, (uint16_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, remap);
	}
	else
	{
		meshopt_generateVertexRemapMulti(remap, (uint32_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount, streams, geom->mVertexBufferCount);
		meshopt_remapIndexBuffer((uint32_t*)indexUpdateDesc.pMappedData, (uint32_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, remap);
	}

	for (size_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
	{
		if (!geom->mVertexStrides[i])
			continue;

		meshopt_remapVertexBuffer(vertexUpdateDesc[i].pMappedData, vertexUpdateDesc[i].pMappedData, geom->mVertexCount, geom->mVertexStrides[i], remap);
	}

	//optimize
	//do we need to optmize per primitive ? 
	///optmizations like overdraw clearly can alter across primitives. but can vertex cache & vertex fetch do it ?
	if (geom->mIndexType == INDEX_TYPE_UINT16)
	{
		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXCACHE)
		{
			meshopt_optimizeVertexCache((uint16_t*)indexUpdateDesc.pMappedData, (uint16_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount);
		}

		//we can only run this if position data is not packed
		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_OVERDRAW && positionPointer)
		{
			for (size_t i = 0; i < geom->mDrawArgCount; i++)
			{
				uint16_t* src = &((uint16_t*)indexUpdateDesc.pMappedData)[geom->pDrawArgs[i].mStartIndex];

				const float kThreshold = 1.01f;
				meshopt_optimizeOverdraw(src, src, geom->pDrawArgs[i].mIndexCount, (float*)positionPointer, geom->mVertexCount, geom->mVertexStrides[positionBinding], kThreshold);
			}
		}

		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXFETCH)
		{
			meshopt_optimizeVertexFetchRemap(remap, (uint16_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount);
			meshopt_remapIndexBuffer((uint16_t*)indexUpdateDesc.pMappedData, (uint16_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, remap);

			for (size_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
			{
				if (!geom->mVertexStrides[i])
					continue;

				meshopt_remapVertexBuffer(vertexUpdateDesc[i].pMappedData, vertexUpdateDesc[i].pMappedData, geom->mVertexCount, geom->mVertexStrides[i], remap);
			}
		}
	}
	else
	{
		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXCACHE)
		{
			meshopt_optimizeVertexCache((uint32_t*)indexUpdateDesc.pMappedData, (uint32_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount);
		}

		//we can only run this if position data is not packed
		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_OVERDRAW && positionPointer)
		{
			for (size_t i = 0; i < geom->mDrawArgCount; i++)
			{
				uint32_t* src = &((uint32_t*)indexUpdateDesc.pMappedData)[geom->pDrawArgs[i].mStartIndex];

				const float kThreshold = 1.01f;
				meshopt_optimizeOverdraw(src, src, geom->pDrawArgs[i].mIndexCount, (float*)positionPointer, geom->mVertexCount, geom->mVertexStrides[positionBinding], kThreshold);
			}
		}

		if (optimizationFlags & MESH_OPTIMIZATION_FLAG_VERTEXFETCH)
		{
			meshopt_optimizeVertexFetchRemap(remap, (uint32_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, geom->mVertexCount);
			meshopt_remapIndexBuffer((uint32_t*)indexUpdateDesc.pMappedData, (uint32_t*)indexUpdateDesc.pMappedData, geom->mIndexCount, remap);

			for (size_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
			{
				if (!geom->mVertexStrides[i])
					continue;

				meshopt_remapVertexBuffer(vertexUpdateDesc[i].pMappedData, vertexUpdateDesc[i].pMappedData, geom->mVertexCount, geom->mVertexStrides[i], remap);
			}
		}
	}

	tf_free(scratchMemory);
}
#endif

static UploadFunctionResult loadGeometry(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, UpdateRequest& pGeometryLoad)
{
	Geometry* geom = NULL;
	GeometryLoadDesc* pDesc = &pGeometryLoad.geomLoadDesc;

	BufferUpdateDesc indexUpdateDesc = {};
	BufferUpdateDesc vertexUpdateDesc[MAX_VERTEX_BINDINGS] = {};
	
	//data for overdraw optimization
	uint32_t positionBinding = 0;
	void* positionPointer = NULL;

	char iext[FS_MAX_PATH] = { 0 };
	fsGetPathExtension(pDesc->pFileName, iext);

	// Geometry in a custom binary file
	if (iext[0] != 0 && stricmp(iext, "bin") == 0)
	{
		FileStream file = {};
		if (!fsOpenStreamFromPath(RD_MESHES, pDesc->pFileName, FM_READ_BINARY, pDesc->pFilePassword, &file))
		{
			LOGF(eERROR, "Failed to open bin file %s", pDesc->pFileName);
			ASSERT(false);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		char magic[TF_ARRAY_COUNT(GEOMETRY_FILE_MAGIC_STR)] = { 0 };
		COMPILE_ASSERT(sizeof(magic) == sizeof(GEOMETRY_FILE_MAGIC_STR));
		fsReadFromStream(&file, magic, sizeof(magic));
		
		if (strncmp(magic, GEOMETRY_FILE_MAGIC_STR, TF_ARRAY_COUNT(magic)) != 0)
		{
			LOGF(eERROR, "File '%s' is not a Geometry file.", pDesc->pFileName);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		uint32_t geomSize = 0;
		fsReadFromStream(&file, &geomSize, sizeof(uint32_t));
		ASSERT(geomSize > 0);
		if (geomSize == 0)
		{
			LOGF(eERROR, "File '%s': Geometry object must have a size greater than 0.", pDesc->pFileName);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		geom = (Geometry*)tf_calloc(1, geomSize);
		ASSERT(geom);
		
		fsReadFromStream(&file, geom, geomSize);
		if (!geom)
		{
			LOGF(eERROR, "File '%s': Geometry object is a nullptr.", pDesc->pFileName);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);    //-V1027
		geom->pInverseBindPoses = (mat4*)((uint8_t*)geom->pDrawArgs + round_up(geom->mDrawArgCount * sizeof(*geom->pDrawArgs), 16));
		geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(geom->mJointCount * sizeof(*geom->pInverseBindPoses), 16));
		
		// Determine index stride
		const uint32_t indexStride = geom->mVertexCount > UINT16_MAX ? sizeof(uint32_t) : sizeof(uint16_t);
		
		uint32_t shadowSize = 0;
		fsReadFromStream(&file, &shadowSize, sizeof(uint32_t));
		ASSERT(shadowSize > 0);
		if (shadowSize == 0)
		{
			LOGF(eERROR, "File '%s': Geometry object must have a shadow size greater than 0.", pDesc->pFileName);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		geom->pShadow = (Geometry::ShadowData*)tf_calloc(1, shadowSize);
		
		fsReadFromStream(&file, geom->pShadow, shadowSize);
		ASSERT(geom->pShadow);
		if (!geom->pShadow)
		{
			LOGF(eERROR, "File '%s': Geometry object's shadow is a nullptr.", pDesc->pFileName);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}
		
		geom->pShadow->pIndices = geom->pShadow + 1;
		geom->pShadow->pAttributes[SEMANTIC_POSITION] = (uint8_t*)geom->pShadow->pIndices + (geom->mIndexCount * indexStride);
		geom->pShadow->pAttributes[SEMANTIC_NORMAL] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] +
			(geom->mVertexStrides[SEMANTIC_POSITION] * geom->mVertexCount);
		geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_NORMAL] +
			(geom->mVertexStrides[SEMANTIC_NORMAL] * geom->mVertexCount);
		
		fsCloseStream(&file);
		
		uint32_t         vertexStrides[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexAttribCount[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexOffsets[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexBindings[SEMANTIC_TEXCOORD9 + 1] = {};
		for (uint32_t i = 0; i < SEMANTIC_TEXCOORD9 + 1; ++i)
			vertexOffsets[i] = UINT_MAX;

		uint32_t defaultTexcoordSemantic = SEMANTIC_UNDEFINED;
		uint32_t defaultTexcoordStride = 0;
		
		
		// Determine vertex stride for each binding
		for (uint32_t i = 0; i < pDesc->pVertexLayout->mAttribCount; ++i)
		{
			const VertexAttrib* attr = &pDesc->pVertexLayout->mAttribs[i];

			bool defaultTexcoords = (attr->mSemantic >= SEMANTIC_TEXCOORD0 && attr->mSemantic <= SEMANTIC_TEXCOORD9);

			const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock(attr->mFormat) >> 3;

			if (defaultTexcoords)
			{
				// Make sure there are only 1 set of default texcoords
				ASSERT(defaultTexcoordSemantic == SEMANTIC_UNDEFINED);

				defaultTexcoordStride = dstFormatSize ? dstFormatSize : 2 * sizeof(float);    // (0.f, 0.f)
				defaultTexcoordSemantic = attr->mSemantic;

				vertexStrides[attr->mBinding] += defaultTexcoordStride;
				vertexOffsets[defaultTexcoordSemantic] = attr->mOffset;
				vertexBindings[defaultTexcoordSemantic] = attr->mBinding;
				++vertexAttribCount[attr->mBinding];

				continue;
			}
			
			const uint32_t srcFormatSize = (uint32_t)geom->mVertexStrides[attr->mSemantic];    //-V522

			vertexStrides[attr->mBinding] += dstFormatSize ? dstFormatSize : srcFormatSize;
			vertexOffsets[attr->mSemantic] = attr->mOffset;
			vertexBindings[attr->mSemantic] = attr->mBinding;
			++vertexAttribCount[attr->mBinding];
		}
		
		// Allocate buffer memory
		const bool structuredBuffers = (pDesc->mFlags & GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS);

		// Index buffer
		BufferDesc indexBufferDesc = {};
		indexBufferDesc.mDescriptors =
			DESCRIPTOR_TYPE_INDEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
				: (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
		indexBufferDesc.mSize = indexStride * geom->mIndexCount;
		indexBufferDesc.mElementCount = indexBufferDesc.mSize / (structuredBuffers ? indexStride : sizeof(uint32_t));
		indexBufferDesc.mStructStride = indexStride;
		indexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mStartState = gUma ? RESOURCE_STATE_INDEX_BUFFER : indexBufferDesc.mStartState;
		addBuffer(pRenderer, &indexBufferDesc, &geom->pIndexBuffer);

		indexUpdateDesc.mSize = geom->mIndexCount * indexStride;
		indexUpdateDesc.pBuffer = geom->pIndexBuffer;
		if (gUma)
		{
			indexUpdateDesc.mInternal.mMappedRange = { (uint8_t*)geom->pIndexBuffer->pCpuMappedAddress };
		}
		else
		{
			indexUpdateDesc.mInternal.mMappedRange = allocateStagingMemory(indexUpdateDesc.mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
		}
		indexUpdateDesc.pMappedData = indexUpdateDesc.mInternal.mMappedRange.pData;

		// Vertex buffers
		uint32_t bufferCounter = 0;
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (!vertexStrides[i])
				continue;

			BufferDesc vertexBufferDesc = {};
			vertexBufferDesc.mDescriptors =
				DESCRIPTOR_TYPE_VERTEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
					: (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
			vertexBufferDesc.mSize = vertexStrides[i] * geom->mVertexCount;
			vertexBufferDesc.mElementCount = vertexBufferDesc.mSize / (structuredBuffers ? vertexStrides[i] : sizeof(uint32_t));
			vertexBufferDesc.mStructStride = vertexStrides[i];
			vertexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vertexBufferDesc.mStartState = gUma ? RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : vertexBufferDesc.mStartState;
			addBuffer(pRenderer, &vertexBufferDesc, &geom->pVertexBuffers[bufferCounter]);

			geom->mVertexStrides[bufferCounter] = vertexStrides[i];

			vertexUpdateDesc[i].pBuffer = geom->pVertexBuffers[bufferCounter];
			vertexUpdateDesc[i].mSize = vertexBufferDesc.mSize;
			if (gUma)
			{
				vertexUpdateDesc[i].mInternal.mMappedRange = { (uint8_t*)geom->pVertexBuffers[bufferCounter]->pCpuMappedAddress, 0 };
			}
			else
			{
				vertexUpdateDesc[i].mInternal.mMappedRange = allocateStagingMemory(vertexUpdateDesc[i].mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
			}
			vertexUpdateDesc[i].pMappedData = vertexUpdateDesc[i].mInternal.mMappedRange.pData;
			++bufferCounter;
		}
		
		geom->mVertexBufferCount = bufferCounter;
		
		memcpy(indexUpdateDesc.pMappedData, geom->pShadow->pIndices, indexStride * geom->mIndexCount);
		
		for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBS; ++i)
		{
			if (!geom->pShadow->pAttributes[i])
				continue;
			
			const uint32_t binding = vertexBindings[i];
			const uint32_t offset = vertexOffsets[i];
			const uint32_t stride = vertexStrides[binding];
			
			const uint8_t* src =
				(uint8_t*)geom->pShadow->pAttributes[i];
			
			// If this vertex attribute is not interleaved with any other attribute use fast path instead of copying one by one
			// In this case a simple memcpy will be enough to transfer the data to the buffer
			if (1 == vertexAttribCount[binding])
			{
				uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData;
				
				memcpy(dst, src, geom->mVertexStrides[i] * geom->mVertexCount);
			}
			else
			{
				uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData;
				// Loop through all vertices copying into the correct place in the vertex buffer
				// Example:
				// [ POSITION | NORMAL | TEXCOORD ] => [ 0 | 12 | 24 ], [ 32 | 44 | 52 ], ... (vertex stride of 32 => 12 + 12 + 8)
				
				for (uint32_t e = 0; e < geom->mVertexCount; ++e)
						memcpy(dst + e * stride + offset, src + e * geom->mVertexStrides[i], geom->mVertexStrides[i]);
			}
		}

		// If the user doesn't want the shadowed data we don't need it any more
		if ((pDesc->mFlags & GEOMETRY_LOAD_FLAG_SHADOWED) != GEOMETRY_LOAD_FLAG_SHADOWED)
		{
			tf_free(geom->pShadow);
			geom->pShadow = nullptr;
		}
		
		tf_free(pDesc->pVertexLayout);
		*pDesc->ppGeometry = geom;
	}
	// Geometry in gltf container
	else if (iext[0] != 0 && (stricmp(iext, "gltf") == 0 || stricmp(iext, "glb") == 0))
	{
		FileStream file = {};
		if (!fsOpenStreamFromPath(RD_MESHES, pDesc->pFileName, FM_READ_BINARY, pDesc->pFilePassword, &file))
		{
			LOGF(eERROR, "Failed to open gltf file %s", pDesc->pFileName);
			ASSERT(false);
			return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
		}

		ssize_t fileSize = fsGetStreamFileSize(&file);
		void* fileData = tf_malloc(fileSize);

		fsReadFromStream(&file, fileData, fileSize);

		cgltf_options options = {};
		cgltf_data* data = NULL;
		options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
		options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
		cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
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
				if (fsOpenStreamFromPath(RD_MESHES, path, FM_READ_BINARY, pDesc->pFilePassword, &fs))
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

		typedef void (*PackingFunction)(uint32_t count, uint32_t srcStride, uint32_t dstStride, uint32_t offset, const uint8_t* src, uint8_t* dst);

		uint32_t         vertexStrides[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexAttribCount[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexOffsets[SEMANTIC_TEXCOORD9 + 1] = {};
		uint32_t         vertexBindings[SEMANTIC_TEXCOORD9 + 1] = {};
		cgltf_attribute* vertexAttribs[SEMANTIC_TEXCOORD9 + 1] = {};
		PackingFunction  vertexPacking[SEMANTIC_TEXCOORD9 + 1] = {};
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

				for (uint32_t j = 0; j < prim->attributes_count; ++j)
				{
					if (prim->attributes[j].data->count != prim->attributes[0].data->count)
					{
						LOGF(eERROR, "Mismatched vertex attribute count for %s, attribute index %d", pDesc->pFileName, j);
					}
					vertexAttribs[util_cgltf_attrib_type_to_semantic(prim->attributes[j].type, prim->attributes[j].index)] = &prim->attributes[j];
				}
			}
		}

		uint32_t defaultTexcoordSemantic = SEMANTIC_UNDEFINED;
		uint32_t defaultTexcoordStride = 0;

		// Determine vertex stride for each binding
		for (uint32_t i = 0; i < pDesc->pVertexLayout->mAttribCount; ++i)
		{
			const VertexAttrib* attr = &pDesc->pVertexLayout->mAttribs[i];
			const cgltf_attribute* cgltfAttr = vertexAttribs[attr->mSemantic];

			bool defaultTexcoords = !cgltfAttr && (attr->mSemantic >= SEMANTIC_TEXCOORD0 && attr->mSemantic <= SEMANTIC_TEXCOORD9);
			ASSERT(cgltfAttr || defaultTexcoords);

			const uint32_t dstFormatSize = TinyImageFormat_BitSizeOfBlock(attr->mFormat) >> 3;

			if (defaultTexcoords)
			{
				// Make sure there are only 1 set of default texcoords
				ASSERT(defaultTexcoordSemantic == SEMANTIC_UNDEFINED);

				defaultTexcoordStride = dstFormatSize ? dstFormatSize : 2 * sizeof(float);    // (0.f, 0.f)
				defaultTexcoordSemantic = attr->mSemantic;

				vertexStrides[attr->mBinding] += defaultTexcoordStride;
				vertexOffsets[defaultTexcoordSemantic] = attr->mOffset;
				vertexBindings[defaultTexcoordSemantic] = attr->mBinding;
				++vertexAttribCount[attr->mBinding];

				continue;
			}

			const uint32_t srcFormatSize = (uint32_t)cgltfAttr->data->stride;    //-V522

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
				case cgltf_attribute_type_joints:
				{
					if (srcFormatSize == sizeof(uint8_t) * 4 && dstFormatSize == sizeof(uint16_t) * 4)
						vertexPacking[attr->mSemantic] = util_unpack_uint8_to_uint16_joints;
					else
					{
						LOGF(eERROR, "Joint size doesn't match");
						ASSERT(false);
					}
					break;
				}
				default: break;
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

		geom = (Geometry*)tf_calloc(1, totalSize);
		ASSERT(geom);

		geom->pDrawArgs = (IndirectDrawIndexArguments*)(geom + 1);    //-V1027
		geom->pInverseBindPoses = (mat4*)((uint8_t*)geom->pDrawArgs + round_up(drawCount * sizeof(*geom->pDrawArgs), 16));
		geom->pJointRemaps = (uint32_t*)((uint8_t*)geom->pInverseBindPoses + round_up(jointCount * sizeof(*geom->pInverseBindPoses), 16));

		uint32_t shadowSize = 0;
		if (pDesc->mFlags & GEOMETRY_LOAD_FLAG_SHADOWED)
		{
			shadowSize += sizeof(Geometry::ShadowData);
			shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_POSITION) * vertexCount;
			shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_NORMAL) * vertexCount;
			shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_TEXCOORD0) * vertexCount;
			shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_JOINTS) * vertexCount; 
			shadowSize += gltfAttributeStride(vertexAttribs, SEMANTIC_WEIGHTS) * vertexCount;
			shadowSize += indexCount * indexStride;

			geom->pShadow = (Geometry::ShadowData*)tf_calloc(1, shadowSize);
			geom->pShadow->pIndices = geom->pShadow + 1;
			geom->pShadow->pAttributes[SEMANTIC_POSITION] = (uint8_t*)geom->pShadow->pIndices + (indexCount * indexStride);
			geom->pShadow->pAttributes[SEMANTIC_NORMAL] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] + gltfAttributeStride(vertexAttribs, SEMANTIC_POSITION) * vertexCount;
			geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_NORMAL] + gltfAttributeStride(vertexAttribs, SEMANTIC_NORMAL) * vertexCount;
			geom->pShadow->pAttributes[SEMANTIC_JOINTS] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] + gltfAttributeStride(vertexAttribs, SEMANTIC_TEXCOORD0) * vertexCount; 
			geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_JOINTS] + gltfAttributeStride(vertexAttribs, SEMANTIC_JOINTS) * vertexCount;
			ASSERT(((const char*)geom->pShadow) + shadowSize == ((char*)geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] + gltfAttributeStride(vertexAttribs, SEMANTIC_WEIGHTS) * vertexCount));
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
		indexBufferDesc.mDescriptors =
			DESCRIPTOR_TYPE_INDEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
				: (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
		indexBufferDesc.mSize = indexStride * indexCount;
		indexBufferDesc.mElementCount = indexBufferDesc.mSize / (structuredBuffers ? indexStride : sizeof(uint32_t));
		indexBufferDesc.mStructStride = indexStride;
		indexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mStartState = gUma ? RESOURCE_STATE_INDEX_BUFFER : indexBufferDesc.mStartState;
		addBuffer(pRenderer, &indexBufferDesc, &geom->pIndexBuffer);

		indexUpdateDesc.mSize = indexCount * indexStride;
		indexUpdateDesc.pBuffer = geom->pIndexBuffer;
		if (gUma)
		{
			indexUpdateDesc.mInternal.mMappedRange = { (uint8_t*)geom->pIndexBuffer->pCpuMappedAddress };
		}
		else
		{
			indexUpdateDesc.mInternal.mMappedRange = allocateStagingMemory(indexUpdateDesc.mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
		}
		indexUpdateDesc.pMappedData = indexUpdateDesc.mInternal.mMappedRange.pData;

		uint32_t bufferCounter = 0;
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (!vertexStrides[i])
				continue;

			BufferDesc vertexBufferDesc = {};
			vertexBufferDesc.mDescriptors =
				DESCRIPTOR_TYPE_VERTEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
					: (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
			vertexBufferDesc.mSize = vertexStrides[i] * vertexCount;
			vertexBufferDesc.mElementCount = vertexBufferDesc.mSize / (structuredBuffers ? vertexStrides[i] : sizeof(uint32_t));
			vertexBufferDesc.mStructStride = vertexStrides[i];
			vertexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vertexBufferDesc.mStartState = gUma ? RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER : vertexBufferDesc.mStartState;
			addBuffer(pRenderer, &vertexBufferDesc, &geom->pVertexBuffers[bufferCounter]);

			geom->mVertexStrides[bufferCounter] = vertexStrides[i];

			vertexUpdateDesc[i].pBuffer = geom->pVertexBuffers[bufferCounter];
			vertexUpdateDesc[i].mSize = vertexBufferDesc.mSize;
			if (gUma)
			{
				vertexUpdateDesc[i].mInternal.mMappedRange = { (uint8_t*)geom->pVertexBuffers[bufferCounter]->pCpuMappedAddress, 0 };
			}
			else
			{
				vertexUpdateDesc[i].mInternal.mMappedRange = allocateStagingMemory(vertexUpdateDesc[i].mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
			}
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
					uint32_t         index = util_cgltf_attrib_type_to_semantic(attr->type, attr->index);

					if (vertexOffsets[index] != UINT_MAX)
					{
						const uint32_t binding = vertexBindings[index];
						const uint32_t offset = vertexOffsets[index];
						const uint32_t stride = vertexStrides[binding];
						const uint8_t* src =
							(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;

						// If this vertex attribute is not interleaved with any other attribute use fast path instead of copying one by one
						// In this case a simple memcpy will be enough to transfer the data to the buffer
						if (1 == vertexAttribCount[binding])
						{
							uint8_t* dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
							if (vertexPacking[index])
								vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, stride, 0, src, dst);
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
								vertexPacking[index]((uint32_t)attr->data->count, (uint32_t)attr->data->stride, stride, offset, src, dst);
							else
								for (uint32_t e = 0; e < attr->data->count; ++e)
									memcpy(dst + e * stride + offset, src + e * attr->data->stride, attr->data->stride);
						}
					}
				}

				// If used, set default texcoords in buffer to (0.f, 0.f) - assume copy engine does not give us zero'd memory
				if (defaultTexcoordSemantic != SEMANTIC_UNDEFINED)
				{
					const uint32_t binding = vertexBindings[defaultTexcoordSemantic];
					const uint32_t offset = vertexOffsets[defaultTexcoordSemantic];
					const uint32_t stride = vertexStrides[binding];

					uint8_t*       dst = (uint8_t*)vertexUpdateDesc[binding].pMappedData + vertexCount * stride;
					const uint32_t count = (uint32_t)prim->attributes[0].data->count;

					for (uint32_t i = 0; i < count; ++i)
						memset(dst + i * stride + offset, 0, defaultTexcoordStride);
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

		// Load the remap joint indices generated in the offline process
		uint32_t remapCount = 0;
		for (uint32_t i = 0; i < data->skins_count; ++i)
		{
			const cgltf_skin* skin = &data->skins[i];
			uint32_t          extrasSize = (uint32_t)(skin->extras.end_offset - skin->extras.start_offset);
			if (extrasSize)
			{
				const char* jointRemaps = (const char*)data->json + skin->extras.start_offset;
				jsmn_parser parser = {};
				jsmntok_t*  tokens = (jsmntok_t*)tf_malloc((skin->joints_count + 1) * sizeof(jsmntok_t));
				jsmn_parse(&parser, (const char*)jointRemaps, extrasSize, tokens, skin->joints_count + 1);
				ASSERT(tokens[0].size == (int)skin->joints_count + 1);
				cgltf_accessor_unpack_floats(
					skin->inverse_bind_matrices, (cgltf_float*)geom->pInverseBindPoses,
					skin->joints_count * sizeof(float[16]) / sizeof(float));
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
			uint32_t    extrasSize = (uint32_t)(data->asset.extras.end_offset - data->asset.extras.start_offset);
			const char* json = data->json + data->asset.extras.start_offset;
			jsmn_parser parser = {};
			jsmntok_t   tokens[5] = {};
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
							const uint8_t* src =
								(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_POSITION] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
						else if (cgltf_attribute_type_normal == attr->type)
						{
							const uint8_t* src =
								(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_NORMAL] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
						else if (cgltf_attribute_type_texcoord == attr->type)
						{
							const uint8_t* src =
								(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_TEXCOORD0] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
						else if (cgltf_attribute_type_joints == attr->type)
						{
							const uint8_t* src =
								(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_JOINTS] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
						else if (cgltf_attribute_type_weights == attr->type)
						{
							const uint8_t* src =
								(uint8_t*)attr->data->buffer_view->buffer->data + attr->data->offset + attr->data->buffer_view->offset;
							uint8_t* dst = (uint8_t*)geom->pShadow->pAttributes[SEMANTIC_WEIGHTS] + vertexCount * attr->data->stride;
							memcpy(dst, src, attr->data->count * attr->data->stride);
						}
					}

					indexCount += (uint32_t)prim->indices->count;
					vertexCount += (uint32_t)prim->attributes->data->count;
				}
			}
		}

		//fill the vertexPointer only if the final layout is a float
		for (size_t i = 0; i < pDesc->pVertexLayout->mAttribCount; i++)
		{
			TinyImageFormat v = pDesc->pVertexLayout->mAttribs[i].mFormat;

			if (pDesc->pVertexLayout->mAttribs[i].mSemantic == SEMANTIC_POSITION &&
				(v == TinyImageFormat_R32G32B32_SFLOAT || v == TinyImageFormat_R32G32B32A32_SFLOAT))
			{
				positionBinding = vertexBindings[SEMANTIC_POSITION];
				positionPointer = (char*)vertexUpdateDesc[positionBinding].pMappedData + vertexOffsets[SEMANTIC_POSITION];
			}
		}

		data->file_data = fileData;
		cgltf_free(data);

		tf_free(pDesc->pVertexLayout);

		*pDesc->ppGeometry = geom;
	}

	// Optmize mesh
#if defined(ENABLE_MESHOPTIMIZER)
	geomOptimize(geom, pDesc->mOptimizationFlags,indexUpdateDesc, vertexUpdateDesc, positionBinding, positionPointer);
#endif

	// Upload mesh
	UploadFunctionResult uploadResult = UPLOAD_FUNCTION_RESULT_COMPLETED;
	if (!gUma)
	{
		uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, indexUpdateDesc);

		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
		{
			if (vertexUpdateDesc[i].pMappedData)
			{
				uploadResult = updateBuffer(pRenderer, pCopyEngine, activeSet, vertexUpdateDesc[i]);
			}
		}
	}
	
	return uploadResult;
}

static UploadFunctionResult copyTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, size_t activeSet, TextureCopyDesc& pTextureCopy)
{
	Texture* texture = pTextureCopy.pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;

	Cmd* cmd = acquireCmd(pCopyEngine, activeSet);

	if (pTextureCopy.pWaitSemaphore)
		arrpush(pCopyEngine->mWaitSemaphores, pTextureCopy.pWaitSemaphore);

#if defined(VULKAN)
	if (gSelectedRendererApi == RENDERER_API_VULKAN)
	{
		TextureBarrier barrier = { texture, pTextureCopy.mTextureState, RESOURCE_STATE_COPY_SOURCE };
		barrier.mAcquire = 1;
		barrier.mQueueType = pTextureCopy.mQueueType;
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif
#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		TextureBarrier barrier = { texture, pTextureCopy.mTextureState, RESOURCE_STATE_COPY_SOURCE };
		barrier.mAcquire = 1;
		barrier.mQueueType = pTextureCopy.mQueueType;
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif

	uint32_t numBytes = 0;
	uint32_t rowBytes = 0;
	uint32_t numRows = 0;

	bool ret = util_get_surface_info(texture->mWidth, texture->mHeight, fmt, &numBytes, &rowBytes, &numRows);
	if (!ret)
	{
		return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
	}

	SubresourceDataDesc subresourceDesc = {};
	subresourceDesc.mArrayLayer = pTextureCopy.mTextureArrayLayer;
	subresourceDesc.mMipLevel = pTextureCopy.mTextureMipLevel;
	subresourceDesc.mSrcOffset = pTextureCopy.mBufferOffset;
#if defined(DIRECT3D11) || defined(METAL) || defined(VULKAN)
	const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
	const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
	uint32_t subRowPitch = round_up(rowBytes, rowAlignment);
	uint32_t subSlicePitch = round_up(subRowPitch * numRows, sliceAlignment);
	subresourceDesc.mRowPitch = subRowPitch;
	subresourceDesc.mSlicePitch = subSlicePitch;
#endif
	cmdCopySubresource(cmd, pTextureCopy.pBuffer, pTextureCopy.pTexture, &subresourceDesc);
#if defined(DIRECT3D12)
	if (gSelectedRendererApi == RENDERER_API_D3D12)
	{
		TextureBarrier barrier = { texture, RESOURCE_STATE_COPY_SOURCE, pTextureCopy.mTextureState };
		barrier.mRelease = 1;
		barrier.mQueueType = pTextureCopy.mQueueType;
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
	}
#endif

	return UPLOAD_FUNCTION_RESULT_COMPLETED;

}
/************************************************************************/
// Internal Resource Loader Implementation
/************************************************************************/
static bool areTasksAvailable(ResourceLoader* pLoader)
{
	for (size_t i = 0; i < MAX_MULTIPLE_GPUS; ++i)
	{
		if (arrlen(pLoader->mRequestQueue[i]))
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

#if defined(GLES)
	GLContext localContext;
	if (gSelectedRendererApi == RENDERER_API_GLES)
	{
		if (!pLoader->mDesc.mSingleThreaded)
			initGLContext(pLoader->ppRenderers[0]->mGLES.pConfig, &localContext, pLoader->ppRenderers[0]->mGLES.pContext);
	}
#endif

	SyncToken maxToken = {};

	while (pLoader->mRun)
	{
		acquireMutex(&pLoader->mQueueMutex);

		// Check for pending tokens
		// Safe to use mTokenCounter as we are inside critical section
		bool allTokensSignaled = (pLoader->mTokenCompleted == tfrg_atomic64_load_relaxed(&pLoader->mTokenCounter));

		while (!areTasksAvailable(pLoader) && allTokensSignaled && pLoader->mRun)
		{
			// No waiting if not running dedicated resource loader thread.
			if (pLoader->mDesc.mSingleThreaded)
			{
				releaseMutex(&pLoader->mQueueMutex);
				return;
			}
			// Sleep until someone adds an update request to the queue
			waitConditionVariable(&pLoader->mQueueCond, &pLoader->mQueueMutex, TIMEOUT_INFINITE);
		}

		releaseMutex(&pLoader->mQueueMutex);

		pLoader->mNextSet = (pLoader->mNextSet + 1) % pLoader->mDesc.mBufferCount;
		for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
		{
			waitCopyEngineSet(pLoader->ppRenderers[nodeIndex], &pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet, true);
			resetCopyEngineSet(pLoader->ppRenderers[nodeIndex], &pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
		}

		// Signal pending tokens from previous frames
		acquireMutex(&pLoader->mTokenMutex);
		tfrg_atomic64_store_release(&pLoader->mTokenCompleted, pLoader->mCurrentTokenState[pLoader->mNextSet]);
		releaseMutex(&pLoader->mTokenMutex);
		wakeAllConditionVariable(&pLoader->mTokenCond);

		uint64_t completionMask = 0;

		for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
		{
			acquireMutex(&pLoader->mQueueMutex);

			UpdateRequest**	pRequestQueue	= &pLoader->mRequestQueue[nodeIndex];
			CopyEngine*		pCopyEngine		= &pLoader->pCopyEngines[nodeIndex];

			if (!arrlen(*pRequestQueue))
			{
				releaseMutex(&pLoader->mQueueMutex);
				continue;
			}

			UpdateRequest* activeQueue = *pRequestQueue;
			*pRequestQueue = NULL;
			releaseMutex(&pLoader->mQueueMutex);

			Renderer* pRenderer = pLoader->ppRenderers[nodeIndex];
			SyncToken maxNodeToken = {};

			for (ptrdiff_t j = 0; j < arrlen(activeQueue); ++j)
			{
				UpdateRequest updateState = activeQueue[j];

				UploadFunctionResult result = UPLOAD_FUNCTION_RESULT_COMPLETED;
				switch (updateState.mType)
				{
					case UPDATE_REQUEST_UPDATE_BUFFER:
						result = updateBuffer(pRenderer, pCopyEngine, pLoader->mNextSet, updateState.bufUpdateDesc);
						break;
					case UPDATE_REQUEST_UPDATE_TEXTURE:
						result = updateTexture(pRenderer, pCopyEngine, pLoader->mNextSet, updateState.texUpdateDesc);
						break;
					case UPDATE_REQUEST_BUFFER_BARRIER:
						cmdResourceBarrier(acquireCmd(pCopyEngine, pLoader->mNextSet), 1, &updateState.bufferBarrier, 0, NULL, 0, NULL);
						result = UPLOAD_FUNCTION_RESULT_COMPLETED;
						break;
					case UPDATE_REQUEST_TEXTURE_BARRIER:
						cmdResourceBarrier(acquireCmd(pCopyEngine, pLoader->mNextSet), 0, NULL, 1, &updateState.textureBarrier, 0, NULL);
						result = UPLOAD_FUNCTION_RESULT_COMPLETED;
						break;
					case UPDATE_REQUEST_LOAD_TEXTURE:
						result = loadTexture(pRenderer, pCopyEngine, pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_LOAD_GEOMETRY:
						result = loadGeometry(pRenderer, pCopyEngine, pLoader->mNextSet, updateState);
						break;
					case UPDATE_REQUEST_COPY_TEXTURE:
						result = copyTexture(pRenderer, pCopyEngine, pLoader->mNextSet, updateState.texCopyDesc);
						break;
					case UPDATE_REQUEST_INVALID: break;
				}

				if (updateState.pUploadBuffer)
				{
					CopyResourceSet& resourceSet = pCopyEngine->resourceSets[pLoader->mNextSet];
					arrpush(resourceSet.mTempBuffers, updateState.pUploadBuffer);
				}

				bool completed = result == UPLOAD_FUNCTION_RESULT_COMPLETED || result == UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

				completionMask |= (uint64_t)completed << nodeIndex;

				if (updateState.mWaitIndex && completed)
				{
					ASSERT(maxNodeToken < updateState.mWaitIndex);
					maxNodeToken = updateState.mWaitIndex;
				}

				ASSERT(result != UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL);
			}

			arrfree(activeQueue);
			maxToken = max(maxToken, maxNodeToken);
		}

		if (completionMask != 0)
		{
			for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
			{
				if (completionMask & ((uint64_t)1 << nodeIndex))
				{
					streamerFlush(&pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
					acquireMutex(&pLoader->mSemaphoreMutex);
					pLoader->pCopyEngines[nodeIndex].pLastCompletedSemaphore =
						pLoader->pCopyEngines[nodeIndex].resourceSets[pLoader->mNextSet].pCopyCompletedSemaphore;
					releaseMutex(&pLoader->mSemaphoreMutex);
				}
			}

		}

		SyncToken nextToken = max(maxToken, getLastTokenCompleted());
		pLoader->mCurrentTokenState[pLoader->mNextSet] = nextToken;

		// Signal submitted tokens
		acquireMutex(&pLoader->mTokenMutex);
		tfrg_atomic64_store_release(&pLoader->mTokenSubmitted, pLoader->mCurrentTokenState[pLoader->mNextSet]);
		releaseMutex(&pLoader->mTokenMutex);
		wakeAllConditionVariable(&pLoader->mTokenCond);

		if (pResourceLoader->mDesc.mSingleThreaded)
		{
			return;
		}
	}

	for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
	{
		streamerFlush(&pLoader->pCopyEngines[nodeIndex], pLoader->mNextSet);
#if defined(DIRECT3D11)
		if (gSelectedRendererApi != RENDERER_API_D3D11)
#endif
#if defined(GLES)
			if (gSelectedRendererApi != RENDERER_API_GLES)
#endif
			{
				waitQueueIdle(pLoader->pCopyEngines[nodeIndex].pQueue);
			}
		cleanupCopyEngine(pLoader->ppRenderers[nodeIndex], &pLoader->pCopyEngines[nodeIndex]);
	}

	freeAllUploadMemory();

#if defined(GLES)
	if (gSelectedRendererApi == RENDERER_API_GLES)
	{
		if (!pResourceLoader->mDesc.mSingleThreaded)
			removeGLContext(&localContext);
	}
#endif
}

static void addResourceLoader(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
{
	ASSERT(rendererCount > 0);
	ASSERT(rendererCount <= MAX_MULTIPLE_GPUS);

	if (!pDesc)
		pDesc = &gDefaultResourceLoaderDesc;

	ResourceLoader* pLoader = tf_new(ResourceLoader);

	uint32_t gpuCount = rendererCount;
	if (ppRenderers[0]->mGpuMode != GPU_MODE_UNLINKED)
	{
		ASSERT(rendererCount == 1);
		gpuCount = ppRenderers[0]->mLinkedNodeCount;
	}

	pLoader->mGpuCount = gpuCount;

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		ASSERT(rendererCount == 1 || ppRenderers[i]->mGpuMode == GPU_MODE_UNLINKED);
		// Replicate single renderer in linked mode, for uniform handling of linked and unlinked multi gpu.
		pLoader->ppRenderers[i] = (rendererCount > 1) ? ppRenderers[i] : ppRenderers[0];
	}

	pLoader->mRun = true;    //-V601
	pLoader->mDesc = *pDesc;

	initMutex(&pLoader->mQueueMutex);
	initMutex(&pLoader->mTokenMutex);
	initConditionVariable(&pLoader->mQueueCond);
	initConditionVariable(&pLoader->mTokenCond);
	initMutex(&pLoader->mSemaphoreMutex);

	pLoader->mTokenCounter = 0;
	pLoader->mTokenCompleted = 0;
	pLoader->mTokenSubmitted = 0;

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		setupCopyEngine(pLoader->ppRenderers[i], &pLoader->pCopyEngines[i], i, pLoader->mDesc.mBufferSize, pLoader->mDesc.mBufferCount);
	}

	ThreadDesc threadDesc = {};
	threadDesc.pFunc = streamerThreadFunc;
	threadDesc.pData = pLoader;
	strncpy(threadDesc.mThreadName, "ResourceLoaderTask", sizeof(threadDesc.mThreadName));

#if defined(NX64)
	threadDesc.mHasAffinityMask = true;
	threadDesc.mAffinityMask = 1;
#endif

#if defined(DIRECT3D11)
	if (gSelectedRendererApi == RENDERER_API_D3D11)
		pLoader->mDesc.mSingleThreaded = true;
#endif

#if defined(ANDROID) && defined(USE_MULTIPLE_RENDER_APIS)
	gUma = gSelectedRendererApi == RENDERER_API_VULKAN;
#endif

	// Create dedicated resource loader thread.
	if (!pLoader->mDesc.mSingleThreaded)
	{
		initThread(&threadDesc, &pLoader->mThread);
	}

	*ppLoader = pLoader;
}

static void removeResourceLoader(ResourceLoader* pLoader)
{
	pLoader->mRun = false;    //-V601

	if (pLoader->mDesc.mSingleThreaded)
	{
		streamerThreadFunc(pLoader);
	}
	else
	{
		wakeOneConditionVariable(&pLoader->mQueueCond);
		joinThread(pLoader->mThread);
	}

	destroyConditionVariable(&pLoader->mQueueCond);
	destroyConditionVariable(&pLoader->mTokenCond);
	destroyMutex(&pLoader->mQueueMutex);
	destroyMutex(&pLoader->mTokenMutex);
	destroyMutex(&pLoader->mSemaphoreMutex);

	tf_delete(pLoader);
}

static void queueBufferUpdate(ResourceLoader* pLoader, BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	uint32_t nodeIndex = pBufferUpdate->pBuffer->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pBufferUpdate));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
	{
		pLastRequest->mWaitIndex = t;
		pLastRequest->pUploadBuffer = (pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER)
																	 ? pBufferUpdate->mInternal.mMappedRange.pBuffer
																	 : NULL;
	}

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

static void queueTextureLoad(ResourceLoader* pLoader, TextureLoadDesc* pTextureUpdate, SyncToken* token)
{
	uint32_t nodeIndex = pTextureUpdate->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pTextureUpdate));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
		pLastRequest->mWaitIndex = t;

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

static void queueGeometryLoad(ResourceLoader* pLoader, GeometryLoadDesc* pGeometryLoad, SyncToken* token)
{
	uint32_t nodeIndex = pGeometryLoad->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pGeometryLoad));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
		pLastRequest->mWaitIndex = t;

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

static void queueTextureUpdate(ResourceLoader* pLoader, TextureUpdateDescInternal* pTextureUpdate, SyncToken* token)
{
	ASSERT(pTextureUpdate->mRange.pBuffer);

	uint32_t nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pTextureUpdate));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
	{
		pLastRequest->mWaitIndex = t;
		pLastRequest->pUploadBuffer =
			(pTextureUpdate->mRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER) ? pTextureUpdate->mRange.pBuffer : NULL;
	}

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

#if defined(VULKAN)
static void queueBufferBarrier(ResourceLoader* pLoader, Buffer* pBuffer, ResourceState state, SyncToken* token)
{
	uint32_t nodeIndex = pBuffer->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest( BufferBarrier{ pBuffer, RESOURCE_STATE_UNDEFINED, state } ));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
		pLastRequest->mWaitIndex = t;

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

static void queueTextureBarrier(ResourceLoader* pLoader, Texture* pTexture, ResourceState state, SyncToken* token)
{
	uint32_t nodeIndex = pTexture->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(TextureBarrier{ pTexture, RESOURCE_STATE_UNDEFINED, state } ));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
		pLastRequest->mWaitIndex = t;

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}
#endif

static void queueTextureCopy(ResourceLoader* pLoader, TextureCopyDesc* pTextureCopy, SyncToken* token)
{
	ASSERT(pTextureCopy->pTexture->mNodeIndex == pTextureCopy->pBuffer->mNodeIndex);
	uint32_t nodeIndex = pTextureCopy->pTexture->mNodeIndex;
	acquireMutex(&pLoader->mQueueMutex);

	SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

	arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pTextureCopy));
	UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
	if (pLastRequest)
		pLastRequest->mWaitIndex = t;

	releaseMutex(&pLoader->mQueueMutex);
	wakeOneConditionVariable(&pLoader->mQueueCond);
	if (token)
		*token = max(t, *token);
}

static void waitForToken(ResourceLoader* pLoader, const SyncToken* token)
{
	if (pLoader->mDesc.mSingleThreaded)
	{
		return;
	}
	acquireMutex(&pLoader->mTokenMutex);
	while (!isTokenCompleted(token))
	{
		waitConditionVariable(&pLoader->mTokenCond, &pLoader->mTokenMutex, TIMEOUT_INFINITE);
	}
	releaseMutex(&pLoader->mTokenMutex);
}

static void waitForTokenSubmitted(ResourceLoader* pLoader, const SyncToken* token)
{
	if (pLoader->mDesc.mSingleThreaded)
	{
		return;
	}
	acquireMutex(&pLoader->mTokenMutex);
	while (!isTokenSubmitted(token))
	{
		waitConditionVariable(&pLoader->mTokenCond, &pLoader->mTokenMutex, TIMEOUT_INFINITE);
	}
	releaseMutex(&pLoader->mTokenMutex);
}
/************************************************************************/
// Resource Loader Interface Implementation
/************************************************************************/
void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc) { addResourceLoader(&pRenderer, 1, pDesc, &pResourceLoader); }

void exitResourceLoaderInterface(Renderer* pRenderer) 
{
	removeResourceLoader(pResourceLoader);
}

void initResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc)
{
	addResourceLoader(ppRenderers, rendererCount, pDesc, &pResourceLoader);
}

void exitResourceLoaderInterface(Renderer** pRenderers, uint32_t rendererCount)
{
	removeResourceLoader(pResourceLoader);
}

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token)
{
	uint64_t stagingBufferSize = pResourceLoader->pCopyEngines[0].bufferSize;
	bool     update = pBufferDesc->pData || pBufferDesc->mForceReset;

	ASSERT(stagingBufferSize > 0);
	if (RESOURCE_MEMORY_USAGE_GPU_ONLY == pBufferDesc->mDesc.mMemoryUsage && !pBufferDesc->mDesc.mStartState && (!update || gUma))
	{
		pBufferDesc->mDesc.mStartState = util_determine_resource_start_state(&pBufferDesc->mDesc);
		LOGF(
			eWARNING, "Buffer start state not provided. Determined the start state as (%u) based on the provided BufferDesc",
			(uint32_t)pBufferDesc->mDesc.mStartState);
	}

	if (pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY && update && !gUma)
	{
		pBufferDesc->mDesc.mStartState = RESOURCE_STATE_COMMON;
	}
	addBuffer(pResourceLoader->ppRenderers[pBufferDesc->mDesc.mNodeIndex], &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

	if (update)
	{
		if (!gUma && pBufferDesc->mDesc.mSize > stagingBufferSize && pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
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
					ASSERT(data);
					memcpy(updateDesc.pMappedData, (char*)data + offset, chunkSize);    //-V769
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
				ASSERT(!pBufferDesc->mDesc.mSize || pBufferDesc->pData);
				if (pBufferDesc->pData)
					memcpy(updateDesc.pMappedData, pBufferDesc->pData, (size_t)pBufferDesc->mDesc.mSize);
			}
			endUpdateResource(&updateDesc, token);
		}
	}
	else
	{
		// Transition GPU buffer to desired state for Vulkan since all Vulkan resources are created in undefined state
#if defined(VULKAN)
		if (gSelectedRendererApi == RENDERER_API_VULKAN && pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY &&
			// Check whether this is required (user specified a state other than undefined / common)
			(pBufferDesc->mDesc.mStartState != RESOURCE_STATE_UNDEFINED && pBufferDesc->mDesc.mStartState != RESOURCE_STATE_COMMON))
			queueBufferBarrier(pResourceLoader, *pBufferDesc->ppBuffer, pBufferDesc->mDesc.mStartState, token);
#endif
	}
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token)
{
	ASSERT(pTextureDesc->ppTexture);

	if (!pTextureDesc->pFileName && pTextureDesc->pDesc)
	{
		ASSERT(pTextureDesc->pDesc->mStartState);

		// If texture is supposed to be filled later (UAV / Update later / ...) proceed with the mStartState provided by the user in the texture description
		addTexture(pResourceLoader->ppRenderers[pTextureDesc->mNodeIndex], pTextureDesc->pDesc, pTextureDesc->ppTexture);

		// Transition texture to desired state for Vulkan since all Vulkan resources are created in undefined state
#if defined(VULKAN)
		if (gSelectedRendererApi == RENDERER_API_VULKAN)
		{
			ResourceState startState = pTextureDesc->pDesc->mStartState;
			// Check whether this is required (user specified a state other than undefined / common)
			if (startState == RESOURCE_STATE_UNDEFINED || startState == RESOURCE_STATE_COMMON)    //-V560
			{
				startState = util_determine_resource_start_state(pTextureDesc->pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE);
			}
			queueTextureBarrier(pResourceLoader, *pTextureDesc->ppTexture, startState, token);
		}
#endif
	}
	else
	{
		TextureLoadDesc updateDesc = *pTextureDesc;
		queueTextureLoad(pResourceLoader, &updateDesc, token);
		if (pResourceLoader->mDesc.mSingleThreaded)
		{
			streamerThreadFunc(pResourceLoader);
		}
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
	if (pResourceLoader->mDesc.mSingleThreaded)
	{
		streamerThreadFunc(pResourceLoader);
	}
}

void removeResource(Buffer* pBuffer)
{
	removeBuffer(pResourceLoader->ppRenderers[pBuffer->mNodeIndex], pBuffer);
}

void removeResource(Texture* pTexture)
{
	removeTexture(pResourceLoader->ppRenderers[pTexture->mNodeIndex], pTexture);
}

void removeResource(Geometry* pGeom)
{
	removeResource(pGeom->pIndexBuffer);

	for (uint32_t i = 0; i < pGeom->mVertexBufferCount; ++i)
		removeResource(pGeom->pVertexBuffers[i]);

	removeGeometryShadowData(pGeom);

	tf_free(pGeom);
}

void removeGeometryShadowData(Geometry* pGeom)
{
	if (pGeom->pShadow)
	{
		tf_free(pGeom->pShadow);
		pGeom->pShadow = nullptr;
	}
}

void beginUpdateResource(BufferUpdateDesc* pBufferUpdate)
{
	Buffer* pBuffer = pBufferUpdate->pBuffer;
	ASSERT(pBuffer);

	uint64_t size = pBufferUpdate->mSize > 0 ? pBufferUpdate->mSize : (pBufferUpdate->pBuffer->mSize - pBufferUpdate->mDstOffset);
	ASSERT(pBufferUpdate->mDstOffset + size <= pBuffer->mSize);

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (gUma || memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		bool map = !pBuffer->pCpuMappedAddress;
		if (map)
		{
			mapBuffer(pResourceLoader->ppRenderers[pBuffer->mNodeIndex], pBuffer, NULL);
		}

		pBufferUpdate->mInternal.mMappedRange = { (uint8_t*)pBuffer->pCpuMappedAddress + pBufferUpdate->mDstOffset, pBuffer };
		pBufferUpdate->pMappedData = pBufferUpdate->mInternal.mMappedRange.pData;
		pBufferUpdate->mInternal.mMappedRange.mFlags = map ? MAPPED_RANGE_FLAG_UNMAP_BUFFER : 0;
	}
	else
	{
		// We need to use a staging buffer.
		MappedMemoryRange range = allocateUploadMemory(pResourceLoader->ppRenderers[pBuffer->mNodeIndex], size, RESOURCE_BUFFER_ALIGNMENT);
		pBufferUpdate->pMappedData = range.pData;

		pBufferUpdate->mInternal.mMappedRange = range;
		pBufferUpdate->mInternal.mMappedRange.mFlags = MAPPED_RANGE_FLAG_TEMP_BUFFER;
	}
}

void endUpdateResource(BufferUpdateDesc* pBufferUpdate, SyncToken* token)
{
	if (pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_UNMAP_BUFFER)
	{
		unmapBuffer(pResourceLoader->ppRenderers[pBufferUpdate->pBuffer->mNodeIndex], pBufferUpdate->pBuffer);
	}

	ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
	if (!gUma && memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
	{
		queueBufferUpdate(pResourceLoader, pBufferUpdate, token);
	}

	// Restore the state to before the beginUpdateResource call.
	pBufferUpdate->pMappedData = NULL;
	pBufferUpdate->mInternal = {};
	if (pResourceLoader->mDesc.mSingleThreaded)
	{
		streamerThreadFunc(pResourceLoader);
	}
}

void beginUpdateResource(TextureUpdateDesc* pTextureUpdate)
{
	const Texture*        texture = pTextureUpdate->pTexture;
	const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
	Renderer*             pRenderer = pResourceLoader->ppRenderers[texture->mNodeIndex];
	const uint32_t        alignment = util_get_texture_subresource_alignment(pRenderer, fmt);

	bool success = util_get_surface_info(
		MIP_REDUCE(texture->mWidth, pTextureUpdate->mMipLevel), MIP_REDUCE(texture->mHeight, pTextureUpdate->mMipLevel), fmt,
		&pTextureUpdate->mSrcSliceStride, &pTextureUpdate->mSrcRowStride, &pTextureUpdate->mRowCount);
	ASSERT(success);
	UNREF_PARAM(success);

	pTextureUpdate->mDstRowStride = round_up(pTextureUpdate->mSrcRowStride, util_get_texture_row_alignment(pRenderer));
	pTextureUpdate->mDstSliceStride = round_up(pTextureUpdate->mDstRowStride * pTextureUpdate->mRowCount, alignment);

	const ssize_t requiredSize = round_up(
		MIP_REDUCE(texture->mDepth, pTextureUpdate->mMipLevel) * pTextureUpdate->mDstSliceStride, alignment);

	// We need to use a staging buffer.
	pTextureUpdate->mInternal.mMappedRange = allocateUploadMemory(pRenderer, requiredSize, alignment);
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
	if (pResourceLoader->mDesc.mSingleThreaded)
	{
		streamerThreadFunc(pResourceLoader);
	}
}

void copyResource(TextureCopyDesc* pTextureDesc, SyncToken* token)
{
	queueTextureCopy(pResourceLoader, pTextureDesc, token);
	if (pResourceLoader->mDesc.mSingleThreaded)
	{
		streamerThreadFunc(pResourceLoader);
	}
}

SyncToken getLastTokenCompleted() { return tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted); }

bool isTokenCompleted(const SyncToken* token) { return *token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted); }

void waitForToken(const SyncToken* token) { waitForToken(pResourceLoader, token); }

SyncToken getLastTokenSubmitted() { return tfrg_atomic64_load_acquire(&pResourceLoader->mTokenSubmitted); }

bool isTokenSubmitted(const SyncToken* token) { return *token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenSubmitted); }

void waitForTokenSubmitted(const SyncToken* token) { waitForTokenSubmitted(pResourceLoader, token); }

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

bool isResourceLoaderSingleThreaded()
{
	ASSERT(pResourceLoader);
	return pResourceLoader->mDesc.mSingleThreaded;
}

Semaphore* getLastSemaphoreCompleted(uint32_t nodeIndex)
{
	acquireMutex(&pResourceLoader->mSemaphoreMutex);
	Semaphore* sem = pResourceLoader->pCopyEngines[nodeIndex].pLastCompletedSemaphore;
	releaseMutex(&pResourceLoader->mSemaphoreMutex);
	return sem;
}

/************************************************************************/
// Shader loading
/************************************************************************/
#if defined(__ANDROID__) && defined(VULKAN)
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
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, ShaderByteCodeBuffer* pShaderByteCodeBuffer, const char* pEntryPoint)
{
	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(pRenderer->mVulkan.pVkActiveGPU, &properties);
	uint32_t version = properties.apiVersion < TARGET_VULKAN_API_VERSION ? properties.apiVersion : TARGET_VULKAN_API_VERSION;
	switch (version) {
	case VK_API_VERSION_1_0:
		version = shaderc_env_version_vulkan_1_0;
		break;
	case VK_API_VERSION_1_1:
		version = shaderc_env_version_vulkan_1_1;
		break;
	default:
		LOGF(eERROR, "Unknown Vulkan version %u", version);
		ASSERT(false);
		version = shaderc_env_version_vulkan_1_0;
		break;
	}

	// compile into spir-V shader
	shaderc_compiler_t        compiler = shaderc_compiler_initialize();
	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		shaderc_compile_options_add_macro_definition(
			options, pMacros[i].definition, strlen(pMacros[i].definition), pMacros[i].value, strlen(pMacros[i].value));
	}

	const char* android_definition = "TARGET_ANDROID";
	shaderc_compile_options_add_macro_definition(options, android_definition, strlen(android_definition), "1", 1);

#if defined(QUEST_VR)
    const char* quest_definition = "TARGET_QUEST";
    shaderc_compile_options_add_macro_definition(options, quest_definition, strlen(quest_definition), "1", 1);
#endif

	shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, version);

	shaderc_compilation_result_t spvShader = shaderc_compile_into_spv(
		compiler, code, codeSize, getShadercShaderType(stage), "shaderc_error", pEntryPoint ? pEntryPoint : "main", options);
	shaderc_compilation_status spvStatus = shaderc_result_get_compilation_status(spvShader);
	if (spvStatus != shaderc_compilation_status_success)
	{
		const char* errorMessage = shaderc_result_get_error_message(spvShader);
		LOGF(LogLevel::eERROR, "Shader compiling failed! with status %s", errorMessage);
		abort();
	}

	// Resize the byteCode block based on the compiled shader size
	pOut->mByteCodeSize = shaderc_result_get_length(spvShader);
	pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 1, pOut->mByteCodeSize, "");
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
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, ShaderByteCodeBuffer* pShaderByteCodeBuffer, const char* pEntryPoint)
{
	bstring commandLine = bempty();
	balloc(&commandLine, 512);
	char          filePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, filePath);
	char outFilePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), outFile, outFilePath);

	bformata(&commandLine, "-V \"%s\" -o \"%s\"", filePath, outFilePath);

	if (target >= shader_target_6_0)
		bcatliteral(&commandLine, " --target-env vulkan1.1 ");

	if (target >= shader_target_6_3)
		bcatliteral(&commandLine, " --target-env spirv1.4");

	if (pEntryPoint != NULL)
		bformata(&commandLine, " -e %s", pEntryPoint);

		// Add platform macro
#ifdef _WINDOWS
	bcatliteral(&commandLine, " \"-D WINDOWS\"");
#elif defined(__ANDROID__)
	bcatliteral(&commandLine, " \"-D ANDROID\"");
#elif defined(__linux__)
	bcatliteral(&commandLine, " \"-D LINUX\"");
#endif

	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		bformata(&commandLine, " \"-D%s=%s\"", pMacros[i].definition, pMacros[i].value);
	}

	const char* vulkanSdkStr = getenv("VULKAN_SDK");
	char*       glslangValidator = nullptr;

	if (vulkanSdkStr)
	{
		glslangValidator = (char*)tf_calloc(strlen(vulkanSdkStr) + 64, sizeof(char));
		strcpy(glslangValidator, vulkanSdkStr);
		strcat(glslangValidator, "/bin/glslangValidator");
	}
	else
	{
		glslangValidator = (char*)tf_calloc(64, sizeof(char));
		strcpy(glslangValidator, "/usr/bin/glslangValidator");
	}

	const char* args[1] = { (const char*)commandLine.data };

	char logFileName[FS_MAX_PATH] = { 0 };
	fsGetPathFileName(outFile, logFileName);
	strcat(logFileName, "_compile.log");
	char logFilePath[FS_MAX_PATH] = { 0 };
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), logFileName, logFilePath);

	if (systemRun(glslangValidator, args, 1, logFilePath) == 0)
	{
		FileStream fh = {};
		bool       success = fsOpenStreamFromPath(RD_SHADER_BINARIES, outFile, FM_READ_BINARY, NULL, &fh);
		//Check if the File Handle exists
		ASSERT(success);
		pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&fh);
		pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 1, pOut->mByteCodeSize, fileName);
		fsReadFromStream(&fh, pOut->pByteCode, pOut->mByteCodeSize);
		fsCloseStream(&fh);
	}
	else
	{
		FileStream fh = {};
		// If for some reason the error file could not be created just log error msg
		if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, logFileName, FM_READ_BINARY, NULL, &fh))
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
	bdestroy(&commandLine);

	tf_free(glslangValidator);
}
#endif
#elif defined(METAL)
// On Metal, on the other hand, we can compile from code into a MTLLibrary, but cannot save this
// object's bytecode to disk. We instead use the xcbuild bash tool to compile the shaders.
void mtl_compileShader(
	Renderer* pRenderer, const char* fileName, const char* outFile, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut,
	ShaderByteCodeBuffer* pShaderByteCodeBuffer, const char* /*pEntryPoint*/)
{
	char filePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_SOURCES), fileName, filePath);
	char outFilePath[FS_MAX_PATH] = {};
	fsAppendPathComponent(fsGetResourceDirectory(RD_SHADER_BINARIES), outFile, outFilePath);
	char intermediateFilePath[FS_MAX_PATH] = {};
	fsAppendPathExtension(outFilePath, "air", intermediateFilePath);

	const char* xcrun = "/usr/bin/xcrun";
	const char** args = NULL;
	arrsetcap(args, 9 + 2 * macroCount);

	// Compile the source into a temporary .air file.
	arrpush(args, "-sdk");
	arrpush(args, "macosx");
	arrpush(args, "metal");
	arrpush(args, "-c");
	arrpush(args, filePath);
	arrpush(args, "-o");
	arrpush(args, intermediateFilePath);

	//enable the 2 below for shader debugging on xcode
	//args.push_back("-MO");
	//args.push_back("-gline-tables-only");
	arrpush(args, "-D");
	arrpush(args, "MTL_SHADER=1");    // Add MTL_SHADER macro to differentiate structs in headers shared by app/shader code.

	bstring* macros = NULL;
	arrsetcap(macros, macroCount);

	// Add user defined macros to the command line
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		bstring macro = bempty();
		bformat(&macro, "%s=%s", pMacros[i].definition, pMacros[i].value);
		arrpush(macros, macro);
		
		arrpush(args, "-D");
		arrpush(args, (const char*)macro.data);
	}

	if (systemRun(xcrun, &args[0], arrlenu(args), NULL) == 0)
	{
		// Create a .metallib file from the .air file.
		arrsetlen(args, 0);
		arrpush(args, "-sdk");
		arrpush(args, "macosx");
		arrpush(args, "metallib");
		arrpush(args, intermediateFilePath);
		arrpush(args, "-o");
		arrpush(args, outFilePath);

		if (systemRun(xcrun, &args[0], arrlenu(args), NULL) == 0)
		{
			// Remove the temp .air file.
			const char* nativePath = intermediateFilePath;
			systemRun("rm", &nativePath, 1, NULL);

			// Store the compiled bytecode.
			FileStream fHandle = {};
			if (fsOpenStreamFromPath(RD_SHADER_BINARIES, outFile, FM_READ_BINARY, NULL, &fHandle))
			{
				pOut->mByteCodeSize = (uint32_t)fsGetStreamFileSize(&fHandle);
				pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 1, pOut->mByteCodeSize, fileName);
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

	for (ptrdiff_t i = 0; i < arrlen(macros); ++i)
	{
		bdestroy(&macros[i]);
	}
	arrfree(macros);
	arrfree(args);
}
#endif
#if defined(DIRECT3D12)
extern void d3d12_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(DIRECT3D11)
extern void d3d11_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(ORBIS)
extern bool orbis_compileShader(
	Renderer* pRenderer, ShaderStage stage, ShaderStage allStages, const char* srcFileName, const char* outFileName, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(PROSPERO)
extern bool prospero_compileShader(
	Renderer* pRenderer, ShaderStage stage, ShaderStage allStages, const char* srcFileName, const char* outFileName, uint32_t macroCount,
	ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif
#if defined(GLES)
extern void gl_compileShader(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
	bool enablePrimitiveId, uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, const char* pEntryPoint);
#endif

#if !defined(NX64)
static bool loadShaderSourceFile(
	const char* pAppName, FileStream* file, const char* filePath, time_t* outTimeStamp, bstring* outCode)
{
	ASSERT(file);
	ASSERT(outTimeStamp && outCode);

	time_t fileTimeStamp = fsGetLastModifiedTime(RD_SHADER_SOURCES, filePath);
	if (fileTimeStamp > *outTimeStamp)
		*outTimeStamp = fileTimeStamp;

	ssize_t fileSize = fsGetStreamFileSize(file);
	char* fileContents = (char*)alloca(fileSize + 1);
	fsReadFromStream(file, fileContents, fileSize);
	fileContents[fileSize] = '\0';
	
	bcatblk(outCode, fileContents, (int)fileSize);
	return true;
}
#endif

struct FSLDerivative
{
	uint64_t mHash, mOffset, mSize;
};

struct FSLMetadata
{
	uint32_t mUseMultiView;
};

struct FSLHeader
{
	char mMagic[4];
	uint32_t mDerivativeCount;
	FSLMetadata mMetadata;
};

void loadByteCode(Renderer* pRenderer, ResourceDirectory resourceDir, const char* binaryShaderPath, BinaryShaderStageDesc* pOut, ShaderByteCodeBuffer* pShaderByteCodeBuffer, FSLMetadata* pOutMetadata)
{
	FileStream binaryFileStream = {};
	const bool result = fsOpenStreamFromPath(resourceDir, binaryShaderPath, FM_READ_BINARY, NULL, &binaryFileStream);
	ASSERT(result);

	ssize_t size = fsGetStreamFileSize(&binaryFileStream);
	ASSERT(size > 0);

	FSLHeader header = {};
	if (sizeof(FSLHeader) != fsReadFromStream(&binaryFileStream, (void*)&header, sizeof(FSLHeader)))
		ASSERT(false);
	
	if (strncmp("@FSL", header.mMagic, 4) != 0)
	{
		// Shader was not compiled using FSL script
		fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, 0);

#if defined(PROSPERO)
		extern void prospero_loadByteCode(Renderer*, FileStream*, ssize_t, BinaryShaderStageDesc*);
		prospero_loadByteCode(pRenderer, &binaryFileStream, size, pOut);
#else
		pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 256, (uint32_t)size, binaryShaderPath);
		pOut->mByteCodeSize = (uint32_t)size;
		fsReadFromStream(&binaryFileStream, (void*)pOut->pByteCode, size);
#endif
	}
	else
	{
		ASSERT(strncmp("@FSL", header.mMagic, 4) == 0);
		const size_t derivativesSize = sizeof(FSLDerivative) * header.mDerivativeCount;
		FSLDerivative* pDerivatives = (FSLDerivative*)alloca(derivativesSize);
		if (derivativesSize != fsReadFromStream(&binaryFileStream, (void*)pDerivatives, derivativesSize))
			ASSERT(false);

		if(pOutMetadata)
			*pOutMetadata = header.mMetadata;

#if defined(PROSPERO)
		ASSERT(header.mDerivativeCount == 1);
		fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, pDerivatives[0].mOffset);

		extern void prospero_loadByteCode(Renderer*, FileStream*, ssize_t, BinaryShaderStageDesc*);
		prospero_loadByteCode(pRenderer, &binaryFileStream, pDerivatives[0].mSize, pOut);
#else
		uint64_t derivativeHash = 0;

#if defined(VULKAN)
		if (gSelectedRendererApi == RENDERER_API_VULKAN)
		{
			// Needs to match with the way we set the derivatives in FSL scripts (vulkan.py, compilers.py)
			derivativeHash =
				(uint64_t)pRenderer->mVulkan.mShaderSampledImageArrayDynamicIndexingSupported |
				(uint64_t)pRenderer->mVulkan.mDescriptorIndexingExtension << 1;

		}
#endif

		for (uint32_t i = 0; i < header.mDerivativeCount; ++i)
		{
			// If we only have one shader it means it's compatible with any GPU, otherwise we need to check the hash
			if (header.mDerivativeCount == 1 || derivativeHash == pDerivatives[i].mHash)
			{
				fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, pDerivatives[i].mOffset);
				pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 256, (uint32_t)pDerivatives[i].mSize, binaryShaderPath);
				pOut->mByteCodeSize = (uint32_t)pDerivatives[i].mSize;
				fsReadFromStream(&binaryFileStream, (void*)pOut->pByteCode, (uint32_t)pDerivatives[i].mSize);
				break;
			}
		}

		ASSERT(pOut->pByteCode);
#endif
	}

	fsCloseStream(&binaryFileStream);
}


// Loads the bytecode from file if the binary shader file is newer than the source
bool check_for_byte_code(Renderer* pRenderer, const char* binaryShaderPath, time_t sourceTimeStamp, BinaryShaderStageDesc* pOut, ShaderByteCodeBuffer* pShaderByteCodeBuffer)
{
	// If source code is loaded from a package, its timestamp will be zero. Else check that binary is not older
	// than source
	time_t dstTimeStamp = fsGetLastModifiedTime(RD_SHADER_BINARIES, binaryShaderPath);
	if (!sourceTimeStamp || (dstTimeStamp < sourceTimeStamp))
		return false;

	loadByteCode(pRenderer, RD_SHADER_BINARIES, binaryShaderPath, pOut, pShaderByteCodeBuffer, NULL);
	return true;
}

// Saves bytecode to a file
bool save_byte_code(const char* binaryShaderPath, char* byteCode, uint32_t byteCodeSize)
{
	if (!byteCodeSize)
		return false;

	FileStream fh = {};

	if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, binaryShaderPath, FM_WRITE_BINARY, NULL, &fh))
		return false;

	fsWriteToStream(&fh, byteCode, byteCodeSize);
	fsCloseStream(&fh);

	return true;
}

static const char* getRendererAPIName()
{
	switch (gSelectedRendererApi)
	{
#if defined(DIRECT3D12)
#if defined(SCARLETT)
	case RENDERER_API_D3D12: return "SCARLETT"; break;
#elif defined(XBOX)
	case RENDERER_API_D3D12: return "XBOX"; break;
#else
	case RENDERER_API_D3D12: return "DIRECT3D12"; break;
#endif
#endif
#if defined(DIRECT3D11)
	case RENDERER_API_D3D11: return "DIRECT3D11"; break;
#endif
#if defined(VULKAN)
#if defined(QUEST_VR)
	case RENDERER_API_VULKAN: return "QUEST"; break;
#else
	case RENDERER_API_VULKAN: return "VULKAN"; break;
#endif
#endif
#if defined(GLES)
	case RENDERER_API_GLES: return "GLES"; break;
#endif
#if defined(METAL)
#if defined(TARGET_IOS)
	case RENDERER_API_METAL: return "IOS"; break;
#else
	case RENDERER_API_METAL: return "MACOS"; break;
#endif
#endif
#if defined(ORBIS)
	case RENDERER_API_ORBIS: return "ORBIS"; break;
#endif
#if defined(PROSPERO)
	case RENDERER_API_PROSPERO: return "PROSPERO"; break;
#endif
	default: break;
	}

	ASSERT(false && "Renderer API name not defined");
	return "";
}

bool load_shader_stage_byte_code(
	Renderer* pRenderer, ShaderTarget target, ShaderStage stage, ShaderStage allStages, const ShaderStageLoadDesc& loadDesc,
	uint32_t macroCount, ShaderMacro* pMacros, BinaryShaderStageDesc* pOut, ShaderByteCodeBuffer* pShaderByteCodeBuffer, FSLMetadata* pOutMetadata)
{
	const char* rendererApi = getRendererAPIName();

	bstring* cleanupBstrings[4] = {NULL};
	int cleanupBstringsCount = 0;
	auto cleanup = [&cleanupBstrings, &cleanupBstringsCount]() {
		for (int i = 0; i < cleanupBstringsCount; ++i)
			bdestroy(cleanupBstrings[i]);
	};

	bstring code = bempty();
	cleanupBstrings[cleanupBstringsCount++] = &code;

	unsigned char fileNameAPIBuf[FS_MAX_PATH];
	bstring fileNameAPI = bemptyfromarr(fileNameAPIBuf);
	cleanupBstrings[cleanupBstringsCount++] = &fileNameAPI;

	if (rendererApi[0] != '\0')
		bformat(&fileNameAPI, "%s/%s", rendererApi, loadDesc.pFileName);
	else
		bcatcstr(&fileNameAPI, loadDesc.pFileName);
		
#if defined(METAL)
	bcatcstr(&fileNameAPI, ".metal");
#endif

	// If there are no macros specified there's no change to the shader source, we can use the binary compiled by FSL offline.
	if (macroCount == 0)
	{
#if defined(GLES)
		if (gSelectedRendererApi != RENDERER_API_GLES)
#endif
		{
			loadByteCode(pRenderer, RD_SHADER_BINARIES, (const char*)fileNameAPI.data, pOut, pShaderByteCodeBuffer, pOutMetadata);
			cleanup();
			return true;
		}
	}

	LOGF(eINFO, "Compiling shader in runtime: %s -> '%s' macroCount=%u", getRendererAPIName(), loadDesc.pFileName, macroCount);
	
#if defined (NX64)
	return false;
#else
	time_t timeStamp = 0;
	FileStream    sourceFileStream = {};
	bool          sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, (const char*)fileNameAPI.data, FM_READ_BINARY, NULL, &sourceFileStream);
	ASSERT(sourceExists && "No source shader present for file");

	if (!loadShaderSourceFile(pRenderer->pName, &sourceFileStream, (const char*)fileNameAPI.data, &timeStamp, &code))
	{
		fsCloseStream(&sourceFileStream);
		cleanup();
		return false;
	}

	bstring shaderDefines = bempty();
	cleanupBstrings[cleanupBstringsCount++] = &shaderDefines;

	balloc(&shaderDefines, 64);

	// Apply user specified macros
	for (uint32_t i = 0; i < macroCount; ++i)
	{
		bcatcstr(&shaderDefines, pMacros[i].definition);
		bcatcstr(&shaderDefines, pMacros[i].value);
	}

	char extension[FS_MAX_PATH] = { 0 };
	fsGetPathExtension(loadDesc.pFileName, extension);
	char fileName[FS_MAX_PATH] = { 0 };
	fsGetPathFileName(loadDesc.pFileName, fileName);

	bstring binaryShaderComponent = bempty();
	cleanupBstrings[cleanupBstringsCount++] = &binaryShaderComponent;

	balloc(&binaryShaderComponent, 128);

	static const size_t seed = 0x31415926;
	size_t shaderDefinesHash = stbds_hash_bstring(&shaderDefines, seed);
	bformat(&binaryShaderComponent, "%s_%s_%zu_%s_%u", rendererApi, fileName, shaderDefinesHash, extension, target);

#ifdef DIRECT3D11
	if (gSelectedRendererApi == RENDERER_API_D3D11)
		bformata(&binaryShaderComponent, "%u", pRenderer->mD3D11.mFeatureLevel);
#endif
	bcatliteral(&binaryShaderComponent, ".bin");

	// Shader source is newer than binary
	if (!check_for_byte_code(pRenderer, (const char*)binaryShaderComponent.data, timeStamp, pOut, pShaderByteCodeBuffer))
	{
		switch (gSelectedRendererApi)
		{
#if defined(DIRECT3D12)
			case RENDERER_API_D3D12:
				d3d12_compileShader(
					pRenderer, target, stage, (const char*)fileNameAPI.data, (uint32_t)code.slen, (const char*)code.data,
					loadDesc.mFlags & SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID, macroCount, pMacros, pOut, loadDesc.pEntryPointName);

				if (!save_byte_code((const char*)binaryShaderComponent.data, (char*)(pOut->pByteCode), pOut->mByteCodeSize))
				{
					LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
				}
				break;
#endif
#if defined(DIRECT3D11)
			case RENDERER_API_D3D11:
				d3d11_compileShader(
					pRenderer, target, stage, (const char*)fileNameAPI.data, (uint32_t)code.slen, (const char*)code.data,
					loadDesc.mFlags & SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID, macroCount, pMacros, pOut, loadDesc.pEntryPointName);

				if (!save_byte_code((const char*)binaryShaderComponent.data, (char*)(pOut->pByteCode), pOut->mByteCodeSize))
				{
					LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
				}
				break;
#endif
#if defined(VULKAN)
			case RENDERER_API_VULKAN:
#if defined(__ANDROID__)
				vk_compileShader(
					pRenderer, stage, (uint32_t)code.slen, (const char*)code.data, (const char*)binaryShaderComponent.data, macroCount, pMacros, pOut,
					pShaderByteCodeBuffer, loadDesc.pEntryPointName);
				if (!save_byte_code((const char*)binaryShaderComponent.data, (char*)(pOut->pByteCode), pOut->mByteCodeSize))
				{
					LOGF(LogLevel::eWARNING, "Failed to save byte code for file %s", loadDesc.pFileName);
				}
#else
				vk_compileShader(
					pRenderer, target, stage, (const char*)fileNameAPI.data, (const char*)binaryShaderComponent.data, macroCount, pMacros, pOut,
					pShaderByteCodeBuffer, loadDesc.pEntryPointName);
#endif
				break;
#endif
#if defined(METAL)
			case RENDERER_API_METAL:
				mtl_compileShader(
					pRenderer, (const char*)fileNameAPI.data,  (const char*)binaryShaderComponent.data, macroCount, pMacros, pOut, 
					pShaderByteCodeBuffer, loadDesc.pEntryPointName);
				break;
#endif
#if defined(GLES)
			case RENDERER_API_GLES:
				gl_compileShader(
					pRenderer, target, stage, loadDesc.pFileName, (uint32_t)code.slen, (const char*)code.data, (const char*)binaryShaderComponent.data,
					macroCount, pMacros, pOut, loadDesc.pEntryPointName);
				break;
#endif
#if defined(ORBIS)
			case RENDERER_API_ORBIS:
				orbis_compileShader(
					pRenderer, stage, allStages, loadDesc.pFileName, (const char*)binaryShaderComponent.data, macroCount, pMacros, pOut,
					loadDesc.pEntryPointName);
				break;
#endif
#if defined(PROSPERO)
			case RENDERER_API_PROSPERO:
				prospero_compileShader(
					pRenderer, stage, allStages, loadDesc.pFileName, (const char*)binaryShaderComponent.data, macroCount, pMacros, pOut,
					loadDesc.pEntryPointName);
				break;
#endif
			default: break;
		}

#if !defined(PROSPERO) && !defined(ORBIS)
		if (!pOut->pByteCode)
		{
			LOGF(eERROR, "Error while generating bytecode for shader %s", loadDesc.pFileName);
			fsCloseStream(&sourceFileStream);
			ASSERT(false);
			cleanup();
			return false;
		}
#endif
	}

	fsCloseStream(&sourceFileStream);
	cleanup();

	return true;
#endif
}

#ifdef TARGET_IOS
bool find_shader_stage(const char* fileName, ShaderDesc* pDesc, ShaderStageDesc** pOutStage, ShaderStage* pStage)
{
	char extension[FS_MAX_PATH] = { 0 };
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
	else if (
		(stricmp(extension, "rgen") == 0) || (stricmp(extension, "rmiss") == 0) || (stricmp(extension, "rchit") == 0) ||
		(stricmp(extension, "rint") == 0) || (stricmp(extension, "rahit") == 0) || (stricmp(extension, "rcall") == 0))
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
#endif

bool find_shader_stage(const char* extension, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
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
	else if (
		(stricmp(extension, "rgen") == 0) || (stricmp(extension, "rmiss") == 0) || (stricmp(extension, "rchit") == 0) ||
		(stricmp(extension, "rint") == 0) || (stricmp(extension, "rahit") == 0) || (stricmp(extension, "rcall") == 0))
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

void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** ppShader)
{
#ifndef DIRECT3D11
	if ((uint32_t)pDesc->mTarget > pRenderer->mShaderTarget)
	{
		LOGF(LogLevel::eERROR, 
			"Requested shader target (%u) is higher than the shader target that the renderer supports (%u). Shader wont be compiled",
			(uint32_t)pDesc->mTarget, (uint32_t)pRenderer->mShaderTarget
		);
		return;
	}
#endif

#ifdef TARGET_IOS

	uint32_t iosMacroCount = 0;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && pDesc->mStages[i].pFileName[0] != 0)
			iosMacroCount += pDesc->mStages[i].mMacroCount;
	}

	// We can't compile binary shaders on IOS, generate shader directly from text
	if(iosMacroCount != 0)
	{
		// Binary shaders are not supported on iOS.
		ShaderDesc desc = {};
		bstring codes[SHADER_STAGE_COUNT];
		ShaderMacro* pMacros[SHADER_STAGE_COUNT] = {};
		for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
		{
			codes[i] = bempty();

			if (pDesc->mStages[i].pFileName && strlen(pDesc->mStages[i].pFileName))
			{
				ShaderStage stage;
				ShaderStageDesc* pStage = NULL;
				if (find_shader_stage(pDesc->mStages[i].pFileName, &desc, &pStage, &stage))
				{
					char metalFileName[FS_MAX_PATH] = { 0 };
					fsAppendPathExtension(pDesc->mStages[i].pFileName, "metal", metalFileName);
					FileStream fh = {};
					bool sourceExists = fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, NULL, &fh);
					ASSERT(sourceExists);

					pStage->pName = pDesc->mStages[i].pFileName;
					time_t timestamp = 0;
					loadShaderSourceFile(pRenderer->pName, &fh, metalFileName, &timestamp, &codes[i]);
					pStage->pCode = (const char*)codes[i].data;
					if (pDesc->mStages[i].pEntryPointName)
						pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
					else
						pStage->pEntryPoint = "stageMain";
					// Apply user specified shader macros
					pStage->mMacroCount = pDesc->mStages[i].mMacroCount;
					pMacros[i] = (ShaderMacro*)alloca(pStage->mMacroCount * sizeof(ShaderMacro));
					pStage->pMacros = pMacros[i];
					for (uint32_t j = 0; j < pDesc->mStages[i].mMacroCount; j++)
						pMacros[i][j] = pDesc->mStages[i].pMacros[j];
					fsCloseStream(&fh);
					desc.mStages |= stage;
				}
			}
		}

		desc.mConstantCount = pDesc->mConstantCount;
		desc.pConstants = pDesc->pConstants;

		addIosShader(pRenderer, &desc, ppShader);
		for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
		{
			bdestroy(&codes[i]);
		}
		return;
	}

#endif
	
	BinaryShaderDesc binaryDesc = {};
#if defined(METAL)
	char* pSources[SHADER_STAGE_COUNT] = {};
#endif

	ShaderStageLoadFlags combinedFlags = SHADER_STAGE_LOAD_FLAG_NONE;

	ShaderByteCodeBuffer shaderByteCodeBuffer = {};
#if !defined(PROSPERO)
	char bytecodeStack[ShaderByteCodeBuffer::kStackSize] = {};
	shaderByteCodeBuffer.pStackMemory = bytecodeStack;
#endif

#if defined(QUEST_VR)
	bool bIsMultivewVR = false;
#endif

	ShaderStage stages = SHADER_STAGE_NONE;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && pDesc->mStages[i].pFileName[0] != 0)
		{
			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			char                   ext[FS_MAX_PATH] = { 0 };
			fsGetPathExtension(pDesc->mStages[i].pFileName, ext);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
				stages |= stage;
		}
	}
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pDesc->mStages[i].pFileName && pDesc->mStages[i].pFileName[0] != 0)
		{
			const char* fileName = pDesc->mStages[i].pFileName;

			ShaderStage            stage;
			BinaryShaderStageDesc* pStage = NULL;
			char                   ext[FS_MAX_PATH] = { 0 };
			fsGetPathExtension(fileName, ext);
			if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
			{
				combinedFlags |= pDesc->mStages[i].mFlags;
				uint32_t macroCount = pDesc->mStages[i].mMacroCount;
				
				ShaderMacro* macros = NULL;
				arrsetlen(macros, macroCount);
				for (uint32_t macro = 0; macro < pDesc->mStages[i].mMacroCount; ++macro)
					macros[macro] = pDesc->mStages[i].pMacros[macro]; //-V595

				FSLMetadata metadata = {};
				if (!load_shader_stage_byte_code(
					pRenderer, pDesc->mTarget, stage, stages, pDesc->mStages[i], macroCount, macros, pStage, &shaderByteCodeBuffer, &metadata))
				{
					arrfree(macros);
					freeShaderByteCode(&shaderByteCodeBuffer, &binaryDesc);
					return;
				}
				arrfree(macros);

				binaryDesc.mStages |= stage;

#if defined(QUEST_VR)
				bIsMultivewVR |= metadata.mUseMultiView;

				// TODO: remove this assert after testing on Quest
				ASSERT(((pDesc->mStages[i].mFlags& SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW) != 0) == metadata.mUseMultiView);
#endif

#if defined(METAL)
				if (pDesc->mStages[i].pEntryPointName)
					pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
				else
					pStage->pEntryPoint = "stageMain";

				char metalFileName[FS_MAX_PATH] = { 0 };
#if defined(TARGET_IOS)
				strcat(metalFileName, "IOS/");
#else
				strcat(metalFileName, "MACOS/");
#endif
				strcat(metalFileName, fileName);
				strcat(metalFileName, ".metal");

				FileStream fh = {};
				fsOpenStreamFromPath(RD_SHADER_SOURCES, metalFileName, FM_READ_BINARY, NULL, &fh);
				size_t metalFileSize = fsGetStreamFileSize(&fh);
				pSources[i] = (char*)tf_malloc(metalFileSize + 1);
				pStage->pSource = pSources[i];
				pStage->mSourceSize = (uint32_t)metalFileSize;
				fsReadFromStream(&fh, pSources[i], metalFileSize);
				pSources[i][metalFileSize] = 0;    // Ensure the shader text is null-terminated
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

	binaryDesc.mConstantCount = pDesc->mConstantCount;
	binaryDesc.pConstants = pDesc->pConstants;

	addShaderBinary(pRenderer, &binaryDesc, ppShader);
	freeShaderByteCode(&shaderByteCodeBuffer, &binaryDesc);

#if defined(QUEST_VR)
	if (ppShader)
	{
		(*ppShader)->mIsMultiviewVR = bIsMultivewVR;
		ASSERT(((combinedFlags & SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW) != 0) == bIsMultivewVR);
	}
#endif

#if defined(METAL)
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		if (pSources[i])
		{
			tf_free(pSources[i]);
		}
	}
#endif
}

/************************************************************************/
// Pipeline cache save, load
/************************************************************************/
void loadPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache)
{
#if defined(DIRECT3D12) || defined(VULKAN)

	char rendererApi[FS_MAX_PATH] = {};
#if defined(USE_MULTIPLE_RENDER_APIS)
	switch (gSelectedRendererApi)
	{
#if defined(DIRECT3D12)
		case RENDERER_API_D3D12: strcat(rendererApi, "DIRECT3D12/"); break;
#endif
#if defined(VULKAN)
		case RENDERER_API_VULKAN: strcat(rendererApi, "VULKAN/"); break;
#endif
		default: break;
	}
#endif

	strncat(rendererApi, pDesc->pFileName, strlen(pDesc->pFileName));

	FileStream stream = {};
	bool       success = fsOpenStreamFromPath(RD_PIPELINE_CACHE, rendererApi, FM_READ_BINARY, pDesc->pFilePassword, &stream);
	ssize_t    dataSize = 0;
	void*      data = NULL;
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

	char rendererApi[FS_MAX_PATH] = {};
#if defined(USE_MULTIPLE_RENDER_APIS)
	switch (gSelectedRendererApi)
	{
#if defined(DIRECT3D12)
		case RENDERER_API_D3D12: strcat(rendererApi, "DIRECT3D12/"); break;
#endif
#if defined(VULKAN)
		case RENDERER_API_VULKAN: strcat(rendererApi, "VULKAN/"); break;
#endif
		default: break;
	}
#endif

	strncat(rendererApi, pDesc->pFileName, strlen(pDesc->pFileName));

	FileStream stream = {};
	if (fsOpenStreamFromPath(RD_PIPELINE_CACHE, rendererApi, FM_WRITE_BINARY, pDesc->pFilePassword, &stream))
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
