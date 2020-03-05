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

#ifdef METAL

#ifndef MICROPROFILE_IMPL
#define     MICROPROFILE_IMPL 1x
#endif

#define RENDERER_IMPLEMENTATION
#define MAX_FRAMES_IN_FLIGHT 3

#define VERTEX_BINDING_OFFSET 0

// Argument Buffer additional debug logging
//#define ARGUMENTBUFFER_DEBUG

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>

#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../ThirdParty/OpenSource/EASTL/string_hash_map.h"

#import "../IRenderer.h"
#include "MetalMemoryAllocator.h"
#include "../../OS/Interfaces/ILog.h"
#include "../../OS/Core/GPUConfig.h"
#include "../../OS/Interfaces/IMemory.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_bits.h"

#include "../../OS/Image/ImageHelper.h" // for GetMipMappedSize

#include "MetalCapBuilder.h"

#define MAX_BUFFER_BINDINGS             31
#define DESCRIPTOR_UPDATE_FREQ_PADDING  10

#define ARGUMENT_BUFFER_SLOT_VERTEX     0
#define ARGUMENT_BUFFER_SLOT_FRAGMENT   1
#define ARGUMENT_BUFFER_SLOT_COMPUTE    0
#define ARGUMENT_BUFFER_SLOT_COUNT      2

extern void mtl_createShaderReflection(
	Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage,
	eastl::unordered_map<uint32_t, MTLVertexFormat>* vertexAttributeFormats, ShaderReflection* pOutReflection);

typedef struct DescriptorIndexMap
{
	eastl::string_hash_map<uint32_t> mMap;
} DescriptorIndexMap;

// in Metal we must split all resources back by shaders
typedef struct ShaderDescriptors
{
	Shader*         pShader;
	
	eastl::string_hash_map<uint32_t> mDescriptorNameToIndexMap;
	
	DescriptorInfo* pDescriptors;
	uint32_t        mDescriptorCount;
} ShaderDescriptors;

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

	static const MTLSamplerAddressMode gMtlAddressModeTranslator[] =
	{
		MTLSamplerAddressModeMirrorRepeat,
		MTLSamplerAddressModeRepeat,
		MTLSamplerAddressModeClampToEdge,
#ifndef TARGET_IOS
		MTLSamplerAddressModeClampToBorderColor,
#else
		MTLSamplerAddressModeClampToEdge,
#endif
	};

// clang-format on

// -- MurmurHash3 begin --
// http://code.google.com/p/smhasher/wiki/MurmurHash3
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

inline uint32_t getblock(const uint32_t * p, int i)
{
    return p[i];
}


inline uint32_t fmix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

/// Calculates MurmurHash3 for the given key.
/**
 * \param key The key to calculate the hash of.
 * \param len Length of the key in bytes.
 * \param seed Seed for the hash.
 * \param[out] out The hash value, a uint32_t in this case.
 */
inline void MurmurHash3_x86_32(const void * key, int len, uint32_t seed, void * out)
{
    const uint8_t * data = (const uint8_t*)key;
    const int nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t * blocks = (const uint32_t *)(data + nblocks*4);

    for(int i = -nblocks; i; i++)
    {
        uint32_t k1 = getblock(blocks,i);

        k1 *= c1;
        k1 = rotl32(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1,13);
        h1 = h1*5+0xe6546b64;
    }

    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

    uint32_t k1 = 0;

    switch(len & 3)
    {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
            k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
    };

    h1 ^= len;

    h1 = fmix(h1);

    *(uint32_t*)out = h1;
}

// -- MurmurHash3 end --

// =================================================================================================
// IMPLEMENTATION
// =================================================================================================

#if defined(RENDERER_IMPLEMENTATION)

#define SAFE_FREE(p_var)         \
	if (p_var)                   \
	{                            \
		conf_free((void*)p_var); \
	}

#if defined(__cplusplus)
#define DECLARE_ZERO(type, var) type var = {};
#else
#define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
uint64_t util_pthread_to_uint64(const pthread_t& value);

bool util_is_compatible_texture_view(const MTLTextureType textureType, const MTLTextureType subviewTye);

bool            util_is_mtl_depth_pixel_format(const MTLPixelFormat format);
bool            util_is_mtl_compressed_pixel_format(const MTLPixelFormat format);
MTLVertexFormat util_to_mtl_vertex_format(const TinyImageFormat format);
MTLLoadAction   util_to_mtl_load_action(const LoadActionType loadActionType);

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT = false, const bool forceNonPrivate = false);

// GPU frame time accessor for macOS and iOS
#define GPU_FREQUENCY   1000000.0
    
@protocol CommandBufferOverride<MTLCommandBuffer>
-(CFTimeInterval) GPUStartTime;
-(CFTimeInterval) GPUEndTime;
@end

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer);
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
void removeTexture(Renderer* pRenderer, Texture* pTexture);

void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
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

static Buffer * pDefaultBuffer = {};
static Sampler* pDefaultSampler = {};

static id<MTLDepthStencilState>  pDefaultDepthState = nil;
static RasterizerStateDesc       gDefaultRasterizerState = {};

// Since there are no descriptor tables in Metal, we just hold a map of all descriptors.
using DescriptorMap = eastl::unordered_map<uint64_t, DescriptorInfo>;
using ConstDescriptorMapIterator = eastl::unordered_map<uint64_t, DescriptorInfo>::const_iterator;
using DescriptorMapIterator = eastl::unordered_map<uint64_t, DescriptorInfo>::iterator;
using DescriptorNameToIndexMap = eastl::unordered_map<uint32_t, uint32_t>;

/************************************************************************/
// Descriptor Set Structure
/************************************************************************/
enum ResourceType
{
    RESOURCE_TYPE_RESOURCE_READ_ONLY,
    RESOURCE_TYPE_RESOURCE_RW,
    RESOURCE_TYPE_HEAP,
    RESOURCE_TYPE_COUNT,
};

typedef struct MTLDescriptorSet
{
    MTLDescriptorSet()
        : mArgumentBufferDescriptors() // constructor required
        , mShadersData()
        , pRootSignature(NULL)
        , mAlignment(0)
        , mChunkSize(0)
        , mMaxSets(0)
        , mUpdateFrequency(0)
        , mNodeIndex(0)
        , mStages(SHADER_STAGE_NONE)
        , pSetResources(NULL)
        , mRootBuffers(0)
    {
    }
    
    struct ArgumentBufferDescriptor
    {
        id<MTLArgumentEncoder>          mArgumentEncoder;
		__strong id<MTLBuffer>*			mArgumentBuffers;
        ShaderStage                     mShaderStage;
    };
    eastl::unordered_map<uint32_t, ArgumentBufferDescriptor*> mArgumentBufferDescriptors;
    
    const RootSignature*                pRootSignature;
    uint32_t                            mAlignment;
    uint32_t                            mChunkSize;
    uint32_t                            mMaxSets;
    uint8_t                             mUpdateFrequency;
    uint8_t                             mNodeIndex;
    uint16_t                            mPadA;
    
    ShaderStage                         mStages;
    
    struct DescriptorResources
    {
        void**                          mResources[RESOURCE_TYPE_COUNT];
        uint32_t                        mResourcesCount[RESOURCE_TYPE_COUNT];
    };
    DescriptorResources*                pSetResources;
    
    struct RootBuffer
    {
        Buffer*                         mBuffer;
        uint64_t                        mOffset;
        uint32_t                        mBufferIndex;
    };
    eastl::vector<RootBuffer>           mRootBuffers;
    
    struct ShaderData {
        void*                           pArgumentBufferDescriptor[DESCRIPTOR_UPDATE_FREQ_COUNT][2];
    };
    eastl::unordered_map<void*, ShaderData> mShadersData;

#ifdef TARGET_IOS
	id<MTLTexture> __weak**             ppRWTextures;
	DescriptorInfo***                   ppRWTextureDescriptors;
	uint8_t                             mRWTextureCount;
#endif
} MTLDescriptorSet;

typedef eastl::unordered_set<void*> ResourcesList[RESOURCE_TYPE_COUNT];

const DescriptorInfo* get_descriptor(const RootSignature* pRootSignature, const char* pResName, uint32_t* pIndex)
{
	decltype(pRootSignature->pDescriptorNameToIndexMap->mMap)::const_iterator it = pRootSignature->pDescriptorNameToIndexMap->mMap.find(pResName);
	if (it != pRootSignature->pDescriptorNameToIndexMap->mMap.end())
	{
		*pIndex = it->second;
		return &pRootSignature->pDescriptors[it->second];
	}
	else
	{
        LOGF(LogLevel::eERROR, "Invalid descriptor param (%s)", pResName);
		return NULL;
	}
}

const DescriptorInfo* get_descriptor_for_shader(const ShaderDescriptors* pShader, const char* pResName, uint32_t* pIndex)
{
    decltype(pShader->mDescriptorNameToIndexMap)::const_iterator it = pShader->mDescriptorNameToIndexMap.find(pResName);
    if (it != pShader->mDescriptorNameToIndexMap.end())
    {
        *pIndex = it->second;
        return &pShader->pDescriptors[it->second];
    }
    else
    {
        return NULL;
    }
}
/************************************************************************/
// Misc
/************************************************************************/
enum BarrierFlag
{
	BARRIER_FLAG_BUFFERS		= 0x1,
	BARRIER_FLAG_TEXTURES		= 0x2,
	BARRIER_FLAG_RENDERTARGETS	= 0x4,
	BARRIER_FLAG_FENCE			= 0x8,
};
	
/************************************************************************/
// Internal functions
/************************************************************************/
void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_update(Cmd* pCmd, const QueueType& encoderType);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);
void util_bind_push_constant(Cmd* pCmd, const DescriptorInfo* pDesc, const void* pConstants);

id<MTLBuffer> util_get_buffer_from_descriptorset(DescriptorSet* pDescriptorSet, uint32_t index);
void util_set_resources_graphics(Cmd* pCmd, MTLDescriptorSet::DescriptorResources* resources, uint32_t stages);
void util_set_resources_compute(Cmd* pCmd, MTLDescriptorSet::DescriptorResources* resources);
void util_set_argument(DescriptorSet* pDescriptorSet, const DescriptorInfo* pDesc, const MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor, uint32_t index,const DescriptorData* pParam, ResourcesList& resources);

void cmdBindDescriptorSet(Cmd* pCmd, uint32_t index, DescriptorSet* pDescriptorSet)
{
    ASSERT(pCmd);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->pMtl->mMaxSets);

    //const size_t offset(pDescriptorSet->pMtl->mArgumentBuffer->mPositionInHeap + (pDescriptorSet->pMtl->mChunkSize * index));
    const size_t offset(0);
        
    // rootcbv
    if (pDescriptorSet->pMtl->mRootBuffers.size())
    {
        for (uint32_t i = 0; i < pDescriptorSet->pMtl->mRootBuffers.size(); ++i)
        {
            const MTLDescriptorSet::RootBuffer& buffer(pDescriptorSet->pMtl->mRootBuffers[i]);
            
            //if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_VERT) // todo
            {
                [pCmd->mtlRenderEncoder setVertexBuffer: buffer.mBuffer->mtlBuffer
                                                 offset: buffer.mOffset
                                                atIndex: buffer.mBufferIndex];
            }
            
            //if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_FRAG) // todo
            {
                [pCmd->mtlRenderEncoder setFragmentBuffer: buffer.mBuffer->mtlBuffer
                                                   offset: buffer.mOffset
                                                  atIndex: buffer.mBufferIndex];
            }
            
            //if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_COMP) // todo
            {
                [pCmd->mtlComputeEncoder setBuffer: buffer.mBuffer->mtlBuffer
                                            offset: buffer.mOffset
                                           atIndex: buffer.mBufferIndex];
            }
        }
    }
	
	// #NOTE: Support for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
	for (uint32_t i = 0; i < pDescriptorSet->pMtl->mRWTextureCount; ++i)
	{
		const DescriptorInfo* pDesc = pDescriptorSet->pMtl->ppRWTextureDescriptors[index][i];
		
		if(pDesc)
		{
			if (pDesc->mUsedStages & SHADER_STAGE_VERT)
			{
				[pCmd->mtlRenderEncoder setVertexTexture: pDescriptorSet->pMtl->ppRWTextures[index][i]
											   atIndex: pDesc->mReg];
			}
			
			if (pDesc->mUsedStages & SHADER_STAGE_FRAG)
			{
				[pCmd->mtlRenderEncoder setFragmentTexture: pDescriptorSet->pMtl->ppRWTextures[index][i]
												 atIndex: pDesc->mReg];
			}

			if (pDesc->mUsedStages & SHADER_STAGE_COMP)
			{
				[pCmd->mtlComputeEncoder setTexture: pDescriptorSet->pMtl->ppRWTextures[index][i]
										  atIndex: pDesc->mReg];
			}
		}
	}
