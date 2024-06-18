/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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
// #define ARGUMENTBUFFER_DEBUG

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif
#import <MetalKit/MetalKit.h>
#include <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <simd/simd.h>

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#import "../../Graphics/Interfaces/IGraphics.h"
#include "../../OS/Interfaces/IOperatingSystem.h"

#ifdef ENABLE_OS_PROC_MEMORY
#include <os/proc.h>
#endif

// Fallback if os_proc_available_memory not available
#include <mach/mach.h>
#include <mach/mach_host.h>

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Math/MathTypes.h"
#include "../../Utilities/Threading/Atomics.h"

#include "MetalCapBuilder.h"
#include "MetalMemoryAllocatorImpl.h"

#include "../../Utilities/Interfaces/IMemory.h"

#if defined(AUTOMATED_TESTING)
#include "IScreenshot.h"
#endif
#define MAX_BUFFER_BINDINGS            31
// Start vertex attribute bindings at index 30 and decrement so we can bind regular buffers from index 0 for simplicity
#define VERTEX_BINDING_OFFSET          (MAX_BUFFER_BINDINGS - 1)
#define DESCRIPTOR_UPDATE_FREQ_PADDING 0

VkAllocationCallbacks gMtlAllocationCallbacks = {
    // pUserData
    NULL,
    // pfnAllocation
    [](void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) { return tf_memalign(alignment, size); },
    // pfnReallocation
    [](void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
    { return tf_realloc(pOriginal, size); },
    // pfnFree
    [](void* pUserData, void* pMemory) { tf_free(pMemory); },
    // pfnInternalAllocation
    [](void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) {},
    // pfnInternalFree
    [](void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope) {}
};

typedef struct DescriptorIndexMap
{
    char*    key;
    uint32_t value;
} DescriptorIndexMap;

typedef struct QuerySampleRange
{
    // Sample start in sample buffer
    uint32_t mRenderStartIndex;
    // Number of samples from 'RenderStartIndex'
    uint32_t mRenderSamples;

    uint32_t mComputeStartIndex;
    uint32_t mComputeSamples;
} QuerySampleRange;

// MAX: 4
#define NUM_RENDER_STAGE_BOUNDARY_COUNTERS  4
#define NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS 2

bool gIssuedQueryIgnoredWarning = false;

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
namespace RENDERER_CPP_NAMESPACE
{
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
	static const MTLSamplerAddressMode gMtlAddressModeTranslator[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
		MTLSamplerAddressModeClampToBorderColor,
	};
#else

	static const MTLSamplerAddressMode gMtlAddressModeTranslatorFallback[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
		MTLSamplerAddressModeClampToEdge,
	};
#endif

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

MTLVertexFormat   util_to_mtl_vertex_format(const TinyImageFormat format);
MTLSamplePosition util_to_mtl_locations(SampleLocations location);

static inline FORGE_CONSTEXPR MTLLoadAction util_to_mtl_load_action(const LoadActionType loadActionType)
{
    switch (loadActionType)
    {
    case LOAD_ACTION_DONTCARE:
        return MTLLoadActionDontCare;
    case LOAD_ACTION_LOAD:
        return MTLLoadActionLoad;
    case LOAD_ACTION_CLEAR:
        return MTLLoadActionClear;
    default:
        return MTLLoadActionDontCare;
    }
}

static inline FORGE_CONSTEXPR MTLStoreAction util_to_mtl_store_action(const StoreActionType storeActionType)
{
    switch (storeActionType)
    {
    case STORE_ACTION_STORE:
        return MTLStoreActionStore;
    case STORE_ACTION_DONTCARE:
        return MTLStoreActionDontCare;
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
    case STORE_ACTION_RESOLVE_STORE:
        return MTLStoreActionStoreAndMultisampleResolve;
    case STORE_ACTION_RESOLVE_DONTCARE:
        return MTLStoreActionMultisampleResolve;
#endif
    default:
        return MTLStoreActionStore;
    }
}

static inline MemoryType util_to_memory_type(ResourceMemoryUsage usage)
{
    MemoryType memoryType = MEMORY_TYPE_GPU_ONLY;
    if (RESOURCE_MEMORY_USAGE_CPU_TO_GPU == usage)
    {
        memoryType = MEMORY_TYPE_CPU_TO_GPU;
    }
    else if (RESOURCE_MEMORY_USAGE_CPU_ONLY == usage || RESOURCE_MEMORY_USAGE_GPU_TO_CPU == usage)
    {
        memoryType = MEMORY_TYPE_GPU_TO_CPU;
    }

    return memoryType;
}

void util_set_heaps_graphics(Cmd* pCmd);
void util_set_heaps_compute(Cmd* pCmd);

void initialize_texture_desc(Renderer* pRenderer, const TextureDesc* pDesc, const bool isRT, MTLPixelFormat pixelFormat,
                             MTLTextureDescriptor* textureDesc);
void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT);

void mtl_createShaderReflection(Renderer* pRenderer, Shader* shader, ShaderStage shaderStage, ShaderReflection* pOutReflection);

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);
void util_set_debug_group(Cmd* pCmd);
void util_unset_debug_group(Cmd* pCmd);

id<MTLCounterSet>          util_get_counterset(id<MTLDevice> device);
MTLCounterResultTimestamp* util_resolve_counter_sample_buffer(uint32_t startSample, uint32_t sampleCount,
                                                              id<MTLCounterSampleBuffer> pSampleBuffer);

// GPU frame time accessor for macOS and iOS
#define GPU_FREQUENCY 1000000000.0 // nanoseconds

DECLARE_RENDERER_FUNCTION(void, getBufferSizeAlign, Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
DECLARE_RENDERER_FUNCTION(void, getTextureSizeAlign, Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset,
                          uint64_t size)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer,
                          const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture,
                          const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)

/************************************************************************/
// Globals
/************************************************************************/
static Texture* pDefaultTextures[TEXTURE_DIM_COUNT] = {};
static Buffer*  pDefaultBuffer = {};
static Buffer*  pDefaultICB = {};
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
    NOREFS id*  pArr;
    NSUInteger* pOffsets;
    uint16_t    mCount;
    uint32_t    mStage : 5;
    uint32_t    mBinding : 26;
    uint32_t    mAccelerationStructure : 1;
} RootDescriptorHandle;

void resize_roothandle(RootDescriptorHandle* pHandle, uint32_t newCount)
{
    if (newCount > pHandle->mCount)
    {
        pHandle->mCount = newCount;
        pHandle->pArr = (NOREFS id<MTLResource>*)tf_realloc(pHandle->pArr, pHandle->mCount * sizeof(pHandle->pArr[0]));
        pHandle->pOffsets = (NSUInteger*)tf_realloc(pHandle->pOffsets, pHandle->mCount * sizeof(pHandle->pOffsets[0]));
    }
}

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

    if (pDescriptorSet->pRootSignature != pCmd->pUsedRootSignature)
    {
        pCmd->mShouldRebindDescriptorSets = 0;
        pCmd->pUsedRootSignature = pDescriptorSet->pRootSignature;
        for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; i++)
        {
            pCmd->mBoundDescriptorSets[i] = NULL;
        }
    }
    pCmd->mBoundDescriptorSets[pDescriptorSet->mUpdateFrequency] = pDescriptorSet;
    pCmd->mBoundDescriptorSetIndices[pDescriptorSet->mUpdateFrequency] = index;
    pCmd->mShouldRebindDescriptorSets &= ~(1 << (int)pDescriptorSet->mUpdateFrequency);

    if (pDescriptorSet->pRootDescriptorData)
    {
        RootDescriptorData* pData = pDescriptorSet->pRootDescriptorData + index;
        for (uint32_t i = 0; i < pDescriptorSet->mRootBufferCount; ++i)
        {
            const RootDescriptorHandle* pHandle = &pData->pBuffers[i];
            if (pHandle->mAccelerationStructure)
            {
#if defined(MTL_RAYTRACING_AVAILABLE)
                if (MTL_RAYTRACING_SUPPORTED)
                {
                    if (pData->pTextures[i].mStage & SHADER_STAGE_COMP)
                    {
                        [pCmd->pComputeEncoder setAccelerationStructure:pHandle->pArr[0] atBufferIndex:pHandle->mBinding];
                    }
                    else
                    {
#if defined(ENABLE_GFX_CMD_SET_ACCELERATION_STRUCTURE)
                        if (pHandle->mStage & SHADER_STAGE_VERT)
                        {
                            [pCmd->pRenderEncoder setVertexAccelerationStructure:pHandle->pArr[0] atBufferIndex:pHandle->mBinding];
                        }
                        if (pData->pTextures[i].mStage & SHADER_STAGE_FRAG)
                        {
                            [pCmd->pRenderEncoder setFragmentAccelerationStructure:pHandle->pArr[0] atBufferIndex:pHandle->mBinding];
                        }
#else
                        ASSERT(false);
#endif
                    }
                }
#endif
            }
            else
            {
                util_bind_root_cbv(pCmd, pHandle);
            }
        }

        for (uint32_t i = 0; i < pDescriptorSet->mRootTextureCount; ++i)
        {
            const RootDescriptorHandle* pHandle = &pData->pTextures[i];
            if (pHandle->mStage & SHADER_STAGE_VERT)
            {
                [pCmd->pRenderEncoder setVertexTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
            if (pData->pTextures[i].mStage & SHADER_STAGE_FRAG)
            {
                [pCmd->pRenderEncoder setFragmentTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
            if (pData->pTextures[i].mStage & SHADER_STAGE_COMP)
            {
                [pCmd->pComputeEncoder setTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
        }

        for (uint32_t i = 0; i < pDescriptorSet->mRootSamplerCount; ++i)
        {
            const RootDescriptorHandle* pHandle = &pData->pSamplers[i];
            if (pHandle->mStage & SHADER_STAGE_VERT)
            {
                [pCmd->pRenderEncoder setVertexSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
            if (pData->pSamplers[i].mStage & SHADER_STAGE_FRAG)
            {
                [pCmd->pRenderEncoder setFragmentSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
            if (pData->pSamplers[i].mStage & SHADER_STAGE_COMP)
            {
                [pCmd->pComputeEncoder setSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
            }
        }
    }

    if (!pDescriptorSet->mArgumentBuffer)
    {
        return;
    }

    const id<MTLBuffer> buffer = pDescriptorSet->mArgumentBuffer->pBuffer;
    const uint64_t      offset = pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride;

    // argument buffers
    if (pDescriptorSet->mStages & SHADER_STAGE_VERT)
    {
        [pCmd->pRenderEncoder setVertexBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
    }

    if (pDescriptorSet->mStages & SHADER_STAGE_FRAG)
    {
        [pCmd->pRenderEncoder setFragmentBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
    }

    if (pDescriptorSet->mStages & SHADER_STAGE_COMP)
    {
        [pCmd->pComputeEncoder setBuffer:buffer offset:offset atIndex:pDescriptorSet->mUpdateFrequency];
    }

    // useResource on the untracked resources (UAVs, RTs)
    // useHeap doc
    // You may only read or sample resources within the specified heaps. This method ignores render targets (textures that specify a
    // MTLTextureUsageRenderTarget usage option) and writable textures (textures that specify a MTLTextureUsageShaderWrite usage option)
    // within the array of heaps. To use these resources, you must call the useResource:usage:stages: method instead.
    if (pDescriptorSet->ppUntrackedData[index])
    {
        const UntrackedResourceData* untracked = pDescriptorSet->ppUntrackedData[index];
        if (pDescriptorSet->mStages & SHADER_STAGE_COMP)
        {
            if (untracked->mData.mCount)
            {
                [pCmd->pComputeEncoder useResources:untracked->mData.pResources count:untracked->mData.mCount usage:MTLResourceUsageRead];
            }
            if (untracked->mRWData.mCount)
            {
                [pCmd->pComputeEncoder useResources:untracked->mRWData.pResources
                                              count:untracked->mRWData.mCount
                                              usage:MTLResourceUsageRead | MTLResourceUsageWrite];
            }
        }
        else
        {
            if (untracked->mData.mCount)
            {
                [pCmd->pRenderEncoder useResources:untracked->mData.pResources count:untracked->mData.mCount usage:MTLResourceUsageRead];
            }
            if (untracked->mRWData.mCount)
            {
                [pCmd->pRenderEncoder useResources:untracked->mRWData.pResources
                                             count:untracked->mRWData.mCount
                                             usage:MTLResourceUsageRead | MTLResourceUsageWrite];
            }
        }
    }
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
    if (pRootSignature != pCmd->pUsedRootSignature)
    {
        pCmd->mShouldRebindDescriptorSets = 0;
        pCmd->pUsedRootSignature = pRootSignature;
        for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; i++)
        {
            pCmd->mBoundDescriptorSets[i] = NULL;
        }
    }
    util_bind_push_constant(pCmd, pDesc, pConstants);
}

#if defined(ENABLE_GRAPHICS_DEBUG) || defined(PVS_STUDIO)
#define VALIDATE_DESCRIPTOR(descriptor, msgFmt, ...)                           \
    if (!VERIFYMSG((descriptor), "%s : " msgFmt, __FUNCTION__, ##__VA_ARGS__)) \
    {                                                                          \
        continue;                                                              \
    }
#else
#define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

void mtl_cmdBindDescriptorSetWithRootCbvs(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                                          const DescriptorData* pParams)
{
    mtl_cmdBindDescriptorSet(pCmd, index, pDescriptorSet);

    const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

    for (uint32_t i = 0; i < count; ++i)
    {
        const DescriptorData* pParam = pParams + i;
        uint32_t              paramIndex = pParam->mBindByIndex ? pParam->mIndex : UINT32_MAX;

        const DescriptorInfo* pDesc =
            (paramIndex != UINT32_MAX) ? (pRootSignature->pDescriptors + paramIndex) : get_descriptor(pRootSignature, pParam->pName);
        if (!pDesc)
        {
            continue;
        }
        if (paramIndex != UINT32_MAX)
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
        }
        else
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
        }

        VALIDATE_DESCRIPTOR(pDesc->mRootDescriptor, "Descriptor (%s) - must be a root cbv", pDesc->pName);
        VALIDATE_DESCRIPTOR(pParam->mCount <= 1, "Descriptor (%s) - cmdBindDescriptorSetWithRootCbvs does not support arrays",
                            pDesc->pName);
        VALIDATE_DESCRIPTOR(pParam->pRanges, "Descriptor (%s) - pRanges must be provided for cmdBindDescriptorSetWithRootCbvs",
                            pDesc->pName);

        DescriptorDataRange range = pParam->pRanges[0];

        VALIDATE_DESCRIPTOR(range.mSize > 0, "Descriptor (%s) - pRanges->mSize is zero", pDesc->pName);

        RootDescriptorHandle handle = {};
        handle.mBinding = pDesc->mReg;
        handle.mStage = pDesc->mUsedStages;
        NOREFS id  resource = pParam->ppBuffers[0]->pBuffer;
        NSUInteger offset = pParam->ppBuffers[0]->mOffset + range.mOffset;
        handle.pArr = &resource;
        handle.pOffsets = &offset;
        handle.mCount = 1;
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
    const bool           needRootData = (pRootSignature->mRootBufferCounts[updateFreq] || pRootSignature->mRootTextureCounts[updateFreq] ||
                               pRootSignature->mRootSamplerCounts[updateFreq]);

    uint32_t totalSize = sizeof(DescriptorSet);

    if (needRootData)
    {
        totalSize += pDesc->mMaxSets * sizeof(RootDescriptorData);
        totalSize += pDesc->mMaxSets * pRootSignature->mRootTextureCounts[updateFreq] * sizeof(RootDescriptorHandle);
        totalSize += pDesc->mMaxSets * pRootSignature->mRootBufferCounts[updateFreq] * sizeof(RootDescriptorHandle);
        totalSize += pDesc->mMaxSets * pRootSignature->mRootSamplerCounts[updateFreq] * sizeof(RootDescriptorHandle);
    }

    if (pRootSignature->mArgumentDescriptors[pDesc->mUpdateFrequency].count)
    {
        totalSize += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
    }

    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
    ASSERT(pDescriptorSet);

    pDescriptorSet->pRootSignature = pRootSignature;
    pDescriptorSet->mUpdateFrequency = updateFreq;
    pDescriptorSet->mNodeIndex = nodeIndex;
    pDescriptorSet->mMaxSets = pDesc->mMaxSets;
    pDescriptorSet->mRootTextureCount = pRootSignature->mRootTextureCounts[updateFreq];
    pDescriptorSet->mRootBufferCount = pRootSignature->mRootBufferCounts[updateFreq];
    pDescriptorSet->mRootSamplerCount = pRootSignature->mRootSamplerCounts[updateFreq];

    uint8_t* mem = (uint8_t*)(pDescriptorSet + 1);

    if (needRootData)
    {
        pDescriptorSet->pRootDescriptorData = (RootDescriptorData*)mem;
        mem += pDesc->mMaxSets * sizeof(RootDescriptorData);

        for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
        {
            pDescriptorSet->pRootDescriptorData[i].pTextures = (RootDescriptorHandle*)mem;
            mem += pRootSignature->mRootTextureCounts[updateFreq] * sizeof(RootDescriptorHandle);

            pDescriptorSet->pRootDescriptorData[i].pBuffers = (RootDescriptorHandle*)mem;
            mem += pRootSignature->mRootBufferCounts[updateFreq] * sizeof(RootDescriptorHandle);

            pDescriptorSet->pRootDescriptorData[i].pSamplers = (RootDescriptorHandle*)mem;
            mem += pRootSignature->mRootSamplerCounts[updateFreq] * sizeof(RootDescriptorHandle);
        }
    }

    NSMutableArray<MTLArgumentDescriptor*>* descriptors = pRootSignature->mArgumentDescriptors[pDesc->mUpdateFrequency];

    if (descriptors.count)
    {
        ShaderStage shaderStages =
            (pRootSignature->mPipelineType == PIPELINE_TYPE_COMPUTE ? SHADER_STAGE_COMP : (SHADER_STAGE_VERT | SHADER_STAGE_FRAG));

#ifdef ENABLE_GRAPHICS_DEBUG
        // to circumvent a metal validation bug which overwrites the arguments array, we make a local copy
        NSMutableArray<MTLArgumentDescriptor*>* descriptorsCopy = [[NSMutableArray alloc] init];
        for (MTLArgumentDescriptor* myArrayElement in descriptors)
        {
            MTLArgumentDescriptor* argDescriptor = [MTLArgumentDescriptor argumentDescriptor];
            argDescriptor.access = myArrayElement.access;
            argDescriptor.arrayLength = myArrayElement.arrayLength;
            argDescriptor.constantBlockAlignment = myArrayElement.constantBlockAlignment;
            argDescriptor.dataType = myArrayElement.dataType;
            argDescriptor.index = myArrayElement.index;
            argDescriptor.textureType = myArrayElement.textureType;
            [descriptorsCopy addObject:argDescriptor];
        }
        descriptors = descriptorsCopy;
#endif

        // create encoder
        pDescriptorSet->mArgumentEncoder = [pRenderer->pDevice newArgumentEncoderWithArguments:descriptors];
        ASSERT(pDescriptorSet->mArgumentEncoder);

        // Create argument buffer
        uint32_t   argumentBufferSize = (uint32_t)round_up_64(pDescriptorSet->mArgumentEncoder.encodedLength, 256);
        BufferDesc bufferDesc = {};
        bufferDesc.mAlignment = 256;
        bufferDesc.mSize = argumentBufferSize * pDesc->mMaxSets;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
#if defined(ENABLE_GRAPHICS_DEBUG)
        char debugName[FS_MAX_PATH] = {};
        snprintf(debugName, sizeof debugName, "Argument Buffer %u", pDesc->mUpdateFrequency);
        bufferDesc.pName = debugName;
#endif
        addBuffer(pRenderer, &bufferDesc, &pDescriptorSet->mArgumentBuffer);

        pDescriptorSet->mStride = argumentBufferSize;
        pDescriptorSet->mStages = shaderStages;

        pDescriptorSet->ppUntrackedData = (UntrackedResourceData**)mem;
        mem += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
    }

    // bind static samplers
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        const DescriptorInfo* desc = &pRootSignature->pDescriptors[i];

        if (desc->mType == DESCRIPTOR_TYPE_SAMPLER && desc->pStaticSampler)
        {
            if (desc->mIsArgumentBufferField)
            {
                if (desc->mReg != updateFreq)
                {
                    continue;
                }

                for (uint32_t j = 0; j < pDescriptorSet->mMaxSets; ++j)
                {
                    [pDescriptorSet->mArgumentEncoder
                        setArgumentBuffer:pDescriptorSet->mArgumentBuffer->pBuffer
                                   offset:pDescriptorSet->mArgumentBuffer->mOffset + j * pDescriptorSet->mStride];
                    [pDescriptorSet->mArgumentEncoder setSamplerState:desc->pStaticSampler atIndex:desc->mHandleIndex];
                }
            }
            else if (desc->mUpdateFrequency == updateFreq)
            {
                for (uint32_t i = 0; i < pDescriptorSet->mMaxSets; ++i)
                {
                    RootDescriptorHandle* handle = &pDescriptorSet->pRootDescriptorData[i].pSamplers[desc->mHandleIndex];
                    handle->mBinding = desc->mReg;
                    handle->mStage = desc->mUsedStages;
                    resize_roothandle(handle, 1);
                    handle->pArr[0] = desc->pStaticSampler;
                }
            }
        }
        else if (desc->mIsArgumentBufferField)
        {
            if (desc->mUpdateFrequency != updateFreq)
            {
                continue;
            }

            for (uint32_t index = 0; index < pDescriptorSet->mMaxSets; ++index)
            {
                [pDescriptorSet->mArgumentEncoder
                    setArgumentBuffer:pDescriptorSet->mArgumentBuffer->pBuffer
                               offset:pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride];

                const DescriptorType type((DescriptorType)desc->mType);
                const uint32_t       arrayStart = 0;
                const uint32_t       arrayCount = desc->mSize;

                switch (type)
                {
                case DESCRIPTOR_TYPE_SAMPLER:
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        [pDescriptorSet->mArgumentEncoder setSamplerState:pDefaultSampler->pSamplerState
                                                                  atIndex:desc->mHandleIndex + arrayStart + j];
                    }
                    break;
                }
                case DESCRIPTOR_TYPE_TEXTURE:
                case DESCRIPTOR_TYPE_RW_TEXTURE:
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        const Texture* texture = pDefaultTextures[desc->mDim];
                        [pDescriptorSet->mArgumentEncoder setTexture:texture->pTexture atIndex:desc->mHandleIndex + arrayStart + j];
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
                    if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                    {
                        if (pRenderer->pGpu->mSettings.mIndirectCommandBuffer)
                        {
                            for (uint32_t j = 0; j < arrayCount; ++j)
                            {
                                [pDescriptorSet->mArgumentEncoder setIndirectCommandBuffer:pDefaultICB->pIndirectCommandBuffer
                                                                                   atIndex:desc->mHandleIndex + arrayStart + j];
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            Buffer* buffer = pDefaultBuffer;

                            [pDescriptorSet->mArgumentEncoder setBuffer:buffer->pBuffer
                                                                 offset:0
                                                                atIndex:desc->mHandleIndex + arrayStart + j];
                        }
                    }

                    break;
                }
                case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
                    break;
                default:
                {
                    ASSERT(false); // unsupported descriptor type
                    break;
                }
                }
            }
        }
    }

    *ppDescriptorSet = pDescriptorSet;
}

void mtl_removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
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

    // If we have samplers, buffers or textures then we need `pRootDescriptorData`, so call VERIFY on it
    if ((pDescriptorSet->mRootSamplerCount || pDescriptorSet->mRootBufferCount || pDescriptorSet->mRootTextureCount) &&
        VERIFY(pDescriptorSet->pRootDescriptorData))
    {
        for (uint32_t i = 0; i < pDescriptorSet->mMaxSets; ++i)
        {
            RootDescriptorData* pRootDescriptorData = pDescriptorSet->pRootDescriptorData + i;

            // We also need to check that each resource array is a valid pointer to keep the sanitizer happy
            if (pDescriptorSet->mRootSamplerCount && VERIFY(pRootDescriptorData->pSamplers))
            {
                for (uint32_t j = 0; j < pDescriptorSet->mRootSamplerCount; ++j)
                {
                    SAFE_FREE(pRootDescriptorData->pSamplers[j].pArr);
                    SAFE_FREE(pRootDescriptorData->pSamplers[j].pOffsets);
                }
            }
            if (pDescriptorSet->mRootBufferCount && VERIFY(pRootDescriptorData->pBuffers))
            {
                for (uint32_t j = 0; j < pDescriptorSet->mRootBufferCount; ++j)
                {
                    SAFE_FREE(pRootDescriptorData->pBuffers[j].pArr);
                    SAFE_FREE(pRootDescriptorData->pBuffers[j].pOffsets);
                }
            }
            if (pDescriptorSet->mRootTextureCount && VERIFY(pRootDescriptorData->pTextures))
            {
                for (uint32_t j = 0; j < pDescriptorSet->mRootTextureCount; ++j)
                {
                    SAFE_FREE(pRootDescriptorData->pTextures[j].pArr);
                    SAFE_FREE(pRootDescriptorData->pTextures[j].pOffsets);
                }
            }
        }
    }
    SAFE_FREE(pDescriptorSet);
}

// UAVs, RTs inside arg buffers need to be tracked manually through useResource
// useHeap ignores all resources with UAV, RT flags
static void TrackUntrackedResource(DescriptorSet* pDescriptorSet, uint32_t index, const MTLResourceUsage usage, id<MTLResource> resource)
{
    if (!pDescriptorSet->ppUntrackedData[index])
    {
        pDescriptorSet->ppUntrackedData[index] = (UntrackedResourceData*)tf_calloc(1, sizeof(UntrackedResourceData));
    }

    UntrackedResourceData*  untracked = pDescriptorSet->ppUntrackedData[index];
    UntrackedResourceArray* dataArray = (usage & MTLResourceUsageWrite) ? &untracked->mRWData : &untracked->mData;

    if (dataArray->mCount >= dataArray->mCapacity)
    {
        ++dataArray->mCapacity;
        dataArray->pResources = (NOREFS id<MTLResource>*)tf_realloc(dataArray->pResources, dataArray->mCapacity * sizeof(id<MTLResource>));
    }

    dataArray->pResources[dataArray->mCount++] = resource;
}

static void BindICBDescriptor(DescriptorSet* pDescriptorSet, uint32_t index, const DescriptorInfo* pDesc, uint32_t arrayStart,
                              uint32_t arrayCount, const DescriptorData* pParam)
{
    for (uint32_t j = 0; j < arrayCount; ++j)
    {
        [pDescriptorSet->mArgumentEncoder setIndirectCommandBuffer:pParam->ppBuffers[j]->pIndirectCommandBuffer
                                                           atIndex:pDesc->mHandleIndex + arrayStart + j];

        TrackUntrackedResource(pDescriptorSet, index, MTLResourceUsageWrite, pParam->ppBuffers[j]->pIndirectCommandBuffer);
    }
}

void mtl_updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count,
                             const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mMaxSets);

    const RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

    if (pDescriptorSet->mArgumentEncoder)
    {
        // set argument buffer to update
        [pDescriptorSet->mArgumentEncoder setArgumentBuffer:pDescriptorSet->mArgumentBuffer->pBuffer
                                                     offset:pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride];

        if (pDescriptorSet->ppUntrackedData[index])
        {
            pDescriptorSet->ppUntrackedData[index]->mData.mCount = 0;
            pDescriptorSet->ppUntrackedData[index]->mRWData.mCount = 0;
        }
    }

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
                if (pDesc->mIsArgumentBufferField)
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        [pDescriptorSet->mArgumentEncoder setSamplerState:pParam->ppSamplers[j]->pSamplerState
                                                                  atIndex:pDesc->mHandleIndex + arrayStart + j];
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pSamplers[pDesc->mHandleIndex];
                    pData->mBinding = pDesc->mReg;
                    pData->mStage = pDesc->mUsedStages;
                    resize_roothandle(pData, arrayCount);
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        pData->pArr[j] = pParam->ppSamplers[j]->pSamplerState;
                    }
                }

                break;
            }
            case DESCRIPTOR_TYPE_TEXTURE:
            {
                if (pDesc->mIsArgumentBufferField)
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        const Texture* texture = pParam->ppTextures[j];
                        NOREFS id<MTLTexture> untracked = nil;

                        if (pParam->mBindStencilResource)
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pStencilTexture
                                                                 atIndex:pDesc->mHandleIndex + arrayStart + j];
                            untracked = texture->pStencilTexture;
                        }
                        else if (pParam->mUAVMipSlice)
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[pParam->mUAVMipSlice]
                                                                 atIndex:pDesc->mHandleIndex + arrayStart + j];
                            untracked = texture->pUAVDescriptors[pParam->mUAVMipSlice];
                        }
                        else
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pTexture atIndex:pDesc->mHandleIndex + arrayStart + j];
                            untracked = texture->pTexture;
                        }

                        bool isRT = texture->pTexture.usage & MTLTextureUsageRenderTarget;
                        if (isRT || texture->mUav || !texture->pAllocation)
                        {
                            TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, untracked);
                        }
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[pDesc->mHandleIndex];
                    pData->mBinding = pDesc->mReg;
                    pData->mStage = pDesc->mUsedStages;
                    resize_roothandle(pData, arrayCount);
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        if (pParam->mBindStencilResource)
                        {
                            pData->pArr[j] = pParam->ppTextures[j]->pStencilTexture;
                        }
                        else if (pParam->mUAVMipSlice)
                        {
                            pData->pArr[j] = pParam->ppTextures[j]->pUAVDescriptors[pParam->mUAVMipSlice];
                        }
                        else
                        {
                            pData->pArr[j] = pParam->ppTextures[j]->pTexture;
                        }
                    }
                }
                break;
            }
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                if (pDesc->mIsArgumentBufferField)
                {
                    if (pParam->mBindMipChain)
                    {
                        for (uint32_t j = 0; j < pParam->ppTextures[0]->mMipLevels; ++j)
                        {
                            const Texture* texture = pParam->ppTextures[0];
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[j]
                                                                 atIndex:pDesc->mHandleIndex + arrayStart + j];
                            TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, texture->pUAVDescriptors[j]);
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            const Texture* texture = pParam->ppTextures[j];
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[pParam->mUAVMipSlice]
                                                                 atIndex:pDesc->mHandleIndex + arrayStart + j];
                            TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, texture->pUAVDescriptors[pParam->mUAVMipSlice]);
                        }
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[pDesc->mHandleIndex];
                    pData->mBinding = pDesc->mReg;
                    pData->mStage = pDesc->mUsedStages;
                    resize_roothandle(pData, arrayCount);
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        pData->pArr[j] = pParam->ppTextures[j]->pUAVDescriptors[pParam->mUAVMipSlice];
                    }
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
                if (pDesc->mIsArgumentBufferField)
                {
                    if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                    {
                        if (pRenderer->pGpu->mSettings.mIndirectCommandBuffer)
                        {
                            BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            Buffer*  buffer = pParam->ppBuffers[j];
                            uint64_t offset = pParam->ppBuffers[j]->mOffset;

                            [pDescriptorSet->mArgumentEncoder
                                setBuffer:buffer->pBuffer
                                   offset:offset + (pParam->pRanges ? pParam->pRanges[j].mOffset : 0)atIndex:pDesc->mHandleIndex +
                                          arrayStart + j];

                            if ((pDesc->mUsage & MTLResourceUsageWrite) || !buffer->pAllocation)
                            {
                                TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, buffer->pBuffer);
                            }
                        }

                        if (pRenderer->pGpu->mSettings.mIndirectCommandBuffer && pParam->mBindICB)
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
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pBuffers[pDesc->mHandleIndex];
                    pData->mBinding = pDesc->mReg;
                    pData->mStage = pDesc->mUsedStages;
                    resize_roothandle(pData, arrayCount);
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        pData->pArr[j] = pParam->ppBuffers[j]->pBuffer;
                        pData->pOffsets[j] = pParam->ppBuffers[j]->mOffset + (pParam->pRanges ? pParam->pRanges[j].mOffset : 0);
                    }

                    if (pRenderer->pGpu->mSettings.mIndirectCommandBuffer)
                    {
                        if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                        {
                            BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
                        }
                        else if (pParam->mBindICB)
                        {
                            const uint32_t        paramIndex = pParam->mBindByIndex ? pParam->mICBIndex : UINT32_MAX;
                            const DescriptorInfo* pDesc = (paramIndex != UINT32_MAX) ? pRootSignature->pDescriptors + paramIndex
                                                                                     : get_descriptor(pRootSignature, pParam->pICBName);
                            if (pDesc)
                            {
                                BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
                            }
                        }
                    }
                }

                break;
            }
