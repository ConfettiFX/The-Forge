/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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
#define MAX_BUFFER_BINDINGS                        31
// Start vertex attribute bindings at index 30 and decrement so we can bind regular buffers from index 0 for simplicity
#define VERTEX_BINDING_OFFSET                      (MAX_BUFFER_BINDINGS - 1)
#define DESCRIPTOR_SET_ARGUMENT_BUFFER_START_INDEX 2

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

typedef struct QuerySampleDesc
{
    union
    {
        struct
        {
            // Sample start in sample buffer
            uint32_t mRenderStartIndex;
            // Number of samples from 'RenderStartIndex'
            uint32_t mRenderSamples;

            uint32_t mComputeStartIndex;
            uint32_t mComputeSamples;
        };
        bool mHasBeginSample;
    };
} QuerySampleDesc;

// MAX: 4
#define NUM_RENDER_STAGE_BOUNDARY_COUNTERS  4
#define NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS 2
#define NUM_DRAW_BOUNDARY_COUNTERS          2

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

void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);
void util_set_debug_group(Cmd* pCmd);
void util_unset_debug_group(Cmd* pCmd);

id<MTLCounterSet>          util_get_counterset(id<MTLDevice> device);
MTLCounterResultTimestamp* util_resolve_counter_sample_buffer(uint32_t startSample, uint32_t sampleCount,
                                                              id<MTLCounterSampleBuffer> pSampleBuffer);
void                       util_update_gpu_to_cpu_timestamp_factor(Renderer* pRenderer);
// GPU frame time accessor for macOS and iOS
#define GPU_FREQUENCY 1000000000.0 // nanoseconds

