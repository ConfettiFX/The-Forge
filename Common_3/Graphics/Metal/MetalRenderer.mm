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

#ifdef METAL

#define RENDERER_IMPLEMENTATION

// Argument Buffer additional debug logging
//#define ARGUMENTBUFFER_DEBUG

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#import "../../Graphics/Interfaces/IGraphics.h"

#ifdef ENABLE_OS_PROC_MEMORY
#include <os/proc.h>
#endif

// Fallback if os_proc_available_memory not available
#include <mach/mach.h>
#include <mach/mach_host.h>

#include "MetalMemoryAllocatorImpl.h"

#include "../../Utilities/Math/MathTypes.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Threading/Atomics.h"
#include "../../Graphics/GPUConfig.h"
#include "../../Utilities/Interfaces/IMemory.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"

#include "MetalCapBuilder.h"

#define MAX_BUFFER_BINDINGS 31
// Start vertex attribute bindings at index 30 and decrement so we can bind regular buffers from index 0 for simplicity
#define VERTEX_BINDING_OFFSET (MAX_BUFFER_BINDINGS - 1)
#define DESCRIPTOR_UPDATE_FREQ_PADDING 0

VkAllocationCallbacks gMtlAllocationCallbacks = {
	// pUserData
	NULL,
	// pfnAllocation
	[](void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) { return tf_memalign(alignment, size); },
	// pfnReallocation
	[](void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
		return tf_realloc(pOriginal, size);
	},
	// pfnFree
	[](void* pUserData, void* pMemory) { tf_free(pMemory); },
	// pfnInternalAllocation
	[](void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) {},
	// pfnInternalFree
	[](void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) {}
};

typedef struct DescriptorIndexMap
{
	char* key;
	uint32_t value;
} DescriptorIndexMap;

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
namespace RENDERER_CPP_NAMESPACE {
#endif

// clang-format off
	MTLBlendOperation gMtlBlendOpTranslator[BlendMode::MAX_BLEND_MODES] =
	{
		MTLBlendOperationAdd,
		MTLBlendOperationSubtract,
		MTLBlendOperationReverseSubtract,
		MTLBlendOperationMin,
		MTLBlendOperationMax,
	};

	MTLBlendFactor gMtlBlendConstantTranslator[BlendConstant::MAX_BLEND_CONSTANTS] =
	{
		MTLBlendFactorZero,
		MTLBlendFactorOne,
		MTLBlendFactorSourceColor,
		MTLBlendFactorOneMinusSourceColor,
		MTLBlendFactorDestinationColor,
		MTLBlendFactorOneMinusDestinationColor,
		MTLBlendFactorSourceAlpha,
		MTLBlendFactorOneMinusSourceAlpha,
		MTLBlendFactorDestinationAlpha,
		MTLBlendFactorOneMinusDestinationAlpha,
		MTLBlendFactorSourceAlphaSaturated,
		MTLBlendFactorBlendColor,
		MTLBlendFactorOneMinusBlendColor,
		//MTLBlendFactorBlendAlpha,
		//MTLBlendFactorOneMinusBlendAlpha,
		//MTLBlendFactorSource1Color,
		//MTLBlendFactorOneMinusSource1Color,
		//MTLBlendFactorSource1Alpha,
		//MTLBlendFactorOneMinusSource1Alpha,
	};

	MTLCompareFunction gMtlComparisonFunctionTranslator[CompareMode::MAX_COMPARE_MODES] =
	{
		MTLCompareFunctionNever,
		MTLCompareFunctionLess,
		MTLCompareFunctionEqual,
		MTLCompareFunctionLessEqual,
		MTLCompareFunctionGreater,
		MTLCompareFunctionNotEqual,
		MTLCompareFunctionGreaterEqual,
		MTLCompareFunctionAlways,
	};

	MTLStencilOperation gMtlStencilOpTranslator[StencilOp::MAX_STENCIL_OPS] = {
		MTLStencilOperationKeep,
		MTLStencilOperationZero,
		MTLStencilOperationReplace,
		MTLStencilOperationInvert,
		MTLStencilOperationIncrementWrap,
		MTLStencilOperationDecrementWrap,
		MTLStencilOperationIncrementClamp,
		MTLStencilOperationDecrementClamp,
	};

	MTLCullMode gMtlCullModeTranslator[CullMode::MAX_CULL_MODES] =
	{
		MTLCullModeNone,
		MTLCullModeBack,
		MTLCullModeFront,
	};

	MTLTriangleFillMode gMtlFillModeTranslator[FillMode::MAX_FILL_MODES] =
	{
		MTLTriangleFillModeFill,
		MTLTriangleFillModeLines,
	};

#if defined(ENABLE_SAMPLER_CLAMP_TO_BORDER)
	API_AVAILABLE(macos(10.12))
	static const MTLSamplerAddressMode gMtlAddressModeTranslator[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
		MTLSamplerAddressModeClampToBorderColor,
	};
#endif

	static const MTLSamplerAddressMode gMtlAddressModeTranslatorFallback[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
		MTLSamplerAddressModeClampToEdge,
	};

// clang-format on

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#define SAFE_FREE(p_var)       \
	if (p_var)                 \
	{                          \
		tf_free((void*)p_var); \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

MTLVertexFormat util_to_mtl_vertex_format(const TinyImageFormat format);

static inline FORGE_CONSTEXPR MTLLoadAction util_to_mtl_load_action(const LoadActionType loadActionType)
{
	switch(loadActionType)
	{
		case LOAD_ACTION_DONTCARE: return MTLLoadActionDontCare;
		case LOAD_ACTION_LOAD: return MTLLoadActionLoad;
		case LOAD_ACTION_CLEAR: return MTLLoadActionClear;
		default: return MTLLoadActionDontCare;
	}
}

static inline FORGE_CONSTEXPR MTLStoreAction util_to_mtl_store_action(const StoreActionType storeActionType)
{
	switch(storeActionType)
	{
		case STORE_ACTION_STORE: return MTLStoreActionStore;
		case STORE_ACTION_DONTCARE: return MTLStoreActionDontCare;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
		case STORE_ACTION_RESOLVE_STORE:
			if (@available(macOS 10.12, iOS 10.0, *))
			{
				return MTLStoreActionStoreAndMultisampleResolve;
			}
			else
			{
				return MTLStoreActionMultisampleResolve;
			}
		case STORE_ACTION_RESOLVE_DONTCARE: return MTLStoreActionMultisampleResolve;
#endif
		default: return MTLStoreActionStore;
	}
}

void util_set_heaps_graphics(Cmd* pCmd);
void util_set_heaps_compute(Cmd* pCmd);

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT);

void mtl_createShaderReflection(Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection);

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);

#if defined(MTL_RAYTRACING_AVAILABLE)
void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline);
void removeRaytracingPipeline(RaytracingPipeline* pPipeline);
#endif

// GPU frame time accessor for macOS and iOS
#define GPU_FREQUENCY 1000000.0

DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
DECLARE_RENDERER_FUNCTION(
	void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)
DECLARE_RENDERER_FUNCTION(void, addVirtualTexture, Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData)
DECLARE_RENDERER_FUNCTION(void, removeVirtualTexture, Renderer* pRenderer, VirtualTexture* pTexture)

/************************************************************************/
// Globals
/************************************************************************/
static Texture* pDefault1DTexture = {};
static Texture* pDefault1DTextureArray = {};
static Texture* pDefault2DTexture = {};
static Texture* pDefault2DTextureArray = {};
static Texture* pDefault3DTexture = {};
static Texture* pDefaultCubeTexture = {};
static Texture* pDefaultCubeTextureArray = {};

static Buffer*  pDefaultBuffer = {};
static Sampler* pDefaultSampler = {};

static id<MTLDepthStencilState> pDefaultDepthState = nil;
static RasterizerStateDesc      gDefaultRasterizerState = {};

/************************************************************************/
// Descriptor Set Structure
/************************************************************************/
const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName)
{
	const DescriptorIndexMap* pMap = pRootSignature->pDescriptorNameToIndexMap;
	
	const DescriptorIndexMap* pNode = shgetp_null(pMap, pResName);
	
	if (pNode != NULL)
	{
		return &pRootSignature->pDescriptors[pNode->value];
	}
	else
	{
		LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}
/************************************************************************/
// Misc
/************************************************************************/
enum BarrierFlag
{
	BARRIER_FLAG_BUFFERS = 0x1,
	BARRIER_FLAG_TEXTURES = 0x2,
	BARRIER_FLAG_RENDERTARGETS = 0x4,
	BARRIER_FLAG_FENCE = 0x8,
};

typedef struct RootDescriptorHandle
{
	NOREFS id pResource;
	uint32_t  mStage : 5;
	uint32_t  mBinding : 27;
	uint32_t  mOffset;
} RootDescriptorHandle;

typedef struct RootDescriptorData
{
	struct RootDescriptorHandle* pTextures;
	struct RootDescriptorHandle* pBuffers;
	struct RootDescriptorHandle* pSamplers;
} RootDescriptorData;
/************************************************************************/
// Internal functions
/************************************************************************/
struct UntrackedResourceArray
{
	NOREFS id* pResources;
	uint16_t   mCount;
	uint16_t   mCapacity;
};

struct UntrackedResourceData
{
	UntrackedResourceArray mData;
	UntrackedResourceArray mRWData;
};

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_update(Cmd* pCmd, const QueueType& encoderType);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);
void util_bind_push_constant(Cmd* pCmd, const DescriptorInfo* pDesc, const void* pConstants);
void util_bind_root_cbv(Cmd* pCmd, const RootDescriptorHandle* pHandle);

void mtl_cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
	ASSERT(pCmd);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mMaxSets);

	if (pDescriptorSet->pRootDescriptorData)
	{
		RootDescriptorData* pData = pDescriptorSet->pRootDescriptorData + index;
		for (uint32_t i = 0; i < pDescriptorSet->mRootBufferCount; ++i)
		{
			const RootDescriptorHandle* pHandle = &pData->pBuffers[i];
			util_bind_root_cbv(pCmd, pHandle);
		}

		for (uint32_t i = 0; i < pDescriptorSet->mRootTextureCount; ++i)
		{
			const RootDescriptorHandle* pHandle = &pData->pTextures[i];

			if (pHandle->mStage & SHADER_STAGE_VERT)
			{
				[pCmd->mtlRenderEncoder setVertexTexture:pHandle->pResource atIndex:pHandle->mBinding];
			}
			if (pData->pTextures[i].mStage & SHADER_STAGE_FRAG)
			{
				[pCmd->mtlRenderEncoder setFragmentTexture:pHandle->pResource atIndex:pHandle->mBinding];
			}
			if (pData->pTextures[i].mStage & SHADER_STAGE_COMP)
			{
				[pCmd->mtlComputeEncoder setTexture:pHandle->pResource atIndex:pHandle->mBinding];
			}
		}

		for (uint32_t i = 0; i < pDescriptorSet->mRootSamplerCount; ++i)
		{
			const RootDescriptorHandle* pHandle = &pData->pSamplers[i];

			if (pHandle->mStage & SHADER_STAGE_VERT)
			{
				[pCmd->mtlRenderEncoder setVertexSamplerState:pHandle->pResource atIndex:pHandle->mBinding];
			}
			if (pData->pSamplers[i].mStage & SHADER_STAGE_FRAG)
			{
				[pCmd->mtlRenderEncoder setFragmentSamplerState:pHandle->pResource atIndex:pHandle->mBinding];
			}
			if (pData->pSamplers[i].mStage & SHADER_STAGE_COMP)
			{
				[pCmd->mtlComputeEncoder setSamplerState:pHandle->pResource atIndex:pHandle->mBinding];
			}
		}
	}

#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		if (!pDescriptorSet->mArgumentBuffer)
		{
			return;
		}

		const id<MTLBuffer> buffer = pDescriptorSet->mArgumentBuffer->mtlBuffer;
		const uint64_t      offset = pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride;

		// argument buffers
		if (pDescriptorSet->mStages & SHADER_STAGE_VERT)
		{
			[pCmd->mtlRenderEncoder setVertexBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
		}

		if (pDescriptorSet->mStages & SHADER_STAGE_FRAG)
		{
			[pCmd->mtlRenderEncoder setFragmentBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
		}

		if (pDescriptorSet->mStages & SHADER_STAGE_COMP)
		{
			[pCmd->mtlComputeEncoder setBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
		}
		
		// useResource on the untracked resources (UAVs, RTs)
		// useHeap doc
		// You may only read or sample resources within the specified heaps. This method ignores render targets (textures that specify a MTLTextureUsageRenderTarget usage option) and writable textures (textures that specify a MTLTextureUsageShaderWrite usage option) within the array of heaps. To use these resources, you must call the useResource:usage:stages: method instead.
		if (pDescriptorSet->ppUntrackedData[index])
		{
			const UntrackedResourceData* untracked = pDescriptorSet->ppUntrackedData[index];
			if (pDescriptorSet->mStages & SHADER_STAGE_COMP)
			{
				if (untracked->mData.mCount)
				{
					[pCmd->mtlComputeEncoder useResources:untracked->mData.pResources count:untracked->mData.mCount usage:MTLResourceUsageRead];
				}
				if (untracked->mRWData.mCount)
				{
					[pCmd->mtlComputeEncoder useResources:untracked->mRWData.pResources count:untracked->mRWData.mCount usage:MTLResourceUsageRead|MTLResourceUsageWrite];
				}
			}
			else
			{
				if (untracked->mData.mCount)
				{
					[pCmd->mtlRenderEncoder useResources:untracked->mData.pResources count:untracked->mData.mCount usage:MTLResourceUsageRead];
				}
				if (untracked->mRWData.mCount)
				{
					[pCmd->mtlRenderEncoder useResources:untracked->mRWData.pResources count:untracked->mRWData.mCount usage:MTLResourceUsageRead|MTLResourceUsageWrite];
				}
			}
		}
	}
#endif
}

//
// Push Constants
//
void mtl_cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
	ASSERT(pCmd);
	ASSERT(pRootSignature);
	ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->mDescriptorCount);

	const DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
	ASSERT(pDesc);
	ASSERT(DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->mType);
	util_bind_push_constant(pCmd, pDesc, pConstants);
}

#if defined(ENABLE_GRAPHICS_DEBUG)
#define VALIDATE_DESCRIPTOR(descriptor, ...)						\
	if (!(descriptor))												\
	{																\
		unsigned char messageBuf[256];								\
		bstring message = bemptyfromarr(messageBuf);				\
		bassigncstr(&message, __FUNCTION__);						\
		bformata(&message, "" __VA_ARGS__);							\
		LOGF(LogLevel::eERROR, "%s", (const char*)message.data);	\
		_FailedAssert(__FILE__, __LINE__, __FUNCTION__);			\
		bdestroy(&message);											\
		continue;													\
	}
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void mtl_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	mtl_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);
	
	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
	
	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam = pParams + i;
		uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		const DescriptorInfo* pDesc =
			(paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
		if (paramIndex != UINT32_MAX)
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
		}
		else
		{
			VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
		}

		VALIDATE_DESCRIPTOR(pDesc->mRootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays", pDesc->pName);
		VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs", pDesc->pName);

		DescriptorDataRange range = pParam->pRanges[0];

		VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges->mSize is zero", pDesc->pName);
		
		RootDescriptorHandle handle = {};
		handle.mBinding = pDesc->mReg;
		handle.pResource = pParam->ppBuffers[0]->mtlBuffer;
		handle.mStage = pDesc->mUsedStages;
		handle.mOffset = (uint32_t)(pParam->ppBuffers[0]->mOffset + range.mOffset);
		util_bind_root_cbv(pCmd, &handle);
	}
}

//
// DescriptorSet
//

void mtl_addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppDescriptorSet);

	const RootSignature* pRootSignature(pDesc->pRootSignature);
	const uint32_t       updateFreq(pDesc->mUpdateFrequency + DESCRIPTOR_UPDATE_FREQ_PADDING);
	const uint32_t       nodeIndex = pDesc->mNodeIndex;
	const bool needRootData = (pRootSignature->mRootBufferCount || pRootSignature->mRootTextureCount || pRootSignature->mRootSamplerCount);

	uint32_t totalSize = sizeof(DescriptorSet);

	if (needRootData)
	{
		totalSize += pDesc->mMaxSets * sizeof(RootDescriptorData);
		totalSize += pDesc->mMaxSets * pRootSignature->mRootTextureCount * sizeof(RootDescriptorHandle);
		totalSize += pDesc->mMaxSets * pRootSignature->mRootBufferCount * sizeof(RootDescriptorHandle);
		totalSize += pDesc->mMaxSets * pRootSignature->mRootSamplerCount * sizeof(RootDescriptorHandle);
	}
	
#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		if (pRootSignature->mArgumentDescriptors[pDesc->mUpdateFrequency].count)
		{
			totalSize += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
		}
	}
#endif

	DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
	ASSERT(pDescriptorSet);

	pDescriptorSet->pRootSignature = pRootSignature;
	pDescriptorSet->mUpdateFrequency = updateFreq;
	pDescriptorSet->mNodeIndex = nodeIndex;
	pDescriptorSet->mMaxSets = pDesc->mMaxSets;
	pDescriptorSet->mRootTextureCount = pRootSignature->mRootTextureCount;
	pDescriptorSet->mRootBufferCount = pRootSignature->mRootBufferCount;
	pDescriptorSet->mRootSamplerCount = pRootSignature->mRootSamplerCount;

	uint8_t* mem = (uint8_t*)(pDescriptorSet + 1);
	
	if (needRootData)
	{
		pDescriptorSet->pRootDescriptorData = (RootDescriptorData*)mem;
		mem += pDesc->mMaxSets * sizeof(RootDescriptorData);

		for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
		{
			pDescriptorSet->pRootDescriptorData[i].pTextures = (RootDescriptorHandle*)mem;
			mem += pRootSignature->mRootTextureCount * sizeof(RootDescriptorHandle);

			pDescriptorSet->pRootDescriptorData[i].pBuffers = (RootDescriptorHandle*)mem;
			mem += pRootSignature->mRootBufferCount * sizeof(RootDescriptorHandle);

			pDescriptorSet->pRootDescriptorData[i].pSamplers = (RootDescriptorHandle*)mem;
			mem += pRootSignature->mRootSamplerCount * sizeof(RootDescriptorHandle);
		}
	}

#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		NSMutableArray<MTLArgumentDescriptor*>* descriptors = pRootSignature->mArgumentDescriptors[pDesc->mUpdateFrequency];

		if (descriptors.count)
		{
			ShaderStage shaderStages =
				(pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE ? SHADER_STAGE_COMP : (SHADER_STAGE_VERT | SHADER_STAGE_FRAG));

			NSArray* sortedArray;
			sortedArray = [descriptors sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
				MTLArgumentDescriptor* first = a;
				MTLArgumentDescriptor* second = b;
				return (NSComparisonResult)(first.index > second.index);
			}];
			ASSERT(sortedArray.count);

			// create encoder
			pDescriptorSet->mArgumentEncoder = [pRenderer->pDevice newArgumentEncoderWithArguments:sortedArray];
			ASSERT(pDescriptorSet->mArgumentEncoder);

			// Create argument buffer
			uint32_t   argumentBufferSize = (uint32_t)round_up_64(pDescriptorSet->mArgumentEncoder.encodedLength, 256);
			BufferDesc bufferDesc = {};
			bufferDesc.mAlignment = 256;
			bufferDesc.mSize = argumentBufferSize * pDesc->mMaxSets;
			bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			addBuffer(pRenderer, &bufferDesc, &pDescriptorSet->mArgumentBuffer);

			pDescriptorSet->mStride = argumentBufferSize;
			pDescriptorSet->mStages = shaderStages;
			
			pDescriptorSet->ppUntrackedData = (UntrackedResourceData**)mem;
			mem += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
		}
	}
#endif

	// bind static samplers
	for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
	{
		const DescriptorInfo& descriptorInfo(pRootSignature->pDescriptors[i]);

		if (descriptorInfo.mType == DESCRIPTOR_TYPE_SAMPLER && descriptorInfo.mtlStaticSampler)
		{
			if (descriptorInfo.mIsArgumentBufferField)
			{
#if defined(ENABLE_ARGUMENT_BUFFERS)
				if (@available(macOS 10.13, iOS 11.0, *))
				{
					if (descriptorInfo.mReg == updateFreq)
					{
						for (uint32_t j = 0; j < pDescriptorSet->mMaxSets; ++j)
						{
							[pDescriptorSet->mArgumentEncoder
								setArgumentBuffer:pDescriptorSet->mArgumentBuffer->mtlBuffer
										   offset:pDescriptorSet->mArgumentBuffer->mOffset + j * pDescriptorSet->mStride];
							[pDescriptorSet->mArgumentEncoder setSamplerState:descriptorInfo.mtlStaticSampler
																	  atIndex:descriptorInfo.mHandleIndex];
						}
					}
				}
#endif
			}
			else
			{
				for (uint32_t i = 0; i < pDescriptorSet->mMaxSets; ++i)
				{
					RootDescriptorHandle* handle = &pDescriptorSet->pRootDescriptorData[i].pSamplers[descriptorInfo.mHandleIndex];
					handle->mBinding = descriptorInfo.mReg;
					handle->mStage = descriptorInfo.mUsedStages;
					handle->pResource = descriptorInfo.mtlStaticSampler;
				}
			}
		}
	}

	*ppDescriptorSet = pDescriptorSet;
}

void mtl_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{
#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		pDescriptorSet->mArgumentEncoder = nil;

		if (pDescriptorSet->mArgumentBuffer)
		{
			removeBuffer(pRenderer, pDescriptorSet->mArgumentBuffer);
			
			for (uint32_t set = 0; set < pDescriptorSet->mMaxSets; ++set)
			{
				if (pDescriptorSet->ppUntrackedData[set])
				{
					SAFE_FREE(pDescriptorSet->ppUntrackedData[set]->mData.pResources);
					SAFE_FREE(pDescriptorSet->ppUntrackedData[set]->mRWData.pResources);
					SAFE_FREE(pDescriptorSet->ppUntrackedData[set]);
				}
			}
		}
	}
#endif

	SAFE_FREE(pDescriptorSet);
}

// UAVs, RTs inside arg buffers need to be tracked manually through useResource
// useHeap ignores all resources with UAV, RT flags
API_AVAILABLE(macos(10.13), ios(11.0))
static void TrackUntrackedResource(DescriptorSet* pDescriptorSet, uint32_t index, const MTLResourceUsage usage, id<MTLResource> resource)
{
	if (!pDescriptorSet->ppUntrackedData[index])
	{
		pDescriptorSet->ppUntrackedData[index] = (UntrackedResourceData*)tf_calloc(1, sizeof(UntrackedResourceData));
	}
	
	UntrackedResourceData* untracked = pDescriptorSet->ppUntrackedData[index];
	UntrackedResourceArray* dataArray = (usage & MTLResourceUsageWrite) ? &untracked->mRWData : &untracked->mData;
	
	if (dataArray->mCount >= dataArray->mCapacity)
	{
		++dataArray->mCapacity;
		dataArray->pResources = (NOREFS id<MTLResource>*)tf_realloc(dataArray->pResources, dataArray->mCapacity * sizeof(id<MTLResource>));
	}

	dataArray->pResources[dataArray->mCount++] = resource;
}