#if defined(MTL_RAYTRACING_AVAILABLE)
            case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
            {
                if (MTL_RAYTRACING_SUPPORTED)
                {
                    if (pDesc->mIsArgumentBufferField)
                    {
                        ASSERT(pParam->ppAccelerationStructures);

                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            ASSERT(pParam->ppAccelerationStructures[j]);
                            extern id<MTLAccelerationStructure> getMTLAccelerationStructure(AccelerationStructure*);
                            id                                  as = getMTLAccelerationStructure(pParam->ppAccelerationStructures[j]);
                            [pDescriptorSet->mArgumentEncoder setAccelerationStructure:as atIndex:pDesc->mHandleIndex + arrayStart + j];

                            TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, as);
                            extern void getMTLAccelerationStructureBottomReferences(
                                AccelerationStructure * pAccelerationStructure, uint32_t * pOutReferenceCount, NOREFS id * *pOutReferences);
                            uint32_t bottomRefCount = 0;
                            NOREFS id* bottomRefs = NULL;
                            // Mark primitive acceleration structures as used since only the instance acceleration structure references
                            // them.
                            getMTLAccelerationStructureBottomReferences(pParam->ppAccelerationStructures[j], &bottomRefCount, &bottomRefs);
                            for (uint32_t ref = 0; ref < bottomRefCount; ++ref)
                            {
                                TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, bottomRefs[ref]);
                            }
                        }
                    }
                    else
                    {
                        ASSERT(arrayCount == 1 && "Array not supported");
                        extern id<MTLAccelerationStructure> getMTLAccelerationStructure(AccelerationStructure*);
                        RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pBuffers[pDesc->mHandleIndex];
                        pData->mAccelerationStructure = true;
                        pData->mBinding = pDesc->mReg;
                        pData->mStage = pDesc->mUsedStages;
                        resize_roothandle(pData, arrayCount);
                        pData->pArr[0] = getMTLAccelerationStructure(pParam->ppAccelerationStructures[0]);

                        extern void getMTLAccelerationStructureBottomReferences(AccelerationStructure * pAccelerationStructure,
                                                                                uint32_t * pOutReferenceCount, NOREFS id * *pOutReferences);
                        uint32_t    bottomRefCount = 0;
                        NOREFS id* bottomRefs = NULL;
                        getMTLAccelerationStructureBottomReferences(pParam->ppAccelerationStructures[0], &bottomRefCount, &bottomRefs);
                        for (uint32_t ref = 0; ref < bottomRefCount; ++ref)
                        {
                            TrackUntrackedResource(pDescriptorSet, index, pDesc->mUsage, bottomRefs[ref]);
                        }
                    }
                }
                break;
            }
#endif
            default:
            {
                ASSERT(0); // unsupported descriptor type
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
static void internal_log(LogLevel level, const char* msg, const char* component) { LOGF(level, "%s ( %s )", component, msg); }

// Resource allocation statistics.
void mtl_calculateMemoryStats(Renderer* pRenderer, char** ppStats) { vmaBuildStatsString(pRenderer->pVmaAllocator, ppStats, VK_TRUE); }

void mtl_calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    VmaTotalStatistics stats;
    vmaCalculateStatistics(pRenderer->pVmaAllocator, &stats);
    *usedBytes = stats.total.statistics.allocationBytes;
    *totalAllocatedBytes = stats.total.statistics.blockBytes;
}

void        mtl_freeMemoryStats(Renderer* pRenderer, char* pStats) { vmaFreeStatsString(pRenderer->pVmaAllocator, pStats); }
/************************************************************************/
// Shader utility functions
/************************************************************************/
static void util_specialize_function(id<MTLLibrary> lib, const ShaderConstant* pConstants, uint32_t count, id<MTLFunction>* inOutFunction)
{
    id<MTLFunction>                function = *inOutFunction;
    MTLFunctionConstantValues*     values = [[MTLFunctionConstantValues alloc] init];
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
MTLColorWriteMask util_to_color_mask(ColorMask colorMask)
{
    MTLColorWriteMask mtlMask =
        ((colorMask & COLOR_MASK_RED) ? MTLColorWriteMaskRed : 0u) | ((colorMask & COLOR_MASK_GREEN) ? MTLColorWriteMaskGreen : 0u) |
        ((colorMask & COLOR_MASK_BLUE) ? MTLColorWriteMaskBlue : 0u) | ((colorMask & COLOR_MASK_ALPHA) ? MTLColorWriteMaskAlpha : 0u);
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
            bool blendEnable = (gMtlBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]] != MTLBlendFactorOne ||
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
            attachments[i].writeMask = util_to_color_mask(pDesc->mColorWriteMasks[blendDescIndex]);
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
    if (pDesc->mStencilTest)
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
static void AddFillBufferPipeline(Renderer* pRenderer)
{
    NSError*  error = nil;
    NSString* fillBuffer = @"#include <metal_stdlib>\n"
                            "using namespace metal;\n"
                            "kernel void cs_fill_buffer(device uint32_t* dstBuffer [[buffer(0)]], constant uint2& valueCount "
                            "[[buffer(1)]], uint dtid [[thread_position_in_grid]])\n"
                            "{"
                            "if (dtid >= valueCount.y) return;"
                            "dstBuffer[dtid] = valueCount.x;"
                            "}";

    id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithSource:fillBuffer options:nil error:&error];
    if (error != nil)
    {
        LOGF(LogLevel::eWARNING, "Could not create library for fillBuffer compute shader: %s", [error.description UTF8String]);
        return;
    }

    // Load the kernel function from the library
    id<MTLFunction> func = [lib newFunctionWithName:@"cs_fill_buffer"];
    // Create the compute pipeline state
    pRenderer->pFillBufferPipeline = [pRenderer->pDevice newComputePipelineStateWithFunction:func error:&error];
    if (error != nil)
    {
        LOGF(LogLevel::eWARNING, "Could not create compute pipeline state for simple compute shader: %s", [error.description UTF8String]);
        return;
    }

    lib = nil;
    func = nil;
}

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
    texture1DDesc.mWidth = 4;
    texture1DDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    texture1DDesc.pName = "DefaultTexture_1D";
    addTexture(pRenderer, &texture1DDesc, &pDefaultTextures[TEXTURE_DIM_1D]);

    TextureDesc texture1DArrayDesc = texture1DDesc;
    texture1DArrayDesc.mArraySize = 2;
    texture1DArrayDesc.pName = "DefaultTexture_1D_ARRAY";
    addTexture(pRenderer, &texture1DArrayDesc, &pDefaultTextures[TEXTURE_DIM_1D_ARRAY]);

    TextureDesc texture2DDesc = texture1DDesc;
    texture2DDesc.mHeight = 4;
    texture2DDesc.pName = "DefaultTexture_2D";
    addTexture(pRenderer, &texture2DDesc, &pDefaultTextures[TEXTURE_DIM_2D]);

    TextureDesc texture2DMSDesc = texture2DDesc;
    texture2DMSDesc.mSampleCount = SAMPLE_COUNT_2;
    texture2DMSDesc.pName = "DefaultTexture_2DMS";
    addTexture(pRenderer, &texture2DMSDesc, &pDefaultTextures[TEXTURE_DIM_2DMS]);

    TextureDesc texture2DArrayDesc = texture2DDesc;
    texture2DArrayDesc.mArraySize = 2;
    texture2DArrayDesc.pName = "DefaultTexture_2D_ARRAY";
    addTexture(pRenderer, &texture2DArrayDesc, &pDefaultTextures[TEXTURE_DIM_2D_ARRAY]);

    TextureDesc texture2DMSArrayDesc = texture2DMSDesc;
    texture2DMSArrayDesc.mSampleCount = SAMPLE_COUNT_2;
    texture2DMSArrayDesc.pName = "DefaultTexture_2DMS_ARRAY";
    addTexture(pRenderer, &texture2DMSArrayDesc, &pDefaultTextures[TEXTURE_DIM_2DMS_ARRAY]);

    TextureDesc texture3DDesc = texture2DDesc;
    texture3DDesc.mDepth = 4;
    texture3DDesc.pName = "DefaultTexture_3D";
    addTexture(pRenderer, &texture3DDesc, &pDefaultTextures[TEXTURE_DIM_3D]);

    TextureDesc textureCubeDesc = texture2DDesc;
    textureCubeDesc.mArraySize = 6;
    textureCubeDesc.mDescriptors |= DESCRIPTOR_TYPE_TEXTURE_CUBE;
    textureCubeDesc.pName = "DefaultTexture_CUBE";
    addTexture(pRenderer, &textureCubeDesc, &pDefaultTextures[TEXTURE_DIM_CUBE]);

    TextureDesc textureCubeArrayDesc = textureCubeDesc;
    textureCubeArrayDesc.mArraySize *= 2;
    textureCubeArrayDesc.pName = "DefaultTexture_CUBE_ARRAY";
    if (pRenderer->pGpu->mSettings.mCubeMapTextureArraySupported)
    {
        addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultTextures[TEXTURE_DIM_CUBE_ARRAY]);
    }

    BufferDesc bufferDesc = {};
    bufferDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferDesc.mStartState = RESOURCE_STATE_COMMON;
    bufferDesc.mSize = sizeof(uint32_t);
    bufferDesc.mFirstElement = 0;
    bufferDesc.mElementCount = 1;
    bufferDesc.mStructStride = sizeof(uint32_t);
    bufferDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    bufferDesc.pName = "Default buffer";
    addBuffer(pRenderer, &bufferDesc, &pDefaultBuffer);

    bufferDesc.mICBMaxCommandCount = 1;
    bufferDesc.pName = "Default ICB";
    addBuffer(pRenderer, &bufferDesc, &pDefaultICB);

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

    AddFillBufferPipeline(pRenderer);
}