extern "C"
{
    void getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
    void getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
    void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
    void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
    void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
    void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
    void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
    void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const struct SubresourceDataDesc* pSubresourceDesc);
    void cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const struct SubresourceDataDesc* pSubresourceDesc);
    void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
    void removeTexture(Renderer* pRenderer, Texture* pTexture);
}

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
    uint32_t    mBinding : 25;
    uint32_t    mNonABResource : 1;
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
void util_bind_root_buffer(Cmd* pCmd, const RootDescriptorHandle* pHandle, uint32_t stages);

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mMaxSets);

    // if no pipeline is bound yet, mask this descriptor set to be bound when cmdBindPipeline is called
    if (pCmd->pBoundPipeline == NULL)
    {
        pCmd->mBoundDescriptorSets[pDescriptorSet->mSetIndex] = pDescriptorSet;
        pCmd->mBoundDescriptorSetIndices[pDescriptorSet->mSetIndex] = index;
        pCmd->mShouldRebindDescriptorSetsMask |= 0x01 << pDescriptorSet->mSetIndex;
        return;
    }
    uint8_t stages = (pCmd->pBoundPipeline->mType == PIPELINE_TYPE_COMPUTE) ? SHADER_STAGE_COMP : SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
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
            else if (pHandle->mNonABResource)
            {
                util_bind_root_buffer(pCmd, pHandle, stages);
            }
        }

        for (uint32_t i = 0; i < pDescriptorSet->mRootTextureCount; ++i)
        {
            const RootDescriptorHandle* pHandle = &pData->pTextures[i];
            if (pHandle->mNonABResource)
            {
                if (stages & SHADER_STAGE_VERT)
                {
                    [pCmd->pRenderEncoder setVertexTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
                if (stages & SHADER_STAGE_FRAG)
                {
                    [pCmd->pRenderEncoder setFragmentTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
                if (stages & SHADER_STAGE_COMP)
                {
                    [pCmd->pComputeEncoder setTextures:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
            }
        }

        for (uint32_t i = 0; i < pDescriptorSet->mRootSamplerCount; ++i)
        {
            const RootDescriptorHandle* pHandle = &pData->pSamplers[i];
            if (pHandle->mNonABResource)
            {
                if (stages & SHADER_STAGE_VERT)
                {
                    [pCmd->pRenderEncoder setVertexSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
                if (stages & SHADER_STAGE_FRAG)
                {
                    [pCmd->pRenderEncoder setFragmentSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
                if (stages & SHADER_STAGE_COMP)
                {
                    [pCmd->pComputeEncoder setSamplerStates:pHandle->pArr withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
                }
            }
        }
    }

    if (pDescriptorSet->mArgumentBuffer)
    {
        const id<MTLBuffer> buffer = pDescriptorSet->mArgumentBuffer->pBuffer;
        const uint64_t      offset = pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride;

        // argument buffers
        if (stages & SHADER_STAGE_VERT)
        {
            [pCmd->pRenderEncoder setVertexBuffer:buffer offset:offset atIndex:pDescriptorSet->mArgumentBufferIndex];
        }

        if (stages & SHADER_STAGE_FRAG)
        {
            [pCmd->pRenderEncoder setFragmentBuffer:buffer offset:offset atIndex:pDescriptorSet->mArgumentBufferIndex];
        }

        if (stages & SHADER_STAGE_COMP)
        {
            [pCmd->pComputeEncoder setBuffer:buffer offset:offset atIndex:pDescriptorSet->mArgumentBufferIndex];
        }

        // useResource on the untracked resources (UAVs, RTs)
        // useHeap doc
        // You may only read or sample resources within the specified heaps. This method ignores render targets (textures that specify a
        // MTLTextureUsageRenderTarget usage option) and writable textures (textures that specify a MTLTextureUsageShaderWrite usage option)
        // within the array of heaps. To use these resources, you must call the useResource:usage:stages: method instead.
        if (pDescriptorSet->ppUntrackedData[index])
        {
            const UntrackedResourceData* untracked = pDescriptorSet->ppUntrackedData[index];
            if (stages & SHADER_STAGE_COMP)
            {
                if (untracked->mData.mCount)
                {
                    [pCmd->pComputeEncoder useResources:untracked->mData.pResources
                                                  count:untracked->mData.mCount
                                                  usage:MTLResourceUsageRead];
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
                    [pCmd->pRenderEncoder useResources:untracked->mData.pResources
                                                 count:untracked->mData.mCount
                                                 usage:MTLResourceUsageRead];
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
}

//
// DescriptorSet
//

void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);
    ASSERT(pDesc->mDescriptorCount > 0);

    uint32_t totalSize = sizeof(DescriptorSet);
    uint32_t rootTextureCount = 0;
    uint32_t rootBufferCount = 0;
    uint32_t rootSamplerCount = 0;
    uint32_t bindingsCount = pDesc->mDescriptorCount;
    uint32_t abResourcesCount = 0;

    for (uint32_t i = 0; i < pDesc->mDescriptorCount; ++i)
    {
        const Descriptor* pResourceDesc = &pDesc->pDescriptors[i];
        if (pResourceDesc->mUseArgumentBuffer || pDesc->mForceArgumentBuffer)
        {
            abResourcesCount++;
        }
        switch (pResourceDesc->mType)
        {
        case DESCRIPTOR_TYPE_SAMPLER:
            rootSamplerCount++;
            break;
        case DESCRIPTOR_TYPE_RW_TEXTURE:
        case DESCRIPTOR_TYPE_TEXTURE:
            rootTextureCount += pResourceDesc->mCount;
            break;
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
        case DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER:
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
            rootBufferCount += pResourceDesc->mCount;
            break;
        default:
            break;
        }
    }

    uint32_t totalDescriptorsCount = rootSamplerCount + rootTextureCount + rootBufferCount;
    totalSize += pDesc->mMaxSets * sizeof(RootDescriptorData);
    totalSize += pDesc->mMaxSets * totalDescriptorsCount * sizeof(RootDescriptorHandle);
    totalSize += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
    totalSize += sizeof(uint32_t) * bindingsCount * 2;

    DescriptorSet* pDescriptorSet = (DescriptorSet*)tf_calloc_memalign(1, alignof(DescriptorSet), totalSize);
    ASSERT(pDescriptorSet);

    pDescriptorSet->pDescriptors = pDesc->pDescriptors;
    pDescriptorSet->mNodeIndex = pDesc->mNodeIndex;
    // 0 and 1 are reserved for cmdWriteMarker
    pDescriptorSet->mArgumentBufferIndex = pDesc->mIndex + DESCRIPTOR_SET_ARGUMENT_BUFFER_START_INDEX;
    pDescriptorSet->mSetIndex = pDesc->mSetIndex;
    pDescriptorSet->mMaxSets = pDesc->mMaxSets;
    pDescriptorSet->mForceArgumentBuffer = pDesc->mForceArgumentBuffer;

    uint8_t* mem = (uint8_t*)(pDescriptorSet + 1);
    pDescriptorSet->pRootDescriptorData = (RootDescriptorData*)mem;
    mem += pDesc->mMaxSets * sizeof(RootDescriptorData);

    for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
    {
        pDescriptorSet->pRootDescriptorData[i].pTextures = (RootDescriptorHandle*)mem;
        mem += rootTextureCount * sizeof(RootDescriptorHandle);
        pDescriptorSet->pRootDescriptorData[i].pBuffers = (RootDescriptorHandle*)mem;
        mem += rootBufferCount * sizeof(RootDescriptorHandle);
        pDescriptorSet->pRootDescriptorData[i].pSamplers = (RootDescriptorHandle*)mem;
        mem += rootSamplerCount * sizeof(RootDescriptorHandle);
    }
    pDescriptorSet->ppUntrackedData = (UntrackedResourceData**)mem;
    mem += pDesc->mMaxSets * sizeof(UntrackedResourceData*);
    pDescriptorSet->pBindings = (uint32_t*)mem;
    mem += bindingsCount * sizeof(uint32_t);
    pDescriptorSet->pResourceIndices = (uint32_t*)mem;

    pDescriptorSet->mRootTextureCount = rootTextureCount;
    pDescriptorSet->mRootBufferCount = rootBufferCount;
    pDescriptorSet->mRootSamplerCount = rootSamplerCount;

    if (abResourcesCount > 0)
    {
        NSMutableArray<MTLArgumentDescriptor*>* argumentDescriptors = [[NSMutableArray alloc] init];
        uint32_t                                argumentMemberIndex = 0;
        for (uint32_t i = 0; i < pDesc->mDescriptorCount; ++i)
        {
            const Descriptor* desc = &pDesc->pDescriptors[i];
            if ((desc->mUseArgumentBuffer == 0) && (pDesc->mForceArgumentBuffer == 0))
            {
                continue;
            }
            MTLArgumentDescriptor* argDescriptor = [[MTLArgumentDescriptor alloc] init];
            argDescriptor.textureType = MTLTextureType2D;
            argDescriptor.access = MTLArgumentAccessReadOnly;

            switch (desc->mType)
            {
            case DESCRIPTOR_TYPE_SAMPLER:
                argDescriptor.dataType = MTLDataTypeSampler;
                argDescriptor.index = argumentMemberIndex;
                break;

            case DESCRIPTOR_TYPE_TEXTURE:
            case DESCRIPTOR_TYPE_RW_TEXTURE:
                argDescriptor.dataType = MTLDataTypeTexture;
                argDescriptor.arrayLength = desc->mCount;
                argDescriptor.index = argumentMemberIndex;
                break;

            case DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER:
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                argDescriptor.dataType = MTLDataTypePointer;
                argDescriptor.arrayLength = desc->mCount;
                argDescriptor.index = argumentMemberIndex;
                break;
            case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
                if (IOS17_RUNTIME)
                {
                    argDescriptor.dataType = MTLDataTypeInstanceAccelerationStructure;
                    argDescriptor.arrayLength = desc->mCount;
                    argDescriptor.index = argumentMemberIndex;
                    break;
                }

            default:
                ASSERT(false); // unsupported descriptor type
                break;
            }
            argumentMemberIndex += desc->mCount;
            [argumentDescriptors addObject:argDescriptor];
        }
#ifdef ENABLE_GRAPHICS_VALIDATION
        // to circumvent a metal validation bug which overwrites the arguments array, we make a local copy
        NSMutableArray<MTLArgumentDescriptor*>* descriptorsCopy = [[NSMutableArray alloc] init];
        for (MTLArgumentDescriptor* myArrayElement in argumentDescriptors)
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
        argumentDescriptors = descriptorsCopy;
#endif

        // create encoder
        pDescriptorSet->mArgumentEncoder = [pRenderer->pDevice newArgumentEncoderWithArguments:argumentDescriptors];
        ASSERT(pDescriptorSet->mArgumentEncoder);

        // Create argument buffer
        uint32_t   argumentBufferSize = (uint32_t)round_up_64(pDescriptorSet->mArgumentEncoder.encodedLength, 256);
        BufferDesc bufferDesc = {};
        bufferDesc.mAlignment = 256;
        bufferDesc.mSize = argumentBufferSize * pDesc->mMaxSets;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
        char debugName[FS_MAX_PATH] = {};
        snprintf(debugName, sizeof debugName, "Argument Buffer %u", pDesc->mIndex);
        bufferDesc.pName = debugName;
#endif
        addBuffer(pRenderer, &bufferDesc, &pDescriptorSet->mArgumentBuffer);
        pDescriptorSet->mStride = argumentBufferSize;

        for (uint32_t index = 0; index < pDescriptorSet->mMaxSets; ++index)
        {
            [pDescriptorSet->mArgumentEncoder setArgumentBuffer:pDescriptorSet->mArgumentBuffer->pBuffer
                                                         offset:pDescriptorSet->mArgumentBuffer->mOffset + index * pDescriptorSet->mStride];
        }
    }
    uint32_t textureBindingIndex = METAL_TEXTURE_BIND_START_INDEX;
    uint32_t bufferBindingIndex = METAL_BUFFER_BIND_START_INDEX;
    uint32_t samplerBindingIndex = 0;
    uint32_t abMemberIndex = 0;
    uint32_t samplerResIndex = 0;
    uint32_t textureResIndex = 0;
    uint32_t bufferResIndex = 0;

    // for devices with limited AB support : aka iPhone7 :
    // extract the correct texture and buffer binding points here
    // only when there is no support for argument buffers
    MetalDescriptorSet* pSrtSets = (MetalDescriptorSet*)pDesc->pSrtSets;
    if (pDesc->mIndex > 0)
    {
        for (uint32_t i = 0; i < pDesc->mIndex; i++)
        {
            MetalDescriptorSet* currSet = pSrtSets + i;
            for (uint32_t j = 0; j < currSet->mDescriptorCount; j++)
            {
                Descriptor* pCurrDesc = currSet->pDescriptors + j;
                // skip AB
                if (pCurrDesc->mUseArgumentBuffer || pCurrDesc->mCount > 1)
                {
                    continue;
                }
                if (pCurrDesc->mType == DESCRIPTOR_TYPE_TEXTURE || pCurrDesc->mType == DESCRIPTOR_TYPE_RW_TEXTURE)
                {
                    textureBindingIndex++;
                }
                if (pCurrDesc->mType == DESCRIPTOR_TYPE_BUFFER || pCurrDesc->mType == DESCRIPTOR_TYPE_BUFFER_RAW ||
                    pCurrDesc->mType == DESCRIPTOR_TYPE_RW_BUFFER || pCurrDesc->mType == DESCRIPTOR_TYPE_RW_BUFFER_RAW ||
                    pCurrDesc->mType == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    bufferBindingIndex++;
                }
            }
        }
    }

    for (uint32_t i = 0; i < pDesc->mDescriptorCount; ++i)
    {
        const Descriptor* desc = &pDesc->pDescriptors[i];
        const uint32_t    arrayCount = desc->mCount;
        bool              useArgumentBuffer = desc->mUseArgumentBuffer || pDesc->mForceArgumentBuffer;
        switch (desc->mType)
        {
        case DESCRIPTOR_TYPE_SAMPLER:
        case DESCRIPTOR_TYPE_TEXTURE:
        case DESCRIPTOR_TYPE_RW_TEXTURE:
            if (useArgumentBuffer)
            {
                pDescriptorSet->pBindings[i] = abMemberIndex;
                abMemberIndex += arrayCount;
                if (desc->mType == DESCRIPTOR_TYPE_SAMPLER)
                {
                    pDescriptorSet->pResourceIndices[i] = samplerResIndex;
                    samplerResIndex += arrayCount;
                }
                else
                {
                    pDescriptorSet->pResourceIndices[i] = textureResIndex;
                    textureResIndex += arrayCount;
                }
            }
            else
            {
                if (desc->mType == DESCRIPTOR_TYPE_SAMPLER)
                {
                    pDescriptorSet->pBindings[i] = samplerBindingIndex;
                    samplerBindingIndex += arrayCount;
                    pDescriptorSet->pResourceIndices[i] = samplerResIndex;
                    samplerResIndex += arrayCount;
                }
                else
                {
                    pDescriptorSet->pBindings[i] = textureBindingIndex;
                    textureBindingIndex += arrayCount;
                    pDescriptorSet->pResourceIndices[i] = textureResIndex;
                    textureResIndex += arrayCount;
                }
            }
            break;

        case DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER:
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER:
        case DESCRIPTOR_TYPE_BUFFER_RAW:
        case DESCRIPTOR_TYPE_RW_BUFFER:
        case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            if (useArgumentBuffer)
            {
                pDescriptorSet->pBindings[i] = abMemberIndex;
                abMemberIndex += arrayCount;
                pDescriptorSet->pResourceIndices[i] = bufferResIndex;
                bufferResIndex += arrayCount;
            }
            else
            {
                pDescriptorSet->pBindings[i] = bufferBindingIndex;
                bufferBindingIndex += arrayCount;
                pDescriptorSet->pResourceIndices[i] = bufferResIndex;
                bufferResIndex += arrayCount;
            }
            break;
        case DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE:
            if (IOS17_RUNTIME)
            {
                pDescriptorSet->pBindings[i] = abMemberIndex++;
                break;
            }

        default:
            pDescriptorSet->pBindings[i] = abMemberIndex++;
            break;
        }
    }

    uint32_t argumentMemberIndex = 0;

    for (uint32_t i = 0; i < pDesc->mDescriptorCount; ++i)
    {
        const Descriptor* desc = &pDesc->pDescriptors[i];
        {
            bool useArgumentBuffer = desc->mUseArgumentBuffer || pDesc->mForceArgumentBuffer;
            for (uint32_t index = 0; index < pDescriptorSet->mMaxSets; ++index)
            {
                const DescriptorType type((DescriptorType)desc->mType);
                const uint32_t       arrayStart = 0;
                const uint32_t       arrayCount = desc->mCount;
                switch (type)
                {
                case DESCRIPTOR_TYPE_SAMPLER:
                {
                    if (useArgumentBuffer)
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            [pDescriptorSet->mArgumentEncoder setSamplerState:pDefaultSampler->pSamplerState
                                                                      atIndex:argumentMemberIndex + arrayStart + j];
                        }
                    }
                    break;
                }
                case DESCRIPTOR_TYPE_TEXTURE:
                case DESCRIPTOR_TYPE_RW_TEXTURE:
                {
                    if (useArgumentBuffer)
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            // #TODO: desc->dim
                            const Texture* texture = pDefaultTextures[TEXTURE_DIM_2D];
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pTexture atIndex:argumentMemberIndex + arrayStart + j];
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
                    if (useArgumentBuffer)
                    {
                        if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                        {
                            if (pRenderer->pGpu->mIndirectCommandBuffer)
                            {
                                for (uint32_t j = 0; j < arrayCount; ++j)
                                {
                                    [pDescriptorSet->mArgumentEncoder setIndirectCommandBuffer:pDefaultICB->pIndirectCommandBuffer
                                                                                       atIndex:argumentMemberIndex + arrayStart + j];
                                }
                            }
                        }
                        else
                        {
                            for (uint32_t j = 0; j < arrayCount; ++j)
                            {
                                Buffer* buffer = pDefaultBuffer;
                                [pDescriptorSet->mArgumentEncoder setBuffer:buffer->pBuffer
                                                                     offset:j
                                                                    atIndex:argumentMemberIndex + arrayStart];
                            }
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
            if (useArgumentBuffer)
            {
                argumentMemberIndex++;
            }
        }
    }

    *ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
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

static void BindICBDescriptor(DescriptorSet* pDescriptorSet, uint32_t index, const Descriptor* pDesc, uint32_t arrayStart,
                              uint32_t arrayCount, const DescriptorData* pParam)
{
    for (uint32_t j = 0; j < arrayCount; ++j)
    {
        [pDescriptorSet->mArgumentEncoder setIndirectCommandBuffer:pParam->ppBuffers[j]->pIndirectCommandBuffer
                                                           atIndex:pDesc->mOffset + arrayStart + j];

        TrackUntrackedResource(pDescriptorSet, index, MTLResourceUsageWrite, pParam->ppBuffers[j]->pIndirectCommandBuffer);
    }
}

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->mMaxSets);

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
        const DescriptorData* pParam = pParams + i;
        const uint32_t        paramIndex = pParam->mIndex;
        const Descriptor*     pDesc = pDescriptorSet->pDescriptors + paramIndex;
        uint32_t              bindIndex = pDescriptorSet->pBindings[paramIndex];
        uint32_t              resIndex = pDescriptorSet->pResourceIndices[paramIndex];
        if (pDesc)
        {
            const DescriptorType type((DescriptorType)pDesc->mType);
            const uint32_t       arrayStart = pParam->mArrayOffset;
            const uint32_t       arrayCount(max(1U, pParam->mCount));

            bool useArgumentBuffer = pDesc->mUseArgumentBuffer || pDescriptorSet->mForceArgumentBuffer;

            switch (type)
            {
            case DESCRIPTOR_TYPE_SAMPLER:
            {
                if (useArgumentBuffer)
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        [pDescriptorSet->mArgumentEncoder setSamplerState:pParam->ppSamplers[j]->pSamplerState
                                                                  atIndex:bindIndex + arrayStart + j];
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pSamplers[resIndex];
                    pData->mBinding = bindIndex;
                    resize_roothandle(pData, arrayCount);
                    pData->mCount = arrayCount;
                    pData->mNonABResource = 1;
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        Sampler* sampler = pParam->ppSamplers[j];
                        pData->pArr[j] = sampler->pSamplerState;
                    }
                }
                break;
            }
            case DESCRIPTOR_TYPE_TEXTURE:
            {
                if (useArgumentBuffer)
                {
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        const Texture* texture = pParam->ppTextures[j];
                        NOREFS id<MTLTexture> untracked = nil;

                        if (pParam->mBindStencilResource)
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pStencilTexture atIndex:bindIndex + arrayStart + j];
                            untracked = texture->pStencilTexture;
                        }
                        else if (pParam->mUAVMipSlice)
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[pParam->mUAVMipSlice]
                                                                 atIndex:bindIndex + arrayStart + j];
                            untracked = texture->pUAVDescriptors[pParam->mUAVMipSlice];
                        }
                        else
                        {
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pTexture atIndex:bindIndex + arrayStart + j];
                            untracked = texture->pTexture;
                        }

                        bool isRT = texture->pTexture.usage & MTLTextureUsageRenderTarget;
                        if (isRT || texture->mUav || !texture->pAllocation)
                        {
                            // TODO: pDesc->mUsage?
                            MTLResourceUsage usage = MTLResourceUsageRead;

                            TrackUntrackedResource(pDescriptorSet, index, usage, untracked);
                        }
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[resIndex];

                    resize_roothandle(pData, arrayCount);
                    pData->mCount = arrayCount;
                    pData->mBinding = bindIndex;
                    pData->mNonABResource = 1;
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        Texture* texture = pParam->ppTextures[j];
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
                            pData->pArr[j] = texture->pTexture;
                        }
                    }
                }

                break;
            }
            case DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                if (useArgumentBuffer)
                {
                    if (pParam->mBindMipChain)
                    {
                        for (uint32_t j = 0; j < pParam->ppTextures[0]->mMipLevels; ++j)
                        {
                            const Texture* texture = pParam->ppTextures[0];
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[j] atIndex:bindIndex + arrayStart + j];
                            MTLResourceUsage usage = MTLResourceUsageRead | MTLResourceUsageWrite;
                            TrackUntrackedResource(pDescriptorSet, index, usage, texture->pUAVDescriptors[j]);
                        }
                    }
                    else
                    {
                        for (uint32_t j = 0; j < arrayCount; ++j)
                        {
                            const Texture* texture = pParam->ppTextures[j];
                            [pDescriptorSet->mArgumentEncoder setTexture:texture->pUAVDescriptors[pParam->mUAVMipSlice]
                                                                 atIndex:bindIndex + arrayStart + j];
                            MTLResourceUsage usage = MTLResourceUsageRead | MTLResourceUsageWrite;
                            TrackUntrackedResource(pDescriptorSet, index, usage, texture->pUAVDescriptors[pParam->mUAVMipSlice]);
                        }
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pTextures[resIndex];
                    pData->mBinding = bindIndex;
                    pData->mNonABResource = 1;
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
                if (useArgumentBuffer)
                {
                    if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                    {
                        if (pRenderer->pGpu->mIndirectCommandBuffer)
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
                            offset = offset + (pParam->pRanges ? pParam->pRanges[j].mOffset : 0);
                            uint64_t bufferIndex = bindIndex + arrayStart + j;

                            [pDescriptorSet->mArgumentEncoder setBuffer:buffer->pBuffer offset:offset atIndex:bufferIndex];

                            MTLResourceUsage usage = MTLResourceUsageRead;
                            if (type == DESCRIPTOR_TYPE_RW_BUFFER || type == DESCRIPTOR_TYPE_RW_BUFFER_RAW)
                            {
                                usage |= MTLResourceUsageWrite;
                            }

                            if ((usage & MTLResourceUsageWrite) || !buffer->pAllocation)
                            {
                                const Buffer* buffer = pParam->ppBuffers[j];
                                TrackUntrackedResource(pDescriptorSet, index, usage, buffer->pBuffer);
                            }
                        }

                        if (pRenderer->pGpu->mIndirectCommandBuffer && pParam->mBindICB)
                        {
                            if (pDesc)
                            {
                                BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
                            }
                        }
                    }
                }
                else
                {
                    RootDescriptorHandle* pData = &pDescriptorSet->pRootDescriptorData[index].pBuffers[resIndex];
                    pData->mBinding = bindIndex;
                    pData->mNonABResource = 1;
                    resize_roothandle(pData, arrayCount);
                    pData->mCount = arrayCount;
                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        pData->pArr[j] = pParam->ppBuffers[j]->pBuffer;
                        pData->pOffsets[j] = pParam->ppBuffers[j]->mOffset + (pParam->pRanges ? pParam->pRanges[j].mOffset : 0);
                    }

                    if (pRenderer->pGpu->mIndirectCommandBuffer)
                    {
                        if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                        {
                            BindICBDescriptor(pDescriptorSet, index, pDesc, arrayStart, arrayCount, pParam);
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
                    ASSERT(pParam->ppAccelerationStructures);

                    for (uint32_t j = 0; j < arrayCount; ++j)
                    {
                        ASSERT(pParam->ppAccelerationStructures[j]);
                        extern id<MTLAccelerationStructure> getMTLAccelerationStructure(AccelerationStructure*);
                        id                                  as = getMTLAccelerationStructure(pParam->ppAccelerationStructures[j]);
                        [pDescriptorSet->mArgumentEncoder setAccelerationStructure:as atIndex:bindIndex + arrayStart + j];

                        MTLResourceUsage usage = MTLResourceUsageRead;
                        TrackUntrackedResource(pDescriptorSet, index, usage, as);
                        extern void getMTLAccelerationStructureBottomReferences(AccelerationStructure * pAccelerationStructure,
                                                                                uint32_t * pOutReferenceCount, NOREFS id * *pOutReferences);
                        uint32_t    bottomRefCount = 0;
                        NOREFS id* bottomRefs = NULL;
                        // Mark primitive acceleration structures as used since only the instance acceleration structure references
                        // them.
                        getMTLAccelerationStructureBottomReferences(pParam->ppAccelerationStructures[j], &bottomRefCount, &bottomRefs);
                        for (uint32_t ref = 0; ref < bottomRefCount; ++ref)
                        {
                            TrackUntrackedResource(pDescriptorSet, index, usage, bottomRefs[ref]);
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
void logMemoryStats(Renderer* pRenderer) { vmaBuildStatsString(pRenderer->pVmaAllocator, VK_TRUE); }

void calculateMemoryUse(Renderer* pRenderer, uint64_t* usedBytes, uint64_t* totalAllocatedBytes)
{
    VmaTotalStatistics stats;
    vmaCalculateStatistics(pRenderer->pVmaAllocator, &stats);
    *usedBytes = stats.total.statistics.allocationBytes;
    *totalAllocatedBytes = stats.total.statistics.blockBytes;
}
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
    if (pRenderer->pGpu->mCubeMapTextureArraySupported)
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

TinyImageFormat getSupportedSwapchainFormat(Renderer* pRenderer, const SwapChainDesc* pDesc, ColorSpace colorSpace)
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

uint32_t getRecommendedSwapchainImageCount(Renderer*, const WindowHandle*) { return 3; }

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

uvec2 queryThreadExecutionWidthAndTotalThreads(id<MTLDevice> gpu)
{
    if (!gpu)
        return uvec2(0);

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
        return uvec2(0);
    }

    // Load the kernel function from the library
    id<MTLFunction> kernelFunction = [defaultLibrary newFunctionWithName:@"simplest"];

    // Create a compute pipeline state
    id<MTLComputePipelineState> computePipelineState = [gpu newComputePipelineStateWithFunction:kernelFunction error:&error];
    if (error != nil)
    {
        LOGF(LogLevel::eWARNING, "Could not create compute pipeline state for simple compute shader: %s", [error.description UTF8String]);
        return uvec2(0);
    }

    return uvec2((uint32_t)computePipelineState.threadExecutionWidth, (uint32_t)computePipelineState.maxTotalThreadsPerThreadgroup);
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

    pMemoryProperties->memoryHeaps[0].size = physicalDevice->pGpu->mVRAM;
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

    if (device->pGpu->mPlacementHeaps)
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

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

static void QueryGpuDesc(GpuDesc* pGpuDesc)
{
    id<MTLDevice> gpu = pGpuDesc->pGPU;

    setDefaultGPUProperties(pGpuDesc);
    GPUVendorPreset& gpuVendor = pGpuDesc->mGpuVendorPreset;
    FillGPUVendorPreset(gpu, gpuVendor);

#ifdef TARGET_IOS
#if defined(ENABLE_OS_PROC_MEMORY)
    pGpuDesc->mVRAM = os_proc_available_memory();
#endif

    if (pGpuDesc->mVRAM <= 0)
    {
        pGpuDesc->mVRAM = util_get_free_memory();
        if (pGpuDesc->mVRAM <= 0)
        {
            pGpuDesc->mVRAM = [[NSProcessInfo processInfo] physicalMemory];
        }
    }
#else
    pGpuDesc->mVRAM = [gpu recommendedMaxWorkingSetSize];
#endif
    ASSERT(pGpuDesc->mVRAM);

    pGpuDesc->mUniformBufferAlignment = 256;
    pGpuDesc->mUploadBufferAlignment = 1;
    pGpuDesc->mUploadBufferTextureAlignment = 16;
    pGpuDesc->mUploadBufferTextureRowAlignment = 1;
    pGpuDesc->mMaxVertexInputBindings = MAX_VERTEX_BINDINGS; // there are no special vertex buffers for input in Metal, only regular buffers
    pGpuDesc->mMultiDrawIndirect = false;                    // multi draw indirect is not supported on Metal: only single draw indirect
    pGpuDesc->mMultiDrawIndirectCount = false;               // multi draw indirect is not supported on Metal: only single draw indirect
    pGpuDesc->mRootConstant = true;
    pGpuDesc->mIndirectRootConstant = false;
    pGpuDesc->mBuiltinDrawID = false;
    pGpuDesc->mPrimitiveIdPsSupported = true;
    if (MTL_HDR_SUPPORTED)
    {
        pGpuDesc->mHDRSupported = true;
    }
#if defined(TARGET_IOS)
    pGpuDesc->mMaxBoundTextures = 31;
    pGpuDesc->mDrawIndexVertexOffsetSupported = false;
    BOOL isiOSAppOnMac = [NSProcessInfo processInfo].isiOSAppOnMac;
    pGpuDesc->mHeaps = !isiOSAppOnMac;
    pGpuDesc->mPlacementHeaps = !isiOSAppOnMac;
#else
    pGpuDesc->mIsHeadLess = [gpu isHeadless];
    pGpuDesc->mMaxBoundTextures = 128;
    pGpuDesc->mHeaps = [gpu isLowPower] ? 0 : 1;
#endif

    pGpuDesc->mAllowBufferTextureInSameHeap = true;
    pGpuDesc->mTimestampQueries = true;
    pGpuDesc->mOcclusionQueries = false;
    pGpuDesc->mPipelineStatsQueries = false;
    pGpuDesc->mGpuMarkers = true;

    uvec2 threadWidthAndTotalCount = queryThreadExecutionWidthAndTotalThreads(gpu);
    pGpuDesc->mWaveLaneCount = threadWidthAndTotalCount.getX();
    pGpuDesc->mMaxTotalComputeThreads = threadWidthAndTotalCount.getY();

    const MTLSize maxThreadsPerThreadgroup = [gpu maxThreadsPerThreadgroup];
    pGpuDesc->mMaxComputeThreads[0] = (uint32_t)maxThreadsPerThreadgroup.width;
    pGpuDesc->mMaxComputeThreads[1] = (uint32_t)maxThreadsPerThreadgroup.height;
    pGpuDesc->mMaxComputeThreads[2] = (uint32_t)maxThreadsPerThreadgroup.depth;

    // Features
    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    pGpuDesc->mROVsSupported = [gpu areRasterOrderGroupsSupported];
    pGpuDesc->mTessellationSupported = true;
    pGpuDesc->mGeometryShaderSupported = false;

    // Note: With enabled Shader Validation we are getting no performance benefit for stencil samples other than
    // first even if stencil test for those samples was failed
    pGpuDesc->mSoftwareVRSSupported = [gpu areProgrammableSamplePositionsSupported];

    // Wave ops crash the compiler if not supported by gpu
    pGpuDesc->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_NONE;
    pGpuDesc->mWaveOpsSupportedStageFlags = SHADER_STAGE_NONE;
    pGpuDesc->mIndirectCommandBuffer = false;
    pGpuDesc->mTessellationIndirectDrawSupported = false;
    pGpuDesc->mCubeMapTextureArraySupported = false;
    pGpuDesc->mPrimitiveIdSupported = false;
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
        pGpuDesc->mIndirectCommandBuffer = true;
#ifdef TARGET_IOS
        pGpuDesc->mDrawIndexVertexOffsetSupported = true;
#endif
    }
    if (highestAppleFamily >= MTLGPUFamilyApple4)
    {
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_QUAD_BIT;
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG;
        pGpuDesc->mMaxBoundTextures = 96;
        pGpuDesc->mCubeMapTextureArraySupported = true;
    }
    if (highestAppleFamily >= MTLGPUFamilyApple5)
    {
        pGpuDesc->mTessellationIndirectDrawSupported = true;
    }
#ifdef ENABLE_GPU_FAMILY_6
    if (highestAppleFamily >= MTLGPUFamilyApple6)
    {
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_BASIC_BIT | WAVE_OPS_SUPPORT_FLAG_VOTE_BIT |
                                          WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT | WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT |
                                          WAVE_OPS_SUPPORT_FLAG_SHUFFLE_RELATIVE_BIT;
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
    }
#endif // ENABLE_GPU_FAMILY_6
#ifdef ENABLE_GPU_FAMILY_7
    if (highestAppleFamily >= MTLGPUFamilyApple7)
    {
        pGpuDesc->mWaveOpsSupportFlags |= WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT;
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
        pGpuDesc->mPrimitiveIdSupported = true;
    }
#endif // ENABLE_GPU_FAMILY_7
#ifdef ENABLE_GPU_FAMILY_8
    if (highestAppleFamily >= MTLGPUFamilyApple8)
    {
    }
#endif // ENABLE_GPU_FAMILY_8
    if ([gpu supportsFamily:MTLGPUFamilyMac2])
    {
        pGpuDesc->mIndirectCommandBuffer = true;
        pGpuDesc->mWaveOpsSupportFlags = WAVE_OPS_SUPPORT_FLAG_ALL;
        pGpuDesc->mWaveOpsSupportedStageFlags |= SHADER_STAGE_FRAG | SHADER_STAGE_COMP;
        pGpuDesc->mTessellationIndirectDrawSupported = true;
        pGpuDesc->mPlacementHeaps = pGpuDesc->mHeaps ? 1 : 0;
        pGpuDesc->mCubeMapTextureArraySupported = true;
        pGpuDesc->mPrimitiveIdSupported = true;
    }

    MTLArgumentBuffersTier abTier = gpu.argumentBuffersSupport;

    if (abTier == MTLArgumentBuffersTier2)
    {
        pGpuDesc->mMaxBoundTextures = 1000000;
    }

#if defined(MTL_RAYTRACING_AVAILABLE)
    if (MTL_RAYTRACING_SUPPORTED)
    {
        pGpuDesc->mRayQuerySupported = [gpu supportsRaytracing];
        pGpuDesc->mRayPipelineSupported = false;
        pGpuDesc->mRaytracingSupported = pGpuDesc->mRayQuerySupported || pGpuDesc->mRayPipelineSupported;
    }
#endif

#ifdef ENABLE_GPU_FAMILY_9
    pGpuDesc->m64BitAtomicsSupported = [pGpuDesc->pGPU supportsFamily:MTLGPUFamilyApple9];
#endif // ENABLE_GPU_FAMILY_9
#ifdef ENABLE_GPU_FAMILY_8
    if ([pGpuDesc->pGPU supportsFamily:MTLGPUFamilyApple8] && [pGpuDesc->pGPU supportsFamily:MTLGPUFamilyMac2])
    {
        pGpuDesc->m64BitAtomicsSupported = true;
    }
#endif // ENABLE_GPU_FAMILY_8

    // Get the supported counter set.

    if (IOS14_RUNTIME)
    {
        pGpuDesc->pCounterSetTimestamp = util_get_counterset(gpu);
        // Mutually exclusive
        pGpuDesc->mStageBoundarySamplingSupported =
            pGpuDesc->pCounterSetTimestamp && [gpu supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
        pGpuDesc->mDrawBoundarySamplingSupported =
            pGpuDesc->pCounterSetTimestamp && [gpu supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary];
    }

#ifdef TARGET_IOS
    pGpuDesc->mUnifiedMemorySupport = [gpu hasUnifiedMemory] ? UMA_SUPPORT_READ_WRITE : UMA_SUPPORT_NONE;
#else
    // (m1,m2 are known to have problems with shared UAV resources )
    pGpuDesc->mUnifiedMemorySupport = [gpu hasUnifiedMemory] ? UMA_SUPPORT_READ : UMA_SUPPORT_NONE;
#endif
}

static bool SelectBestGpu(const RendererDesc* settings, Renderer* pRenderer)
{
    RendererContext* pContext = pRenderer->pContext;
    const GpuDesc*   gpus = pContext->mGpus;
    uint32_t         gpuCount = pContext->mGpuCount;
    uint32_t         gpuIndex = settings->pContext ? settings->mGpuIndex : UINT32_MAX;

    GpuDesc gpuDesc[MAX_MULTIPLE_GPUS] = {};
    for (uint32_t i = 0; i < gpuCount; i++)
    {
        gpuDesc[i] = gpus[i];
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
    gpuIndex = util_select_best_gpu(gpuDesc, pContext->mGpuCount);
#endif
#endif

    // no proper driver support on darwin, currently returning: GpuFamily: %d OSVersion: %s
    // bool driverValid = checkDriverRejectionSettings(&gpuSettings[gpuIndex]);
    // if (!driverValid)
    //{
    //	return false;
    //}

    ASSERT(gpuIndex < pContext->mGpuCount);

    bool gpuSupported = util_check_is_gpu_supported(&gpuDesc[gpuIndex]);
    if (!gpuSupported)
    {
        LOGF(eERROR, "Failed to Init Renderer: %s", getUnsupportedGPUMsg());
        return false;
    }

    pRenderer->pDevice = gpus[gpuIndex].pGPU;
    pRenderer->pGpu = &gpus[gpuIndex];
    pRenderer->mLinkedNodeCount = 1;

    LOGF(LogLevel::eINFO, "GPU[%d] is selected as default GPU", gpuIndex);
    LOGF(LogLevel::eINFO, "Name of selected gpu: %s", pRenderer->pGpu->mGpuVendorPreset.mGpuName);
    LOGF(LogLevel::eINFO, "Vendor id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mVendorId);
    LOGF(LogLevel::eINFO, "Model id of selected gpu: %#x", pRenderer->pGpu->mGpuVendorPreset.mModelId);
    LOGF(LogLevel::eINFO, "Preset of selected gpu: %s", presetLevelToString(pRenderer->pGpu->mGpuVendorPreset.mPresetLevel));

    MTLArgumentBuffersTier abTier = pRenderer->pDevice.argumentBuffersSupport;
    LOGF(LogLevel::eINFO, "Metal: Argument Buffer Tier: %lu", abTier);
    LOGF(LogLevel::eINFO, "Metal: Max Arg Buffer Textures: %u", pRenderer->pGpu->mMaxBoundTextures);

    return true;
}

void initRendererContext(const char* appName, const RendererContextDesc* pDesc, RendererContext** ppContext)
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
        setDefaultGPUProperties(&pContext->mGpus[i]);
    }

    pContext->mGpuCount = gpuCount;
    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        pContext->mGpus[i].pGPU = gpus[i];
        QueryGpuDesc(&pContext->mGpus[i]);
        mtlCapsBuilder(pContext->mGpus[i].pGPU, &pContext->mGpus[i]);
        applyGPUConfigurationRules(&pContext->mGpus[i]);

        LOGF(LogLevel::eINFO, "GPU[%i] detected. Vendor ID: %#x, Model ID: %#x, Preset: %s, GPU Name: %s", i,
             pContext->mGpus[i].mGpuVendorPreset.mVendorId, pContext->mGpus[i].mGpuVendorPreset.mModelId,
             presetLevelToString(pContext->mGpus[i].mGpuVendorPreset.mPresetLevel), pContext->mGpus[i].mGpuVendorPreset.mGpuName);
    }

    *ppContext = pContext;
}

void exitRendererContext(RendererContext* pContext)
{
    ASSERT(pContext);
    SAFE_FREE(pContext);
}

void initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
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
        initRendererContext(appName, &contextDesc, &pRenderer->pContext);
        pRenderer->mOwnsContext = true;
    }

    // Initialize the Metal bits
    {
        if (!SelectBestGpu(settings, pRenderer))
        {
            // set device to null
            pRenderer->pDevice = nil;
            // remove allocated renderer
            SAFE_FREE(pRenderer);
            *ppRenderer = NULL;
            return;
        }

        LOGF(LogLevel::eINFO, "Metal: Heaps: %s", pRenderer->pGpu->mHeaps ? "true" : "false");
        LOGF(LogLevel::eINFO, "Metal: Placement Heaps: %s", pRenderer->pGpu->mPlacementHeaps ? "true" : "false");

        if (!pRenderer->pGpu->mPlacementHeaps)
        {
            pRenderer->pHeapMutex = (Mutex*)tf_malloc(sizeof(Mutex));
            initMutex(pRenderer->pHeapMutex);
        }

        // exit app if gpu being used has an office preset.
        if (pRenderer->pGpu->mGpuVendorPreset.mPresetLevel < GPU_PRESET_VERYLOW)
        {
            ASSERT(pRenderer->pGpu->mGpuVendorPreset.mPresetLevel >= GPU_PRESET_VERYLOW);

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
        if (pRenderer->pGpu->mPlacementHeaps)
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

        pRenderer->mGpuToCpuTimestampFactor = 0;
        pRenderer->mPrevCpuTimestamp = 0;
        pRenderer->mPrevGpuTimestamp = 0;

        // Renderer is good! Assign it to result!
        *(ppRenderer) = pRenderer;
    }

    if (IOS14_RUNTIME)
    {
#if defined(ENABLE_GRAPHICS_VALIDATION)
        pRenderer->pContext->mMtl.mExtendedEncoderDebugReport = true;
#else
        pRenderer->pContext->mMtl.mExtendedEncoderDebugReport = settings->mEnableGpuBasedValidation;
#endif
    }
}

void exitRenderer(Renderer* pRenderer)
{
    ASSERT(pRenderer);

    remove_default_resources(pRenderer);

    if (pRenderer->pGpu->mPlacementHeaps)
    {
        vmaDestroyAllocator(pRenderer->pVmaAllocator);
    }

    pRenderer->pDevice = nil;

    if (pRenderer->pHeapMutex)
    {
        exitMutex(pRenderer->pHeapMutex);
    }

    SAFE_FREE(pRenderer->pHeapMutex);
    SAFE_FREE(pRenderer->pHeaps);

    if (pRenderer->mOwnsContext)
    {
        exitRendererContext(pRenderer->pContext);
    }

    SAFE_FREE(pRenderer);
}

void initFence(Renderer* pRenderer, Fence** ppFence)
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

void exitFence(Renderer* pRenderer, Fence* pFence)
{
    ASSERT(pFence);
    pFence->pSemaphore = nil;

    SAFE_FREE(pFence);
}

void initSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
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

void exitSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
    ASSERT(pSemaphore);

    pSemaphore->pSemaphore = nil;

    SAFE_FREE(pSemaphore);
}

void initQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
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

void exitQueue(Renderer* pRenderer, Queue* pQueue)
{
    ASSERT(pQueue);

    pQueue->pCommandQueue = nil;
    pQueue->pQueueFence = nil;

    SAFE_FREE(pQueue);
}

void initCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->pDevice != nil);
    ASSERT(ppCmdPool);

    CmdPool* pCmdPool = (CmdPool*)tf_calloc(1, sizeof(CmdPool));
    ASSERT(pCmdPool);

    pCmdPool->pQueue = pDesc->pQueue;

    *ppCmdPool = pCmdPool;
}

void exitCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    ASSERT(pCmdPool);
    SAFE_FREE(pCmdPool);
}

void initCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
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

void exitCmd(Renderer* pRenderer, Cmd* pCmd)
{
    ASSERT(pCmd);
    pCmd->pCommandBuffer = nil;

    SAFE_FREE(pCmd);
}

void initCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
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
        ::initCmd(pRenderer, pDesc, &ppCmds[i]);
    }

    *pppCmd = ppCmds;
}

void exitCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
{
    // verify that given command list is valid
    ASSERT(ppCmds);

    // remove every given cmd in array
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        exitCmd(pRenderer, ppCmds[i]);
    }

    SAFE_FREE(ppCmds);
}

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain)
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

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
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