static void BindICBDescriptor(DescriptorSet* pDescriptorSet, uint32_t index, const DescriptorInfo* pDesc, uint32_t arrayStart, uint32_t arrayCount, const DescriptorData* pParam)
{
#if defined(ENABLE_INDIRECT_COMMAND_BUFFERS)
	if (@available(macOS 10.14, iOS 12.0, *))
	{
		for (uint32_t j = 0; j < arrayCount; ++j)
		{
			[pDescriptorSet->mArgumentEncoder
				setIndirectCommandBuffer:pParam->ppBuffers[j]->mtlIndirectCommandBuffer
								 atIndex:pDesc->mHandleIndex + arrayStart + j];
			
			TrackUntrackedResource(pDescriptorSet, index, MTLResourceUsageWrite, pParam->ppBuffers[j]->mtlIndirectCommandBuffer);
		}
	}
#endif
}

void mtl_updateDescriptorSet(
	Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
	ASSERT(pRenderer);
	ASSERT(pDescriptorSet);
	ASSERT(index < pDescriptorSet->mMaxSets);

	const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		if (pDescriptorSet->mArgumentEncoder)
		{
			// set argument buffer to update
			[pDescriptorSet->mArgumentEncoder
				setArgumentBuffer:pDescriptorSet->mArgumentBuffer->mtlBuffer
						   offset:pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride];
			
			if (pDescriptorSet->ppUntrackedData[index])
			{
				pDescriptorSet->ppUntrackedData[index]->mData.mCount = 0;
				pDescriptorSet->ppUntrackedData[index]->mRWData.mCount = 0;
			}
		}
	}
#endif

	for (uint32_t i = 0; i < count; ++i)
	{
		const DescriptorData* pParam(pParams + i);
		const uint32_t        paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

		const DescriptorInfo* pDesc = NULL;

		if (paramIndex != UINT32_MAX)
		{
			pDesc = &pRootSignature->pDescriptors[paramIndex];
		}
		else
		{
			pDesc = get_descriptor(pRootSignature, pParam->pName);
		}

		if (pDesc)
		{
			const DescriptorType type((DescriptorType)pDesc->mType);
			const uint32_t       arrayStart = pParam->mArrayOffset;
			const uint32_t       arrayCount(max(1U, pParam->mCount));

			switch (type)
			{
				case DESCRIPTOR_TYPE_SAMPLER:
				{
#if defined(ENABLE_ARGUMENT_BUFFERS)
					if (pDesc->mIsArgumentBufferField)
					{
						if (@available(macOS 10.13, iOS 11.0, *))
						{
							for (uint32_t j = 0; j < arrayCount; ++j)
							{
								[pDescriptorSet->mArgumentEncoder setSamplerState:pParam->ppSamplers[j]->mtlSamplerState
																		  atIndex:pDesc->mHandleIndex + arrayStart + j];
							}
						}
					}
					else
#endif
					{
						ASSERT(arrayCount == 1 && "Array not supported");
						RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pSamplers[pDesc->mHandleIndex];
						pData->mBinding = pDesc->mReg;
						pData->mStage = pDesc->mUsedStages;
						pData->pResource = pParam->ppSamplers[0]->mtlSamplerState;
					}

					break;
				}
				case DESCRIPTOR_TYPE_TEXTURE:
				{
#if defined(ENABLE_ARGUMENT_BUFFERS)
					if (pDesc->mIsArgumentBufferField)
					{
						if (@available(macOS 10.13, iOS 11.0, *))
						{
							for (uint32_t j = 0; j < arrayCount; ++j)
							{
								const Texture* texture = pParam->ppTextures[j];
								
								if (pParam->mBindStencilResource)
								{
									[pDescriptorSet->mArgumentEncoder setTexture:pParam->ppTextures[j]->mtlStencilTexture
																		 atIndex:pDesc->mHandleIndex + arrayStart + j];
								}
								else if (pParam->mUAVMipSlice)
								{
									[pDescriptorSet->mArgumentEncoder
										setTexture:texture->pMtlUAVDescriptors[pParam->mUAVMipSlice]
											atIndex:pDesc->mHandleIndex + arrayStart + j];
								}
								else
								{
									[pDescriptorSet->mArgumentEncoder setTexture:pParam->ppTextures[j]->mtlTexture
																		 atIndex:pDesc->mHandleIndex + arrayStart + j];
								}
								
								if (texture->mRT || texture->mUav || !texture->pAllocation)
								{
									TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, texture->mtlTexture);
								}
							}
						}
					}
					else
#endif
					{
						ASSERT(arrayCount == 1 && "Array not supported");
						RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[pDesc->mHandleIndex];
						pData->mBinding = pDesc->mReg;
						pData->mStage = pDesc->mUsedStages;

						if (pParam->mBindStencilResource)
						{
							pData->pResource = pParam->ppTextures[0]->mtlStencilTexture;
						}
						else if (pParam->mUAVMipSlice)
						{
							pData->pResource = pParam->ppTextures[0]->pMtlUAVDescriptors[pParam->mUAVMipSlice];
						}
						else
						{
							pData->pResource = pParam->ppTextures[0]->mtlTexture;
						}
					}
					break;
				}
				case DESCRIPTOR_TYPE_RW_TEXTURE:
				{
#if defined(ENABLE_ARGUMENT_BUFFERS)
					if (pDesc->mIsArgumentBufferField)
					{
						if (@available(macOS 10.13, iOS 11.0, *))
						{
							if (pParam->mBindMipChain)
							{
								for (uint32_t j = 0; j < pParam->ppTextures[0]->mMipLevels; ++j)
								{
									[pDescriptorSet->mArgumentEncoder setTexture:pParam->ppTextures[0]->pMtlUAVDescriptors[j]
																		 atIndex:pDesc->mHandleIndex + arrayStart + j];
								}
								
								TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, pParam->ppTextures[0]->mtlTexture);
							}
							else
							{
								for (uint32_t j = 0; j < arrayCount; ++j)
								{
									const Texture* texture = pParam->ppTextures[j];
									
									[pDescriptorSet->mArgumentEncoder
										setTexture:texture->pMtlUAVDescriptors[pParam->mUAVMipSlice]
										   atIndex:pDesc->mHandleIndex + arrayStart + j];
									
									TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, texture->mtlTexture);
								}
							}
						}
					}
					else
#endif
					{
						ASSERT(arrayCount == 1 && "Array not supported");
						RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[pDesc->mHandleIndex];
						pData->mBinding = pDesc->mReg;
						pData->mStage = pDesc->mUsedStages;
						pData->pResource = pParam->ppTextures[0]->pMtlUAVDescriptors[pParam->mUAVMipSlice];
					}
					break;
				}
				case DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER:
				case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER:
				case DESCRIPTOR_TYPE_BUFFER_RAW:
				case DESCRIPTOR_TYPE_RW_BUFFER:
				case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
				{
#if defined(ENABLE_ARGUMENT_BUFFERS)
					if (pDesc->mIsArgumentBufferField)
					{
						if (@available(macOS 10.13, iOS 11.0, *))
						{
							if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
							{
								if (pRenderer->pActiveGpuSettings->mIndirectCommandBuffer)
								{
									BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
								}
							}
							else
							{
								for (uint32_t j = 0; j < arrayCount; ++j)
								{
									Buffer* buffer = pParam->ppBuffers[j];
									uint64_t      offset = pParam->ppBuffers[j]->mOffset;

									[pDescriptorSet->mArgumentEncoder
										setBuffer:buffer->mtlBuffer
										   offset:offset + (pParam->pRanges ? pParam->pRanges[j].mOffset : 0) atIndex:pDesc->mHandleIndex + arrayStart + j];
									
									if ((pDesc->mUsage & MTLResourceUsageWrite) || !buffer->pAllocation)
									{
										TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, buffer->mtlBuffer);
									}
								}
								
								if (pRenderer->pActiveGpuSettings->mIndirectCommandBuffer && pParam->mBindICB)
								{
									const uint32_t        paramIndex = pParam->mBindByIndex ? pParam->mICBIndex : UINT32_MAX;
									const DescriptorInfo* pDesc = NULL;

									if (paramIndex != UINT32_MAX)
									{
										pDesc = &pRootSignature->pDescriptors[paramIndex];
									}
									else
									{
										pDesc = get_descriptor(pRootSignature, pParam->pICBName);
									}
									
									if (pDesc)
									{
										BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
									}
								}
							}
						}
					}
					else
#endif
					{
						ASSERT(arrayCount == 1 && "Array not supported");
						RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pBuffers[pDesc->mHandleIndex];
						pData->mBinding = pDesc->mReg;
						pData->mStage = pDesc->mUsedStages;
						pData->pResource = pParam->ppBuffers[0]->mtlBuffer;
						pData->mOffset = (uint32_t)(pParam->ppBuffers[0]->mOffset + (pParam->pRanges ? pParam->pRanges[0].mOffset : 0));
					}

					break;
				}
				case DESCRIPTOR_TYPE_RAY_TRACING:
				{
					// todo?
					ASSERT(false);
					break;
				}
				default:
				{
					ASSERT(0);    // unsupported descriptor type
					break;
				}
			}
		}
	}
}
/************************************************************************/
// Logging
/************************************************************************/
// Proxy log callback
static void internal_log(LogLevel level, const char* msg, const char* component)
{
	LOGF(level, "%s ( %s )", component, msg);
}

// Resource allocation statistics.
void mtl_calculateMemoryStats(Renderer* pRenderer, char** ppStats) { vmaBuildStatsString(pRenderer->pVmaAllocator, ppStats, VK_TRUE); }

void mtl_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
	VmaTotalStatistics stats;
	vmaCalculateStatistics(pRenderer->pVmaAllocator, &stats);
	*usedBytes = stats.total.statistics.allocationBytes;
	*totalAllocatedBytes = stats.total.statistics.blockBytes;
}

void mtl_freeMemoryStats(Renderer* pRenderer, char* pStats)
{
	vmaFreeStatsString(pRenderer->pVmaAllocator, pStats);
}
/************************************************************************/
// Shader utility functions
/************************************************************************/
API_AVAILABLE(macos(10.12), ios(10.0))
static void util_specialize_function(id<MTLLibrary> lib, const ShaderConstant* pConstants, uint32_t count, id<MTLFunction>* inOutFunction)
{
	id<MTLFunction> function = *inOutFunction;
	MTLFunctionConstantValues* values = [[MTLFunctionConstantValues alloc] init];
	NSArray<MTLFunctionConstant*>* mtlConstants = [[function functionConstantsDictionary] allValues];
	for (uint32_t i = 0; i < count; ++i)
	{
		const ShaderConstant* constant = &pConstants[i];
		for (const MTLFunctionConstant* mtlConstant in mtlConstants)
		{
			if (mtlConstant.index == constant->mIndex)
			{
				[values setConstantValue:constant->pValue type:mtlConstant.type atIndex:mtlConstant.index];
			}
		}
	}
	
	*inOutFunction = [lib newFunctionWithName:[function name] constantValues:values error:nil];
}
/************************************************************************/
// Pipeline state functions
/************************************************************************/
MTLColorWriteMask util_to_color_mask(uint32_t colorMask)
{
	MTLColorWriteMask mtlMask =
		((colorMask & 0x1) ? MTLColorWriteMaskRed   : 0u )|
		((colorMask & 0x2) ? MTLColorWriteMaskGreen : 0u )|
		((colorMask & 0x4) ? MTLColorWriteMaskBlue  : 0u )|
		((colorMask & 0x8) ? MTLColorWriteMaskAlpha : 0u );
	return mtlMask;
}

void util_to_blend_desc(const BlendStateDesc* pDesc, MTLRenderPipelineColorAttachmentDescriptorArray* attachments, uint32_t attachmentCount)
{
	int blendDescIndex = 0;
#ifdef _DEBUG
	for (uint32_t i = 0; i < attachmentCount; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			ASSERT(pDesc->mSrcFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mSrcAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mDstAlphaFactors[blendDescIndex] < BlendConstant::MAX_BLEND_CONSTANTS);
			ASSERT(pDesc->mBlendModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
			ASSERT(pDesc->mBlendAlphaModes[blendDescIndex] < BlendMode::MAX_BLEND_MODES);
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}

	blendDescIndex = 0;
#endif

	// Go over each RT blend state.
	for (uint32_t i = 0; i < attachmentCount; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			bool blendEnable =
				(gMtlBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != MTLBlendFactorOne ||
				 gMtlBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]] != MTLBlendFactorZero ||
				 gMtlBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]] != MTLBlendFactorOne ||
				 gMtlBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]] != MTLBlendFactorZero);

			attachments[i].blendingEnabled = blendEnable;
			attachments[i].rgbBlendOperation = gMtlBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			attachments[i].alphaBlendOperation = gMtlBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
			attachments[i].sourceRGBBlendFactor = gMtlBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			attachments[i].destinationRGBBlendFactor = gMtlBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			attachments[i].sourceAlphaBlendFactor = gMtlBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			attachments[i].destinationAlphaBlendFactor = gMtlBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
			attachments[i].writeMask = util_to_color_mask(pDesc->mMasks[blendDescIndex]);
		}

		if (pDesc->mIndependentBlend)
			++blendDescIndex;
	}
}

id<MTLDepthStencilState> util_to_depth_state(Renderer* pRenderer, const DepthStateDesc* pDesc)
{
	ASSERT(pDesc->mDepthFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthFrontFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilFrontPass < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackFunc < CompareMode::MAX_COMPARE_MODES);
	ASSERT(pDesc->mStencilBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mDepthBackFail < StencilOp::MAX_STENCIL_OPS);
	ASSERT(pDesc->mStencilBackPass < StencilOp::MAX_STENCIL_OPS);

	MTLDepthStencilDescriptor* descriptor = [[MTLDepthStencilDescriptor alloc] init];
	// Set comparison function to always if depth test is disabled
	descriptor.depthCompareFunction = pDesc->mDepthTest ? gMtlComparisonFunctionTranslator[pDesc->mDepthFunc] : MTLCompareFunctionAlways;
	descriptor.depthWriteEnabled = pDesc->mDepthWrite;
    if(pDesc->mStencilTest)
    {
        descriptor.backFaceStencil.stencilCompareFunction = gMtlComparisonFunctionTranslator[pDesc->mStencilBackFunc];
        descriptor.backFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthBackFail];
        descriptor.backFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilBackFail];
        descriptor.backFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilBackPass];
        descriptor.backFaceStencil.readMask = pDesc->mStencilReadMask;
        descriptor.backFaceStencil.writeMask = pDesc->mStencilWriteMask;
        descriptor.frontFaceStencil.stencilCompareFunction = gMtlComparisonFunctionTranslator[pDesc->mStencilFrontFunc];
        descriptor.frontFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthFrontFail];
        descriptor.frontFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontFail];
        descriptor.frontFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontPass];
        descriptor.frontFaceStencil.readMask = pDesc->mStencilReadMask;
        descriptor.frontFaceStencil.writeMask = pDesc->mStencilWriteMask;
    }
    else
    {
        descriptor.backFaceStencil = nil;
        descriptor.frontFaceStencil = nil;
    }

	id<MTLDepthStencilState> ds = [pRenderer->pDevice newDepthStencilStateWithDescriptor:descriptor];
	ASSERT(ds);

	return ds;
}
/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
void add_default_resources(Renderer* pRenderer)
{
	TextureDesc texture1DDesc = {};
	texture1DDesc.mArraySize = 1;
	texture1DDesc.mDepth = 1;
	texture1DDesc.mFormat = TinyImageFormat_R8_UNORM;
	texture1DDesc.mHeight = 1;
	texture1DDesc.mMipLevels = 1;
	texture1DDesc.mSampleCount = SAMPLE_COUNT_1;
	texture1DDesc.mStartState = RESOURCE_STATE_COMMON;
	texture1DDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
	texture1DDesc.mWidth = 2;
	texture1DDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
	addTexture(pRenderer, &texture1DDesc, &pDefault1DTexture);

	TextureDesc texture1DArrayDesc = texture1DDesc;
	texture1DArrayDesc.mArraySize = 2;
	addTexture(pRenderer, &texture1DArrayDesc, &pDefault1DTextureArray);

	TextureDesc texture2DDesc = texture1DDesc;
	texture2DDesc.mHeight = 2;
	addTexture(pRenderer, &texture2DDesc, &pDefault2DTexture);

	TextureDesc texture2DArrayDesc = texture2DDesc;
	texture2DArrayDesc.mArraySize = 2;
	addTexture(pRenderer, &texture2DArrayDesc, &pDefault2DTextureArray);

	TextureDesc texture3DDesc = texture2DDesc;
	texture3DDesc.mDepth = 2;
	addTexture(pRenderer, &texture3DDesc, &pDefault3DTexture);

	TextureDesc textureCubeDesc = texture2DDesc;
	textureCubeDesc.mArraySize = 6;
	textureCubeDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
	addTexture(pRenderer, &textureCubeDesc, &pDefaultCubeTexture);

	TextureDesc textureCubeArrayDesc = textureCubeDesc;
	textureCubeArrayDesc.mArraySize *= 2;
#ifndef TARGET_IOS
	addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultCubeTextureArray);
#elif defined(ENABLE_TEXTURE_CUBE_ARRAYS)
	if (@available(iOS 11.0, *))
	{
		if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
		{
			addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultCubeTextureArray);
		}
	}
#endif

	BufferDesc bufferDesc = {};
	bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	bufferDesc.mStartState = RESOURCE_STATE_COMMON;
	bufferDesc.mSize = sizeof(uint32_t);
	bufferDesc.mFirstElement = 0;
	bufferDesc.mElementCount = 1;
	bufferDesc.mStructStride = sizeof(uint32_t);
	bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
	addBuffer(pRenderer, &bufferDesc, &pDefaultBuffer);

	SamplerDesc samplerDesc = {};
	samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
	addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

	DepthStateDesc depthStateDesc = {};
	depthStateDesc.mDepthFunc = CMP_ALWAYS;
	depthStateDesc.mDepthTest = false;
	depthStateDesc.mDepthWrite = false;
	depthStateDesc.mStencilBackFunc = CMP_ALWAYS;
	depthStateDesc.mStencilFrontFunc = CMP_ALWAYS;
	depthStateDesc.mStencilReadMask = 0xFF;
	depthStateDesc.mStencilWriteMask = 0xFF;
	pDefaultDepthState = util_to_depth_state(pRenderer, &depthStateDesc);

	gDefaultRasterizerState = {};
	gDefaultRasterizerState.mCullMode = CULL_MODE_NONE;
}

void remove_default_resources(Renderer* pRenderer)
{
	removeTexture(pRenderer, pDefault1DTexture);
	removeTexture(pRenderer, pDefault1DTextureArray);
	removeTexture(pRenderer, pDefault2DTexture);
	removeTexture(pRenderer, pDefault2DTextureArray);
	removeTexture(pRenderer, pDefault3DTexture);
	removeTexture(pRenderer, pDefaultCubeTexture);

	if (pDefaultCubeTextureArray)
	{
		removeTexture(pRenderer, pDefaultCubeTextureArray);
	}

	removeBuffer(pRenderer, pDefaultBuffer);
	removeSampler(pRenderer, pDefaultSampler);

	pDefaultDepthState = nil;
}

// -------------------------------------------------------------------------------------------------
// API functions
// -------------------------------------------------------------------------------------------------

TinyImageFormat mtl_getRecommendedSwapchainFormat(bool hintHDR, bool hintSRGB) 
{ 
	if (hintSRGB)
		return TinyImageFormat_B8G8R8A8_SRGB;
	else
		return TinyImageFormat_B8G8R8A8_UNORM;
}

#ifndef TARGET_IOS
static uint32_t GetEntryProperty(io_registry_entry_t entry, CFStringRef propertyName)
{

	uint32_t value = 0;
	CFTypeRef cfProp = IORegistryEntrySearchCFProperty(entry,
													   kIOServicePlane,
													   propertyName,
													   kCFAllocatorDefault,
													   kIORegistryIterateRecursively |
													   kIORegistryIterateParents);
	if (cfProp)
	{
		const uint32_t* pValue = (const uint32_t*)(CFDataGetBytePtr((CFDataRef)cfProp));
		if (pValue)
		{
			value = *pValue;
		}
		CFRelease(cfProp);
	}

	return value;
}
#endif

void FillGPUVendorPreset(id<MTLDevice> gpu, GPUVendorPreset& gpuVendor)
{
	strncpy(gpuVendor.mGpuName, [gpu.name UTF8String], MAX_GPU_VENDOR_STRING_LENGTH);
	
	uint32_t familyTier = getFamilyTier(gpu);
	NSString *version = [[NSProcessInfo processInfo] operatingSystemVersionString];
	snprintf(gpuVendor.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "GpuFamily: %d OSVersion: %s", familyTier, version.UTF8String);
	
#ifdef TARGET_IOS
	constexpr uint32_t kAppleVendorId = 0x106b;
	snprintf(gpuVendor.mVendorId, MAX_GPU_VENDOR_STRING_LENGTH, "0x%0x", kAppleVendorId);
	snprintf(gpuVendor.mModelId, MAX_GPU_VENDOR_STRING_LENGTH, "0x%0x", familyTier);
#else
	io_registry_entry_t entry;
	uint64_t regID = 0;
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		regID = [gpu registryID];
	}
	if (regID)
	{
		entry = IOServiceGetMatchingService(kIOMasterPortDefault, IORegistryEntryIDMatching(regID));
		if (entry)
		{
			// That returned the IOGraphicsAccelerator nub. Its parent, then, is the actual PCI device.
			io_registry_entry_t parent;
			if (IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent) == kIOReturnSuccess)
			{
				uint32_t vendorID = GetEntryProperty(parent, CFSTR("vendor-id"));
				uint32_t deviceID = GetEntryProperty(parent, CFSTR("device-id"));
				snprintf(gpuVendor.mVendorId, sizeof(gpuVendor.mVendorId), "0x%0x", vendorID);
				snprintf(gpuVendor.mModelId, sizeof(gpuVendor.mModelId), "0x%0x", deviceID);
				IOObjectRelease(parent);
			}
			IOObjectRelease(entry);
		}
	}
#endif
}

uint32_t queryThreadExecutionWidth(id<MTLDevice> gpu)
{
	if (!gpu)
		return 0;

	NSError*  error = nil;
	NSString* defaultComputeShader =
		@"#include <metal_stdlib>\n"
		 "using namespace metal;\n"
		 "kernel void simplest(texture2d<float, access::write> output [[texture(0)]],uint2 gid [[thread_position_in_grid]])\n"
		 "{output.write(float4(0, 0, 0, 1), gid);}";

	// Load all the shader files with a .metal file extension in the project
	id<MTLLibrary> defaultLibrary = [gpu newLibraryWithSource:defaultComputeShader options:nil error:&error];

	if (error != nil)
	{
		LOGF(LogLevel::eWARNING, "Could not create library for simple compute shader: %s", [[error localizedDescription] UTF8String]);
		return 0;
	}

	// Load the kernel function from the library
	id<MTLFunction> kernelFunction = [defaultLibrary newFunctionWithName:@"simplest"];

	// Create a compute pipeline state
	id<MTLComputePipelineState> computePipelineState = [gpu newComputePipelineStateWithFunction:kernelFunction error:&error];
	if (error != nil)
	{
		LOGF(
			LogLevel::eWARNING, "Could not create compute pipeline state for simple compute shader: %s",
			[[error localizedDescription] UTF8String]);
		return 0;
	}

	return (uint32_t)computePipelineState.threadExecutionWidth;
}

static MTLResourceOptions gMemoryOptions[VK_MAX_MEMORY_TYPES] =
{
	MTLResourceStorageModePrivate,
	MTLResourceStorageModePrivate,
	MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined,
	MTLResourceStorageModeShared | MTLResourceCPUCacheModeDefaultCache,
};