void remove_default_resources(Renderer* pRenderer)
{
    for (uint32_t i = 0; i < TEXTURE_DIM_COUNT; ++i)
    {
        if (pDefaultTextures[i])
        {
            removeTexture(pRenderer, pDefaultTextures[i]);
        }
    }

    removeBuffer(pRenderer, pDefaultBuffer);
    removeBuffer(pRenderer, pDefaultICB);
    removeSampler(pRenderer, pDefaultSampler);

    pDefaultDepthState = nil;

    pRenderer->pFillBufferPipeline = nil;
}

// -------------------------------------------------------------------------------------------------
// API functions
// -------------------------------------------------------------------------------------------------

TinyImageFormat mtl_getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
{
    if (COLOR_SPACE_SDR_SRGB == colorSpace)
        return TinyImageFormat_B8G8R8A8_SRGB;
    if (COLOR_SPACE_SDR_LINEAR == colorSpace)
        return TinyImageFormat_B8G8R8A8_UNORM;

    bool outputSupportsHDR = false;
#ifdef TARGET_IOS
    if (MTL_HDR_SUPPORTED)
    {
        outputSupportsHDR = true;
    }
#else
    outputSupportsHDR = [[NSScreen mainScreen] maximumPotentialExtendedDynamicRangeColorComponentValue] > 1.0f;
#endif

    if (outputSupportsHDR)
    {
        if (COLOR_SPACE_P2020 == colorSpace)
            return TinyImageFormat_R10G10B10A2_UNORM;
        if (COLOR_SPACE_EXTENDED_SRGB == colorSpace)
            return TinyImageFormat_R16G16B16A16_SFLOAT;
    }

    return TinyImageFormat_UNDEFINED;
}

uint32_t mtl_getRecommendedSwapchainImageCount(Renderer*, const WindowHandle*) { return 3; }

#if !defined(TARGET_IOS)
static uint32_t GetEntryProperty(io_registry_entry_t entry, CFStringRef propertyName)
{
    uint32_t  value = 0;
    CFTypeRef cfProp = IORegistryEntrySearchCFProperty(entry, kIOServicePlane, propertyName, kCFAllocatorDefault,
                                                       kIORegistryIterateRecursively | kIORegistryIterateParents);
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

#define VENDOR_ID_APPLE 0x106b

bool FillGPUVendorPreset(id<MTLDevice> gpu, GPUVendorPreset& gpuVendor)
{
    strncpy(gpuVendor.mGpuName, [gpu.name UTF8String], MAX_GPU_VENDOR_STRING_LENGTH);

    uint32_t  familyTier = getFamilyTier(gpu);
    NSString* version = [[NSProcessInfo processInfo] operatingSystemVersionString];
    snprintf(gpuVendor.mGpuDriverVersion, MAX_GPU_VENDOR_STRING_LENGTH, "GpuFamily: %d OSVersion: %s", familyTier, version.UTF8String);

    gpuVendor.mModelId = 0x0;

    // No vendor id, device id for Apple GPUs
    if (strstr(gpuVendor.mGpuName, "Apple"))
    {
        strncpy(gpuVendor.mVendorName, "apple", MAX_GPU_VENDOR_STRING_LENGTH);
        extern uint32_t getGPUModelID(const char* modelName);
        gpuVendor.mModelId = getGPUModelID(gpuVendor.mGpuName);
        gpuVendor.mVendorId = VENDOR_ID_APPLE;
    }
    else
    {
#if defined(TARGET_IOS)
        ASSERTFAIL("Non-Apple GPU not supported");
#else
        io_registry_entry_t entry;
        uint64_t            regID = 0;
        regID = [gpu registryID];
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
                    gpuVendor.mVendorId = vendorID;
                    gpuVendor.mModelId = deviceID;
                    IOObjectRelease(parent);
                }
                IOObjectRelease(entry);
            }
        }

        strncpy(gpuVendor.mVendorName, getGPUVendorName(gpuVendor.mVendorId), MAX_GPU_VENDOR_STRING_LENGTH);
#endif
    }

    gpuVendor.mPresetLevel = getGPUPresetLevel(gpuVendor.mVendorId, gpuVendor.mModelId, gpuVendor.mVendorName, gpuVendor.mGpuName);
    return true;
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
        LOGF(LogLevel::eWARNING, "Could not create library for simple compute shader: %s", [error.description UTF8String]);
        return 0;
    }

    // Load the kernel function from the library
    id<MTLFunction> kernelFunction = [defaultLibrary newFunctionWithName:@"simplest"];

    // Create a compute pipeline state
    id<MTLComputePipelineState> computePipelineState = [gpu newComputePipelineStateWithFunction:kernelFunction error:&error];
    if (error != nil)
    {
        LOGF(LogLevel::eWARNING, "Could not create compute pipeline state for simple compute shader: %s", [error.description UTF8String]);
        return 0;
    }

    return (uint32_t)computePipelineState.threadExecutionWidth;
}

static MTLResourceOptions gMemoryOptions[VK_MAX_MEMORY_TYPES] = {
    MTLResourceStorageModePrivate,
    MTLResourceStorageModePrivate,
    MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined,
    MTLResourceStorageModeShared | MTLResourceCPUCacheModeDefaultCache,
};

static MTLCPUCacheMode gMemoryCacheModes[VK_MAX_MEMORY_TYPES] = {
    MTLCPUCacheModeDefaultCache,
    MTLCPUCacheModeDefaultCache,
    MTLCPUCacheModeWriteCombined,
    MTLCPUCacheModeDefaultCache,
};

static MTLStorageMode gMemoryStorageModes[VK_MAX_MEMORY_TYPES] = {
    MTLStorageModePrivate,
    MTLStorageModePrivate,
    MTLStorageModeShared,
    MTLStorageModeShared,
};

void vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties)
{
    pProperties->limits.bufferImageGranularity = 64;
}

void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    pMemoryProperties->memoryHeapCount = VK_MAX_MEMORY_HEAPS;
    pMemoryProperties->memoryTypeCount = VK_MAX_MEMORY_TYPES;

    constexpr uint32_t sharedHeapIndex = VK_MAX_MEMORY_HEAPS - 1;

    pMemoryProperties->memoryHeaps[0].size = physicalDevice->pGpu->mSettings.mVRAM;
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

VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator,
                          VkDeviceMemory* pMemory)
{
    MTLResourceOptions resourceOptions = gMemoryOptions[pAllocateInfo->memoryTypeIndex];
    MTLCPUCacheMode    cacheMode = gMemoryCacheModes[pAllocateInfo->memoryTypeIndex];
    MTLStorageMode     storageMode = gMemoryStorageModes[pAllocateInfo->memoryTypeIndex];
    MTLHeapDescriptor* heapDesc = [[MTLHeapDescriptor alloc] init];
    [heapDesc setSize:pAllocateInfo->allocationSize];

    UNREF_PARAM(cacheMode);
    UNREF_PARAM(storageMode);

    if (device->pGpu->mSettings.mPlacementHeaps)
    {
        [heapDesc setType:MTLHeapTypePlacement];
    }

    [heapDesc setResourceOptions:resourceOptions];

    // We cannot create heap with MTLStorageModeShared in macOS
    // Instead we allocate a big buffer which acts as our heap
    // and create suballocations out of it
#if !defined(TARGET_APPLE_ARM64)
    if (MTLStorageModeShared != storageMode)
#endif
    {
        VkDeviceMemory_T* memory =
            (VkDeviceMemory_T*)pAllocator->pfnAllocation(NULL, sizeof(VkDeviceMemory_T), alignof(VkDeviceMemory_T), 0);
        memset(memory, 0, sizeof(VkDeviceMemory_T));
        memory->pHeap = [device->pDevice newHeapWithDescriptor:heapDesc];

#if defined(ENABLE_GRAPHICS_DEBUG)
        switch (pAllocateInfo->memoryTypeIndex)
        {
        case MEMORY_TYPE_GPU_ONLY:
            [memory->pHeap setLabel:@"MTLHEAP_GPU_ONLY"];
            break;
        case MEMORY_TYPE_GPU_ONLY_COLOR_RTS:
            [memory->pHeap setLabel:@"MTLHEAP_GPU_ONLY_COLOR_RTS"];
            break;
        case MEMORY_TYPE_CPU_TO_GPU:
            [memory->pHeap setLabel:@"MTLHEAP_CPU_TO_GPU"];
            break;
        case MEMORY_TYPE_GPU_TO_CPU:
            [memory->pHeap setLabel:@"MTLHEAP_GPU_TO_CPU"];
            break;
        default:
            break;
        }
#endif

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
                device->pHeaps = (NOREFS id<MTLHeap>*)pAllocator->pfnReallocation(
                    NULL, device->pHeaps, device->mHeapCapacity * sizeof(id<MTLHeap>), alignof(id<MTLHeap>), 0);
            }

            device->pHeaps[device->mHeapCount++] = memory->pHeap;
        }
    }

#if !defined(TARGET_APPLE_ARM64)
    if (!(*pMemory) && MTLStorageModeShared == storageMode)
    {
        VkDeviceMemory_T* memory =
            (VkDeviceMemory_T*)pAllocator->pfnAllocation(NULL, sizeof(VkDeviceMemory_T), alignof(VkDeviceMemory_T), 0);
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

// Stub functions to prevent VMA from asserting during runtime
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) { return nullptr; }
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice instance, const char* pName) { return nullptr; }
VkResult vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
{
    return VK_SUCCESS;
}
void     vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {}
VkResult vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    return VK_SUCCESS;
}
VkResult vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges)
{
    return VK_SUCCESS;
}
VkResult vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return VK_SUCCESS; }
VkResult vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return VK_SUCCESS; }
void     vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {}
void     vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {}
VkResult vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer)
{
    return VK_SUCCESS;
}
void     vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {}
VkResult vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage)
{
    return VK_SUCCESS;
}
void vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {}
void vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                     const VkBufferCopy* pRegions)
{
}

static VmaAllocationInfo util_render_alloc(Renderer* pRenderer, ResourceMemoryUsage memUsage, MemoryType memType, NSUInteger size,
                                           NSUInteger align, VmaAllocation* pAlloc)
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
    const uint32_t NEW_BLOCK_SIZE_SHIFT_MAX = 3;

    const VkDeviceSize maxExistingBlockSize = 0;
    for (uint32_t i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
    {
        const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
        if (smallerNewBlockSize > maxExistingBlockSize && smallerNewBlockSize >= sizeAlign.size * 2)
        {
            newBlockSize = smallerNewBlockSize;
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
    VkResult res = vkAllocateMemory(pRenderer, &allocInfo, &gMtlAllocationCallbacks, &heap);
    gMtlAllocationCallbacks.pfnFree(NULL, heap);
    UNREF_PARAM(res);
    ASSERT(VK_SUCCESS == res);

    return index;
}

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
        LOGF(LogLevel::eERROR, "Failed to fetch vm statistics");
        return UINT64_MAX;
    }

    return vmStat.free_count * pageSize;
}
#endif

static void QueryGPUSettings(GpuInfo* gpuInfo, GPUSettings* pOutSettings)
{
    id<MTLDevice> gpu = gpuInfo->pGPU;

    setDefaultGPUSettings(pOutSettings);
    GPUVendorPreset& gpuVendor = pOutSettings->mGpuVendorPreset;
    FillGPUVendorPreset(gpu, gpuVendor);

#ifdef TARGET_IOS
#if defined(ENABLE_OS_PROC_MEMORY)
    pOutSettings->mVRAM = os_proc_available_memory();
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
    pOutSettings->mVRAM = [gpu recommendedMaxWorkingSetSize];
#endif
    ASSERT(pOutSettings->mVRAM);

    pOutSettings->mUniformBufferAlignment = 256;
    pOutSettings->mUploadBufferTextureAlignment = 16;
    pOutSettings->mUploadBufferTextureRowAlignment = 1;
    pOutSettings->mMaxVertexInputBindings =
        MAX_VERTEX_BINDINGS;                  // there are no special vertex buffers for input in Metal, only regular buffers
    pOutSettings->mMultiDrawIndirect = false; // multi draw indirect is not supported on Metal: only single draw indirect
    pOutSettings->mIndirectRootConstant = false;
    pOutSettings->mBuiltinDrawID = false;
    if (MTL_HDR_SUPPORTED)
    {
        pOutSettings->mHDRSupported = true;
    }
#if defined(TARGET_IOS)
    pOutSettings->mMaxBoundTextures = 31;
    pOutSettings->mDrawIndexVertexOffsetSupported = false;
    BOOL isiOSAppOnMac = [NSProcessInfo processInfo].isiOSAppOnMac;
    pOutSettings->mHeaps = !isiOSAppOnMac;
    pOutSettings->mPlacementHeaps = !isiOSAppOnMac;
#else
    pOutSettings->mIsHeadLess = [gpu isHeadless];
    pOutSettings->mMaxBoundTextures = 128;
    pOutSettings->mHeaps = [gpu isLowPower] ? 0 : 1;
#endif

    pOutSettings->mAllowBufferTextureInSameHeap = true;
    pOutSettings->mTimestampQueries = true;
    pOutSettings->mOcclusionQueries = false;
    pOutSettings->mPipelineStatsQueries = false;
    pOutSettings->mGpuMarkers = true;

    const MTLSize maxThreadsPerThreadgroup = [gpu maxThreadsPerThreadgroup];
    pOutSettings->mMaxTotalComputeThreads = (uint32_t)maxThreadsPerThreadgroup.width;
    pOutSettings->mMaxComputeThreads[0] = (uint32_t)maxThreadsPerThreadgroup.width;
    pOutSettings->mMaxComputeThreads[1] = (uint32_t)maxThreadsPerThreadgroup.height;
    pOutSettings->mMaxComputeThreads[2] = (uint32_t)maxThreadsPerThreadgroup.depth;

    // Features
    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    pOutSettings->mROVsSupported = [gpu areRasterOrderGroupsSupported];
    pOutSettings->mTessellationSupported = true;
    pOutSettings->mGeometryShaderSupported = false;
    pOutSettings->mWaveLaneCount = queryThreadExecutionWidth(gpu);

    // Note: With enabled Shader Validation we are getting no performance benefit for stencil samples other than
    // first even if stencil test for those samples was failed
    pOutSettings->mSoftwareVRSSupported = [gpu areProgrammableSamplePositionsSupported];

    // Wave ops crash the compiler if not supported by gpu
    pOutSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
    pOutSettings->mWaveOpsSupportedStageFlags = SHADER_STAGE_NONE;
    pOutSettings->mIndirectCommandBuffer = false;
    pOutSettings->mTessellationIndirectDrawSupported = false;
    pOutSettings->mCubeMapTextureArraySupported = false;
    pOutSettings->mPrimitiveIdSupported = false;
    MTLGPUFamily highestAppleFamily = MTLGPUFamilyApple1;
    int          currentFamily = HIGHEST_GPU_FAMILY;
    for (; currentFamily >= (int)MTLGPUFamilyApple1; currentFamily--)
    {
        if ([gpu supportsFamily:(MTLGPUFamily)currentFamily])
        {
            highestAppleFamily = (MTLGPUFamily)currentFamily;
            break;
        }
    }

    if (highestAppleFamily >= MTLGPUFamilyApple1)
    {
    }
    if (highestAppleFamily >= MTLGPUFamilyApple2)
    {
    }
    if (highestAppleFamily >= MTLGPUFamilyApple3)
    {
        pOutSettings->mIndirectCommandBuffer = true;
#ifdef TARGET_IOS
        pOutSettings->mDrawIndexVertexOffsetSupported = true;
#endif
    }
    if (highestAppleFamily >= MTLGPUFamilyApple4)
    {
        pOutSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
        pOutSettings->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG;
        pOutSettings->mMaxBoundTextures = 96;
        pOutSettings->mCubeMapTextureArraySupported = true;
    }
    if (highestAppleFamily >= MTLGPUFamilyApple5)
    {
        pOutSettings->mTessellationIndirectDrawSupported = true;
    }
#ifdef ENABLE_GPU_FAMILY_6
    if (highestAppleFamily >= MTLGPUFamilyApple6)
    {
        pOutSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BASIC_BIT | WAVE_OPS_SUPPORT_FLAG_VOTE_BIT |
                                              WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT | WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT |
                                              WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
        pOutSettings->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
    }
#endif // ENABLE_GPU_FAMILY_6
#ifdef ENABLE_GPU_FAMILY_7
    if (highestAppleFamily >= MTLGPUFamilyApple7)
    {
        pOutSettings->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
        pOutSettings->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
        pOutSettings->mPrimitiveIdSupported = true;
    }
#endif // ENABLE_GPU_FAMILY_7
#ifdef ENABLE_GPU_FAMILY_8
    if (highestAppleFamily >= MTLGPUFamilyApple8)
    {
    }
#endif // ENABLE_GPU_FAMILY_8
    if ([gpu supportsFamily:MTLGPUFamilyMac2])
    {
        pOutSettings->mIndirectCommandBuffer = true;
        pOutSettings->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
        pOutSettings->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
        pOutSettings->mTessellationIndirectDrawSupported = true;
        pOutSettings->mPlacementHeaps = pOutSettings->mHeaps ? 1 : 0;
        pOutSettings->mCubeMapTextureArraySupported = true;
        pOutSettings->mPrimitiveIdSupported = true;
    }

    MTLArgumentBuffersTier abTier = gpu.argumentBuffersSupport;

    if (abTier == MTLArgumentBuffersTier2)
    {
        pOutSettings->mMaxBoundTextures = 1000000;
    }

#if defined(MTL_RAYTRACING_AVAILABLE)
    if (MTL_RAYTRACING_SUPPORTED)
    {
        pOutSettings->mRayQuerySupported = [gpu supportsRaytracing];
        pOutSettings->mRayPipelineSupported = false;
        pOutSettings->mRaytracingSupported = pOutSettings->mRayQuerySupported || pOutSettings->mRayPipelineSupported;
    }
#endif

#ifdef ENABLE_GPU_FAMILY_9
    pOutSettings->m64BitAtomicsSupported = [gpuInfo->pGPU supportsFamily:MTLGPUFamilyApple9];
#endif // ENABLE_GPU_FAMILY_9
#ifdef ENABLE_GPU_FAMILY_8
    if ([gpuInfo->pGPU supportsFamily:MTLGPUFamilyApple8] && [gpuInfo->pGPU supportsFamily:MTLGPUFamilyMac2])
    {
        pOutSettings->m64BitAtomicsSupported = true;
    }
#endif // ENABLE_GPU_FAMILY_8

    // Get the supported counter set.
    // We only support timestamps at stage boundary..
    gpuInfo->pCounterSetTimestamp = util_get_counterset(gpu);
    gpuInfo->mCounterTimestampEnabled = gpuInfo->pCounterSetTimestamp != nil;
}

static bool SelectBestGpu(const RendererDesc* settings, Renderer* pRenderer)
{
    RendererContext* pContext = pRenderer->pContext;
    const GpuInfo*   gpus = pContext->mGpus;
    uint32_t         gpuCount = pContext->mGpuCount;
    uint32_t         gpuIndex = settings->pContext ? settings->mGpuIndex : UINT32_MAX;

    GPUSettings gpuSettings[MAX_MULTIPLE_GPUS] = {};
    for (uint32_t i = 0; i < gpuCount; i++)
    {
        gpuSettings[i] = gpus[i].mSettings;
    }

#ifdef TARGET_IOS
    gpuIndex = 0;
#else
    if (gpuCount < 1)
    {
        LOGF(LogLevel::eERROR, "Failed to enumerate any physical Metal devices");
        return false;
    }
#endif

#ifndef TARGET_IOS
#if defined(AUTOMATED_TESTING) && defined(ACTIVE_TESTING_GPU)
    selectActiveGpu(gpuSettings, &gpuIndex, gpuCount);
#else
    gpuIndex = util_select_best_gpu(gpuSettings, pContext->mGpuCount);
#endif
#endif

    // no proper driver support on darwin, currently returning: GpuFamily: %d OSVersion: %s
    // bool driverValid = checkDriverRejectionSettings(&gpuSettings[gpuIndex]);
    // if (!driverValid)
    //{
    //	setRendererInitializationError("Driver rejection return invalid result.\nPlease, update your driver to the latest version.");
    //	return false;
    //}

    ASSERT(gpuIndex < pContext->mGpuCount);
    pRenderer->pDevice = gpus[gpuIndex].pGPU;
    pRenderer->pGpu = &gpus[gpuIndex];
    pRenderer->mLinkedNodeCount = 1;

    LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
    LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pGpu->mSettings.mGpuVendorPreset.mGpuName);
    LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->mSettings.mGpuVendorPreset.mVendorId);
    LOGF(LogLevel::eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->mSettings.mGpuVendorPreset.mModelId);
    LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel));

    MTLArgumentBuffersTier abTier = pRenderer->pDevice.argumentBuffersSupport;
    LOGF(LogLevel::eINFO, "Metal: Argument Buffer Tier: %lu", abTier);
    LOGF(LogLevel::eINFO, "Metal: Max Arg Buffer Textures: %u", pRenderer->pGpu->mSettings.mMaxBoundTextures);

    return true;
}