#endif
    
    const uint32_t argBuffersCount((uint32_t)pDescriptorSet->pMtl->mArgumentBufferDescriptors.size());
    
    if (argBuffersCount > 1)
    {
        ASSERT(pCmd->pShader); // shader required. need to set pipline object first
        
        for (uint32_t sh = 0; sh < pDescriptorSet->pMtl->pRootSignature->mShaderDescriptorsCount; ++sh)
        {
            ShaderDescriptors& shaderDescriptors(pDescriptorSet->pMtl->pRootSignature->pShaderDescriptors[sh]);
                        
            if (shaderDescriptors.pShader == pCmd->pShader)
            {
                const MTLDescriptorSet::ShaderData& shaderData = pDescriptorSet->pMtl->mShadersData[(void*)&shaderDescriptors];

                for (uint32_t slot = 0; slot < ARGUMENT_BUFFER_SLOT_COUNT; ++slot)
                {
                    const MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor((MTLDescriptorSet::ArgumentBufferDescriptor*)shaderData.pArgumentBufferDescriptor[pDescriptorSet->pMtl->mUpdateFrequency - DESCRIPTOR_UPDATE_FREQ_PADDING][slot]);
                    
                    if (argumentBufferDescriptor)
                    {
                        // argument buffers
                        if (argumentBufferDescriptor->mShaderStage & SHADER_STAGE_VERT)
                        {
                            [pCmd->mtlRenderEncoder setVertexBuffer: argumentBufferDescriptor->mArgumentBuffers[index]
                                                             offset: offset
                                                            atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                            
                            util_set_resources_graphics(pCmd, &pDescriptorSet->pMtl->pSetResources[index], MTLRenderStageVertex);
                        }

                        if (argumentBufferDescriptor->mShaderStage & SHADER_STAGE_FRAG)
                        {
                            [pCmd->mtlRenderEncoder setFragmentBuffer: argumentBufferDescriptor->mArgumentBuffers[index]
                                                               offset: offset
                                                              atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                            
                            util_set_resources_graphics(pCmd, &pDescriptorSet->pMtl->pSetResources[index], MTLRenderStageFragment);
                        }
                            
                        if (argumentBufferDescriptor->mShaderStage & SHADER_STAGE_COMP)
                        {
                            [pCmd->mtlComputeEncoder setBuffer: argumentBufferDescriptor->mArgumentBuffers[index]
                                                        offset: offset
                                                       atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                            
                            util_set_resources_compute(pCmd, &pDescriptorSet->pMtl->pSetResources[index]);
                        }
                    }
                }
                
                break;
            }
        }
    }
    else
    {
        for (auto it = pDescriptorSet->pMtl->mArgumentBufferDescriptors.begin(); it != pDescriptorSet->pMtl->mArgumentBufferDescriptors.end(); ++it)
        {
            const MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor(it->second);
            const id<MTLBuffer> buffer(argumentBufferDescriptor->mArgumentBuffers[index]);
            
            // argument buffers
            if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_VERT)
            {
                [pCmd->mtlRenderEncoder setVertexBuffer: buffer
                                                 offset: offset
                                                atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                
                util_set_resources_graphics(pCmd, &pDescriptorSet->pMtl->pSetResources[index], MTLRenderStageVertex);
            }

            if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_FRAG)
            {
                [pCmd->mtlRenderEncoder setFragmentBuffer: buffer
                                                   offset: offset
                                                  atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                
                util_set_resources_graphics(pCmd, &pDescriptorSet->pMtl->pSetResources[index], MTLRenderStageFragment);
            }
                
            if (pDescriptorSet->pMtl->mStages & SHADER_STAGE_COMP)
            {
                [pCmd->mtlComputeEncoder setBuffer: buffer
                                            offset: offset
                                           atIndex: pDescriptorSet->pMtl->mUpdateFrequency];
                
                util_set_resources_compute(pCmd, &pDescriptorSet->pMtl->pSetResources[index]);
            }
        }
    }
}

//
// Push Constants
//
void cmdBindPushConstants(Cmd* pCmd, RootSignature* pRootSignature, const char* pName, const void* pConstants)
{
    ASSERT(pCmd);
    ASSERT(pRootSignature);
    ASSERT(pName);
    
    uint32_t descIndex = -1;
    
    for (uint32_t i = 0; i < pRootSignature->mShaderDescriptorsCount; ++i)
    {
        const ShaderDescriptors& shaderPair(pRootSignature->pShaderDescriptors[i]);
        
        const DescriptorInfo* pDesc = get_descriptor_for_shader(&shaderPair, pName, &descIndex);
        
        if(pDesc)
        {
            util_bind_push_constant(pCmd, pDesc, pConstants);
        }
    }
}

void cmdBindPushConstantsByIndex(Cmd* pCmd, RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
    ASSERT(pCmd);
    ASSERT(pRootSignature);
    ASSERT(paramIndex != (uint32_t)-1);
    
    const RootSignature::IndexedDescriptor& indexedDescriptor(pRootSignature->mIndexedDescriptorInfo[paramIndex]);
    
    for (uint32_t i = 0; i < indexedDescriptor.mDescriptorCount; ++i)
    {
        const DescriptorInfo* pDesc = indexedDescriptor.pDescriptors[i];

        if(pDesc)
        {
            util_bind_push_constant(pCmd, pDesc, pConstants);
        }
    }
}

//
// Add DescriptorSet
//
uint32_t hash_descriptor(const ArgumentDescriptor& descriptor)
{
    uint32 result = 0;
    
    result = (uint32_t)descriptor.mDataType;
    result |= descriptor.mBufferIndex << 4;
    result |= descriptor.mArgumentIndex << 8;
    result |= descriptor.mAccessType << 12;
    result |= descriptor.mTextureType << 16;
    result |= descriptor.mArrayLength << 20;
    
    return result;
}

void hash_combine(uint32_t* seed, uint32_t value)
{
    (*seed) ^= value + 0x9e3779b9 + ((*seed) << 6) + ((*seed) >> 2);
}

MTLDescriptorSet::ArgumentBufferDescriptor* util_add_shader_descriptor(Renderer* pRenderer, DescriptorSet* pDescriptorSet, const DescriptorSetDesc* pDesc, uint32_t hash, NSArray* descriptors, ShaderStage shaderStage, Shader* shader)
{
    ASSERT(hash);
    ASSERT(descriptors.count);
    
    auto it = pDescriptorSet->pMtl->mArgumentBufferDescriptors.find(hash);
    
    if (it != pDescriptorSet->pMtl->mArgumentBufferDescriptors.end())
    {
        it->second->mShaderStage |= shaderStage;
        
        return it->second;
    }
    else
    {
        NSArray *sortedArray;
        sortedArray = [descriptors sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
            MTLArgumentDescriptor *first = a;
            MTLArgumentDescriptor *second = b;
            return (NSComparisonResult)(first.index > second.index);
        }];
        ASSERT(sortedArray.count);
        
        MTLDescriptorSet::ArgumentBufferDescriptor* newBufferDescriptor = (MTLDescriptorSet::ArgumentBufferDescriptor*)conf_calloc(1, sizeof(MTLDescriptorSet::ArgumentBufferDescriptor));
        
        newBufferDescriptor->mShaderStage = shaderStage;
        
        // create encoder
        if (shader == nullptr)
        {
            newBufferDescriptor->mArgumentEncoder = [pRenderer->pDevice newArgumentEncoderWithArguments: sortedArray];
        }
        else
        {
            switch (shaderStage)
            {
                case SHADER_STAGE_VERT:
                    newBufferDescriptor->mArgumentEncoder = [shader->mtlVertexShader newArgumentEncoderWithBufferIndex:pDescriptorSet->pMtl->mUpdateFrequency];
                    break;
                case SHADER_STAGE_FRAG:
                    newBufferDescriptor->mArgumentEncoder = [shader->mtlFragmentShader newArgumentEncoderWithBufferIndex:pDescriptorSet->pMtl->mUpdateFrequency];
                    break;
                case SHADER_STAGE_COMP:
                    newBufferDescriptor->mArgumentEncoder = [shader->mtlComputeShader newArgumentEncoderWithBufferIndex:pDescriptorSet->pMtl->mUpdateFrequency];
                    break;
                default:
                    ASSERT(0);
            }
        }
        
        ASSERT(newBufferDescriptor->mArgumentEncoder);
        
        // create buffers
		newBufferDescriptor->mArgumentBuffers = (__strong id<MTLBuffer>*)conf_calloc(pDesc->mMaxSets, sizeof(id<MTLBuffer>));
        for (uint32_t i = 0; i < pDesc->mMaxSets; ++i)
        {
			newBufferDescriptor->mArgumentBuffers[i] = [pRenderer->pResourceAllocator->m_Device newBufferWithLength:newBufferDescriptor->mArgumentEncoder.encodedLength options:MTLResourceStorageModeShared];
        }

        
        pDescriptorSet->pMtl->mArgumentBufferDescriptors[hash] = newBufferDescriptor;
        
        return newBufferDescriptor;
    }
}

void addDescriptorSet(Renderer* pRenderer, const DescriptorSetDesc* pDesc, DescriptorSet** ppDescriptorSet)
{
    ASSERT(pRenderer);
    ASSERT(pDesc);
    ASSERT(ppDescriptorSet);
    
    DescriptorSet* pDescriptorSet = (DescriptorSet*)conf_calloc(1, sizeof(*pDescriptorSet) + sizeof(MTLDescriptorSet));
    ASSERT(pDescriptorSet);
    
	pDescriptorSet->pMtl = (MTLDescriptorSet*)(pDescriptorSet + 1);
    conf_placement_new<MTLDescriptorSet>(pDescriptorSet->pMtl); // need it to initialize hash map
    
    const RootSignature* pRootSignature(pDesc->pRootSignature);
    const uint32_t updateFreq(pDesc->mUpdateFrequency + DESCRIPTOR_UPDATE_FREQ_PADDING);
    const uint32_t nodeIndex = pDesc->mNodeIndex;
    
    pDescriptorSet->pMtl->pRootSignature = pRootSignature;
    pDescriptorSet->pMtl->mUpdateFrequency = updateFreq;
    pDescriptorSet->pMtl->mNodeIndex = nodeIndex;
    pDescriptorSet->pMtl->mMaxSets = pDesc->mMaxSets;

    pDescriptorSet->pMtl->pSetResources = (MTLDescriptorSet::DescriptorResources*)conf_calloc(pDescriptorSet->pMtl->mMaxSets, sizeof(MTLDescriptorSet::DescriptorResources));
    
    // combine argument encoder
    //todo: dscriptors by shaders
    NSMutableArray<MTLArgumentDescriptor*>* descriptorsVs = [[NSMutableArray alloc] initWithCapacity:10];
    NSMutableArray<MTLArgumentDescriptor*>* descriptorsFs = [[NSMutableArray alloc] initWithCapacity:10];
    NSMutableArray<MTLArgumentDescriptor*>* descriptorsCs = [[NSMutableArray alloc] initWithCapacity:10];
    
    uint32_t hashVs = 0;
    uint32_t hashFs = 0;
    uint32_t hashCs = 0;
    
    bool createWorkaround = false;

    for (uint32_t sh = 0; sh < pRootSignature->mShaderDescriptorsCount; ++sh)
    {
#ifdef ARGUMENTBUFFER_DEBUG
        LOGF(LogLevel::eINFO, "> NEW ARGUMENT BUFFER");
#endif

        const ShaderDescriptors& shaderPair(pRootSignature->pShaderDescriptors[sh]);
        
        for (uint32_t i = 0; i < shaderPair.mDescriptorCount; ++i)
        {
            const DescriptorInfo& descriptorInfo(shaderPair.pDescriptors[i]);
            
			// #NOTE: Support for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
			if (descriptorInfo.mType == DESCRIPTOR_TYPE_RW_TEXTURE)
			{
				++pDescriptorSet->pMtl->mRWTextureCount;
				continue;
			}
#endif
            if (descriptorInfo.mType != DESCRIPTOR_TYPE_ARGUMENT_BUFFER)
            {
                const ArgumentDescriptor& memberDescriptor(*descriptorInfo.mtlArgumentDescriptors);
                
                if (memberDescriptor.mBufferIndex == updateFreq)
                {
                    uint32_t hash = hash_descriptor(memberDescriptor);
                    
                    createWorkaround = (memberDescriptor.mDataType == MTLDataTypeIndirectCommandBuffer) || (memberDescriptor.mDataType == MTLDataTypeRenderPipeline);
                    
                    MTLArgumentDescriptor* metalDescriptor = [MTLArgumentDescriptor argumentDescriptor];
                    metalDescriptor.access = memberDescriptor.mAccessType;
                    metalDescriptor.arrayLength = memberDescriptor.mArrayLength;
                    metalDescriptor.constantBlockAlignment = memberDescriptor.mAlignment;
                    metalDescriptor.dataType = memberDescriptor.mDataType;
                    metalDescriptor.index = memberDescriptor.mArgumentIndex;
                    metalDescriptor.textureType = memberDescriptor.mTextureType;
                    
                    ASSERT(metalDescriptor.dataType != MTLDataTypeNone);
                    
                    uint32_t nameHash = 0;
                    MurmurHash3_x86_32(descriptorInfo.pName, (uint32_t)strlen(descriptorInfo.pName), 0, &nameHash);
                    
                    if (descriptorInfo.mUsedStages & SHADER_STAGE_VERT)
                    {
                        [descriptorsVs addObject:metalDescriptor];
                        hash_combine(&hashVs, hash);
                        hash_combine(&hashVs, nameHash);
                        
#ifdef ARGUMENTBUFFER_DEBUG
                        LOGF(LogLevel::eINFO, "    %s [at %d] -> VS (hash %d)", shaderResourceDesc.name, memberDescriptor.mArgumentIndex, hashVs);
#endif
                    }
                    
                    if (descriptorInfo.mUsedStages & SHADER_STAGE_FRAG)
                    {
                        [descriptorsFs addObject:metalDescriptor];
                        hash_combine(&hashFs, hash);
                        hash_combine(&hashFs, nameHash);
                        
#ifdef ARGUMENTBUFFER_DEBUG
                        LOGF(LogLevel::eINFO, "    %s [at %d] -> FS (hash %d)", shaderResourceDesc.name, memberDescriptor.mArgumentIndex, hashFs);
#endif
                    }
                    
                    if (descriptorInfo.mUsedStages & SHADER_STAGE_COMP)
                    {
                        [descriptorsCs addObject:metalDescriptor];
                        hash_combine(&hashCs, hash);
                        hash_combine(&hashCs, nameHash);
                        
#ifdef ARGUMENTBUFFER_DEBUG
                        LOGF(LogLevel::eINFO, "    %s [at %d] -> CS (hash %d)", shaderResourceDesc.name, memberDescriptor.mArgumentIndex, hashCs);
#endif
                    }
                }
            }
        }
        
        MTLDescriptorSet::ShaderData shaderData = { 0 };
        if (descriptorsVs.count)
        {
            ASSERT(descriptorsCs.count == 0);
            
            ASSERT(shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_VERTEX] == NULL);
            
            shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_VERTEX] = util_add_shader_descriptor(pRenderer, pDescriptorSet, pDesc, hashVs, descriptorsVs, SHADER_STAGE_VERT, createWorkaround ? shaderPair.pShader : nullptr);
            descriptorsVs = [[NSMutableArray alloc] initWithCapacity:10];
            hashVs = 0;
            
#ifdef ARGUMENTBUFFER_DEBUG
            LOGF(LogLevel::eINFO, "> VS BufferDesc (%d): %p", pDesc->mUpdateFrequency, shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_VERTEX]);
#endif

            pDescriptorSet->pMtl->mStages |= SHADER_STAGE_VERT;
        }
        else if (descriptorsCs.count)
        {
            ASSERT(shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_COMPUTE] == NULL);
            
            shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_COMPUTE] = util_add_shader_descriptor(pRenderer, pDescriptorSet, pDesc, hashCs, descriptorsCs, SHADER_STAGE_COMP, createWorkaround ? shaderPair.pShader : nullptr);
            descriptorsCs = [[NSMutableArray alloc] initWithCapacity:10];
            hashCs = 0;

#ifdef ARGUMENTBUFFER_DEBUG
            LOGF(LogLevel::eINFO, "> CS BufferDesc (%d): %p", pDesc->mUpdateFrequency, shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_COMPUTE]);
#endif

            pDescriptorSet->pMtl->mStages |= SHADER_STAGE_COMP;
        }
        else
        {
            shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_VERTEX] = NULL;
        }
        
        if (descriptorsFs.count)
        {
            ASSERT(descriptorsCs.count == 0);
            
            ASSERT(shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_FRAGMENT] == NULL);
            
            shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_FRAGMENT] = util_add_shader_descriptor(pRenderer, pDescriptorSet, pDesc, hashFs, descriptorsFs, SHADER_STAGE_FRAG, createWorkaround ? shaderPair.pShader : nullptr);
            descriptorsFs = [[NSMutableArray alloc] initWithCapacity:10];
            hashFs = 0;

#ifdef ARGUMENTBUFFER_DEBUG
            LOGF(LogLevel::eINFO, "> FS BufferDesc (%d): %p", pDesc->mUpdateFrequency, shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_FRAGMENT]);
#endif

            pDescriptorSet->pMtl->mStages |= SHADER_STAGE_FRAG;
        }
        else
        {
            shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_FRAGMENT] = NULL;
        }
        
        if (pDescriptorSet->pMtl->mStages != SHADER_STAGE_NONE)
        {
            pDescriptorSet->pMtl->mShadersData[(void*)&shaderPair] = shaderData;
            
            // bind static samplers
            // todo: only if new buffer
            for (uint32_t i = 0; i < shaderPair.mDescriptorCount; ++i)
            {
                const DescriptorInfo& descriptorInfo(shaderPair.pDescriptors[i]);
                
                if (descriptorInfo.mReg == updateFreq &&
                    descriptorInfo.mType == DESCRIPTOR_TYPE_SAMPLER &&
                    descriptorInfo.mtlStaticSampler)
                {
                    //for (uint32_t k = 0; k < 2; ++k)
                    {
                        MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor((MTLDescriptorSet::ArgumentBufferDescriptor*)shaderData.pArgumentBufferDescriptor[pDesc->mUpdateFrequency][ARGUMENT_BUFFER_SLOT_FRAGMENT]);
                        
                        if (argumentBufferDescriptor)
                        {
                            for (uint32_t j = 0; j < pDescriptorSet->pMtl->mMaxSets; ++j)
                            {
                                [argumentBufferDescriptor->mArgumentEncoder setArgumentBuffer: argumentBufferDescriptor->mArgumentBuffers[j]
                                                                                       offset: 0];
                                [argumentBufferDescriptor->mArgumentEncoder setSamplerState: descriptorInfo.mtlStaticSampler
                                                                                    atIndex: descriptorInfo.mHandleIndex];
                            }
                        }
                    }
                }
            }
        }
    }
	
	// #NOTE: Support for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
	pDescriptorSet->pMtl->ppRWTextures = (id<MTLTexture> __weak**)conf_calloc(pDescriptorSet->pMtl->mMaxSets, sizeof(id<MTLTexture>*));
	pDescriptorSet->pMtl->ppRWTextureDescriptors = (DescriptorInfo***)conf_calloc(pDescriptorSet->pMtl->mMaxSets, sizeof(DescriptorInfo**));
	ASSERT(pDescriptorSet->pMtl->ppRWTextures);
	ASSERT(pDescriptorSet->pMtl->ppRWTextureDescriptors);
	for (uint32_t i = 0; i < pDescriptorSet->pMtl->mMaxSets; ++i)
	{
		pDescriptorSet->pMtl->ppRWTextures[i] = (id<MTLTexture> __weak*)conf_calloc(pDescriptorSet->pMtl->mRWTextureCount, sizeof(id<MTLTexture>));
		pDescriptorSet->pMtl->ppRWTextureDescriptors[i] = (DescriptorInfo**)conf_calloc(pDescriptorSet->pMtl->mRWTextureCount, sizeof(DescriptorInfo*));
		ASSERT(pDescriptorSet->pMtl->ppRWTextures[i]);
		ASSERT(pDescriptorSet->pMtl->ppRWTextureDescriptors[i]);
	}
#endif
    
    *ppDescriptorSet = pDescriptorSet;
}

void removeDescriptorSet(Renderer* pRenderer, DescriptorSet* pDescriptorSet)
{    
    for (eastl::unordered_map<uint32_t, MTLDescriptorSet::ArgumentBufferDescriptor*>::const_iterator it = pDescriptorSet->pMtl->mArgumentBufferDescriptors.begin();
		 it != pDescriptorSet->pMtl->mArgumentBufferDescriptors.end(); ++it)
    {
        it->second->mArgumentEncoder = nil;
        for (uint32_t i = 0; i < pDescriptorSet->pMtl->mMaxSets; ++i)
        {
			it->second->mArgumentBuffers[i] = nil;
		}
		conf_free(it->second->mArgumentBuffers);
		it->second->mArgumentBuffers = nullptr;
        
		it->second->~ArgumentBufferDescriptor();
        conf_free(it->second);
    }
    
    pDescriptorSet->pMtl->mArgumentBufferDescriptors.clear();
    
    pDescriptorSet->pMtl->mRootBuffers.clear();

    for (uint32_t i = 0; i < pDescriptorSet->pMtl->mMaxSets; ++i)
    {
        for (uint32_t j = 0; j < RESOURCE_TYPE_COUNT; ++j)
        {
            pDescriptorSet->pMtl->pSetResources[i].mResourcesCount[j] = 0;
            conf_free(pDescriptorSet->pMtl->pSetResources[i].mResources[j]);
        }
    }
    
    SAFE_FREE(pDescriptorSet->pMtl->pSetResources);
    
	// #NOTE: Support for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
	for (uint32_t i = 0; i < pDescriptorSet->pMtl->mMaxSets; ++i)
	{
		SAFE_FREE(pDescriptorSet->pMtl->ppRWTextures[i]);
		SAFE_FREE(pDescriptorSet->pMtl->ppRWTextureDescriptors[i]);
	}
	SAFE_FREE(pDescriptorSet->pMtl->ppRWTextures);
	SAFE_FREE(pDescriptorSet->pMtl->ppRWTextureDescriptors);
#endif
	
	pDescriptorSet->pMtl->~MTLDescriptorSet();
    SAFE_FREE(pDescriptorSet);
}

void getDescriptorIndex(RootSignature* pRootSignature, const char* pName, uint32_t* pOutIndex)
{
    ASSERT(pRootSignature);
    ASSERT(pName);
    ASSERT(pOutIndex);
    
    const DescriptorInfo* pDesc = NULL;
    uint32_t paramIndex;
    
    eastl::vector<const DescriptorInfo*> descriptors;
    
    // collect all
    for (uint32_t sh = 0; sh < pRootSignature->mShaderDescriptorsCount; ++sh)
    {
        ShaderDescriptors& shaderPair(pRootSignature->pShaderDescriptors[sh]);
        
        pDesc = get_descriptor_for_shader(&shaderPair, pName, &paramIndex);
        if (pDesc)
        {
            descriptors.push_back(pDesc);
        }
    }
    
    if (descriptors.size())
    {
        RootSignature::IndexedDescriptor info;
        
        info.mDescriptorCount = (uint32_t)descriptors.size();
        info.pDescriptors = (const DescriptorInfo**)conf_calloc(info.mDescriptorCount, sizeof(DescriptorInfo));
        
        for (uint32_t i = 0; i < info.mDescriptorCount; ++i)
        {
            info.pDescriptors[i] = descriptors[i];
        }
        
		pRootSignature->mIndexedDescriptorInfo = (RootSignature::IndexedDescriptor*) conf_realloc(pRootSignature->mIndexedDescriptorInfo, pRootSignature->mIndexedDescriptorCount + 1);
		pRootSignature->mIndexedDescriptorInfo[pRootSignature->mIndexedDescriptorCount++] = info;
        
        *pOutIndex = (uint32_t)(pRootSignature->mIndexedDescriptorCount) - 1;
    }
    else
    {
        *pOutIndex = 0;
    }
}

void updateDescriptorSet(Renderer* pRenderer, uint32_t index, DescriptorSet* pDescriptorSet, uint32_t count, const DescriptorData* pParams)
{
    ASSERT(pRenderer);
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->pMtl->mMaxSets);
    
    // resources
    ResourcesList resources;
    
    const RootSignature* pRootSignature = pDescriptorSet->pMtl->pRootSignature;
    
#ifdef ARGUMENTBUFFER_DEBUG
	LOGF(LogLevel::eWARNING, "updateDescriptorSet()");
#endif
	
    for (auto it = pDescriptorSet->pMtl->mArgumentBufferDescriptors.begin(); it != pDescriptorSet->pMtl->mArgumentBufferDescriptors.end(); ++it)
    {
        const MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor(it->second);
        
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t paramIndex = pParams->mIndex;
            
            const DescriptorData* pParam(pParams + i);
            const DescriptorInfo* pDesc = NULL;
            
            if (paramIndex != (uint32_t)-1)
            {
                const RootSignature::IndexedDescriptor& info(pRootSignature->mIndexedDescriptorInfo[paramIndex]);
                
                for (uint32_t i = 0; i < info.mDescriptorCount; ++i)
                {
                    pDesc = info.pDescriptors[i];
                    
                    util_set_argument(pDescriptorSet, pDesc, argumentBufferDescriptor, index, pParam, resources);
                }
            }
            else
            {
                // find first occurance
                bool found = false;
                for (uint32_t sh = 0; sh < pRootSignature->mShaderDescriptorsCount; ++sh)
                {
                    const ShaderDescriptors& shaderPair(pRootSignature->pShaderDescriptors[sh]);
                    
                    pDesc = get_descriptor_for_shader(&shaderPair, pParam->pName, &paramIndex);
                    
                    if (pDesc)
                    {
                        // check desc aviability
                        if (pDesc->mIsArgumentBufferField)
                        {
                            MTLDescriptorSet::ShaderData& shaderData(pDescriptorSet->pMtl->mShadersData[(void*)&shaderPair]);
                            
                            if (pDesc->mUsedStages & SHADER_STAGE_VERT || pDesc->mUsedStages & SHADER_STAGE_COMP)
                            {
                                found = (shaderData.pArgumentBufferDescriptor[pDescriptorSet->pMtl->mUpdateFrequency - DESCRIPTOR_UPDATE_FREQ_PADDING][ARGUMENT_BUFFER_SLOT_VERTEX] == argumentBufferDescriptor);
                            }
                            
                            if (!found && pDesc->mUsedStages & SHADER_STAGE_FRAG)
                            {
                                found = (shaderData.pArgumentBufferDescriptor[pDescriptorSet->pMtl->mUpdateFrequency - DESCRIPTOR_UPDATE_FREQ_PADDING][ARGUMENT_BUFFER_SLOT_FRAGMENT] == argumentBufferDescriptor);
                            }
                        } else {
                            // not ant argument buffer parameter (3d font hacks)
                            found = true;
                        }
                        
                        if (found)
                            break;
                        else
                            pDesc = NULL;
                    }
                }
                
                // param not in buffer
                if (!pDesc)
                    continue;
                
				// #NOTE: Spport for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
				if (pDesc->mType == DESCRIPTOR_TYPE_RW_TEXTURE)
					continue;
#endif
                util_set_argument(pDescriptorSet, pDesc, argumentBufferDescriptor, index, pParam, resources);
            }
        }
    }
        
    // prepare resources list
    uint32_t n = 0;
    
    for (uint32_t i = 0; i < RESOURCE_TYPE_COUNT; ++i)
    {
        if (pDescriptorSet->pMtl->pSetResources[index].mResourcesCount[i])
        {
            // todo: do not resize if possible
            conf_free(pDescriptorSet->pMtl->pSetResources[index].mResources[i]);
            pDescriptorSet->pMtl->pSetResources[index].mResources[i] = NULL;
        }
        
        pDescriptorSet->pMtl->pSetResources[index].mResourcesCount[i] = static_cast<uint32_t>(resources[i].size());
        if (pDescriptorSet->pMtl->pSetResources[index].mResourcesCount[i])
        {
            n = 0;
            pDescriptorSet->pMtl->pSetResources[index].mResources[i] = (void**)conf_malloc(pDescriptorSet->pMtl->pSetResources[index].mResourcesCount[i] * sizeof(void*));
            for (eastl::unordered_set<void*>::const_iterator it = resources[i].begin(); it != resources[i].end(); ++it)
            {
                pDescriptorSet->pMtl->pSetResources[index].mResources[i][n++] = *it;
            }
        }
    }

	// #NOTE: Spport for RW textures on iOS until they are supported through argument buffers