static MTLCPUCacheMode gMemoryCacheModes[VK_MAX_MEMORY_TYPES] =
{
	MTLCPUCacheModeDefaultCache,
	MTLCPUCacheModeDefaultCache,
	MTLCPUCacheModeWriteCombined,
	MTLCPUCacheModeDefaultCache,
};

static MTLStorageMode gMemoryStorageModes[VK_MAX_MEMORY_TYPES] =
{
	MTLStorageModePrivate,
	MTLStorageModePrivate,
	MTLStorageModeShared,
	MTLStorageModeShared,
};

#if defined(ENABLE_HEAPS)
void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
	pProperties->limits.bufferImageGranularity = 64;
}

void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
	pMemoryProperties->memoryHeapCount = VK_MAX_MEMORY_HEAPS;
	pMemoryProperties->memoryTypeCount = VK_MAX_MEMORY_TYPES;
	
	constexpr uint32_t sharedHeapIndex = VK_MAX_MEMORY_HEAPS - 1;

	pMemoryProperties->memoryHeaps[0].size = physicalDevice->pActiveGpuSettings->mVRAM;
	pMemoryProperties->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
	
#ifndef TARGET_APPLE_ARM64
	pMemoryProperties->memoryHeaps[1].size = [[NSProcessInfo processInfo] physicalMemory];
#endif

	// GPU friendly for textures, gpu buffers, ...
	pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_ONLY].heapIndex = 0;
	pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_ONLY].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_ONLY_COLOR_RTS] = pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_ONLY];

	// Only for staging data
	pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_TO_CPU].heapIndex = sharedHeapIndex;
	pMemoryProperties->memoryTypes[MEMORY_TYPE_GPU_TO_CPU].propertyFlags =
	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

	// For frequently changing data on CPU and read once on GPU like constant buffers
	pMemoryProperties->memoryTypes[MEMORY_TYPE_CPU_TO_GPU].heapIndex = sharedHeapIndex;
	pMemoryProperties->memoryTypes[MEMORY_TYPE_CPU_TO_GPU].propertyFlags =
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

API_AVAILABLE(macos(10.13), ios(10.0))
VkResult vkAllocateMemory(
	VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
	MTLResourceOptions resourceOptions = gMemoryOptions[pAllocateInfo->memoryTypeIndex];
	MTLCPUCacheMode    cacheMode = gMemoryCacheModes[pAllocateInfo->memoryTypeIndex];
	MTLStorageMode     storageMode = gMemoryStorageModes[pAllocateInfo->memoryTypeIndex];
	MTLHeapDescriptor* heapDesc = [[MTLHeapDescriptor alloc] init];
	[heapDesc setSize:pAllocateInfo->allocationSize];

	UNREF_PARAM(cacheMode);
	UNREF_PARAM(storageMode);

#if defined(ENABLE_HEAP_PLACEMENT)
	if (@available(macOS 10.15, iOS 13.0, *))
	{
		if (device->pActiveGpuSettings->mPlacementHeaps)
		{
			[heapDesc setType:MTLHeapTypePlacement];
		}
	}
#endif

	if (@available(macOS 10.15, iOS 13.0, *))
	{
#if defined(ENABLE_HEAP_RESOURCE_OPTIONS)
		[heapDesc setResourceOptions:resourceOptions];
#endif
	}
	else
	{
		// Fallback on earlier versions
		[heapDesc setStorageMode:storageMode];
		[heapDesc setCpuCacheMode:cacheMode];
	}

	// We cannot create heap with MTLStorageModeShared in macOS
	// Instead we allocate a big buffer which acts as our heap
	// and create suballocations out of it
#if !defined(TARGET_APPLE_ARM64)
	if (MTLStorageModeShared != storageMode)
#endif
	{
		VkDeviceMemory_T* memory = (VkDeviceMemory_T*)pAllocator->pfnAllocation(NULL, sizeof(VkDeviceMemory_T), alignof(VkDeviceMemory_T), 0);
		memset(memory, 0, sizeof(VkDeviceMemory_T));
		memory->pHeap = [device->pDevice newHeapWithDescriptor:heapDesc];
		ASSERT(memory->pHeap);
		// need to return out of device memory here as well
		// as new heap will return null with no error message
		// if device is out of available memory
		*pMemory = memory;
		if (!memory->pHeap)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		if (MEMORY_TYPE_GPU_ONLY_COLOR_RTS != pAllocateInfo->memoryTypeIndex)
		{
			if ((device->mHeapCount + 1) > device->mHeapCapacity)
			{
				if (!device->mHeapCapacity)
				{
					device->mHeapCapacity = 1;
				}
				else
				{
					device->mHeapCapacity <<= 1;
				}
				device->pHeaps = (NOREFS id<MTLHeap>*)pAllocator->pfnReallocation(NULL, device->pHeaps, device->mHeapCapacity * sizeof(id<MTLHeap>), alignof(id<MTLHeap>), 0);
			}

			device->pHeaps[device->mHeapCount++] = memory->pHeap;
		}
	}
	
#if !defined(TARGET_APPLE_ARM64)
	if (!(*pMemory) && MTLStorageModeShared == storageMode)
	{
		VkDeviceMemory_T* memory = (VkDeviceMemory_T*)pAllocator->pfnAllocation(NULL, sizeof(VkDeviceMemory_T), alignof(VkDeviceMemory_T), 0);
		memset(memory, 0, sizeof(VkDeviceMemory_T));
		memory->pHeap = [device->pDevice newBufferWithLength:pAllocateInfo->allocationSize options:resourceOptions];
		((id<MTLBuffer>)memory->pHeap).label = @"Suballocation Buffer";
		*pMemory = memory;
	}
#endif

	ASSERT(*pMemory);

	if (!(*pMemory))
	{
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}

	return VK_SUCCESS;
}

void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
	if (@available(macOS 10.13, iOS 10.0, *))
	{
		uint32_t heapIndex = UINT32_MAX;
		for (uint32_t i = 0; i < device->mHeapCount; ++i)
		{
			if ([memory->pHeap isEqual:device->pHeaps[i]])
			{
				heapIndex = i;
				break;
			}
		}

		if (heapIndex != UINT32_MAX)
		{
			// Put the null heap at the end
			for (uint32_t i = heapIndex + 1; i < device->mHeapCount; ++i)
			{
				device->pHeaps[i - 1] = device->pHeaps[i];
			}
			--device->mHeapCount;
		}

		[memory->pHeap setPurgeableState:MTLPurgeableStateEmpty];
		memory->pHeap = nil;
		pAllocator->pfnFree(NULL, memory);
	}
}

// Stub functions to prevent VMA from asserting during runtime
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) { return nullptr; }
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice instance, const char* pName) { return nullptr; }
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) { return VK_SUCCESS; }
void     vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {}
VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) { return VK_SUCCESS; }
VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) { return VK_SUCCESS; }
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return VK_SUCCESS; }
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return VK_SUCCESS; }
void     vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {}
void     vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {}
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) { return VK_SUCCESS; }
void     vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {}
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) { return VK_SUCCESS; }
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {}
void vkCmdCopyBuffer(
	VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {}

static VmaAllocationInfo
	util_render_alloc(Renderer* pRenderer, ResourceMemoryUsage memUsage, MemoryType memType, NSUInteger size, NSUInteger align, VmaAllocation* pAlloc)
{
	VmaAllocationInfo    allocInfo = {};
	VkMemoryRequirements memReqs = {};
	memReqs.alignment = (VkDeviceSize)align;
	memReqs.size = (VkDeviceSize)size;
	memReqs.memoryTypeBits = 1 << memType;
	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
	allocCreateInfo.usage = (VmaMemoryUsage)memUsage;
	VkResult res = vmaAllocateMemory(pRenderer->pVmaAllocator, &memReqs, &allocCreateInfo, pAlloc, &allocInfo);
	ASSERT(VK_SUCCESS == res);
	if (VK_SUCCESS != res)
	{
		LOGF(eERROR, "Failed to allocate memory for texture");
		return {};
	}

	return allocInfo;
}

// Fallback to relying on automatic placement of resources through MTLHeap
// VMA is of no use here as we cannot specify the offset to create a certain
// resource in the heap
static uint32_t util_find_heap_with_space(Renderer* pRenderer, MemoryType memUsage, MTLSizeAndAlign sizeAlign)
{
	// No heaps available which fulfill our requirements. Create a new heap
	uint32_t index = pRenderer->mHeapCount;

	MTLStorageMode  storageMode = gMemoryStorageModes[memUsage];
	MTLCPUCacheMode cacheMode = gMemoryCacheModes[memUsage];

	if (@available(macOS 10.13, iOS 10.0, *))
	{
		MutexLock lock(*pRenderer->pHeapMutex);

		for (uint32_t i = 0; i < pRenderer->mHeapCount; ++i)
		{
			if ([pRenderer->pHeaps[i] storageMode] == storageMode && [pRenderer->pHeaps[i] cpuCacheMode] == cacheMode)
			{
				uint64_t maxAllocationSize = [pRenderer->pHeaps[i] maxAvailableSizeWithAlignment:sizeAlign.align];

				if (maxAllocationSize >= sizeAlign.size)
				{
					return i;
				}
			}
		}

		// Allocate 1/8, 1/4, 1/2 as first blocks.
		uint64_t       newBlockSize = VMA_SMALL_HEAP_MAX_SIZE;
		uint32_t       newBlockSizeShift = 0;
		const uint32_t NEW_BLOCK_SIZE_SHIFT_MAX = 3;

		const VkDeviceSize maxExistingBlockSize = 0;
		for (uint32_t i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
		{
			const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
			if (smallerNewBlockSize > maxExistingBlockSize && smallerNewBlockSize >= sizeAlign.size * 2)
			{
				newBlockSize = smallerNewBlockSize;
				++newBlockSizeShift;
			}
			else
			{
				break;
			}
		}

		VkDeviceMemory       heap = nil;
		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = max((uint64_t)sizeAlign.size, newBlockSize);
		allocInfo.memoryTypeIndex = memUsage;
		VkResult res = vkAllocateMemory(pRenderer, &allocInfo, NULL, &heap);
		gMtlAllocationCallbacks.pfnFree(NULL, heap);
		UNREF_PARAM(res);
		ASSERT(VK_SUCCESS == res);
	}

	return index;
}
#endif

#ifdef TARGET_IOS
static uint64_t util_get_free_memory()
{
	mach_port_t            hostPort;
	mach_msg_type_number_t hostSize;
	vm_size_t              pageSize;

	hostPort = mach_host_self();
	hostSize = sizeof(vm_statistics_data_t) / sizeof(integer_t);
	host_page_size(hostPort, &pageSize);

	vm_statistics_data_t vmStat;

	if (host_statistics(hostPort, HOST_VM_INFO, (host_info_t)&vmStat, &hostSize) != KERN_SUCCESS)
	{
		NSLog(@"Failed to fetch vm statistics");
		return UINT64_MAX;
	}

	return vmStat.free_count * pageSize;
}
#endif

static void FillGPUSettings(id<MTLDevice> gpu, GPUSettings* pOutSettings)
{
	GPUVendorPreset& gpuVendor = pOutSettings->mGpuVendorPreset;
	FillGPUVendorPreset(gpu, gpuVendor);
	
#ifdef TARGET_IOS
#if defined(ENABLE_OS_PROC_MEMORY)
	if (@available(iOS 13.0, *))
	{
		pOutSettings->mVRAM = os_proc_available_memory();
	}
#endif

	if (pOutSettings->mVRAM <= 0)
	{
		pOutSettings->mVRAM = util_get_free_memory();
		if (pOutSettings->mVRAM <= 0)
		{
			pOutSettings->mVRAM = [[NSProcessInfo processInfo] physicalMemory];
		}
	}
#else
	if (@available(macOS 10.12, *))
	{
		pOutSettings->mVRAM = [gpu recommendedMaxWorkingSetSize];
	}
#endif
	ASSERT(pOutSettings->mVRAM);
	
	pOutSettings->mUniformBufferAlignment = 256;
	pOutSettings->mUploadBufferTextureAlignment = 16;
	pOutSettings->mUploadBufferTextureRowAlignment = 1;
	pOutSettings->mMaxVertexInputBindings =
		MAX_VERTEX_BINDINGS;                      // there are no special vertex buffers for input in Metal, only regular buffers
	pOutSettings->mMultiDrawIndirect = false;    // multi draw indirect is not supported on Metal: only single draw indirect
	pOutSettings->mIndirectRootConstant = false;
	pOutSettings->mBuiltinDrawID = false;
	if (@available(macOS 10.14, iOS 12.0, *))
	{
#if defined(TARGET_IOS)
		pOutSettings->mIndirectCommandBuffer = [gpu supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1];
#else
		pOutSettings->mIndirectCommandBuffer = [gpu supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1];
#endif
	}

	// Features
	// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

#if defined(ENABLE_ROVS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		pOutSettings->mROVsSupported = [gpu areRasterOrderGroupsSupported];
	}
#endif
	pOutSettings->mTessellationSupported = true;
	pOutSettings->mGeometryShaderSupported = false;
	pOutSettings->mWaveLaneCount = queryThreadExecutionWidth(gpu);

	// Wave ops crash the compiler if not supported by gpu
	pOutSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
#ifdef TARGET_IOS
	pOutSettings->mDrawIndexVertexOffsetSupported = [gpu supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1];

	if (@available(iOS 12.0, *))
	{
		pOutSettings->mTessellationIndirectDrawSupported = [gpu supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily5_v1];
	}
	
	if (@available(iOS 13.0, *))
	{
		if ([gpu supportsFamily:(MTLGPUFamily)1006])    // family 6
		{
			pOutSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
		}
	}
#else
	pOutSettings->mTessellationIndirectDrawSupported = true;
	
	if (@available(macOS 10.14, *))
	{
		if ([gpu supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1])
		{
			pOutSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
		}
	}
#endif

#if defined(ENABLE_ARGUMENT_BUFFERS)
	// argument buffer capabilities
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		MTLArgumentBuffersTier abTier = gpu.argumentBuffersSupport;

		if (abTier == MTLArgumentBuffersTier2)
		{
			pOutSettings->mArgumentBufferMaxTextures = 500000;
		}
		else
		{
#if defined(TARGET_IOS)
#if defined(ENABLE_GPU_FAMILY)
			if (@available(macOS 10.15, iOS 13.0, *))
			{
				// iOS caps
				if ([gpu supportsFamily:MTLGPUFamilyApple4])    // A11 and higher
				{
					pOutSettings->mArgumentBufferMaxTextures = 96;
				}
				else
				{
					pOutSettings->mArgumentBufferMaxTextures = 31;
				}
			}
			else if (@available(iOS 12.0, *))
			{
				if ([gpu supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v2])
				{
					pOutSettings->mArgumentBufferMaxTextures = 96;
				}
			}
			else
			{
				pOutSettings->mArgumentBufferMaxTextures = 31;
			}
#endif
#else
			pOutSettings->mArgumentBufferMaxTextures = 128;
#endif
		}
	}
#endif

	// Heap and Placement heap support
#if defined(ENABLE_HEAPS)
#if defined(TARGET_IOS)
	// Heaps and Placement heaps supported on all iOS devices. Only restriction is iOS version
	if (@available(iOS 10.0, *))
	{
		BOOL isiOSAppOnMac = false;
		if (@available(iOS 14.0, *))
		{
			isiOSAppOnMac = [NSProcessInfo processInfo].isiOSAppOnMac;
		}
		pOutSettings->mHeaps = !isiOSAppOnMac;

#if defined(ENABLE_HEAP_PLACEMENT)
		if (@available(iOS 13.0, *))
		{
			pOutSettings->mPlacementHeaps = !isiOSAppOnMac;
		}
#endif
	}
#else
	// Disable heaps on low power devices due to driver bugs
	if (@available(macOS 10.13, *))
	{
		pOutSettings->mHeaps = [gpu isLowPower] ? 0 : 1;
		if (pOutSettings->mHeaps)
		{
#if defined(ENABLE_HEAP_PLACEMENT)
			if (@available(macOS 10.15, *))
			{
				pOutSettings->mPlacementHeaps = ([gpu supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily2_v1] ? 1 : 0);
			}
#endif
		}
	}
#endif
#endif
	
	gpuVendor.mPresetLevel = getGPUPresetLevel(gpuVendor.mVendorId, gpuVendor.mModelId, gpuVendor.mRevisionId);
}

static bool SelectBestGpu(const RendererDesc* settings, Renderer* pRenderer)
{
	RendererContextDesc contextDesc = {};
	RendererContext* pContext = settings->pContext;
	if (!pContext)
	{
		initRendererContext("Dummy", &contextDesc, &pContext);
	}
	else
	{
		ASSERT(settings->mGpuIndex >= 0 && settings->mGpuIndex < MAX_MULTIPLE_GPUS);
	}
	
	const GpuInfo* gpus = pContext->mGpus;
	uint32_t gpuCount = pContext->mGpuCount;
	uint32_t gpuIndex = settings->pContext ? settings->mGpuIndex : UINT32_MAX;
	
#ifdef TARGET_IOS
	gpuIndex = 0;
#else
	if (gpuCount < 1)
	{
		LOGF(LogLevel::eERROR, "Failed to enumerate any physical Metal devices");
		return false;
	}

	typedef bool (*DeviceBetterFunc)(uint32_t, uint32_t, const GpuInfo*);
	DeviceBetterFunc isDeviceBetter = [](uint32_t testIndex, uint32_t refIndex, const GpuInfo* gpus)
	{
		const GPUSettings& testSettings = gpus[testIndex].mSettings;
		const GPUSettings& refSettings = gpus[refIndex].mSettings;

		// First test the preset level
		if (testSettings.mGpuVendorPreset.mPresetLevel != refSettings.mGpuVendorPreset.mPresetLevel)
		{
			return testSettings.mGpuVendorPreset.mPresetLevel > refSettings.mGpuVendorPreset.mPresetLevel;
		}

		// Next test discrete vs integrated/software
		id<MTLDevice> testDevice = gpus[testIndex].pGPU;
		id<MTLDevice> refDevice = gpus[refIndex].pGPU;

		// Dont use headless GPUs (cant render to display)
		if (![testDevice isHeadless] && [refDevice isHeadless])
		{
			return true;
		}
		
		if ([testDevice isHeadless] && ![refDevice isHeadless])
		{
			return false;
		}
		
		// Prefer discrete GPU
		if (![testDevice isLowPower] && [refDevice isLowPower])
		{
			return true;
		}
		
		if ([testDevice isLowPower] && ![refDevice isLowPower])
		{
			return false;
		}
		
		return testSettings.mVRAM > refSettings.mVRAM;
	};
#endif

	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		const GPUVendorPreset& gpuVendor = gpus[i].mSettings.mGpuVendorPreset;
		LOGF(
			LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %s, Model ID: %s, Preset: %s, GPU Name: %s", i,
			 gpuVendor.mVendorId, gpuVendor.mModelId,
			presetLevelToString(gpuVendor.mPresetLevel), gpuVendor.mGpuName);
	}

#ifndef TARGET_IOS
#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU)
	selectActiveGpu(gpuSettings, &gpuIndex, gpuCount);
#else
	if (UINT32_MAX == gpuIndex)
	{
		for (uint32_t i = 0; i < gpuCount; ++i)
		{
			if (UINT32_MAX == gpuIndex || isDeviceBetter(i, gpuIndex, gpus))
			{
				gpuIndex = i;
			}
		}
	}
#endif
#endif

	ASSERT(gpuIndex != UINT32_MAX);
	
	pRenderer->pDevice = gpus[gpuIndex].pGPU;
	pRenderer->pActiveGpuSettings = (GPUSettings*)tf_malloc(sizeof(GPUSettings));
	*pRenderer->pActiveGpuSettings = gpus[gpuIndex].mSettings;
	pRenderer->mLinkedNodeCount = 1;

	LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
	LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mGpuName);
	LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId);
	LOGF(LogLevel::eINFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId);
	LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel));
	
#if defined(ENABLE_ARGUMENT_BUFFERS)
	// argument buffer capabilities
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		MTLArgumentBuffersTier abTier = pRenderer->pDevice.argumentBuffersSupport;
		LOGF(LogLevel::eINFO, "Metal: Argument Buffer Tier: %lu", abTier);
		LOGF(LogLevel::eINFO, "Metal: Max Arg Buffer Textures: %u", pRenderer->pActiveGpuSettings->mArgumentBufferMaxTextures);
	}
#endif
	
	if (!settings->pContext)
	{
		exitRendererContext(pContext);
	}

	return true;
}

void mtl_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
	ASSERT(appName);
	ASSERT(pDesc);
	ASSERT(ppContext);

#ifdef TARGET_IOS
	id<MTLDevice> gpus[1] = { MTLCreateSystemDefaultDevice() };
	uint32_t gpuCount = 1;
#else
	NSArray <id<MTLDevice>>* gpus = MTLCopyAllDevices();
	uint32_t gpuCount = min((uint32_t)MAX_MULTIPLE_GPUS, (uint32_t)[gpus count]);
#endif

	if (gpuCount < 1)
	{
		LOGF(LogLevel::eERROR, "Failed to enumerate any physical Metal devices");
		*ppContext = NULL;
		return;
	}

	RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));
	pContext->mGpuCount = gpuCount;
	for (uint32_t i = 0; i < gpuCount; ++i)
	{
		pContext->mGpus[i].pGPU = gpus[i];
		FillGPUSettings(pContext->mGpus[i].pGPU, &pContext->mGpus[i].mSettings);
	}
	
	*ppContext = pContext;
}

void mtl_exitRendererContext(RendererContext* pContext)
{
	ASSERT(pContext);
	SAFE_FREE(pContext);
}