void mtl_initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
{
    ASSERT(appName);
    ASSERT(pDesc);
    ASSERT(ppContext);

#ifdef TARGET_IOS
    id<MTLDevice> gpus[1] = { MTLCreateSystemDefaultDevice() };
    uint32_t      gpuCount = 1;
#else
    NSArray<id<MTLDevice>>* gpus = MTLCopyAllDevices();
    uint32_t                gpuCount = min((uint32_t)MAX_MULTIPLE_GPUS, (uint32_t)[gpus count]);
#endif

    if (gpuCount < 1)
    {
        LOGF(LogLevel::eERROR, "Failed to enumerate any physical Metal devices");
        *ppContext = NULL;
        return;
    }

    RendererContext* pContext = (RendererContext*)tf_calloc_memalign(1, alignof(RendererContext), sizeof(RendererContext));

    for (uint32_t i = 0; i < TF_ARRAY_COUNT(pContext->mGpus); ++i)
    {
        setDefaultGPUSettings(&pContext->mGpus[i].mSettings);
    }

    pContext->mGpuCount = gpuCount;
    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        pContext->mGpus[i].pGPU = gpus[i];
        QueryGPUSettings(&pContext->mGpus[i], &pContext->mGpus[i].mSettings);
        mtlCapsBuilder(pContext->mGpus[i].pGPU, &pContext->mGpus[i].mCapBits);
        applyGPUConfigurationRules(&pContext->mGpus[i].mSettings, &pContext->mGpus[i].mCapBits);

        LOGF(LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %#x, Model ID: %#x, Preset: %s, GPU Name: %s", i,
             pContext->mGpus[i].mSettings.mGpuVendorPreset.mVendorId, pContext->mGpus[i].mSettings.mGpuVendorPreset.mModelId,
             presetLevelToString(pContext->mGpus[i].mSettings.mGpuVendorPreset.mPresetLevel),
             pContext->mGpus[i].mSettings.mGpuVendorPreset.mGpuName);
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

    pRenderer->pName = appName;
    pRenderer->mRendererApi = RENDERER_API_METAL;
    pRenderer->mGpuMode = settings->mGpuMode;
    pRenderer->mShaderTarget = settings->mShaderTarget;

    if (settings->pContext)
    {
        pRenderer->pContext = settings->pContext;
        pRenderer->mOwnsContext = false;
    }
    else
    {
        RendererContextDesc contextDesc = {};
        contextDesc.mEnableGpuBasedValidation = settings->mEnableGpuBasedValidation;
        mtl_initRendererContext(appName, &contextDesc, &pRenderer->pContext);
        pRenderer->mOwnsContext = true;
    }

    // Initialize the Metal bits
    {
        SelectBestGpu(settings, pRenderer);

        LOGF(LogLevel::eINFO, "Metal: Heaps: %s", pRenderer->pGpu->mSettings.mHeaps ? "true" : "false");
        LOGF(LogLevel::eINFO, "Metal: Placement Heaps: %s", pRenderer->pGpu->mSettings.mPlacementHeaps ? "true" : "false");

        if (!pRenderer->pGpu->mSettings.mPlacementHeaps)
        {
            pRenderer->pHeapMutex = (Mutex*)tf_malloc(sizeof(Mutex));
            initMutex(pRenderer->pHeapMutex);
        }

        // exit app if gpu being used has an office preset.
        if (pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel < GPU_PRESET_VERYLOW)
        {
            ASSERT(pRenderer->pGpu->mSettings.mGpuVendorPreset.mPresetLevel >= GPU_PRESET_VERYLOW);

            // set device to null
            pRenderer->pDevice = nil;
            // remove allocated renderer
            SAFE_FREE(pRenderer);

            LOGF(LogLevel::eERROR, "Selected GPU has an office Preset in gpu.cfg");
            LOGF(LogLevel::eERROR, "Office Preset is not supported by the Forge");

            *ppRenderer = NULL;
#ifdef AUTOMATED_TESTING
            // exit with success return code not to show failure on Jenkins
            exit(0);
#endif
            return;
        }

        // Create allocator
        if (pRenderer->pGpu->mSettings.mPlacementHeaps)
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

        LOGF(LogLevel::eINFO, "Renderer: VMA Allocator: %s", pRenderer->pVmaAllocator ? "true" : "false");

        // Create default resources.
        add_default_resources(pRenderer);

        // Renderer is good! Assign it to result!
        *(ppRenderer) = pRenderer;
    }

    if (IOS14_RUNTIME)
    {
#if defined(ENABLE_GRAPHICS_DEBUG)
        pRenderer->pContext->mMtl.mExtendedEncoderDebugReport = true;
#else
        pRenderer->pContext->mMtl.mExtendedEncoderDebugReport = settings->mEnableGpuBasedValidation;
#endif
    }
}

void mtl_exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    remove_default_resources(pRenderer);

    if (pRenderer->pGpu->mSettings.mPlacementHeaps)
    {
        vmaDestroyAllocator(pRenderer->pVmaAllocator);
    }

    pRenderer->pDevice = nil;

    if (pRenderer->pHeapMutex)
    {
        destroyMutex(pRenderer->pHeapMutex);
    }

    SAFE_FREE(pRenderer->pHeapMutex);
    SAFE_FREE(pRenderer->pHeaps);

    if (pRenderer->mOwnsContext)
    {
        mtl_exitRendererContext(pRenderer->pContext);
    }

    SAFE_FREE(pRenderer);
}

void mtl_addFence(Renderer* pRenderer, Fence** ppFence)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->pDevice != nil);
    ASSERT(ppFence);

    Fence* pFence = (Fence*)tf_calloc(1, sizeof(Fence));
    ASSERT(pFence);

    pFence->pSemaphore = dispatch_semaphore_create(0);
    pFence->mSubmitted = false;

    *ppFence = pFence;
}

void mtl_removeFence(Renderer* pRenderer, Fence* pFence)
{
    ASSERT(pFence);
    pFence->pSemaphore = nil;

    SAFE_FREE(pFence);
}

void mtl_addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
    ASSERT(pRenderer);
    ASSERT(ppSemaphore);

    Semaphore* pSemaphore = (Semaphore*)tf_calloc(1, sizeof(Semaphore));
    ASSERT(pSemaphore);

    pSemaphore->pSemaphore = [pRenderer->pDevice newEvent];
    pSemaphore->mValue = 0;
    pSemaphore->mSignaled = false;

    *ppSemaphore = pSemaphore;
}

void mtl_removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    ASSERT(pSemaphore);

    pSemaphore->pSemaphore = nil;

    SAFE_FREE(pSemaphore);
}

void mtl_addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
    ASSERT(pDesc);
    ASSERT(ppQueue);

    Queue* pQueue = (Queue*)tf_calloc(1, sizeof(Queue));
    ASSERT(pQueue);

    const char* queueNames[] = {
        "GRAPHICS_QUEUE",
        "TRANSFER_QUEUE",
        "COMPUTE_QUEUE",
    };
    COMPILE_ASSERT(TF_ARRAY_COUNT(queueNames) == MAX_QUEUE_TYPE);

    pQueue->mNodeIndex = pDesc->mNodeIndex;
    pQueue->mType = pDesc->mType;
    pQueue->pCommandQueue = [pRenderer->pDevice newCommandQueueWithMaxCommandBufferCount:512];
    [pQueue->pCommandQueue setLabel:[NSString stringWithUTF8String:(pDesc->pName ? pDesc->pName : queueNames[pDesc->mType])]];

    pQueue->mBarrierFlags = 0;
    pQueue->pQueueFence = [pRenderer->pDevice newFence];
    ASSERT(pQueue->pCommandQueue != nil);

    *ppQueue = pQueue;
}

void mtl_removeQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pQueue);

    pQueue->pCommandQueue = nil;
    pQueue->pQueueFence = nil;

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
    pCmd->pCommandBuffer = nil;

    SAFE_FREE(pCmd);
}

void mtl_addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
    // verify that ***cmd is valid
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(cmdCount);
    ASSERT(pppCmd);

    Cmd** ppCmds = (Cmd**)tf_calloc(cmdCount, sizeof(Cmd*));
    ASSERT(ppCmds);

    // add n new cmds to given pool
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        ::addCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void mtl_removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    // verify that given command list is valid
    ASSERT(ppCmds);

    // remove every given cmd in array
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
    // no need to have vsync on layers otherwise we will wait on semaphores
    // get a copy of the layer for nextDrawables
    CAMetalLayer* layer = (CAMetalLayer*)pSwapchain->pForgeView.layer;

    // only available on mac OS.
    // VSync seems to be necessary on iOS.

    if (!pSwapchain->mEnableVsync)
    {
        layer.displaySyncEnabled = false;
    }
    else
    {
        layer.displaySyncEnabled = true;
    }
#endif
}

const CFStringRef util_mtl_colorspace(ColorSpace colorSpace)
{
    if (MTL_HDR_SUPPORTED)
    {
        if (COLOR_SPACE_P2020 == colorSpace)
            return kCGColorSpaceITUR_2020_PQ;
        if (COLOR_SPACE_EXTENDED_SRGB == colorSpace)
            return kCGColorSpaceExtendedLinearSRGB;
    }
    return kCGColorSpaceSRGB;
}

static void util_set_colorspace(SwapChain* pSwapChain, ColorSpace colorSpace)
{
    if (MTL_HDR_SUPPORTED)
    {
        CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
        layer.wantsExtendedDynamicRangeContent = false;
        layer.colorspace = nil;
        if (COLOR_SPACE_SDR_SRGB < colorSpace)
        {
            layer.wantsExtendedDynamicRangeContent = true;
            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(util_mtl_colorspace(colorSpace));
            layer.colorspace = colorspace;
            CGColorSpaceRelease(colorspace);
        }
    }
}

void mtl_addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppSwapChain);

    LOGF(LogLevel::eINFO, "Adding Metal swapchain @ %ux%u", pDesc->mWidth, pDesc->mHeight);

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
        LOGF(eWARNING,
             "Metal: CAMetalLayer device and Renderer device are not matching. Fixing by assigning Renderer device to CAMetalLayer device");
        layer.device = pRenderer->pDevice;
    }
    util_set_colorspace(pSwapChain, pDesc->mColorSpace);

    // only available on mac OS.
    // VSync seems to be necessary on iOS.
#if defined(ENABLE_DISPLAY_SYNC_TOGGLE)
    if (!pDesc->mEnableVsync)
    {
        // This needs to be set to false to have working non-vsync
        // otherwise present drawables will wait on vsync.
        layer.displaySyncEnabled = false;
    }
    else
    {
        // This needs to be set to false to have working vsync
        layer.displaySyncEnabled = true;
    }
#endif
    // Set the view pixel format to match the swapchain's pixel format.
    layer.pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mColorFormat);
#else
    pSwapChain->pForgeView = (__bridge UIView*)pDesc->mWindowHandle.window;
    pSwapChain->pForgeView.autoresizesSubviews = TRUE;

    CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
    // Set the view pixel format to match the swapchain's pixel format.
    layer.pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mColorFormat);
    util_set_colorspace(pSwapChain, pDesc->mColorSpace);
#endif

    pSwapChain->mMTKDrawable = nil;

    // Create present command buffer for the swapchain.
    pSwapChain->presentCommandBuffer = [pDesc->ppPresentQueues[0]->pCommandQueue commandBuffer];

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
        pSwapChain->ppRenderTargets[i]->pTexture->pTexture = nil;
    }

    pSwapChain->mImageCount = pDesc->mImageCount;
    pSwapChain->mEnableVsync = pDesc->mEnableVsync;
    pSwapChain->mFormat = pDesc->mColorFormat;
    pSwapChain->mColorSpace = pDesc->mColorSpace;

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

void mtl_addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppHeap);

    if (!pRenderer->pGpu->mSettings.mPlacementHeaps)
    {
        return;
    }

    VmaAllocationCreateInfo vma_mem_reqs = {};
    vma_mem_reqs.usage = (VmaMemoryUsage)pDesc->mMemoryUsage;
    vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VkMemoryRequirements vkMemReq = {};
    vkMemReq.size = pDesc->mSize;
    vkMemReq.alignment = pDesc->mAlignment;
    vkMemReq.memoryTypeBits = UINT32_MAX;

    VmaAllocationInfo alloc_info = {};
    VmaAllocation     alloc = {};

    VkResult res = vmaAllocateMemory(pRenderer->pVmaAllocator, &vkMemReq, &vma_mem_reqs, &alloc, &alloc_info);
    ASSERT(VK_SUCCESS == res);
    if (VK_SUCCESS != res)
    {
        LOGF(eERROR, "Failed to allocate memory for ResourceHeap");
        return;
    }

    ASSERT(alloc_info.size >= pDesc->mSize);
    ASSERT(alloc_info.offset == 0);

    ResourceHeap* pHeap = (ResourceHeap*)tf_calloc(1, sizeof(ResourceHeap));
    pHeap->pAllocation = alloc;
    pHeap->pHeap = alloc_info.deviceMemory->pHeap;
    pHeap->mSize = pDesc->mSize;
    *ppHeap = pHeap;
}

void mtl_removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    if (!pRenderer->pGpu->mSettings.mPlacementHeaps)
    {
        return;
    }

    ASSERT(pHeap);

    if (pHeap->pAllocation)
    {
        vmaFreeMemory(pRenderer->pVmaAllocator, pHeap->pAllocation);
    }

    SAFE_FREE(pHeap);
}

void mtl_getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);

    uint64_t allocationSize = pDesc->mSize;
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        const uint64_t minAlignment = pRenderer->pGpu->mSettings.mUniformBufferAlignment;
        allocationSize = round_up_64(allocationSize, minAlignment);
    }

    const MemoryType   memoryType = util_to_memory_type(pDesc->mMemoryUsage);
    MTLResourceOptions resourceOptions = gMemoryOptions[memoryType];
    MTLSizeAndAlign    sizeAlign = [pRenderer->pDevice heapBufferSizeAndAlignWithLength:allocationSize options:resourceOptions];
    pOut->mSize = sizeAlign.size;
    pOut->mAlignment = sizeAlign.align;
}

void mtl_getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);
    const MTLPixelFormat pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mFormat);

    MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];
    initialize_texture_desc(pRenderer, pDesc, false, pixelFormat, textureDesc);

    const MTLSizeAndAlign sizeAlign = [pRenderer->pDevice heapTextureSizeAndAlignWithDescriptor:textureDesc];
    pOut->mSize = sizeAlign.size;
    pOut->mAlignment = sizeAlign.align;
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
    const MemoryType   memoryType = util_to_memory_type(pDesc->mMemoryUsage);
    MTLResourceOptions resourceOptions = gMemoryOptions[memoryType];
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        uint64_t minAlignment = pRenderer->pGpu->mSettings.mUniformBufferAlignment;
        allocationSize = round_up_64(allocationSize, minAlignment);
        ((BufferDesc*)pDesc)->mAlignment = (uint32_t)minAlignment;
    }

    //	//Use isLowPower to determine if running intel integrated gpu
    //	//There's currently an intel driver bug with placed resources so we need to create
    //	//new resources that are GPU only in their own memory space
    //	//0x8086 is intel vendor id
    //	if (pRenderer->pGpu->mSettings.mGpuVendorPreset.mVendorId == GPU_VENDOR_INTEL &&
    //		(ResourceMemoryUsage)pDesc->mMemoryUsage & RESOURCE_MEMORY_USAGE_GPU_ONLY)
    //		((BufferDesc*)pDesc)->mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    // Indirect command buffer does not need backing buffer memory
    // Directly allocated through device
    if (pRenderer->pGpu->mSettings.mIndirectCommandBuffer && (pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER))
    {
        MTLIndirectCommandBufferDescriptor* icbDescriptor = [MTLIndirectCommandBufferDescriptor alloc];

        switch (pDesc->mICBDrawType)
        {
        case INDIRECT_DRAW:
            icbDescriptor.commandTypes = MTLIndirectCommandTypeDraw;
            break;
        case INDIRECT_DRAW_INDEX:
            icbDescriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
            break;
        default:
            ASSERT(false); // unsupported command type
        }

        icbDescriptor.inheritBuffers = true;
        icbDescriptor.inheritPipelineState = true;

        pBuffer->pIndirectCommandBuffer = [pRenderer->pDevice newIndirectCommandBufferWithDescriptor:icbDescriptor
                                                                                     maxCommandCount:pDesc->mICBMaxCommandCount
                                                                                             options:0];
    }

    if (pDesc->pPlacement && pRenderer->pGpu->mSettings.mPlacementHeaps)
    {
        pBuffer->pBuffer = [pDesc->pPlacement->pHeap->pHeap newBufferWithLength:allocationSize
                                                                        options:resourceOptions
                                                                         offset:pDesc->pPlacement->mOffset];
    }

    if (!pBuffer->pBuffer && (!pRenderer->pGpu->mSettings.mHeaps || (RESOURCE_MEMORY_USAGE_GPU_ONLY != pDesc->mMemoryUsage &&
                                                                     RESOURCE_MEMORY_USAGE_CPU_TO_GPU != pDesc->mMemoryUsage)))
    {
        pBuffer->pBuffer = [pRenderer->pDevice newBufferWithLength:allocationSize options:resourceOptions];
    }
    else if (!pBuffer->pBuffer)
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
            if (pRenderer->pGpu->mSettings.mPlacementHeaps)
            {
                VmaAllocationInfo allocInfo =
                    util_render_alloc(pRenderer, pDesc->mMemoryUsage, memoryType, sizeAlign.size, sizeAlign.align, &pBuffer->pAllocation);

                pBuffer->pBuffer = [allocInfo.deviceMemory->pHeap newBufferWithLength:allocationSize
                                                                              options:resourceOptions
                                                                               offset:allocInfo.offset];
                ASSERT(pBuffer->pBuffer);
            }
            else
            {
                // If placement heaps are not supported we cannot use VMA
                // Instead we have to rely on MTLHeap automatic placement
                uint32_t heapIndex = util_find_heap_with_space(pRenderer, memoryType, sizeAlign);

                // Fallback on earlier versions
                pBuffer->pBuffer = [pRenderer->pHeaps[heapIndex] newBufferWithLength:allocationSize options:resourceOptions];
                ASSERT(pBuffer->pBuffer);
            }
        }
        else
        {
            VmaAllocationInfo allocInfo =
                util_render_alloc(pRenderer, pDesc->mMemoryUsage, memoryType, sizeAlign.size, sizeAlign.align, &pBuffer->pAllocation);

            pBuffer->pBuffer = (id<MTLBuffer>)allocInfo.deviceMemory->pHeap;
            pBuffer->mOffset = allocInfo.offset;
        }
    }

    if (pBuffer->pBuffer && !(resourceOptions & MTLResourceStorageModePrivate) && (pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT))
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
        ASSERT(pBuffer->pBuffer);
    }

    pBuffer->pBuffer = nil;
    pBuffer->pIndirectCommandBuffer = nil;

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
        [(id<MPSSVGFTextureAllocator>)pTexture->mpsTextureAllocator returnTexture:pTexture->pTexture];
        pTexture->mpsTextureAllocator = nil;
    }
#endif

    pTexture->pTexture = nil;

    // Destroy descriptors
    if (pTexture->pUAVDescriptors && !TinyImageFormat_HasStencil((TinyImageFormat)pTexture->mFormat))
    {
        for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
        {
            pTexture->pUAVDescriptors[i] = nil;
        }
    }
    else
    {
        pTexture->pStencilTexture = nil;
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
    rtDesc.pPlacement = pDesc->pPlacement;

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
    pRenderTarget->mDescriptors = pDesc->mDescriptors;

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

    // default sampler lod values
    // used if not overriden by mSetLodRange or not Linear mipmaps
    float minSamplerLod = 0;
    float maxSamplerLod = pDesc->mMipMapMode == MIPMAP_MODE_LINEAR ? FLT_MAX : 0;
    // user provided lods
    if (pDesc->mSetLodRange)
    {
        minSamplerLod = pDesc->mMinLod;
        maxSamplerLod = pDesc->mMaxLod;
    }

    MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = (pDesc->mMinFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
    samplerDesc.magFilter = (pDesc->mMagFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
    samplerDesc.mipFilter = (pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear);
    samplerDesc.maxAnisotropy = (pDesc->mMaxAnisotropy == 0 ? 1 : pDesc->mMaxAnisotropy); // 0 is not allowed in Metal

    samplerDesc.lodMinClamp = minSamplerLod;
    samplerDesc.lodMaxClamp = maxSamplerLod;

    const MTLSamplerAddressMode* addressModeTable = NULL;
#if defined(ENABLE_SAMPLER_CLAMP_TO_BORDER)
    addressModeTable = gMtlAddressModeTranslator;
#else
    addressModeTable = gMtlAddressModeTranslatorFallback;
#endif
    samplerDesc.sAddressMode = addressModeTable[pDesc->mAddressU];
    samplerDesc.tAddressMode = addressModeTable[pDesc->mAddressV];
    samplerDesc.rAddressMode = addressModeTable[pDesc->mAddressW];
    samplerDesc.compareFunction = gMtlComparisonFunctionTranslator[pDesc->mCompareFunc];
    samplerDesc.supportArgumentBuffers = YES;

    pSampler->pSamplerState = [pRenderer->pDevice newSamplerStateWithDescriptor:samplerDesc];

    *ppSampler = pSampler;
}

void mtl_removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pSampler);
    pSampler->pSamplerState = nil;
    SAFE_FREE(pSampler);
}

