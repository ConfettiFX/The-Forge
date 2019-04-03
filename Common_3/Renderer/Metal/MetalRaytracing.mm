/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
 https://developer.apple.com/documentation/metalperformanceshaders/metal_for_accelerating_ray_tracing
 
 iOS 12.0+
 macOS 10.14+
 Xcode 10.0+
 */

#ifdef METAL

#if !defined(__APPLE__) && !defined(TARGET_OS_MAC)
#error "MacOs is needed!"
#endif
#import <simd/simd.h>
#import <MetalKit/MetalKit.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

#import "../IRenderer.h"
#import "../IRay.h"
#import "../ResourceLoader.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Interfaces/IMemoryManager.h"

#define MAX_BUFFER_BINDINGS 31

extern void mtl_createShaderReflection(Renderer* pRenderer, Shader* shader, const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, tinystl::unordered_map<uint32_t, MTLVertexFormat>* vertexAttributeFormats, ShaderReflection* pOutReflection);
    
    struct AccelerationStructure
    {
        MPSAccelerationStructureGroup* pSharedGroup;
        NSMutableArray<MPSTriangleAccelerationStructure*>* pBottomAS;
        MPSInstanceAccelerationStructure *pInstanceAccel;
        id <MTLBuffer> mVertexPositionBuffer;
        id <MTLBuffer> mIndexBuffer;
        id <MTLBuffer> mMasks;
        id <MTLBuffer> mInstanceIDs;
        id <MTLBuffer> mHitGroupIndices;
        tinystl::vector<uint32_t> mActiveHitGroups;
    };
    
    struct ShaderReference
    {
        uint32_t hitShader;
        uint32_t missShader;
        bool     active;
    };

    struct RaytracingShaderInfoSet
    {
        id <MTLBuffer>             mHitSettings;
        NSMutableArray<id <MTLComputePipelineState> >*  mHitPipelines;
        NSMutableArray* mHitGroupsRaysBuffers; //id <MTLBuffer>
        NSMutableArray* mIntersectionBuffer; //id <MTLBuffer>
        NSMutableArray* mPayloadBuffer; //id <MTLBuffer>
        ShaderReference*    pHitReferences;
    };
    
    struct RaytracingShaderTable
    {
        RaytracingShaderInfoSet mHitShadersInfo;
        RaytracingShaderInfoSet mMissShadersInfo;
		DescriptorBinder* pDescriptorBinder;
        uint32_t mRayGenHitRef;
        uint32_t mRayGenMissRef;
        bool mInvokeShaders;
    };
    
    struct RaysDispatchUniformBuffer
    {
        unsigned int width;
        unsigned int height;
        unsigned int blocksWide;
    };
    
    //struct used in shaders. Here it is declared to use it's sizeof() for rayStride
    struct Ray {
        float3 origin;
        uint mask;
        float3 direction;
        float maxDistance;
        uint isPrimaryRay;
    };

    
    struct HitShaderSettings
    {
        uint32_t hitGroupID;
    };
    
    struct RaytracingPipeline
    {
        id <MTLComputePipelineState> mRayPipeline;
        NSMutableArray<id <MTLComputePipelineState> >*  mHitPipelines;
        char**                                          mHitGroupNames;
        NSMutableArray<id <MTLComputePipelineState> >*  mMissPipelines;
        char**                                          mMissGroupNames;
        MPSRayIntersector* mIntersector;
        id <MTLBuffer> mRayGenRaysBuffer;
        id <MTLBuffer> mIntersectionBuffer;
        id <MTLBuffer> mSettingsBuffer;
        id <MTLBuffer> mPayloadBuffer;
        uint32_t       mMaxRaysCount;
        uint32_t       mPayloadRecordSize;
    };
    
    //implemented in MetalRenderer.mm
    extern void util_end_current_encoders(Cmd* pCmd);

    bool isRaytracingSupported(Renderer* pRenderer)
    {
        return true;
    }

    bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing)
    {
        Raytracing* pRaytracing = conf_new<Raytracing>();
        // Create a raytracer for our Metal device
        pRaytracing->pIntersector = [[MPSRayIntersector alloc] initWithDevice: pRenderer->pDevice];
        
        MPSRayOriginMinDistanceDirectionMaxDistance s;
        pRaytracing->pIntersector.rayDataType = MPSRayDataTypeOriginMaskDirectionMaxDistance;
        pRaytracing->pIntersector.rayStride = sizeof(Ray);
        pRaytracing->pIntersector.rayMaskOptions = MPSRayMaskOptionPrimitive;
        pRaytracing->pRenderer = pRenderer;
        
        *ppRaytracing = pRaytracing;
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////
    void computeBuffersSize(const AccelerationStructureDescTop* pDesc, unsigned* pVbSize, unsigned* pIbSize, unsigned* pMasksSize)
    {
        ASSERT(pDesc->mIndexType == INDEX_TYPE_UINT16 || pDesc->mIndexType == INDEX_TYPE_UINT32);
        unsigned indexSize = pDesc->mIndexType == INDEX_TYPE_UINT16 ? 2 : 4;
        unsigned vbSize = 0;
        unsigned ibSize = 0;
        unsigned mskSize = 0;
        for (unsigned i = 0; i < pDesc->mBottomASDescsCount; ++i)
        {
            for (unsigned idesc = 0; idesc < pDesc->mBottomASDescs[i].mDescCount; ++idesc)
            {
                vbSize += pDesc->mBottomASDescs[i].pGeometryDescs[idesc].vertexCount * sizeof(float3);
                ibSize += pDesc->mBottomASDescs[i].pGeometryDescs[idesc].indicesCount * indexSize;
                
                if (pDesc->mBottomASDescs[i].pGeometryDescs[idesc].indicesCount > 0)
                    mskSize += (pDesc->mBottomASDescs[i].pGeometryDescs[idesc].indicesCount / 3) * sizeof(uint32_t);
                else
                    mskSize += (pDesc->mBottomASDescs[i].pGeometryDescs[idesc].vertexCount / 3) * sizeof(uint32_t);
            }
        }
        if (pVbSize != NULL)
        {
            *pVbSize = vbSize;
        }
        if (pIbSize != NULL)
        {
            *pIbSize = ibSize;
        }
        if (pMasksSize != NULL)
        {
            *pMasksSize = mskSize;
        }
    }
    
    //////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////
    struct ASOffset{
        tinystl::vector<unsigned> vbGeometriesOffsets;
        tinystl::vector<unsigned> ibGeometriesOffsets;
        tinystl::vector<unsigned> masksGeometriesOffsets;
        unsigned vbSize;
        unsigned ibSize;
        unsigned trianglesCount;
        unsigned vertexStride;
    };
    
    template<typename T1, typename T2>
    void copyIndices(T1* dst, T2* src, unsigned count)
    {
        for (unsigned i = 0; i < count; ++i)
            dst[i] = src[i];
    }
    
    void createVertexAndIndexBuffers(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc,
                                     id <MTLBuffer>* pVbBuffer, id <MTLBuffer>* pIbBuffer, id <MTLBuffer>* pMasksBuffer,
                                     tinystl::vector<ASOffset>& outOffsets)
    {
        outOffsets.resize(pDesc->mBottomASDescsCount);
        // Vertex data should be stored in private or managed buffers on discrete GPU systems (AMD, NVIDIA).
        // Private buffers are stored entirely in GPU memory and cannot be accessed by the CPU. Managed
        // buffers maintain a copy in CPU memory and a copy in GPU memory.
        MTLResourceOptions options = 0;
#if !TARGET_OS_IPHONE
        options = MTLResourceStorageModeManaged;
#else
        options = MTLResourceStorageModeShared;
#endif
        unsigned vbSize = 0, ibSize = 0, mskSize = 0;
        computeBuffersSize(pDesc, &vbSize, &ibSize, &mskSize);
        id <MTLBuffer> _vertexPositionBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:vbSize options:options];
        id <MTLBuffer> _indexBuffer = nil;
        id <MTLBuffer> _masksBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:mskSize options:options];
        
        unsigned vbOffset = 0;
        unsigned ibOffset = 0;
        unsigned mbOffset = 0;
        uint8_t* vbDstPtr = static_cast<uint8_t*>(_vertexPositionBuffer.contents);
        uint8_t* ibDstPtr = NULL;
        if (ibSize > 0)
        {
            _indexBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:ibSize options:options];
            ibDstPtr = static_cast<uint8_t*>(_indexBuffer.contents);
        }
        
        tinystl::vector<uint32_t> masksVector;
        masksVector.reserve(pDesc->mBottomASDescsCount * 1000); //initial reservation

        for (unsigned i = 0; i < pDesc->mBottomASDescsCount; ++i)
        {
            ASOffset& asoffset = outOffsets[i];
            asoffset.vbGeometriesOffsets.resize(pDesc->mBottomASDescs[i].mDescCount);
            asoffset.ibGeometriesOffsets.resize(pDesc->mBottomASDescs[i].mDescCount);
            asoffset.masksGeometriesOffsets.resize(pDesc->mBottomASDescs[i].mDescCount);
            asoffset.vbSize = 0;
            asoffset.ibSize = 0;
            asoffset.trianglesCount = 0;
            asoffset.vertexStride = sizeof(float3);
            for (unsigned j = 0; j < pDesc->mBottomASDescs[i].mDescCount; ++j)
            {
                asoffset.vbGeometriesOffsets[j] = vbOffset;
                asoffset.ibGeometriesOffsets[j] = ibOffset;
                asoffset.masksGeometriesOffsets[j] = mbOffset;
                
                // Copy vertex data into buffers
                memcpy(&vbDstPtr[vbOffset],
                       pDesc->mBottomASDescs[i].pGeometryDescs[j].pVertexArray,
                       pDesc->mBottomASDescs[i].pGeometryDescs[j].vertexCount * sizeof(float3));
                vbOffset += pDesc->mBottomASDescs[i].pGeometryDescs[j].vertexCount * sizeof(float3);
                asoffset.vbSize += pDesc->mBottomASDescs[i].pGeometryDescs[j].vertexCount * sizeof(float3);
                
                if (ibDstPtr != NULL)
                {
                    unsigned indexSize = (pDesc->mIndexType == INDEX_TYPE_UINT16 ? 2 : 4);
                    void* instanceIndexPtr = (pDesc->mBottomASDescs[i].pGeometryDescs[j].indexType == INDEX_TYPE_UINT16 ?
                                                                    (void*)pDesc->mBottomASDescs[i].pGeometryDescs[j].pIndices16 :
                                                                    (void*)pDesc->mBottomASDescs[i].pGeometryDescs[j].pIndices32);
                    if (pDesc->mIndexType == pDesc->mBottomASDescs[i].pGeometryDescs[j].indexType)
                    {
                        memcpy(&ibDstPtr[ibOffset], instanceIndexPtr,
                               pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount * indexSize);
                    } else
                    {
                        if (pDesc->mIndexType == INDEX_TYPE_UINT16 &&
                            pDesc->mBottomASDescs[i].pGeometryDescs[j].indexType == INDEX_TYPE_UINT32)
                        {
                            copyIndices<uint16_t, uint32_t>((uint16_t*)&ibDstPtr[ibOffset],
                                                            pDesc->mBottomASDescs[i].pGeometryDescs[j].pIndices32,
                                                            pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount);
                        } else
                        if (pDesc->mIndexType == INDEX_TYPE_UINT32 &&
                            pDesc->mBottomASDescs[i].pGeometryDescs[j].indexType == INDEX_TYPE_UINT16)
                        {
                            copyIndices<uint32_t, uint16_t>((uint32_t*)&ibDstPtr[ibOffset],
                                        pDesc->mBottomASDescs[i].pGeometryDescs[j].pIndices16,
                                        pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount);
                        } else
                        {
                            ASSERT(false && "New index type was introduced!?");
                        }
                    }
                    
                    ibOffset += pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount * indexSize;
                    asoffset.ibSize += pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount * indexSize;
                    asoffset.trianglesCount += pDesc->mBottomASDescs[i].pGeometryDescs[j].indicesCount / 3;
                } else
                {
                    asoffset.trianglesCount += pDesc->mBottomASDescs[i].pGeometryDescs[j].vertexCount / 3;
                }
                for (unsigned msk = 0; msk < asoffset.trianglesCount; ++msk)
                {
                    masksVector.push_back(0xFFFFFFFF);
                }

            }
            
        }
        
        memcpy(_masksBuffer.contents, masksVector.data(), masksVector.size() * sizeof(uint32_t));
        