void mtl_initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	Renderer* pRenderer = (Renderer*)tf_calloc_memalign(1, alignof(Renderer), sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)tf_calloc(strlen(appName) + 1, sizeof(char));
	strcpy(pRenderer->pName, appName);

	pRenderer->mGpuMode = settings->mGpuMode;
	pRenderer->mEnableGpuBasedValidation = settings->mEnableGPUBasedValidation;
	pRenderer->mShaderTarget = settings->mShaderTarget;

	// Initialize the Metal bits
	{
		SelectBestGpu(settings, pRenderer);

		utils_caps_builder(pRenderer);

		LOGF(LogLevel::eINFO, "Metal: Heaps: %s", pRenderer->pActiveGpuSettings->mHeaps ? "true" : "false");
		LOGF(LogLevel::eINFO, "Metal: Placement Heaps: %s", pRenderer->pActiveGpuSettings->mPlacementHeaps ? "true" : "false");

#ifndef TARGET_IOS
		MTLFeatureSet featureSet = MTLFeatureSet_macOS_GPUFamily1_v1;
#else
		MTLFeatureSet featureSet = MTLFeatureSet_iOS_GPUFamily1_v1;
#endif
		while (1)
		{
			BOOL supports = [pRenderer->pDevice supportsFeatureSet:featureSet];
			if (!supports)
			{
				featureSet = (MTLFeatureSet)((uint64_t)featureSet - 1);
				break;
			}

			featureSet = (MTLFeatureSet)((uint64_t)featureSet + 1);
		}

		LOGF(LogLevel::eINFO, "Metal: GPU Family: %lu", featureSet);

		if (!pRenderer->pActiveGpuSettings->mPlacementHeaps)
		{
			pRenderer->pHeapMutex = (Mutex*)tf_malloc(sizeof(Mutex));
			initMutex(pRenderer->pHeapMutex);
		}

		// exit app if gpu being used has an office preset.
		if (pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel < GPU_PRESET_LOW)
		{
			ASSERT(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_LOW);

			//remove allocated name
			SAFE_FREE(pRenderer->pName);
			//set device to null
			pRenderer->pDevice = nil;
			//remove allocated renderer
			SAFE_FREE(pRenderer);

			LOGF(LogLevel::eERROR, "Selected GPU has an office Preset in gpu.cfg");
			LOGF(LogLevel::eERROR, "Office Preset is not supported by the Forge");

			*ppRenderer = NULL;
#ifdef AUTOMATED_TESTING
			//exit with success return code not to show failure on Jenkins
			exit(0);
#endif
			return;
		}

		// Create allocator
#if defined(ENABLE_HEAP_PLACEMENT)
		if (pRenderer->pActiveGpuSettings->mPlacementHeaps)
		{
			if (@available(macOS 10.15, iOS 13.0, *))
			{
				VmaAllocatorCreateInfo createInfo = {};
				VmaVulkanFunctions     vulkanFunctions = {};
				// Only 3 relevant functions for our memory allocation. The rest are there to just keep VMA from asserting failure
				vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
				vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
				vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
				// Stub
				vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
				vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
				vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
				vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
				vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
				vulkanFunctions.vkCreateImage = vkCreateImage;
				vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
				vulkanFunctions.vkDestroyImage = vkDestroyImage;
				vulkanFunctions.vkFreeMemory = vkFreeMemory;
				vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
				vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
				vulkanFunctions.vkMapMemory = vkMapMemory;
				vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
				vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
				vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
				vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

				createInfo.pVulkanFunctions = &vulkanFunctions;
				createInfo.pAllocationCallbacks = &gMtlAllocationCallbacks;
				createInfo.instance = pRenderer;
				createInfo.device = pRenderer;
				createInfo.physicalDevice = pRenderer;
				createInfo.instance = pRenderer;
				VkResult result = vmaCreateAllocator(&createInfo, &pRenderer->pVmaAllocator);
				UNREF_PARAM(result);
				ASSERT(VK_SUCCESS == result);
			}
		}

		LOGF(LogLevel::eINFO, "Renderer: VMA Allocator: %s", pRenderer->pVmaAllocator ? "true" : "false");
#endif

		// Create default resources.
		add_default_resources(pRenderer);

		// Renderer is good! Assign it to result!
		*(ppRenderer) = pRenderer;
	}
}

void mtl_exitRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	remove_default_resources(pRenderer);

#if defined(ENABLE_HEAP_PLACEMENT)
	if (@available(macOS 10.15, iOS 13.0, *))
	{
		if (pRenderer->pActiveGpuSettings->mPlacementHeaps)
		{
			vmaDestroyAllocator(pRenderer->pVmaAllocator);
		}
	}
#endif

	pRenderer->pDevice = nil;

	if (pRenderer->pHeapMutex)
	{
		destroyMutex(pRenderer->pHeapMutex);
	}

	SAFE_FREE(pRenderer->pHeapMutex);
#if defined(ENABLE_HEAPS)
	if (@available(macOS 10.13, iOS 10.0, *))
	{
		SAFE_FREE(pRenderer->pHeaps);
	}
#endif
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer);
}

void mtl_addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppFence);

	Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	pFence->pMtlSemaphore = dispatch_semaphore_create(0);
	pFence->mSubmitted = false;

	*ppFence = pFence;
}

void mtl_removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pFence);
	pFence->pMtlSemaphore = nil;

	SAFE_FREE(pFence);
}

void mtl_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(ppSemaphore);

	Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
	ASSERT(pSemaphore);

#if defined(ENABLE_EVENT_SEMAPHORE)
	if (@available(macOS 10.14, iOS 12.0, *))
	{
		pSemaphore->pMtlSemaphore = [pRenderer->pDevice newEvent];
	}
#endif

	*ppSemaphore = pSemaphore;
}

void mtl_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pSemaphore);

#if defined(ENABLE_EVENT_SEMAPHORE)
	if (@available(macOS 10.14, iOS 12.0, *))
	{
		pSemaphore->pMtlSemaphore = nil;
	}
#endif

	SAFE_FREE(pSemaphore);
}

void mtl_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pDesc);
	ASSERT(ppQueue);

	Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	pQueue->mNodeIndex = pDesc->mNodeIndex;
	pQueue->mType = pDesc->mType;
	pQueue->mtlCommandQueue = [pRenderer->pDevice newCommandQueueWithMaxCommandBufferCount:512];
	pQueue->mBarrierFlags = 0;
#if defined(ENABLE_FENCES)
	if (@available(macOS 10.13, iOS 10.0, *))
	{
		pQueue->mtlQueueFence = [pRenderer->pDevice newFence];
	}
#endif
	ASSERT(pQueue->mtlCommandQueue != nil);

	*ppQueue = pQueue;
}

void mtl_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pQueue);
	pQueue->mtlCommandQueue = nil;
#if defined(ENABLE_FENCES)
	if (@available(macOS 10.13, iOS 10.0, *))
	{
		pQueue->mtlQueueFence = nil;
	}
#endif

	SAFE_FREE(pQueue);
}

void mtl_addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppCmdPool);

	CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void mtl_removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pCmdPool);
	SAFE_FREE(pCmdPool);
}

void mtl_addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppCmd);

	Cmd* pCmd = (Cmd*)tf_calloc_memalign(1, alignof(Cmd), sizeof(Cmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pRenderer;
	pCmd->pQueue = pDesc->pPool->pQueue;

	*ppCmd = pCmd;
}

void mtl_removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	ASSERT(pCmd);
	pCmd->mtlCommandBuffer = nil;

	SAFE_FREE(pCmd);
}

void mtl_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
	//verify that ***cmd is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(cmdCount);
	ASSERT(pppCmd);

	Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
	ASSERT(ppCmds);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pRenderer, pDesc, &ppCmds[i]);
	}

	*pppCmd = ppCmds;
}

void mtl_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
	//verify that given command list is valid
	ASSERT(ppCmds);

	//remove every given cmd in array
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		removeCmd(pRenderer, ppCmds[i]);
	}

	SAFE_FREE(ppCmds);
}

void mtl_toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain)
{
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
	SwapChain* pSwapchain = *ppSwapchain;
	pSwapchain->mEnableVsync = !pSwapchain->mEnableVsync;
	//no need to have vsync on layers otherwise we will wait on semaphores
	//get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)pSwapchain->pForgeView.layer;

	//only available on mac OS.
	//VSync seems to be necessary on iOS.

	if (@available(macOS 10.13, *))
	{
		if (!pSwapchain->mEnableVsync)
		{
			layer.displaySyncEnabled = false;
		}
		else
		{
			layer.displaySyncEnabled = true;
		}
	}
#endif
}

void mtl_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = (SwapChain*)tf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
	ASSERT(pSwapChain);

	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);

#if !defined(TARGET_IOS)
	pSwapChain->pForgeView = (__bridge NSView*)pDesc->mWindowHandle.window;
	pSwapChain->pForgeView.autoresizesSubviews = TRUE;

	// no need to have vsync on layers otherwise we will wait on semaphores
	// get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
	// Make sure we are using same device for layer and renderer
	if (layer.device != pRenderer->pDevice)
	{
		LOGF(eWARNING, "Metal: CAMetalLayer device and Renderer device are not matching. Fixing by assigning Renderer device to CAMetalLayer device");
		layer.device = pRenderer->pDevice;
	}
	
	//only available on mac OS.
	//VSync seems to be necessary on iOS.
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
	if (@available(macOS 10.13, *))
	{
		if (!pDesc->mEnableVsync)
		{
			//This needs to be set to false to have working non-vsync
			//otherwise present drawables will wait on vsync.
			layer.displaySyncEnabled = false;
		}
		else
		{
			//This needs to be set to false to have working vsync
			layer.displaySyncEnabled = true;
		}
	}
#endif
	// Set the view pixel format to match the swapchain's pixel format.
	layer.pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mColorFormat);
#else
	pSwapChain->pForgeView = (__bridge UIView*)pDesc->mWindowHandle.window;
	pSwapChain->pForgeView.autoresizesSubviews = TRUE;

	if(@available(ios 13.0, *))
	{
		CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
		// Set the view pixel format to match the swapchain's pixel format.
		layer.pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mColorFormat);
	}
#endif

	pSwapChain->mMTKDrawable = nil;

	// Create present command buffer for the swapchain.
	pSwapChain->presentCommandBuffer = [pDesc->ppPresentQueues[0]->mtlCommandQueue commandBuffer];

	// Create the swapchain RT descriptor.
	RenderTargetDesc descColor = {};
	descColor.mWidth = pDesc->mWidth;
	descColor.mHeight = pDesc->mHeight;
	descColor.mDepth = 1;
	descColor.mArraySize = 1;
	descColor.mFormat = pDesc->mColorFormat;
	descColor.mClearValue = pDesc->mColorClearValue;
	descColor.mSampleCount = SAMPLE_COUNT_1;
	descColor.mSampleQuality = 0;
	descColor.mFlags |= TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;

	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
		pSwapChain->ppRenderTargets[i]->pTexture->mtlTexture = nil;
	}

	pSwapChain->mImageCount = pDesc->mImageCount;
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;

	*ppSwapChain = pSwapChain;
}

void mtl_removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pSwapChain);

	pSwapChain->presentCommandBuffer = nil;

	for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
		removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);

	SAFE_FREE(pSwapChain);
}

void mtl_addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	ASSERT(pRenderer);
	ASSERT(ppBuffer);
	ASSERT(pDesc);
	ASSERT((pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER) || pDesc->mSize > 0);
	ASSERT(pRenderer->pDevice != nil);

	Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
	ASSERT(pBuffer);

	uint64_t           allocationSize = pDesc->mSize;
	MemoryType         memoryType = MEMORY_TYPE_GPU_ONLY;
	if (RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pDesc->mMemoryUsage)
	{
		memoryType = MEMORY_TYPE_CPU_TO_GPU;
	}
	else if (RESOURCE_MEMORY_USAGE_CPU_ONLY == pDesc->mMemoryUsage || RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->mMemoryUsage)
	{
		memoryType = MEMORY_TYPE_GPU_TO_CPU;
	}
	MTLResourceOptions resourceOptions = gMemoryOptions[memoryType];
	// Align the buffer size to multiples of the dynamic uniform buffer minimum size
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
		allocationSize = round_up_64(allocationSize, minAlignment);
		((BufferDesc*)pDesc)->mAlignment = (uint32_t)minAlignment;
	}

	//	//Use isLowPower to determine if running intel integrated gpu
	//	//There's currently an intel driver bug with placed resources so we need to create
	//	//new resources that are GPU only in their own memory space
	//	//0x8086 is intel vendor id
	//	if (strcmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId, "0x8086") == 0 &&
	//		(ResourceMemoryUsage)pDesc->mMemoryUsage & RESOURCE_MEMORY_USAGE_GPU_ONLY)
	//		((BufferDesc*)pDesc)->mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

	// Indirect command buffer does not need backing buffer memory
	// Directly allocated through device
	if (pRenderer->pActiveGpuSettings->mIndirectCommandBuffer && (pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER))
	{
#if defined(ENABLE_INDIRECT_COMMAND_BUFFERS)
		if (@available(macOS 10.14, iOS 12.0, *))
		{
			MTLIndirectCommandBufferDescriptor* icbDescriptor = [MTLIndirectCommandBufferDescriptor alloc];

			switch (pDesc->mICBDrawType)
			{
				case INDIRECT_DRAW: icbDescriptor.commandTypes = MTLIndirectCommandTypeDraw; break;
				case INDIRECT_DRAW_INDEX: icbDescriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed; break;
				default: ASSERT(false);    // unsupported command type
			}

			icbDescriptor.inheritBuffers = true;
#if defined(ENABLE_INDIRECT_COMMAND_BUFFER_INHERIT_PIPELINE)
			if (@available(macOS 10.14, iOS 13.0, *))
			{
				icbDescriptor.inheritPipelineState = true;
			}
#endif

			pBuffer->mtlIndirectCommandBuffer = [pRenderer->pDevice newIndirectCommandBufferWithDescriptor:icbDescriptor
																						   maxCommandCount:pDesc->mICBMaxCommandCount
																								   options:0];
		}
#endif
	}

	if (!pRenderer->pActiveGpuSettings->mHeaps || (RESOURCE_MEMORY_USAGE_GPU_ONLY != pDesc->mMemoryUsage && RESOURCE_MEMORY_USAGE_CPU_TO_GPU != pDesc->mMemoryUsage))
	{
		pBuffer->mtlBuffer = [pRenderer->pDevice newBufferWithLength:allocationSize options:resourceOptions];
	}
#if defined(ENABLE_HEAPS)
	else
	{
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			// We cannot use heaps on macOS for upload buffers.
			// Instead we sub-allocate out of a buffer resource
			// which we treat as a pseudo-heap
#if defined(TARGET_APPLE_ARM64)
			bool canUseHeaps = true;
#else
			bool canUseHeaps = RESOURCE_MEMORY_USAGE_GPU_ONLY == pDesc->mMemoryUsage;
#endif

			MTLSizeAndAlign sizeAlign = [pRenderer->pDevice heapBufferSizeAndAlignWithLength:allocationSize options:resourceOptions];
			sizeAlign.align = max((NSUInteger)pDesc->mAlignment, sizeAlign.align);

			if (canUseHeaps)
			{
				if (pRenderer->pActiveGpuSettings->mPlacementHeaps)
				{
#if defined(ENABLE_HEAP_PLACEMENT)
					if (@available(macOS 10.15, iOS 13.0, *))
					{
						VmaAllocationInfo allocInfo =
							util_render_alloc(pRenderer, pDesc->mMemoryUsage, memoryType, sizeAlign.size, sizeAlign.align, &pBuffer->pAllocation);

						pBuffer->mtlBuffer = [allocInfo.deviceMemory->pHeap newBufferWithLength:allocationSize
																						options:resourceOptions
																						 offset:allocInfo.offset];
						ASSERT(pBuffer->mtlBuffer);
					}
#endif
				}
				else
				{
					// If placement heaps are not supported we cannot use VMA
					// Instead we have to rely on MTLHeap automatic placement
					uint32_t heapIndex = util_find_heap_with_space(pRenderer, memoryType, sizeAlign);

					// Fallback on earlier versions
					pBuffer->mtlBuffer = [pRenderer->pHeaps[heapIndex] newBufferWithLength:allocationSize options:resourceOptions];
					ASSERT(pBuffer->mtlBuffer);
				}
			}
			else
			{
				VmaAllocationInfo allocInfo =
					util_render_alloc(pRenderer, pDesc->mMemoryUsage, memoryType, sizeAlign.size, sizeAlign.align, &pBuffer->pAllocation);

				pBuffer->mtlBuffer = (id<MTLBuffer>)allocInfo.deviceMemory->pHeap;
				pBuffer->mOffset = allocInfo.offset;
			}
		}
	}
#endif
	
	if (pBuffer->mtlBuffer && !(resourceOptions & MTLResourceStorageModePrivate) &&
		(pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT))
	{
		mapBuffer(pRenderer, pBuffer, NULL);
	}

	pBuffer->mSize = (uint32_t)pDesc->mSize;
	pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
	pBuffer->mNodeIndex = pDesc->mNodeIndex;
	pBuffer->mDescriptors = pDesc->mDescriptors;
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		setBufferName(pRenderer, pBuffer, pDesc->pName);
	}
#endif
	*ppBuffer = pBuffer;
}

void mtl_removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer);

	if (!(pBuffer->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER))
	{
		ASSERT(pBuffer->mtlBuffer);
	}

	pBuffer->mtlBuffer = nil;

#if defined(ENABLE_INDIRECT_COMMAND_BUFFERS)
	if (@available(macOS 10.14, iOS 12.0, *))
	{
		pBuffer->mtlIndirectCommandBuffer = nil;
	}
#endif

	if (pBuffer->pAllocation)
	{
		vmaFreeMemory(pRenderer->pVmaAllocator, pBuffer->pAllocation);
	}

	SAFE_FREE(pBuffer);
}

void mtl_addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		internal_log(eERROR, "Multi-Sampled textures cannot have mip maps", "MetalRenderer");
		return;
	}

	add_texture(pRenderer, pDesc, ppTexture, false);
}

void mtl_removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pTexture);

#if defined(MTL_RAYTRACING_AVAILABLE)
	if (pTexture->mpsTextureAllocator)
	{
		if (@available(macOS 10.15, iOS 13.0, *))
		{
			[(id<MPSSVGFTextureAllocator>)pTexture->mpsTextureAllocator returnTexture:pTexture->mtlTexture];
			pTexture->mpsTextureAllocator = nil;
		}
	}
#endif

	pTexture->mtlTexture = nil;
    
	// Destroy descriptors
	if (pTexture->pMtlUAVDescriptors && !TinyImageFormat_HasStencil((TinyImageFormat)pTexture->mFormat))
	{
		for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
		{
			pTexture->pMtlUAVDescriptors[i] = nil;
		}
	}
	else
	{
		pTexture->mtlStencilTexture = nil;
	}

	if (pTexture->pAllocation)
	{
		vmaFreeMemory(pRenderer->pVmaAllocator, pTexture->pAllocation);
	}

	SAFE_FREE(pTexture);
}

void mtl_addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)tf_calloc_memalign(1, alignof(RenderTarget), sizeof(RenderTarget));
	ASSERT(pRenderTarget);

	TextureDesc rtDesc = {};
	rtDesc.mFlags = pDesc->mFlags;
	rtDesc.mWidth = pDesc->mWidth;
	rtDesc.mHeight = pDesc->mHeight;
	rtDesc.mDepth = pDesc->mDepth;
	rtDesc.mArraySize = pDesc->mArraySize;
	rtDesc.mMipLevels = pDesc->mMipLevels;
	rtDesc.mSampleCount = pDesc->mSampleCount;
	rtDesc.mSampleQuality = pDesc->mSampleQuality;
	rtDesc.mFormat = pDesc->mFormat;
	rtDesc.mClearValue = pDesc->mClearValue;
	rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	rtDesc.mStartState = RESOURCE_STATE_UNDEFINED;
	rtDesc.pNativeHandle = pDesc->pNativeHandle;
	rtDesc.mDescriptors |= pDesc->mDescriptors;
	rtDesc.pName = pDesc->pName;

	add_texture(pRenderer, &rtDesc, &pRenderTarget->pTexture, true);

	pRenderTarget->mClearValue = pDesc->mClearValue;
	pRenderTarget->mWidth = pDesc->mWidth;
	pRenderTarget->mHeight = pDesc->mHeight;
	pRenderTarget->mArraySize = pDesc->mArraySize;
	pRenderTarget->mDepth = pDesc->mDepth;
	pRenderTarget->mMipLevels = pDesc->mMipLevels;
	pRenderTarget->mSampleCount = pDesc->mSampleCount;
	pRenderTarget->mSampleQuality = pDesc->mSampleQuality;
	pRenderTarget->mFormat = pDesc->mFormat;
	
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	if (pDesc->mFlags & TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT)
	{
		RenderTargetDesc resolveRTDesc = *pDesc;
		resolveRTDesc.mFlags &= ~(TEXTURE_CREATION_FLAG_CREATE_RESOLVE_ATTACHMENT | TEXTURE_CREATION_FLAG_ON_TILE);
		resolveRTDesc.mSampleCount = SAMPLE_COUNT_1;
		addRenderTarget(pRenderer, &resolveRTDesc, &pRenderTarget->pResolveAttachment);
	}
#endif

	*ppRenderTarget = pRenderTarget;
}

void mtl_removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	removeTexture(pRenderer, pRenderTarget->pTexture);
	
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
	if (pRenderTarget->pResolveAttachment)
	{
		removeRenderTarget(pRenderer, pRenderTarget->pResolveAttachment);
	}
#endif
	
	SAFE_FREE(pRenderTarget);
}

void mtl_addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
	ASSERT(ppSampler);

	Sampler* pSampler = (Sampler*)tf_calloc_memalign(1, alignof(Sampler), sizeof(Sampler));
	ASSERT(pSampler);
	
	//default sampler lod values
	//used if not overriden by mSetLodRange or not Linear mipmaps
	float minSamplerLod = 0;
	float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? FLT_MAX : 0;
	//user provided lods
	if(pDesc->mSetLodRange)
	{
		minSamplerLod = pDesc->mMinLod;
		maxSamplerLod = pDesc->mMaxLod;
	}

	MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
	samplerDesc.minFilter = (pDesc->mMinFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.magFilter = (pDesc->mMagFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.mipFilter = (pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear);
	samplerDesc.maxAnisotropy = (pDesc->mMaxAnisotropy == 0 ? 1 : pDesc->mMaxAnisotropy);    // 0 is not allowed in Metal

	samplerDesc.lodMinClamp = minSamplerLod;
	samplerDesc.lodMaxClamp = maxSamplerLod;

	const MTLSamplerAddressMode* addressModeTable = NULL;
#if defined(ENABLE_SAMPLER_CLAMP_TO_BORDER)
	if (@available(macOS 10.12, *))
	{
		addressModeTable = gMtlAddressModeTranslator;
	}
	else
#endif
	{
		addressModeTable = gMtlAddressModeTranslatorFallback;
	}

	samplerDesc.sAddressMode = addressModeTable[pDesc->mAddressU];
	samplerDesc.tAddressMode = addressModeTable[pDesc->mAddressV];
	samplerDesc.rAddressMode = addressModeTable[pDesc->mAddressW];
	samplerDesc.compareFunction = gMtlComparisonFunctionTranslator[pDesc->mCompareFunc];
#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		samplerDesc.supportArgumentBuffers = YES;
	}
#endif

	pSampler->mtlSamplerState = [pRenderer->pDevice newSamplerStateWithDescriptor:samplerDesc];

	*ppSampler = pSampler;
}

void mtl_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pSampler);
	pSampler->mtlSamplerState = nil;
	SAFE_FREE(pSampler);
}

