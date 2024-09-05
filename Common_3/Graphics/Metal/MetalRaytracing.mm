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

/*
 iOS 15.0+
 macOS 12.0+
 Xcode 13.0+
 */

#include "../../Graphics/GraphicsConfig.h"

#ifdef METAL

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

#import "../../Graphics/Interfaces/IGraphics.h"
#import "../../Graphics/Interfaces/IRay.h"
#import "../../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../Utilities/Interfaces/ILog.h"

#include "../../Utilities/Interfaces/IMemory.h"

#if defined(MTL_RAYTRACING_AVAILABLE)

void add_texture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** pTexture, const bool isRT);
void util_end_current_encoders(Cmd* pCmd, bool forceBarrier);
void util_barrier_required(Cmd* pCmd, const QueueType& encoderType);

void addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer);
void removeBuffer(Renderer* pRenderer, Buffer* pBuffer);

struct Raytracing
{
    Renderer* pRenderer;
};

struct IOS17_API AccelerationStructure
{
    id<MTLAccelerationStructure> mAS;
    Buffer*                      pInstanceDescBuffer;
    Buffer*                      pScratchBuffer;
    union
    {
        MTLPrimitiveAccelerationStructureDescriptor* pBottomDesc;
        struct
        {
            MTLInstanceAccelerationStructureDescriptor* pTopDesc;
            NOREFS id* pBottomASReferences;
            uint32_t   mBottomASReferenceCount;
        };
    };
    uint32_t                  mDescCount;
    AccelerationStructureType mType;
};

IOS17_API
bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
{
    ASSERT(pRenderer);
    ASSERT(ppRaytracing);

    Raytracing* pRaytracing = (Raytracing*)tf_calloc(1, sizeof(Raytracing));
    ASSERT(pRaytracing);

    pRaytracing->pRenderer = pRenderer;

    *ppRaytracing = pRaytracing;
    return true;
}