void removeSwapChain(Renderer* pRenderer, SwapChain* pSwapChain)
{
    ASSERT(pRenderer);
    ASSERT(pSwapChain);

    pSwapChain->presentCommandBuffer = nil;

    for (uint32_t i = 0; i < pSwapChain->mImageCount; ++i)
        removeRenderTarget(pRenderer, pSwapChain->ppRenderTargets[i]);

    SAFE_FREE(pSwapChain);
}

void addResourceHeap(Renderer* pRenderer, const ResourceHeapDesc* pDesc, ResourceHeap** ppHeap)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppHeap);

    if (!pRenderer->pGpu->mPlacementHeaps)
    {
        return;
    }

    const bool isUma =
        pRenderer->pGpu->mUnifiedMemorySupport != UMA_SUPPORT_NONE &&
        (pRenderer->pGpu->mUnifiedMemorySupport == UMA_SUPPORT_READ_WRITE || (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_MASK) == 0);

    // Same reasoning as addBuffer. Forced to always create MTLStorageMode_Shared heap here.
    VmaAllocationCreateInfo vma_mem_reqs = {};
    if (isUma && pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
    {
        vma_mem_reqs.usage = (VmaMemoryUsage)RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    }
    else
    {
        vma_mem_reqs.usage = (VmaMemoryUsage)pDesc->mMemoryUsage;
    }
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

void removeResourceHeap(Renderer* pRenderer, ResourceHeap* pHeap)
{
    if (!pRenderer->pGpu->mPlacementHeaps)
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

void getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut)
{
    UNREF_PARAM(pRenderer);
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(pOut);

    uint64_t allocationSize = pDesc->mSize;
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        const uint64_t minAlignment = pRenderer->pGpu->mUniformBufferAlignment;
        allocationSize = round_up_64(allocationSize, minAlignment);
    }

    const MemoryType   memoryType = util_to_memory_type(pDesc->mMemoryUsage);
    MTLResourceOptions resourceOptions = gMemoryOptions[memoryType];
    MTLSizeAndAlign    sizeAlign = [pRenderer->pDevice heapBufferSizeAndAlignWithLength:allocationSize options:resourceOptions];
    pOut->mSize = sizeAlign.size;
    pOut->mAlignment = sizeAlign.align;
}

void getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut)
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

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
    ASSERT(pRenderer);
    ASSERT(ppBuffer);
    ASSERT(pDesc);
    ASSERT((pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER) || pDesc->mSize > 0);
    ASSERT(pRenderer->pDevice != nil);

    Buffer* pBuffer = (Buffer*)tf_calloc_memalign(1, alignof(Buffer), sizeof(Buffer));
    ASSERT(pBuffer);

    uint64_t   allocationSize = pDesc->mSize;
    MemoryType memoryType;

    const bool isUma =
        pRenderer->pGpu->mUnifiedMemorySupport != UMA_SUPPORT_NONE &&
        (pRenderer->pGpu->mUnifiedMemorySupport == UMA_SUPPORT_READ_WRITE || (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_MASK) == 0);

    // Resource Loader assumes devices support uma will always update a buffer from the cpu side directly.
    // Naively allocate MTLStorageMode_Shared since there is no way of knowing whether a buffer will be updated or not here.
    if (isUma && pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
    {
        memoryType = MEMORY_TYPE_CPU_TO_GPU;
    }
    else
    {
        memoryType = util_to_memory_type(pDesc->mMemoryUsage);
    }
    MTLResourceOptions resourceOptions = gMemoryOptions[memoryType];
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pDesc->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        uint64_t minAlignment = pRenderer->pGpu->mUniformBufferAlignment;
        allocationSize = round_up_64(allocationSize, minAlignment);
        ((BufferDesc*)pDesc)->mAlignment = (uint32_t)minAlignment;
    }

    //	//Use isLowPower to determine if running intel integrated gpu
    //	//There's currently an intel driver bug with placed resources so we need to create
    //	//new resources that are GPU only in their own memory space
    //	//0x8086 is intel vendor id
    //	if (pRenderer->pGpu->mGpuVendorPreset.mVendorId == GPU_VENDOR_INTEL &&
    //		(ResourceMemoryUsage)pDesc->mMemoryUsage & RESOURCE_MEMORY_USAGE_GPU_ONLY)
    //		((BufferDesc*)pDesc)->mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    // Indirect command buffer does not need backing buffer memory
    // Directly allocated through device
    if (pRenderer->pGpu->mIndirectCommandBuffer && (pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER))
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

    if (pDesc->pPlacement && pRenderer->pGpu->mPlacementHeaps)
    {
        pBuffer->pBuffer = [pDesc->pPlacement->pHeap->pHeap newBufferWithLength:allocationSize
                                                                        options:resourceOptions
                                                                         offset:pDesc->pPlacement->mOffset];
    }

    if (!pBuffer->pBuffer && (!pRenderer->pGpu->mHeaps || (RESOURCE_MEMORY_USAGE_GPU_ONLY != pDesc->mMemoryUsage &&
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
            if (pRenderer->pGpu->mPlacementHeaps)
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pDesc->pName)
    {
        setBufferName(pRenderer, pBuffer, pDesc->pName);
    }
#endif
    *ppBuffer = pBuffer;
}

void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
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

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
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

void removeTexture(Renderer* pRenderer, Texture* pTexture)
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

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
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

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
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

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
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

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
    ASSERT(pSampler);
    pSampler->pSamplerState = nil;
    SAFE_FREE(pSampler);
}

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
    ASSERT(pRenderer);
    ASSERT(pDesc && pDesc->mStages);
    ASSERT(ppShaderProgram);

    Shader* pShaderProgram = (Shader*)tf_calloc(1, sizeof(Shader));
    ASSERT(pShaderProgram);

    pShaderProgram->mStages = pDesc->mStages;

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
        }
    }

    if (pShaderProgram->mStages & SHADER_STAGE_COMP)
    {
        for (size_t i = 0; i < 3; ++i)
        {
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

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
    ASSERT(pShaderProgram);

    pShaderProgram->pVertexShader = nil;
    pShaderProgram->pFragmentShader = nil;
    pShaderProgram->pComputeShader = nil;
    SAFE_FREE(pShaderProgram);
}
void addGraphicsPipelineImpl(Renderer* pRenderer, const char* pName, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->pDevice != nil);
    ASSERT(pDesc);
    ASSERT(pDesc->pShaderProgram);
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
        MTLPixelFormat depthStencilFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mDepthStencilFormat);
        if (TinyImageFormat_HasDepth(pDesc->mDepthStencilFormat))
        {
            renderPipelineDesc.depthAttachmentPixelFormat = depthStencilFormat;
        }
        if (TinyImageFormat_HasStencil(pDesc->mDepthStencilFormat))
        {
            renderPipelineDesc.stencilAttachmentPixelFormat = depthStencilFormat;
        }
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

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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
    ASSERT(ppPipeline);

    Pipeline* pPipeline = (Pipeline*)tf_calloc_memalign(1, alignof(Pipeline), sizeof(Pipeline));
    ASSERT(pPipeline);

    pPipeline->mType = PIPELINE_TYPE_COMPUTE;

    MTLComputePipelineDescriptor* pipelineDesc = [[MTLComputePipelineDescriptor alloc] init];
    pipelineDesc.computeFunction = pDesc->pShaderProgram->pComputeShader;
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

    const uint32_t* numThreadsPerGroup = pDesc->pShaderProgram->mNumThreadsPerGroup;
    uint32_t        totalThreadsPerGroup = numThreadsPerGroup[0] * numThreadsPerGroup[1] * numThreadsPerGroup[2];
    uint32_t        maxTotalThreadsPerThreadgroup = (uint32_t)pPipeline->pComputePipelineState.maxTotalThreadsPerThreadgroup;
    if (maxTotalThreadsPerThreadgroup < totalThreadsPerGroup)
    {
        LOGF(LogLevel::eERROR,
             "The total threads per threadgroup for this pipeline (%d) is insufficient for the numThreadsPerGroup (%d). Failing Pipeline "
             "creation",
             maxTotalThreadsPerThreadgroup, totalThreadsPerGroup);
        removePipeline(pRenderer, pPipeline);
    }
    else
    {
        pPipeline->mNumThreadsPerGroup = MTLSizeMake(numThreadsPerGroup[0], numThreadsPerGroup[1], numThreadsPerGroup[2]);
    }

    *ppPipeline = pPipeline;
}

void addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
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

void removePipeline(Renderer* pRenderer, Pipeline* pPipeline)
{
    ASSERT(pPipeline);

    pPipeline->pRenderPipelineState = nil;
    pPipeline->pComputePipelineState = nil;
    pPipeline->pDepthStencilState = nil;

    SAFE_FREE(pPipeline);
}

void addPipelineCache(Renderer*, const PipelineCacheDesc*, PipelineCache**) {}

void removePipelineCache(Renderer*, PipelineCache*) {}

void getPipelineCacheData(Renderer*, PipelineCache*, size_t*, void*) {}
// -------------------------------------------------------------------------------------------------
// Buffer functions
// -------------------------------------------------------------------------------------------------
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
    ASSERT(pBuffer->pBuffer.storageMode != MTLStorageModePrivate && "Trying to map non-cpu accessible resource");
    pBuffer->pCpuMappedAddress = (uint8_t*)pBuffer->pBuffer.contents + pBuffer->mOffset;
}
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
    ASSERT(pBuffer->pBuffer.storageMode != MTLStorageModePrivate && "Trying to unmap non-cpu accessible resource");
    pBuffer->pCpuMappedAddress = nil;
}

// -------------------------------------------------------------------------------------------------
// Command buffer functions
// -------------------------------------------------------------------------------------------------

void resetCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(pCmdPool);
}

void beginCmd(Cmd* pCmd)
{
    @autoreleasepool
    {
        ASSERT(pCmd);
        pCmd->pRenderEncoder = nil;
        pCmd->pComputeEncoder = nil;
        pCmd->pBlitEncoder = nil;
        pCmd->pBoundPipeline = NULL;
        pCmd->mBoundIndexBuffer = nil;
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

    for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        pCmd->mBoundDescriptorSets[i] = NULL;
    }
    pCmd->mShouldRebindPipeline = false;
    pCmd->mPipelineType = PIPELINE_TYPE_UNDEFINED;
}

void endCmd(Cmd* pCmd)
{
    @autoreleasepool
    {
        util_end_current_encoders(pCmd, true);
    }
}

void cmdBindRenderTargets(Cmd* pCmd, const BindRenderTargetsDesc* pDesc)
{
    ASSERT(pCmd);

    if (!pDesc)
    {
        return;
    }

    if (pCmd->pBoundPipeline)
    {
        pCmd->mShouldRebindPipeline = 1;
    }

    const bool hasDepthStencil = pDesc->mDepthStencil.pDepthStencil;

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

        if (hasDepthStencil)
        {
            const BindDepthTargetDesc* desc = &pDesc->mDepthStencil;
            TinyImageFormat            dsFormat =
                TinyImageFormat_FromMTLPixelFormat((TinyImageFormat_MTLPixelFormat)desc->pDepthStencil->pTexture->pTexture.pixelFormat);
            bool hasDepth = TinyImageFormat_HasDepth(dsFormat);
            bool hasStencil = TinyImageFormat_HasStencil(dsFormat);

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
            }
            if (hasStencil)
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

            if (hasStencil)
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
            if (hasStencil)
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

        SampleCount sampleCount = pDesc->mRenderTargetCount > 0 ? pDesc->mRenderTargets[0].pRenderTarget->mSampleCount : SAMPLE_COUNT_1;
        uint32_t    sampleLocationsCount = pDesc->mSampleLocation.mGridSizeX * pDesc->mSampleLocation.mGridSizeY * sampleCount;
        ASSERT(sampleLocationsCount < MAX_SAMPLE_LOCATIONS);
        if (sampleLocationsCount > 0)
        {
            MTLSamplePosition samplePositions[MAX_SAMPLE_LOCATIONS];
            for (uint32_t i = 0; i < sampleLocationsCount; i++)
            {
                samplePositions[i] = util_to_mtl_locations(pDesc->mSampleLocation.pLocations[i]);
            }
            if (sampleCount > SAMPLE_COUNT_1)
            {
                [renderPassDesc setSamplePositions:samplePositions count:sampleLocationsCount];
            }
        }

        if (!pDesc->mRenderTargetCount && !hasDepthStencil)
        {
            renderPassDesc.renderTargetWidth = pDesc->mExtent[0];
            renderPassDesc.renderTargetHeight = pDesc->mExtent[1];
            renderPassDesc.defaultRasterSampleCount = 1;
        }

        // Add a sampler buffer attachment
        if (IOS14_RUNTIME)
        {
            if (pCmd->pRenderer->pGpu->mStageBoundarySamplingSupported && pCmd->pCurrentQueryPool != nil)
            {
                MTLRenderPassSampleBufferAttachmentDescriptor* sampleAttachmentDesc = renderPassDesc.sampleBufferAttachments[0];
                QueryPool*                                     pQueryPool = pCmd->pCurrentQueryPool;
                QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pCmd->mCurrentQueryIndex];

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

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
        util_set_debug_group(pCmd);
        if (pCmd->mDebugMarker[0])
        {
            pCmd->pRenderEncoder.label = [NSString stringWithUTF8String:pCmd->mDebugMarker];
        }
#endif

        util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS); // apply the graphics barriers before flushing them

        util_set_heaps_graphics(pCmd);
    }

    for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        if (pCmd->mBoundDescriptorSets[i])
        {
            cmdBindDescriptorSet(pCmd, pCmd->mBoundDescriptorSetIndices[i], pCmd->mBoundDescriptorSets[i]);
        }
    }
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
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

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
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