#ifdef TARGET_IOS
void mtl_addShader(Renderer* pRenderer, const ShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);

	uint32_t         reflectionCount = 0;
	ShaderReflection stageReflections[SHADER_STAGE_COUNT];
	const char*      entryNames[SHADER_STAGE_COUNT] = {};

	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		const char*		source = NULL;
		const char*		entryPoint = NULL;
		const char*		shaderName = NULL;
		ShaderMacro*	shaderMacros = NULL;
		uint32_t		shaderMacrosCount = 0;
		
		__strong id<MTLFunction>* compiled_code = NULL;

		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					source = pDesc->mVert.pCode;
					entryPoint = pDesc->mVert.pEntryPoint;
					shaderName = pDesc->mVert.pName;
					shaderMacros = pDesc->mVert.pMacros;
					shaderMacrosCount = pDesc->mVert.mMacroCount;
					compiled_code = &(pShaderProgram->mtlVertexShader);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					source = pDesc->mFrag.pCode;
					entryPoint = pDesc->mFrag.pEntryPoint;
					shaderName = pDesc->mFrag.pName;
					shaderMacros = pDesc->mFrag.pMacros;
					shaderMacrosCount = pDesc->mFrag.mMacroCount;
					compiled_code = &(pShaderProgram->mtlFragmentShader);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					source = pDesc->mComp.pCode;
					entryPoint = pDesc->mComp.pEntryPoint;
					shaderName = pDesc->mComp.pName;
					shaderMacros = pDesc->mComp.pMacros;
					shaderMacrosCount = pDesc->mComp.mMacroCount;
					compiled_code = &(pShaderProgram->mtlComputeShader);
				}
				break;
				default: break;
			}

			entryNames[reflectionCount] = entryPoint;

			// Create a NSDictionary for all the shader macros.
			NSNumberFormatter* numberFormatter =
				[[NSNumberFormatter alloc] init];    // Used for reading NSNumbers macro values from strings.
			numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;

			NSMutableDictionary* macroDictionary = [NSMutableDictionary dictionaryWithCapacity:shaderMacrosCount];
			for (uint32_t i = 0; i < shaderMacrosCount; i++)
			{
				NSString* key = [NSString stringWithUTF8String:shaderMacros[i].definition];

				// Try reading the macro value as a NSNumber. If failed, use it as an NSString.
				NSString* valueString = [NSString stringWithUTF8String:shaderMacros[i].value];
				NSNumber* valueNumber = [numberFormatter numberFromString:valueString];
				macroDictionary[key] = valueNumber ? valueNumber : valueString;
			}

			// Compile the code
			NSString* shaderSource = [[NSString alloc] initWithUTF8String:source];
			NSError*  error = nil;

			MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
			options.preprocessorMacros = macroDictionary;
			id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithSource:shaderSource options:options error:&error];

			// Warning
			if (error)
			{
				if (lib)
				{
					LOGF(
						LogLevel::eWARNING, "Loaded shader %s with the following warnings:\n %s", shaderName,
						[[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
								  //ASSERT(0);
				}
				// Error
				else
				{
					LOGF(
						LogLevel::eERROR, "Couldn't load shader %s with the following error:\n %s", shaderName,
						[[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
					ASSERT(0);
				}
			}

			if (lib)
			{
				pShaderProgram->mtlLibrary = lib;

				NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:entryPoint];
				id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
				assert(function != nil && "Entry point not found in shader.");
				
				if (pDesc->mConstantCount)
				{
					@autoreleasepool
					{
						if (@available(macOS 10.12, iOS 10.0, *))
						{
							util_specialize_function(lib, pDesc->pConstants, pDesc->mConstantCount, &function);
						}
					}
				}
				
				*compiled_code = function;
			}

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)source, (uint32_t)strlen(source), stage_mask,
				&stageReflections[reflectionCount++]);
		}
	}

	pShaderProgram->pEntryNames = (char**)tf_calloc(reflectionCount, sizeof(char*));
	memcpy(pShaderProgram->pEntryNames, entryNames, reflectionCount * sizeof(char*));
	createPipelineReflection(stageReflections, reflectionCount, pShaderProgram->pReflection);

#if defined(ENABLE_TESSELLATION)
	if (@available(macOS 10.12, iOS 10.0, *))
	{
		if (pShaderProgram->mtlVertexShader)
		{
			pShaderProgram->mTessellation = pShaderProgram->mtlVertexShader.patchType != MTLPatchTypeNone;
		}
	}
#endif
	
	addShaderDependencies(pShaderProgram, NULL);

	*ppShaderProgram = pShaderProgram;
}
#endif

void mtl_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);

	const char*                                     entryNames[SHADER_STAGE_COUNT] = {};

	uint32_t reflectionCount = 0;
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		ShaderStage                  stage_mask = (ShaderStage)(1 << i);
		const BinaryShaderStageDesc* pStage = NULL;
		__strong id<MTLFunction>* compiled_code = NULL;

		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					pStage = &pDesc->mVert;
					compiled_code = &(pShaderProgram->mtlVertexShader);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					pStage = &pDesc->mFrag;
					compiled_code = &(pShaderProgram->mtlFragmentShader);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					pStage = &pDesc->mComp;
					compiled_code = &(pShaderProgram->mtlComputeShader);
				}
				break;
				default: break;
			}

			// Create a MTLLibrary from bytecode.
			dispatch_data_t byteCode =
				dispatch_data_create(pStage->pByteCode, pStage->mByteCodeSize, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
			id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithData:byteCode error:nil];
			ASSERT(lib);
			pShaderProgram->mtlLibrary = lib;

			// Create a MTLFunction from the loaded MTLLibrary.
			NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:pStage->pEntryPoint];
			id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
			ASSERT(function);
			
			if (pDesc->mConstantCount)
			{
				@autoreleasepool
				{
					if (@available(macOS 10.12, iOS 10.0, *))
					{
						util_specialize_function(lib, pDesc->pConstants, pDesc->mConstantCount, &function);
					}
				}
			}
			
			*compiled_code = function;

			entryNames[reflectionCount] = pStage->pEntryPoint;

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)pStage->pSource, pStage->mSourceSize, stage_mask,
				&pShaderProgram->pReflection->mStageReflections[reflectionCount++]);
		}
	}

	pShaderProgram->pEntryNames = (char**)tf_calloc(reflectionCount, sizeof(char*));
	memcpy(pShaderProgram->pEntryNames, entryNames, reflectionCount * sizeof(char*));

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

	if (pShaderProgram->mStages & SHADER_STAGE_COMP)
	{
		pShaderProgram->mNumThreadsPerGroup[0] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[0];
		pShaderProgram->mNumThreadsPerGroup[1] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[1];
		pShaderProgram->mNumThreadsPerGroup[2] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[2];
	}

#if defined(ENABLE_TESSELLATION)
	if (@available(macOS 10.12, iOS 10.0, *))
	{
		if (pShaderProgram->mtlVertexShader)
		{
			pShaderProgram->mTessellation = pShaderProgram->mtlVertexShader.patchType != MTLPatchTypeNone;
		}
	}
#endif
	
	addShaderDependencies(pShaderProgram, pDesc);

	*ppShaderProgram = pShaderProgram;
}

void mtl_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	ASSERT(pShaderProgram);
	
	removeShaderDependencies(pShaderProgram);
	
	pShaderProgram->mtlVertexShader = nil;
	pShaderProgram->mtlFragmentShader = nil;
	pShaderProgram->mtlComputeShader = nil;
	pShaderProgram->mtlLibrary = nil;

	destroyPipelineReflection(pShaderProgram->pReflection);
	SAFE_FREE(pShaderProgram->pEntryNames);
	SAFE_FREE(pShaderProgram);
}

void mtl_addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppRootSignature);

	ShaderResource* shaderResources = NULL;

	// Collect static samplers
	typedef struct SamplerMap
	{
		char*		key;
		Sampler*	value;
	}SamplerMap;
	SamplerMap*				staticSamplerMap = NULL;
	DescriptorIndexMap*		indexMap = NULL;
	ShaderStage				shaderStages = SHADER_STAGE_NONE;
	PipelineType			pipelineType = PIPELINE_TYPE_UNDEFINED;
	sh_new_arena(indexMap);
	sh_new_arena(staticSamplerMap);

	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
	{
		shput(staticSamplerMap, pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
	}

	// Collect all unique shader resources in the given shaders
	// Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
	for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
	{
		PipelineReflection const* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

		// Keep track of the used pipeline stages
		shaderStages |= pReflection->mShaderStages;

		if (pReflection->mShaderStages & SHADER_STAGE_COMP)
		{
			pipelineType = PIPELINE_TYPE_COMPUTE;
		}
		else
		{
			pipelineType = PIPELINE_TYPE_GRAPHICS;
		}

		for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
		{
			ShaderResource const* pRes = &pReflection->pShaderResources[i];

			// All bindless resources will go in the static descriptor table
			if (pRes->type == DESCRIPTOR_TYPE_UNDEFINED)
			{
				continue;
			}

			// Find all unique resources
			DescriptorIndexMap* pNode = shgetp_null(indexMap, pRes->name);
			if (pNode == NULL)
			{
				ShaderResource* it = NULL;
				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					ShaderResource* a = &shaderResources[i];
					const ShaderResource* b = pRes;
					// HLSL - Every type has different register type unlike Vulkan where all registers are shared by all types
					if (a->mIsArgumentBufferField != b->mIsArgumentBufferField)
					{
						continue;
					}
					else if (!a->mIsArgumentBufferField && !b->mIsArgumentBufferField)
					{
						if ( (a->type == b->type) && (a->used_stages == b->used_stages) && (((a->reg ^ b->reg) | (a->set ^ b->set)) == 0))
						{
							it = a;
							break;
						}
					}
					else if ((a->type == b->type) && ((((uint64_t)a->mtlArgumentDescriptors.mArgumentIndex << 32) |
												   ((uint64_t)a->mtlArgumentDescriptors.mBufferIndex & 0xFFFFFFFF)) ==
												  (((uint64_t)b->mtlArgumentDescriptors.mArgumentIndex << 32) |
												   ((uint64_t)b->mtlArgumentDescriptors.mBufferIndex & 0xFFFFFFFF))))
					{
						it = a;
						break;
					}
				}
				
				if (it == NULL)
				{
					shput(indexMap, pRes->name, (uint32_t)arrlen(shaderResources));

					arrpush(shaderResources, *pRes);
				}
				else
				{
					ASSERT(pRes->type == it->type);
					if (pRes->type != it->type)
					{
						LOGF(
							LogLevel::eERROR,
							"\nFailed to create root signature\n"
							"Shared shader resources %s and %s have mismatching types (%u) and (%u). All shader resources "
							"sharing the same register and space addRootSignature "
							"must have the same type",
							pRes->name, it->name, (uint32_t)pRes->type, (uint32_t)it->type);
						ASSERT(false);
						return;
					}

					uint32_t val = shgetp_null(indexMap, it->name)->value;
					shput(indexMap, pRes->name, val);

					it->used_stages |= pRes->used_stages;
				}
			}
			// If the resource was already collected, just update the shader stage mask in case it is used in a different
			// shader stage in this case
			else
			{
				if (shaderResources[pNode->value].reg != pRes->reg)
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching register. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					ASSERT(false);
					return;
				}
				if (shaderResources[pNode->value].set != pRes->set)
				{
					LOGF(
						LogLevel::eERROR,
						"\nFailed to create root signature\n"
						"Shared shader resource %s has mismatching space. All shader resources "
						"shared by multiple shaders specified in addRootSignature "
						"have the same register and space",
						pRes->name);
					ASSERT(false);
					return;
				}

				for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
				{
					ShaderResource* pCur = &shaderResources[i];
					if (strcmp(pCur->name, pNode->key) == 0)
					{
						pCur->used_stages |= pRes->used_stages;
						break;
					}
				}
			}
		}
	}

	size_t totalSize = sizeof(RootSignature);
	totalSize += arrlenu(shaderResources) * sizeof(DescriptorInfo);
	RootSignature* pRootSignature = (RootSignature*)tf_calloc_memalign(1, alignof(RootSignature), totalSize);
	ASSERT(pRootSignature);

	pRootSignature->mPipelineType = pipelineType;
	pRootSignature->mDescriptorCount = (uint32_t)arrlen(shaderResources);
	pRootSignature->pDescriptors = (DescriptorInfo*)(pRootSignature + 1);
	pRootSignature->pDescriptorNameToIndexMap = indexMap;
	ASSERT(pRootSignature->pDescriptorNameToIndexMap);

	// Collect all shader resources in the given shaders
	{
		for (ptrdiff_t i = 0; i < arrlen(shaderResources); ++i)
		{
			ShaderResource const* pRes = &shaderResources[i];

			//
			DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];

			uint32_t                        setIndex = pRes->set;
			const DescriptorUpdateFrequency updateFreq((DescriptorUpdateFrequency)setIndex);

			shgetp_null(pRootSignature->pDescriptorNameToIndexMap, pRes->name)->value = (uint32_t)i;

			pDesc->mReg = pRes->reg;
			//                pDesc->mDesc.set = pRes->set;
			pDesc->mSize = pRes->size;
			//                pDesc->mDesc.alignment = pRes->alignment;
			pDesc->mType = pRes->type;
			pDesc->mUsedStages = pRes->used_stages;
			pDesc->mIsArgumentBufferField = pRes->mIsArgumentBufferField;
			pDesc->pName = pRes->name;
			pDesc->mUpdateFrequency = updateFreq;
			if (pDesc->mIsArgumentBufferField)
			{
				pDesc->mHandleIndex = pRes->mtlArgumentDescriptors.mArgumentIndex;
			}
			else if (pDesc->mType != DESCRIPTOR_TYPE_ROOT_CONSTANT)
			{
				pDesc->mRootDescriptor = 1;
				
				if (isDescriptorRootCbv(pDesc->pName))
				{
					continue;
				}
				
				switch (pDesc->mType)
				{
					case DESCRIPTOR_TYPE_SAMPLER: pDesc->mHandleIndex = pRootSignature->mRootSamplerCount++; break;
					case DESCRIPTOR_TYPE_TEXTURE:
					case DESCRIPTOR_TYPE_RW_TEXTURE:
						pDesc->mHandleIndex = pRootSignature->mRootTextureCount++;
						break;
						// Everything else is a buffer
					default: pDesc->mHandleIndex = pRootSignature->mRootBufferCount++; break;
				}
			}

			pDesc->mtlStaticSampler = nil;

#if defined(ENABLE_ARGUMENT_BUFFERS)
			if (@available(macOS 10.13, iOS 11.0, *))
			{
				if (MTLArgumentAccessWriteOnly == pRes->mtlArgumentDescriptors.mAccessType)
				{
					pDesc->mUsage = MTLResourceUsageWrite;
				}
				else if (MTLArgumentAccessReadWrite == pRes->mtlArgumentDescriptors.mAccessType)
				{
					pDesc->mUsage = MTLResourceUsageRead | MTLResourceUsageWrite;
				}
				else
				{
					pDesc->mUsage = MTLResourceUsageRead;
				}
			}
#endif

			// In case we're binding a texture, we need to specify the texture type so the bound resource type matches the one defined in the shader.
			if (pRes->type == DESCRIPTOR_TYPE_TEXTURE || pRes->type == DESCRIPTOR_TYPE_RW_TEXTURE)
			{
				//                    pDesc->mDesc.mtlTextureType = pRes->mtlTextureType;
			}

			// static samplers
			if (pRes->type == DESCRIPTOR_TYPE_SAMPLER)
			{
				SamplerMap* pNode = shgetp_null(staticSamplerMap, pRes->name);
				if (pNode != NULL)
				{
					pDesc->mtlStaticSampler = pNode->value->mtlSamplerState;
				}
			}
		}
	}

#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			pRootSignature->mArgumentDescriptors[i] = [[NSMutableArray alloc] init];
		}

		// Create argument buffer descriptors (update template)
		for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
		{
			const DescriptorInfo& descriptorInfo(pRootSignature->pDescriptors[i]);

			if (descriptorInfo.mIsArgumentBufferField)
			{
				const ArgumentDescriptor& memberDescriptor(shaderResources[i].mtlArgumentDescriptors);

				DescriptorUpdateFrequency updateFreq =
					(DescriptorUpdateFrequency)(memberDescriptor.mBufferIndex - DESCRIPTOR_UPDATE_FREQ_PADDING);

				if (updateFreq < (uint32_t)DESCRIPTOR_UPDATE_FREQ_COUNT)
				{
					MTLArgumentDescriptor* argDescriptor = [MTLArgumentDescriptor argumentDescriptor];
					argDescriptor.access = memberDescriptor.mAccessType;
					argDescriptor.arrayLength = memberDescriptor.mArrayLength;
					argDescriptor.constantBlockAlignment = memberDescriptor.mAlignment;
					argDescriptor.dataType = memberDescriptor.mDataType;
					argDescriptor.index = memberDescriptor.mArgumentIndex;
					argDescriptor.textureType = memberDescriptor.mTextureType;

					ASSERT(argDescriptor.dataType != MTLDataTypeNone);

					[pRootSignature->mArgumentDescriptors[updateFreq] addObject:argDescriptor];
				}
			}
		}

		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			[pRootSignature->mArgumentDescriptors[i] sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
				MTLArgumentDescriptor* first = a;
				MTLArgumentDescriptor* second = b;
				return (NSComparisonResult)(first.index > second.index);
			}];
		}
	}
#endif
	
	addRootSignatureDependencies(pRootSignature, pRootSignatureDesc);

	*ppRootSignature = pRootSignature;
	arrfree(shaderResources);
	shfree(staticSamplerMap);
}

void mtl_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRootSignature);
	
	removeRootSignatureDependencies(pRootSignature);

#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
		for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
		{
			pRootSignature->mArgumentDescriptors[i] = nil;
		}
	}
#endif

	shfree(pRootSignature->pDescriptorNameToIndexMap);
	SAFE_FREE(pRootSignature);
}

void addGraphicsPipelineImpl(Renderer* pRenderer, const char* pName, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(ppPipeline);

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;
	pPipeline->mTessellation = pDesc->pShaderProgram->mTessellation;
	
#if defined(ENABLE_TESSELLATION)
	if (@available(macOS 10.12, iOS 10.0, *))
	{
		pPipeline->mPatchControlPointCount = (uint32_t) pDesc->pShaderProgram->mtlVertexShader.patchControlPointCount;
	}
#endif

	// create metal pipeline descriptor
	MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	renderPipelineDesc.vertexFunction = pDesc->pShaderProgram->mtlVertexShader;
	renderPipelineDesc.fragmentFunction = pDesc->pShaderProgram->mtlFragmentShader;
	renderPipelineDesc.sampleCount = pDesc->mSampleCount;

#if defined(ENABLE_INDIRECT_COMMAND_BUFFERS)
	if (@available(macOS 10.14, iOS 12.0, *))
	{
		renderPipelineDesc.supportIndirectCommandBuffers = pDesc->mSupportIndirectCommandBuffer;
	}
#endif

	// add vertex layout to descriptor
	if (pDesc->pVertexLayout != nil)
	{
		// setup vertex descriptors
		for (uint32_t i = 0; i < pDesc->pVertexLayout->mAttribCount; ++i)
		{
			const VertexAttrib* attrib = pDesc->pVertexLayout->mAttribs + i;
			// #NOTE: Buffer index starts at 30 and decrements based on binding
			// Example: If attrib->mBinding is 3, bufferIndex will be 27
			const uint32_t bufferIndex = VERTEX_BINDING_OFFSET - attrib->mBinding;

			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].offset = attrib->mOffset;
			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].bufferIndex = bufferIndex;
			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].format = util_to_mtl_vertex_format(attrib->mFormat);

			//setup layout for all bindings instead of just 0.
			if (!pDesc->pVertexLayout->mStrides[attrib->mBinding])
			{
				const uint32_t attribExtent = TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8 + attrib->mOffset;
				const uint32_t stride = (uint32_t)max( renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride, (NSUInteger)attribExtent );
				renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride = stride;
			}
			else
			{
				renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride = pDesc->pVertexLayout->mStrides[attrib->mBinding];
			}
			renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepRate = 1;

#if defined(ENABLE_TESSELLATION)
			if (pDesc->pShaderProgram->mTessellation)
			{
				if (@available(macOS 10.12, iOS 10.0, *))
				{
					renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerPatchControlPoint;
				}
			}
			else
#endif
				if (attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
			{
				renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerInstance;
			}
			else
			{
				renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerVertex;
			}
		}
	}

	//available on ios 12.0 and required for render_target_array_index semantics
	if (@available(*, iOS 12.0))
	{
		// add pipeline settings to descriptor
		switch (pDesc->mPrimitiveTopo)
		{
			case PRIMITIVE_TOPO_POINT_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint; break;
			case PRIMITIVE_TOPO_LINE_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine; break;
			case PRIMITIVE_TOPO_LINE_STRIP: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine; break;
			case PRIMITIVE_TOPO_TRI_LIST: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
			case PRIMITIVE_TOPO_TRI_STRIP: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
			default: renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle; break;
		}
	}

	// assign blend state
	if (pDesc->pBlendState)
		util_to_blend_desc(pDesc->pBlendState, renderPipelineDesc.colorAttachments, pDesc->mRenderTargetCount);

	// assign render target pixel format for all attachments
	for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
	{
		renderPipelineDesc.colorAttachments[i].pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->pColorFormats[i]);
	}

	// assign rasterizer state
	const RasterizerStateDesc* pRasterizer = pDesc->pRasterizerState ? pDesc->pRasterizerState : &gDefaultRasterizerState;
	pPipeline->mCullMode = (uint32_t)gMtlCullModeTranslator[pRasterizer->mCullMode];
	pPipeline->mFillMode = (uint32_t)gMtlFillModeTranslator[pRasterizer->mFillMode];
	pPipeline->mDepthBias = pRasterizer->mDepthBias;
	pPipeline->mSlopeScale = pRasterizer->mSlopeScaledDepthBias;
	pPipeline->mDepthClipMode = pRasterizer->mDepthClampEnable ? (uint32_t)MTLDepthClipModeClamp : (uint32_t)MTLDepthClipModeClip;
	// #TODO: Add if Metal API provides ability to set these
	//	rasterizerState.scissorEnable = pDesc->mScissor;
	//	rasterizerState.multisampleEnable = pDesc->mMultiSample;
	pPipeline->mWinding = (pRasterizer->mFrontFace == FRONT_FACE_CCW ? MTLWindingCounterClockwise : MTLWindingClockwise);

	// assign pixel format form depth attachment
	if (pDesc->mDepthStencilFormat != TinyImageFormat_UNDEFINED)
	{
		// assign depth state
		pPipeline->mtlDepthStencilState = pDesc->pDepthState ? util_to_depth_state(pRenderer, pDesc->pDepthState) : pDefaultDepthState;
		
		renderPipelineDesc.depthAttachmentPixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mDepthStencilFormat);
		if (pDesc->mDepthStencilFormat == TinyImageFormat_D24_UNORM_S8_UINT ||
			pDesc->mDepthStencilFormat == TinyImageFormat_D32_SFLOAT_S8_UINT)
			renderPipelineDesc.stencilAttachmentPixelFormat = renderPipelineDesc.depthAttachmentPixelFormat;
	}

	// assign common tesselation configuration if needed.
#if defined(ENABLE_TESSELLATION)
	if (@available(macOS 10.12, iOS 10.0, *))
	{
		if (pDesc->pShaderProgram->mtlVertexShader.patchType != MTLPatchTypeNone)
		{
			renderPipelineDesc.tessellationFactorScaleEnabled = NO;
			renderPipelineDesc.tessellationFactorFormat = MTLTessellationFactorFormatHalf;
			renderPipelineDesc.tessellationControlPointIndexType = MTLTessellationControlPointIndexTypeNone;
			renderPipelineDesc.tessellationFactorStepFunction = MTLTessellationFactorStepFunctionConstant;
			renderPipelineDesc.tessellationOutputWindingOrder = MTLWindingClockwise;
			renderPipelineDesc.tessellationPartitionMode = MTLTessellationPartitionModeFractionalEven;
#if TARGET_OS_IOS
			// In iOS, the maximum tessellation factor is 16
			renderPipelineDesc.maxTessellationFactor = 16;
#elif TARGET_OS_OSX
			// In OS X, the maximum tessellation factor is 64
			renderPipelineDesc.maxTessellationFactor = 64;
#endif
		}
	}
#endif
	
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pName)
	{
		renderPipelineDesc.label = [NSString stringWithUTF8String:pName];
	}
#endif

	// create pipeline from descriptor
	NSError* error = nil;
	pPipeline->mtlRenderPipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
																						 options:MTLPipelineOptionNone
																					  reflection:nil
																						   error:&error];
	if (!pPipeline->mtlRenderPipelineState)
	{
		LOGF(LogLevel::eERROR, "Failed to create render pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		ASSERT(false);
		return;
	}

	switch (pDesc->mPrimitiveTopo)
	{
		case PRIMITIVE_TOPO_POINT_LIST: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypePoint; break;
		case PRIMITIVE_TOPO_LINE_LIST: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypeLine; break;
		case PRIMITIVE_TOPO_LINE_STRIP: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypeLineStrip; break;
		case PRIMITIVE_TOPO_TRI_LIST: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangle; break;
		case PRIMITIVE_TOPO_TRI_STRIP: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangleStrip; break;
		default: pPipeline->mMtlPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangle; break;
	}

	*ppPipeline = pPipeline;
}

