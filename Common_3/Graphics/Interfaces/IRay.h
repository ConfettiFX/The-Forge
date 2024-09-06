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

#pragma once

#include "IGraphics.h"

#ifdef METAL
#import <MetalKit/MetalKit.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#endif

typedef struct Renderer                        Renderer;
typedef struct Raytracing                      Raytracing;
typedef struct Buffer                          Buffer;
typedef struct Texture                         Texture;
typedef struct Cmd                             Cmd;
typedef struct AccelerationStructure           AccelerationStructure;
typedef struct AccelerationStructureDescBottom AccelerationStructureDescBottom;
typedef struct RootSignature                   RootSignature;
typedef struct ShaderResource                  ShaderResource;
typedef struct DescriptorData                  DescriptorData;
typedef struct ID3D12Device5                   ID3D12Device5;
typedef struct SSVGFDenoiser                   SSVGFDenoiser;

typedef enum AccelerationStructureType
{
    ACCELERATION_STRUCTURE_TYPE_BOTTOM = 0,
    ACCELERATION_STRUCTURE_TYPE_TOP,
} AccelerationStructureType;

// Supported by DXR. Metal ignores this.
typedef enum AccelerationStructureBuildFlags
{
    ACCELERATION_STRUCTURE_BUILD_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE = 0x1,
    ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION = 0x2,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE = 0x4,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD = 0x8,
    ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY = 0x10,
    ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE = 0x20,
} AccelerationStructureBuildFlags;
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureBuildFlags)

// Rustam: check if this can be mapped to Metal
typedef enum AccelerationStructureGeometryFlags
{
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE = 0x1,
    ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION = 0x2
} AccelerationStructureGeometryFlags;
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureGeometryFlags)

// Rustam: check if this can be mapped to Metal
typedef enum AccelerationStructureInstanceFlags
{
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE = 0,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE = 0x1,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE = 0x2,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE = 0x4,
    ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_NON_OPAQUE = 0x8
} AccelerationStructureInstanceFlags;
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureInstanceFlags)

typedef struct AccelerationStructureInstanceDesc
{
    AccelerationStructure*             pBottomAS;
    /// Row major affine transform for transforming the vertices in the geometry stored in pAccelerationStructure
    float                              mTransform[12];
    /// User defined instanced ID which can be queried in the shader
    uint32_t                           mInstanceID;
    uint32_t                           mInstanceMask;
    uint32_t                           mInstanceContributionToHitGroupIndex;
    AccelerationStructureInstanceFlags mFlags;
} AccelerationStructureInstanceDesc;

typedef struct AccelerationStructureGeometryDesc
{
    Buffer*                            pVertexBuffer;
    Buffer*                            pIndexBuffer;
    uint32_t                           mVertexOffset;
    uint32_t                           mVertexCount;
    uint32_t                           mVertexStride;
    TinyImageFormat                    mVertexFormat;
    uint32_t                           mIndexOffset;
    uint32_t                           mIndexCount;
    IndexType                          mIndexType;
    AccelerationStructureGeometryFlags mFlags;
} AccelerationStructureGeometryDesc;
/************************************************************************/
//	  Bottom Level Structures define the geometry data such as vertex buffers, index buffers
//	  Top Level Structures define the instance data for the geometry such as instance matrix, instance ID, ...
// #mDescCount - Number of geometries or instances in this structure
/************************************************************************/
typedef struct AccelerationStructureDescBottom
{
    /// Number of geometries / instances in thie acceleration structure
    uint32_t                           mDescCount;
    /// Array of geometries in the bottom level acceleration structure
    AccelerationStructureGeometryDesc* pGeometryDescs;
} AccelerationStructureDescBottom;

typedef struct AccelerationStructureDescTop
{
    uint32_t                           mDescCount;
    AccelerationStructureInstanceDesc* pInstanceDescs;
} AccelerationStructureDescTop;

typedef struct AccelerationStructureDesc
{
    AccelerationStructureType       mType;
    AccelerationStructureBuildFlags mFlags;
    union
    {
        AccelerationStructureDescBottom mBottom;
        AccelerationStructureDescTop    mTop;
    };
} AccelerationStructureDesc;

typedef struct RaytracingBuildASDesc
{
    AccelerationStructure* pAccelerationStructure;
    bool                   mIssueRWBarrier;
} RaytracingBuildASDesc;

bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing);
void exitRaytracing(Renderer* pRenderer, Raytracing* pRaytracing);

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDesc* pDesc,
                              AccelerationStructure** ppAccelerationStructure);
void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);
/// Free the scratch memory allocated by acceleration structure after it has been built completely
/// Does not free acceleration structure
void removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc);

#ifdef METAL
void addSSVGFDenoiser(Renderer* pRenderer, SSVGFDenoiser** ppDenoiser);
void removeSSVGFDenoiser(SSVGFDenoiser* pDenoiser);
void clearSSVGFDenoiserTemporalHistory(SSVGFDenoiser* pDenoiser);
void cmdSSVGFDenoise(Cmd* pCmd, SSVGFDenoiser* pDenoiser, Texture* pSourceTexture, Texture* pMotionVectorTexture,
                     Texture* pDepthNormalTexture, Texture* pPreviousDepthNormalTexture, Texture** ppOut);
#endif