IOS17_API
void exitRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
{
    ASSERT(pRenderer);
    ASSERT(pRaytracing);
    tf_free(pRaytracing);
}
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_ACCELERATION_STRUCTURE_VERTEX_FORMAT)
static inline FORGE_CONSTEXPR MTLAttributeFormat ToMTLAttributeFormat(TinyImageFormat fmt)
{
    switch (fmt)
    {
    case TinyImageFormat_R8G8_UINT:
        return MTLAttributeFormatUChar2;
    case TinyImageFormat_R8G8B8_UINT:
        return MTLAttributeFormatUChar3;
    case TinyImageFormat_R8G8B8A8_UINT:
        return MTLAttributeFormatUChar4;

    case TinyImageFormat_R8G8_SINT:
        return MTLAttributeFormatChar2;
    case TinyImageFormat_R8G8B8_SINT:
        return MTLAttributeFormatChar3;
    case TinyImageFormat_R8G8B8A8_SINT:
        return MTLAttributeFormatChar4;

    case TinyImageFormat_R8G8_UNORM:
        return MTLAttributeFormatUChar2Normalized;
    case TinyImageFormat_R8G8B8_UNORM:
        return MTLAttributeFormatUChar3Normalized;
    case TinyImageFormat_R8G8B8A8_UNORM:
        return MTLAttributeFormatUChar4Normalized;

    case TinyImageFormat_R8G8_SNORM:
        return MTLAttributeFormatChar2Normalized;
    case TinyImageFormat_R8G8B8_SNORM:
        return MTLAttributeFormatChar3Normalized;
    case TinyImageFormat_R8G8B8A8_SNORM:
        return MTLAttributeFormatChar4Normalized;

    case TinyImageFormat_R16G16_UNORM:
        return MTLAttributeFormatUShort2Normalized;
    case TinyImageFormat_R16G16B16_UNORM:
        return MTLAttributeFormatUShort3Normalized;
    case TinyImageFormat_R16G16B16A16_UNORM:
        return MTLAttributeFormatUShort4Normalized;

    case TinyImageFormat_R16G16_SNORM:
        return MTLAttributeFormatShort2Normalized;
    case TinyImageFormat_R16G16B16_SNORM:
        return MTLAttributeFormatShort3Normalized;
    case TinyImageFormat_R16G16B16A16_SNORM:
        return MTLAttributeFormatShort4Normalized;

    case TinyImageFormat_R16G16_SINT:
        return MTLAttributeFormatShort2;
    case TinyImageFormat_R16G16B16_SINT:
        return MTLAttributeFormatShort3;
    case TinyImageFormat_R16G16B16A16_SINT:
        return MTLAttributeFormatShort4;

    case TinyImageFormat_R16G16_UINT:
        return MTLAttributeFormatUShort2;
    case TinyImageFormat_R16G16B16_UINT:
        return MTLAttributeFormatUShort3;
    case TinyImageFormat_R16G16B16A16_UINT:
        return MTLAttributeFormatUShort4;

    case TinyImageFormat_R16G16_SFLOAT:
        return MTLAttributeFormatHalf2;
    case TinyImageFormat_R16G16B16_SFLOAT:
        return MTLAttributeFormatHalf3;
    case TinyImageFormat_R16G16B16A16_SFLOAT:
        return MTLAttributeFormatHalf4;

    case TinyImageFormat_R32_SFLOAT:
        return MTLAttributeFormatFloat;
    case TinyImageFormat_R32G32_SFLOAT:
        return MTLAttributeFormatFloat2;
    case TinyImageFormat_R32G32B32_SFLOAT:
        return MTLAttributeFormatFloat3;
    case TinyImageFormat_R32G32B32A32_SFLOAT:
        return MTLAttributeFormatFloat4;

    case TinyImageFormat_R32_SINT:
        return MTLAttributeFormatInt;
    case TinyImageFormat_R32G32_SINT:
        return MTLAttributeFormatInt2;
    case TinyImageFormat_R32G32B32_SINT:
        return MTLAttributeFormatInt3;
    case TinyImageFormat_R32G32B32A32_SINT:
        return MTLAttributeFormatInt4;

    case TinyImageFormat_R10G10B10A2_SNORM:
        return MTLAttributeFormatInt1010102Normalized;
    case TinyImageFormat_R10G10B10A2_UNORM:
        return MTLAttributeFormatUInt1010102Normalized;

    case TinyImageFormat_R32_UINT:
        return MTLAttributeFormatUInt;
    case TinyImageFormat_R32G32_UINT:
        return MTLAttributeFormatUInt2;
    case TinyImageFormat_R32G32B32_UINT:
        return MTLAttributeFormatUInt3;
    case TinyImageFormat_R32G32B32A32_UINT:
        return MTLAttributeFormatUInt4;

        // TODO add this UINT + UNORM format to TinyImageFormat
        //		case TinyImageFormat_RGB10A2: return MTLAttributeFormatUInt1010102Normalized;
    default:
        break;
    }

    switch (fmt)
    {
    case TinyImageFormat_R8_UINT:
        return MTLAttributeFormatUChar;
    case TinyImageFormat_R8_SINT:
        return MTLAttributeFormatChar;
    case TinyImageFormat_R8_UNORM:
        return MTLAttributeFormatUCharNormalized;
    case TinyImageFormat_R8_SNORM:
        return MTLAttributeFormatCharNormalized;
    case TinyImageFormat_R16_UNORM:
        return MTLAttributeFormatUShortNormalized;
    case TinyImageFormat_R16_SNORM:
        return MTLAttributeFormatShortNormalized;
    case TinyImageFormat_R16_SINT:
        return MTLAttributeFormatShort;
    case TinyImageFormat_R16_UINT:
        return MTLAttributeFormatUShort;
    case TinyImageFormat_R16_SFLOAT:
        return MTLAttributeFormatHalf;
    default:
        break;
    }

    return MTLAttributeFormatInvalid;
}
#endif

IOS17_API
static inline FORGE_CONSTEXPR MTLAccelerationStructureInstanceOptions ToMTLASOptions(AccelerationStructureInstanceFlags flags)
{
    MTLAccelerationStructureInstanceOptions ret = MTLAccelerationStructureInstanceOptionNone;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE)
        ret |= MTLAccelerationStructureInstanceOptionOpaque;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE)
        ret |= MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
    if (flags & ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE)
        ret |= MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise;

    return ret;
}