#ifdef TARGET_IOS
	if (pDescriptorSet->pMtl->mRWTextureCount)
	{
		uint8_t rwCount = 0;
		uint32_t descIndex = -1;
		for (uint32_t i = 0; i < count; ++i)
		{
			const DescriptorInfo* pDesc = NULL;
            if (pParams[i].mIndex != (uint32_t)-1)
            {
                const RootSignature::IndexedDescriptor& info(pDescriptorSet->pMtl->pRootSignature->mIndexedDescriptorInfo[pParams[i].mIndex]);
				pDesc = info.pDescriptors[i];
            }
			else
			{
				pDesc = get_descriptor_for_shader(&pDescriptorSet->pMtl->pRootSignature->pShaderDescriptors[0], pParams[i].pName, &descIndex);
			}
			
			if(pDesc && pDesc->mType == DESCRIPTOR_TYPE_RW_TEXTURE)
			{
				pDescriptorSet->pMtl->ppRWTextures[index][rwCount] = pParams[i].ppTextures[0]->pMtlUAVDescriptors[pParams[i].mUAVMipSlice];
				pDescriptorSet->pMtl->ppRWTextureDescriptors[index][rwCount] = (DescriptorInfo*)pDesc;
				++rwCount;
			}
		}
	}
#endif
}


/************************************************************************/
// Logging
/************************************************************************/
// Proxy log callback
static void internal_log(LogType type, const char* msg, const char* component)
{
	switch (type)
	{
		case LOG_TYPE_INFO: LOGF(LogLevel::eINFO, "%s ( %s )", component, msg); break;
		case LOG_TYPE_WARN: LOGF(LogLevel::eWARNING, "%s ( %s )", component, msg); break;
		case LOG_TYPE_DEBUG: LOGF(LogLevel::eDEBUG, "%s ( %s )", component, msg); break;
		case LOG_TYPE_ERROR: LOGF(LogLevel::eERROR, "%s ( %s )", component, msg); break;
		default: break;
	}
}

// Resource allocation statistics.
void calculateMemoryStats(Renderer* pRenderer, char** stats) { resourceAllocBuildStatsString(pRenderer->pResourceAllocator, stats, 0); }
void freeMemoryStats(Renderer* pRenderer, char* stats) { resourceAllocFreeStatsString(pRenderer->pResourceAllocator, stats); }
/************************************************************************/
// Pipeline state functions
/************************************************************************/
void util_to_blend_desc(const BlendStateDesc* pDesc, MTLRenderPipelineColorAttachmentDescriptorArray* attachments)
{
	int blendDescIndex = 0;
#ifdef _DEBUG
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
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
	for (int i = 0; i < MAX_RENDER_TARGET_ATTACHMENTS; ++i)
	{
		if (pDesc->mRenderTargetMask & (1 << i))
		{
			attachments[blendDescIndex].blendingEnabled = true;
			attachments[blendDescIndex].rgbBlendOperation = gMtlBlendOpTranslator[pDesc->mBlendModes[blendDescIndex]];
			attachments[blendDescIndex].alphaBlendOperation = gMtlBlendOpTranslator[pDesc->mBlendAlphaModes[blendDescIndex]];
			attachments[blendDescIndex].sourceRGBBlendFactor = gMtlBlendConstantTranslator[pDesc->mSrcFactors[blendDescIndex]];
			attachments[blendDescIndex].destinationRGBBlendFactor = gMtlBlendConstantTranslator[pDesc->mDstFactors[blendDescIndex]];
			attachments[blendDescIndex].sourceAlphaBlendFactor = gMtlBlendConstantTranslator[pDesc->mSrcAlphaFactors[blendDescIndex]];
			attachments[blendDescIndex].destinationAlphaBlendFactor = gMtlBlendConstantTranslator[pDesc->mDstAlphaFactors[blendDescIndex]];
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
	descriptor.backFaceStencil.stencilCompareFunction = pDesc->mStencilTest ? gMtlComparisonFunctionTranslator[pDesc->mStencilBackFunc] : MTLCompareFunctionAlways;
	descriptor.backFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthBackFail];
	descriptor.backFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilBackFail];
	descriptor.backFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilBackPass];
	descriptor.backFaceStencil.readMask = pDesc->mStencilReadMask;
	descriptor.backFaceStencil.writeMask = pDesc->mStencilWriteMask;
	descriptor.frontFaceStencil.stencilCompareFunction = pDesc->mStencilTest ? gMtlComparisonFunctionTranslator[pDesc->mStencilFrontFunc] : MTLCompareFunctionAlways;
	descriptor.frontFaceStencil.depthFailureOperation = gMtlStencilOpTranslator[pDesc->mDepthFrontFail];
	descriptor.frontFaceStencil.stencilFailureOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontFail];
	descriptor.frontFaceStencil.depthStencilPassOperation = gMtlStencilOpTranslator[pDesc->mStencilFrontPass];
	descriptor.frontFaceStencil.readMask = pDesc->mStencilReadMask;
	descriptor.frontFaceStencil.writeMask = pDesc->mStencilWriteMask;

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
#else
	if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
		addTexture(pRenderer, &textureCubeArrayDesc, &pDefaultCubeTextureArray);
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
#ifdef TARGET_IOS
	if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
#endif
	removeTexture(pRenderer, pDefaultCubeTextureArray);

	removeBuffer(pRenderer, pDefaultBuffer);
	removeSampler(pRenderer, pDefaultSampler);

	pDefaultDepthState = nil;
}

// -------------------------------------------------------------------------------------------------
// API functions
// -------------------------------------------------------------------------------------------------

TinyImageFormat getRecommendedSwapchainFormat(bool hintHDR)
{
	return TinyImageFormat_B8G8R8A8_UNORM;
}

#ifndef TARGET_IOS
// Returns the CFDictionary that contains the system profiler data type described in inDataType.
CFDictionaryRef findDictionaryForDataType(const CFArrayRef inArray, CFStringRef inDataType)
{
	UInt8           i;
	CFDictionaryRef theDictionary;

	// Search the array of dictionaries for a CFDictionary that matches
	for (i = 0; i < CFArrayGetCount(inArray); i++)
	{
		theDictionary = (CFDictionaryRef)CFArrayGetValueAtIndex(inArray, i);

		// If the CFDictionary at this index has a key/value pair with the value equal to inDataType, retain and return it.
		if (CFDictionaryContainsValue(theDictionary, inDataType))
		{
			return (theDictionary);
		}
	}

	return (NULL);
}

// Returns the CFArray of ???item??? dictionaries.
CFArrayRef getItemsArrayFromDictionary(const CFDictionaryRef inDictionary)
{
	CFArrayRef itemsArray;

	// Retrieve the CFDictionary that has a key/value pair with the key equal to ???_items???.
	itemsArray = (CFArrayRef)CFDictionaryGetValue(inDictionary, CFSTR("_items"));
	if (itemsArray != NULL)
		CFRetain(itemsArray);

	return (itemsArray);
}
//Used to call system profiler to retrieve GPU information such as vendor id and model id
void retrieveSystemProfilerInformation(eastl::string& outVendorId)
{
	FILE*           sys_profile;
	size_t          bytesRead = 0;
	char            streamBuffer[1024 * 512];
	UInt8           i = 0;
	CFDataRef       xmlData;
	CFDictionaryRef hwInfoDict;
	CFArrayRef      itemsArray;
	CFIndex         arrayCount;

	// popen will fork and invoke the system_profiler command and return a stream reference with its result data
	sys_profile = popen("system_profiler SPDisplaysDataType -xml", "r");
	// Read the stream into a memory buffer
	bytesRead = fread(streamBuffer, sizeof(char), sizeof(streamBuffer), sys_profile);
	// Close the stream
	pclose(sys_profile);
        if (bytesRead == 0)
        {
            LOGF(LogLevel::eERROR, "Couldn't read SPDisplaysData from system_profiler");
            outVendorId = eastl::string("0x0000");
            return;
        }
	// Create a CFDataRef with the xml data
	xmlData = CFDataCreate(kCFAllocatorDefault, (UInt8*)streamBuffer, bytesRead);
	// CFPropertyListCreateFromXMLData reads in the XML data and will parse it into a CFArrayRef for us.
	CFStringRef errorString;
	//read xml data
	CFArrayRef propertyArray =
		((CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL, (CFErrorRef*)&errorString));

	// This will be the dictionary that contains all the Hardware information that system_profiler knows about.
	hwInfoDict = findDictionaryForDataType(propertyArray, CFSTR("SPDisplaysDataType"));
	if (hwInfoDict != NULL)
	{
		itemsArray = getItemsArrayFromDictionary(hwInfoDict);

		if (itemsArray != NULL)
		{
			// Find out how many items in this category ??? each one is a dictionary
			arrayCount = CFArrayGetCount(itemsArray);

			for (i = 0; i < arrayCount; i++)
			{
				CFMutableStringRef outputString;

				// Create a mutable CFStringRef with the dictionary value found with key ???machine_name???
				// This is the machine_name of this mac machine.
				// Here you can give any value in key tag,to get its corresponding content
				outputString = CFStringCreateMutableCopy(
					kCFAllocatorDefault, 0,
					(CFStringRef)CFDictionaryGetValue(
						(CFDictionaryRef)CFArrayGetValueAtIndex(itemsArray, i), CFSTR("spdisplays_device-id")));
				NSString* outNS = (__bridge NSString*)outputString;
				outVendorId = [outNS.lowercaseString UTF8String];
				//your code here
				//(you can append output string OR modify your function according to your need )
				CFRelease(outputString);
			}

			CFRelease(itemsArray);
		}
		hwInfoDict = nil;
	}
	CFRelease(xmlData);
	CFRelease(propertyArray);
}
//Used to go through the given registry ID for the select device.
//Multiple id's can be found so they get filtered using the inModel id that was taken
//from system profile
void displayGraphicsInfo(uint64_t regId, eastl::string inModel, GPUVendorPreset& vendorVecOut)
{
	// Get dictionary of all the PCI Devices
	//CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");
	CFMutableDictionaryRef matchDict = IORegistryEntryIDMatching(regId);
	// Create an iterator
	io_iterator_t iterator;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iterator) == kIOReturnSuccess)
	{
		// Iterator for devices found
		io_registry_entry_t regEntry;

		while ((regEntry = IOIteratorNext(iterator)))
		{
			// Put this services object into a dictionary object.
			CFMutableDictionaryRef serviceDictionary;
			if (IORegistryEntryCreateCFProperties(regEntry, &serviceDictionary, kCFAllocatorDefault, kNilOptions) != kIOReturnSuccess)
			{
				// Service dictionary creation failed.
				IOObjectRelease(regEntry);
				continue;
			}
			NSString* ioPCIMatch = nil;
			//on macbook IOPCIPrimaryMatch is used
			if (CFDictionaryContainsKey(serviceDictionary, CFSTR("IOPCIPrimaryMatch")))
			{
				ioPCIMatch = (NSString*)CFDictionaryGetValue(serviceDictionary, CFSTR("IOPCIPrimaryMatch"));
			}
			else
			{
				//on iMac IOPCIMatch is used
				ioPCIMatch = (NSString*)CFDictionaryGetValue(serviceDictionary, CFSTR("IOPCIMatch"));
			}

			if (ioPCIMatch)
			{
				//get list of vendors from PCI Match above
				//this is a reflection of the display kext
				NSArray* vendors = [ioPCIMatch componentsSeparatedByString:@" "];
				for (id vendor in vendors)
				{
					NSString* modelId = [vendor substringToIndex:6];
					NSString* vendorId = [vendor substringFromIndex:6];
					vendorId = [@"0x" stringByAppendingString:vendorId];
					eastl::string modelIdString = [modelId.lowercaseString UTF8String];
					eastl::string vendorIdString = [vendorId.lowercaseString UTF8String];
					//filter out unwated model id's
					if (modelIdString != inModel)
						continue;

					strncpy(vendorVecOut.mModelId, modelIdString.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
					strncpy(vendorVecOut.mVendorId, vendorIdString.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
					vendorVecOut.mPresetLevel = GPUPresetLevel::GPU_PRESET_LOW;
					break;
				}
			}

			// Release the dictionary
			CFRelease(serviceDictionary);
			// Release the serviceObject
			IOObjectRelease(regEntry);
		}
		// Release the iterator
		IOObjectRelease(iterator);
	}
}
#endif
uint32_t queryThreadExecutionWidth(Renderer* pRenderer)
{
	if (!pRenderer)
		return 0;

	NSError*  error = nil;
	NSString* defaultComputeShader =
		@"#include <metal_stdlib>\n"
		 "using namespace metal;\n"
		 "kernel void simplest(texture2d<float, access::write> output [[texture(0)]],uint2 gid [[thread_position_in_grid]])\n"
		 "{output.write(float4(0, 0, 0, 1), gid);}";

	// Load all the shader files with a .metal file extension in the project
	id<MTLLibrary> defaultLibrary = [pRenderer->pDevice newLibraryWithSource:defaultComputeShader options:nil error:&error];

	if (error != nil)
	{
		LOGF(LogLevel::eWARNING, "Could not create library for simple compute shader: %s", [[error localizedDescription] UTF8String]);
		return 0;
	}

	// Load the kernel function from the library
	id<MTLFunction> kernelFunction = [defaultLibrary newFunctionWithName:@"simplest"];

	// Create a compute pipeline state
	id<MTLComputePipelineState> computePipelineState = [pRenderer->pDevice newComputePipelineStateWithFunction:kernelFunction error:&error];
	if (error != nil)
	{
		LOGF(LogLevel::eWARNING, "Could not create compute pipeline state for simple compute shader: %s", [[error localizedDescription] UTF8String]);
		return 0;
	}

	return (uint32_t)computePipelineState.threadExecutionWidth;
}

void initRenderer(const char* appName, const RendererDesc* settings, Renderer** ppRenderer)
{
	Renderer* pRenderer = (Renderer*)conf_calloc(1, sizeof(*pRenderer));
	ASSERT(pRenderer);

	pRenderer->pName = (char*)conf_calloc(strlen(appName) + 1, sizeof(char));
	memcpy(pRenderer->pName, appName, strlen(appName));
	
	pRenderer->mGpuMode = settings->mGpuMode;
	pRenderer->mApi = RENDERER_API_METAL;
	pRenderer->mEnableGpuBasedValidation = settings->mEnableGPUBasedValidation;
	pRenderer->mShaderTarget = settings->mShaderTarget;
	
	// Initialize the Metal bits
	{
		// Get the systems default device.
		pRenderer->pDevice = MTLCreateSystemDefaultDevice();

		utils_caps_builder(pRenderer);

		//get gpu vendor and model id.
		GPUVendorPreset gpuVendor;
		gpuVendor.mPresetLevel = GPUPresetLevel::GPU_PRESET_LOW;
#ifndef TARGET_IOS
		eastl::string outModelId;
		retrieveSystemProfilerInformation(outModelId);
		displayGraphicsInfo(pRenderer->pDevice.registryID, outModelId, gpuVendor);
		eastl::string mDeviceName = [pRenderer->pDevice.name UTF8String];
		strncpy(gpuVendor.mGpuName, mDeviceName.c_str(), MAX_GPU_VENDOR_STRING_LENGTH);
		LOGF(LogLevel::eINFO, "Current Gpu Name: %s", gpuVendor.mGpuName);
		LOGF(LogLevel::eINFO, "Current Gpu Vendor ID: %s", gpuVendor.mVendorId);
		LOGF(LogLevel::eINFO, "Current Gpu Model ID: %s", gpuVendor.mModelId);
#else
		strncpy(gpuVendor.mVendorId, "Apple", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(gpuVendor.mModelId, "iOS", MAX_GPU_VENDOR_STRING_LENGTH);
		strncpy(gpuVendor.mGpuName, [pRenderer->pDevice.name cStringUsingEncoding:NSUTF8StringEncoding], MAX_GPU_VENDOR_STRING_LENGTH);
#endif

		// Set the default GPU settings.
		pRenderer->mLinkedNodeCount = 1;
		GPUSettings gpuSettings[MAX_GPUS] = {};
		gpuSettings[0].mUniformBufferAlignment = 256;
		gpuSettings[0].mUploadBufferTextureAlignment = 16;
		gpuSettings[0].mUploadBufferTextureRowAlignment = 1;
		gpuSettings[0].mMaxVertexInputBindings = MAX_VERTEX_BINDINGS;    // there are no special vertex buffers for input in Metal, only regular buffers
		gpuSettings[0].mMultiDrawIndirect = false;    // multi draw indirect is not supported on Metal: only single draw indirect
		gpuSettings[0].mGpuVendorPreset = gpuVendor;
		gpuSettings[0].mROVsSupported = [pRenderer->pDevice areRasterOrderGroupsSupported];
		gpuSettings[0].mWaveLaneCount = queryThreadExecutionWidth(pRenderer);
		
		// argument buffer capabilities
		MTLArgumentBuffersTier abTier = pRenderer->pDevice.argumentBuffersSupport;
		
		//
		if (abTier == MTLArgumentBuffersTier2)
		{
			gpuSettings[0].mArgumentBufferMaxTextures = 500000;
		}
		else
		{
#ifdef TARGET_IOS
			if (@available(macOS 10.15, iOS 13.0, *))
			{
				// iOS caps
				if ([pRenderer->pDevice supportsFamily: MTLGPUFamilyApple4]) // A11 and higher
				{
					gpuSettings[0].mArgumentBufferMaxTextures = 96;
				}
				else
				{
					gpuSettings[0].mArgumentBufferMaxTextures = 31;
				}
			}
			else
			{
				if ([pRenderer->pDevice supportsFeatureSet: MTLFeatureSet_iOS_GPUFamily4_v2])
				{
					gpuSettings[0].mArgumentBufferMaxTextures = 96;
				}
				else
				{
					gpuSettings[0].mArgumentBufferMaxTextures = 31;
				}
			}
#else
			gpuSettings[0].mArgumentBufferMaxTextures = 128;
#endif
		}
		
		pRenderer->pActiveGpuSettings = (GPUSettings*)conf_malloc(sizeof(GPUSettings));
		*pRenderer->pActiveGpuSettings = gpuSettings[0];
		
#ifndef TARGET_IOS
		setGPUPresetLevel(pRenderer, 1, gpuSettings);
		//exit app if gpu being used has an office preset.
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
#endif
		// Create a resource allocator.
		AllocatorCreateInfo info = { 0 };
		info.device = pRenderer->pDevice;
		//info.physicalDevice = pRenderer->pActiveGPU;
		createAllocator(&info, &pRenderer->pResourceAllocator);

		// Create default resources.
		add_default_resources(pRenderer);
		
		ShaderMacro rendererShaderDefines[] =
		{
			{ "UPDATE_FREQ_NONE",      "10" },
			{ "UPDATE_FREQ_PER_FRAME", "11" },
			{ "UPDATE_FREQ_PER_BATCH", "12" },
			{ "UPDATE_FREQ_PER_DRAW",  "13" },
			{ "UPDATE_FREQ_USER",      "20" },
	#ifdef TARGET_IOS
			{ "TARGET_IOS", "" },
	#endif
		};
		pRenderer->mBuiltinShaderDefinesCount = sizeof(rendererShaderDefines) / sizeof(rendererShaderDefines[0]);
		pRenderer->pBuiltinShaderDefines = (ShaderMacro*)conf_calloc(pRenderer->mBuiltinShaderDefinesCount, sizeof(ShaderMacro));
		for (uint32_t i = 0; i < pRenderer->mBuiltinShaderDefinesCount; ++i)
		{
			conf_placement_new<ShaderMacro>(&pRenderer->pBuiltinShaderDefines[i]);
			pRenderer->pBuiltinShaderDefines[i] = rendererShaderDefines[i];
		}

		// Renderer is good! Assign it to result!
		*(ppRenderer) = pRenderer;
	}
}

void removeRenderer(Renderer* pRenderer)
{
	ASSERT(pRenderer);
	for (uint32_t i = 0; i < pRenderer->mBuiltinShaderDefinesCount; ++i)
		pRenderer->pBuiltinShaderDefines[i].~ShaderMacro();
	SAFE_FREE(pRenderer->pBuiltinShaderDefines);
	remove_default_resources(pRenderer);
	destroyAllocator(pRenderer->pResourceAllocator);
	pRenderer->pDevice = nil;
	SAFE_FREE(pRenderer->pName);
	SAFE_FREE(pRenderer->pActiveGpuSettings);
	SAFE_FREE(pRenderer->pCapBits);
	SAFE_FREE(pRenderer);
}

void addFence(Renderer* pRenderer, Fence** ppFence)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppFence);
	
	Fence* pFence = (Fence*)conf_calloc(1, sizeof(Fence));
	ASSERT(pFence);

	pFence->pMtlSemaphore = dispatch_semaphore_create(0);
	pFence->mSubmitted = false;

	*ppFence = pFence;
}