void mtl_addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mStages);
    ASSERT(ppShaderProgram);

    Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
    ASSERT(pShaderProgram);

    pShaderProgram->mStages = pDesc->mStages;
    pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);

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
                compiled_code = &(pShaderProgram->pVertexShader);
            }
            break;
            case SHADER_STAGE_FRAG:
            {
                pStage = &pDesc->mFrag;
                compiled_code = &(pShaderProgram->pFragmentShader);
            }
            break;
            case SHADER_STAGE_COMP:
            {
                pStage = &pDesc->mComp;
                compiled_code = &(pShaderProgram->pComputeShader);
            }
            break;
            default:
                break;
            }

            // Create a MTLLibrary from bytecode.
            dispatch_data_t byteCode =
                dispatch_data_create(pStage->pByteCode, pStage->mByteCodeSize, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithData:byteCode error:nil];
            ASSERT(lib);

            // Create a MTLFunction from the loaded MTLLibrary.
            NSString* entryPointNStr = [lib functionNames][0];
            if (pStage->pEntryPoint)
            {
                entryPointNStr = [[NSString alloc] initWithUTF8String:pStage->pEntryPoint];
            }
            id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
            ASSERT(function);

            if (pDesc->mConstantCount)
            {
                @autoreleasepool
                {
                    util_specialize_function(lib, pDesc->pConstants, pDesc->mConstantCount, &function);
                }
            }

            *compiled_code = function;

            mtl_createShaderReflection(pRenderer, pShaderProgram, stage_mask,
                                       &pShaderProgram->pReflection->mStageReflections[reflectionCount++]);

            if (pShaderProgram->mStages & SHADER_STAGE_FRAG)
            {
                pShaderProgram->pReflection->mStageReflections[reflectionCount].mOutputRenderTargetTypesMask =
                    pDesc->mFrag.mOutputRenderTargetTypesMask;
            }
        }
    }

    createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

    if (pShaderProgram->mStages & SHADER_STAGE_COMP)
    {
        for (size_t i = 0; i < 3; ++i)
        {
            pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[i] = pDesc->mComp.mNumThreadsPerGroup[i];
            pShaderProgram->mNumThreadsPerGroup[i] = pDesc->mComp.mNumThreadsPerGroup[i];
            // ASSERT(pShaderProgram->mNumThreadsPerGroup[i]);
        }
    }

    if (pShaderProgram->pVertexShader)
    {
        pShaderProgram->mTessellation = pShaderProgram->pVertexShader.patchType != MTLPatchTypeNone;
    }

    *ppShaderProgram = pShaderProgram;
}

void mtl_removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    ASSERT(pShaderProgram);

    pShaderProgram->pVertexShader = nil;
    pShaderProgram->pFragmentShader = nil;
    pShaderProgram->pComputeShader = nil;

    destroyPipelineReflection(pShaderProgram->pReflection);
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
        char*    key;
        Sampler* value;
    } SamplerMap;
    SamplerMap*         staticSamplerMap = NULL;
    DescriptorIndexMap* indexMap = NULL;
    ShaderStage         shaderStages = SHADER_STAGE_NONE;
    PipelineType        pipelineType = PIPELINE_TYPE_UNDEFINED;
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
                    ShaderResource*       a = &shaderResources[i];
                    const ShaderResource* b = pRes;
                    // HLSL - Every type has different register type unlike Vulkan where all registers are shared by all types
                    if (a->mIsArgumentBufferField != b->mIsArgumentBufferField)
                    {
                        continue;
                    }
                    else if (!a->mIsArgumentBufferField && !b->mIsArgumentBufferField)
                    {
                        if ((a->type == b->type) && (a->used_stages == b->used_stages) && (((a->reg ^ b->reg) | (a->set ^ b->set)) == 0))
                        {
                            it = a;
                            break;
                        }
                    }
                    else if ((a->type == b->type) && ((((uint64_t)a->mArgumentDescriptor.mArgumentIndex << 32) |
                                                       ((uint64_t)a->mArgumentDescriptor.mBufferIndex & 0xFFFFFFFF)) ==
                                                      (((uint64_t)b->mArgumentDescriptor.mArgumentIndex << 32) |
                                                       ((uint64_t)b->mArgumentDescriptor.mBufferIndex & 0xFFFFFFFF))))
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
                        LOGF(LogLevel::eERROR,
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
                    LOGF(LogLevel::eERROR,
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
                    LOGF(LogLevel::eERROR,
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

            uint32_t                        setIndex = pRes->mIsArgumentBufferField ? pRes->reg : pRes->set;
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
            pDesc->mDim = pRes->dim;
            if (pDesc->mIsArgumentBufferField)
            {
                pDesc->mHandleIndex = pRes->mArgumentDescriptor.mArgumentIndex;
                pDesc->mSize = max(1u, pRes->mArgumentDescriptor.mArrayLength);
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
                case DESCRIPTOR_TYPE_SAMPLER:
                    pDesc->mHandleIndex = pRootSignature->mRootSamplerCounts[updateFreq]++;
                    break;
                case DESCRIPTOR_TYPE_TEXTURE:
                case DESCRIPTOR_TYPE_RW_TEXTURE:
                    pDesc->mHandleIndex = pRootSignature->mRootTextureCounts[updateFreq]++;
                    break;
                    // Everything else is a buffer
                case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
                default:
                    pDesc->mHandleIndex = pRootSignature->mRootBufferCounts[updateFreq]++;
                    break;
                }
            }

            pDesc->pStaticSampler = nil;

            if (MTLArgumentAccessWriteOnly == pRes->mArgumentDescriptor.mAccessType)
            {
                pDesc->mUsage = MTLResourceUsageWrite;
            }
            else if (MTLArgumentAccessReadWrite == pRes->mArgumentDescriptor.mAccessType)
            {
                pDesc->mUsage = MTLResourceUsageRead | MTLResourceUsageWrite;
            }
            else
            {
                pDesc->mUsage = MTLResourceUsageRead;
            }

            // In case we're binding a texture, we need to specify the texture type so the bound resource type matches the one defined in
            // the shader.
            if (pRes->type == DESCRIPTOR_TYPE_TEXTURE || pRes->type == DESCRIPTOR_TYPE_RW_TEXTURE)
            {
                //                    pDesc->mDesc.pTextureType = pRes->pTextureType;
            }

            // static samplers
            if (pRes->type == DESCRIPTOR_TYPE_SAMPLER)
            {
                SamplerMap* pNode = shgetp_null(staticSamplerMap, pRes->name);
                if (pNode != NULL)
                {
                    pDesc->pStaticSampler = pNode->value->pSamplerState;
                }
            }
        }
    }

    NSMutableArray<MTLArgumentDescriptor*>* argumentDescriptors[DESCRIPTOR_UPDATE_FREQ_COUNT] = {};
    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
    {
        argumentDescriptors[i] = [[NSMutableArray alloc] init];
    }

    // Create argument buffer descriptors (update template)
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        const DescriptorInfo& descriptorInfo(pRootSignature->pDescriptors[i]);

        if (descriptorInfo.mIsArgumentBufferField)
        {
            const ArgumentDescriptor& memberDescriptor(shaderResources[i].mArgumentDescriptor);

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

                [argumentDescriptors[updateFreq] addObject:argDescriptor];
            }
        }
    }

    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
    {
        pRootSignature->mArgumentDescriptors[i] = [[argumentDescriptors[i] sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
            MTLArgumentDescriptor* first = a;
            MTLArgumentDescriptor* second = b;
            return (NSComparisonResult)(first.index > second.index);
        }] mutableCopy];
    }

    *ppRootSignature = pRootSignature;
    arrfree(shaderResources);
    shfree(staticSamplerMap);
}

void mtl_removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
    ASSERT(pRenderer);
    ASSERT(pRootSignature);

    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; ++i)
    {
        pRootSignature->mArgumentDescriptors[i] = nil;
    }

    shfree(pRootSignature->pDescriptorNameToIndexMap);
    SAFE_FREE(pRootSignature);
}

uint32_t mtl_getDescriptorIndexFromName(const RootSignature* pRootSignature, const char* pName)
{
    for (uint32_t i = 0; i < pRootSignature->mDescriptorCount; ++i)
    {
        if (!strcmp(pName, pRootSignature->pDescriptors[i].pName))
        {
            return i;
        }
    }

    return UINT32_MAX;
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
    pPipeline->mPatchControlPointCount = (uint32_t)pDesc->pShaderProgram->pVertexShader.patchControlPointCount;

    // create metal pipeline descriptor
    MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    renderPipelineDesc.vertexFunction = pDesc->pShaderProgram->pVertexShader;
    renderPipelineDesc.fragmentFunction = pDesc->pShaderProgram->pFragmentShader;
    renderPipelineDesc.sampleCount = pDesc->mSampleCount;

    ASSERT(!pDesc->mSupportIndirectCommandBuffer || pDesc->pShaderProgram->mICB);
    renderPipelineDesc.supportIndirectCommandBuffers = pDesc->mSupportIndirectCommandBuffer;

    // add vertex layout to descriptor
    if (pDesc->pVertexLayout)
    {
        // setup vertex bindings
        for (uint32_t b = 0; b < pDesc->pVertexLayout->mBindingCount; ++b)
        {
            const VertexBinding* binding = pDesc->pVertexLayout->mBindings + b;
            // #NOTE: Buffer index starts at 30 and decrements based on binding
            // Example: If attrib->mBinding is 3, bufferIndex will be 27
            const uint32_t       bufferIndex = VERTEX_BINDING_OFFSET - b;

            renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride = binding->mStride;
            renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepRate = 1;

            if (pDesc->pShaderProgram->mTessellation)
            {
                renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerPatchControlPoint;
            }
            else if (VERTEX_BINDING_RATE_INSTANCE == binding->mRate)
            {
                renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerInstance;
            }
            else
            {
                renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stepFunction = MTLVertexStepFunctionPerVertex;
            }
        }

        // setup vertex attributes
        for (uint32_t i = 0; i < pDesc->pVertexLayout->mAttribCount; ++i)
        {
            const VertexAttrib*  attrib = pDesc->pVertexLayout->mAttribs + i;
            const VertexBinding* binding = &pDesc->pVertexLayout->mBindings[attrib->mBinding];
            const uint32_t       bufferIndex = VERTEX_BINDING_OFFSET - attrib->mBinding;

            renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].offset = attrib->mOffset;
            renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].bufferIndex = bufferIndex;
            renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].format = util_to_mtl_vertex_format(attrib->mFormat);

            // update binding stride if necessary
            if (!binding->mStride)
            {
                // guessing stride using attribute offset in case there are several attributes at the same binding
                const uint32_t currentStride = (uint32_t)renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride;
                renderPipelineDesc.vertexDescriptor.layouts[bufferIndex].stride =
                    max(attrib->mOffset + TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8, currentStride);
            }
        }
    }

    // available on ios 12.0 and required for render_target_array_index semantics
    //  add pipeline settings to descriptor
    switch (pDesc->mPrimitiveTopo)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;
        break;
    case PRIMITIVE_TOPO_LINE_LIST:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
        break;
    case PRIMITIVE_TOPO_LINE_STRIP:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
        break;
    case PRIMITIVE_TOPO_TRI_LIST:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        break;
    case PRIMITIVE_TOPO_TRI_STRIP:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        break;
    default:
        renderPipelineDesc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        break;
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
        pPipeline->pDepthStencilState = pDesc->pDepthState ? util_to_depth_state(pRenderer, pDesc->pDepthState) : pDefaultDepthState;

        renderPipelineDesc.depthAttachmentPixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mDepthStencilFormat);
        if (pDesc->mDepthStencilFormat == TinyImageFormat_D24_UNORM_S8_UINT ||
            pDesc->mDepthStencilFormat == TinyImageFormat_D32_SFLOAT_S8_UINT)
            renderPipelineDesc.stencilAttachmentPixelFormat = renderPipelineDesc.depthAttachmentPixelFormat;
    }

    // assign common tesselation configuration if needed.
    if (pDesc->pShaderProgram->pVertexShader.patchType != MTLPatchTypeNone)
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

#if defined(ENABLE_GRAPHICS_DEBUG)
    if (pName)
    {
        renderPipelineDesc.label = [NSString stringWithUTF8String:pName];
    }
#endif

    // create pipeline from descriptor
    NSError* error = nil;
    pPipeline->pRenderPipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
                                                                                       options:MTLPipelineOptionNone
                                                                                    reflection:nil
                                                                                         error:&error];
    if (!pPipeline->pRenderPipelineState)
    {
        LOGF(LogLevel::eERROR, "Failed to create render pipeline state, error:\n%s", [error.description UTF8String]);
        ASSERT(false);
        return;
    }

    switch (pDesc->mPrimitiveTopo)
    {
    case PRIMITIVE_TOPO_POINT_LIST:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypePoint;
        break;
    case PRIMITIVE_TOPO_LINE_LIST:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypeLine;
        break;
    case PRIMITIVE_TOPO_LINE_STRIP:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypeLineStrip;
        break;
    case PRIMITIVE_TOPO_TRI_LIST:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangle;
        break;
    case PRIMITIVE_TOPO_TRI_STRIP:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangleStrip;
        break;
    default:
        pPipeline->mPrimitiveType = (uint32_t)MTLPrimitiveTypeTriangle;
        break;
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
    const uint32_t* numThreadsPerGroup = pDesc->pShaderProgram->mNumThreadsPerGroup;
    pPipeline->mNumThreadsPerGroup = MTLSizeMake(numThreadsPerGroup[0], numThreadsPerGroup[1], numThreadsPerGroup[2]);

    MTLComputePipelineDescriptor* pipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
    pipelineDesc.computeFunction = pDesc->pShaderProgram->pComputeShader;
#if defined(ENABLE_GRAPHICS_DEBUG)
    if (pName)
    {
        pipelineDesc.label = [NSString stringWithUTF8String:pName];
    }
#endif

    NSError* error = nil;
    pPipeline->pComputePipelineState = [pRenderer->pDevice newComputePipelineStateWithDescriptor:pipelineDesc
                                                                                         options:MTLPipelineOptionNone
                                                                                      reflection:NULL
                                                                                           error:&error];
    if (!pPipeline->pComputePipelineState)
    {
        LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [error.description UTF8String]);
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
    default:
        ASSERT(false); // unknown pipeline type
        break;
    }
}

void mtl_removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pPipeline);

    pPipeline->pRenderPipelineState = nil;
    pPipeline->pComputePipelineState = nil;
    pPipeline->pDepthStencilState = nil;

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
    case INDIRECT_COMMAND_BUFFER_OPTIMIZE:
        pCommandSignature->mStride = 1;
        break;
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
    ASSERT(pBuffer->pBuffer.storageMode != MTLStorageModePrivate && "Trying to map non-cpu accessible resource");
    pBuffer->pCpuMappedAddress = (uint8_t*)pBuffer->pBuffer.contents + pBuffer->mOffset;
}
void mtl_unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    ASSERT(pBuffer->pBuffer.storageMode != MTLStorageModePrivate && "Trying to unmap non-cpu accessible resource");
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
        pCmd->pRenderEncoder = nil;
        pCmd->pComputeEncoder = nil;
        pCmd->pBlitEncoder = nil;
        pCmd->pBoundPipeline = NULL;
        pCmd->mBoundIndexBuffer = nil;
#ifdef ENABLE_GRAPHICS_DEBUG
        pCmd->mDebugMarker[0] = '\0';
#endif

        // 'mExtendedEncoderDebugReport' should be disabled already if current OS version is < IOS14_RUNTIME.
        if (pCmd->pRenderer->pContext->mMtl.mExtendedEncoderDebugReport)
        {
            // Need this to supress '@available' warnings.
            if (IOS14_RUNTIME)
            {
                // Enable command buffer to save additional info. on GPU runtime error..
                MTLCommandBufferDescriptor* pDesc = [[MTLCommandBufferDescriptor alloc] init];
                pDesc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
                pCmd->pCommandBuffer = [pCmd->pQueue->pCommandQueue commandBufferWithDescriptor:pDesc];
            }
        }
        else
        {
            pCmd->pCommandBuffer = [pCmd->pQueue->pCommandQueue commandBuffer];
        }

        [pCmd->pCommandBuffer setLabel:[pCmd->pQueue->pCommandQueue label]];
    }

    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; i++)
    {
        pCmd->mBoundDescriptorSets[i] = NULL;
        pCmd->mBoundDescriptorSetIndices[i] = UINT32_MAX;
    }
    pCmd->pUsedRootSignature = NULL;
    pCmd->mShouldRebindDescriptorSets = false;
    pCmd->mShouldRebindPipeline = false;
    pCmd->mPipelineType = PIPELINE_TYPE_UNDEFINED;
}

void mtl_endCmd(Cmd* pCmd)
{
    @autoreleasepool
    {
        util_end_current_encoders(pCmd, true);
    }
}

void mtl_cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);

    if (!pDesc)
    {
        return;
    }

    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; i++)
    {
        if (pCmd->mBoundDescriptorSets[i])
        {
            ASSERT(pCmd->mBoundDescriptorSets[i]->pRootSignature == pCmd->pUsedRootSignature);
            pCmd->mShouldRebindDescriptorSets |= 1 << i;
        }
    }
    if (pCmd->pBoundPipeline)
    {
        pCmd->mShouldRebindPipeline = 1;
    }

    const bool hasDepth = pDesc->mDepthStencil.pDepthStencil;

    @autoreleasepool
    {
        MTLRenderPassDescriptor* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];

        // Flush color attachments
        for (uint32_t i = 0; i < pDesc->mRenderTargetCount; ++i)
        {
            const BindRenderTargetDesc* desc = &pDesc->mRenderTargets[i];
            Texture*                    colorAttachment = desc->pRenderTarget->pTexture;

            renderPassDesc.colorAttachments[i].texture = colorAttachment->pTexture;
            renderPassDesc.colorAttachments[i].level = desc->mUseMipSlice ? desc->mMipSlice : 0;
            if (desc->mUseArraySlice)
            {
                if (desc->pRenderTarget->mDepth > 1)
                {
                    renderPassDesc.colorAttachments[i].depthPlane = desc->mArraySlice;
                }
                else
                {
                    renderPassDesc.colorAttachments[i].slice = desc->mArraySlice;
                }
            }
            else if (desc->pRenderTarget->mArraySize > 1)
            {
                renderPassDesc.renderTargetArrayLength = desc->pRenderTarget->mArraySize;
            }
            else if (desc->pRenderTarget->mDepth > 1)
            {
                renderPassDesc.renderTargetArrayLength = desc->pRenderTarget->mDepth;
            }

#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            bool resolveAttachment =
                STORE_ACTION_RESOLVE_STORE == desc->mStoreAction || STORE_ACTION_RESOLVE_DONTCARE == desc->mStoreAction;

            if (resolveAttachment)
            {
                ASSERT(desc->pRenderTarget->pResolveAttachment);

                id<MTLTexture> resolveAttachment = desc->pRenderTarget->pResolveAttachment->pTexture->pTexture;
                renderPassDesc.colorAttachments[i].resolveTexture = resolveAttachment;
                renderPassDesc.colorAttachments[i].resolveLevel = desc->mUseMipSlice ? desc->mMipSlice : 0;
                if (desc->mUseArraySlice)
                {
                    if (desc->pRenderTarget->mDepth > 1)
                    {
                        renderPassDesc.colorAttachments[i].resolveDepthPlane = desc->mArraySlice;
                    }
                    else
                    {
                        renderPassDesc.colorAttachments[i].resolveSlice = desc->mArraySlice;
                    }
                }
            }
#endif

            renderPassDesc.colorAttachments[i].loadAction = util_to_mtl_load_action(desc->mLoadAction);

            // For on-tile (memoryless) textures, we never need to store.
#if defined(USE_MSAA_RESOLVE_ATTACHMENTS)
            if (resolveAttachment)
            {
                renderPassDesc.colorAttachments[i].storeAction =
                    colorAttachment->mLazilyAllocated ? MTLStoreActionMultisampleResolve : util_to_mtl_store_action(desc->mStoreAction);
            }
            else
#endif
            {
                renderPassDesc.colorAttachments[i].storeAction =
                    colorAttachment->mLazilyAllocated ? MTLStoreActionDontCare : util_to_mtl_store_action(desc->mStoreAction);
            }

            const ClearValue& clearValue = desc->mOverrideClearValue ? desc->mClearValue : desc->pRenderTarget->mClearValue;
            renderPassDesc.colorAttachments[i].clearColor = MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);
        }

        if (hasDepth)
        {
            const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
            renderPassDesc.depthAttachment.texture = desc->pDepthStencil->pTexture->pTexture;
            if (desc->mUseMipSlice)
            {
                renderPassDesc.depthAttachment.level = desc->mMipSlice;
            }
            if (desc->mUseArraySlice)
            {
                renderPassDesc.depthAttachment.slice = desc->mArraySlice;
            }
#ifndef TARGET_IOS
            bool isStencilEnabled = desc->pDepthStencil->pTexture->pTexture.pixelFormat == MTLPixelFormatDepth24Unorm_Stencil8;
#else
            bool isStencilEnabled = false;
#endif
            isStencilEnabled =
                isStencilEnabled || desc->pDepthStencil->pTexture->pTexture.pixelFormat == MTLPixelFormatDepth32Float_Stencil8;
            if (isStencilEnabled)
            {
                renderPassDesc.stencilAttachment.texture = desc->pDepthStencil->pTexture->pTexture;
                if (desc->mUseMipSlice)
                {
                    renderPassDesc.stencilAttachment.level = desc->mMipSlice;
                }
                if (desc->mUseArraySlice)
                {
                    renderPassDesc.stencilAttachment.slice = desc->mArraySlice;
                }
            }

            renderPassDesc.depthAttachment.loadAction = util_to_mtl_load_action(desc->mLoadAction);
            // For on-tile (memoryless) textures, we never need to store.
            renderPassDesc.depthAttachment.storeAction =
                desc->pDepthStencil->pTexture->mLazilyAllocated ? MTLStoreActionDontCare : util_to_mtl_store_action(desc->mStoreAction);

            if (isStencilEnabled)
            {
                renderPassDesc.stencilAttachment.loadAction = util_to_mtl_load_action(desc->mLoadActionStencil);
                renderPassDesc.stencilAttachment.storeAction = desc->pDepthStencil->pTexture->mLazilyAllocated
                                                                   ? MTLStoreActionDontCare
                                                                   : util_to_mtl_store_action(desc->mStoreActionStencil);
            }
            else
            {
                renderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
                renderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
            }

            const ClearValue& clearValue = desc->mOverrideClearValue ? desc->mClearValue : desc->pDepthStencil->mClearValue;
            renderPassDesc.depthAttachment.clearDepth = clearValue.depth;
            if (isStencilEnabled)
            {
                renderPassDesc.stencilAttachment.clearStencil = clearValue.stencil;
            }
        }
        else
        {
            renderPassDesc.depthAttachment.loadAction = MTLLoadActionDontCare;
            renderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
            renderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
            renderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
        }

        if (pCmd->mSampleLocationsCount > 0 && pDesc->mRenderTargetCount > 0)
        {
            if (pDesc->mRenderTargets[0].pRenderTarget->mSampleCount > SAMPLE_COUNT_1)
            {
                [renderPassDesc setSamplePositions:pCmd->mSamplePositions count:pCmd->mSampleLocationsCount];
            }
        }

        if (!pDesc->mRenderTargetCount && !hasDepth)
        {
            renderPassDesc.renderTargetWidth = pDesc->mExtent[0];
            renderPassDesc.renderTargetHeight = pDesc->mExtent[1];
            renderPassDesc.defaultRasterSampleCount = 1;
        }

        // Add a sampler buffer attachment
        if (IOS14_RUNTIME)
        {
            if (pCmd->pRenderer->pGpu->mCounterTimestampEnabled && pCmd->pCurrentQueryPool != nil)
            {
                MTLRenderPassSampleBufferAttachmentDescriptor* sampleAttachmentDesc = renderPassDesc.sampleBufferAttachments[0];
                QueryPool*                                     pQueryPool = pCmd->pCurrentQueryPool;
                QuerySampleRange* pSample = &((QuerySampleRange*)pQueryPool->pQueries)[pCmd->mCurrentQueryIndex];

                uint32_t sampleStartIndex = pSample->mRenderStartIndex + (pSample->mRenderSamples * NUM_RENDER_STAGE_BOUNDARY_COUNTERS);

                if (sampleStartIndex < (pQueryPool->mCount * NUM_RENDER_STAGE_BOUNDARY_COUNTERS))
                {
                    sampleAttachmentDesc.sampleBuffer = pQueryPool->pSampleBuffer;
                    sampleAttachmentDesc.startOfVertexSampleIndex = sampleStartIndex;
                    sampleAttachmentDesc.endOfVertexSampleIndex = sampleStartIndex + 1;
                    sampleAttachmentDesc.startOfFragmentSampleIndex = sampleStartIndex + 2;
                    sampleAttachmentDesc.endOfFragmentSampleIndex = sampleStartIndex + 3;

                    pSample->mRenderSamples++;
                    pQueryPool->mRenderSamplesOffset += NUM_RENDER_STAGE_BOUNDARY_COUNTERS;
                }
            }
        }

        util_end_current_encoders(pCmd, false);
        pCmd->pRenderEncoder = [pCmd->pCommandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];