IOS17_API
void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc,
                              AccelerationStructure** ppAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pDesc);
    ASSERT(ppAccelerationStructure);

    NSMutableArray<id<MTLAccelerationStructure>>* primitiveASArray = [[NSMutableArray alloc] init];
    NSMutableArray<id<MTLAccelerationStructure>>* uniqueBottomAS = [[NSMutableArray alloc] init];
    uint32_t                                      bottomReferenceCount = 0;
    size_t                                        memSize = sizeof(AccelerationStructure);
    if (ACCELERATION_STRUCTURE_TYPE_TOP == pDesc->mType)
    {
        for (uint32_t i = 0; i < pDesc->mTop.mDescCount; ++i)
        {
            AccelerationStructureInstanceDesc* pInst = &pDesc->mTop.pInstanceDescs[i];
            [primitiveASArray addObject:pInst->pBottomAS->mAS];
        }

        [uniqueBottomAS setArray:[[NSSet setWithArray:primitiveASArray] allObjects]];
        bottomReferenceCount = (uint32_t)[uniqueBottomAS count];
        memSize += bottomReferenceCount * sizeof(id);
    }

    AccelerationStructure* pAS = (AccelerationStructure*)tf_calloc(1, memSize);
    ASSERT(pAS);

    pAS->mType = pDesc->mType;

    id<MTLDevice> device = pRaytracing->pRenderer->pDevice;

    // Query for the sizes needed to store and build the acceleration structure.
    MTLAccelerationStructureSizes accelSizes = {};

    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == pDesc->mType)
    {
        NSMutableArray<MTLAccelerationStructureTriangleGeometryDescriptor*>* geomDescs = [[NSMutableArray alloc] init];
        pAS->mDescCount = pDesc->mBottom.mDescCount;

        // Create a primitive acceleration structure descriptor to contain all the triangle geometry.
        MTLPrimitiveAccelerationStructureDescriptor* asDesc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];

        for (uint32_t j = 0; j < pAS->mDescCount; ++j)
        {
            AccelerationStructureGeometryDesc*                  pGeom = &pDesc->mBottom.pGeometryDescs[j];
            MTLAccelerationStructureTriangleGeometryDescriptor* geomDesc = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];

            if (pGeom->mIndexCount)
            {
                ASSERT(pGeom->pIndexBuffer);
                geomDesc.indexBuffer = pGeom->pIndexBuffer->pBuffer;
                geomDesc.indexBufferOffset = pGeom->mIndexOffset;
                geomDesc.indexType = (pGeom->mIndexType == INDEX_TYPE_UINT16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32);
                geomDesc.triangleCount = pGeom->mIndexCount / 3;
            }
            else
            {
                geomDesc.triangleCount = pGeom->mVertexCount / 3;
            }

            ASSERT(pGeom->pVertexBuffer);
            ASSERT(pGeom->mVertexCount);

            geomDesc.vertexBuffer = pGeom->pVertexBuffer->pBuffer;
            geomDesc.vertexBufferOffset = pGeom->mVertexOffset;
            geomDesc.vertexStride = pGeom->mVertexStride;
#if defined(ENABLE_ACCELERATION_STRUCTURE_VERTEX_FORMAT)
            if (MTL_RAYTRACING_SUPPORTED)
            {
                geomDesc.vertexFormat = ToMTLAttributeFormat(pGeom->mVertexFormat);
            }