void removeFence(Renderer* pRenderer, Fence* pFence)
{
	ASSERT(pFence);
	pFence->pMtlSemaphore = nil;

	SAFE_FREE(pFence);
}

void addSemaphore(Renderer* pRenderer, Semaphore** ppSemaphore)
{
	ASSERT(pRenderer);
	ASSERT(ppSemaphore);
	
	Semaphore* pSemaphore = (Semaphore*)conf_calloc(1, sizeof(Semaphore));
	ASSERT(pSemaphore);

	pSemaphore->pMtlSemaphore = dispatch_semaphore_create(0);

	*ppSemaphore = pSemaphore;
}

void removeSemaphore(Renderer* pRenderer, Semaphore* pSemaphore)
{
	ASSERT(pSemaphore);
	pSemaphore->pMtlSemaphore = nil;

	SAFE_FREE(pSemaphore);
}

void addQueue(Renderer* pRenderer, QueueDesc* pDesc, Queue** ppQueue)
{
	ASSERT(pDesc);
	ASSERT(ppQueue);
	
	Queue* pQueue = (Queue*)conf_calloc(1, sizeof(Queue));
	ASSERT(pQueue);

	pQueue->mNodeIndex = pDesc->mNodeIndex;
	pQueue->mType = pDesc->mType;
	pQueue->mtlCommandQueue = [pRenderer->pDevice newCommandQueueWithMaxCommandBufferCount:512];
	pQueue->mUploadGranularity = {1, 1, 1};
	pQueue->mBarrierFlags = 0;
	pQueue->mtlQueueFence = [pRenderer->pDevice newFence];
	ASSERT(pQueue->mtlCommandQueue != nil);

	*ppQueue = pQueue;
}

void removeQueue(Renderer* pRenderer, Queue* pQueue)
{
	ASSERT(pQueue);
	pQueue->mtlCommandQueue = nil;
	pQueue->mtlQueueFence = nil;

	SAFE_FREE(pQueue);
}

void addCmdPool(Renderer* pRenderer, const CmdPoolDesc* pDesc, CmdPool** ppCmdPool)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppCmdPool);
	
	CmdPool* pCmdPool = (CmdPool*)conf_calloc(1, sizeof(CmdPool));
	ASSERT(pCmdPool);

	pCmdPool->pQueue = pDesc->pQueue;

	*ppCmdPool = pCmdPool;
}

void removeCmdPool(Renderer* pRenderer, CmdPool* pCmdPool)
{
	ASSERT(pCmdPool);
	SAFE_FREE(pCmdPool);
}

void addCmd(Renderer* pRenderer, const CmdDesc* pDesc, Cmd** ppCmd)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppCmd);
	
	Cmd* pCmd = (Cmd*)conf_calloc(1, sizeof(Cmd));
	ASSERT(pCmd);

	pCmd->pRenderer = pRenderer;
	pCmd->pQueue = pDesc->pPool->pQueue;

	*ppCmd = pCmd;
}

void removeCmd(Renderer* pRenderer, Cmd* pCmd)
{
	ASSERT(pCmd);
	pCmd->mtlCommandBuffer = nil;
	pCmd->pRenderPassDesc = nil;

	SAFE_FREE(pCmd);
}

void addCmd_n(Renderer* pRenderer, const CmdDesc* pDesc, uint32_t cmdCount, Cmd*** pppCmd)
{
	//verify that ***cmd is valid
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(cmdCount);
	ASSERT(pppCmd);

	Cmd** ppCmds = (Cmd**)conf_calloc(cmdCount, sizeof(Cmd*));
	ASSERT(ppCmds);

	//add n new cmds to given pool
	for (uint32_t i = 0; i < cmdCount; ++i)
	{
		::addCmd(pRenderer, pDesc, &ppCmds[i]);
	}

	*pppCmd = ppCmds;
}

void removeCmd_n(Renderer* pRenderer, uint32_t cmdCount, Cmd** ppCmds)
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

void toggleVSync(Renderer* pRenderer, SwapChain** ppSwapchain)
{
#if !defined(TARGET_IOS)
	SwapChain* pSwapchain = *ppSwapchain;
	pSwapchain->mEnableVsync = !pSwapchain->mEnableVsync;
	//no need to have vsync on layers otherwise we will wait on semaphores
	//get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)pSwapchain->pForgeView.layer;

	//only available on mac OS.
	//VSync seems to be necessary on iOS.
	if (!pSwapchain->mEnableVsync)
		layer.displaySyncEnabled = false;
	else
		layer.displaySyncEnabled = true;
#endif
}

void addSwapChain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppSwapChain);

	SwapChain* pSwapChain = (SwapChain*)conf_calloc(1, sizeof(SwapChain) + pDesc->mImageCount * sizeof(RenderTarget*));
	ASSERT(pSwapChain);

	pSwapChain->ppRenderTargets = (RenderTarget**)(pSwapChain + 1);

#if !defined(TARGET_IOS)

    NSWindow* window = (__bridge NSWindow*)pDesc->mWindowHandle.window;
	pSwapChain->pForgeView = window.contentView;
	pSwapChain->pForgeView.autoresizesSubviews = TRUE;

	//no need to have vsync on layers otherwise we will wait on semaphores
	//get a copy of the layer for nextDrawables
	CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;

	//only available on mac OS.
	//VSync seems to be necessary on iOS.
	if (!pDesc->mEnableVsync)
	{
		//This needs to be set to false to have working non-vsync
		//otherwise present drawables will wait on vsync.
		layer.displaySyncEnabled = false;
	}
	else
		//This needs to be set to false to have working vsync
		layer.displaySyncEnabled = true;
#else
    UIWindow* window = (__bridge UIWindow*)pDesc->mWindowHandle.window;
    pSwapChain->pForgeView = window.rootViewController.view;
    pSwapChain->pForgeView.autoresizesSubviews = TRUE;
   
    CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;
#endif
    // Set the view pixel format to match the swapchain's pixel format.
    layer.pixelFormat = (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(pDesc->mColorFormat);

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

	for (uint32_t i = 0; i < pDesc->mImageCount; ++i)
	{
		addRenderTarget(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
		destroyTexture(pRenderer->pResourceAllocator, pSwapChain->ppRenderTargets[i]->pTexture);
	}
	
	pSwapChain->mImageCount = pDesc->mImageCount;
	pSwapChain->mEnableVsync = pDesc->mEnableVsync;

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

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer)
{
	ASSERT(pRenderer);
	ASSERT(ppBuffer);
	ASSERT(pDesc);
	ASSERT((pDesc->mDescriptors & DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER) || pDesc->mSize > 0);
	ASSERT(pRenderer->pDevice != nil);
    
	Buffer* pBuffer = (Buffer*)conf_calloc(1, sizeof(Buffer));
	ASSERT(pBuffer);

	uint64_t allocationSize = pBuffer->mSize;
	// Align the buffer size to multiples of the dynamic uniform buffer minimum size
	if (pBuffer->mDescriptors & DESCRIPTOR_TYPE_UNIFORM_BUFFER)
	{
		uint64_t minAlignment = pRenderer->pActiveGpuSettings->mUniformBufferAlignment;
		allocationSize = round_up_64(allocationSize, minAlignment);
	}

	//Use isLowPower to determine if running intel integrated gpu
	//There's currently an intel driver bug with placed resources so we need to create
	//new resources that are GPU only in their own memory space
	//0x8086 is intel vendor id
	if (strcmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId, "0x8086") == 0 &&
		(ResourceMemoryUsage)pDesc->mMemoryUsage & RESOURCE_MEMORY_USAGE_GPU_ONLY)
		((BufferDesc*)pDesc)->mFlags |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

	// Get the proper memory requiremnets for the given buffer.
	AllocatorMemoryRequirements mem_reqs = { 0 };
	mem_reqs.usage = (ResourceMemoryUsage)pDesc->mMemoryUsage;
	mem_reqs.flags = 0;
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
	if (pDesc->mFlags & BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
		mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_PERSISTENT_MAP_BIT;

	bool             allocSuccess;
	allocSuccess = createBuffer(pRenderer->pResourceAllocator, pDesc, &mem_reqs, pBuffer);
	ASSERT(allocSuccess);
	
#ifdef TARGET_IOS
	mapBuffer(pRenderer, pBuffer, NULL);
#endif

	pBuffer->mCurrentState = pDesc->mStartState;

	// If buffer is a suballocation use offset in heap else use zero offset (placed resource / committed resource)
	if (pBuffer->pMtlAllocation->GetResource())
		pBuffer->mPositionInHeap = (uint32_t)pBuffer->pMtlAllocation->GetOffset();
	else
		pBuffer->mPositionInHeap = 0;
	
	pBuffer->mSize = (uint32_t)pDesc->mSize;
	pBuffer->mMemoryUsage = pDesc->mMemoryUsage;
	pBuffer->mNodeIndex = pDesc->mNodeIndex;
	pBuffer->mStartState = pDesc->mStartState;
	pBuffer->mDescriptors = pDesc->mDescriptors;

	*ppBuffer = pBuffer;
}
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer);
	destroyBuffer(pRenderer->pResourceAllocator, pBuffer);
	SAFE_FREE(pBuffer);
}

void addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mWidth && pDesc->mHeight && (pDesc->mDepth || pDesc->mArraySize));
	if (pDesc->mSampleCount > SAMPLE_COUNT_1 && pDesc->mMipLevels > 1)
	{
		internal_log(LOG_TYPE_ERROR, "Multi-Sampled textures cannot have mip maps", "MetalRenderer");
		return;
	}
	add_texture(pRenderer, pDesc, ppTexture, false);
}

void addRenderTarget(Renderer* pRenderer, const RenderTargetDesc* pDesc, RenderTarget** ppRenderTarget)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(ppRenderTarget);

	((RenderTargetDesc*)pDesc)->mMipLevels = max(1U, pDesc->mMipLevels);

	RenderTarget* pRenderTarget = (RenderTarget*)conf_calloc(1, sizeof(RenderTarget));
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
	rtDesc.mHostVisible = false;

	rtDesc.mDescriptors |= pDesc->mDescriptors;

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

	*ppRenderTarget = pRenderTarget;
}

void removeTexture(Renderer* pRenderer, Texture* pTexture)
{
	ASSERT(pTexture);

	// Destroy descriptors
	if (pTexture->pMtlUAVDescriptors)
	{
		for (uint32_t i = 0; i < pTexture->mMipLevels; ++i)
		{
			pTexture->pMtlUAVDescriptors[i] = nil;
		}
	}

	destroyTexture(pRenderer->pResourceAllocator, pTexture);
	SAFE_FREE(pTexture);
}

void removeRenderTarget(Renderer* pRenderer, RenderTarget* pRenderTarget)
{
	removeTexture(pRenderer, pRenderTarget->pTexture);
	SAFE_FREE(pRenderTarget);
}

void addSampler(Renderer* pRenderer, const SamplerDesc* pDesc, Sampler** ppSampler)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc->mCompareFunc < MAX_COMPARE_MODES);
	ASSERT(ppSampler);
	
	Sampler* pSampler = (Sampler*)conf_calloc(1, sizeof(Sampler));
	ASSERT(pSampler);

	MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
	samplerDesc.minFilter = (pDesc->mMinFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.magFilter = (pDesc->mMagFilter == FILTER_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear);
	samplerDesc.mipFilter = (pDesc->mMipMapMode == MIPMAP_MODE_NEAREST ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear);
	samplerDesc.maxAnisotropy = (pDesc->mMaxAnisotropy == 0 ? 1 : pDesc->mMaxAnisotropy);    // 0 is not allowed in Metal
	samplerDesc.sAddressMode = gMtlAddressModeTranslator[pDesc->mAddressU];
	samplerDesc.tAddressMode = gMtlAddressModeTranslator[pDesc->mAddressV];
	samplerDesc.rAddressMode = gMtlAddressModeTranslator[pDesc->mAddressW];
	samplerDesc.compareFunction = gMtlComparisonFunctionTranslator[pDesc->mCompareFunc];
    samplerDesc.supportArgumentBuffers = YES;

	pSampler->mtlSamplerState = [pRenderer->pDevice newSamplerStateWithDescriptor:samplerDesc];

	*ppSampler = pSampler;
}

void removeSampler(Renderer* pRenderer, Sampler* pSampler)
{
	ASSERT(pSampler);
	pSampler->mtlSamplerState = nil;
	SAFE_FREE(pSampler);
}

#ifdef TARGET_IOS
void addShader(Renderer* pRenderer, const ShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppShaderProgram);
	
	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);
	
	eastl::unordered_map<uint32_t, MTLVertexFormat> vertexAttributeFormats;

	uint32_t         reflectionCount = 0;
	ShaderReflection stageReflections[SHADER_STAGE_COUNT];
	const char*      entryNames[SHADER_STAGE_COUNT] = {};
	
	for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
	{
		eastl::string              source = NULL;
		const char*                  entry_point = NULL;
		const char*                  shader_name = NULL;
		eastl::vector<ShaderMacro> shader_macros;
		__strong id<MTLFunction>* compiled_code = NULL;

		ShaderStage stage_mask = (ShaderStage)(1 << i);
		if (stage_mask == (pShaderProgram->mStages & stage_mask))
		{
			switch (stage_mask)
			{
				case SHADER_STAGE_VERT:
				{
					source = pDesc->mVert.pCode;
					entry_point = pDesc->mVert.pEntryPoint;
					shader_name = pDesc->mVert.pName;
					shader_macros = eastl::vector<ShaderMacro>(pDesc->mVert.mMacroCount);
					memcpy(shader_macros.data(), pDesc->mVert.pMacros, pDesc->mVert.mMacroCount * sizeof(ShaderMacro));
					compiled_code = &(pShaderProgram->mtlVertexShader);
				}
				break;
				case SHADER_STAGE_FRAG:
				{
					source = pDesc->mFrag.pCode;
					entry_point = pDesc->mFrag.pEntryPoint;
					shader_name = pDesc->mFrag.pName;
					shader_macros = eastl::vector<ShaderMacro>(pDesc->mFrag.mMacroCount);
					memcpy(shader_macros.data(), pDesc->mFrag.pMacros, pDesc->mFrag.mMacroCount * sizeof(ShaderMacro));
					compiled_code = &(pShaderProgram->mtlFragmentShader);
				}
				break;
				case SHADER_STAGE_COMP:
				{
					source = pDesc->mComp.pCode;
					entry_point = pDesc->mComp.pEntryPoint;
					shader_name = pDesc->mComp.pName;
					shader_macros = eastl::vector<ShaderMacro>(pDesc->mComp.mMacroCount);
					memcpy(shader_macros.data(), pDesc->mComp.pMacros, pDesc->mComp.mMacroCount * sizeof(ShaderMacro));
					compiled_code = &(pShaderProgram->mtlComputeShader);
				}
				break;
				default: break;
			}
			
			entryNames[reflectionCount] = entry_point;

			// Create a NSDictionary for all the shader macros.
			NSNumberFormatter* numberFormatter =
				[[NSNumberFormatter alloc] init];    // Used for reading NSNumbers macro values from strings.
			numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;
            
            NSMutableDictionary* macroDictionary = [NSMutableDictionary dictionaryWithCapacity:shader_macros.size()];
			for (uint i = 0; i < shader_macros.size(); i++)
			{
                NSString *key = [NSString stringWithUTF8String:shader_macros[i].definition];

				// Try reading the macro value as a NSNumber. If failed, use it as an NSString.
				NSString* valueString = [NSString stringWithUTF8String:shader_macros[i].value];
				NSNumber* valueNumber = [numberFormatter numberFromString:valueString];
                macroDictionary[key] = valueNumber ? valueNumber : valueString;
			}

			// Compile the code
			NSString* shaderSource = [[NSString alloc] initWithUTF8String:source.c_str()];
			NSError*  error = nil;

			MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
			options.preprocessorMacros = macroDictionary;
			id<MTLLibrary> lib = [pRenderer->pDevice newLibraryWithSource:shaderSource options:options error:&error];

			// Warning
			if (error)
			{
				if (lib)
				{
					LOGF(LogLevel::eWARNING,
						"Loaded shader %s with the following warnings:\n %s", shader_name, [[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
                    //ASSERT(0);
				}
				// Error
				else
				{
					LOGF(LogLevel::eERROR,
						"Couldn't load shader %s with the following error:\n %s", shader_name, [[error localizedDescription] UTF8String]);
					error = 0;    //  error string is an autorelease object.
                    ASSERT(0);
				}
			}

			if (lib)
			{
				pShaderProgram->mtlLibrary = lib;
				
				NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:entry_point];
				id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
				assert(function != nil && "Entry point not found in shader.");
				*compiled_code = function;
			}

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)source.c_str(), (uint32_t)source.size(), stage_mask, &vertexAttributeFormats,
				&stageReflections[reflectionCount++]);
		}
	}
	
	pShaderProgram->pEntryNames = (char**)conf_calloc(reflectionCount, sizeof(char*));
	memcpy(pShaderProgram->pEntryNames, entryNames, reflectionCount * sizeof(char*));
	createPipelineReflection(stageReflections, reflectionCount, pShaderProgram->pReflection);

	*ppShaderProgram = pShaderProgram;
}
#endif