#ifdef ENABLE_GRAPHICS_DEBUG
        util_set_debug_group(pCmd);
        if (pCmd->mDebugMarker[0])
        {
            pCmd->pRenderEncoder.label = [NSString stringWithUTF8String:pCmd->mDebugMarker];
        }
#endif

        util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS); // apply the graphics barriers before flushing them

        util_set_heaps_graphics(pCmd);
    }
}

// Note: setSamplePositions in Metal is incosistent with other APIs: it cannot be called inside the renderpass
void mtl_cmdSetSampleLocations(Cmd* pCmd, SampleCount samples_count, uint32_t grid_size_x, uint32_t grid_size_y, SampleLocations* locations)
{
    uint32_t sampleLocationsCount = samples_count * grid_size_x * grid_size_y;
    ASSERT(sampleLocationsCount <= MAX_SAMPLE_LOCATIONS);

    for (int i = 0; i < sampleLocationsCount; ++i)
    {
        pCmd->mSamplePositions[i] = util_to_mtl_locations(locations[i]);
        ;
    }
    pCmd->mSampleLocationsCount = sampleLocationsCount;
}

void mtl_cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    ASSERT(pCmd);
    if (pCmd->pRenderEncoder == nil)
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

    [pCmd->pRenderEncoder setViewport:viewport];
}

void mtl_cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    ASSERT(pCmd);
    if (pCmd->pRenderEncoder == nil)
    {
        internal_log(eERROR, "Using cmdSetScissor out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetScissor");
        return;
    }

    MTLScissorRect scissor;
    scissor.x = x;
    scissor.y = y;
    scissor.width = width;
    scissor.height = height;
    if (height + y == 0 || width + x == 0)
    {
        scissor.width = max(1u, width);
        scissor.height = max(1u, height);
    }

    [pCmd->pRenderEncoder setScissorRect:scissor];
}

void mtl_cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);

    [pCmd->pRenderEncoder setStencilReferenceValue:val];
}

void mtl_cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);

    pCmd->pBoundPipeline = pPipeline;
    pCmd->mShouldRebindPipeline = 0;

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
            [pCmd->pRenderEncoder setRenderPipelineState:pPipeline->pRenderPipelineState];

            [pCmd->pRenderEncoder setCullMode:(MTLCullMode)pPipeline->mCullMode];
            [pCmd->pRenderEncoder setTriangleFillMode:(MTLTriangleFillMode)pPipeline->mFillMode];
            [pCmd->pRenderEncoder setFrontFacingWinding:(MTLWinding)pPipeline->mWinding];
            [pCmd->pRenderEncoder setDepthBias:pPipeline->mDepthBias slopeScale:pPipeline->mSlopeScale clamp:0.0f];
            [pCmd->pRenderEncoder setDepthClipMode:(MTLDepthClipMode)pPipeline->mDepthClipMode];

            if (pPipeline->pDepthStencilState)
            {
                [pCmd->pRenderEncoder setDepthStencilState:pPipeline->pDepthStencilState];
            }

            pCmd->mSelectedPrimitiveType = (uint32_t)pPipeline->mPrimitiveType;
        }
        else if (pPipeline->mType == PIPELINE_TYPE_COMPUTE)
        {
            if (!pCmd->pComputeEncoder)
            {
                // Add a sampler buffer attachment
                if (IOS14_RUNTIME)
                {
                    MTLComputePassDescriptor* computePassDescriptor = [MTLComputePassDescriptor computePassDescriptor];
                    MTLComputePassSampleBufferAttachmentDescriptor* sampleAttachmentDesc = computePassDescriptor.sampleBufferAttachments[0];
                    if (pCmd->pRenderer->pGpu->mCounterTimestampEnabled && pCmd->pCurrentQueryPool != nil)
                    {
                        QueryPool*        pQueryPool = pCmd->pCurrentQueryPool;
                        QuerySampleRange* pSample = &((QuerySampleRange*)pQueryPool->pQueries)[pCmd->mCurrentQueryIndex];

                        uint32_t sampleStartIndex =
                            pSample->mComputeStartIndex + (pSample->mComputeSamples * NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS);

                        if (sampleStartIndex < (pQueryPool->mCount * NUM_RENDER_STAGE_BOUNDARY_COUNTERS) +
                                                   (pQueryPool->mCount * NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS))
                        {
                            sampleAttachmentDesc.sampleBuffer = pQueryPool->pSampleBuffer;
                            sampleAttachmentDesc.startOfEncoderSampleIndex = sampleStartIndex;
                            sampleAttachmentDesc.endOfEncoderSampleIndex = sampleStartIndex + 1;

                            pSample->mComputeSamples++;
                            pQueryPool->mComputeSamplesOffset += NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS;
                        }
                    }

                    util_end_current_encoders(pCmd, barrierRequired);
                    pCmd->pComputeEncoder = [pCmd->pCommandBuffer computeCommandEncoderWithDescriptor:computePassDescriptor];
#ifdef ENABLE_GRAPHICS_DEBUG
                    util_set_debug_group(pCmd);
                    if (pCmd->mDebugMarker[0])
                    {
                        if (pCmd->pRenderEncoder)
                        {
                            pCmd->pRenderEncoder.label = [NSString stringWithUTF8String:pCmd->mDebugMarker];
                        }
                        if (pCmd->pComputeEncoder)
                        {
                            pCmd->pComputeEncoder.label = [NSString stringWithUTF8String:pCmd->mDebugMarker];
                        }
                    }
#endif

                    util_set_heaps_compute(pCmd);
                }
            }
            [pCmd->pComputeEncoder setComputePipelineState:pPipeline->pComputePipelineState];
        }
        else
        {
            ASSERT(false); // unknown pipeline type
        }
    }
}

void mtl_cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);

    pCmd->mBoundIndexBuffer = pBuffer->pBuffer;
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
    if (pCmd->pBoundPipeline && pCmd->pBoundPipeline->mTessellation)
    {
        startIdx = 1;
        [pCmd->pRenderEncoder setTessellationFactorBuffer:ppBuffers[0]->pBuffer
                                                   offset:ppBuffers[0]->mOffset + (pOffsets ? pOffsets[0] : 0)instanceStride:0];
    }

    for (uint32_t i = 0; i < bufferCount - startIdx; i++)
    {
        uint32_t index = startIdx + i;
        uint32_t offset = (uint32_t)(ppBuffers[index]->mOffset + (pOffsets ? pOffsets[index] : 0));
        [pCmd->pRenderEncoder setVertexBuffer:ppBuffers[index]->pBuffer offset:offset atIndex:(VERTEX_BINDING_OFFSET - i)];
#ifdef ENABLE_DRAW_INDEX_BASE_VERTEX_FALLBACK
        pCmd->mOffsets[i] = offset;
        pCmd->mStrides[i] = pStrides[index];
#endif
    }
}

void RebindState(Cmd* pCmd)
{
    for (uint32_t i = 0; i < DESCRIPTOR_UPDATE_FREQ_COUNT; i++)
    {
        if (pCmd->mShouldRebindDescriptorSets & (1 << i))
        {
            cmdBindDescriptorSet(pCmd, pCmd->mBoundDescriptorSetIndices[i], pCmd->mBoundDescriptorSets[i]);
        }
    }
    if (pCmd->mShouldRebindPipeline)
    {
        cmdBindPipeline(pCmd, pCmd->pBoundPipeline);
    }
    pCmd->mShouldRebindDescriptorSets = 0;
    pCmd->mShouldRebindPipeline = 0;
}

void mtl_cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
    ASSERT(pCmd);

    RebindState(pCmd);

    if (!pCmd->pBoundPipeline->mTessellation)
    {
        [pCmd->pRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                 vertexStart:firstVertex
                                 vertexCount:vertexCount];
    }
    else // Tessellated draw version.
    {
        [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                               patchStart:firstVertex
                               patchCount:vertexCount
                         patchIndexBuffer:nil
                   patchIndexBufferOffset:0
                            instanceCount:1
                             baseInstance:0];
    }
}

void mtl_cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    ASSERT(pCmd);

    RebindState(pCmd);

    if (!pCmd->pBoundPipeline->mTessellation)
    {
        if (firstInstance == 0)
        {
            [pCmd->pRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                     vertexStart:firstVertex
                                     vertexCount:vertexCount
                                   instanceCount:instanceCount];
        }
        else
        {
            [pCmd->pRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                     vertexStart:firstVertex
                                     vertexCount:vertexCount
                                   instanceCount:instanceCount
                                    baseInstance:firstInstance];
        }
    }
    else // Tessellated draw version.
    {
        [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                               patchStart:firstVertex
                               patchCount:vertexCount
                         patchIndexBuffer:nil
                   patchIndexBufferOffset:0
                            instanceCount:instanceCount
                             baseInstance:firstInstance];
    }
}

void mtl_cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
{
    ASSERT(pCmd);

    RebindState(pCmd);

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
                    [pCmd->pRenderEncoder setVertexBufferOffset:offset + stride * firstVertex atIndex:(VERTEX_BINDING_OFFSET - i)];
                }
            }
        }

        [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                         indexCount:indexCount
                                          indexType:indexType
                                        indexBuffer:indexBuffer
                                  indexBufferOffset:offset];
#else
        if (!firstVertex)
        {
            [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                             indexCount:indexCount
                                              indexType:indexType
                                            indexBuffer:indexBuffer
                                      indexBufferOffset:offset];
        }
        else
        {
            // sizeof can't be applied to bitfield.
            ASSERT((bool)pCmd->pRenderer->pGpu->mSettings.mDrawIndexVertexOffsetSupported);
            [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                             indexCount:indexCount
                                              indexType:indexType
                                            indexBuffer:indexBuffer
                                      indexBufferOffset:offset
                                          instanceCount:1
                                             baseVertex:firstVertex
                                           baseInstance:0];
        }
#endif
#else
        [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                         indexCount:indexCount
                                          indexType:indexType
                                        indexBuffer:indexBuffer
                                  indexBufferOffset:offset
                                      instanceCount:1
                                         baseVertex:firstVertex
                                       baseInstance:0];
#endif
    }
    else // Tessellated draw version.
    {
        // to suppress warning passing nil to controlPointIndexBuffer
        // todo: Add control point index buffer to be passed when necessary
        id<MTLBuffer> _Nullable indexBuf = nil;
        [pCmd->pRenderEncoder drawIndexedPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                      patchStart:firstIndex
                                      patchCount:indexCount
                                patchIndexBuffer:indexBuffer
                          patchIndexBufferOffset:0
                         controlPointIndexBuffer:indexBuf
                   controlPointIndexBufferOffset:0
                                   instanceCount:1
                                    baseInstance:0];
    }
}

void mtl_cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
                                 uint32_t firstInstance)
{
    ASSERT(pCmd);

    RebindState(pCmd);

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
                        [pCmd->pRenderEncoder setVertexBufferOffset:offset + stride * firstVertex atIndex:(VERTEX_BINDING_OFFSET - i)];
                    }
                }
            }

            [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                             indexCount:indexCount
                                              indexType:indexType
                                            indexBuffer:indexBuffer
                                      indexBufferOffset:offset
                                          instanceCount:instanceCount];
        }
#else
        if (firstInstance || firstVertex)
        {
            [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
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
            [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                             indexCount:indexCount
                                              indexType:indexType
                                            indexBuffer:indexBuffer
                                      indexBufferOffset:offset
                                          instanceCount:instanceCount];
        }
#endif

#else
        [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                         indexCount:indexCount
                                          indexType:indexType
                                        indexBuffer:indexBuffer
                                  indexBufferOffset:offset
                                      instanceCount:instanceCount
                                         baseVertex:firstVertex
                                       baseInstance:firstInstance];
#endif
    }
    else // Tessellated draw version.
    {
        // to supress warning passing nil to controlPointIndexBuffer
        // todo: Add control point index buffer to be passed when necessary
        id<MTLBuffer> _Nullable indexBuf = nil;
        [pCmd->pRenderEncoder drawIndexedPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                      patchStart:firstIndex
                                      patchCount:indexCount
                                patchIndexBuffer:indexBuffer
                          patchIndexBufferOffset:0
                         controlPointIndexBuffer:indexBuf
                   controlPointIndexBufferOffset:0
                                   instanceCount:instanceCount
                                    baseInstance:firstInstance];
    }
}

void mtl_cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    ASSERT(pCmd);
    ASSERT(pCmd->pComputeEncoder != nil);

    // There might have been a barrier inserted since last dispatch call
    // This only applies to dispatch since you can issue barriers inside compute pass but this is not possible inside render pass
    // For render pass, barriers are issued in cmdBindRenderTargets after beginning new encoder
    util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);

    MTLSize threadgroupCount = MTLSizeMake(groupCountX, groupCountY, groupCountZ);

    [pCmd->pComputeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:pCmd->pBoundPipeline->mNumThreadsPerGroup];
}

void mtl_cmdExecuteIndirect(Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer,
                            uint64_t bufferOffset, Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    ASSERT(pCmd);

    RebindState(pCmd);

    const IndirectArgumentType drawType = pCommandSignature->mDrawType;

    if (pCmd->pRenderer->pGpu->mSettings.mIndirectCommandBuffer)
    {
        ASSERT(pCommandSignature->mStride);
        uint64_t rangeOffset = bufferOffset / pCommandSignature->mStride;

        if (drawType == INDIRECT_COMMAND_BUFFER_OPTIMIZE && maxCommandCount)
        {
            if (!pCmd->pBlitEncoder)
            {
                util_end_current_encoders(pCmd, false);
                pCmd->pBlitEncoder = [pCmd->pCommandBuffer blitCommandEncoder];
                util_set_debug_group(pCmd);
                util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
            }

            [pCmd->pBlitEncoder optimizeIndirectCommandBuffer:pIndirectBuffer->pIndirectCommandBuffer
                                                    withRange:NSMakeRange(rangeOffset, maxCommandCount)];
            return;
        }
        else if (drawType == INDIRECT_COMMAND_BUFFER_RESET && maxCommandCount)
        {
            if (!pCmd->pBlitEncoder)
            {
                util_end_current_encoders(pCmd, false);
                pCmd->pBlitEncoder = [pCmd->pCommandBuffer blitCommandEncoder];
                util_set_debug_group(pCmd);
                util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
            }
            [pCmd->pBlitEncoder resetCommandsInBuffer:pIndirectBuffer->pIndirectCommandBuffer
                                            withRange:NSMakeRange(rangeOffset, maxCommandCount)];
            return;
        }
        else if ((pIndirectBuffer->pIndirectCommandBuffer || drawType == INDIRECT_COMMAND_BUFFER) && maxCommandCount)
        {
            [pCmd->pRenderEncoder executeCommandsInBuffer:pIndirectBuffer->pIndirectCommandBuffer
                                                withRange:NSMakeRange(rangeOffset, maxCommandCount)];
            return;
        }
    }

    if (drawType == INDIRECT_DRAW)
    {
        if (!pCmd->pBoundPipeline->mTessellation)
        {
            for (uint32_t i = 0; i < maxCommandCount; i++)
            {
                uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
                [pCmd->pRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                      indirectBuffer:pIndirectBuffer->pBuffer
                                indirectBufferOffset:indirectBufferOffset];
            }
        }
        else // Tessellated indirect draw version.
        {
            ASSERT((bool)pCmd->pRenderer->pGpu->mSettings.mTessellationIndirectDrawSupported);
            for (uint32_t i = 0; i < maxCommandCount; i++)
            {
                uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
                [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                 patchIndexBuffer:nil
                           patchIndexBufferOffset:0
                                   indirectBuffer:pIndirectBuffer->pBuffer
                             indirectBufferOffset:indirectBufferOffset];
            }
        }
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

                [pCmd->pRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                                  indexType:indexType
                                                indexBuffer:indexBuffer
                                          indexBufferOffset:0
                                             indirectBuffer:pIndirectBuffer->pBuffer
                                       indirectBufferOffset:indirectBufferOffset];
            }
        }
        else // Tessellated indirect draw version.
        {
            ASSERT((bool)pCmd->pRenderer->pGpu->mSettings.mTessellationIndirectDrawSupported);
            for (uint32_t i = 0; i < maxCommandCount; ++i)
            {
                id       indexBuffer = pCmd->mBoundIndexBuffer;
                uint64_t indirectBufferOffset = bufferOffset + pCommandSignature->mStride * i;
                [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                 patchIndexBuffer:indexBuffer
                           patchIndexBufferOffset:0
                                   indirectBuffer:pIndirectBuffer->pBuffer
                             indirectBufferOffset:indirectBufferOffset];
            }
        }
    }
    else if (drawType == INDIRECT_DISPATCH)
    {
        // There might have been a barrier inserted since last dispatch call
        // This only applies to dispatch since you can issue barriers inside compute pass but this is not possible inside render pass
        // For render pass, barriers are issued in cmdBindRenderTargets after beginning new encoder
        util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);

        for (uint32_t i = 0; i < maxCommandCount; ++i)
        {
            [pCmd->pComputeEncoder dispatchThreadgroupsWithIndirectBuffer:pIndirectBuffer->pBuffer
                                                     indirectBufferOffset:bufferOffset + pCommandSignature->mStride * i
                                                    threadsPerThreadgroup:pCmd->pBoundPipeline->mNumThreadsPerGroup];
        }
    }
}