void addComputePipelineImpl(Renderer* pRenderer, const char* pName, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(ppPipeline);

	Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mType = PIPELINE_TYPE_COMPUTE;
	const uint32_t* numThreadsPerGroup = pDesc->pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup;
	pPipeline->mNumThreadsPerGroup = MTLSizeMake(numThreadsPerGroup[0], numThreadsPerGroup[1], numThreadsPerGroup[2]);
	
	MTLComputePipelineDescriptor* pipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
	pipelineDesc.computeFunction = pDesc->pShaderProgram->mtlComputeShader;
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pName)
	{
		pipelineDesc.label = [NSString stringWithUTF8String:pName];
	}
#endif
	
	NSError* error = nil;
	pPipeline->mtlComputePipelineState = [pRenderer->pDevice newComputePipelineStateWithDescriptor:pipelineDesc options:MTLPipelineOptionNone reflection:NULL error:&error];
	if (!pPipeline->mtlComputePipelineState)
	{
		LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		SAFE_FREE(pPipeline);
		return;
	}

	*ppPipeline = pPipeline;
}

void mtl_addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);

	switch (pDesc->mType)
	{
		case (PIPELINE_TYPE_COMPUTE):
		{
			addComputePipelineImpl(pRenderer, pDesc->pName, &pDesc->mComputeDesc, ppPipeline);
			break;
		}
		case (PIPELINE_TYPE_GRAPHICS):
		{
			addGraphicsPipelineImpl(pRenderer, pDesc->pName, &pDesc->mGraphicsDesc, ppPipeline);
			break;
		}
#if defined(MTL_RAYTRACING_AVAILABLE)
		case (PIPELINE_TYPE_RAYTRACING):
		{
			addRaytracingPipeline(&pDesc->mRaytracingDesc, ppPipeline);
			break;
		}
#endif
		default:
			ASSERT(false);    // unknown pipeline type
			break;
	}
	
	addPipelineDependencies(*ppPipeline, pDesc);
}

void mtl_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
	ASSERT(pPipeline);
	
	removePipelineDependencies(pPipeline);
	
	pPipeline->mtlRenderPipelineState = nil;
	pPipeline->mtlComputePipelineState = nil;
	pPipeline->mtlDepthStencilState = nil;

#if defined(MTL_RAYTRACING_AVAILABLE)
	if (pPipeline->mType == PIPELINE_TYPE_RAYTRACING)
	{
		removeRaytracingPipeline(pPipeline->pRaytracingPipeline);
	}
#endif

	SAFE_FREE(pPipeline);
}

void mtl_addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**) {}

void mtl_removePipelineCache(Renderer*, PipelineCache*) {}

void mtl_getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*) {}

void mtl_addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer != nil);
	ASSERT(pDesc != nil);
	ASSERT(pDesc->mIndirectArgCount == 1);
	
	CommandSignature* pCommandSignature =
		(CommandSignature*)tf_calloc(1, sizeof(CommandSignature) + sizeof(IndirectArgument) * pDesc->mIndirectArgCount);
	ASSERT(pCommandSignature);
	
	pCommandSignature->mDrawType = pDesc->pArgDescs[0].mType;
	switch (pDesc->pArgDescs[0].mType)
	{
		case INDIRECT_DRAW:
			pCommandSignature->mStride += sizeof(IndirectDrawArguments);
			break;
		case INDIRECT_DRAW_INDEX:
			pCommandSignature->mStride += sizeof(IndirectDrawIndexArguments);
			break;
		case INDIRECT_DISPATCH:
			pCommandSignature->mStride += sizeof(IndirectDispatchArguments);
			break;
		case INDIRECT_COMMAND_BUFFER:
		case INDIRECT_COMMAND_BUFFER_RESET:
		case INDIRECT_COMMAND_BUFFER_OPTIMIZE: break;
		default:
			ASSERT(false);
			break;
	}

	if (!pDesc->mPacked)
	{
		pCommandSignature->mStride = round_up(pCommandSignature->mStride, 16);
	}

	*ppCommandSignature = pCommandSignature;
}

void mtl_removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	ASSERT(pCommandSignature);
	SAFE_FREE(pCommandSignature);
}
// -------------------------------------------------------------------------------------------------
// Buffer functions
// -------------------------------------------------------------------------------------------------
void mtl_mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ASSERT(pBuffer->mtlBuffer.storageMode != MTLStorageModePrivate && "Trying to map non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = (uint8_t*)pBuffer->mtlBuffer.contents + pBuffer->mOffset;
}
void mtl_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer->mtlBuffer.storageMode != MTLStorageModePrivate && "Trying to unmap non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = nil;
}

// -------------------------------------------------------------------------------------------------
// Command buffer functions
// -------------------------------------------------------------------------------------------------

void mtl_resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	UNREF_PARAM(pRenderer);
	UNREF_PARAM(pCmdPool);
}

void mtl_beginCmd(Cmd* pCmd)
{
	@autoreleasepool
	{
		ASSERT(pCmd);
		pCmd->mtlRenderEncoder = nil;
		pCmd->mtlComputeEncoder = nil;
		pCmd->mtlBlitEncoder = nil;
		pCmd->pBoundPipeline = NULL;
		pCmd->mBoundIndexBuffer = nil;
		pCmd->pLastFrameQuery = nil;
		pCmd->mtlCommandBuffer = [pCmd->pQueue->mtlCommandQueue commandBuffer];
	}

	pCmd->mPipelineType = PIPELINE_TYPE_UNDEFINED;
}

void mtl_endCmd(Cmd* pCmd)
{
	@autoreleasepool
	{
		util_end_current_encoders(pCmd, true);
	}
}

void mtl_cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* pLoadActions,
	uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
{
	ASSERT(pCmd);

	if (!renderTargetCount && !pDepthStencil)
	{
		return;
	}

	@autoreleasepool
	{
		MTLRenderPassDescriptor* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];

		// Flush color attachments
		for (uint32_t i = 0; i < renderTargetCount; i++)
		{
			Texture* colorAttachment = ppRenderTargets[i]->pTexture;

			renderPassDesc.colorAttachments[i].texture = colorAttachment->mtlTexture;
			renderPassDesc.colorAttachments[i].level = pColorMipSlices ? pColorMipSlices[i] : 0;
			if (pColorArraySlices)
			{
				if (ppRenderTargets[i]->mDepth > 1)
				{
					renderPassDesc.colorAttachments[i].depthPlane = pColorArraySlices[i];
				}
				else
				{
					renderPassDesc.colorAttachments[i].slice = pColorArraySlices[i];
				}
			}
			else if (ppRenderTargets[i]->mArraySize > 1)
			{
				if (@available(macOS 10.11, iOS 12.0, *))
				{
					renderPassDesc.renderTargetArrayLength = ppRenderTargets[i]->mArraySize;
				}
				else
				{
					ASSERT(false);
				}
			}
			else if (ppRenderTargets[i]->mDepth > 1)
			{
				if (@available(macOS 10.11, iOS 12.0, *))
				{
					renderPassDesc.renderTargetArrayLength = ppRenderTargets[i]->mDepth;
				}
				else
				{
					ASSERT(false);
				}
			}
			
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			bool resolveAttachment = pLoadActions && (STORE_ACTION_RESOLVE_STORE == pLoadActions->mStoreActionsColor[i] || STORE_ACTION_RESOLVE_DONTCARE == pLoadActions->mStoreActionsColor[i]);
			
			if(resolveAttachment)
			{
				ASSERT(ppRenderTargets[i]->pResolveAttachment);
				
				id<MTLTexture> resolveAttachment = ppRenderTargets[i]->pResolveAttachment->pTexture->mtlTexture;
				renderPassDesc.colorAttachments[i].resolveTexture = resolveAttachment;
				renderPassDesc.colorAttachments[i].resolveLevel = pColorMipSlices ? pColorMipSlices[i] : 0;
				if (pColorArraySlices)
				{
					if (ppRenderTargets[i]->mDepth > 1)
					{
						renderPassDesc.colorAttachments[i].resolveDepthPlane = pColorArraySlices[i];
					}
					else
					{
						renderPassDesc.colorAttachments[i].resolveSlice = pColorArraySlices[i];
					}
				}
			}
#endif

			renderPassDesc.colorAttachments[i].loadAction =
				(pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionsColor[i]) : MTLLoadActionDontCare);
			
			// For on-tile (memoryless) textures, we never need to store.
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
			if (resolveAttachment)
			{
				renderPassDesc.colorAttachments[i].storeAction = colorAttachment->mLazilyAllocated ? MTLStoreActionMultisampleResolve :
					util_to_mtl_store_action(pLoadActions->mStoreActionsColor[i]);
			}
			else
#endif
			{
				renderPassDesc.colorAttachments[i].storeAction = colorAttachment->mLazilyAllocated ? MTLStoreActionDontCare :
					(pLoadActions ? util_to_mtl_store_action(pLoadActions->mStoreActionsColor[i]) : MTLStoreActionStore);
			}

			if (pLoadActions != NULL)
			{
				const ClearValue& clearValue = pLoadActions->mClearColorValues[i];
				renderPassDesc.colorAttachments[i].clearColor =
					MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);
			}
		}

		if (pDepthStencil)
		{
			renderPassDesc.depthAttachment.texture = pDepthStencil->pTexture->mtlTexture;
			renderPassDesc.depthAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
			renderPassDesc.depthAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
#ifndef TARGET_IOS
			bool isStencilEnabled = pDepthStencil->pTexture->mtlPixelFormat == MTLPixelFormatDepth24Unorm_Stencil8;
#else
			bool isStencilEnabled = false;
#endif
			isStencilEnabled = isStencilEnabled || pDepthStencil->pTexture->mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8;
			if (isStencilEnabled)
			{
				renderPassDesc.stencilAttachment.texture = pDepthStencil->pTexture->mtlTexture;
				renderPassDesc.stencilAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
				renderPassDesc.stencilAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
			}

			renderPassDesc.depthAttachment.loadAction =
				(pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionDepth) : MTLLoadActionDontCare);
			// For on-tile (memoryless) textures, we never need to store.
			renderPassDesc.depthAttachment.storeAction = pDepthStencil->pTexture->mLazilyAllocated ? MTLStoreActionDontCare :
				(pLoadActions ? util_to_mtl_store_action(pLoadActions->mStoreActionDepth) : MTLStoreActionStore);

			if (isStencilEnabled)
			{
				renderPassDesc.stencilAttachment.loadAction =
					pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionStencil) : MTLLoadActionDontCare;
				renderPassDesc.stencilAttachment.storeAction = pDepthStencil->pTexture->mLazilyAllocated ? MTLStoreActionDontCare :
					(pLoadActions ? util_to_mtl_store_action(pLoadActions->mStoreActionStencil) : MTLStoreActionStore);
			}
			else
			{
				renderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
				renderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
			}

			if (pLoadActions)
			{
				renderPassDesc.depthAttachment.clearDepth = pLoadActions->mClearDepth.depth;
				if (isStencilEnabled)
					renderPassDesc.stencilAttachment.clearStencil = pLoadActions->mClearDepth.stencil;
			}
		}
		else
		{
			renderPassDesc.depthAttachment.loadAction = MTLLoadActionDontCare;
			renderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
			renderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
			renderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
		}

		util_end_current_encoders(pCmd, false);
		pCmd->mtlRenderEncoder = [pCmd->mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
		util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);    // apply the graphics barriers before flushing them

		util_set_heaps_graphics(pCmd);
	}
}

void mtl_cmdSetShadingRate(
	Cmd* pCmd, ShadingRate shadingRate, Texture* pTexture, ShadingRateCombiner postRasterizerRate, ShadingRateCombiner finalRate)
{
}

void mtl_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(eERROR, "Using cmdSetViewport out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetViewport");
		return;
	}

	MTLViewport viewport;
	viewport.originX = x;
	viewport.originY = y;
	viewport.width = width;
	viewport.height = height;
	viewport.znear = minDepth;
	viewport.zfar = maxDepth;

	[pCmd->mtlRenderEncoder setViewport:viewport];
}

void mtl_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(eERROR, "Using cmdSetScissor out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetScissor");
		return;
	}

	MTLScissorRect scissor;
	scissor.x = x;
	scissor.y = y;
	scissor.width = width;
	scissor.height = height;
	if(height+y==0 || width+x==0 )
	{
		scissor.width = max(1u, width);
		scissor.height = max(1u, height);
	}

	[pCmd->mtlRenderEncoder setScissorRect:scissor];
}

void mtl_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
	ASSERT(pCmd);

	[pCmd->mtlRenderEncoder setStencilReferenceValue:val];
}

void mtl_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	pCmd->pBoundPipeline = pPipeline;

	bool barrierRequired = pCmd->mPipelineType != pPipeline->mType;
	pCmd->mPipelineType = pPipeline->mType;
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
	pCmd->mFirstVertex = 0;
	for (int i = 0; i < MAX_VERTEX_BINDINGS; ++i)
	{
		pCmd->mStrides[i] = 0;
	}
#endif

	@autoreleasepool
	{
		if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
		{
			[pCmd->mtlRenderEncoder setRenderPipelineState:pPipeline->mtlRenderPipelineState];

			[pCmd->mtlRenderEncoder setCullMode:(MTLCullMode)pPipeline->mCullMode];
			[pCmd->mtlRenderEncoder setTriangleFillMode:(MTLTriangleFillMode)pPipeline->mFillMode];
			[pCmd->mtlRenderEncoder setFrontFacingWinding:(MTLWinding)pPipeline->mWinding];
			[pCmd->mtlRenderEncoder setDepthBias:pPipeline->mDepthBias slopeScale:pPipeline->mSlopeScale clamp:0.0f];
#ifdef ENABLE_DEPTH_CLIP_MODE
			if (@available(macOS 10.11, iOS 11.0, *))
			{
				[pCmd->mtlRenderEncoder setDepthClipMode:(MTLDepthClipMode)pPipeline->mDepthClipMode];
			}
#endif

			if (pPipeline->mtlDepthStencilState)
			{
				[pCmd->mtlRenderEncoder setDepthStencilState:pPipeline->mtlDepthStencilState];
			}

			pCmd->mSelectedPrimitiveType = (uint32_t)pPipeline->mMtlPrimitiveType;
		}
		else if (pPipeline->mType == PIPELINE_TYPE_COMPUTE)
		{
			if (!pCmd->mtlComputeEncoder)
			{
				util_end_current_encoders(pCmd, barrierRequired);
				pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];

				util_set_heaps_compute(pCmd);
			}
			[pCmd->mtlComputeEncoder setComputePipelineState:pPipeline->mtlComputePipelineState];
		}
		else if (pPipeline->mType == PIPELINE_TYPE_RAYTRACING)
		{
			if (!pCmd->mtlComputeEncoder)
			{
				util_end_current_encoders(pCmd, barrierRequired);
				pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];

				util_set_heaps_compute(pCmd);
			}
		}
		else
		{
			ASSERT(false);    // unknown pipeline type
		}
	}
}

void mtl_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);

	pCmd->mBoundIndexBuffer = pBuffer->mtlBuffer;
	pCmd->mBoundIndexBufferOffset = (uint32_t)(offset + pBuffer->mOffset);
	pCmd->mIndexType = (INDEX_TYPE_UINT16 == indexType ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
	pCmd->mIndexStride = (INDEX_TYPE_UINT16 == indexType ? sizeof(uint16_t) : sizeof(uint32_t));
}

void mtl_cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);

	// When using a poss-tessellation vertex shader, the first vertex buffer bound is used as the tessellation factors buffer.
	uint32_t startIdx = 0;
#if defined(ENABLE_TESSELLATION)
	if (@available(macOS 10.12, iOS 10.0, *))
	{
		if (pCmd->pBoundPipeline && pCmd->pBoundPipeline->mTessellation)
		{
			startIdx = 1;
			[pCmd->mtlRenderEncoder setTessellationFactorBuffer:ppBuffers[0]->mtlBuffer
														 offset:ppBuffers[0]->mOffset + (pOffsets ? pOffsets[0] : 0)instanceStride:0];
		}
	}
#endif

	for (uint32_t i = 0; i < bufferCount - startIdx; i++)
	{
		uint32_t index = startIdx + i;
		uint32_t offset = (uint32_t)(ppBuffers[index]->mOffset + (pOffsets ? pOffsets[index] : 0));
		[pCmd->mtlRenderEncoder
			setVertexBuffer:ppBuffers[index]->mtlBuffer
					 offset:offset atIndex:(VERTEX_BINDING_OFFSET - i)];
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
		pCmd->mOffsets[i] = offset;
		pCmd->mStrides[i] = pStrides[index];
#endif
	}
}

void mtl_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);
	
	if (!pCmd->pBoundPipeline->mTessellation)
	{
		[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
								   vertexStart:firstVertex
								   vertexCount:vertexCount];
	}
#if defined(ENABLE_TESSELLATION)
	else if (@available(macOS 10.12, iOS 10.0, *))    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
								 patchStart:firstVertex
								 patchCount:vertexCount
						   patchIndexBuffer:nil
					 patchIndexBufferOffset:0
							  instanceCount:1
							   baseInstance:0];
	}
#endif
}

void mtl_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
	ASSERT(pCmd);

	if (!pCmd->pBoundPipeline->mTessellation)
	{
		if (firstInstance == 0)
		{
			[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount];
		}
		else
		{
			[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount
									  baseInstance:firstInstance];
		}
	}
#if defined(ENABLE_TESSELLATION)
	else if (@available(macOS 10.12, iOS 10.0, *))    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
								 patchStart:firstVertex
								 patchCount:vertexCount
						   patchIndexBuffer:nil
					 patchIndexBufferOffset:0
							  instanceCount:instanceCount
							   baseInstance:firstInstance];
	}
#endif
}

void mtl_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
	ASSERT(pCmd);

	id           indexBuffer = pCmd->mBoundIndexBuffer;
	MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
	uint64_t     offset = pCmd->mBoundIndexBufferOffset + (firstIndex * pCmd->mIndexStride);

	if (!pCmd->pBoundPipeline->mTessellation)
	{
#ifdef TARGET_IOS
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
		if (firstVertex != pCmd->mFirstVertex)
		{
			pCmd->mFirstVertex = firstVertex;
			for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
			{
				uint32_t offset = pCmd->mOffsets[i];
				uint32_t stride = pCmd->mStrides[i];
				
				if (stride != 0)
				{
					[pCmd->mtlRenderEncoder
						setVertexBufferOffset: offset + stride * firstVertex atIndex: (VERTEX_BINDING_OFFSET - i)];
				}
			}
		}
		
		[pCmd->mtlRenderEncoder
			drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
										indexCount:indexCount
										indexType:indexType
										indexBuffer:indexBuffer
										indexBufferOffset:offset];
#else
		if (!firstVertex)
		{
			[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
											   indexCount:indexCount
												indexType:indexType
											  indexBuffer:indexBuffer
										indexBufferOffset:offset];
		}
		else
		{
			//only ios devices supporting gpu family 3_v1 and above can use baseVertex and baseInstance
			//if lower than 3_v1 render without base info but artifacts will occur if used.
			if ([pCmd->pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
			{
				[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
												   indexCount:indexCount
													indexType:indexType
												  indexBuffer:indexBuffer
											indexBufferOffset:offset
												instanceCount:1
												   baseVertex:firstVertex
												 baseInstance:0];
			}
			else
			{
				LOGF(LogLevel::eERROR, "Current device does not support firstVertex and firstInstance (3_v1 feature set)");
				return;
			}
		}
#endif
#else
		[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
										   indexCount:indexCount
											indexType:indexType
										  indexBuffer:indexBuffer
									indexBufferOffset:offset
										instanceCount:1
										   baseVertex:firstVertex
										 baseInstance:0];
#endif
	}
#if defined(ENABLE_TESSELLATION)
	else if (@available(macOS 10.12, iOS 10.0, *))    // Tessellated draw version.
	{
		//to suppress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pBoundPipeline->mPatchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer
							patchIndexBufferOffset:0
						   controlPointIndexBuffer:indexBuf
					 controlPointIndexBufferOffset:0
									 instanceCount:1
									  baseInstance:0];
	}
#endif
}

void mtl_cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	id           indexBuffer = pCmd->mBoundIndexBuffer;
	MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
	uint64_t     offset = pCmd->mBoundIndexBufferOffset + (firstIndex * pCmd->mIndexStride);

	if (!pCmd->pBoundPipeline->mTessellation)
	{
#ifdef TARGET_IOS
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
		if (firstInstance)
		{
			LOGF(LogLevel::eERROR, "Current device does not support firstInstance (3_v1 feature set)");
			ASSERT(false);
			return;
		}
		else
		{
			if (firstVertex != pCmd->mFirstVertex)
			{
				pCmd->mFirstVertex = firstVertex;
				for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
				{
					uint32_t offset = pCmd->mOffsets[i];
					uint32_t stride = pCmd->mStrides[i];
					
					if (stride != 0)
					{
						[pCmd->mtlRenderEncoder
							setVertexBufferOffset: offset + stride * firstVertex atIndex: (VERTEX_BINDING_OFFSET - i)];
					}
				}
			}
			
			[pCmd->mtlRenderEncoder
				drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
											indexCount:indexCount
											indexType:indexType
											indexBuffer:indexBuffer
											indexBufferOffset:offset
											instanceCount:instanceCount];
		}
#else
		if (firstInstance || firstVertex)
		{
			[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
												   indexCount:indexCount
													indexType:indexType
												  indexBuffer:indexBuffer
											indexBufferOffset:offset
												instanceCount:instanceCount
												   baseVertex:firstVertex
												 baseInstance:firstInstance];
		}
		else
		{
			[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
											   indexCount:indexCount
												indexType:indexType
											  indexBuffer:indexBuffer
										indexBufferOffset:offset
											instanceCount:instanceCount];
		}
#endif
		
#else
		[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
										   indexCount:indexCount
											indexType:indexType
										  indexBuffer:indexBuffer
									indexBufferOffset:offset
										instanceCount:instanceCount
										   baseVertex:firstVertex
										 baseInstance:firstInstance];
#endif
	}
#if defined(ENABLE_TESSELLATION)
	else if (@available(macOS 10.12, iOS 10.0, *))    // Tessellated draw version.
	{
		//to supress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pBoundPipeline->mPatchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer
							patchIndexBufferOffset:0
						   controlPointIndexBuffer:indexBuf
					 controlPointIndexBufferOffset:0
									 instanceCount:instanceCount
									  baseInstance:firstInstance];
	}
#endif
}

void mtl_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	ASSERT(pCmd);
	ASSERT(pCmd->mtlComputeEncoder != nil);

	// There might have been a barrier inserted since last dispatch call
	// This only applies to dispatch since you can issue barriers inside compute pass but this is not possible inside render pass
	// For render pass, barriers are issued in cmdBindRenderTargets after beginning new encoder
	util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);

	MTLSize threadgroupCount = MTLSizeMake(groupCountX, groupCountY, groupCountZ);

	[pCmd->mtlComputeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:pCmd->pBoundPipeline->mNumThreadsPerGroup];
}

void mtl_cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCmd);

	const IndirectArgumentType drawType = pCommandSignature->mDrawType;