void addShaderBinary(Renderer* pRenderer, const BinaryShaderDesc* pDesc, Shader** ppShaderProgram)
{
	ASSERT(pRenderer);
	ASSERT(pDesc && pDesc->mStages);
	ASSERT(ppShaderProgram);

	Shader* pShaderProgram = (Shader*)conf_calloc(1, sizeof(Shader) + sizeof(PipelineReflection));
	ASSERT(pShaderProgram);

	pShaderProgram->mStages = pDesc->mStages;
	pShaderProgram->pReflection = (PipelineReflection*)(pShaderProgram + 1);

	eastl::unordered_map<uint32_t, MTLVertexFormat> vertexAttributeFormats;
	const char* entryNames[SHADER_STAGE_COUNT] = {};

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
			pShaderProgram->mtlLibrary = lib;
			
			// Create a MTLFunction from the loaded MTLLibrary.
			NSString*       entryPointNStr = [[NSString alloc] initWithUTF8String:pStage->pEntryPoint];
			id<MTLFunction> function = [lib newFunctionWithName:entryPointNStr];
			*compiled_code = function;
			
			entryNames[reflectionCount] = pStage->pEntryPoint;

			mtl_createShaderReflection(
				pRenderer, pShaderProgram, (const uint8_t*)pStage->pSource, pStage->mSourceSize, stage_mask,
				&vertexAttributeFormats, &pShaderProgram->pReflection->mStageReflections[reflectionCount++]);
		}
	}
	
	pShaderProgram->pEntryNames = (char**)conf_calloc(reflectionCount, sizeof(char*));
	memcpy(pShaderProgram->pEntryNames, entryNames, reflectionCount * sizeof(char*));

	createPipelineReflection(pShaderProgram->pReflection->mStageReflections, reflectionCount, pShaderProgram->pReflection);

	if (pShaderProgram->mStages & SHADER_STAGE_COMP)
	{
		pShaderProgram->mNumThreadsPerGroup[0] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[0];
		pShaderProgram->mNumThreadsPerGroup[1] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[1];
		pShaderProgram->mNumThreadsPerGroup[2] = pShaderProgram->pReflection->mStageReflections[0].mNumThreadsPerGroup[2];
	}

	*ppShaderProgram = pShaderProgram;
}

void removeShader(Renderer* pRenderer, Shader* pShaderProgram)
{
	ASSERT(pShaderProgram);
	pShaderProgram->mtlVertexShader = nil;
	pShaderProgram->mtlFragmentShader = nil;
	pShaderProgram->mtlComputeShader = nil;
	pShaderProgram->mtlLibrary = nil;

	destroyPipelineReflection(pShaderProgram->pReflection);
	SAFE_FREE(pShaderProgram->pEntryNames);
	SAFE_FREE(pShaderProgram);
}

void addRootSignature(Renderer* pRenderer, const RootSignatureDesc* pRootSignatureDesc, RootSignature** ppRootSignature)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(ppRootSignature);
	
	RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(RootSignature) + sizeof(DescriptorIndexMap));
	pRootSignature->pDescriptorNameToIndexMap = (DescriptorIndexMap*)(pRootSignature + 1);
	ASSERT(pRootSignature->pDescriptorNameToIndexMap);
	conf_placement_new<DescriptorIndexMap>(pRootSignature->pDescriptorNameToIndexMap);
	
	eastl::vector<ShaderResource>        shaderResources;

	// Collect static samplers
	eastl::vector<eastl::pair<ShaderResource const*, Sampler*> > staticSamplers;
	eastl::string_hash_map<Sampler*>                            staticSamplerMap;
	for (uint32_t i = 0; i < pRootSignatureDesc->mStaticSamplerCount; ++i)
		staticSamplerMap.insert(pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i]);
    
    {
        pRootSignature->pShaderDescriptors = (ShaderDescriptors*)conf_calloc(pRootSignatureDesc->mShaderCount, sizeof(ShaderDescriptors));
        pRootSignature->mShaderDescriptorsCount = pRootSignatureDesc->mShaderCount;
        
        // Collect all shader resources in the given shaders
        for (uint32_t sh = 0; sh < pRootSignatureDesc->mShaderCount; ++sh)
        {
            const PipelineReflection* pReflection = pRootSignatureDesc->ppShaders[sh]->pReflection;

            if (pReflection->mShaderStages & SHADER_STAGE_COMP)
            {
                pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
            }
            else
            {
                pRootSignature->mPipelineType = PIPELINE_TYPE_GRAPHICS;
            }
            
            pRootSignature->pShaderDescriptors[sh].pShader = pRootSignatureDesc->ppShaders[sh];
            pRootSignature->pShaderDescriptors[sh].mDescriptorCount = pReflection->mShaderResourceCount;
            pRootSignature->pShaderDescriptors[sh].pDescriptors = (DescriptorInfo*)conf_calloc( pRootSignature->pShaderDescriptors[sh].mDescriptorCount, sizeof(DescriptorInfo) );
            conf_placement_new<eastl::unordered_map<uint32_t, uint32_t>>(&pRootSignature->pShaderDescriptors[sh].mDescriptorNameToIndexMap);
            
            for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
            {
                ShaderResource const* pRes = &pReflection->pShaderResources[i];
                
                //
                DescriptorInfo*           pDesc = &pRootSignature->pShaderDescriptors[sh].pDescriptors[i];
                                
                uint32_t                  setIndex = pRes->set;
                const DescriptorUpdateFrequency updateFreq((DescriptorUpdateFrequency)setIndex);

                pRootSignature->pShaderDescriptors[sh].mDescriptorNameToIndexMap[pRes->name] = i;
                
                pDesc->mReg = pRes->reg;
//                pDesc->mDesc.set = pRes->set;
                pDesc->mSize = pRes->size;
//                pDesc->mDesc.alignment = pRes->alignment;
                pDesc->mType = pRes->type;
                pDesc->mUsedStages = pRes->used_stages;
                pDesc->mIsArgumentBufferField = pRes->mIsArgumentBufferField;
                pDesc->mtlArgumentDescriptors = &pRes->mtlArgumentDescriptors;
				pDesc->pName = pRes->name;
                pDesc->mUpdateFrequency = updateFreq;
                if (pDesc->mIsArgumentBufferField)
                    pDesc->mHandleIndex = pRes->mtlArgumentDescriptors.mArgumentIndex;
                else
                    pDesc->mHandleIndex = pRes->reg;
                pDesc->mtlStaticSampler = nil;

                // In case we're binding a texture, we need to specify the texture type so the bound resource type matches the one defined in the shader.
                if (pRes->type == DESCRIPTOR_TYPE_TEXTURE || pRes->type == DESCRIPTOR_TYPE_RW_TEXTURE)
                {
//                    pDesc->mDesc.mtlTextureType = pRes->mtlTextureType;
                }
                
                // static samplers
                if (pRes->type == DESCRIPTOR_TYPE_SAMPLER)
                {
                    eastl::string_hash_map<Sampler*>::const_iterator pNode = staticSamplerMap.find(pRes->name);
                    if (pNode != staticSamplerMap.end())
                    {
                        pDesc->mtlStaticSampler = pNode->second->mtlSamplerState;
                    }
                }
            }
        }
    }

	*ppRootSignature = pRootSignature;
}

void removeRootSignature(Renderer* pRenderer, RootSignature* pRootSignature)
{
	for (uint32_t i = 0; i < pRootSignature->mShaderDescriptorsCount; ++i)
	{
		pRootSignature->pShaderDescriptors[i].mDescriptorNameToIndexMap.clear(true);
		SAFE_FREE(pRootSignature->pShaderDescriptors[i].pDescriptors);
	}
	
	SAFE_FREE(pRootSignature->pShaderDescriptors);
	SAFE_FREE(pRootSignature->pDescriptors);
	
	pRootSignature->pDescriptorNameToIndexMap->mMap.clear(true);
	SAFE_FREE(pRootSignature);
}

uint32_t util_calculate_vertex_layout_stride(const VertexLayout* pVertexLayout)
{
	ASSERT(pVertexLayout);

	uint32_t result = 0;
	for (uint32_t i = 0; i < pVertexLayout->mAttribCount; ++i)
	{
		result += TinyImageFormat_BitSizeOfBlock(pVertexLayout->mAttribs[i].mFormat) / 8;
	}
	return result;
}

void addGraphicsPipelineImpl(Renderer* pRenderer, const GraphicsPipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(ppPipeline);
	
	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mType = PIPELINE_TYPE_GRAPHICS;
	pPipeline->pShader = pDesc->pShaderProgram;

	// create metal pipeline descriptor
	MTLRenderPipelineDescriptor* renderPipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
	renderPipelineDesc.vertexFunction = pDesc->pShaderProgram->mtlVertexShader;
	renderPipelineDesc.fragmentFunction = pDesc->pShaderProgram->mtlFragmentShader;
	renderPipelineDesc.sampleCount = pDesc->mSampleCount;

    renderPipelineDesc.supportIndirectCommandBuffers = pDesc->mSupportIndirectCommandBuffer;
    
	// add vertex layout to descriptor
	if (pDesc->pVertexLayout != nil)
	{
		uint32_t bindingValue = UINT32_MAX;
		// setup vertex descriptors
		for (uint i = 0; i < pDesc->pVertexLayout->mAttribCount; i++)
		{
			const VertexAttrib* attrib = pDesc->pVertexLayout->mAttribs + i;

			if (bindingValue != attrib->mBinding)
			{
				bindingValue = attrib->mBinding;
			}

			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].offset = attrib->mOffset;
			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].bufferIndex = attrib->mBinding + VERTEX_BINDING_OFFSET;
			renderPipelineDesc.vertexDescriptor.attributes[attrib->mLocation].format = util_to_mtl_vertex_format(attrib->mFormat);

			//setup layout for all bindings instead of just 0.
			renderPipelineDesc.vertexDescriptor.layouts[attrib->mBinding + VERTEX_BINDING_OFFSET].stride += TinyImageFormat_BitSizeOfBlock(attrib->mFormat) / 8;
			renderPipelineDesc.vertexDescriptor.layouts[attrib->mBinding + VERTEX_BINDING_OFFSET].stepRate = 1;
			if(pPipeline->pShader->mtlVertexShader.patchType != MTLPatchTypeNone)
				renderPipelineDesc.vertexDescriptor.layouts[attrib->mBinding +VERTEX_BINDING_OFFSET].stepFunction = MTLVertexStepFunctionPerPatchControlPoint;
			else if(attrib->mRate == VERTEX_ATTRIB_RATE_INSTANCE)
				renderPipelineDesc.vertexDescriptor.layouts[attrib->mBinding + VERTEX_BINDING_OFFSET].stepFunction = MTLVertexStepFunctionPerInstance;
			else
				renderPipelineDesc.vertexDescriptor.layouts[attrib->mBinding + VERTEX_BINDING_OFFSET].stepFunction = MTLVertexStepFunctionPerVertex;
		}
	}

#if !defined(TARGET_IOS)
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
#endif

	// assign blend state
	if (pDesc->pBlendState)
		util_to_blend_desc(pDesc->pBlendState, renderPipelineDesc.colorAttachments);
	
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
	// #TODO: Add if Metal API provides ability to set these
	//	rasterizerState.scissorEnable = pDesc->mScissor;
	//	rasterizerState.multisampleEnable = pDesc->mMultiSample;
	pPipeline->mWinding = (pRasterizer->mFrontFace == FRONT_FACE_CCW ? MTLWindingCounterClockwise : MTLWindingClockwise);
	
	// assign depth state
	pPipeline->mtlDepthStencilState = pDesc->pDepthState ? util_to_depth_state(pRenderer, pDesc->pDepthState) : pDefaultDepthState;

	// assign pixel format form depth attachment
	if (pDesc->mDepthStencilFormat != TinyImageFormat_UNDEFINED)
	{
		renderPipelineDesc.depthAttachmentPixelFormat = (MTLPixelFormat) TinyImageFormat_ToMTLPixelFormat(pDesc->mDepthStencilFormat);
		if (pDesc->mDepthStencilFormat == TinyImageFormat_D24_UNORM_S8_UINT || pDesc->mDepthStencilFormat == TinyImageFormat_D32_SFLOAT_S8_UINT)
			renderPipelineDesc.stencilAttachmentPixelFormat = renderPipelineDesc.depthAttachmentPixelFormat;
	}

	// assign common tesselation configuration if needed.
	if (pPipeline->pShader->mtlVertexShader.patchType != MTLPatchTypeNone)
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

	// create pipeline from descriptor
	NSError* error = nil;
	pPipeline->mtlRenderPipelineState = [pRenderer->pDevice newRenderPipelineStateWithDescriptor:renderPipelineDesc
                                                            options:MTLPipelineOptionNone
                                                            reflection:nil
                                                            error:&error];
	if (!pPipeline->mtlRenderPipelineState)
	{
		LOGF(LogLevel::eERROR, "Failed to create render pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
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

void addComputePipelineImpl(Renderer* pRenderer, const ComputePipelineDesc* pDesc, Pipeline** ppPipeline)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pDesc);
	ASSERT(pDesc->pShaderProgram);
	ASSERT(pDesc->pRootSignature);
	ASSERT(ppPipeline);
	
	Pipeline* pPipeline = (Pipeline*)conf_calloc(1, sizeof(Pipeline));
	ASSERT(pPipeline);

	pPipeline->mType = PIPELINE_TYPE_COMPUTE;
	pPipeline->pShader = pDesc->pShaderProgram;

	NSError* error = nil;
	pPipeline->mtlComputePipelineState = [pRenderer->pDevice newComputePipelineStateWithFunction:pDesc->pShaderProgram->mtlComputeShader error:&error];
	if (!pPipeline->mtlComputePipelineState)
	{
		LOGF(LogLevel::eERROR, "Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
		SAFE_FREE(pPipeline);
		return;
	}

	*ppPipeline = pPipeline;
}

extern void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline);
extern void removeRaytracingPipeline(RaytracingPipeline* pPipeline);
    
void addPipeline(Renderer* pRenderer, const PipelineDesc* pDesc, Pipeline** ppPipeline)
{
    ASSERT(pRenderer);
    ASSERT(pRenderer->pDevice != nil);
    
    switch (pDesc->mType)
    {
        case(PIPELINE_TYPE_COMPUTE):
        {
            addComputePipelineImpl(pRenderer, &pDesc->mComputeDesc, ppPipeline);
            break;
        }
        case(PIPELINE_TYPE_GRAPHICS):
        {
            addGraphicsPipelineImpl(pRenderer, &pDesc->mGraphicsDesc, ppPipeline);
            break;
        }
        case(PIPELINE_TYPE_RAYTRACING):
        {
            addRaytracingPipeline(&pDesc->mRaytracingDesc, ppPipeline);
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
	pPipeline->mtlRenderPipelineState = nil;
	pPipeline->mtlComputePipelineState = nil;
	pPipeline->mtlDepthStencilState = nil;
    if (pPipeline->mType == PIPELINE_TYPE_RAYTRACING)
        removeRaytracingPipeline(pPipeline->pRaytracingPipeline);

	SAFE_FREE(pPipeline);
}

void addIndirectCommandSignature(Renderer* pRenderer, const CommandSignatureDesc* pDesc, CommandSignature** ppCommandSignature)
{
	ASSERT(pRenderer != nil);
	ASSERT(pDesc != nil);

	CommandSignature* pCommandSignature = (CommandSignature*)conf_calloc(1, sizeof(CommandSignature));
	ASSERT(pCommandSignature);

	for (uint32_t i = 0; i < pDesc->mIndirectArgCount; i++)
	{
		const IndirectArgumentDescriptor* argDesc = pDesc->pArgDescs + i;
		if (argDesc->mType != INDIRECT_DRAW &&
			argDesc->mType != INDIRECT_DISPATCH &&
			argDesc->mType != INDIRECT_DRAW_INDEX &&
			argDesc->mType != INDIRECT_COMMAND_BUFFER &&
			argDesc->mType != INDIRECT_COMMAND_BUFFER_OPTIMIZE)
		{
			ASSERT(!"Unsupported indirect argument type.");
			return;
		}

		if (i == 0)
		{
			pCommandSignature->mDrawType = argDesc->mType;
		}
		else if (pCommandSignature->mDrawType != argDesc->mType)
		{
			assert(!"All elements in the root signature must be of the same type.");
			SAFE_FREE(pCommandSignature);
			return;
		}
	}

	*ppCommandSignature = pCommandSignature;
}

void removeIndirectCommandSignature(Renderer* pRenderer, CommandSignature* pCommandSignature)
{
	ASSERT(pCommandSignature);
	SAFE_FREE(pCommandSignature);
}

// -------------------------------------------------------------------------------------------------
// Buffer functions
// -------------------------------------------------------------------------------------------------
void mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
{
	ASSERT(pBuffer->mtlBuffer.storageMode != MTLStorageModePrivate && "Trying to map non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = pBuffer->mtlBuffer.contents;
}
void unmapBuffer(Renderer* pRenderer, Buffer* pBuffer)
{
	ASSERT(pBuffer->mtlBuffer.storageMode != MTLStorageModePrivate && "Trying to unmap non-cpu accessible resource");
	pBuffer->pCpuMappedAddress = nil;
}

// -------------------------------------------------------------------------------------------------
// Command buffer functions
// -------------------------------------------------------------------------------------------------

void beginCmd(Cmd* pCmd)
{
	@autoreleasepool
	{
		ASSERT(pCmd);
		pCmd->mtlRenderEncoder = nil;
		pCmd->mtlComputeEncoder = nil;
		pCmd->mtlBlitEncoder = nil;
		pCmd->pShader = nil;
		pCmd->pRenderPassDesc = nil;
		pCmd->selectedIndexBuffer = nil;
		pCmd->pLastFrameQuery = nil;
		pCmd->mtlCommandBuffer = [pCmd->pQueue->mtlCommandQueue commandBuffer];
	}
}

void endCmd(Cmd* pCmd)
{
    @autoreleasepool
    {
        util_end_current_encoders(pCmd, true);
    }
}

void cmdBindRenderTargets(
	Cmd* pCmd, uint32_t renderTargetCount, RenderTarget** ppRenderTargets, RenderTarget* pDepthStencil, const LoadActionsDesc* pLoadActions,
	uint32_t* pColorArraySlices, uint32_t* pColorMipSlices, uint32_t depthArraySlice, uint32_t depthMipSlice)
{
	ASSERT(pCmd);

    @autoreleasepool
    {
        util_end_current_encoders(pCmd, true);
    }

	if (!renderTargetCount && !pDepthStencil)
		return;

	@autoreleasepool
	{
		pCmd->pRenderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];

		// Flush color attachments
		for (uint32_t i = 0; i < renderTargetCount; i++)
		{
			Texture* colorAttachment = ppRenderTargets[i]->pTexture;

			pCmd->pRenderPassDesc.colorAttachments[i].texture = colorAttachment->mtlTexture;
			pCmd->pRenderPassDesc.colorAttachments[i].level = pColorMipSlices ? pColorMipSlices[i] : 0;
			if (pColorArraySlices)
			{
				if (ppRenderTargets[i]->mDepth > 1)
					pCmd->pRenderPassDesc.colorAttachments[i].depthPlane = pColorArraySlices[i];
				else
					pCmd->pRenderPassDesc.colorAttachments[i].slice = pColorArraySlices[i];
			}

            // For on-tile (memoryless) textures, we never need to load or store.
			if (colorAttachment->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
			{
				pCmd->pRenderPassDesc.colorAttachments[i].loadAction =
                    (pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionsColor[i]) : MTLLoadActionDontCare);
				pCmd->pRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionDontCare;
			}
			else
			{
				pCmd->pRenderPassDesc.colorAttachments[i].loadAction =
					(pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionsColor[i]) : MTLLoadActionLoad);
				pCmd->pRenderPassDesc.colorAttachments[i].storeAction = MTLStoreActionStore;
			}
			if (pLoadActions != NULL)
			{
				const ClearValue& clearValue = pLoadActions->mClearColorValues[i];
				pCmd->pRenderPassDesc.colorAttachments[i].clearColor =
					MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);
			}
		}

		if (pDepthStencil != nil)
		{
			pCmd->pRenderPassDesc.depthAttachment.texture = pDepthStencil->pTexture->mtlTexture;
			pCmd->pRenderPassDesc.depthAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
			pCmd->pRenderPassDesc.depthAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
#ifndef TARGET_IOS
			bool isStencilEnabled = pDepthStencil->pTexture->mtlPixelFormat == MTLPixelFormatDepth24Unorm_Stencil8;
#else
            bool isStencilEnabled = false;
#endif
            isStencilEnabled = isStencilEnabled || pDepthStencil->pTexture->mtlPixelFormat == MTLPixelFormatDepth32Float_Stencil8;
			if (isStencilEnabled)
			{
				pCmd->pRenderPassDesc.stencilAttachment.texture = pDepthStencil->pTexture->mtlTexture;
				pCmd->pRenderPassDesc.stencilAttachment.level = (depthMipSlice != -1 ? depthMipSlice : 0);
				pCmd->pRenderPassDesc.stencilAttachment.slice = (depthArraySlice != -1 ? depthArraySlice : 0);
			}
            
            // For on-tile (memoryless) textures, we never need to load or store.
            if (pDepthStencil->pTexture->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
            {
                pCmd->pRenderPassDesc.depthAttachment.loadAction =
                    (pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionDepth) : MTLLoadActionDontCare);
                pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
                pCmd->pRenderPassDesc.stencilAttachment.loadAction =
                    (pLoadActions != NULL ? util_to_mtl_load_action(pLoadActions->mLoadActionStencil) : MTLLoadActionDontCare);
                pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
            }
            else
            {
                pCmd->pRenderPassDesc.depthAttachment.loadAction =
                pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionDepth) : MTLLoadActionDontCare;
                pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionStore;
                if (isStencilEnabled)
                {
                    pCmd->pRenderPassDesc.stencilAttachment.loadAction =
                    pLoadActions ? util_to_mtl_load_action(pLoadActions->mLoadActionStencil) : MTLLoadActionDontCare;
                    pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionStore;
                }
                else
                {
                    pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
                    pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
                }
            }
            
			if (pLoadActions)
			{
				pCmd->pRenderPassDesc.depthAttachment.clearDepth = pLoadActions->mClearDepth.depth;
				if (isStencilEnabled)
					pCmd->pRenderPassDesc.stencilAttachment.clearStencil = pLoadActions->mClearDepth.stencil;
			}
		}
		else
		{
			pCmd->pRenderPassDesc.depthAttachment.loadAction = MTLLoadActionDontCare;
			pCmd->pRenderPassDesc.stencilAttachment.loadAction = MTLLoadActionDontCare;
			pCmd->pRenderPassDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
			pCmd->pRenderPassDesc.stencilAttachment.storeAction = MTLStoreActionDontCare;
		}

		util_end_current_encoders(pCmd, false);
		pCmd->mtlRenderEncoder = [pCmd->mtlCommandBuffer renderCommandEncoderWithDescriptor:pCmd->pRenderPassDesc];
	}
}