void cmdSetStencilReferenceValue(Cmd* pCmd, uint32_t val)
{
    ASSERT(pCmd);

    [pCmd->pRenderEncoder setStencilReferenceValue:val];
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
    ASSERT(pCmd);
    ASSERT(pPipeline);

    uint32_t shouldRebind = pCmd->mShouldRebindPipeline;
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
                    if (pCmd->pRenderer->pGpu->mStageBoundarySamplingSupported && pCmd->pCurrentQueryPool != nil)
                    {
                        QueryPool*       pQueryPool = pCmd->pCurrentQueryPool;
                        QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pCmd->mCurrentQueryIndex];

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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

    // if pipeline type changed we need to force rebind the shared sets
    bool     pipelineTypeChanged = barrierRequired;
    uint32_t rebindMask = (pipelineTypeChanged || shouldRebind) ? MAX_DESCRIPTOR_SETS_MASK : pCmd->mShouldRebindDescriptorSetsMask;
    for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        uint32_t currSetMask = 0x01 << i;

        if (rebindMask & currSetMask)
        {
            if (pCmd->mBoundDescriptorSets[i])
            {
                cmdBindDescriptorSet(pCmd, pCmd->mBoundDescriptorSetIndices[i], pCmd->mBoundDescriptorSets[i]);
            }
        }
    }
    pCmd->mShouldRebindDescriptorSetsMask = 0;
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
    ASSERT(pCmd);
    ASSERT(pBuffer);

    pCmd->mBoundIndexBuffer = pBuffer->pBuffer;
    pCmd->mBoundIndexBufferOffset = (uint32_t)(offset + pBuffer->mOffset);
    pCmd->mIndexType = (INDEX_TYPE_UINT16 == indexType ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
    pCmd->mIndexStride = (INDEX_TYPE_UINT16 == indexType ? sizeof(uint16_t) : sizeof(uint32_t));
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
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
    if (pCmd->mShouldRebindPipeline)
    {
        cmdBindPipeline(pCmd, pCmd->pBoundPipeline);
    }
    pCmd->mShouldRebindPipeline = 0;
}

void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
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

void cmdDrawInstanced(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
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

void cmdDrawIndexed(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t firstVertex)
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
            ASSERT((bool)pCmd->pRenderer->pGpu->mDrawIndexVertexOffsetSupported);
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

void cmdDrawIndexedInstanced(Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
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

void cmdDispatch(Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
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

void cmdExecuteIndirect(Cmd* pCmd, IndirectArgumentType type, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
                        Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    ASSERT(pCmd);

    RebindState(pCmd);

    if (pCmd->pRenderer->pGpu->mIndirectCommandBuffer)
    {
        uint64_t rangeOffset = bufferOffset;

        if (type == INDIRECT_COMMAND_BUFFER_OPTIMIZE && maxCommandCount)
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
        else if (type == INDIRECT_COMMAND_BUFFER_RESET && maxCommandCount)
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
        else if ((pIndirectBuffer->pIndirectCommandBuffer || type == INDIRECT_COMMAND_BUFFER) && maxCommandCount)
        {
            [pCmd->pRenderEncoder executeCommandsInBuffer:pIndirectBuffer->pIndirectCommandBuffer
                                                withRange:NSMakeRange(rangeOffset, maxCommandCount)];
            return;
        }
    }

    static const uint32_t cmdStrides[3] = {
        sizeof(IndirectDrawArguments),
        sizeof(IndirectDrawIndexArguments),
        sizeof(IndirectDispatchArguments),
    };
    const uint32_t stride = cmdStrides[type];

    if (type == INDIRECT_DRAW)
    {
        if (!pCmd->pBoundPipeline->mTessellation)
        {
            for (uint32_t i = 0; i < maxCommandCount; i++)
            {
                uint64_t indirectBufferOffset = bufferOffset + stride * i;
                [pCmd->pRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->mSelectedPrimitiveType
                                      indirectBuffer:pIndirectBuffer->pBuffer
                                indirectBufferOffset:indirectBufferOffset];
            }
        }
        else // Tessellated indirect draw version.
        {
            ASSERT((bool)pCmd->pRenderer->pGpu->mTessellationIndirectDrawSupported);
            for (uint32_t i = 0; i < maxCommandCount; i++)
            {
                uint64_t indirectBufferOffset = bufferOffset + stride * i;
                [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                 patchIndexBuffer:nil
                           patchIndexBufferOffset:0
                                   indirectBuffer:pIndirectBuffer->pBuffer
                             indirectBufferOffset:indirectBufferOffset];
            }
        }
    }
    else if (type == INDIRECT_DRAW_INDEX)
    {
        if (!pCmd->pBoundPipeline->mTessellation)
        {
            for (uint32_t i = 0; i < maxCommandCount; ++i)
            {
                id           indexBuffer = pCmd->mBoundIndexBuffer;
                MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
                uint64_t     indirectBufferOffset = bufferOffset + stride * i;

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
            ASSERT((bool)pCmd->pRenderer->pGpu->mTessellationIndirectDrawSupported);
            for (uint32_t i = 0; i < maxCommandCount; ++i)
            {
                id       indexBuffer = pCmd->mBoundIndexBuffer;
                uint64_t indirectBufferOffset = bufferOffset + stride * i;
                [pCmd->pRenderEncoder drawPatches:pCmd->pBoundPipeline->mPatchControlPointCount
                                 patchIndexBuffer:indexBuffer
                           patchIndexBufferOffset:0
                                   indirectBuffer:pIndirectBuffer->pBuffer
                             indirectBufferOffset:indirectBufferOffset];
            }
        }
    }
    else if (type == INDIRECT_DISPATCH)
    {
        // There might have been a barrier inserted since last dispatch call
        // This only applies to dispatch since you can issue barriers inside compute pass but this is not possible inside render pass
        // For render pass, barriers are issued in cmdBindRenderTargets after beginning new encoder
        util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);

        for (uint32_t i = 0; i < maxCommandCount; ++i)
        {
            [pCmd->pComputeEncoder dispatchThreadgroupsWithIndirectBuffer:pIndirectBuffer->pBuffer
                                                     indirectBufferOffset:bufferOffset + stride * i
                                                    threadsPerThreadgroup:pCmd->pBoundPipeline->mNumThreadsPerGroup];
        }
    }
}

void cmdResourceBarrier(Cmd* pCmd, uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
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

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
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

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pIntermediate, const SubresourceDataDesc* pSubresourceDesc)
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

void cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc)
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

void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->pDevice != nil);
    ASSERT(pSwapChain);
    ASSERT(pSignalSemaphore || pFence);

    @autoreleasepool
    {
        CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
#ifdef AUTOMATED_TESTING
        layer.framebufferOnly = !isScreenshotCaptureRequested();
#endif
        pSwapChain->mMTKDrawable = [layer nextDrawable];

        pSwapChain->mIndex = (pSwapChain->mIndex + 1) % pSwapChain->mImageCount;
        *pImageIndex = pSwapChain->mIndex;

        pSwapChain->ppRenderTargets[pSwapChain->mIndex]->pTexture->pTexture = pSwapChain->mMTKDrawable.texture;
    }
}

void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
    ASSERT(pQueue);
    ASSERT(pDesc);
    ASSERT(pDesc->pSwapChain);

    @autoreleasepool
    {
        SwapChain* pSwapChain = pDesc->pSwapChain;
        ASSERT(pQueue->pCommandQueue != nil);

#if defined(AUTOMATED_TESTING)
        if (isScreenshotCaptureRequested())
        {
            ScreenshotDesc desc = {};
            desc.pRenderTarget = pSwapChain->ppRenderTargets[pDesc->mIndex];
            desc.mColorSpace = pSwapChain->mColorSpace;
            desc.ppWaitSemaphores = pDesc->ppWaitSemaphores;
            desc.mWaitSemaphoresCount = pDesc->mWaitSemaphoreCount;
            desc.flipRedBlue = false;
            desc.discardAlpha = true;
            captureScreenshot(&desc);
        }
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

void waitForFences(Renderer* pRenderer, uint32_t fenceCount, Fence** ppFences)
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

void waitQueueIdle(Queue* pQueue)
{
    ASSERT(pQueue);
    id<MTLCommandBuffer> waitCmdBuf = [pQueue->pCommandQueue commandBufferWithUnretainedReferences];

    [waitCmdBuf commit];
    [waitCmdBuf waitUntilCompleted];

    waitCmdBuf = nil;
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
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
void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    snprintf(pCmd->mDebugMarker, MAX_DEBUG_NAME_LENGTH, "%s", pName);
    util_set_debug_group(pCmd);
#endif
}

void cmdEndDebugMarker(Cmd* pCmd)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    util_unset_debug_group(pCmd);
#endif
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    if (pCmd->pRenderEncoder)
        [pCmd->pRenderEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
    else if (pCmd->pComputeEncoder)
        [pCmd->pComputeEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
    else if (pCmd->pBlitEncoder)
        [pCmd->pBlitEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
#endif
}

void cmdWriteMarker(Cmd* pCmd, const MarkerDesc* pDesc)
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

void getTimestampFrequency(Queue* pQueue, double* pFrequency) { *pFrequency = GPU_FREQUENCY; }

void initQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
    if (QUERY_TYPE_TIMESTAMP != pDesc->mType)
    {
        ASSERT(false && "Not supported");
        return;
    }

    if (!pRenderer->pGpu->mTimestampQueries)
    {
        ASSERT(false && "Not supported");
        return;
    }

    QueryPool* pQueryPool = (QueryPool*)tf_calloc(1, sizeof(QueryPool));
    ASSERT(pQueryPool);

    pQueryPool->mCount = pDesc->mQueryCount;

    // Create counter sampler buffer
    if (pDesc->mType == QUERY_TYPE_TIMESTAMP)
    {
        NSUInteger sampleCount = 0;
        pQueryPool->pQueries = (void*)tf_calloc(pQueryPool->mCount, sizeof(QuerySampleDesc));
        if (pRenderer->pGpu->mStageBoundarySamplingSupported)
        {
            sampleCount =
                NUM_RENDER_STAGE_BOUNDARY_COUNTERS * pDesc->mQueryCount + NUM_COMPUTE_STAGE_BOUNDARY_COUNTERS * pDesc->mQueryCount;
        }
        else if (pRenderer->pGpu->mDrawBoundarySamplingSupported)
        {
            sampleCount = NUM_DRAW_BOUNDARY_COUNTERS * pDesc->mQueryCount;
        }
        @autoreleasepool
        {
            MTLCounterSampleBufferDescriptor* pDescriptor;
            pDescriptor = [[MTLCounterSampleBufferDescriptor alloc] init];

            // This counter set instance belongs to the `device` instance.
            pDescriptor.counterSet = pRenderer->pGpu->pCounterSetTimestamp;

            // Set the buffer to use shared memory so the CPU and GPU can directly access its contents.
            pDescriptor.storageMode = MTLStorageModeShared;

            // Add Stage (4) and Compute (2) encoder counters..
            pDescriptor.sampleCount = sampleCount;
            // Create the sample buffer by passing the descriptor to the device's factory method.
            NSError* error = nil;
            pQueryPool->pSampleBuffer = [pRenderer->pGpu->pGPU newCounterSampleBufferWithDescriptor:pDescriptor error:&error];

            if (error != nil)
            {
                LOGF(LogLevel::eERROR, "Device failed to create a counter sample buffer: %s", [error.description UTF8String]);
            }
        }
    }

    *ppQueryPool = pQueryPool;
}

void exitQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
    pQueryPool->pSampleBuffer = nil;
    SAFE_FREE(pQueryPool->pQueries);
    SAFE_FREE(pQueryPool);
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    if (pQueryPool->mType == QUERY_TYPE_TIMESTAMP)
    {
        pCmd->pCurrentQueryPool = pQueryPool;
        pCmd->mCurrentQueryIndex = pQuery->mIndex;

        if (pQuery->mIndex == 0)
        {
            pQueryPool->mRenderSamplesOffset = 0;
            pQueryPool->mComputeSamplesOffset = pQueryPool->mCount * NUM_RENDER_STAGE_BOUNDARY_COUNTERS;
            util_update_gpu_to_cpu_timestamp_factor(pCmd->pRenderer);
        }

        if (pCmd->pRenderer->pGpu->mStageBoundarySamplingSupported)
        {
            QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pQuery->mIndex];
            pSample->mRenderSamples = 0;
            pSample->mComputeSamples = 0;
            pSample->mRenderStartIndex = pQueryPool->mRenderSamplesOffset;
            pSample->mComputeStartIndex = pQueryPool->mComputeSamplesOffset;
        }
        else if (pCmd->pRenderer->pGpu->mDrawBoundarySamplingSupported)
        {
            QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pQuery->mIndex];
            if (pCmd->pRenderEncoder)
            {
                [pCmd->pRenderEncoder sampleCountersInBuffer:pQueryPool->pSampleBuffer atSampleIndex:pQuery->mIndex * 2 withBarrier:NO];
                pSample->mHasBeginSample = true;
            }
        }
    }
}