#if defined(ENABLE_INDIRECT_COMMAND_BUFFERS)
	if (pCmd->pRenderer->pActiveGpuSettings->mIndirectCommandBuffer)
	{
		if (@available(macOS 10.14, iOS 12.0, *))
		{
			if (drawType == INDIRECT_COMMAND_BUFFER_OPTIMIZE && maxCommandCount)
			{
				if (!pCmd->mtlBlitEncoder)
				{
					util_end_current_encoders(pCmd, false);
					pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
					util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
				}
				[pCmd->mtlBlitEncoder optimizeIndirectCommandBuffer:pIndirectBuffer->mtlIndirectCommandBuffer
														  withRange:NSMakeRange(0, maxCommandCount)];
				return;
			}
			else if (drawType == INDIRECT_COMMAND_BUFFER_RESET && maxCommandCount)
			{
				if (!pCmd->mtlBlitEncoder)
				{
					util_end_current_encoders(pCmd, false);
					pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
					util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
				}
				[pCmd->mtlBlitEncoder resetCommandsInBuffer:pIndirectBuffer->mtlIndirectCommandBuffer
												  withRange:NSMakeRange(0, maxCommandCount)];
				return;
			}
			else if ((pIndirectBuffer->mtlIndirectCommandBuffer || drawType == INDIRECT_COMMAND_BUFFER) && maxCommandCount)
			{
				[pCmd->mtlRenderEncoder executeCommandsInBuffer:pIndirectBuffer->mtlIndirectCommandBuffer
													  withRange:NSMakeRange(0, maxCommandCount)];
				return;
			}
		}
	}
#endif

	if (drawType == INDIRECT_DRAW)
	{
		if (!pCmd->pBoundPipeline->mTessellation)
		{
			for (uint32_t i = 0; i < maxCommandCount; i++)
			{
				uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
				[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
										indirectBuffer:pIndirectBuffer->mtlBuffer
								  indirectBufferOffset:indirectBufferOffset];
			}
		}
#if defined(ENABLE_TESSELLATION)
		else if (@available(macOS 10.12, iOS 12.0, *))    // Tessellated indirect draw version.
		{
			if (pCmd->pRenderer->pActiveGpuSettings->mTessellationIndirectDrawSupported)
			{
				for (uint32_t i = 0; i < maxCommandCount; i++)
				{
					uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
					[pCmd->mtlRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
									   patchIndexBuffer:nil
								 patchIndexBufferOffset:0
										 indirectBuffer:pIndirectBuffer->mtlBuffer
								   indirectBufferOffset:indirectBufferOffset];
				}
			}
			else
			{
				LOGF(eWARNING, "GPU does not support drawPatches with indirect buffer. Skipping draw");
			}
		}
#endif
	}
	else if (drawType == INDIRECT_DRAW_INDEX)
	{
		if (!pCmd->pBoundPipeline->mTessellation)
		{
			for (uint32_t i = 0; i < maxCommandCount; ++i)
			{
				id           indexBuffer = pCmd->mBoundIndexBuffer;
				MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
				uint64_t     indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;

				[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
													indexType:indexType
												  indexBuffer:indexBuffer
											indexBufferOffset:0
											   indirectBuffer:pIndirectBuffer->mtlBuffer
										 indirectBufferOffset:indirectBufferOffset];
			}
		}
#if defined(ENABLE_TESSELLATION)
		else if (@available(macOS 10.12, iOS 12.0, *))    // Tessellated indirect draw version.
		{
			if (pCmd->pRenderer->pActiveGpuSettings->mTessellationIndirectDrawSupported)
			{
				for (uint32_t i = 0; i < maxCommandCount; ++i)
				{
					id       indexBuffer = pCmd->mBoundIndexBuffer;
					uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
					[pCmd->mtlRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
									   patchIndexBuffer:indexBuffer
								 patchIndexBufferOffset:0
										 indirectBuffer:pIndirectBuffer->mtlBuffer
								   indirectBufferOffset:indirectBufferOffset];
				}
			}
			else
			{
				LOGF(eWARNING, "GPU does not support drawPatches with indirect buffer. Skipping draw");
			}
		}
#endif
	}
	else if (drawType == INDIRECT_DISPATCH)
	{
		// There might have been a barrier inserted since last dispatch call
		// This only applies to dispatch since you can issue barriers inside compute pass but this is not possible inside render pass
		// For render pass, barriers are issued in cmdBindRenderTargets after beginning new encoder
		util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);
		
		for (uint32_t i = 0; i < maxCommandCount; ++i)
		{
			[pCmd->mtlComputeEncoder dispatchThreadgroupsWithIndirectBuffer:pIndirectBuffer->mtlBuffer
													   indirectBufferOffset:bufferOffset + pCommandSignature->mStride * i
													  threadsPerThreadgroup:pCmd->pBoundPipeline->mNumThreadsPerGroup];
		}
	}
}

void mtl_cmdResourceBarrier(
	Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
	uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
	if (numBufferBarriers)
	{
		pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_BUFFERS;
	}

	if (numTextureBarriers)
	{
		pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_TEXTURES;
	}

	if (numRtBarriers)
	{
		pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_RENDERTARGETS;
		pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_TEXTURES;
	}
}

void mtl_cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->mtlBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mtlBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->mSize);
	ASSERT(dstOffset + size <= pBuffer->mSize);

	if (!pCmd->mtlBlitEncoder)
	{
		util_end_current_encoders(pCmd, false);
		pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
	}

	[pCmd->mtlBlitEncoder copyFromBuffer:pSrcBuffer->mtlBuffer
							sourceOffset:srcOffset + pSrcBuffer->mOffset
								toBuffer:pBuffer->mtlBuffer
					   destinationOffset:dstOffset + pBuffer->mOffset
									size:size];
}

typedef struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
} SubresourceDataDesc;

void mtl_cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pIntermediate, const SubresourceDataDesc* pSubresourceDesc)
{
	MTLSize sourceSize = MTLSizeMake(
		max(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel), max(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel),
		max(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel));

#ifdef TARGET_IOS
	uint64_t formatNamespace =
		(TinyImageFormat_Code((TinyImageFormat)pTexture->mFormat) & ((1 << TinyImageFormat_NAMESPACE_REQUIRED_BITS) - 1));
	bool isPvrtc = (TinyImageFormat_NAMESPACE_PVRTC == formatNamespace);

	// PVRTC - replaceRegion is the most straightforward method
	if (isPvrtc)
	{
		MTLRegion region = MTLRegionMake3D(0, 0, 0, sourceSize.width, sourceSize.height, sourceSize.depth);
		[pTexture->mtlTexture replaceRegion:region
								mipmapLevel:pSubresourceDesc->mMipLevel
								  withBytes:(uint8_t*)pIntermediate->pCpuMappedAddress + pSubresourceDesc->mSrcOffset
								bytesPerRow:0];
		return;
	}
#endif

	if (!pCmd->mtlBlitEncoder)
	{
		util_end_current_encoders(pCmd, false);
		pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
	}

	// Copy to the texture's final subresource.
	[pCmd->mtlBlitEncoder copyFromBuffer:pIntermediate->mtlBuffer
							sourceOffset:pSubresourceDesc->mSrcOffset + pIntermediate->mOffset
					   sourceBytesPerRow:pSubresourceDesc->mRowPitch
					 sourceBytesPerImage:pSubresourceDesc->mSlicePitch
							  sourceSize:sourceSize
							   toTexture:pTexture->mtlTexture
						destinationSlice:pSubresourceDesc->mArrayLayer
						destinationLevel:pSubresourceDesc->mMipLevel
					   destinationOrigin:MTLOriginMake(0, 0, 0)
								 options:MTLBlitOptionNone];
}

void mtl_acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pSwapChain);
	ASSERT(pSignalSemaphore || pFence);

	@autoreleasepool
	{
		if(@available(ios 13.0, *))
		{
			CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
			pSwapChain->mMTKDrawable = [layer nextDrawable];
		}

		pSwapChain->mIndex = (pSwapChain->mIndex + 1) % pSwapChain->mImageCount;
		*pImageIndex = pSwapChain->mIndex;

		pSwapChain->ppRenderTargets[pSwapChain->mIndex]->pTexture->mtlTexture = pSwapChain->mMTKDrawable.texture;
	}
}

void mtl_queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

	uint32_t    cmdCount = pDesc->mCmdCount;
	Cmd**       ppCmds = pDesc->ppCmds;
	Fence*      pFence = pDesc->pSignalFence;
	uint32_t    waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	uint32_t    signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
	Semaphore** ppSignalSemaphores = pDesc->ppSignalSemaphores;

	ASSERT(cmdCount > 0);
	ASSERT(ppCmds);
	if (waitSemaphoreCount > 0)
	{
		ASSERT(ppWaitSemaphores);
	}
	if (signalSemaphoreCount > 0)
	{
		ASSERT(ppSignalSemaphores);
	}

	@autoreleasepool
	{
		// set the queue built-in semaphore to signal when all command lists finished their execution
		__block uint32_t commandsFinished = 0;
		__block Fence* completedFence = NULL;

		if (pFence)
		{
			completedFence = pFence;
			pFence->mSubmitted = true;
		}

		for (uint32_t i = 0; i < cmdCount; i++)
		{
			__block Cmd* pCmd = ppCmds[i];
			[pCmd->mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
				commandsFinished++;

#if defined(ENABLE_GPU_TIMESTAMPS)
				if (pCmd->pLastFrameQuery)
				{
					if (@available(macOS 10.15, iOS 10.3, *))
					{
						const double gpuStartTime([buffer GPUStartTime]);
						const double gpuEndTime([buffer GPUEndTime]);

						pCmd->pLastFrameQuery->mGpuTimestampStart =
							min(pCmd->pLastFrameQuery->mGpuTimestampStart, gpuStartTime * GPU_FREQUENCY);

						pCmd->pLastFrameQuery->mGpuTimestampEnd = max(pCmd->pLastFrameQuery->mGpuTimestampEnd, gpuEndTime * GPU_FREQUENCY);
					}
				}
#endif

				if (commandsFinished == cmdCount)
				{
					if (completedFence)
					{
						dispatch_semaphore_signal(completedFence->pMtlSemaphore);
					}
				}
			}];
		}

		// Signal the signal semaphores after the last command buffer has finished execution
#if defined(ENABLE_EVENT_SEMAPHORE)
		if (@available(macOS 10.14, iOS 12.0, *))
		{
			for (uint32_t i = 0; i < signalSemaphoreCount; i++)
			{
				[ppCmds[cmdCount - 1]->mtlCommandBuffer encodeSignalEvent:ppSignalSemaphores[i]->pMtlSemaphore value:1];

				ppSignalSemaphores[i]->mSignaled = 1;
			}

			bool waitCommandBufferRequired = false;
			for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
			{
				if (ppWaitSemaphores[i]->mSignaled)
				{
					waitCommandBufferRequired = true;
					break;
				}
			}

			// Commit a wait command buffer if there are wait semaphores
			// To make sure command buffers from different queues get executed in correct order
			if (waitCommandBufferRequired)
			{
				id<MTLCommandBuffer> waitCommandBuffer = [pQueue->mtlCommandQueue commandBufferWithUnretainedReferences];
				for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
				{
					if (ppWaitSemaphores[i]->mSignaled)
					{
						[waitCommandBuffer encodeWaitForEvent:ppWaitSemaphores[i]->pMtlSemaphore value:1];

						ppWaitSemaphores[i]->mSignaled = 0;
					}
				}

				[waitCommandBuffer commit];
				waitCommandBuffer = nil;
			}
		}
#endif

		// commit the command lists
		for (uint32_t i = 0; i < cmdCount; i++)
		{
			// Commit any uncommitted encoder. This is necessary before committing the command buffer
			util_end_current_encoders(ppCmds[i], false);

			[ppCmds[i]->mtlCommandBuffer commit];
			ppCmds[i]->mtlCommandBuffer = nil;
		}
	}
}

void mtl_queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);
	ASSERT(pDesc->pSwapChain);

	@autoreleasepool
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;
		ASSERT(pQueue->mtlCommandQueue != nil);

		// after committing a command buffer no more commands can be encoded on it: create a new command buffer for future commands
		pSwapChain->presentCommandBuffer = [pQueue->mtlCommandQueue commandBuffer];

#ifndef TARGET_IOS
		[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];
#else
		//[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable
		//							 afterMinimumDuration:1.0 / 120.0];
		[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];
#endif

		pSwapChain->ppRenderTargets[pSwapChain->mIndex]->pTexture->mtlTexture = nil;
		pSwapChain->mMTKDrawable = nil;

		[pSwapChain->presentCommandBuffer commit];
		pSwapChain->presentCommandBuffer = nil;
	}
}

void mtl_waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
{
	ASSERT(pRenderer);
	ASSERT(fenceCount);
	ASSERT(ppFences);

	for (uint32_t i = 0; i < fenceCount; i++)
	{
		if (ppFences[i]->mSubmitted)
		{
			dispatch_semaphore_wait(ppFences[i]->pMtlSemaphore, DISPATCH_TIME_FOREVER);
		}
		ppFences[i]->mSubmitted = false;
	}
}

void mtl_waitQueueIdle(Queue* pQueue)
{
	ASSERT(pQueue);
	id<MTLCommandBuffer> waitCmdBuf = [pQueue->mtlCommandQueue commandBufferWithUnretainedReferences];

	[waitCmdBuf commit];
	[waitCmdBuf waitUntilCompleted];

	waitCmdBuf = nil;
}

void mtl_getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	ASSERT(pFence);
	*pFenceStatus = FENCE_STATUS_COMPLETE;
	if (pFence->mSubmitted)
	{
		// Check the fence status (and mark it as unsubmitted it if it has successfully decremented).
		long status = dispatch_semaphore_wait(pFence->pMtlSemaphore, DISPATCH_TIME_NOW);
		if (status == 0)
		{
			pFence->mSubmitted = false;
		}

		*pFenceStatus = (status == 0 ? FENCE_STATUS_COMPLETE : FENCE_STATUS_INCOMPLETE);
	}
}

void getRawTextureHandle(Renderer* pRenderer, Texture* pTexture, void** ppHandle)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(ppHandle);

	*ppHandle = (void*)CFBridgingRetain(pTexture->mtlTexture);
}

/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void mtl_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_COMMAND_BUFFER_DEBUG_MARKERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
        if(pCmd->mtlRenderEncoder)
        {
            [pCmd->mtlRenderEncoder pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
        }
        else
        {
            [pCmd->mtlCommandBuffer pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
        }
	}
	else
#endif
	{
		// #TODO: Figure out how to synchronize use command encoder debug markers
	}
}

void mtl_cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_COMMAND_BUFFER_DEBUG_MARKERS)
	if (@available(macOS 10.13, iOS 11.0, *))
	{
        if(pCmd->mtlRenderEncoder)
        {
            [pCmd->mtlRenderEncoder popDebugGroup];
        }
        else
        {
            [pCmd->mtlCommandBuffer popDebugGroup];
        }
	}
	else
#endif
	{
		// #TODO: Figure out how to synchronize use command encoder debug markers
	}
}

void mtl_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->mtlRenderEncoder)
		[pCmd->mtlRenderEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlComputeEncoder)
		[pCmd->mtlComputeEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlBlitEncoder)
		[pCmd->mtlBlitEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
}

uint32_t mtl_cmdWriteMarker(Cmd* pCmd, MarkerType markerType, uint32_t markerValue, Buffer* pBuffer, size_t offset, bool useAutoFlags)
{
	return 0;
}

void mtl_getTimestampFrequency(Queue* pQueue, double* pFrequency) { *pFrequency = GPU_FREQUENCY; }

void mtl_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
	ASSERT(pQueryPool);

	pQueryPool->mCount = pDesc->mQueryCount;
	pQueryPool->mGpuTimestampStart = DBL_MAX;
	pQueryPool->mGpuTimestampEnd = DBL_MIN;

	*ppQueryPool = pQueryPool;
}

void mtl_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool) { SAFE_FREE(pQueryPool); }

void mtl_cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount) {}

void mtl_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery) { pCmd->pLastFrameQuery = pQueryPool; }

void mtl_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery) {}

void mtl_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
	uint64_t* data = (uint64_t*)((uint8_t*)pReadbackBuffer->mtlBuffer.contents + pReadbackBuffer->mOffset);
	
	// Temporary workaround for a race condition: sometimes the command list
	// completion handler which populates these variables has not yet run,
	// so they are still DBL_MAX/DBL_MIN. These checks prevent undefined behavior
	// from trying to cast a too-large or too-small double value to a uint64_t.
	double timestampStart = pQueryPool->mGpuTimestampStart;
	double timestampEnd = pQueryPool->mGpuTimestampEnd;
	if (timestampStart != DBL_MAX)
	{
		data[0] = (uint64_t)timestampStart;
		pQueryPool->mGpuTimestampStart = DBL_MAX;
	}
	if (timestampEnd != DBL_MIN)
	{
		data[1] = (uint64_t)timestampEnd;
		pQueryPool->mGpuTimestampEnd = DBL_MIN;
	}
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void mtl_setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pBuffer);
	ASSERT(pName);

#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pBuffer->mtlBuffer)
	{
		NSString* str = [NSString stringWithUTF8String:pName];
		
#if !defined(TARGET_IOS)
		if (RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pBuffer->mMemoryUsage)
		{
			if (@available(macOS 10.12, iOS 10.0, *))
			{
				[pBuffer->mtlBuffer addDebugMarker:str range:NSMakeRange(pBuffer->mOffset, pBuffer->mSize)];
			}
		}
		else
#endif
		{
			pBuffer->mtlBuffer.label = str;
		}
	}
#endif
}

void mtl_setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
	ASSERT(pRenderer);
	ASSERT(pTexture);
	ASSERT(pName);

#if defined(ENABLE_GRAPHICS_DEBUG)
	NSString* str = [NSString stringWithUTF8String:pName];
	pTexture->mtlTexture.label = str;
#endif
}

void mtl_setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
	setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void mtl_setPipelineName(Renderer*, Pipeline*, const char*) {}
// -------------------------------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------
// Internal utility functions
// -------------------------------------------------------------------------------------------------

void util_bind_push_constant(Cmd* pCmd, const DescriptorInfo* pDesc, const void* pConstants)
{
	if (pDesc->mUsedStages & SHADER_STAGE_VERT)
	{
		[pCmd->mtlRenderEncoder setVertexBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
	}

	if (pDesc->mUsedStages & SHADER_STAGE_FRAG)
	{
		[pCmd->mtlRenderEncoder setFragmentBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
	}

	if (pDesc->mUsedStages & SHADER_STAGE_COMP)
	{
		[pCmd->mtlComputeEncoder setBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
	}
}

void util_bind_root_cbv(Cmd* pCmd, const RootDescriptorHandle* pHandle)
{
	if (pHandle->mStage & SHADER_STAGE_VERT)
	{
		[pCmd->mtlRenderEncoder setVertexBuffer:pHandle->pResource offset:pHandle->mOffset atIndex:	pHandle->mBinding];
	}

	if (pHandle->mStage & SHADER_STAGE_FRAG)
	{
		[pCmd->mtlRenderEncoder setFragmentBuffer:pHandle->pResource offset:pHandle->mOffset atIndex:	pHandle->mBinding];
	}

	if (pHandle->mStage & SHADER_STAGE_COMP)
	{
		[pCmd->mtlComputeEncoder setBuffer:pHandle->pResource offset:pHandle->mOffset atIndex:	pHandle->mBinding];
	}
}

void util_set_heaps_graphics(Cmd* pCmd)
{
#if defined(ENABLE_ARGUMENT_BUFFERS)
#if defined(ENABLE_ARGUMENT_BUFFER_USE_STAGES)
	if (@available(macOS 10.15, iOS 14.0, *))
	{
		[pCmd->mtlRenderEncoder useHeaps:pCmd->pRenderer->pHeaps
                                   count:pCmd->pRenderer->mHeapCount
								  stages:MTLRenderStageVertex | MTLRenderStageFragment];
	}
#else
	if (@available(macOS 10.13, iOS 14.0, *))
	{
		// Fallback on earlier versions
		[pCmd->mtlRenderEncoder useHeaps:pCmd->pRenderer->pHeaps count:pCmd->pRenderer->mHeapCount];
	}
#endif
#endif
}

void util_set_heaps_compute(Cmd* pCmd)
{
#if defined(ENABLE_ARGUMENT_BUFFERS)
	if (@available(macOS 10.13, iOS 14.0, *))
	{
		[pCmd->mtlComputeEncoder useHeaps:pCmd->pRenderer->pHeaps count:pCmd->pRenderer->mHeapCount];
	}
#endif
}

MTLVertexFormat util_to_mtl_vertex_format(const TinyImageFormat format)
{
	switch (format)
	{

		case TinyImageFormat_R8G8_UINT: return MTLVertexFormatUChar2;
		case TinyImageFormat_R8G8B8_UINT: return MTLVertexFormatUChar3;
		case TinyImageFormat_R8G8B8A8_UINT: return MTLVertexFormatUChar4;

		case TinyImageFormat_R8G8_SINT: return MTLVertexFormatChar2;
		case TinyImageFormat_R8G8B8_SINT: return MTLVertexFormatChar3;
		case TinyImageFormat_R8G8B8A8_SINT: return MTLVertexFormatChar4;

		case TinyImageFormat_R8G8_UNORM: return MTLVertexFormatUChar2Normalized;
		case TinyImageFormat_R8G8B8_UNORM: return MTLVertexFormatUChar3Normalized;
		case TinyImageFormat_R8G8B8A8_UNORM: return MTLVertexFormatUChar4Normalized;

		case TinyImageFormat_R8G8_SNORM: return MTLVertexFormatChar2Normalized;
		case TinyImageFormat_R8G8B8_SNORM: return MTLVertexFormatChar3Normalized;
		case TinyImageFormat_R8G8B8A8_SNORM: return MTLVertexFormatChar4Normalized;

		case TinyImageFormat_R16G16_UNORM: return MTLVertexFormatUShort2Normalized;
		case TinyImageFormat_R16G16B16_UNORM: return MTLVertexFormatUShort3Normalized;
		case TinyImageFormat_R16G16B16A16_UNORM: return MTLVertexFormatUShort4Normalized;

		case TinyImageFormat_R16G16_SNORM: return MTLVertexFormatShort2Normalized;
		case TinyImageFormat_R16G16B16_SNORM: return MTLVertexFormatShort3Normalized;
		case TinyImageFormat_R16G16B16A16_SNORM: return MTLVertexFormatShort4Normalized;

		case TinyImageFormat_R16G16_SINT: return MTLVertexFormatShort2;
		case TinyImageFormat_R16G16B16_SINT: return MTLVertexFormatShort3;
		case TinyImageFormat_R16G16B16A16_SINT: return MTLVertexFormatShort4;

		case TinyImageFormat_R16G16_UINT: return MTLVertexFormatUShort2;
		case TinyImageFormat_R16G16B16_UINT: return MTLVertexFormatUShort3;
		case TinyImageFormat_R16G16B16A16_UINT: return MTLVertexFormatUShort4;

		case TinyImageFormat_R16G16_SFLOAT: return MTLVertexFormatHalf2;
		case TinyImageFormat_R16G16B16_SFLOAT: return MTLVertexFormatHalf3;
		case TinyImageFormat_R16G16B16A16_SFLOAT: return MTLVertexFormatHalf4;

		case TinyImageFormat_R32_SFLOAT: return MTLVertexFormatFloat;
		case TinyImageFormat_R32G32_SFLOAT: return MTLVertexFormatFloat2;
		case TinyImageFormat_R32G32B32_SFLOAT: return MTLVertexFormatFloat3;
		case TinyImageFormat_R32G32B32A32_SFLOAT: return MTLVertexFormatFloat4;

		case TinyImageFormat_R32_SINT: return MTLVertexFormatInt;
		case TinyImageFormat_R32G32_SINT: return MTLVertexFormatInt2;
		case TinyImageFormat_R32G32B32_SINT: return MTLVertexFormatInt3;
		case TinyImageFormat_R32G32B32A32_SINT: return MTLVertexFormatInt4;

		case TinyImageFormat_R10G10B10A2_SNORM: return MTLVertexFormatInt1010102Normalized;
		case TinyImageFormat_R10G10B10A2_UNORM: return MTLVertexFormatUInt1010102Normalized;

		case TinyImageFormat_R32_UINT: return MTLVertexFormatUInt;
		case TinyImageFormat_R32G32_UINT: return MTLVertexFormatUInt2;
		case TinyImageFormat_R32G32B32_UINT: return MTLVertexFormatUInt3;
		case TinyImageFormat_R32G32B32A32_UINT:
			return MTLVertexFormatUInt4;

			// TODO add this UINT + UNORM format to TinyImageFormat
			//		case TinyImageFormat_RGB10A2: return MTLVertexFormatUInt1010102Normalized;
		default: break;
	}
	LOGF(LogLevel::eERROR, "Unknown vertex format: %d", format);
	return MTLVertexFormatInvalid;
}

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier)
{
	const bool barrierRequired(pCmd->pQueue->mBarrierFlags);
	UNREF_PARAM(barrierRequired);

	if (pCmd->mtlRenderEncoder != nil)
	{
		ASSERT(pCmd->mtlComputeEncoder == nil && pCmd->mtlBlitEncoder == nil);

#if defined(ENABLE_FENCES)
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			if (barrierRequired || forceBarrier)
			{
				[pCmd->mtlRenderEncoder updateFence:pCmd->pQueue->mtlQueueFence afterStages:MTLRenderStageFragment];
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
			}
		}
#endif

		[pCmd->mtlRenderEncoder endEncoding];
		pCmd->mtlRenderEncoder = nil;
	}

	if (pCmd->mtlComputeEncoder != nil)
	{
		ASSERT(pCmd->mtlRenderEncoder == nil && pCmd->mtlBlitEncoder == nil);

#if defined(ENABLE_FENCES)
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			if (barrierRequired || forceBarrier)
			{
				[pCmd->mtlComputeEncoder updateFence:pCmd->pQueue->mtlQueueFence];
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
			}
		}
#endif

		[pCmd->mtlComputeEncoder endEncoding];
		pCmd->mtlComputeEncoder = nil;
	}

	if (pCmd->mtlBlitEncoder != nil)
	{
		ASSERT(pCmd->mtlRenderEncoder == nil && pCmd->mtlComputeEncoder == nil);

#if defined(ENABLE_FENCES)
		if (@available(macOS 10.13, iOS 10.0, *))
		{
			if (barrierRequired || forceBarrier)
			{
				[pCmd->mtlBlitEncoder updateFence:pCmd->pQueue->mtlQueueFence];
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
			}
		}
#endif

		[pCmd->mtlBlitEncoder endEncoding];
		pCmd->mtlBlitEncoder = nil;
	}
}