void mtl_cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
                            TextureBarrier* pTextureBarriers, uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
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
    ASSERT(pSrcBuffer->pBuffer);
    ASSERT(pBuffer);
    ASSERT(pBuffer->pBuffer);
    ASSERT(srcOffset + size <= pSrcBuffer->mSize);
    ASSERT(dstOffset + size <= pBuffer->mSize);

    if (!pCmd->pBlitEncoder)
    {
        util_end_current_encoders(pCmd, false);
        pCmd->pBlitEncoder = [pCmd->pCommandBuffer blitCommandEncoder];
        util_set_debug_group(pCmd);
    }

    [pCmd->pBlitEncoder copyFromBuffer:pSrcBuffer->pBuffer
                          sourceOffset:srcOffset + pSrcBuffer->mOffset
                              toBuffer:pBuffer->pBuffer
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
    MTLSize sourceSize =
        MTLSizeMake(max(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel), max(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel),
                    max(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel));

#ifdef TARGET_IOS
    uint64_t formatNamespace =
        (TinyImageFormat_Code((TinyImageFormat)pTexture->mFormat) & ((1 << TinyImageFormat_NAMESPACE_REQUIRED_BITS) - 1));
    bool isPvrtc = (TinyImageFormat_NAMESPACE_PVRTC == formatNamespace);

    // PVRTC - replaceRegion is the most straightforward method
    if (isPvrtc)
    {
        MTLRegion region = MTLRegionMake3D(0, 0, 0, sourceSize.width, sourceSize.height, sourceSize.depth);
        [pTexture->pTexture replaceRegion:region
                              mipmapLevel:pSubresourceDesc->mMipLevel
                                withBytes:(uint8_t*)pIntermediate->pCpuMappedAddress + pSubresourceDesc->mSrcOffset
                              bytesPerRow:0];
        return;
    }
#endif

    if (!pCmd->pBlitEncoder)
    {
        util_end_current_encoders(pCmd, false);
        pCmd->pBlitEncoder = [pCmd->pCommandBuffer blitCommandEncoder];
        util_set_debug_group(pCmd);
    }

    // Copy to the texture's final subresource.
    [pCmd->pBlitEncoder copyFromBuffer:pIntermediate->pBuffer
                          sourceOffset:pSubresourceDesc->mSrcOffset + pIntermediate->mOffset
                     sourceBytesPerRow:pSubresourceDesc->mRowPitch
                   sourceBytesPerImage:pSubresourceDesc->mSlicePitch
                            sourceSize:sourceSize
                             toTexture:pTexture->pTexture
                      destinationSlice:pSubresourceDesc->mArrayLayer
                      destinationLevel:pSubresourceDesc->mMipLevel
                     destinationOrigin:MTLOriginMake(0, 0, 0)
                               options:MTLBlitOptionNone];
}

void mtl_cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc)
{
    MTLSize sourceSize =
        MTLSizeMake(max(1, pTexture->mWidth >> pSubresourceDesc->mMipLevel), max(1, pTexture->mHeight >> pSubresourceDesc->mMipLevel),
                    max(1, pTexture->mDepth >> pSubresourceDesc->mMipLevel));

    if (!pCmd->pBlitEncoder)
    {
        util_end_current_encoders(pCmd, false);
        pCmd->pBlitEncoder = [pCmd->pCommandBuffer blitCommandEncoder];
        util_set_debug_group(pCmd);
    }

    // Copy to the texture's final subresource.
    [pCmd->pBlitEncoder copyFromTexture:pTexture->pTexture
                            sourceSlice:pSubresourceDesc->mArrayLayer
                            sourceLevel:pSubresourceDesc->mMipLevel
                           sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:sourceSize
                               toBuffer:pDstBuffer->pBuffer
                      destinationOffset:pDstBuffer->mOffset + pSubresourceDesc->mSrcOffset
                 destinationBytesPerRow:pSubresourceDesc->mRowPitch
               destinationBytesPerImage:pSubresourceDesc->mSlicePitch
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
        CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
        pSwapChain->mMTKDrawable = [layer nextDrawable];

        pSwapChain->mIndex = (pSwapChain->mIndex + 1) % pSwapChain->mImageCount;
        *pImageIndex = pSwapChain->mIndex;

        pSwapChain->ppRenderTargets[pSwapChain->mIndex]->pTexture->pTexture = pSwapChain->mMTKDrawable.texture;
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
        __block tfrg_atomic32_t commandsFinished = 0;
        __block Fence* completedFence = NULL;

        if (pFence)
        {
            completedFence = pFence;
            pFence->mSubmitted = true;
        }

        for (uint32_t i = 0; i < cmdCount; i++)
        {
            [ppCmds[i]->pCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
                uint32_t handlersCalled = 1u + tfrg_atomic32_add_relaxed(&commandsFinished, 1);

                if (handlersCalled == cmdCount)
                {
                    if (completedFence)
                    {
                        dispatch_semaphore_signal(completedFence->pSemaphore);
                    }
                }

                // Log any error that occurred while executing commands..
                if (nil != buffer.error)
                {
                    LOGF(LogLevel::eERROR, "Failed to execute commands with error (%s)", [buffer.error.description UTF8String]);
                }
            }];
        }

        // Signal the signal semaphores after the last command buffer has finished execution
        for (uint32_t i = 0; i < signalSemaphoreCount; i++)
        {
            Semaphore* semaphore = ppSignalSemaphores[i];
            [ppCmds[cmdCount - 1]->pCommandBuffer encodeSignalEvent:semaphore->pSemaphore value:++semaphore->mValue];
            semaphore->mSignaled = true;
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
            id<MTLCommandBuffer> waitCommandBuffer = [pQueue->pCommandQueue commandBufferWithUnretainedReferences];
#if defined(ENABLE_GRAPHICS_DEBUG)
            waitCommandBuffer.label = @"WAIT_COMMAND_BUFFER";
#endif
            for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
            {
                Semaphore* semaphore = ppWaitSemaphores[i];
                if (semaphore->mSignaled)
                {
                    [waitCommandBuffer encodeWaitForEvent:semaphore->pSemaphore value:semaphore->mValue];
                    semaphore->mSignaled = false;
                }
            }

            [waitCommandBuffer commit];
            waitCommandBuffer = nil;
        }

        // commit the command lists
        for (uint32_t i = 0; i < cmdCount; i++)
        {
            // Commit any uncommitted encoder. This is necessary before committing the command buffer
            util_end_current_encoders(ppCmds[i], false);

            [ppCmds[i]->pCommandBuffer commit];
            ppCmds[i]->pCommandBuffer = nil;
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
        ASSERT(pQueue->pCommandQueue != nil);

#if defined(AUTOMATED_TESTING)
        captureScreenshot(pSwapChain, pDesc->mIndex, true, false);
#endif

        // after committing a command buffer no more commands can be encoded on it: create a new command buffer for future commands
        pSwapChain->presentCommandBuffer = [pQueue->pCommandQueue commandBuffer];
        pSwapChain->presentCommandBuffer.label = @"PRESENT";

        [pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];

        pSwapChain->ppRenderTargets[pSwapChain->mIndex]->pTexture->pTexture = nil;
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
            dispatch_semaphore_wait(ppFences[i]->pSemaphore, DISPATCH_TIME_FOREVER);
        }
        ppFences[i]->mSubmitted = false;
    }
}

void mtl_waitQueueIdle(Queue* pQueue)
{
    ASSERT(pQueue);
    id<MTLCommandBuffer> waitCmdBuf = [pQueue->pCommandQueue commandBufferWithUnretainedReferences];

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
        long status = dispatch_semaphore_wait(pFence->pSemaphore, DISPATCH_TIME_NOW);
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

    *ppHandle = (void*)CFBridgingRetain(pTexture->pTexture);
}

/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void mtl_cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#ifdef ENABLE_GRAPHICS_DEBUG
    snprintf(pCmd->mDebugMarker, MAX_DEBUG_NAME_LENGTH, "%s", pName);
    util_set_debug_group(pCmd);
#endif
}

void mtl_cmdEndDebugMarker(Cmd* pCmd) { util_unset_debug_group(pCmd); }

void mtl_cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
    if (pCmd->pRenderEncoder)
        [pCmd->pRenderEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
    else if (pCmd->pComputeEncoder)
        [pCmd->pComputeEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
    else if (pCmd->pBlitEncoder)
        [pCmd->pBlitEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
}

void mtl_cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
{
    ASSERT(pCmd);
    ASSERT(pDesc);
    ASSERT(pDesc->pBuffer);

    const bool waitForWrite = pDesc->mFlags & MARKER_FLAG_WAIT_FOR_WRITE;
    if (!pCmd->pComputeEncoder)
    {
        util_end_current_encoders(pCmd, waitForWrite);
        pCmd->pComputeEncoder = [pCmd->pCommandBuffer computeCommandEncoder];
        util_set_debug_group(pCmd);
        util_set_heaps_compute(pCmd);
    }

    MTLSize  threadgroupCount = MTLSizeMake(1, 1, 1);
    MTLSize  threadsPerGroup = MTLSizeMake(1, 1, 1);
    uint32_t valueCount[2] = { pDesc->mValue, 1 };
    cmdBeginDebugMarker(pCmd, 1.0f, 1.0f, 0.0f, "WriteMarker");
    [pCmd->pComputeEncoder setComputePipelineState:pCmd->pRenderer->pFillBufferPipeline];
    [pCmd->pComputeEncoder setBuffer:pDesc->pBuffer->pBuffer offset:pDesc->mOffset atIndex:0];
    [pCmd->pComputeEncoder setBytes:valueCount length:sizeof(valueCount) atIndex:1];
    [pCmd->pComputeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadsPerGroup];
    cmdEndDebugMarker(pCmd);

    if (waitForWrite)
    {
        util_end_current_encoders(pCmd, waitForWrite);
    }
}

void mtl_getTimestampFrequency(Queue* pQueue, double* pFrequency) { *pFrequency = GPU_FREQUENCY; }

void mtl_addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    if (QUERY_TYPE_TIMESTAMP != pDesc->mType)
    {
        ASSERT(false && "Not supported");
        return;
    }

    if (!pRenderer->pGpu->mSettings.mTimestampQueries)
    {
        ASSERT(false && "Not supported");
        return;
    }

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
    ASSERT(pQueryPool);

    pQueryPool->mCount = pDesc->mQueryCount;
    pQueryPool->mStride = sizeof(QuerySampleRange);

    // Create counter sampler buffer
    if (pDesc->mType == QUERY_TYPE_TIMESTAMP)
    {
        if (pRenderer->pGpu->mCounterTimestampEnabled)
        {
            pQueryPool->pQueries = (void*)tf_calloc(pQueryPool->mCount, sizeof(QuerySampleRange));
            memset(pQueryPool->pQueries, 0, sizeof(QuerySampleRange) * pQueryPool->mCount);

            @autoreleasepool
            {
                MTLCounterSampleBufferDescriptor* pDescriptor;
                pDescriptor = [[MTLCounterSampleBufferDescriptor alloc] init];

                // This counter set instance belongs to the `device` instance.
                pDescriptor.counterSet = pRenderer->pGpu->pCounterSetTimestamp;

                // Set the buffer to use shared memory so the CPU and GPU can directly access its contents.
                pDescriptor.storageMode = MTLStorageModeShared;

                // Add Stage (4) and Compute (2) encoder counters..
                pDescriptor.sampleCount =
                    (NUM_RENDER_STAGE_BOUNDARY_COUNTERS * pDesc->mQueryCount) + (NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS * pDesc->mQueryCount);

                // Create the sample buffer by passing the descriptor to the device's factory method.
                NSError* error = nil;
                pQueryPool->pSampleBuffer = [pRenderer->pGpu->pGPU newCounterSampleBufferWithDescriptor:pDescriptor error:&error];

                if (error != nil)
                {
                    LOGF(LogLevel::eERROR, "Device failed to create a counter sample buffer: %s", [error.description UTF8String]);
                }
            }
        }
    }

    *ppQueryPool = pQueryPool;
}

void mtl_removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    pQueryPool->pSampleBuffer = nil;
    SAFE_FREE(pQueryPool->pQueries);
    SAFE_FREE(pQueryPool);
}

void mtl_cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    if (pQueryPool->mType == QUERY_TYPE_TIMESTAMP)
    {
        pCmd->pCurrentQueryPool = pQueryPool;
        pCmd->mCurrentQueryIndex = pQuery->mIndex;

        if (pQuery->mIndex == 0)
        {
            pQueryPool->mRenderSamplesOffset = 0;
            pQueryPool->mComputeSamplesOffset = pQueryPool->mCount * NUM_RENDER_STAGE_BOUNDARY_COUNTERS;
        }

        if (pQueryPool->pQueries)
        {
            QuerySampleRange* pSample = &((QuerySampleRange*)pQueryPool->pQueries)[pQuery->mIndex];
            pSample->mRenderSamples = 0;
            pSample->mComputeSamples = 0;
            pSample->mRenderStartIndex = pQueryPool->mRenderSamplesOffset;
            pSample->mComputeStartIndex = pQueryPool->mComputeSamplesOffset;
        }
    }
}

void mtl_cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    pCmd->pCurrentQueryPool = nil;
    pCmd->mCurrentQueryIndex = -1;

    if (pQueryPool->mType == QUERY_TYPE_TIMESTAMP)
    {
        if (!gIssuedQueryIgnoredWarning && pQueryPool->pQueries)
        {
            QuerySampleRange* pSample = &((QuerySampleRange*)pQueryPool->pQueries)[pQuery->mIndex];
            if (pSample->mRenderSamples == 0 && pSample->mComputeSamples == 0)
            {
                gIssuedQueryIgnoredWarning = true;
                LOGF(eWARNING, "A BeginQuery() is being ignored: Try moving it above cmdBindRenderTarget() or cmdBindPipeline(Compute). "
                               "Warning only issued once.");
            }
        }
    }
}

void mtl_cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount) {}

void mtl_cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount) {}