void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    pCmd->pCurrentQueryPool = nil;
    pCmd->mCurrentQueryIndex = -1;

    if (pQueryPool->mType == QUERY_TYPE_TIMESTAMP)
    {
        if (pCmd->pRenderer->pGpu->mDrawBoundarySamplingSupported)
        {
            QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pQuery->mIndex];
            if (!gIssuedQueryIgnoredWarning && !pSample->mHasBeginSample)
            {
                gIssuedQueryIgnoredWarning = true;
                LOGF(eWARNING,
                     "A draw boundary BeginQuery() is being ignored: Try moving it above cmdBindRenderTarget(). Warning only issued once.");
                return;
            }

            if (pCmd->pRenderEncoder)
            {
                [pCmd->pRenderEncoder sampleCountersInBuffer:pQueryPool->pSampleBuffer atSampleIndex:pQuery->mIndex * 2 + 1 withBarrier:NO];
            }
        }
        else if (pCmd->pRenderer->pGpu->mStageBoundarySamplingSupported)
        {
            QuerySampleDesc* pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[pQuery->mIndex];
            if (!gIssuedQueryIgnoredWarning && pSample->mRenderSamples == 0 && pSample->mComputeSamples == 0)
            {
                gIssuedQueryIgnoredWarning = true;
                LOGF(eWARNING, "A stage boundary BeginQuery() is being ignored: Try moving it above cmdBindRenderTarget() or "
                               "cmdBindPipeline(Compute). "
                               "Warning only issued once.");
            }
        }
    }
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount) {}

void cmdResetQuery(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount) {}

void getQueryData(Renderer* pRenderer, QueryPool* pQueryPool, uint32_t queryIndex, QueryData* pOutData)
{
    ASSERT(pRenderer);
    ASSERT(pQueryPool);
    ASSERT(pOutData);

    uint64_t sumEncoderTimestamps = 0;

    if (pQueryPool->pSampleBuffer != nil)
    {
        QuerySampleDesc*           pSample = &((QuerySampleDesc*)pQueryPool->pQueries)[queryIndex];
        MTLCounterResultTimestamp* timestamps;
        if (pRenderer->pGpu->mStageBoundarySamplingSupported)
        {
            uint32_t numSamples = pSample->mRenderSamples;
            uint32_t startSampleIndex = pSample->mRenderStartIndex;
            uint32_t sampleCount = numSamples * NUM_RENDER_STAGE_BOUNDARY_COUNTERS;

            // Cast the data's bytes property to the counter's result type.
            timestamps = util_resolve_counter_sample_buffer(startSampleIndex, sampleCount, pQueryPool->pSampleBuffer);

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
            pOutData->mBeginTimestamp = 0;
            pOutData->mEndTimestamp = sumEncoderTimestamps * pRenderer->mGpuToCpuTimestampFactor;
        }
        else if (pRenderer->pGpu->mDrawBoundarySamplingSupported)
        {
            if (!pSample->mHasBeginSample)
            {
                pOutData->mBeginTimestamp = 0;
                pOutData->mEndTimestamp = 0;
                return;
            }
            timestamps = util_resolve_counter_sample_buffer(queryIndex * 2, NUM_DRAW_BOUNDARY_COUNTERS, pQueryPool->pSampleBuffer);
            if (timestamps != nil)
            {
                pOutData->mBeginTimestamp = timestamps[0].timestamp * pRenderer->mGpuToCpuTimestampFactor;
                pOutData->mEndTimestamp = timestamps[1].timestamp * pRenderer->mGpuToCpuTimestampFactor;
            }
        }
    }
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void setBufferName(Renderer* pRenderer, Buffer* pBuffer, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pBuffer);
    ASSERT(pName);

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

void setTextureName(Renderer* pRenderer, Texture* pTexture, const char* pName)
{
    ASSERT(pRenderer);
    ASSERT(pTexture);
    ASSERT(pName);

#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
    NSString* str = [NSString stringWithUTF8String:pName];
    pTexture->pTexture.label = str;
#endif
}

void setRenderTargetName(Renderer* pRenderer, RenderTarget* pRenderTarget, const char* pName)
{
    setTextureName(pRenderer, pRenderTarget->pTexture, pName);
}

void setPipelineName(Renderer*, Pipeline*, const char*) {}
// -------------------------------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------
// Internal utility functions
// -------------------------------------------------------------------------------------------------

void util_bind_root_buffer(Cmd* pCmd, const RootDescriptorHandle* pHandle, uint32_t stages)
{
    if (stages & SHADER_STAGE_VERT)
    {
        [pCmd->pRenderEncoder setVertexBuffers:pHandle->pArr
                                       offsets:pHandle->pOffsets
                                     withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
    }

    if (stages & SHADER_STAGE_FRAG)
    {
        [pCmd->pRenderEncoder setFragmentBuffers:pHandle->pArr
                                         offsets:pHandle->pOffsets
                                       withRange:NSMakeRange(pHandle->mBinding, pHandle->mCount)];
    }

    if (stages & SHADER_STAGE_COMP)
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

void util_update_gpu_to_cpu_timestamp_factor(Renderer* pRenderer)
{
    MTLTimestamp currentCpuTimestamp;
    MTLTimestamp currentGpuTimestamp;

    [pRenderer->pGpu->pGPU sampleTimestamps:&currentCpuTimestamp gpuTimestamp:&currentGpuTimestamp];
    if (currentCpuTimestamp > pRenderer->mPrevCpuTimestamp && currentGpuTimestamp > pRenderer->mPrevGpuTimestamp)
    {
        double cpuDelta = currentCpuTimestamp - pRenderer->mPrevCpuTimestamp;
        double gpuDelta = currentGpuTimestamp - pRenderer->mPrevGpuTimestamp;
        pRenderer->mGpuToCpuTimestampFactor = cpuDelta / gpuDelta;
    }
    pRenderer->mPrevCpuTimestamp = currentCpuTimestamp;
    pRenderer->mPrevGpuTimestamp = currentGpuTimestamp;
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
                ASSERT((bool)pRenderer->pGpu->mCubeMapTextureArraySupported);
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

    if (pDesc->mFormat == TinyImageFormat_D24_UNORM_S8_UINT && !(pRenderer->pGpu->mFormatCaps[pDesc->mFormat] & FORMAT_CAP_RENDER_TARGET))
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
            setTextureName(pRenderer, pTexture, "Memoryless Texture");
#endif
        }
#endif

        if (pDesc->pPlacement && pRenderer->pGpu->mPlacementHeaps)
        {
            pTexture->pTexture = [pDesc->pPlacement->pHeap->pHeap newTextureWithDescriptor:textureDesc offset:pDesc->pPlacement->mOffset];
        }

        if (!pTexture->pTexture)
        {
            if (pRenderer->pGpu->mHeaps)
            {
                MTLSizeAndAlign sizeAlign = [pRenderer->pDevice heapTextureSizeAndAlignWithDescriptor:textureDesc];
                bool            useHeapPlacementHeaps = false;

                // Need to check mPlacementHeaps since it depends on gpu family as well as macOS/iOS version
                if (pRenderer->pGpu->mPlacementHeaps)
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

            if (TinyImageFormat_HasStencil(pDesc->mFormat))
            {
                MTLPixelFormat targetStencilFormat = pixelFormat;
                switch (pixelFormat)
                {
                case MTLPixelFormatStencil8:
                    targetStencilFormat = MTLPixelFormatStencil8;
                    break;
                case MTLPixelFormatDepth32Float_Stencil8:
                    targetStencilFormat = MTLPixelFormatDepth32Float_Stencil8;
                    break;
#ifndef TARGET_IOS
                case MTLPixelFormatDepth24Unorm_Stencil8:
                    targetStencilFormat = MTLPixelFormatX24_Stencil8;
                    break;
#endif
                default:
                    ASSERTFAIL("Cannot generate stencil image view");
                    break;
                }
                pTexture->pStencilTexture = [pTexture->pTexture newTextureViewWithPixelFormat:targetStencilFormat];
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
#if defined(ENABLE_GRAPHICS_DEBUG_ANNOTATION)
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

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
} // namespace RENDERER_CPP_NAMESPACE
#endif
#endif