void cmdSetViewport(Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(LOG_TYPE_ERROR, "Using cmdSetViewport out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetViewport");
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

void cmdSetScissor(Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	ASSERT(pCmd);
	if (pCmd->mtlRenderEncoder == nil)
	{
		internal_log(LOG_TYPE_ERROR, "Using cmdSetScissor out of a cmdBeginRender / cmdEndRender block is not allowed", "cmdSetScissor");
		return;
	}

	// Get the maximum safe scissor values for the current render pass.
	uint32_t maxScissorX = pCmd->pRenderPassDesc.colorAttachments[0].texture.width > 0
							   ? (uint32_t)pCmd->pRenderPassDesc.colorAttachments[0].texture.width
							   : (uint32_t)pCmd->pRenderPassDesc.depthAttachment.texture.width;
	uint32_t maxScissorY = pCmd->pRenderPassDesc.colorAttachments[0].texture.height > 0
							   ? (uint32_t)pCmd->pRenderPassDesc.colorAttachments[0].texture.height
							   : (uint32_t)pCmd->pRenderPassDesc.depthAttachment.texture.height;
	uint32_t maxScissorW = maxScissorX - int32_t(max(x, 0));
	uint32_t maxScissorH = maxScissorY - int32_t(max(y, 0));

	// Make sure neither width or height are 0 (unsupported by Metal).
	if (width == 0u)
		width = 1u;
	if (height == 0u)
		height = 1u;

	MTLScissorRect scissor;
	scissor.x = min(x, maxScissorX);
	scissor.y = min(y, maxScissorY);
	scissor.width = min(width, maxScissorW);
	scissor.height = min(height, maxScissorH);

	[pCmd->mtlRenderEncoder setScissorRect:scissor];
}

void cmdBindPipeline(Cmd* pCmd, Pipeline* pPipeline)
{
	ASSERT(pCmd);
	ASSERT(pPipeline);

	pCmd->pShader = pPipeline->pShader;
	
	@autoreleasepool
	{
		if (pPipeline->mType == PIPELINE_TYPE_GRAPHICS)
		{
			[pCmd->mtlRenderEncoder setRenderPipelineState:pPipeline->mtlRenderPipelineState];

			[pCmd->mtlRenderEncoder setCullMode:(MTLCullMode)pPipeline->mCullMode];
			[pCmd->mtlRenderEncoder setTriangleFillMode:(MTLTriangleFillMode)pPipeline->mFillMode];
			[pCmd->mtlRenderEncoder setFrontFacingWinding:(MTLWinding)pPipeline->mWinding];
			[pCmd->mtlRenderEncoder setDepthBias:pPipeline->mDepthBias slopeScale:pPipeline->mSlopeScale clamp:0.0f];

			if (pCmd->pRenderPassDesc.depthAttachment.texture != nil)
			{
				[pCmd->mtlRenderEncoder setDepthStencilState:pPipeline->mtlDepthStencilState];
			}
			
			pCmd->selectedPrimitiveType = (MTLPrimitiveType)pPipeline->mMtlPrimitiveType;
		}
		else if (pPipeline->mType == PIPELINE_TYPE_COMPUTE)
		{
			if (!pCmd->mtlComputeEncoder)
			{
				util_end_current_encoders(pCmd, false);
				pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];
			}
			[pCmd->mtlComputeEncoder setComputePipelineState:pPipeline->mtlComputePipelineState];
		}
        else if (pPipeline->mType == PIPELINE_TYPE_RAYTRACING)
        {
            if (!pCmd->mtlComputeEncoder)
            {
                util_end_current_encoders(pCmd, false);
                pCmd->mtlComputeEncoder = [pCmd->mtlCommandBuffer computeCommandEncoder];
            }
        }
        else
        {
            ASSERT(false); // unknown pipline type
        }
	}
}

void cmdBindIndexBuffer(Cmd* pCmd, Buffer* pBuffer, uint32_t indexType, uint64_t offset)
{
	ASSERT(pCmd);
	ASSERT(pBuffer);

	pCmd->selectedIndexBuffer = pBuffer;
	pCmd->mSelectedIndexBufferOffset = offset;
	pCmd->mIndexType = (INDEX_TYPE_UINT16 == indexType ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
	pCmd->mIndexStride = (INDEX_TYPE_UINT16 == indexType ? sizeof(uint16_t) : sizeof(uint32_t));
}

void cmdBindVertexBuffer(Cmd* pCmd, uint32_t bufferCount, Buffer** ppBuffers, const uint32_t* pStrides, const uint64_t* pOffsets)
{
	ASSERT(pCmd);
	ASSERT(0 != bufferCount);
	ASSERT(ppBuffers);

	// When using a poss-tessellation vertex shader, the first vertex buffer bound is used as the tessellation factors buffer.
	uint startIdx = 0;
	if (pCmd->pShader && pCmd->pShader->mtlVertexShader.patchType != MTLPatchTypeNone)
	{
		startIdx = 1;
		[pCmd->mtlRenderEncoder setTessellationFactorBuffer:ppBuffers[0]->mtlBuffer offset:0 instanceStride:0];
	}

	for (uint32_t i = startIdx; i < bufferCount; i++)
	{
		[pCmd->mtlRenderEncoder setVertexBuffer:ppBuffers[i]->mtlBuffer
										 offset:(ppBuffers[i]->mPositionInHeap + (pOffsets ? pOffsets[i] : 0))
										atIndex:((i - startIdx)+VERTEX_BINDING_OFFSET)];
	}
}
    
void cmdDraw(Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex)
{
	ASSERT(pCmd);
	util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
	
	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType vertexStart:firstVertex vertexCount:vertexCount];
	}
	else    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
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
	
	util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
	
	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		if (firstInstance == 0)
		{
			[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount];
		}
		else
		{
			[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
									   vertexStart:firstVertex
									   vertexCount:vertexCount
									 instanceCount:instanceCount
									  baseInstance:firstInstance];
		}
	}
	else    // Tessellated draw version.
	{
		[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
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
	
	util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
	
	Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
	MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
	uint64_t     offset = pCmd->mSelectedIndexBufferOffset + (firstIndex * pCmd->mIndexStride);

	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		//only ios devices supporting gpu family 3_v1 and above can use baseVertex and baseInstance
		//if lower than 3_v1 render without base info but artifacts will occur if used.
#ifdef TARGET_IOS
		if ([pCmd->pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1])
#endif
		{
			[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
											   indexCount:indexCount
												indexType:indexType
											  indexBuffer:indexBuffer->mtlBuffer
										indexBufferOffset:offset
											instanceCount:1
											   baseVertex:firstVertex
											 baseInstance:0];
		}
#ifdef TARGET_IOS
		else
		{
			LOGF(LogLevel::eERROR, "Current device does not support ios gpuFamily 3_v1 feature set.");
			return;
		}
#endif
	}
	else    // Tessellated draw version.
	{
		//to supress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer->mtlBuffer
							patchIndexBufferOffset:0
						   controlPointIndexBuffer:indexBuf
					 controlPointIndexBufferOffset:0
									 instanceCount:1
									  baseInstance:0];
	}
}

void cmdDrawIndexedInstanced(
	Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance, uint32_t firstVertex)
{
	ASSERT(pCmd);

	util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
	
	Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
	MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
	uint64_t     offset = pCmd->mSelectedIndexBufferOffset + (firstIndex * pCmd->mIndexStride);

	if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
	{
		[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
										   indexCount:indexCount
											indexType:indexType
										  indexBuffer:indexBuffer->mtlBuffer
									indexBufferOffset:offset
										instanceCount:instanceCount
										   baseVertex:firstVertex
										 baseInstance:firstInstance];
	}
	else    // Tessellated draw version.
	{
		//to supress warning passing nil to controlPointIndexBuffer
		//todo: Add control point index buffer to be passed when necessary
		id<MTLBuffer> _Nullable indexBuf = nil;
		[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
										patchStart:firstIndex
										patchCount:indexCount
								  patchIndexBuffer:indexBuffer->mtlBuffer
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
	ASSERT(pCmd->mtlComputeEncoder != nil);

	util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);
	
	Shader* shader = pCmd->pShader;

	MTLSize threadsPerThreadgroup =
		MTLSizeMake(shader->mNumThreadsPerGroup[0], shader->mNumThreadsPerGroup[1], shader->mNumThreadsPerGroup[2]);
	MTLSize threadgroupCount = MTLSizeMake(groupCountX, groupCountY, groupCountZ);

	[pCmd->mtlComputeEncoder dispatchThreadgroups:threadgroupCount threadsPerThreadgroup:threadsPerThreadgroup];
}

void util_optimize_icb(Cmd* pCmd, uint maxCommandCount, Buffer* pIndirectCommandBuffer)
{
    ASSERT(pCmd);
    ASSERT(pIndirectCommandBuffer);
    
    if (maxCommandCount)
    {
        util_end_current_encoders(pCmd, false);
        
        pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
        
        [pCmd->mtlBlitEncoder optimizeIndirectCommandBuffer:pIndirectCommandBuffer->mtlIndirectCommandBuffer withRange:NSMakeRange(0, maxCommandCount)];
    }
}

void cmdExecuteIndirect(
	Cmd* pCmd, CommandSignature* pCommandSignature, uint maxCommandCount, Buffer* pIndirectBuffer, uint64_t bufferOffset,
	Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
	ASSERT(pCmd);
	
    /*
    // CPU encoded ICB
    MTLIndirectCommandBufferDescriptor* icbDescriptor = [MTLIndirectCommandBufferDescriptor alloc];
    icbDescriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
    icbDescriptor.inheritBuffers = NO;
    icbDescriptor.maxVertexBufferBindCount = 3;
    icbDescriptor.maxFragmentBufferBindCount = 0;
    icbDescriptor.inheritPipelineState = YES;
    
    id <MTLIndirectCommandBuffer> _indirectCommandBuffer = [pCmd->pRenderer->pDevice
                                                            newIndirectCommandBufferWithDescriptor:icbDescriptor
                                                            maxCommandCount:maxCommandCount
                                                            options:0];
    
    const Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
    const MTLIndexType indexType = (indexBuffer->mDesc.mIndexType == INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
    
    for (uint32_t i = 0; i < maxCommandCount; i++)
    {
        uint64_t     indirectBufferOffset = bufferOffset + sizeof(IndirectDrawIndexArguments) * i;
        
        id<MTLIndirectRenderCommand> ICBCommand = [_indirectCommandBuffer indirectRenderCommandAtIndex:i];
        
//        [ICBCommand setVertexBuffer:_vertexBuffer[objIndex]
//                             offset:0
//                            atIndex:AAPLVertexBufferIndexVertices];
        
//        [ICBCommand setVertexBuffer:_indirectFrameStateBuffer
//                             offset:0
//                            atIndex:AAPLVertexBufferIndexFrameState];
        
//        [ICBCommand setVertexBuffer:_objectParameters
//                             offset:0
//                            atIndex:AAPLVertexBufferIndexObjectParams];
        
//        const NSUInteger vertexCount = _vertexBuffer[objIndex].length/sizeof(AAPLVertex);
        
        // [pCmd->mtlRenderEncoder setFragmentBytes:&i length:sizeof(i) atIndex:20]; // drawId
        //[ICBCommand setFragmentBuffer:&i offset:sizeof(i) atIndex:20];
        
        IndirectDrawIndexArguments* args = static_cast<IndirectDrawIndexArguments*>(pIndirectBuffer->mtlBuffer.contents);
        
        [ICBCommand
         drawIndexedPrimitives:pCmd->selectedPrimitiveType
         indexCount:args[i].mIndexCount
         indexType:indexType
         indexBuffer:indexBuffer->mtlBuffer
         indexBufferOffset:0
         instanceCount:args[i].mInstanceCount
         baseVertex:args[i].mVertexOffset
         baseInstance:args[i].mStartInstance
        ];
    }
    
    util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
    [pCmd->mtlRenderEncoder executeCommandsInBuffer:_indirectCommandBuffer withRange:NSMakeRange(0, maxCommandCount)];
    */
    
	if (pCommandSignature->mDrawType == INDIRECT_COMMAND_BUFFER_OPTIMIZE)
	{
		util_optimize_icb(pCmd, maxCommandCount, pIndirectBuffer);
	}
	else if (pCommandSignature->mDrawType == INDIRECT_COMMAND_BUFFER)
	{
		if (maxCommandCount)
		{
			util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
			[pCmd->mtlRenderEncoder executeCommandsInBuffer:pIndirectBuffer->mtlIndirectCommandBuffer withRange:NSMakeRange(0, maxCommandCount)];
		}
	}
	else
	{
		for (uint32_t i = 0; i < maxCommandCount; i++)
		{
			if (pCommandSignature->mDrawType == INDIRECT_DRAW)
			{
				util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
				
				uint64_t indirectBufferOffset = bufferOffset + sizeof(IndirectDrawArguments) * i;
				if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
				{
					[pCmd->mtlRenderEncoder drawPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
											indirectBuffer:pIndirectBuffer->mtlBuffer
									  indirectBufferOffset:indirectBufferOffset];
				}
				else    // Tessellated draw version.
				{
#ifndef TARGET_IOS
					[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
									   patchIndexBuffer:nil
								 patchIndexBufferOffset:0
										 indirectBuffer:pIndirectBuffer->mtlBuffer
								   indirectBufferOffset:indirectBufferOffset];
#else
					// Tessellated indirect-draw is not supported on iOS.
					// Instead, read regular draw arguments from the indirect draw buffer.
					mapBuffer(pCmd->pRenderer, pIndirectBuffer, NULL);
					IndirectDrawArguments* pDrawArgs = (IndirectDrawArguments*)(pIndirectBuffer->pCpuMappedAddress) + indirectBufferOffset;
					unmapBuffer(pCmd->pRenderer, pIndirectBuffer);

					if (pDrawArgs->mVertexCount)
					{
					
					[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
											 patchStart:pDrawArgs->mStartVertex
											 patchCount:pDrawArgs->mVertexCount
									   patchIndexBuffer:nil
								 patchIndexBufferOffset:0
										  instanceCount:pDrawArgs->mInstanceCount
										   baseInstance:pDrawArgs->mStartInstance];
					}
#endif
				}
			}
			else if (pCommandSignature->mDrawType == INDIRECT_DRAW_INDEX)
			{
				util_barrier_required(pCmd, QUEUE_TYPE_GRAPHICS);
				
				Buffer*      indexBuffer = pCmd->selectedIndexBuffer;
				MTLIndexType indexType = (MTLIndexType)pCmd->mIndexType;
				uint64_t     indirectBufferOffset = bufferOffset + sizeof(IndirectDrawIndexArguments) * i;

				if (pCmd->pShader->mtlVertexShader.patchType == MTLPatchTypeNone)
				{
					[pCmd->mtlRenderEncoder setFragmentBytes:&i length:sizeof(i) atIndex:20]; // drawId
					
					[pCmd->mtlRenderEncoder drawIndexedPrimitives:(MTLPrimitiveType)pCmd->selectedPrimitiveType
														indexType:indexType
													  indexBuffer:indexBuffer->mtlBuffer
												indexBufferOffset:0
												   indirectBuffer:pIndirectBuffer->mtlBuffer
											 indirectBufferOffset:indirectBufferOffset];
				}
				else    // Tessellated draw version.
				{
#ifndef TARGET_IOS
					[pCmd->mtlRenderEncoder drawPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
									   patchIndexBuffer:indexBuffer->mtlBuffer
								 patchIndexBufferOffset:0
										 indirectBuffer:pIndirectBuffer->mtlBuffer
								   indirectBufferOffset:indirectBufferOffset];
#else
					// Tessellated indirect-draw is not supported on iOS.
					// Instead, read regular draw arguments from the indirect draw buffer.
					mapBuffer(pCmd->pRenderer, pIndirectBuffer, NULL);
					IndirectDrawIndexArguments* pDrawArgs =
						(IndirectDrawIndexArguments*)(pIndirectBuffer->pCpuMappedAddress) + indirectBufferOffset;
					unmapBuffer(pCmd->pRenderer, pIndirectBuffer);

					//to supress warning passing nil to controlPointIndexBuffer
					//todo: Add control point index buffer to be passed when necessary
					id<MTLBuffer> _Nullable ctrlPtIndexBuf = nil;
					[pCmd->mtlRenderEncoder drawIndexedPatches:pCmd->pShader->mtlVertexShader.patchControlPointCount
													patchStart:pDrawArgs->mStartIndex
													patchCount:pDrawArgs->mIndexCount
											  patchIndexBuffer:indexBuffer->mtlBuffer
										patchIndexBufferOffset:0
									   controlPointIndexBuffer:ctrlPtIndexBuf
								 controlPointIndexBufferOffset:0
												 instanceCount:pDrawArgs->mInstanceCount
												  baseInstance:pDrawArgs->mStartInstance];
#endif
				}
			}
			else if (pCommandSignature->mDrawType == INDIRECT_DISPATCH)
			{
				util_barrier_required(pCmd, QUEUE_TYPE_COMPUTE);

				Shader* shader = pCmd->pShader;
				MTLSize threadsPerThreadgroup =
					MTLSizeMake(shader->mNumThreadsPerGroup[0], shader->mNumThreadsPerGroup[1], shader->mNumThreadsPerGroup[2]);
				
				[pCmd->mtlComputeEncoder dispatchThreadgroupsWithIndirectBuffer:pIndirectBuffer->mtlBuffer indirectBufferOffset:bufferOffset threadsPerThreadgroup:threadsPerThreadgroup];
			}
		}
	}
}

void cmdResourceBarrier(Cmd* pCmd,
uint32_t numBufferBarriers, BufferBarrier* pBufferBarriers,
uint32_t numTextureBarriers, TextureBarrier* pTextureBarriers,
uint32_t numRtBarriers, RenderTargetBarrier* pRtBarriers)
{
    if (numBufferBarriers)
    {
        for (uint32_t i = 0; i < numBufferBarriers; ++i)
        {
            BufferBarrier* pTrans = &pBufferBarriers[i];
            Buffer*        pBuffer = pTrans->pBuffer;
            
            if (!(pTrans->mNewState & pBuffer->mCurrentState) || pBuffer->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
            {
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_BUFFERS;
                pBuffer->mCurrentState = pTrans->mNewState;
            }
        }
    }
    
    if (numTextureBarriers)
    {
        for (uint32_t i = 0; i < numTextureBarriers; ++i)
        {
            TextureBarrier* pTrans = &pTextureBarriers[i];
            Texture*        pTexture = pTrans->pTexture;
            
            if (!(pTrans->mNewState & pTexture->mCurrentState) || pTexture->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
            {
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_TEXTURES;
                pTexture->mCurrentState = pTrans->mNewState;
            }
        }
    }
	
	if (numRtBarriers)
    {
        for (uint32_t i = 0; i < numRtBarriers; ++i)
        {
            RenderTargetBarrier* pTrans = &pRtBarriers[i];
            Texture*        pTexture = pTrans->pRenderTarget->pTexture;
            
            if (!(pTrans->mNewState & pTexture->mCurrentState) || pTexture->mCurrentState == RESOURCE_STATE_UNORDERED_ACCESS)
            {
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_RENDERTARGETS;
				pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_TEXTURES;
                pTexture->mCurrentState = pTrans->mNewState;
            }
        }
    }
}

void cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
	ASSERT(pCmd);
	ASSERT(pSrcBuffer);
	ASSERT(pSrcBuffer->mtlBuffer);
	ASSERT(pBuffer);
	ASSERT(pBuffer->mtlBuffer);
	ASSERT(srcOffset + size <= pSrcBuffer->mSize);
	ASSERT(dstOffset + size <= pBuffer->mSize);

	util_end_current_encoders(pCmd, false);
	pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];

	util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
	
	[pCmd->mtlBlitEncoder copyFromBuffer:pSrcBuffer->mtlBuffer
							sourceOffset:pSrcBuffer->mPositionInHeap + srcOffset
								toBuffer:pBuffer->mtlBuffer
					   destinationOffset:pBuffer->mPositionInHeap + dstOffset
									size:size];
}

void cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pIntermediate, SubresourceDataDesc* pSubresourceDesc)
{
	util_end_current_encoders(pCmd, false);
	pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];

	util_barrier_required(pCmd, QUEUE_TYPE_TRANSFER);
	