void mtl_getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    uint64_t sumEncoderTimestamps = 0;

    if (pQueryPool->pSampleBuffer != nil)
    {
        QuerySampleRange* pSample = &((QuerySampleRange*)pQueryPool->pQueries)[queryIndex];

        uint32_t numSamples = pSample->mRenderSamples;
        uint32_t startSampleIndex = pSample->mRenderStartIndex;
        uint32_t sampleCount = numSamples * NUM_RENDER_STAGE_BOUNDARY_COUNTERS;

        // Cast the data's bytes property to the counter's result type.
        MTLCounterResultTimestamp* timestamps =
            util_resolve_counter_sample_buffer(startSampleIndex, sampleCount, pQueryPool->pSampleBuffer);

        if (timestamps != nil)
        {
            uint64_t maxVertTime = 0, minFragTime = UINT64_MAX;
            // Check for invalid values within the (resolved) data from the counter sample buffer.
            for (int index = 0; index < numSamples; ++index)
            {
                uint32_t currentSampleIdx = index * NUM_RENDER_STAGE_BOUNDARY_COUNTERS;

                // Start and end of vertex stage.
                MTLTimestamp sVertTime = timestamps[currentSampleIdx].timestamp;
                MTLTimestamp eVertTime = timestamps[currentSampleIdx + 1].timestamp;

                // Start and end of fragment stage.
                MTLTimestamp sFragTime = timestamps[currentSampleIdx + 2].timestamp;
                MTLTimestamp eFragTime = timestamps[currentSampleIdx + 3].timestamp;

                if (sVertTime == MTLCounterErrorValue || eVertTime == MTLCounterErrorValue || sFragTime == MTLCounterErrorValue ||
                    eFragTime == MTLCounterErrorValue)
                {
                    LOGF(eWARNING, "Render encoder timestamp sample %u (of %u) has an error value.", currentSampleIdx, sampleCount);
                    continue;
                }

                sumEncoderTimestamps += (eFragTime - sFragTime) + (eVertTime - sVertTime);

                maxVertTime = max(maxVertTime, eVertTime);
                minFragTime = min(minFragTime, sFragTime);
            }

            // Subtract overlap delta..
            if (minFragTime != UINT64_MAX)
                sumEncoderTimestamps -= (maxVertTime > minFragTime) ? maxVertTime - minFragTime : 0;
        }

        numSamples = pSample->mComputeSamples;
        startSampleIndex = pSample->mComputeStartIndex;
        sampleCount = numSamples * NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS;

        timestamps = util_resolve_counter_sample_buffer(startSampleIndex, sampleCount, pQueryPool->pSampleBuffer);

        if (timestamps != nil)
        {
            // Check for invalid values within the (resolved) data from the counter sample buffer.
            for (int index = 0; index < numSamples; ++index)
            {
                uint32_t currentSampleIdx = index * NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS;

                // Start and end of vertex stage.
                MTLTimestamp startTimestamp = timestamps[currentSampleIdx].timestamp;
                MTLTimestamp endTimestamp = timestamps[currentSampleIdx + 1].timestamp;

                if (startTimestamp == MTLCounterErrorValue || endTimestamp == MTLCounterErrorValue)
                {
                    LOGF(eWARNING, "Compute encoder timestamp sample %u (of %u) has an error value.", currentSampleIdx, sampleCount);
                    continue;
                }

                sumEncoderTimestamps += endTimestamp - startTimestamp;
            }
        }
    }

    pOutData->mBeginTimestamp = 0;
    pOutData->mEndTimestamp = sumEncoderTimestamps;
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
    if (pBuffer->pBuffer)
    {
        NSString* str = [NSString stringWithUTF8String:pName];

#if !defined(TARGET_APPLE_ARM64)
        if (RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pBuffer->mMemoryUsage)
        {
            [pBuffer->pBuffer addDebugMarker:str range:NSMakeRange(pBuffer->mOffset, pBuffer->mSize)];
        }
        else
#endif
        {
            pBuffer->pBuffer.label = str;
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
    pTexture->pTexture.label = str;
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
        [pCmd->pRenderEncoder setVertexBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
    }

    if (pDesc->mUsedStages & SHADER_STAGE_FRAG)
    {
        [pCmd->pRenderEncoder setFragmentBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
    }

    if (pDesc->mUsedStages & SHADER_STAGE_COMP)
    {
        [pCmd->pComputeEncoder setBytes:pConstants length:pDesc->mSize atIndex:pDesc->mReg];
    }
}

void util_bind_root_cbv(Cmd* pCmd, const RootDescriptorHandle* pHandle)
{
    if (pHandle->mStage & SHADER_STAGE_VERT)
    {
        [pCmd->pRenderEncoder setVertexBuffers:pHandle->pArr
                                       offsets:pHandle->pOffsets
                                     withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
    }

    if (pHandle->mStage & SHADER_STAGE_FRAG)
    {
        [pCmd->pRenderEncoder setFragmentBuffers:pHandle->pArr
                                         offsets:pHandle->pOffsets
                                       withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
    }

    if (pHandle->mStage & SHADER_STAGE_COMP)
    {
        [pCmd->pComputeEncoder setBuffers:pHandle->pArr
                                  offsets:pHandle->pOffsets
                                withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
    }
}

void util_set_heaps_graphics(Cmd* pCmd) { [pCmd->pRenderEncoder useHeaps:pCmd->pRenderer->pHeaps count:pCmd->pRenderer->mHeapCount]; }

void util_set_heaps_compute(Cmd* pCmd) { [pCmd->pComputeEncoder useHeaps:pCmd->pRenderer->pHeaps count:pCmd->pRenderer->mHeapCount]; }

MTLVertexFormat util_to_mtl_vertex_format(const TinyImageFormat format)
{
    switch (format)
    {
    case TinyImageFormat_R8G8_UINT:
        return MTLVertexFormatUChar2;
    case TinyImageFormat_R8G8B8_UINT:
        return MTLVertexFormatUChar3;
    case TinyImageFormat_R8G8B8A8_UINT:
        return MTLVertexFormatUChar4;

    case TinyImageFormat_R8G8_SINT:
        return MTLVertexFormatChar2;
    case TinyImageFormat_R8G8B8_SINT:
        return MTLVertexFormatChar3;
    case TinyImageFormat_R8G8B8A8_SINT:
        return MTLVertexFormatChar4;

    case TinyImageFormat_R8G8_UNORM:
        return MTLVertexFormatUChar2Normalized;
    case TinyImageFormat_R8G8B8_UNORM:
        return MTLVertexFormatUChar3Normalized;
    case TinyImageFormat_R8G8B8A8_UNORM:
        return MTLVertexFormatUChar4Normalized;

    case TinyImageFormat_R8G8_SNORM:
        return MTLVertexFormatChar2Normalized;
    case TinyImageFormat_R8G8B8_SNORM:
        return MTLVertexFormatChar3Normalized;
    case TinyImageFormat_R8G8B8A8_SNORM:
        return MTLVertexFormatChar4Normalized;

    case TinyImageFormat_R16G16_UNORM:
        return MTLVertexFormatUShort2Normalized;
    case TinyImageFormat_R16G16B16_UNORM:
        return MTLVertexFormatUShort3Normalized;
    case TinyImageFormat_R16G16B16A16_UNORM:
        return MTLVertexFormatUShort4Normalized;

    case TinyImageFormat_R16G16_SNORM:
        return MTLVertexFormatShort2Normalized;
    case TinyImageFormat_R16G16B16_SNORM:
        return MTLVertexFormatShort3Normalized;
    case TinyImageFormat_R16G16B16A16_SNORM:
        return MTLVertexFormatShort4Normalized;

    case TinyImageFormat_R16G16_SINT:
        return MTLVertexFormatShort2;
    case TinyImageFormat_R16G16B16_SINT:
        return MTLVertexFormatShort3;
    case TinyImageFormat_R16G16B16A16_SINT:
        return MTLVertexFormatShort4;

    case TinyImageFormat_R16G16_UINT:
        return MTLVertexFormatUShort2;
    case TinyImageFormat_R16G16B16_UINT:
        return MTLVertexFormatUShort3;
    case TinyImageFormat_R16G16B16A16_UINT:
        return MTLVertexFormatUShort4;

    case TinyImageFormat_R16G16_SFLOAT:
        return MTLVertexFormatHalf2;
    case TinyImageFormat_R16G16B16_SFLOAT:
        return MTLVertexFormatHalf3;
    case TinyImageFormat_R16G16B16A16_SFLOAT:
        return MTLVertexFormatHalf4;

    case TinyImageFormat_R32_SFLOAT:
        return MTLVertexFormatFloat;
    case TinyImageFormat_R32G32_SFLOAT:
        return MTLVertexFormatFloat2;
    case TinyImageFormat_R32G32B32_SFLOAT:
        return MTLVertexFormatFloat3;
    case TinyImageFormat_R32G32B32A32_SFLOAT:
        return MTLVertexFormatFloat4;

    case TinyImageFormat_R32_SINT:
        return MTLVertexFormatInt;
    case TinyImageFormat_R32G32_SINT:
        return MTLVertexFormatInt2;
    case TinyImageFormat_R32G32B32_SINT:
        return MTLVertexFormatInt3;
    case TinyImageFormat_R32G32B32A32_SINT:
        return MTLVertexFormatInt4;

    case TinyImageFormat_R10G10B10A2_SNORM:
        return MTLVertexFormatInt1010102Normalized;
    case TinyImageFormat_R10G10B10A2_UNORM:
        return MTLVertexFormatUInt1010102Normalized;

    case TinyImageFormat_R32_UINT:
        return MTLVertexFormatUInt;
    case TinyImageFormat_R32G32_UINT:
        return MTLVertexFormatUInt2;
    case TinyImageFormat_R32G32B32_UINT:
        return MTLVertexFormatUInt3;
    case TinyImageFormat_R32G32B32A32_UINT:
        return MTLVertexFormatUInt4;

        // TODO add this UINT + UNORM format to TinyImageFormat
        //		case TinyImageFormat_RGB10A2: return MTLVertexFormatUInt1010102Normalized;
    default:
        break;
    }
    switch (format)
    {
    case TinyImageFormat_R8_UINT:
        return MTLVertexFormatUChar;
    case TinyImageFormat_R8_SINT:
        return MTLVertexFormatChar;
    case TinyImageFormat_R8_UNORM:
        return MTLVertexFormatUCharNormalized;
    case TinyImageFormat_R8_SNORM:
        return MTLVertexFormatCharNormalized;
    case TinyImageFormat_R16_UNORM:
        return MTLVertexFormatUShortNormalized;
    case TinyImageFormat_R16_SNORM:
        return MTLVertexFormatShortNormalized;
    case TinyImageFormat_R16_SINT:
        return MTLVertexFormatShort;
    case TinyImageFormat_R16_UINT:
        return MTLVertexFormatUShort;
    case TinyImageFormat_R16_SFLOAT:
        return MTLVertexFormatHalf;
    default:
        break;
    }

    LOGF(LogLevel::eERROR, "Unknown vertex format: %d", format);
    return MTLVertexFormatInvalid;
}

MTLSamplePosition util_to_mtl_locations(SampleLocations location)
{
    MTLSamplePosition result = { location.mX / 16.f + 0.5f, location.mY / 16.f + 0.5f };
    return result;
}

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier)
{
    const bool barrierRequired(pCmd->pQueue->mBarrierFlags);
    UNREF_PARAM(barrierRequired);

    if (pCmd->pRenderEncoder != nil)
    {
        ASSERT(pCmd->pComputeEncoder == nil && pCmd->pBlitEncoder == nil);

        if (barrierRequired || forceBarrier)
        {
            [pCmd->pRenderEncoder updateFence:pCmd->pQueue->pQueueFence afterStages:MTLRenderStageFragment];
            pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
        }

        [pCmd->pRenderEncoder endEncoding];
        pCmd->pRenderEncoder = nil;
    }

    if (pCmd->pComputeEncoder != nil)
    {
        ASSERT(pCmd->pRenderEncoder == nil && pCmd->pBlitEncoder == nil);

        if (barrierRequired || forceBarrier)
        {
            [pCmd->pComputeEncoder updateFence:pCmd->pQueue->pQueueFence];
            pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
        }

        [pCmd->pComputeEncoder endEncoding];
        pCmd->pComputeEncoder = nil;
    }

    if (pCmd->pBlitEncoder != nil)
    {
        ASSERT(pCmd->pRenderEncoder == nil && pCmd->pComputeEncoder == nil);

        if (barrierRequired || forceBarrier)
        {
            [pCmd->pBlitEncoder updateFence:pCmd->pQueue->pQueueFence];
            pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
        }

        [pCmd->pBlitEncoder endEncoding];
        pCmd->pBlitEncoder = nil;
    }

#if defined(MTL_RAYTRACING_AVAILABLE)
    if (MTL_RAYTRACING_SUPPORTED)
    {
        if (pCmd->pASEncoder != nil)
        {
            ASSERT(pCmd->pRenderEncoder == nil && pCmd->pComputeEncoder == nil && pCmd->pBlitEncoder == nil);

            if (barrierRequired || forceBarrier)
            {
                [pCmd->pASEncoder updateFence:pCmd->pQueue->pQueueFence];
                pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
            }

            [pCmd->pASEncoder endEncoding];
            pCmd->pASEncoder = nil;
        }
    }
#endif
}

void util_barrier_required(Cmd* pCmd, const QueueType& encoderType)
{
    if (pCmd->pQueue->mBarrierFlags)
    {
        bool issuedWait = false;
        if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
        {
#if defined(MTL_RAYTRACING_AVAILABLE)
            if (MTL_RAYTRACING_SUPPORTED)
            {
                if (pCmd->pASEncoder != nil)
                {
                    [pCmd->pASEncoder waitForFence:pCmd->pQueue->pQueueFence];
                    issuedWait = true;
                }
            }
#endif
            if (!issuedWait)
            {
                switch (encoderType)
                {
                case QUEUE_TYPE_GRAPHICS:
                    [pCmd->pRenderEncoder waitForFence:pCmd->pQueue->pQueueFence beforeStages:MTLRenderStageVertex];
                    break;
                case QUEUE_TYPE_COMPUTE:
                    [pCmd->pComputeEncoder waitForFence:pCmd->pQueue->pQueueFence];
                    break;
                case QUEUE_TYPE_TRANSFER:
                    [pCmd->pBlitEncoder waitForFence:pCmd->pQueue->pQueueFence];
                    break;
                default:
                    ASSERT(false);
                }
            }
        }
        else
        {
            switch (encoderType)
            {
            case QUEUE_TYPE_GRAPHICS:
            {
#if defined(ENABLE_MEMORY_BARRIERS_GRAPHICS)
                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
                {
                    [pCmd->pRenderEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers
                                                     afterStages:MTLRenderStageFragment
                                                    beforeStages:MTLRenderStageVertex];
                }

                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
                {
                    [pCmd->pRenderEncoder memoryBarrierWithScope:MTLBarrierScopeTextures
                                                     afterStages:MTLRenderStageFragment
                                                    beforeStages:MTLRenderStageVertex];
                }

                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_RENDERTARGETS)
                {
                    [pCmd->pRenderEncoder memoryBarrierWithScope:MTLBarrierScopeRenderTargets
                                                     afterStages:MTLRenderStageFragment
                                                    beforeStages:MTLRenderStageVertex];
                }
#endif
            }
            break;

            case QUEUE_TYPE_COMPUTE:
            {
                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
                {
                    [pCmd->pComputeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
                }

                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
                {
                    [pCmd->pComputeEncoder memoryBarrierWithScope:MTLBarrierScopeTextures];
                }
            }
            break;

            case QUEUE_TYPE_TRANSFER:
                // we cant use barriers with blit encoder, only fence if available
                if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
                {
                    [pCmd->pBlitEncoder waitForFence:pCmd->pQueue->pQueueFence];
                }
                break;

            default:
                ASSERT(false);
            }
        }

        pCmd->pQueue->mBarrierFlags = 0;
    }
}

void util_set_debug_group(Cmd* pCmd)
{
#ifdef ENABLE_GRAPHICS_DEBUG
    if (pCmd->mDebugMarker[0] == '\0')
    {
        return;
    }
    if (nil != pCmd->pRenderEncoder)
    {
        [pCmd->pRenderEncoder pushDebugGroup:[NSString stringWithUTF8String:pCmd->mDebugMarker]];
    }
    else if (nil != pCmd->pComputeEncoder)
    {
        [pCmd->pComputeEncoder pushDebugGroup:[NSString stringWithUTF8String:pCmd->mDebugMarker]];
    }
    if (nil != pCmd->pBlitEncoder)
    {
        [pCmd->pBlitEncoder pushDebugGroup:[NSString stringWithUTF8String:pCmd->mDebugMarker]];
    }
#endif
}

void util_unset_debug_group(Cmd* pCmd)
{
#ifdef ENABLE_GRAPHICS_DEBUG
    pCmd->mDebugMarker[0] = '\0';
    if (nil != pCmd->pRenderEncoder)
    {
        [pCmd->pRenderEncoder popDebugGroup];
    }
    else if (nil != pCmd->pComputeEncoder)
    {
        [pCmd->pComputeEncoder popDebugGroup];
    }
    if (nil != pCmd->pBlitEncoder)
    {
        [pCmd->pBlitEncoder popDebugGroup];
    }
#endif
}

id<MTLCounterSet> util_get_counterset(id<MTLDevice> device)
{
    if (IOS14_RUNTIME)
    {
        if (![device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
        {
            LOGF(eINFO, "Timestamp sampling not supported at Stage Boundaries.");
            return nil;
        }

        id<MTLCounterSet> pCounterSet = nil;
        for (id<MTLCounterSet> set in device.counterSets)
        {
            if ([MTLCommonCounterSetTimestamp isEqualToString:set.name])
            {
                pCounterSet = set;
            }
        }
        if (!pCounterSet)
            return nil;
        for (id<MTLCounter> counter in pCounterSet.counters)
        {
            if ([MTLCommonCounterTimestamp isEqualToString:counter.name])
            {
                return pCounterSet;
            }
        }
    }

    return nil;
}

MTLCounterResultTimestamp* util_resolve_counter_sample_buffer(uint32_t startSample, uint32_t sampleCount,
                                                              id<MTLCounterSampleBuffer> pSampleBuffer)
{
    NSRange range = NSMakeRange(startSample, sampleCount);

    // Convert the contents of the counter sample buffer into the standard data format.
    const NSData* data = [pSampleBuffer resolveCounterRange:range];
    if (nil == data)
    {
        return nil;
    }

    const NSUInteger resolvedSampleCount = data.length / sizeof(MTLCounterResultTimestamp);
    if (resolvedSampleCount < sampleCount)
    {
        LOGF(eWARNING, "Only %u out of %u timestamps resolved.", resolvedSampleCount, sampleCount);
        return nil;
    }

    // Cast the data's bytes property to the counter's result type.
    return (MTLCounterResultTimestamp*)(data.bytes);
}

void initialize_texture_desc(Renderer* pRenderer, const TextureDesc* pDesc, const bool isRT, MTLPixelFormat pixelFormat,
                             MTLTextureDescriptor* textureDesc)
{
    const MemoryType memoryType = isRT ? MEMORY_TYPE_GPU_ONLY_COLOR_RTS : MEMORY_TYPE_GPU_ONLY;
    const bool       isDepthBuffer = TinyImageFormat_HasDepth(pDesc->mFormat) || TinyImageFormat_HasStencil(pDesc->mFormat);

    uint32_t mipLevels = pDesc->mMipLevels;
    if (!(pDesc->mFlags & (TEXTURE_CREATION_FLAG_FORCE_2D | TEXTURE_CREATION_FLAG_FORCE_3D)) && pDesc->mHeight == 1)
        mipLevels = 1;

    textureDesc.pixelFormat = pixelFormat;
    textureDesc.width = pDesc->mWidth;
    textureDesc.height = pDesc->mHeight;
    textureDesc.depth = pDesc->mDepth;
    textureDesc.mipmapLevelCount = mipLevels;
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
            else
            {
                ASSERT((bool)pRenderer->pGpu->mSettings.mCubeMapTextureArraySupported);
                textureDesc.textureType = MTLTextureTypeCubeArray;
                textureDesc.arrayLength /= 6;
            }
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
    default:
        break;
    }

    if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_TEXTURE) != 0)
    {
        textureDesc.usage |= MTLTextureUsageShaderRead;
        if (TinyImageFormat_IsDepthAndStencil(pDesc->mFormat))
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
}

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT)
{
    ASSERT(ppTexture);

    if (!(pDesc->mFlags & (TEXTURE_CREATION_FLAG_FORCE_2D | TEXTURE_CREATION_FLAG_FORCE_3D)) && pDesc->mHeight == 1)
        ((TextureDesc*)pDesc)->mMipLevels = 1;

    size_t totalSize = sizeof(Texture);
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
        totalSize += pDesc->mMipLevels * sizeof(id<MTLTexture>);

    Texture* pTexture = (Texture*)tf_calloc_memalign(1, alignof(Texture), totalSize);
    ASSERT(pTexture);

    void* mem = (pTexture + 1);

    const MemoryType memoryType = isRT ? MEMORY_TYPE_GPU_ONLY_COLOR_RTS : MEMORY_TYPE_GPU_ONLY;

    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
        pTexture->pUAVDescriptors = (id<MTLTexture> __strong*)mem;

    MTLPixelFormat pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mFormat);

    if (pDesc->mFormat == TinyImageFormat_D24_UNORM_S8_UINT &&
        !(pRenderer->pGpu->mCapBits.mFormatCaps[pDesc->mFormat] & FORMAT_CAP_RENDER_TARGET))
    {
        internal_log(eWARNING, "Format D24S8 is not supported on this device. Using D32S8 instead", "addTexture");
        pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
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
            id<MTLTexture>   pTexture;
            CVPixelBufferRef pPixelBuffer;
        };
        MetalNativeTextureHandle* handle = (MetalNativeTextureHandle*)pDesc->pNativeHandle;
        if (handle->pTexture)
        {
            pTexture->pTexture = handle->pTexture;
        }
        else if (handle->pPixelBuffer)
        {
            pTexture->pTexture = CVMetalTextureGetTexture(handle->pPixelBuffer);
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
        initialize_texture_desc(pRenderer, pDesc, isRT, pixelFormat, textureDesc);

        // For memoryless textures, we dont need any backing memory
#if defined(ENABLE_MEMORYLESS_TEXTURES)
        if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
        {
            textureDesc.resourceOptions = MTLResourceStorageModeMemoryless;

            pTexture->pTexture = [pRenderer->pDevice newTextureWithDescriptor:textureDesc];
            pTexture->mLazilyAllocated = true;
            ASSERT(pTexture->pTexture);
#if defined(ENABLE_GRAPHICS_DEBUG)
            setTextureName(pRenderer, pTexture, "Memoryless Texture");
#endif
        }
#endif

        if (pDesc->pPlacement && pRenderer->pGpu->mSettings.mPlacementHeaps)
        {
            pTexture->pTexture = [pDesc->pPlacement->pHeap->pHeap newTextureWithDescriptor:textureDesc offset:pDesc->pPlacement->mOffset];
        }

        if (!pTexture->pTexture)
        {
            if (pRenderer->pGpu->mSettings.mHeaps)
            {
                MTLSizeAndAlign sizeAlign = [pRenderer->pDevice heapTextureSizeAndAlignWithDescriptor:textureDesc];
                bool            useHeapPlacementHeaps = false;

                // Need to check mPlacementHeaps since it depends on gpu family as well as macOS/iOS version
                if (pRenderer->pGpu->mSettings.mPlacementHeaps)
                {
                    VmaAllocationInfo allocInfo = util_render_alloc(pRenderer, RESOURCE_MEMORY_USAGE_GPU_ONLY, memoryType, sizeAlign.size,
                                                                    sizeAlign.align, &pTexture->pAllocation);

                    pTexture->pTexture = [allocInfo.deviceMemory->pHeap newTextureWithDescriptor:textureDesc offset:allocInfo.offset];

                    useHeapPlacementHeaps = true;
                }

                if (!useHeapPlacementHeaps)
                {
                    // If placement heaps are not supported we cannot use VMA
                    // Instead we have to rely on MTLHeap automatic placement
                    uint32_t heapIndex = util_find_heap_with_space(pRenderer, memoryType, sizeAlign);

                    pTexture->pTexture = [pRenderer->pHeaps[heapIndex] newTextureWithDescriptor:textureDesc];
                }
            }
            else
            {
                pTexture->pTexture = [pRenderer->pDevice newTextureWithDescriptor:textureDesc];
            }

            ASSERT(pTexture->pTexture);

            if (TinyImageFormat_IsDepthAndStencil(pDesc->mFormat))
            {
#ifndef TARGET_IOS
                pTexture->pStencilTexture = [pTexture->pTexture
                    newTextureViewWithPixelFormat:(pixelFormat == MTLPixelFormatDepth32Float_Stencil8 ? MTLPixelFormatX32_Stencil8
                                                                                                      : MTLPixelFormatX24_Stencil8)];
#else
                pTexture->pStencilTexture = [pTexture->pTexture newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8];
#endif
                ASSERT(pTexture->pStencilTexture);
            }
        }
    }

    NSRange slices = NSMakeRange(0, pDesc->mArraySize);

    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        MTLTextureType uavType = pTexture->pTexture.textureType;
        if (pTexture->pTexture.textureType == MTLTextureTypeCube)
        {
            uavType = MTLTextureType2DArray;
        }
        else
        {
            if (pTexture->pTexture.textureType == MTLTextureTypeCubeArray)
            {
                uavType = MTLTextureType2DArray;
            }
        }

        for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
        {
            NSRange levels = NSMakeRange(i, 1);
            pTexture->pUAVDescriptors[i] = [pTexture->pTexture newTextureViewWithPixelFormat:pTexture->pTexture.pixelFormat
                                                                                 textureType:uavType
                                                                                      levels:levels
                                                                                      slices:slices];
        }
    }

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

/************************************************************************/
/************************************************************************/
#endif // RENDERER_IMPLEMENTATION

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
    addResourceHeap = mtl_addResourceHeap;
    removeResourceHeap = mtl_removeResourceHeap;
    getBufferSizeAlign = mtl_getBufferSizeAlign;
    getTextureSizeAlign = mtl_getTextureSizeAlign;
    addBuffer = mtl_addBuffer;
    removeBuffer = mtl_removeBuffer;
    mapBuffer = mtl_mapBuffer;
    unmapBuffer = mtl_unmapBuffer;
    cmdUpdateBuffer = mtl_cmdUpdateBuffer;
    cmdUpdateSubresource = mtl_cmdUpdateSubresource;
    cmdCopySubresource = mtl_cmdCopySubresource;
    addTexture = mtl_addTexture;
    removeTexture = mtl_removeTexture;

    // shader functions
    addShaderBinary = mtl_addShaderBinary;
    removeShader = mtl_removeShader;

    addRootSignature = mtl_addRootSignature;
    removeRootSignature = mtl_removeRootSignature;
    getDescriptorIndexFromName = mtl_getDescriptorIndexFromName;

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
    // Note: setSamplePositions in Metal is incosistent with other APIs: it cannot be called inside the renderpass
    cmdSetSampleLocations = mtl_cmdSetSampleLocations;
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

    // queue/fence/swapchain functions
    acquireNextImage = mtl_acquireNextImage;
    queueSubmit = mtl_queueSubmit;
    queuePresent = mtl_queuePresent;
    waitQueueIdle = mtl_waitQueueIdle;
    getFenceStatus = mtl_getFenceStatus;
    waitForFences = mtl_waitForFences;
    toggleVSync = mtl_toggleVSync;

    getSupportedSwapchainFormat = mtl_getSupportedSwapchainFormat;
    getRecommendedSwapchainImageCount = mtl_getRecommendedSwapchainImageCount;

    // indirect Draw functions
    addIndirectCommandSignature = mtl_addIndirectCommandSignature;
    removeIndirectCommandSignature = mtl_removeIndirectCommandSignature;
    cmdExecuteIndirect = mtl_cmdExecuteIndirect;

    /************************************************************************/
    // GPU Query Interface
    /************************************************************************/
    getTimestampFrequency = mtl_getTimestampFrequency;
    addQueryPool = mtl_addQueryPool;
    removeQueryPool = mtl_removeQueryPool;
    cmdBeginQuery = mtl_cmdBeginQuery;
    cmdEndQuery = mtl_cmdEndQuery;
    cmdResolveQuery = mtl_cmdResolveQuery;
    cmdResetQuery = mtl_cmdResetQuery;
    getQueryData = mtl_getQueryData;
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
} // namespace RENDERER_CPP_NAMESPACE
#endif
#endif