#endif

            [geomDescs addObject:geomDesc];
        }

        asDesc.geometryDescriptors = geomDescs;
        pAS->pBottomDesc = asDesc;
        accelSizes = [device accelerationStructureSizesWithDescriptor:asDesc];
    }
    else
    {
        pAS->mBottomASReferenceCount = bottomReferenceCount;
        void* mem = (pAS + 1);
        pAS->pBottomASReferences = (NOREFS id*)(mem);
        for (uint32_t i = 0; i < pAS->mBottomASReferenceCount; ++i)
        {
            pAS->pBottomASReferences[i] = uniqueBottomAS[i];
        }
        pAS->mDescCount = pDesc->mTop.mDescCount;

        MTLInstanceAccelerationStructureDescriptor* asDesc = [MTLInstanceAccelerationStructureDescriptor descriptor];

        BufferDesc instanceDesc = {};
        instanceDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        instanceDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        instanceDesc.mSize = sizeof(MTLAccelerationStructureInstanceDescriptor) * pAS->mDescCount;
        addBuffer(pRaytracing->pRenderer, &instanceDesc, &pAS->pInstanceDescBuffer);

        MTLAccelerationStructureInstanceDescriptor* instanceDescs =
            (MTLAccelerationStructureInstanceDescriptor*)pAS->pInstanceDescBuffer->pCpuMappedAddress;

        NSMutableArray<id<MTLAccelerationStructure>>* primitiveASArray = [[NSMutableArray alloc] init];

        for (uint32_t i = 0; i < pDesc->mTop.mDescCount; ++i)
        {
            AccelerationStructureInstanceDesc* pInst = &pDesc->mTop.pInstanceDescs[i];
            [primitiveASArray addObject:pInst->pBottomAS->mAS];
        }

        for (uint32_t i = 0; i < pDesc->mTop.mDescCount; ++i)
        {
            AccelerationStructureInstanceDesc* pInst = &pDesc->mTop.pInstanceDescs[i];
            ASSERT(pInst->pBottomAS);
            instanceDescs[i].options = ToMTLASOptions(pInst->mFlags);
            instanceDescs[i].mask = pInst->mInstanceMask;
            instanceDescs[i].accelerationStructureIndex = (uint32_t)[primitiveASArray indexOfObject:pInst->pBottomAS->mAS];
            // Copy the first three rows of the instance transformation matrix. Metal
            // assumes that the bottom row is (0, 0, 0, 1), which allows the renderer to
            // tightly pack instance descriptors in memory.
            for (uint32_t column = 0; column < 4; ++column)
            {
                for (uint32_t row = 0; row < 3; ++row)
                {
                    instanceDescs[i].transformationMatrix.columns[column][row] = pInst->mTransform[row * 4 + column];
                }
            }
        }

        asDesc.instanceCount = pAS->mDescCount;
        asDesc.instanceDescriptorBuffer = pAS->pInstanceDescBuffer->pBuffer;
        asDesc.instancedAccelerationStructures = primitiveASArray;

        pAS->pTopDesc = asDesc;
        accelSizes = [device accelerationStructureSizesWithDescriptor:asDesc];
    }

    // Allocate an acceleration structure large enough for this descriptor. This method
    // doesn't actually build the acceleration structure, but rather allocates memory.
    pAS->mAS = [device newAccelerationStructureWithSize:accelSizes.accelerationStructureSize];
    ASSERT(pAS->mAS);

    // Allocate scratch space Metal uses to build the acceleration structure.
    // Use MTLResourceStorageModePrivate for the best performance because the sample
    // doesn't need access to buffer's contents.
    BufferDesc scratchBufferDesc = {};
    scratchBufferDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
    scratchBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    scratchBufferDesc.mStartState = RESOURCE_STATE_COMMON;
    scratchBufferDesc.mFlags = BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
    scratchBufferDesc.mSize = accelSizes.buildScratchBufferSize;
    addBuffer(pRaytracing->pRenderer, &scratchBufferDesc, &pAS->pScratchBuffer);

    *ppAccelerationStructure = pAS;
}

IOS17_API
void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    ASSERT(pRaytracing);
    ASSERT(pAccelerationStructure);

    if (ACCELERATION_STRUCTURE_TYPE_TOP == pAccelerationStructure->mType)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pInstanceDescBuffer);
        pAccelerationStructure->pTopDesc = nil;
    }
    else
    {
        pAccelerationStructure->pBottomDesc = nil;
    }

    if (pAccelerationStructure->pScratchBuffer)
    {
        removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
        pAccelerationStructure->pScratchBuffer = NULL;
    }

    pAccelerationStructure->mAS = nil;
    tf_free(pAccelerationStructure);
}

IOS17_API
void removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
{
    if (!pAccelerationStructure->pScratchBuffer)
    {
        return;
    }
    removeBuffer(pRaytracing->pRenderer, pAccelerationStructure->pScratchBuffer);
    pAccelerationStructure->pScratchBuffer = NULL;
}

IOS17_API
void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
{
    if (!pCmd->pASEncoder)
    {
        util_end_current_encoders(pCmd, true);
        pCmd->pASEncoder = [pCmd->pCommandBuffer accelerationStructureCommandEncoder];
    }

    AccelerationStructure* as = pDesc->pAccelerationStructure;
    if (ACCELERATION_STRUCTURE_TYPE_BOTTOM == as->mType)
    {
        [pCmd->pASEncoder buildAccelerationStructure:as->mAS
                                          descriptor:as->pBottomDesc
                                       scratchBuffer:as->pScratchBuffer->pBuffer
                                 scratchBufferOffset:0];
    }
    else
    {
        [pCmd->pASEncoder buildAccelerationStructure:as->mAS
                                          descriptor:as->pTopDesc
                                       scratchBuffer:as->pScratchBuffer->pBuffer
                                 scratchBufferOffset:0];
    }

    if (pDesc->mIssueRWBarrier)
    {
        util_end_current_encoders(pCmd, true);
    }
}