#ifndef TARGET_IOS
	MTLBlitOption blitOptions = MTLBlitOptionNone;
#else
    // PVR formats get special case
	TinyImageFormat fmt = TinyImageFormat_FromMTLPixelFormat((TinyImageFormat_MTLPixelFormat)pTexture->mtlPixelFormat);
    uint64_t const tifname = (TinyImageFormat_Code(fmt) & TinyImageFormat_NAMESPACE_REQUIRED_BITS);
    bool const isPvrtc = (tifname == TinyImageFormat_NAMESPACE_PVRTC);

	MTLBlitOption blitOptions = isPvrtc ? MTLBlitOptionRowLinearPVRTC : MTLBlitOptionNone;
#endif

	// Copy to the texture's final subresource.
	[pCmd->mtlBlitEncoder copyFromBuffer:pIntermediate->mtlBuffer
							sourceOffset:pIntermediate->mPositionInHeap + pSubresourceDesc->mBufferOffset
					   sourceBytesPerRow:pSubresourceDesc->mRowPitch
					 sourceBytesPerImage:pSubresourceDesc->mSlicePitch
							  sourceSize:MTLSizeMake(pSubresourceDesc->mRegion.mWidth, pSubresourceDesc->mRegion.mHeight, pSubresourceDesc->mRegion.mDepth)
							   toTexture:pTexture->mtlTexture
						destinationSlice:pSubresourceDesc->mArrayLayer
						destinationLevel:pSubresourceDesc->mMipLevel
					   destinationOrigin:MTLOriginMake(pSubresourceDesc->mRegion.mXOffset, pSubresourceDesc->mRegion.mYOffset, pSubresourceDesc->mRegion.mZOffset)
								 options:blitOptions];
}

void acquireNextImage(Renderer* pRenderer, SwapChain* pSwapChain, Semaphore* pSignalSemaphore, Fence* pFence, uint32_t* pImageIndex)
{
	ASSERT(pRenderer);
	ASSERT(pRenderer->pDevice != nil);
	ASSERT(pSwapChain);
	ASSERT(pSignalSemaphore || pFence);

	CAMetalLayer* layer = (CAMetalLayer*)pSwapChain->pForgeView.layer;

	if (pSwapChain->mMTKDrawable == nil)
		pSwapChain->mMTKDrawable = [layer nextDrawable];

	// Look for the render target containing this texture.
	// If not found: assign it to an empty slot
	for (uint32_t i = 0; i < pSwapChain->mImageCount; i++)
	{
		RenderTarget* renderTarget = pSwapChain->ppRenderTargets[i];
		if (renderTarget->pTexture->mtlTexture == pSwapChain->mMTKDrawable.texture)
		{
			*pImageIndex = i;
			return;
		}
	}

	// Not found: assign the texture to an empty slot
	for (uint32_t i = 0; i < pSwapChain->mImageCount; i++)
	{
		RenderTarget* renderTarget = pSwapChain->ppRenderTargets[i];
		if (renderTarget->pTexture->mtlTexture == nil)
		{
			renderTarget->pTexture->mtlTexture = pSwapChain->mMTKDrawable.texture;

//			// Update the swapchain RT size according to the new drawable's size.
//			renderTarget->pTexture->mDesc.mWidth = (uint32_t)pSwapChain->mMTKDrawable.texture.width;
//			renderTarget->pTexture->mDesc.mHeight = (uint32_t)pSwapChain->mMTKDrawable.texture.height;
//			pSwapChain->ppRenderTargets[i]->mDesc.mWidth = renderTarget->pTexture->mDesc.mWidth;
//			pSwapChain->ppRenderTargets[i]->mDesc.mHeight = renderTarget->pTexture->mDesc.mHeight;

			*pImageIndex = i;
			return;
		}
	}

	// The swapchain textures have changed internally:
	// Invalidate the texures and re-acquire the render targets
	for (uint32_t i = 0; i < pSwapChain->mImageCount; i++)
	{
		pSwapChain->ppRenderTargets[i]->pTexture->mtlTexture = nil;
	}
	acquireNextImage(pRenderer, pSwapChain, pSignalSemaphore, pFence, pImageIndex);
}

void queueSubmit(Queue* pQueue, const QueueSubmitDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);
	
	uint32_t cmdCount = pDesc->mCmdCount;
	Cmd** ppCmds = pDesc->ppCmds;
	Fence* pFence = pDesc->pSignalFence;
	uint32_t waitSemaphoreCount = pDesc->mWaitSemaphoreCount;
	Semaphore** ppWaitSemaphores = pDesc->ppWaitSemaphores;
	uint32_t signalSemaphoreCount = pDesc->mSignalSemaphoreCount;
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

	// set the queue built-in semaphore to signal when all command lists finished their execution
	__block uint32_t commandsFinished = 0;
	__weak dispatch_semaphore_t completedFence = nil;
    
	if (pFence)
	{
		completedFence = pFence->pMtlSemaphore;
		pFence->mSubmitted = true;
	}
	for (uint32_t i = 0; i < cmdCount; i++)
	{
		__block Cmd* pCmd = ppCmds[i];
		[pCmd->mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
		{
			commandsFinished++;
            
            id<CommandBufferOverride> fixedObj = (id<CommandBufferOverride>)buffer;
            			
			if (pCmd->pLastFrameQuery)
			{
				const double gpuStartTime([fixedObj GPUStartTime]);
				const double gpuEndTime([fixedObj GPUEndTime]);

				pCmd->pLastFrameQuery->mGpuTimestampStart = gpuStartTime * GPU_FREQUENCY;
				pCmd->pLastFrameQuery->mGpuTimestampEnd = gpuEndTime * GPU_FREQUENCY;
			}
            
			if (commandsFinished == cmdCount)
			{
				if (completedFence)
					dispatch_semaphore_signal(completedFence);
            }
		}];
	}

	// commit the command lists
	for (uint32_t i = 0; i < cmdCount; i++)
	{
		// register the following semaphores for signaling after the work has been done
		for (uint32_t j = 0; j < signalSemaphoreCount; j++)
		{
			__weak dispatch_semaphore_t blockSemaphore = ppSignalSemaphores[j]->pMtlSemaphore;
			[ppCmds[i]->mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal(blockSemaphore); }];
		}

		// Commit any uncommited encoder. This is necessary before committing the command buffer
		util_end_current_encoders(ppCmds[i], false);

		[ppCmds[i]->mtlCommandBuffer commit];
	}
}

void queuePresent(Queue* pQueue, const QueuePresentDesc* pDesc)
{
	ASSERT(pQueue);
	ASSERT(pDesc);

	if (pDesc->pSwapChain)
	{
		SwapChain* pSwapChain = pDesc->pSwapChain;
		ASSERT(pQueue->mtlCommandQueue != nil);

		@autoreleasepool
		{
#ifndef TARGET_IOS
			[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];
#else
			//[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable
			//							 afterMinimumDuration:1.0 / 120.0];
			[pSwapChain->presentCommandBuffer presentDrawable:pSwapChain->mMTKDrawable];
#endif
		}
		
		[pSwapChain->presentCommandBuffer commit];

		// after committing a command buffer no more commands can be encoded on it: create a new command buffer for future commands
		pSwapChain->presentCommandBuffer = [pQueue->mtlCommandQueue commandBuffer];
		pSwapChain->mMTKDrawable = nil;
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
			dispatch_semaphore_wait(ppFences[i]->pMtlSemaphore, DISPATCH_TIME_FOREVER);
		ppFences[i]->mSubmitted = false;
	}
}

void waitQueueIdle(Queue* pQueue)
{
	ASSERT(pQueue);
	id<MTLCommandBuffer> waitCmdBuf = [pQueue->mtlCommandQueue commandBufferWithUnretainedReferences];

	[waitCmdBuf commit];
    [waitCmdBuf waitUntilCompleted];    
}

void getFenceStatus(Renderer* pRenderer, Fence* pFence, FenceStatus* pFenceStatus)
{
	ASSERT(pFence);
	*pFenceStatus = FENCE_STATUS_COMPLETE;
	if (pFence->mSubmitted)
	{
		// Check the fence status (and mark it as unsubmitted it if it has succesfully decremented).
		long status = dispatch_semaphore_wait(pFence->pMtlSemaphore, DISPATCH_TIME_NOW);
		if (status == 0)
			pFence->mSubmitted = false;

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

void cmdBeginDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	[pCmd->mtlCommandBuffer pushDebugGroup:[NSString stringWithFormat:@"%s", pName]];
}

void cmdBeginDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
{
	va_list argptr;
	va_start(argptr, pFormat);
	char buffer[65536];
	vsnprintf(buffer, sizeof(buffer), pFormat, argptr);
	va_end(argptr);
	cmdBeginDebugMarker(pCmd, r, g, b, buffer);
}

void cmdEndDebugMarker(Cmd* pCmd)
{
	[pCmd->mtlCommandBuffer popDebugGroup];
}

void cmdAddDebugMarker(Cmd* pCmd, float r, float g, float b, const char* pName)
{
	if (pCmd->mtlRenderEncoder)
		[pCmd->mtlRenderEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlComputeEncoder)
		[pCmd->mtlComputeEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
	else if (pCmd->mtlBlitEncoder)
		[pCmd->mtlBlitEncoder insertDebugSignpost:[NSString stringWithFormat:@"%s", pName]];
}

void cmdAddDebugMarkerf(Cmd* pCmd, float r, float g, float b, const char* pFormat, ...)
{
	va_list argptr;
	va_start(argptr, pFormat);
	char buffer[65536];
	vsnprintf(buffer, sizeof(buffer), pFormat, argptr);
	va_end(argptr);

	cmdAddDebugMarker(pCmd, r, g, b, buffer);
}

void getTimestampFrequency(Queue* pQueue, double* pFrequency)
{
    *pFrequency = GPU_FREQUENCY;
}
    
void addQueryPool(Renderer* pRenderer, const QueryPoolDesc* pDesc, QueryPool** ppQueryPool)
{
	QueryPool* pQueryPool = (QueryPool*)conf_calloc(1, sizeof(QueryPool));
	ASSERT(pQueryPool);

	pQueryPool->mCount = pDesc->mQueryCount;
    
    // currently this is just a dummy struct for iOS GPU frame counters
    pQueryPool->mGpuTimestampStart = 0.0;
    pQueryPool->mGpuTimestampEnd = 0.0;

	*ppQueryPool = pQueryPool;
}

void removeQueryPool(Renderer* pRenderer, QueryPool* pQueryPool)
{
	SAFE_FREE(pQueryPool);
}

void cmdResetQueryPool(Cmd* pCmd, QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
}

void cmdBeginQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
    pCmd->pLastFrameQuery = pQueryPool;
}
    
void cmdEndQuery(Cmd* pCmd, QueryPool* pQueryPool, QueryDesc* pQuery)
{
}

void cmdResolveQuery(Cmd* pCmd, QueryPool* pQueryPool, Buffer* pReadbackBuffer, uint32_t startQuery, uint32_t queryCount)
{
    uint64_t* data = (uint64_t*)pReadbackBuffer->mtlBuffer.contents;
    
    data[0] = pQueryPool->mGpuTimestampStart;
    data[1] = pQueryPool->mGpuTimestampEnd;
}
    
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
        [pCmd->mtlRenderEncoder setVertexBytes: pConstants
                                        length: pDesc->mSize
                                       atIndex: pDesc->mReg];
    }
    
    if (pDesc->mUsedStages & SHADER_STAGE_FRAG)
    {
        [pCmd->mtlRenderEncoder setFragmentBytes: pConstants
                                          length: pDesc->mSize
                                         atIndex: pDesc->mReg];
    }

    if (pDesc->mUsedStages & SHADER_STAGE_COMP)
    {
        [pCmd->mtlComputeEncoder setBytes: pConstants
                                   length: pDesc->mSize
                                  atIndex: pDesc->mReg];
    }
}

void util_set_resources_graphics(Cmd* pCmd, MTLDescriptorSet::DescriptorResources* resources, uint32_t stages)
{
    for (uint32_t i = 0; i < RESOURCE_TYPE_COUNT; ++i)
    {
        const uint32_t resourceCount(resources->mResourcesCount[i]);
        
        if (!resourceCount)
            continue;
        
        switch (i)
        {
            case RESOURCE_TYPE_RESOURCE_RW:
                if(@available(iOS 13.0, macOS 10.15, *))
                {
                    [pCmd->mtlRenderEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                   count: resourceCount
                                                   usage: MTLResourceUsageRead | MTLResourceUsageSample | MTLResourceUsageWrite
                                                  stages: stages];
                        
                }
                else
                {
                    [pCmd->mtlRenderEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                   count: resourceCount
                                                   usage: MTLResourceUsageRead | MTLResourceUsageSample | MTLResourceUsageWrite];
                }
                break;
            case RESOURCE_TYPE_RESOURCE_READ_ONLY:
                if(@available(iOS 13.0, macOS 10.15, *))
                {
                    [pCmd->mtlRenderEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                   count: resourceCount
                                                   usage: MTLResourceUsageRead | MTLResourceUsageSample
                                                  stages: stages];
                }
                else
                {
                    [pCmd->mtlRenderEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                   count: resourceCount
                                                   usage: MTLResourceUsageRead | MTLResourceUsageSample];
                }
                break;
            case RESOURCE_TYPE_HEAP:
                if(@available(iOS 13.0, macOS 10.15, *))
                {
                    [pCmd->mtlRenderEncoder useHeaps: (__unsafe_unretained id<MTLHeap>*)(void*)resources->mResources[i]
                                               count: resourceCount
                                              stages: stages];
                }
                else
                {
                    [pCmd->mtlRenderEncoder useHeaps: (__unsafe_unretained id<MTLHeap>*)(void*)resources->mResources[i]
                                               count: resourceCount];
                }
                break;
            default:
                ASSERT(0);
                break;
        }
    }
}

void util_set_resources_compute(Cmd* pCmd, MTLDescriptorSet::DescriptorResources* resources)
{
    for (uint32_t i = 0; i < RESOURCE_TYPE_COUNT; ++i)
    {
        const uint32_t resourceCount(resources->mResourcesCount[i]);
        
        if (!resourceCount)
            continue;
        
        switch (i)
        {
            case RESOURCE_TYPE_RESOURCE_RW:
                [pCmd->mtlComputeEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                count: resourceCount
                                                usage: MTLResourceUsageRead | MTLResourceUsageSample | MTLResourceUsageWrite];
                break;
            case RESOURCE_TYPE_RESOURCE_READ_ONLY:
                [pCmd->mtlComputeEncoder useResources: (__unsafe_unretained id<MTLResource>*)(void*)resources->mResources[i]
                                                count: resourceCount
                                                usage: MTLResourceUsageRead | MTLResourceUsageSample];
                break;
            case RESOURCE_TYPE_HEAP:
                [pCmd->mtlComputeEncoder useHeaps: (__unsafe_unretained id<MTLHeap>*)(void*)resources->mResources[i]
                                            count: resourceCount];
                break;
            default:
                ASSERT(0);
                break;
        }
    }
}

id<MTLBuffer> util_get_buffer_from_descriptorset(DescriptorSet* pDescriptorSet, uint32_t index)
{
    ASSERT(pDescriptorSet);
    ASSERT(index < pDescriptorSet->pMtl->mMaxSets);
    
    const uint32 descriptorsCount((uint32)pDescriptorSet->pMtl->mArgumentBufferDescriptors.size());
    ASSERT(descriptorsCount);
    auto it = pDescriptorSet->pMtl->mArgumentBufferDescriptors.begin();
    MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor(it->second);
    return argumentBufferDescriptor->mArgumentBuffers[index];
}