#if !TARGET_OS_IPHONE
        [_vertexPositionBuffer didModifyRange:NSMakeRange(0, _vertexPositionBuffer.length)];
        if (ibSize > 0)
        {
            [_indexBuffer didModifyRange:NSMakeRange(0, _indexBuffer.length)];
        }
        [_masksBuffer didModifyRange:NSMakeRange(0, _masksBuffer.length)];
#endif
        
        *pVbBuffer = _vertexPositionBuffer;
        *pIbBuffer = _indexBuffer;
        *pMasksBuffer = _masksBuffer;
    }
    
    id <MTLBuffer> createInstanceIDBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
    {
        MTLResourceOptions options = 0;
#if !TARGET_OS_IPHONE
        options = MTLResourceStorageModeManaged;
#else
        options = MTLResourceStorageModeShared;
#endif
        
        unsigned bufferSize = pDesc->mInstancesDescCount * sizeof(uint32_t);
        id <MTLBuffer> result = [pRaytracing->pRenderer->pDevice newBufferWithLength:bufferSize options:options];
        
        uint32_t *ptr = (uint32_t*)result.contents;
        for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
        {
            ptr[i] = pDesc->pInstanceDescs[i].mInstanceID;
        }
        
#if !TARGET_OS_IPHONE
        [result didModifyRange:NSMakeRange(0, result.length)];
#endif
        
        return result;
    }
    
    id <MTLBuffer> createInstancesIndicesBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
    {
        tinystl::vector<uint32_t> instancesIndices(pDesc->mInstancesDescCount);
        for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
            instancesIndices[i] = pDesc->pInstanceDescs[i].mAccelerationStructureIndex;
        
        ASSERT(pDesc->mInstancesDescCount > 0);
        unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
        
        MTLResourceOptions options = 0;
#if !TARGET_OS_IPHONE
        options = MTLResourceStorageModeManaged;
#else
        options = MTLResourceStorageModeShared;
#endif
        
        id <MTLBuffer> instanceBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
        memcpy(instanceBuffer.contents, instancesIndices.data(), instanceBufferLength);
        
#if !TARGET_OS_IPHONE
        [instanceBuffer didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
        return instanceBuffer;
    }
    
    id <MTLBuffer> createTransformationsBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
    {
#if !TARGET_OS_IPHONE
        MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
        MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
        
        unsigned instanceBufferLength = sizeof(matrix_float4x4) * pDesc->mInstancesDescCount;
        id <MTLBuffer> transformations = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
        uint8_t* ptr = static_cast<uint8_t*>(transformations.contents);
        for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
        {
            float* tr = &pDesc->pInstanceDescs[i].mTransform[0];
            vector_float4 v1 = {tr[0], tr[4], tr[8], 0.0};
            vector_float4 v2 = {tr[1], tr[5], tr[9], 0.0};
            vector_float4 v3 = {tr[2], tr[6], tr[10], 0.0};
            vector_float4 v4 = {tr[3], tr[7], tr[11], 1.0};
            matrix_float4x4 m = {v1,v2,v3,v4};
            memcpy(&ptr[i * sizeof(matrix_float4x4)], &m, sizeof(matrix_float4x4));
        }
        
#if !TARGET_OS_IPHONE
        [transformations didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
        return transformations;
    }
    
    uint32_t encodeInstanceMask(uint32_t mask, uint32_t hitGroupIndex, uint32_t instanceID)
    {
        ASSERT(mask < 0x100); //in DXR limited to 8 bits
        ASSERT(hitGroupIndex < 0x10); //in DXR limited to 16
        ASSERT(instanceID < 0x10000);
        
        return (instanceID << 16) | (hitGroupIndex << 8) | (mask);
    }
    
    id <MTLBuffer> createInstancesMaskBuffer(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
    {
#if !TARGET_OS_IPHONE
        MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
        MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
        
        unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
        id <MTLBuffer> ids = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
        uint32_t* ptr = static_cast<uint32_t*>(ids.contents);
        for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
        {
            ptr[i] = encodeInstanceMask(pDesc->pInstanceDescs[i].mInstanceMask,
                                        pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex,
                                        pDesc->pInstanceDescs[i].mInstanceID);
        }
#if !TARGET_OS_IPHONE
        [ids didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
        return ids;
    }
    
    id <MTLBuffer> createHitGroupIndicesBuffer (Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc)
    {
#if !TARGET_OS_IPHONE
        MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
        MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
        
        unsigned instanceBufferLength = sizeof(uint32_t) * pDesc->mInstancesDescCount;
        id <MTLBuffer> ids = [pRaytracing->pRenderer->pDevice newBufferWithLength:instanceBufferLength options:options];
        uint32_t* ptr = static_cast<uint32_t*>(ids.contents);
        for (unsigned i = 0; i < pDesc->mInstancesDescCount; ++i)
        {
            ptr[i] = pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex;
        }
#if !TARGET_OS_IPHONE
        [ids didModifyRange:NSMakeRange(0, instanceBufferLength)];
#endif
        return ids;
    }
    
    //////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////
    void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
    {
        ASSERT(pRaytracing);
        ASSERT(pRaytracing->pRenderer);
        ASSERT(pRaytracing->pIntersector);
        ASSERT(ppAccelerationStructure);
        
        AccelerationStructure* AS = (AccelerationStructure*)conf_malloc(sizeof(AccelerationStructure));
        memset(AS, 0, sizeof(*AS));
        conf_placement_new<tinystl::vector<uint32_t> >(&AS->mActiveHitGroups);
        AS->pBottomAS = [[NSMutableArray alloc] init];
        
        //pDesc->mFlags. Just ignore this
        id <MTLBuffer> _vertexPositionBuffer = nil;
        id <MTLBuffer> _indexBuffer = nil;
        id <MTLBuffer> _masks = nil;
        tinystl::vector<ASOffset> offsets;
        
        //copy vertices and indices to buffer
        createVertexAndIndexBuffers(pRaytracing, pDesc, &_vertexPositionBuffer, &_indexBuffer, &_masks, offsets);

        MPSAccelerationStructureGroup* group = [[MPSAccelerationStructureGroup alloc] initWithDevice:pRaytracing->pRenderer->pDevice];
        AS->pSharedGroup = group;
        MPSInstanceAccelerationStructure* as = [[MPSInstanceAccelerationStructure alloc] initWithGroup:group];
        AS->pInstanceAccel = as;
        AS->mIndexBuffer = _indexBuffer;
        AS->mVertexPositionBuffer = _vertexPositionBuffer;
        AS->mMasks = _masks;
        AS->mInstanceIDs = createInstanceIDBuffer(pRaytracing, pDesc);
        AS->mHitGroupIndices = createHitGroupIndicesBuffer(pRaytracing, pDesc);
        for (unsigned i = 0; i < pDesc->mBottomASDescsCount; ++i)
        {
            uint32_t hitID = pDesc->pInstanceDescs[i].mInstanceContributionToHitGroupIndex;
            //find for vector is O(n) but this is done once. It is faster for iteration (than iteration over set) which will happen often
            auto it = AS->mActiveHitGroups.find(hitID);
            if (it == AS->mActiveHitGroups.end())
                AS->mActiveHitGroups.push_back(hitID);
        }
        for (unsigned i = 0; i < pDesc->mBottomASDescsCount; ++i)
        {
            // Create an acceleration structure from our vertex position data
            MPSTriangleAccelerationStructure* _accelerationStructure = [[MPSTriangleAccelerationStructure alloc] initWithGroup:group];
            
            _accelerationStructure.vertexBuffer = _vertexPositionBuffer;
            _accelerationStructure.vertexBufferOffset = offsets[i].vbGeometriesOffsets[0];
            _accelerationStructure.vertexStride = offsets[i].vertexStride;
            
            _accelerationStructure.indexBuffer = _indexBuffer;
            _accelerationStructure.indexType = pDesc->mIndexType == INDEX_TYPE_UINT32 ? MPSDataTypeUInt32 : MPSDataTypeUInt16;
            _accelerationStructure.indexBufferOffset = offsets[i].ibGeometriesOffsets[0];
            
            _accelerationStructure.maskBuffer = _masks;
            _accelerationStructure.maskBufferOffset = offsets[i].masksGeometriesOffsets[0];

            _accelerationStructure.triangleCount = offsets[i].trianglesCount;
            
            [AS->pBottomAS addObject:_accelerationStructure];
        }
        AS->pInstanceAccel.accelerationStructures = AS->pBottomAS;
        AS->pInstanceAccel.instanceCount = pDesc->mInstancesDescCount;
        AS->pInstanceAccel.instanceBuffer = createInstancesIndicesBuffer(pRaytracing, pDesc);

        //generate buffer for instances transformations
        {
            AS->pInstanceAccel.transformBuffer = createTransformationsBuffer(pRaytracing, pDesc);
            AS->pInstanceAccel.transformType = MPSTransformTypeFloat4x4;
        }
        //generate instances ID buffer
        AS->pInstanceAccel.maskBuffer = createInstancesMaskBuffer(pRaytracing, pDesc);
        
        *ppAccelerationStructure = AS;
    }
    
    void cmdBuildTopAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
    {
        ASSERT(pRaytracing);
        ASSERT(pAccelerationStructure);
        ASSERT(pAccelerationStructure->pInstanceAccel);
        [pAccelerationStructure->pInstanceAccel rebuild];
    }
    
    void cmdBuildBottomAS(Cmd* pCmd, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure, unsigned bottomASIndex)
    {
        ASSERT(pRaytracing);
        ASSERT(pAccelerationStructure);
        ASSERT(bottomASIndex < pAccelerationStructure->pBottomAS.count);
        ASSERT(pAccelerationStructure->pBottomAS[bottomASIndex]);
        [pAccelerationStructure->pBottomAS[bottomASIndex] rebuild];
    }
    
    void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
    {
        for (unsigned i = 0; i < pDesc->mBottomASIndicesCount; ++i)
        {
            cmdBuildBottomAS(pCmd, pRaytracing, pDesc->pAccelerationStructure, i);
        }
        cmdBuildTopAS(pCmd, pRaytracing, pDesc->pAccelerationStructure);
    }
    
    void cmdCopyTexture(Cmd* pCmd, Texture* pDst, Texture* pSrc)
    {
        ASSERT(pDst->mDesc.mWidth == pSrc->mDesc.mWidth);
        ASSERT(pDst->mDesc.mHeight == pSrc->mDesc.mHeight);
        ASSERT(pDst->mDesc.mMipLevels == pSrc->mDesc.mMipLevels);
        ASSERT(pDst->mDesc.mArraySize == pSrc->mDesc.mArraySize);
        util_end_current_encoders(pCmd);
        
        pCmd->mtlBlitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
        
        uint nLayers = min(pSrc->mDesc.mArraySize, pDst->mDesc.mArraySize);
        uint nMips = min(pSrc->mDesc.mMipLevels, pDst->mDesc.mMipLevels);
        uint32_t width = min(pSrc->mDesc.mWidth, pDst->mDesc.mWidth);
        uint height = min(pSrc->mDesc.mHeight, pDst->mDesc.mHeight);
        for (uint32_t l = 0; l < nLayers; ++l)
        {
            for (uint32_t m = 0; m < nMips; ++m)
            {
                uint32_t mipmapWidth    = max(width >> m, 1u);
                uint32_t mipmapHeight   = max(height >> m, 1u);
                
                [pCmd->mtlBlitEncoder copyFromTexture:pSrc->mtlTexture sourceSlice:l sourceLevel:m sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(mipmapWidth, mipmapHeight, 1) toTexture:pDst->mtlTexture destinationSlice:l destinationLevel:m destinationOrigin:MTLOriginMake(0, 0, 0)];
            }
        }
    }
    
    void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing)
    {
        ASSERT(pRaytracing);
        pRaytracing->pIntersector = nil;
        pRaytracing->~Raytracing();
        memset(pRaytracing, 0, sizeof(*pRaytracing));
        conf_free(pRaytracing);
    }
    
    void addRaytracingPipeline(const RaytracingPipelineDesc* pDesc, Pipeline** ppPipeline)
    {
        ASSERT(pDesc);
        ASSERT(ppPipeline);
        
        Raytracing* pRaytracing = pDesc->pRaytracing;
        ASSERT(pRaytracing);
        
        Pipeline* pGenericPipeline = (Pipeline*)conf_calloc(1, sizeof(Pipeline));
        
        RaytracingPipeline* pPipeline = (RaytracingPipeline*)conf_calloc(1, sizeof(RaytracingPipeline));
        memset(pPipeline, 0, sizeof(*pPipeline));
        
        pGenericPipeline->pRaytracingPipeline = pPipeline;
        /******************************/
        // Create compute pipelines
        MTLComputePipelineDescriptor *computeDescriptor = [[MTLComputePipelineDescriptor alloc] init];
        // Set to YES to allow compiler to make certain optimizations
        computeDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;
        
        NSString* entryPointNStr = [[NSString alloc] initWithUTF8String:pDesc->pRayGenShader->mtlComputeShaderEntryPoint.c_str()];
        computeDescriptor.computeFunction = pDesc->pRayGenShader->mtlComputeShader;
        
        NSError *error = NULL;
        pPipeline->mRayPipeline = [pRaytracing->pRenderer->pDevice
                                newComputePipelineStateWithDescriptor:computeDescriptor
                                                              options:0
                                                           reflection:nil
                                                                error:&error];
        
        pPipeline->mHitPipelines = [[NSMutableArray alloc] init];
        pPipeline->mMissPipelines = [[NSMutableArray alloc] init];
        
#if !TARGET_OS_IPHONE
        MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
        MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
        pPipeline->mHitGroupNames = (char**)conf_calloc(pDesc->mHitGroupCount, sizeof(char*));
        for (uint32_t i = 0; i < pDesc->mHitGroupCount; ++i)
        {
            MTLComputePipelineDescriptor *computeDescriptor = [[MTLComputePipelineDescriptor alloc] init];
            // Set to YES to allow compiler to make certain optimizations
            computeDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;
            
            //Rustam: take care about "any hit" and "intersection" shaders
            entryPointNStr = [[NSString alloc] initWithUTF8String:pDesc->pHitGroups[i].pClosestHitShader->mtlComputeShaderEntryPoint.c_str()];
            
            computeDescriptor.computeFunction = pDesc->pHitGroups[i].pClosestHitShader->mtlComputeShader;
            
            id <MTLComputePipelineState> _pipeline = [pRaytracing->pRenderer->pDevice
                                                            newComputePipelineStateWithDescriptor:computeDescriptor
                                                            options: 0
                                                            reflection:nil
                                                            error:&error];
            if (!_pipeline)
            {
                LOGERRORF("Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
            }
            [pPipeline->mHitPipelines addObject: _pipeline];
            
            const char* groupName = pDesc->pHitGroups[i].pHitGroupName;
            pPipeline->mHitGroupNames[i] = (char*)conf_calloc(strlen(groupName) + 1, sizeof(char));
            memcpy(pPipeline->mHitGroupNames[i], groupName, strlen(groupName) + 1);
        }
        
        pPipeline->mMissGroupNames = (char**)conf_calloc(pDesc->mMissShaderCount, sizeof(char*));
        for (uint32_t i = 0; i < pDesc->mMissShaderCount; ++i)
        {
            MTLComputePipelineDescriptor *computeDescriptor = [[MTLComputePipelineDescriptor alloc] init];
            // Set to YES to allow compiler to make certain optimizations
            computeDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;
            
            //Rustam: take care about "any hit" and "intersection" shaders
            entryPointNStr = [[NSString alloc] initWithUTF8String:pDesc->ppMissShaders[i]->mtlComputeShaderEntryPoint.c_str()];
            
            computeDescriptor.computeFunction = pDesc->ppMissShaders[i]->mtlComputeShader;
            
            id <MTLComputePipelineState> _pipeline = [pRaytracing->pRenderer->pDevice
                                                      newComputePipelineStateWithDescriptor:computeDescriptor
                                                      options: 0
                                                      reflection:nil
                                                      error:&error];
            if (!_pipeline)
            {
                LOGERRORF("Failed to create compute pipeline state, error:\n%s", [[error localizedDescription] UTF8String]);
            }
            [pPipeline->mMissPipelines addObject: _pipeline];
            
            const char* groupName = pDesc->ppMissShaders[i]->mtlComputeShaderEntryPoint.c_str();
            pPipeline->mMissGroupNames[i] = (char*)conf_calloc(strlen(groupName) + 1, sizeof(char));
            memcpy(pPipeline->mMissGroupNames[i], groupName, strlen(groupName) + 1);
        }
        
        /******************************/
        // Create a raytracer for our Metal device
        pPipeline->mIntersector = [[MPSRayIntersector alloc] initWithDevice:pRaytracing->pRenderer->pDevice];
        
        pPipeline->mIntersector.rayDataType     = MPSRayDataTypeOriginMaskDirectionMaxDistance;
        pPipeline->mIntersector.rayStride       = sizeof(Ray);
        pPipeline->mIntersector.rayMaskOptions  = MPSRayMaskOptionInstance;
        
        //MPSIntersectionDistancePrimitiveIndexInstanceIndexCoordinates
        pPipeline->mIntersector.intersectionDataType = MPSIntersectionDataTypeDistancePrimitiveIndexInstanceIndexCoordinates;
        pPipeline->mMaxRaysCount = pDesc->mMaxRaysCount;
        pPipeline->mPayloadRecordSize = pDesc->mPayloadSize;
        
        //Create rays buffer for RayGen shader
        NSUInteger raysBufferSize = pPipeline->mIntersector.rayStride * pDesc->mMaxRaysCount;
        pPipeline->mRayGenRaysBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:raysBufferSize options:MTLResourceStorageModePrivate];
        
        NSUInteger payloadBufferSize = pDesc->mPayloadSize * pDesc->mMaxRaysCount;
        pPipeline->mPayloadBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:payloadBufferSize options:MTLResourceStorageModePrivate];
        
        //Create buffer for settings (width, height,...)
        static const size_t alignedUniformsSize = (sizeof(RaysDispatchUniformBuffer) + 255) & ~255;
        NSUInteger uniformBufferSize = alignedUniformsSize; //Rustam: do multiple sized buffer for "Ring-buffer" approach
        pPipeline->mSettingsBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:uniformBufferSize options:options];
        
        //Create intersections buffer for initial intersection test
        NSUInteger intersectionsBufferSize = sizeof(MPSIntersectionDistancePrimitiveIndexInstanceIndexCoordinates) * pDesc->mMaxRaysCount;
        pPipeline->mIntersectionBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:intersectionsBufferSize
                                                                                      options:MTLResourceStorageModePrivate];
        
#if !TARGET_OS_IPHONE
        [pPipeline->mSettingsBuffer didModifyRange:NSMakeRange(0, pPipeline->mSettingsBuffer.length)];
#endif
        
        pGenericPipeline->mType = PIPELINE_TYPE_RAYTRACING;
        *ppPipeline = pGenericPipeline;
    }

    void removeRaytracingPipeline(RaytracingPipeline* pPipeline)
    {
        ASSERT(pPipeline);
        
        for (uint32_t i = 0; i < pPipeline->mHitPipelines.count; ++i)
            conf_free(pPipeline->mHitGroupNames[i]);
        conf_free(pPipeline->mHitGroupNames);
        
        for (uint32_t i = 0; i < pPipeline->mMissPipelines.count; ++i)
            conf_free(pPipeline->mMissGroupNames[i]);
        conf_free(pPipeline->mMissGroupNames);
        
        pPipeline->mHitPipelines = nil;
        pPipeline->mIntersectionBuffer = nil;
        pPipeline->mMissPipelines = nil;
        pPipeline->mPayloadBuffer = nil;
        pPipeline->mRayGenRaysBuffer = nil;
        pPipeline->mRayPipeline = nil;
        pPipeline->mSettingsBuffer = nil;
        
        pPipeline->~RaytracingPipeline();
        memset(pPipeline, 0, sizeof(*pPipeline));
        
        conf_free(pPipeline);
    }
    
    void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
    {
        pAccelerationStructure->mHitGroupIndices = nil;
        pAccelerationStructure->mIndexBuffer = nil;
        pAccelerationStructure->mInstanceIDs = nil;
        pAccelerationStructure->mMasks = nil;
        pAccelerationStructure->mVertexPositionBuffer = nil;
        pAccelerationStructure->pBottomAS = nil;
        pAccelerationStructure->pInstanceAccel = nil;
        pAccelerationStructure->pSharedGroup = nil;
        
        pAccelerationStructure->~AccelerationStructure();
        conf_free(pAccelerationStructure);
    }
    
    void setupShadersInfo(Raytracing* pRaytracing,
                          RaytracingShaderInfoSet* shadersInfo,
                          uint32_t groupsCount, RaytracingPipeline* pPipeline,
                          RaytracingShaderTableRecordDesc* pHitGroups,
                          NSMutableArray<id <MTLComputePipelineState> >*  groupPipelines,
                          char** groupNames)
    {
#if !TARGET_OS_IPHONE
        MTLResourceOptions options = MTLResourceStorageModeManaged;
#else
        MTLResourceOptions options = MTLResourceStorageModeShared;
#endif
        shadersInfo->mHitPipelines = [[NSMutableArray alloc] init];
        
        uint32_t settingsBufferSize = sizeof(HitShaderSettings) * groupsCount;
        shadersInfo->mHitSettings = [pRaytracing->pRenderer->pDevice newBufferWithLength:settingsBufferSize options:options];
        HitShaderSettings* shaderSettingsPtr = (HitShaderSettings*)shadersInfo->mHitSettings.contents;
        
        for (uint32_t i = 0; i < groupsCount; ++i)
        {
            const char *name = pHitGroups[i].pName;
            id<MTLComputePipelineState> pipeline = nil;
            for (uint32_t j = 0; j < groupPipelines.count; ++j)
            {
                const char* nameFromPipeline = groupNames[j];
                if (strcmp(name, nameFromPipeline) == 0)
                {
                    pipeline = groupPipelines[j];
                    break;
                }
            }
            
            [shadersInfo->mHitPipelines addObject:pipeline];
            shaderSettingsPtr[i].hitGroupID = i;
        }
        
#if !TARGET_OS_IPHONE
        [shadersInfo->mHitSettings didModifyRange:NSMakeRange(0, shadersInfo->mHitSettings.length)];
#endif
        /***************************************************************/
        /*Check what shaders generate secondary rays*/
        /***************************************************************/
        shadersInfo->pHitReferences = (ShaderReference*)conf_calloc(groupsCount, sizeof(ShaderReference));
        shadersInfo->mHitGroupsRaysBuffers = [[NSMutableArray alloc] init];
        shadersInfo->mIntersectionBuffer = [[NSMutableArray alloc] init];
        shadersInfo->mPayloadBuffer = [[NSMutableArray alloc] init];
        NSUInteger raysBufferSize = pPipeline->mIntersector.rayStride * pPipeline->mMaxRaysCount;
        NSUInteger intersectionsBufferSize = sizeof(MPSIntersectionDistancePrimitiveIndexInstanceIndexCoordinates) * pPipeline->mMaxRaysCount;
        NSUInteger payloadBufferSize = pPipeline->mPayloadRecordSize * pPipeline->mMaxRaysCount;
        for (uint32_t i = 0; i < groupsCount; ++i)
        {
            shadersInfo->pHitReferences[i].active = pHitGroups[i].mInvokeTraceRay;
            id <MTLBuffer> raysBuffer = nil;
            id <MTLBuffer> intersectionsBuffer = nil;
            id <MTLBuffer> payloadBuffer = nil;
            if (pHitGroups[i].mInvokeTraceRay)
            {
                raysBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:raysBufferSize options:MTLResourceStorageModePrivate];
                intersectionsBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:intersectionsBufferSize
                                                                                   options:MTLResourceStorageModePrivate];
                payloadBuffer = [pRaytracing->pRenderer->pDevice newBufferWithLength:payloadBufferSize options:MTLResourceStorageModePrivate];
                
                shadersInfo->pHitReferences[i].hitShader = pHitGroups[i].mHitShaderIndex;
                shadersInfo->pHitReferences[i].missShader = pHitGroups[i].mMissShaderIndex;
            }
            
            if (raysBuffer == nil)
                [shadersInfo->mHitGroupsRaysBuffers addObject: [NSNull null] ];
            else
                [shadersInfo->mHitGroupsRaysBuffers addObject: raysBuffer ];
            
            if (intersectionsBuffer == nil)
                [shadersInfo->mIntersectionBuffer addObject: [NSNull null] ];
            else
                [shadersInfo->mIntersectionBuffer addObject: intersectionsBuffer ];
            
            if (payloadBuffer == nil)
                [shadersInfo->mPayloadBuffer addObject: [NSNull null]];
            else
                [shadersInfo->mPayloadBuffer addObject: payloadBuffer];
        }
    }
    
    void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
    {
        ASSERT(pRaytracing);
        ASSERT(pDesc);
        ASSERT(ppTable);
        
        RaytracingShaderTable* table = (RaytracingShaderTable*)conf_calloc(1, sizeof(RaytracingShaderTable));
        memset(table, 0, sizeof(RaytracingShaderTable));

        table->mRayGenHitRef = pDesc->pRayGenShader->mHitShaderIndex;
        table->mRayGenMissRef = pDesc->pRayGenShader->mMissShaderIndex;
        table->mInvokeShaders = pDesc->pRayGenShader->mInvokeTraceRay;
		table->pDescriptorBinder = pDesc->pDescriptorBinder;

        /***************************************************************/
        /*Setup shaders settings*/
        /***************************************************************/
        RaytracingPipeline* pPipeline = pDesc->pPipeline->pRaytracingPipeline;
        setupShadersInfo(pRaytracing, &table->mHitShadersInfo, pDesc->mHitGroupCount, pPipeline, pDesc->pHitGroups,
                         pPipeline->mHitPipelines, pPipeline->mHitGroupNames);
        setupShadersInfo(pRaytracing, &table->mMissShadersInfo, pDesc->mMissShaderCount, pPipeline, pDesc->pMissShaders,
                         pPipeline->mMissPipelines, pPipeline->mMissGroupNames);
        *ppTable = table;
    }
    
    void removeShaderInfoSet(RaytracingShaderInfoSet* infoSet)
    {
        infoSet->mHitPipelines = nil;
        infoSet->mHitSettings = nil;
        infoSet->mHitGroupsRaysBuffers = nil;
        infoSet->mIntersectionBuffer = nil;
        infoSet->mPayloadBuffer = nil;
        
        if (infoSet->pHitReferences != NULL)
        {
            conf_free(infoSet->pHitReferences);
        }
        infoSet->~RaytracingShaderInfoSet();
    }
    
    void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable)
    {
        ASSERT(pTable);

        removeShaderInfoSet(&pTable->mHitShadersInfo);
        removeShaderInfoSet(&pTable->mMissShadersInfo);
        pTable->~RaytracingShaderTable();
        memset(pTable, 0, sizeof(*pTable));
        conf_free(pTable);
    }
    
    void addRaytracingRootSignature(Renderer* pRenderer, const ShaderResource* pResources, uint32_t resourceCount,
 bool local, RootSignature** ppRootSignature, const RootSignatureDesc* pRootDesc = NULL)
    {
        ASSERT(pRenderer);
        ASSERT(pRenderer->pDevice);
        
        RootSignature* pRootSignature = (RootSignature*)conf_calloc(1, sizeof(*pRootSignature));
        memset(pRootSignature, 0, sizeof(*pRootSignature));
        pRootSignature->mPipelineType = PIPELINE_TYPE_COMPUTE;
        
        // Collect static samplers
        tinystl::vector <tinystl::pair<ShaderResource const*, Sampler*> > staticSamplers;
        tinystl::unordered_map<tinystl::string, Sampler*> staticSamplerMap;
        if (pRootDesc != NULL)
        {
            for (uint32_t i = 0; i < pRootDesc->mStaticSamplerCount; ++i)
                staticSamplerMap.insert({ pRootDesc->ppStaticSamplerNames[i], pRootDesc->ppStaticSamplers[i] });
        }
        
        conf_placement_new<tinystl::unordered_map<uint32_t, uint32_t>>(&pRootSignature->pDescriptorNameToIndexMap);
        
        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            ShaderResource const* pRes = &pResources[i];
            
            // Find all unique resources
            tinystl::unordered_hash_node<uint32_t, uint32_t>* pNode = pRootSignature->pDescriptorNameToIndexMap.find(tinystl::hash(pRes->name)).node;
            if (!pNode)
            {
                if (pRes->type == DESCRIPTOR_TYPE_SAMPLER)
                {
                    // If the sampler is a static sampler, no need to put it in the descriptor table
                    const tinystl::unordered_hash_node<tinystl::string, Sampler*>* pNode = staticSamplerMap.find(pRes->name).node;
                    
                    if (pNode)
                    {
                        LOGINFOF("Descriptor (%s) : User specified Static Sampler", pRes->name);
                        staticSamplers.push_back({ pRes, pNode->second });
                    }
                    else
                    {
                        pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), i });
                    }
                }
                else
                {
                    pRootSignature->pDescriptorNameToIndexMap.insert({ tinystl::hash(pRes->name), i });
                }
            } else
            {
                ASSERT(false && "Provided resources should be unique");
            }
        }
        
        if (resourceCount > 0)
        {
            pRootSignature->mDescriptorCount = resourceCount;
            pRootSignature->pDescriptors = (DescriptorInfo*)conf_calloc(pRootSignature->mDescriptorCount, sizeof(DescriptorInfo));
        }
        
        // Fill the descriptor array to be stored in the root signature
        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            DescriptorInfo* pDesc = &pRootSignature->pDescriptors[i];
            ShaderResource const* pRes = &pResources[i];
            uint32_t setIndex = pRes->set;
            DescriptorUpdateFrequency updateFreq = (DescriptorUpdateFrequency)setIndex;
            
            pDesc->mDesc.reg = pRes->reg;
            pDesc->mDesc.set = pRes->set;
            pDesc->mDesc.size = pRes->size;
            pDesc->mDesc.type = pRes->type;
            pDesc->mDesc.used_stages = pRes->used_stages;
            pDesc->mDesc.name_size = pRes->name_size;
            pDesc->mDesc.name = (const char*)conf_calloc(pDesc->mDesc.name_size + 1, sizeof(char));
            memcpy((char*)pDesc->mDesc.name, pRes->name, pRes->name_size);
            pDesc->mUpdateFrquency = updateFreq;
            
            // In case we're binding a texture, we need to specify the texture type so the bound resource type matches the one defined in the shader.
            if(pRes->type == DESCRIPTOR_TYPE_TEXTURE || pRes->type == DESCRIPTOR_TYPE_RW_TEXTURE)
            {
                pDesc->mDesc.mtlTextureType = pRes->mtlTextureType;
            }
            
            // If we're binding an argument buffer, we also need to get the type of the resources that this buffer will store.
            if(pRes->mtlArgumentBufferType != DESCRIPTOR_TYPE_UNDEFINED)
            {
                pDesc->mDesc.mtlArgumentBufferType = pRes->mtlArgumentBufferType;
            }
        }
        
        pRootSignature->mStaticSamplerCount = (uint32_t)staticSamplers.size();
        pRootSignature->ppStaticSamplers = (Sampler**)conf_calloc(staticSamplers.size(), sizeof(Sampler*));
        pRootSignature->pStaticSamplerStages = (ShaderStage*)conf_calloc(staticSamplers.size(), sizeof(ShaderStage));
        pRootSignature->pStaticSamplerSlots = (uint32_t*)conf_calloc(staticSamplers.size(), sizeof(uint32_t));
        for (uint32_t i = 0; i < pRootSignature->mStaticSamplerCount; ++i)
        {
            pRootSignature->ppStaticSamplers[i] = staticSamplers[i].second;
            pRootSignature->pStaticSamplerStages[i] = staticSamplers[i].first->used_stages;
            pRootSignature->pStaticSamplerSlots[i] = staticSamplers[i].first->reg;
        }
        
        *ppRootSignature = pRootSignature;
    }
    
    extern void cmdBindLocalDescriptors(Cmd* pCmd, DescriptorBinder* pDescriptorBinder, RootSignature* pRootSignature, uint32_t numDescriptors, DescriptorData* pDescParams);
    
    void dispatch(id <MTLComputeCommandEncoder> computeEncoder,
                  id <MTLBuffer> raysBuffer,
                  id <MTLBuffer> globalSettingsBuffer,
                  id <MTLBuffer> intersectionsBuffer,
                  id <MTLBuffer> indexBuffer,
                  id <MTLBuffer> instancesIDsBuffer,
                  id <MTLBuffer> payloadBuffer,
                  id <MTLBuffer> masksBuffer,
                  id <MTLBuffer> hitGroupIndices,
                  id <MTLBuffer> shaderSettingsBuffer,
                  uint32_t shaderSettingsOffset,
                  id <MTLComputePipelineState> pipeline,
                  MTLSize threadgroups,
                  MTLSize threadsPerThreadgroup)
    {
        //pHitGroups[0].pRootSignature
        [computeEncoder setBuffer:raysBuffer                offset:0              atIndex:0];
        [computeEncoder setBuffer:globalSettingsBuffer      offset:0              atIndex:1];
        [computeEncoder setBuffer:intersectionsBuffer       offset:0              atIndex:2];
        [computeEncoder setBuffer:indexBuffer               offset:0              atIndex:3];
        [computeEncoder setBuffer:instancesIDsBuffer        offset:0              atIndex:4];
        [computeEncoder setBuffer:payloadBuffer             offset:0              atIndex:5];
        [computeEncoder setBuffer:masksBuffer               offset:0              atIndex:6];
        [computeEncoder setBuffer:hitGroupIndices           offset:0              atIndex:7];
        [computeEncoder setBuffer:shaderSettingsBuffer      offset:shaderSettingsOffset       atIndex:8];
        [computeEncoder setComputePipelineState:pipeline];
        [computeEncoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerThreadgroup];
    }
    
    void invokeShader(Cmd* pCmd, Raytracing* pRaytracing,
                      const RaytracingDispatchDesc* pDesc,
                      RaytracingShaderInfoSet* pShadersInfo,
                      uint32_t shaderId,
                      id <MTLBuffer> raysBuffer,
                      id <MTLBuffer> globalSettingsBuffer,
                      id <MTLBuffer> intersectionsBuffer,
                      id <MTLBuffer> indexBuffer,
                      id <MTLBuffer> instancesIDsBuffer,
                      id <MTLBuffer> payloadBuffer,
                      id <MTLBuffer> masksBuffer,
                      id <MTLBuffer> hitGroupIndices,
                      MTLSize threadgroups,
                      MTLSize threadsPerThreadgroup)
    {
        RaytracingShaderTable* pShaderTable = pDesc->pShaderTable;
        id <MTLBuffer> raysBufferLocal = raysBuffer;
        id <MTLBuffer> payloadBufferLocal = payloadBuffer;
        if (pShadersInfo->pHitReferences[shaderId].active)
        {
            //If shader generates secondary rays then we use copy of rays buffer because it will be overwritten
            id <MTLBlitCommandEncoder> blitEncoder = [pCmd->mtlCommandBuffer blitCommandEncoder];
            raysBufferLocal = pShadersInfo->mHitGroupsRaysBuffers[shaderId];
            payloadBufferLocal = pShadersInfo->mPayloadBuffer[shaderId];
            
            [blitEncoder copyFromBuffer:raysBuffer sourceOffset:0
                               toBuffer:raysBufferLocal destinationOffset:0
                                   size:raysBuffer.length];
            [blitEncoder copyFromBuffer:payloadBuffer sourceOffset:0
                               toBuffer:payloadBufferLocal destinationOffset:0
                                   size:payloadBuffer.length];
            
            [blitEncoder endEncoding];
        }
        
        //Bind "Global Root Signature" again
        cmdBindDescriptors(pCmd, pDesc->pShaderTable->pDescriptorBinder, pDesc->pRootSignature, pDesc->mRootSignatureDescriptorsCount, pDesc->pRootSignatureDescriptorData);
        id <MTLComputeCommandEncoder> computeEncoder = pCmd->mtlComputeEncoder;
        dispatch(computeEncoder, raysBufferLocal, globalSettingsBuffer, intersectionsBuffer,
                 indexBuffer, instancesIDsBuffer, payloadBuffer, masksBuffer, hitGroupIndices,
                 pShadersInfo->mHitSettings, shaderId * sizeof(HitShaderSettings),
                 pShadersInfo->mHitPipelines[shaderId], threadgroups, threadsPerThreadgroup);
        
        if (pShadersInfo->pHitReferences[shaderId].active)
        {
            util_end_current_encoders(pCmd);
            uint32_t hitRef = pShadersInfo->pHitReferences[shaderId].hitShader;
            uint32_t missRef = pShadersInfo->pHitReferences[shaderId].missShader;
            NSUInteger width = (NSUInteger)pDesc->mWidth;
            NSUInteger height = (NSUInteger)pDesc->mHeight;
            
            id <MTLBuffer> intersectionBufferLocal = pShadersInfo->mIntersectionBuffer[shaderId];
            RaytracingPipeline* pPipeline = pDesc->pPipeline->pRaytracingPipeline;
            [pPipeline->mIntersector encodeIntersectionToCommandBuffer:pCmd->mtlCommandBuffer      // Command buffer to encode into
                                             intersectionType:MPSIntersectionTypeNearest  // Intersection test type //Rustam: Get this from RayFlags from shader
                                                    rayBuffer:raysBufferLocal   // Ray buffer
                                              rayBufferOffset:0                           // Offset into ray buffer
                                           intersectionBuffer:intersectionBufferLocal // Intersection buffer (destination)
                                     intersectionBufferOffset:0                           // Offset into intersection buffer
                                                     rayCount:width * height              // Number of rays
                                        accelerationStructure:pDesc->pTopLevelAccelerationStructure->pInstanceAccel];    // Acceleration structure
            
            //invoke hit shader
            invokeShader(pCmd, pRaytracing, pDesc, &pShaderTable->mHitShadersInfo,
                         hitRef, raysBufferLocal, globalSettingsBuffer,
                         intersectionBufferLocal, indexBuffer, instancesIDsBuffer, payloadBufferLocal,
                         masksBuffer, hitGroupIndices,
                         threadgroups, threadsPerThreadgroup);
            
            //invoke miss shader
            invokeShader(pCmd, pRaytracing, pDesc, &pShaderTable->mMissShadersInfo,
                         missRef, raysBufferLocal, globalSettingsBuffer,
                         intersectionBufferLocal, indexBuffer, instancesIDsBuffer, payloadBufferLocal,
                         masksBuffer, hitGroupIndices,
                         threadgroups, threadsPerThreadgroup);
        }
    }

    void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)
    {
        NSUInteger width = (NSUInteger)pDesc->mWidth;
        NSUInteger height = (NSUInteger)pDesc->mHeight;
        
        RaytracingPipeline* pRaytracingPipeline = pDesc->pPipeline->pRaytracingPipeline;
        /*setup settings values in buffer*/
        //Rustam: implement ring-buffer
        RaysDispatchUniformBuffer *uniforms = (RaysDispatchUniformBuffer *)pRaytracingPipeline->mSettingsBuffer.contents;
        uniforms->width = (unsigned int)width;
        uniforms->height = (unsigned int)height;
        uniforms->blocksWide = (unsigned int)(width + 15) / 16;
        
#if !TARGET_OS_IPHONE
        [pRaytracingPipeline->mSettingsBuffer didModifyRange:NSMakeRange(0, pRaytracingPipeline->mSettingsBuffer.length)];
#endif
        
        // We will launch a rectangular grid of threads on the GPU to generate the rays. Threads are launched in
        // groups called "threadgroups". We need to align the number of threads to be a multiple of the threadgroup
        // size. We indicated when compiling the pipeline that the threadgroup size would be a multiple of the thread
        // execution width (SIMD group size) which is typically 32 or 64 so 8x8 is a safe threadgroup size which
        // should be small to be supported on most devices. A more advanced application would choose the threadgroup
        // size dynamically.
        MTLSize threadsPerThreadgroup = MTLSizeMake(8, 8, 1);
        MTLSize threadgroups = MTLSizeMake((width  + threadsPerThreadgroup.width  - 1) / threadsPerThreadgroup.width,
                                           (height + threadsPerThreadgroup.height - 1) / threadsPerThreadgroup.height,
                                           1);
        
        // First, we will generate rays on the GPU. We create a compute command encoder which will be used to add
        // commands to the command buffer.
        //In terms of DXR here we bind "Global Root Signature"
        cmdBindDescriptors(pCmd, pDesc->pShaderTable->pDescriptorBinder, pDesc->pRootSignature, pDesc->mRootSignatureDescriptorsCount, pDesc->pRootSignatureDescriptorData);
        id <MTLComputeCommandEncoder> computeEncoder = pCmd->mtlComputeEncoder;
        /*********************************************************************************/
        //Now we work with initial RayGen shader
        /*********************************************************************************/
        // Bind buffers needed by the compute pipeline
        [computeEncoder setBuffer:pRaytracingPipeline->mRayGenRaysBuffer       offset:0                    atIndex:0];
        [computeEncoder setBuffer:pRaytracingPipeline->mSettingsBuffer         offset:0                    atIndex:1];
        // Bind the ray generation compute pipeline
        [computeEncoder setComputePipelineState:pRaytracingPipeline->mRayPipeline];
        // Launch threads
        [computeEncoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerThreadgroup];
        
        // End the encoder
        util_end_current_encoders(pCmd);
        
        if (!pDesc->pShaderTable->mInvokeShaders) return;
        
        // We can then pass the rays to the MPSRayIntersector to compute the intersections with our acceleration structure
        //_intersector.rayMaskOptions = //Rustam: get this from RayMask from shader
        [pRaytracingPipeline->mIntersector encodeIntersectionToCommandBuffer:pCmd->mtlCommandBuffer      // Command buffer to encode into
                                       intersectionType:MPSIntersectionTypeNearest  // Intersection test type //Rustam: Get this from RayFlags from shader
                                              rayBuffer:pRaytracingPipeline->mRayGenRaysBuffer   // Ray buffer
                                        rayBufferOffset:0                           // Offset into ray buffer
                                     intersectionBuffer:pRaytracingPipeline->mIntersectionBuffer // Intersection buffer (destination)
                               intersectionBufferOffset:0                           // Offset into intersection buffer
                                               rayCount:width * height              // Number of rays
                                  accelerationStructure:pDesc->pTopLevelAccelerationStructure->pInstanceAccel];    // Acceleration structure
        
        /*********************************************************************************/
        //Now we can execute Hit/Miss shader
        /*********************************************************************************/
        //Execute initial hit shaders which process rays from  raygen shader
        for (unsigned int i = 0; i < pDesc->pTopLevelAccelerationStructure->mActiveHitGroups.size(); ++i)
        {
            uint32_t hitId = pDesc->pTopLevelAccelerationStructure->mActiveHitGroups[i];
            /*If this hit shader emits secondary rays*/
            invokeShader(pCmd, pRaytracing, pDesc,
                         &pDesc->pShaderTable->mHitShadersInfo, hitId,
                         pRaytracingPipeline->mRayGenRaysBuffer,
                         pRaytracingPipeline->mSettingsBuffer,
                         pRaytracingPipeline->mIntersectionBuffer,
                         pDesc->pTopLevelAccelerationStructure->mIndexBuffer,
                         pDesc->pTopLevelAccelerationStructure->mInstanceIDs,
                         pRaytracingPipeline->mPayloadBuffer,
                         pDesc->pTopLevelAccelerationStructure->mMasks,
                         pDesc->pTopLevelAccelerationStructure->mHitGroupIndices,
                         threadgroups, threadsPerThreadgroup);

        }
        //Execute miss shader
        uint32_t missId = pDesc->pShaderTable->mRayGenMissRef;
        invokeShader(pCmd, pRaytracing, pDesc,
                     &pDesc->pShaderTable->mMissShadersInfo, missId,
                     pRaytracingPipeline->mRayGenRaysBuffer,
                     pRaytracingPipeline->mSettingsBuffer,
                     pRaytracingPipeline->mIntersectionBuffer,
                     pDesc->pTopLevelAccelerationStructure->mIndexBuffer,
                     pDesc->pTopLevelAccelerationStructure->mInstanceIDs,
                     pRaytracingPipeline->mPayloadBuffer,
                     pDesc->pTopLevelAccelerationStructure->mMasks,
                     pDesc->pTopLevelAccelerationStructure->mHitGroupIndices,
                     threadgroups, threadsPerThreadgroup);
    }

#endif