IOS17_API
id<MTLAccelerationStructure> getMTLAccelerationStructure(AccelerationStructure* pAccelerationStructure)
{
    return pAccelerationStructure->mAS;
}

IOS17_API
void getMTLAccelerationStructureBottomReferences(AccelerationStructure* pAccelerationStructure, uint32_t* pOutReferenceCount,
                                                 NOREFS id** pOutReferences)
{
    ASSERT(ACCELERATION_STRUCTURE_TYPE_TOP == pAccelerationStructure->mType);
    *pOutReferenceCount = pAccelerationStructure->mBottomASReferenceCount;
    *pOutReferences = pAccelerationStructure->pBottomASReferences;
}

struct SSVGFDenoiser
{
    id pDenoiser; // MPSSVGFDenoiser*
};

void addSSVGFDenoiser(Renderer* pRenderer, SSVGFDenoiser** ppDenoiser)
{
    SSVGFDenoiser* denoiser = (SSVGFDenoiser*)tf_calloc(1, sizeof(SSVGFDenoiser));
    denoiser->pDenoiser = [[MPSSVGFDenoiser alloc] initWithDevice:pRenderer->pDevice];
    *ppDenoiser = denoiser;
}

void removeSSVGFDenoiser(SSVGFDenoiser* pDenoiser)
{
    if (!pDenoiser)
    {
        return;
    }

    pDenoiser->pDenoiser = nil;
    tf_free(pDenoiser);
}

void clearSSVGFDenoiserTemporalHistory(SSVGFDenoiser* pDenoiser)
{
    ASSERT(pDenoiser);

    [(MPSSVGFDenoiser*)pDenoiser->pDenoiser clearTemporalHistory];
}

void cmdSSVGFDenoise(Cmd* pCmd, SSVGFDenoiser* pDenoiser, Texture* pSourceTexture, Texture* pMotionVectorTexture,
                     Texture* pDepthNormalTexture, Texture* pPreviousDepthNormalTexture, Texture** ppOut)
{
    ASSERT(pDenoiser);
    if (pCmd->pComputeEncoder)
    {
        [pCmd->pComputeEncoder memoryBarrierWithScope:MTLBarrierScopeTextures];
    }

    util_end_current_encoders(pCmd, false);

    MPSSVGFDenoiser* denoiser = (MPSSVGFDenoiser*)pDenoiser->pDenoiser;

    id<MTLTexture> resultTexture = [denoiser encodeToCommandBuffer:pCmd->pCommandBuffer
                                                     sourceTexture:pSourceTexture->pTexture
                                               motionVectorTexture:pMotionVectorTexture->pTexture
                                                depthNormalTexture:pDepthNormalTexture->pTexture
                                        previousDepthNormalTexture:pPreviousDepthNormalTexture->pTexture];

    struct MetalNativeTextureHandle
    {
        id<MTLTexture>   pTexture;
        CVPixelBufferRef pPixelBuffer;
    };
    MetalNativeTextureHandle handle = { resultTexture };

    TextureDesc resultTextureDesc = {};
    resultTextureDesc.mFlags = TEXTURE_CREATION_FLAG_NONE;
    resultTextureDesc.mWidth = (uint32_t)resultTexture.width;
    resultTextureDesc.mHeight = (uint32_t)resultTexture.height;
    resultTextureDesc.mDepth = (uint32_t)resultTexture.depth;
    resultTextureDesc.mArraySize = (uint32_t)resultTexture.arrayLength;
    resultTextureDesc.mMipLevels = (uint32_t)resultTexture.mipmapLevelCount;
    resultTextureDesc.mSampleCount = SAMPLE_COUNT_1;
    resultTextureDesc.mSampleQuality = 0;
    resultTextureDesc.mFormat = TinyImageFormat_FromMTLPixelFormat((TinyImageFormat_MTLPixelFormat)resultTexture.pixelFormat);
    resultTextureDesc.mClearValue = {};
    resultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    resultTextureDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
    resultTextureDesc.pNativeHandle = &handle;

    resultTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;

    add_texture(pCmd->pRenderer, &resultTextureDesc, ppOut, false);
    (*ppOut)->mpsTextureAllocator = denoiser.textureAllocator;
}
#endif

#endif