void util_barrier_required(Cmd* pCmd, const QueueType& encoderType)
{
#if defined(ENABLE_FENCES)
	if (@available(macOS 10.13, iOS 10.0, *))
	{
		if (pCmd->pQueue->mBarrierFlags)
		{
			if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
			{
				switch (encoderType)
				{
					case QUEUE_TYPE_GRAPHICS:
						[pCmd->mtlRenderEncoder waitForFence:pCmd->pQueue->mtlQueueFence beforeStages:MTLRenderStageFragment];
						break;
					case QUEUE_TYPE_COMPUTE: [pCmd->mtlComputeEncoder waitForFence:pCmd->pQueue->mtlQueueFence]; break;
					case QUEUE_TYPE_TRANSFER: [pCmd->mtlBlitEncoder waitForFence:pCmd->pQueue->mtlQueueFence]; break;
					default: ASSERT(false);
				}
			}
			else
			{
				switch (encoderType)
				{
					case QUEUE_TYPE_GRAPHICS:
						if (@available(macOS 10.14, *))
						{
#if defined(ENABLE_MEMORY_BARRIERS_GRAPHICS)
							if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
							{
								[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers
																   afterStages:MTLRenderStageFragment
																  beforeStages:MTLRenderStageVertex];
							}

							if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
							{
								[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeTextures
																   afterStages:MTLRenderStageFragment
																  beforeStages:MTLRenderStageVertex];
							}

							if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_RENDERTARGETS)
							{
								[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeRenderTargets
																   afterStages:MTLRenderStageFragment
																  beforeStages:MTLRenderStageVertex];
							}
#endif
						}
						else
						{
							// memoryBarriers not available before macOS 10.14 and iOS 12.0
							[pCmd->mtlRenderEncoder waitForFence:pCmd->pQueue->mtlQueueFence beforeStages:MTLRenderStageFragment];
						}
						break;

					case QUEUE_TYPE_COMPUTE:
						if (@available(macOS 10.14, iOS 12.0, *))
						{
#if defined(ENABLE_MEMORY_BARRIERS_COMPUTE)
							if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
							{
								[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
							}

							if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
							{
								[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeTextures];
							}
#endif
						}
						else
						{
							// memoryBarriers not available before macOS 10.14 and iOS 12.0
							[pCmd->mtlComputeEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
						}
						break;

					case QUEUE_TYPE_TRANSFER:
						// we cant use barriers with blit encoder, only fence if available
						if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
						{
							[pCmd->mtlBlitEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
						}
						break;

					default: ASSERT(false);
				}
			}

			pCmd->pQueue->mBarrierFlags = 0;
		}
	}
#endif
}

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT)
{
	ASSERT(ppTexture);

	size_t totalSize = sizeof(Texture);
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		totalSize += pDesc->mMipLevels * sizeof(id<MTLTexture>);

	Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), totalSize);
	ASSERT(pTexture);

	void* mem = (pTexture + 1);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		pTexture->pMtlUAVDescriptors = (id<MTLTexture> __strong*)mem;

	if (!(pDesc->mFlags & (TEXTURE_CREATION_FLAG_FORCE_2D | TEXTURE_CREATION_FLAG_FORCE_3D) ) && pDesc->mHeight == 1)

		((TextureDesc*)pDesc)->mMipLevels = 1;

	bool isDepthBuffer = TinyImageFormat_HasDepth(pDesc->mFormat) || TinyImageFormat_HasStencil(pDesc->mFormat);

	pTexture->mtlPixelFormat = (uint32_t)TinyImageFormat_ToMTLPixelFormat(pDesc->mFormat);

	if (pDesc->mFormat == TinyImageFormat_D24_UNORM_S8_UINT && !pRenderer->pCapBits->canRenderTargetWriteTo[pDesc->mFormat])
	{
		internal_log(eWARNING, "Format D24S8 is not supported on this device. Using D32S8 instead", "addTexture");
		pTexture->mtlPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		((TextureDesc*)pDesc)->mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
	}

	if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
	{
		pTexture->mOwnsImage = false;
	}
	// If we've passed a native handle, it means the texture is already on device memory, and we just need to assign it.
	else if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		
		struct MetalNativeTextureHandle
		{
			id<MTLTexture>    pTexture;
			CVPixelBufferRef  pPixelBuffer;
		};
		MetalNativeTextureHandle* handle = (MetalNativeTextureHandle*)pDesc->pNativeHandle;
		if (handle->pTexture)
		{
			pTexture->mtlTexture = handle->pTexture;
		}
		else if (handle->pPixelBuffer)
		{
			pTexture->mtlTexture = CVMetalTextureGetTexture(handle->pPixelBuffer);
		}
		else
		{
			ASSERT(false);
			internal_log(eERROR, "Invalid pNativeHandle specified in TextureDesc", "addTexture");
			return;
		}
	}
	// Otherwise, we need to create a new texture.
	else
	{
		pTexture->mOwnsImage = true;

		// Create a MTLTextureDescriptor that matches our requirements.
		MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];
		const MemoryType memoryType = isRT ? MEMORY_TYPE_GPU_ONLY_COLOR_RTS : MEMORY_TYPE_GPU_ONLY;

		textureDesc.pixelFormat = (MTLPixelFormat)pTexture->mtlPixelFormat;
		textureDesc.width = pDesc->mWidth;
		textureDesc.height = pDesc->mHeight;
		textureDesc.depth = pDesc->mDepth;
		textureDesc.mipmapLevelCount = pDesc->mMipLevels;
		textureDesc.sampleCount = pDesc->mSampleCount;
		textureDesc.arrayLength = pDesc->mArraySize;
		textureDesc.storageMode = gMemoryStorageModes[memoryType];
		textureDesc.cpuCacheMode = gMemoryCacheModes[memoryType];
		textureDesc.resourceOptions = gMemoryOptions[memoryType];
// Simulator doesn't support shared depth/stencil textures
#ifdef TARGET_IOS_SIMULATOR
		if (isDepthBuffer)
		{
			textureDesc.storageMode = MTLStorageModePrivate;
			textureDesc.resourceOptions = MTLResourceStorageModePrivate;
		}
#endif

		MTLTextureType textureType = {};
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_2D)
		{
			ASSERT(pDesc->mDepth == 1);
			textureType = MTLTextureType2D;
		}
		else if (pDesc->mFlags & TEXTURE_CREATION_FLAG_FORCE_3D)
		{
			textureType = MTLTextureType3D;
		}
		else
		{
			if (pDesc->mDepth > 1)
				textureType = MTLTextureType3D;
			else if (pDesc->mHeight > 1)
				textureType = MTLTextureType2D;
			else
				textureType = MTLTextureType1D;
		}

		switch (textureType)
		{
			case MTLTextureType3D:
			{
				textureDesc.textureType = MTLTextureType3D;
				break;
			}
			case MTLTextureType2D:
			{
				if (DESCRIPTOR_TYPE_TEXTURE_CUBE == (pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE_CUBE))
				{
					if (pDesc->mArraySize == 6)
					{
						textureDesc.textureType = MTLTextureTypeCube;
						textureDesc.arrayLength = 1;
					}
#ifndef TARGET_IOS
					else
					{
						textureDesc.textureType = MTLTextureTypeCubeArray;
						textureDesc.arrayLength /= 6;
					}
#else
					else if (@available(iOS 11.0, *))
					{
#if defined(ENABLE_TEXTURE_CUBE_ARRAYS)
						if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
						{
							textureDesc.textureType = MTLTextureTypeCubeArray;
							textureDesc.arrayLength /= 6;
						}
						else
#endif
						{
							internal_log(eERROR, "Cube Array textures are not supported on this iOS device", "addTexture");
						}
					}
					else
					{
						internal_log(eERROR, "Cube Array textures are not supported on this iOS device", "addTexture");
					}
#endif
				}
				else
				{
					if (pDesc->mArraySize > 1)
						textureDesc.textureType = MTLTextureType2DArray;
					else if (pDesc->mSampleCount > SAMPLE_COUNT_1)
						textureDesc.textureType = MTLTextureType2DMultisample;
					else
						textureDesc.textureType = MTLTextureType2D;
				}

				break;
			}
			case MTLTextureType1D:
			{
				if (pDesc->mArraySize > 1)
					textureDesc.textureType = MTLTextureType1DArray;
				else
					textureDesc.textureType = MTLTextureType1D;

				break;
			}
			default: break;
		}

		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE) != 0)
		{
			textureDesc.usage |= MTLTextureUsageShaderRead;
			if(TinyImageFormat_IsDepthAndStencil(pDesc->mFormat))
			{
				textureDesc.usage |= MTLTextureUsagePixelFormatView;
			}
		}

		if (isRT || isDepthBuffer)
		{
			textureDesc.usage |= MTLTextureUsageRenderTarget;
		}

		// RW texture flags
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE) != 0)
		{
			textureDesc.usage |= MTLTextureUsagePixelFormatView;
			textureDesc.usage |= MTLTextureUsageShaderWrite;
		}

		// For memoryless textures, we dont need any backing memory
#if defined(ENABLE_MEMORYLESS_TEXTURES)
		if (@available(iOS 10.0, *))
		{
			if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
			{
				textureDesc.resourceOptions = MTLResourceStorageModeMemoryless;

				pTexture->mtlTexture = [pRenderer->pDevice newTextureWithDescriptor:textureDesc];
				pTexture->mLazilyAllocated = true;
				ASSERT(pTexture->mtlTexture);
#if defined(ENABLE_GRAPHICS_DEBUG)
				setTextureName(pRenderer, pTexture, "Memoryless Texture");
#endif
			}
		}
#endif
		if (!pTexture->mtlTexture)
		{
#if defined(ENABLE_HEAPS)
			if (pRenderer->pActiveGpuSettings->mHeaps)
			{
				if (@available(macOS 10.13, iOS 10.0, *))
				{
					MTLSizeAndAlign sizeAlign = [pRenderer->pDevice heapTextureSizeAndAlignWithDescriptor:textureDesc];
#if defined(ENABLE_HEAP_PLACEMENT)
					bool useHeapPlacementHeaps = false;

					// Need to check mPlacementHeaps since it depends on gpu family as well as macOS/iOS version
					if (@available(macOS 10.15, iOS 13.0, *))
					{
						if (pRenderer->pActiveGpuSettings->mPlacementHeaps)
						{
							VmaAllocationInfo allocInfo = util_render_alloc(
								pRenderer, RESOURCE_MEMORY_USAGE_GPU_ONLY, memoryType, sizeAlign.size, sizeAlign.align, &pTexture->pAllocation);

							pTexture->mtlTexture = [allocInfo.deviceMemory->pHeap newTextureWithDescriptor:textureDesc
																									offset:allocInfo.offset];

							useHeapPlacementHeaps = true;
						}
					}

					if (!useHeapPlacementHeaps)
#endif
					{
						// If placement heaps are not supported we cannot use VMA
						// Instead we have to rely on MTLHeap automatic placement
						uint32_t heapIndex = util_find_heap_with_space(pRenderer, memoryType, sizeAlign);

						pTexture->mtlTexture = [pRenderer->pHeaps[heapIndex] newTextureWithDescriptor:textureDesc];
					}
				}
				else
				{
					pTexture->mtlTexture = [pRenderer->pDevice newTextureWithDescriptor:textureDesc];
				}
			}
			else
#endif
			{
				pTexture->mtlTexture = [pRenderer->pDevice newTextureWithDescriptor:textureDesc];
			}

			ASSERT(pTexture->mtlTexture);
                
			if(TinyImageFormat_IsDepthAndStencil(pDesc->mFormat))
			{
				if (@available(macOS 10.12, iOS 10.0, *))
				{
#ifndef TARGET_IOS
					pTexture->mtlStencilTexture = [pTexture->mtlTexture newTextureViewWithPixelFormat:(pTexture->mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8 ? MTLPixelFormatX32_Stencil8 : MTLPixelFormatX24_Stencil8)];
#else
					pTexture->mtlStencilTexture = [pTexture->mtlTexture newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8];
#endif
					ASSERT(pTexture->mtlStencilTexture);
				}
			}
		}
	}

	NSRange slices = NSMakeRange(0, pDesc->mArraySize);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		MTLTextureType uavType = pTexture->mtlTexture.textureType;
		if (pTexture->mtlTexture.textureType == MTLTextureTypeCube)
		{
			uavType = MTLTextureType2DArray;
		}
#if defined(ENABLE_TEXTURE_CUBE_ARRAYS)
		else if (@available(macOS 10.11, iOS 11.0, *))
		{
			if (pTexture->mtlTexture.textureType == MTLTextureTypeCubeArray)
			{
				uavType = MTLTextureType2DArray;
			}
		}
#endif

		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			NSRange levels = NSMakeRange(i, 1);
			pTexture->pMtlUAVDescriptors[i] = [pTexture->mtlTexture newTextureViewWithPixelFormat:pTexture->mtlTexture.pixelFormat
																					  textureType:uavType
																						   levels:levels
																						   slices:slices];
		}
	}

	pTexture->mRT = isRT;
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mUav = pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;
	pTexture->mArraySizeMinusOne = pDesc->mArraySize - 1;
	pTexture->mFormat = pDesc->mFormat;
	pTexture->mSampleCount = pDesc->mSampleCount;
#if defined(ENABLE_GRAPHICS_DEBUG)
	if (pDesc->pName)
	{
		setTextureName(pRenderer, pTexture, pDesc->pName);
	}
#endif
	*ppTexture = pTexture;
}

// Virtual Textures
void mtl_addVirtualTexture(Cmd* pCmd, const TextureDesc* pDesc, Texture** ppTexture, void* pImageData) {}

void mtl_removeVirtualTexture(Renderer* pRenderer, VirtualTexture* pSvt) {}

void mtl_cmdUpdateVirtualTexture(Cmd* cmd, Texture* pTexture, uint32_t currentImage) {}

/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION

void initMetalRenderer(const char* appName, const RendererDesc* pSettings, Renderer** ppRenderer)
{
	// API functions
	addFence = mtl_addFence;
	removeFence = mtl_removeFence;
	addSemaphore = mtl_addSemaphore;
	removeSemaphore = mtl_removeSemaphore;
	addQueue = mtl_addQueue;
	removeQueue = mtl_removeQueue;
	addSwapChain = mtl_addSwapChain;
	removeSwapChain = mtl_removeSwapChain;

	// command pool functions
	addCmdPool = mtl_addCmdPool;
	removeCmdPool = mtl_removeCmdPool;
	addCmd = mtl_addCmd;
	removeCmd = mtl_removeCmd;
	addCmd_n = mtl_addCmd_n;
	removeCmd_n = mtl_removeCmd_n;

	addRenderTarget = mtl_addRenderTarget;
	removeRenderTarget = mtl_removeRenderTarget;
	addSampler = mtl_addSampler;
	removeSampler = mtl_removeSampler;

	// Resource Load functions
	addBuffer = mtl_addBuffer;
	removeBuffer = mtl_removeBuffer;
	mapBuffer = mtl_mapBuffer;
	unmapBuffer = mtl_unmapBuffer;
	cmdUpdateBuffer = mtl_cmdUpdateBuffer;
	cmdUpdateSubresource = mtl_cmdUpdateSubresource;
	addTexture = mtl_addTexture;
	removeTexture = mtl_removeTexture;
	addVirtualTexture = mtl_addVirtualTexture;
	removeVirtualTexture = mtl_removeVirtualTexture;

	// shader functions
#if defined(TARGET_IOS)
	addIosShader = mtl_addShader;
#endif
	addShaderBinary = mtl_addShaderBinary;
	removeShader = mtl_removeShader;

	addRootSignature = mtl_addRootSignature;
	removeRootSignature = mtl_removeRootSignature;

	// pipeline functions
	addPipeline = mtl_addPipeline;
	removePipeline = mtl_removePipeline;
	addPipelineCache = mtl_addPipelineCache;
	getPipelineCacheData = mtl_getPipelineCacheData;
	removePipelineCache = mtl_removePipelineCache;

	// Descriptor Set functions
	addDescriptorSet = mtl_addDescriptorSet;
	removeDescriptorSet = mtl_removeDescriptorSet;
	updateDescriptorSet = mtl_updateDescriptorSet;

	// command buffer functions
	resetCmdPool = mtl_resetCmdPool;
	beginCmd = mtl_beginCmd;
	endCmd = mtl_endCmd;
	cmdBindRenderTargets = mtl_cmdBindRenderTargets;
	cmdSetShadingRate = mtl_cmdSetShadingRate;
	cmdSetViewport = mtl_cmdSetViewport;
	cmdSetScissor = mtl_cmdSetScissor;
	cmdSetStencilReferenceValue = mtl_cmdSetStencilReferenceValue;
	cmdBindPipeline = mtl_cmdBindPipeline;
	cmdBindDescriptorSet = mtl_cmdBindDescriptorSet;
	cmdBindPushConstants = mtl_cmdBindPushConstants;
	cmdBindDescriptorSetWithRootCbvs = mtl_cmdBindDescriptorSetWithRootCbvs;
	cmdBindIndexBuffer = mtl_cmdBindIndexBuffer;
	cmdBindVertexBuffer = mtl_cmdBindVertexBuffer;
	cmdDraw = mtl_cmdDraw;
	cmdDrawInstanced = mtl_cmdDrawInstanced;
	cmdDrawIndexed = mtl_cmdDrawIndexed;
	cmdDrawIndexedInstanced = mtl_cmdDrawIndexedInstanced;
	cmdDispatch = mtl_cmdDispatch;

	// Transition Commands
	cmdResourceBarrier = mtl_cmdResourceBarrier;
	// Virtual Textures
	cmdUpdateVirtualTexture = mtl_cmdUpdateVirtualTexture;

	// queue/fence/swapchain functions
	acquireNextImage = mtl_acquireNextImage;
	queueSubmit = mtl_queueSubmit;
	queuePresent = mtl_queuePresent;
	waitQueueIdle = mtl_waitQueueIdle;
	getFenceStatus = mtl_getFenceStatus;
	waitForFences = mtl_waitForFences;
	toggleVSync = mtl_toggleVSync;

	getRecommendedSwapchainFormat = mtl_getRecommendedSwapchainFormat;

	//indirect Draw functions
	addIndirectCommandSignature = mtl_addIndirectCommandSignature;
	removeIndirectCommandSignature = mtl_removeIndirectCommandSignature;
	cmdExecuteIndirect = mtl_cmdExecuteIndirect;

	/************************************************************************/
	// GPU Query Interface
	/************************************************************************/
	getTimestampFrequency = mtl_getTimestampFrequency;
	addQueryPool = mtl_addQueryPool;
	removeQueryPool = mtl_removeQueryPool;
	cmdResetQueryPool = mtl_cmdResetQueryPool;
	cmdBeginQuery = mtl_cmdBeginQuery;
	cmdEndQuery = mtl_cmdEndQuery;
	cmdResolveQuery = mtl_cmdResolveQuery;
	/************************************************************************/
	// Stats Info Interface
	/************************************************************************/
	calculateMemoryStats = mtl_calculateMemoryStats;
	calculateMemoryUse = mtl_calculateMemoryUse;
	freeMemoryStats = mtl_freeMemoryStats;
	/************************************************************************/
	// Debug Marker Interface
	/************************************************************************/
	cmdBeginDebugMarker = mtl_cmdBeginDebugMarker;
	cmdEndDebugMarker = mtl_cmdEndDebugMarker;
	cmdAddDebugMarker = mtl_cmdAddDebugMarker;
	cmdWriteMarker = mtl_cmdWriteMarker;
	/************************************************************************/
	// Resource Debug Naming Interface
	/************************************************************************/
	setBufferName = mtl_setBufferName;
	setTextureName = mtl_setTextureName;
	setRenderTargetName = mtl_setRenderTargetName;
	setPipelineName = mtl_setPipelineName;

	mtl_initRenderer(appName, pSettings, ppRenderer);
}

void exitMetalRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);

	mtl_exitRenderer(pRenderer);
}

void initMetalRendererContext(const char* appName, const RendererContextDesc* pSettings, RendererContext** ppContext)
{
	// No need to initialize API function pointers, initRenderer MUST be called before using anything else anyway.
	mtl_initRendererContext(appName, pSettings, ppContext);
}

void exitMetalRendererContext(RendererContext* pContext)
{
	ASSERT(pContext);
	mtl_exitRendererContext(pContext);
}

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
}    // namespace RENDERER_CPP_NAMESPACE
#endif
#endif