void util_set_argument(DescriptorSet* pDescriptorSet, const DescriptorInfo* pDesc, const MTLDescriptorSet::ArgumentBufferDescriptor* argumentBufferDescriptor, uint32_t index,const DescriptorData* pParam, ResourcesList& resources)
{
    // set argument buffer to update
    [argumentBufferDescriptor->mArgumentEncoder setArgumentBuffer: argumentBufferDescriptor->mArgumentBuffers[index]
                                                           offset: 0];
    
    const DescriptorType type((DescriptorType)pDesc->mType);
    
    const uint32_t arrayCount(max(1U, pParam->mCount));
    for (uint32_t j = 0; j < arrayCount; ++j)
    {
        switch (type)
        {
            case DESCRIPTOR_TYPE_SAMPLER:
                {
                    ASSERT(pDesc->mIsArgumentBufferField);
                    
                    [argumentBufferDescriptor->mArgumentEncoder setSamplerState: pParam->ppSamplers[j]->mtlSamplerState
                                                                       atIndex: pDesc->mHandleIndex + j];
                }
                break;
            case DESCRIPTOR_TYPE_TEXTURE:
            case DESCRIPTOR_TYPE_RW_TEXTURE:
                {
                    ASSERT(pDesc->mIsArgumentBufferField);
                    
                    if (type == DESCRIPTOR_TYPE_RW_TEXTURE && pParam->ppTextures[j]->pMtlUAVDescriptors)
                    {
                        [argumentBufferDescriptor->mArgumentEncoder setTexture: pParam->ppTextures[j]->pMtlUAVDescriptors[pParam->mUAVMipSlice]
                                                                       atIndex: pDesc->mHandleIndex + j];
                    }
                    else
                    {
                        [argumentBufferDescriptor->mArgumentEncoder setTexture: pParam->ppTextures[j]->mtlTexture
                                                                       atIndex: pDesc->mHandleIndex + j];
                    }
                    
                    // resources
                    {
                        eastl::unordered_set<void*>* resourceStorage = &resources[RESOURCE_TYPE_HEAP];
						void* resource = NULL;
						if (pParam->ppTextures[j]->pMtlAllocation)
							resource = (__bridge void*)pParam->ppTextures[j]->pMtlAllocation->GetMemory();
                        
                        if (resource == NULL)
                        {
                            resourceStorage = (type == DESCRIPTOR_TYPE_RW_TEXTURE) ? &resources[RESOURCE_TYPE_RESOURCE_RW] : &resources[RESOURCE_TYPE_RESOURCE_READ_ONLY];
                            resource = (__bridge void*)pParam->ppTextures[j]->mtlTexture;
                        }
                        ASSERT(resource);
                        
                        eastl::unordered_set<void*>::const_iterator it = resourceStorage->find(resource);
                        if (it == resourceStorage->end())
                            resourceStorage->insert(resource);
                    }
                }
                break;
            case DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER:
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER:
            case DESCRIPTOR_TYPE_BUFFER_RAW:
            case DESCRIPTOR_TYPE_RW_BUFFER:
            case DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                {
                    if (pDesc->mIsArgumentBufferField)
                    {
						id<MTLResource> buffer = nullptr;
						ResourceAllocation* allocation = nullptr;
						uint64_t positionInHeap = 0;
						
                        if (pParam->mExtractBuffer)
                        {
                            buffer = util_get_buffer_from_descriptorset(pParam->ppDescriptorSet[0], pParam->mDescriptorSetBufferIndex);
                        }
                        else
                        {
							if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
								buffer = pParam->ppBuffers[j]->mtlIndirectCommandBuffer;
							else
								buffer = pParam->ppBuffers[j]->mtlBuffer;
							allocation = pParam->ppBuffers[j]->pMtlAllocation;
							positionInHeap = pParam->ppBuffers[j]->mPositionInHeap;
                        }

#ifdef ARGUMENTBUFFER_DEBUG
						LOGF(LogLevel::eWARNING, "Updated field '%s'[%d] (index %d) to buffer %d (%p) for descriptor %p", pDesc->mDesc.name, j, (pDesc->mHandleIndex + j), index, buffer, argumentBufferDescriptor->mArgumentBuffers[index]);
#endif

                        if (type == DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER)
                        {
                            [argumentBufferDescriptor->mArgumentEncoder setIndirectCommandBuffer: (id<MTLIndirectCommandBuffer>)buffer
                                                                                         atIndex: pDesc->mHandleIndex + j];
                        }
                        else
                        {
                            [argumentBufferDescriptor->mArgumentEncoder setBuffer: (id<MTLBuffer>)buffer
                                                                           offset: (positionInHeap + (pParam->pOffsets ? pParam->pOffsets[j] : 0))
                                                                          atIndex: pDesc->mHandleIndex + j];
                        }
                        
                        // resources
                        {
                            eastl::unordered_set<void*>* resourceStorage = &resources[RESOURCE_TYPE_HEAP];
                            void* resource = allocation ? (__bridge void*)allocation->GetMemory() : nullptr;
                            
                            if (resource == NULL)
                            {
                                resourceStorage = (type == DESCRIPTOR_TYPE_RW_BUFFER || type == DESCRIPTOR_TYPE_RW_BUFFER_RAW) ?  &resources[RESOURCE_TYPE_RESOURCE_RW] : &resources[RESOURCE_TYPE_RESOURCE_READ_ONLY];
                                resource = (__bridge void*)buffer;
                            }
                            ASSERT(resource);
                            
                            eastl::unordered_set<void*>::const_iterator it = resourceStorage->find(resource);
                            if (it == resourceStorage->end())
                                resourceStorage->insert(resource);
                        }
                    }
                    else
                    {
                        ASSERT(j == 0 && "Array not supported");
                        
                        MTLDescriptorSet::RootBuffer rootBuffer;
                        rootBuffer.mBuffer = pParam->ppBuffers[j];
                        rootBuffer.mBufferIndex = pDesc->mHandleIndex;
                        
                        bool found = false;
                        for (uint32_t k = 0; k < pDescriptorSet->pMtl->mRootBuffers.size(); ++k)
                        {
                            if (pDescriptorSet->pMtl->mRootBuffers[k].mBufferIndex == pDesc->mHandleIndex)
                            {
                                pDescriptorSet->pMtl->mRootBuffers[k] = rootBuffer;
                                pDescriptorSet->pMtl->mRootBuffers[k].mOffset = pParam->pOffsets ? pParam->pOffsets[j] : 0;
                                
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found)
                        {
                            pDescriptorSet->pMtl->mRootBuffers.push_back(rootBuffer);
                        }
                    }
                }
                break;
            case DESCRIPTOR_TYPE_RENDER_PIPELINE_STATE:
                {
                    ASSERT(pDesc->mIsArgumentBufferField);
                    [argumentBufferDescriptor->mArgumentEncoder setRenderPipelineState: pParam->ppPipelines[j]->mtlRenderPipelineState
                                                                               atIndex: pDesc->mHandleIndex + j];
                }
                break;
            case DESCRIPTOR_TYPE_RAY_TRACING:
                {
                    // todo?
                    ASSERT(false);
                }
                break;
            default:
                ASSERT(0); // unsupported descriptor type
                break;
        }
    }
}

uint64_t util_pthread_to_uint64(const pthread_t& value)
{
	uint64_t threadId = 0;
	memcpy(&threadId, &value, sizeof(value));
	return threadId;
}


bool util_is_mtl_depth_pixel_format(const MTLPixelFormat format)
{
	if (format == MTLPixelFormatDepth32Float || format == MTLPixelFormatDepth32Float_Stencil8)
		return true;
	
#ifdef TARGET_IOS
	if (@available(iOS 13, *))
	{
		if (format == MTLPixelFormatDepth16Unorm)
			return true;
	}
#else
	if (format == MTLPixelFormatDepth16Unorm || format == MTLPixelFormatDepth24Unorm_Stencil8)
		return true;
#endif
		
	return false;
}

bool util_is_mtl_compressed_pixel_format(const MTLPixelFormat format)
{
#ifndef TARGET_IOS
	return format >= MTLPixelFormatBC1_RGBA;
#else
	return format >= MTLPixelFormatPVRTC_RGB_2BPP;
#endif
}

MTLVertexFormat util_to_mtl_vertex_format(const TinyImageFormat format)
{
	switch (format)
	{
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

		case TinyImageFormat_R32_UINT: return MTLVertexFormatUInt;
		case TinyImageFormat_R32G32_UINT: return MTLVertexFormatUInt2;
		case TinyImageFormat_R32G32B32_UINT: return MTLVertexFormatUInt3;
		case TinyImageFormat_R32G32B32A32_UINT: return MTLVertexFormatUInt4;

		// TODO add this UINT + UNORM format to TinyImageFormat
//		case TinyImageFormat_RGB10A2: return MTLVertexFormatUInt1010102Normalized;
		default: break;
	}
	LOGF(LogLevel::eERROR, "Unknown vertex format: %d", format);
	return MTLVertexFormatInvalid;
}

MTLLoadAction util_to_mtl_load_action(const LoadActionType loadActionType)
{
	if (loadActionType == LOAD_ACTION_DONTCARE)
		return MTLLoadActionDontCare;
	else if (loadActionType == LOAD_ACTION_LOAD)
		return MTLLoadActionLoad;
	else
		return MTLLoadActionClear;
}
	
void util_end_current_encoders(Cmd* pCmd, bool forceBarrier)
{
	const bool barrierRequired(pCmd->pQueue->mBarrierFlags);
	
	if (pCmd->mtlRenderEncoder != nil)
	{
		ASSERT(pCmd->mtlComputeEncoder == nil && pCmd->mtlBlitEncoder == nil);
		
		if (barrierRequired || forceBarrier)
		{
			[pCmd->mtlRenderEncoder updateFence:pCmd->pQueue->mtlQueueFence afterStages:MTLRenderStageFragment];
			pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
		}
		
		[pCmd->mtlRenderEncoder endEncoding];
		pCmd->mtlRenderEncoder = nil;
	}
	
	if (pCmd->mtlComputeEncoder != nil)
	{
		ASSERT(pCmd->mtlRenderEncoder == nil && pCmd->mtlBlitEncoder == nil);
		
		if (barrierRequired || forceBarrier)
		{
			[pCmd->mtlComputeEncoder updateFence:pCmd->pQueue->mtlQueueFence];
			pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
		}
		
		[pCmd->mtlComputeEncoder endEncoding];
		pCmd->mtlComputeEncoder = nil;
	}
	
	if (pCmd->mtlBlitEncoder != nil)
	{
		ASSERT(pCmd->mtlRenderEncoder == nil && pCmd->mtlComputeEncoder == nil);
		
		if (barrierRequired || forceBarrier)
		{
			[pCmd->mtlBlitEncoder updateFence:pCmd->pQueue->mtlQueueFence];
			pCmd->pQueue->mBarrierFlags |= BARRIER_FLAG_FENCE;
		}
		
		[pCmd->mtlBlitEncoder endEncoding];
		pCmd->mtlBlitEncoder = nil;
	}
}

void util_barrier_required(Cmd* pCmd, const QueueType& encoderType)
{
	if (pCmd->pQueue->mBarrierFlags)
	{
		if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
		{
			switch (encoderType)
			{
				case QUEUE_TYPE_GRAPHICS:
					[pCmd->mtlRenderEncoder waitForFence:pCmd->pQueue->mtlQueueFence beforeStages:MTLRenderStageVertex];
					break;
				case QUEUE_TYPE_COMPUTE:
					[pCmd->mtlComputeEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
					break;
				case QUEUE_TYPE_TRANSFER:
					[pCmd->mtlBlitEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
					break;
				default:
					ASSERT(false);
			}
		}
		else
		{
			switch (encoderType)
			{
				case QUEUE_TYPE_GRAPHICS:
#ifdef TARGET_IOS
					// memoryBarrierWithScope for render encoder is unavailable for iOS
					// fallback to fence
					[pCmd->mtlRenderEncoder waitForFence:pCmd->pQueue->mtlQueueFence beforeStages:MTLRenderStageVertex];
#else
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
					{
						[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers afterStages:MTLRenderStageFragment beforeStages:MTLRenderStageVertex];
					}
					
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
					{
						[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeTextures afterStages:MTLRenderStageFragment beforeStages:MTLRenderStageVertex];
					}
					
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_RENDERTARGETS)
					{
						[pCmd->mtlRenderEncoder memoryBarrierWithScope:MTLBarrierScopeRenderTargets afterStages:MTLRenderStageFragment beforeStages:MTLRenderStageVertex];
					}
#endif
					break;
					
				case QUEUE_TYPE_COMPUTE:
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_BUFFERS)
					{
						[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeBuffers];
					}
					
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_TEXTURES)
					{
						[pCmd->mtlComputeEncoder memoryBarrierWithScope:MTLBarrierScopeTextures];
					}
					break;
					
				case QUEUE_TYPE_TRANSFER:
					// we cant use barriers with blit encoder, only fence if available
					if (pCmd->pQueue->mBarrierFlags & BARRIER_FLAG_FENCE)
					{
						[pCmd->mtlBlitEncoder waitForFence:pCmd->pQueue->mtlQueueFence];
					}
					break;
					
				default:
					ASSERT(false);
			}
		}
		
		pCmd->pQueue->mBarrierFlags = 0;
	}
}

bool util_is_compatible_texture_view(const MTLTextureType& textureType, const MTLTextureType& subviewTye)
{
	switch (textureType)
	{
		case MTLTextureType1D:
			if (subviewTye != MTLTextureType1D)
				return false;
			return true;
		case MTLTextureType2D:
			if (subviewTye != MTLTextureType2D && subviewTye != MTLTextureType2DArray)
				return false;
			return true;
		case MTLTextureType2DArray:
		case MTLTextureTypeCube:
		case MTLTextureTypeCubeArray:
			if (subviewTye != MTLTextureType2D && subviewTye != MTLTextureType2DArray && subviewTye != MTLTextureTypeCube &&
				subviewTye != MTLTextureTypeCubeArray)
				return false;
			return true;
		case MTLTextureType3D:
			if (subviewTye != MTLTextureType3D)
				return false;
			return true;
		default: return false;
	}

	return false;
}

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture, const bool isRT, const bool forceNonPrivate)
{
	ASSERT(ppTexture);

	size_t totalSize = sizeof(Texture);
	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		totalSize += pDesc->mMipLevels * sizeof(id<MTLTexture>);

	Texture* pTexture = (Texture*)conf_calloc(1, totalSize);
	ASSERT(pTexture);
	
	void* mem = (pTexture + 1);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
		pTexture->pMtlUAVDescriptors = (id<MTLTexture> __strong*)mem;

	if (pDesc->mHeight == 1)
		((TextureDesc*)pDesc)->mMipLevels = 1;

	pTexture->mtlPixelFormat = (MTLPixelFormat) TinyImageFormat_ToMTLPixelFormat(pDesc->mFormat);

    if (pDesc->mFormat == TinyImageFormat_D24_UNORM_S8_UINT && !pRenderer->pCapBits->canRenderTargetWriteTo[pDesc->mFormat])
    {
		internal_log(LOG_TYPE_WARN, "Format D24S8 is not supported on this device. Using D32S8 instead", "addTexture");
		pTexture->mtlPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		((TextureDesc*)pDesc)->mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
	}

	pTexture->mIsCompressed = util_is_mtl_compressed_pixel_format((MTLPixelFormat)pTexture->mtlPixelFormat);

	if (pDesc->mHostVisible)
	{
		internal_log(
			LOG_TYPE_WARN, "Host visible textures are not supported, memory of resulting texture will not be mapped for CPU visibility",
			"addTexture");
	}

	// If we've passed a native handle, it means the texture is already on device memory, and we just need to assign it.
	if (pDesc->pNativeHandle)
	{
		pTexture->mOwnsImage = false;
		pTexture->mtlTexture = (id<MTLTexture>)CFBridgingRelease(pDesc->pNativeHandle);
	}
	// Otherwise, we need to create a new texture.
	else
	{
		pTexture->mOwnsImage = true;

		// Create a MTLTextureDescriptor that matches our requirements.
		MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];

		textureDesc.pixelFormat = (MTLPixelFormat)pTexture->mtlPixelFormat;
		textureDesc.width = pDesc->mWidth;
		textureDesc.height = pDesc->mHeight;
		textureDesc.depth = pDesc->mDepth;
		textureDesc.mipmapLevelCount = pDesc->mMipLevels;
		textureDesc.sampleCount = pDesc->mSampleCount;
		textureDesc.arrayLength = pDesc->mArraySize;
		textureDesc.storageMode = forceNonPrivate ? MTLStorageModeShared : MTLStorageModePrivate;
		textureDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
		
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

		switch(textureType)
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
				else if ([pRenderer->pDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1])
				{
					textureDesc.textureType = MTLTextureTypeCubeArray;
					textureDesc.arrayLength /= 6;
				}
				else
				{
					internal_log(LOG_TYPE_ERROR, "Cube Array textures are not supported on this iOS device", "addTexture");
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
		default:
			break;
		}

		bool isDepthBuffer = util_is_mtl_depth_pixel_format((MTLPixelFormat)pTexture->mtlPixelFormat);
		bool isMultiSampled = pDesc->mSampleCount > 1;
		if (isDepthBuffer || isMultiSampled)
			textureDesc.resourceOptions = MTLResourceStorageModePrivate;
#ifdef TARGET_IOS
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_ON_TILE)
		{
			textureDesc.resourceOptions = MTLResourceStorageModeMemoryless;
		}
#endif

		if (isRT || isDepthBuffer)
			textureDesc.usage |= MTLTextureUsageRenderTarget;
		//Create texture views only if DESCRIPTOR_RW_TEXTURE was used.
		if ((pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE) != 0)
		{
			textureDesc.usage |= MTLTextureUsagePixelFormatView;
			textureDesc.usage |= MTLTextureUsageShaderWrite;
		}

		// Allocate the texture's memory.
		AllocatorMemoryRequirements mem_reqs = { 0 };
		mem_reqs.usage = forceNonPrivate ? (ResourceMemoryUsage)RESOURCE_MEMORY_USAGE_UNKNOWN : (ResourceMemoryUsage)RESOURCE_MEMORY_USAGE_GPU_ONLY;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_OWN_MEMORY_BIT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_SHARED_BIT;
		if (pDesc->mFlags & TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
			mem_reqs.flags |= RESOURCE_MEMORY_REQUIREMENT_SHARED_ADAPTER_BIT;

		TextureCreateInfo alloc_info = { textureDesc, isRT || isDepthBuffer, isMultiSampled, pDesc->pDebugName };
		bool              allocSuccess;
		allocSuccess = createTexture(pRenderer->pResourceAllocator, &alloc_info, &mem_reqs, pTexture);
		ASSERT(allocSuccess);
	}

	NSRange slices = NSMakeRange(0, pDesc->mArraySize);

	if (pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE)
	{
		MTLTextureType uavType = pTexture->mtlTexture.textureType;
		if (pTexture->mtlTexture.textureType == MTLTextureTypeCube || pTexture->mtlTexture.textureType == MTLTextureTypeCubeArray)
		{
			uavType = MTLTextureType2DArray;
		}
		for (uint32_t i = 0; i < pDesc->mMipLevels; ++i)
		{
			NSRange levels = NSMakeRange(i, 1);
			pTexture->pMtlUAVDescriptors[i] = [pTexture->mtlTexture newTextureViewWithPixelFormat:pTexture->mtlTexture.pixelFormat
																					  textureType:uavType
																						   levels:levels
																						   slices:slices];
		}
	}
	
	pTexture->mNodeIndex = pDesc->mNodeIndex;
	pTexture->mStartState = pDesc->mStartState;
	pTexture->mUav = pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE;
	pTexture->mMipLevels = pDesc->mMipLevels;
	pTexture->mFlags = pDesc->mFlags;
	pTexture->mWidth = pDesc->mWidth;
	pTexture->mHeight = pDesc->mHeight;
	pTexture->mDepth = pDesc->mDepth;

	*ppTexture = pTexture;
}

/************************************************************************/
/************************************************************************/
#endif    // RENDERER_IMPLEMENTATION

#if defined(__cplusplus) && defined(RENDERER_CPP_NAMESPACE)
}    // namespace RENDERER_CPP_NAMESPACE
#endif
#endif
