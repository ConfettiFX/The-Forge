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

#pragma once

#include "IGraphics.h"

#ifdef METAL
#import <MetalKit/MetalKit.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#endif

typedef struct Renderer Renderer;
typedef struct Buffer Buffer;
typedef struct Texture Texture;
typedef struct Cmd Cmd;
typedef struct AccelerationStructure AccelerationStructure;
typedef struct AccelerationStructureDescBottom AccelerationStructureDescBottom;
typedef struct RaytracingPipeline RaytracingPipeline;
typedef struct RaytracingShaderTable RaytracingShaderTable;
typedef struct RootSignature RootSignature;
typedef struct RootSignatureDesc RootSignatureDesc;
typedef struct ShaderResource ShaderResource;
typedef struct DescriptorData DescriptorData;
typedef struct ID3D12Device5 ID3D12Device5;
typedef struct ParallelPrimitives ParallelPrimitives;
typedef struct SSVGFDenoiser SSVGFDenoiser;

//Supported by DXR. Metal ignores this.
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

//Rustam: check if this can be mapped to Metal
typedef enum AccelerationStructureGeometryFlags
{
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE = 0,
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE = 0x1,
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION = 0x2
} AccelerationStructureGeometryFlags;
MAKE_ENUM_FLAG(uint32_t, AccelerationStructureGeometryFlags)

//Rustam: check if this can be mapped to Metal
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
	uint32_t                mAccelerationStructureIndex;
	/// Row major affine transform for transforming the vertices in the geometry stored in pAccelerationStructure
	float                   mTransform[12];
	/// User defined instanced ID which can be queried in the shader
	uint32_t                mInstanceID;
	uint32_t                mInstanceMask;
	uint32_t                mInstanceContributionToHitGroupIndex;
	AccelerationStructureInstanceFlags  mFlags;
} AccelerationStructureInstanceDesc;

typedef struct AccelerationStructureGeometryDesc
{
	void*                               pVertexArray;
    union
	{
        uint32_t*                       pIndices32;
        uint16_t*                       pIndices16;
    };
	AccelerationStructureGeometryFlags  mFlags;
	uint32_t                            mVertexCount;
	uint32_t                            mIndexCount;
	IndexType                           mIndexType;
} AccelerationStructureGeometryDesc;
/************************************************************************/
//	  Bottom Level Structures define the geometry data such as vertex buffers, index buffers
//	  Top Level Structures define the instance data for the geometry such as instance matrix, instance ID, ...
// #mDescCount - Number of geometries or instances in this structure
/************************************************************************/
typedef struct AccelerationStructureDescBottom
{
	AccelerationStructureBuildFlags         mFlags;
	/// Number of geometries / instances in thie acceleration structure
	uint32_t                                mDescCount;
    /// Array of geometries in the bottom level acceleration structure
    AccelerationStructureGeometryDesc*      pGeometryDescs;
} AccelerationStructureDescBottom;

typedef struct AccelerationStructureDescTop
{
    AccelerationStructureBuildFlags         mFlags;
	uint32_t                                mInstancesDescCount;
    AccelerationStructureInstanceDesc*      pInstanceDescs;
    AccelerationStructureDescBottom*        mBottomASDesc;
    IndexType                               mIndexType;
} AccelerationStructureDescTop;

typedef struct RaytracingShaderTableDesc
{
	Pipeline*                           pPipeline;
	RootSignature*                      pGlobalRootSignature;
	const char*                         pRayGenShader;
	const char**                        pMissShaders;
	const char**                        pHitGroups;
	uint32_t                            mMissShaderCount;
	uint32_t                            mHitGroupCount;
} RaytracingShaderTableDesc;

typedef struct RaytracingDispatchDesc
{
	uint32_t                mWidth;
	uint32_t                mHeight;
	RaytracingShaderTable*  pShaderTable;
#if defined(METAL)
	AccelerationStructure*  pTopLevelAccelerationStructure;
    DescriptorSet*          pSets[DESCRIPTOR_UPDATE_FREQ_COUNT];
    uint32_t                pIndexes[DESCRIPTOR_UPDATE_FREQ_COUNT];
#endif
} RaytracingDispatchDesc;

typedef struct RaytracingBuildASDesc
{
	AccelerationStructure** ppAccelerationStructures;
	uint32_t                mCount;
	uint32_t                mBottomASIndicesCount;
	uint32_t*               pBottomASIndices;
} RaytracingBuildASDesc;

struct Raytracing
{
	Renderer*                    pRenderer;

#ifdef DIRECT3D12
	ID3D12Device5*               pDxrDevice;
	uint64_t                     mDescriptorsAllocated;
#endif
#ifdef METAL
    MPSRayIntersector*           pIntersector API_AVAILABLE(macos(10.14), ios(12.0));
	
	ParallelPrimitives*          pParallelPrimitives;
	id <MTLComputePipelineState> mClassificationPipeline;
	id <MTLArgumentEncoder>      mClassificationArgumentEncoder API_AVAILABLE(macos(10.13), ios(11.0));
#endif
#ifdef VULKAN
#if defined(VK_KHR_RAY_TRACING_PIPELINE_SPEC_VERSION) && defined(VK_KHR_ACCELERATION_STRUCTURE_SPEC_VERSION)
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR  mRayTracingPipelineProperties;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR mAccelerationStructureFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures mEnabledBufferDeviceAddressFeatures;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR mEnabledRayTracingPipelineFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR mEnabledAccelerationStructureFeatures;
#endif
#endif
};


DECLARE_RENDERER_FUNCTION(bool, isRaytracingSupported, Renderer* pRenderer)
DECLARE_RENDERER_FUNCTION(bool, initRaytracing, Renderer* pRenderer, Raytracing** ppRaytracing);
DECLARE_RENDERER_FUNCTION(void, removeRaytracing, Renderer* pRenderer, Raytracing* pRaytracing)

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
DECLARE_RENDERER_FUNCTION(void, addAccelerationStructure, Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure)
DECLARE_RENDERER_FUNCTION(void, removeAccelerationStructure, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)
/// Free the scratch memory allocated by acceleration structure after it has been built completely
/// Does not free acceleration structure
DECLARE_RENDERER_FUNCTION(void, removeAccelerationStructureScratch, Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure)

DECLARE_RENDERER_FUNCTION(void, addRaytracingShaderTable, Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable)
DECLARE_RENDERER_FUNCTION(void, removeRaytracingShaderTable, Raytracing* pRaytracing, RaytracingShaderTable* pTable)

DECLARE_RENDERER_FUNCTION(void, cmdBuildAccelerationStructure, Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc)
DECLARE_RENDERER_FUNCTION(void, cmdDispatchRays, Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc)

#ifdef METAL
DECLARE_RENDERER_FUNCTION(void, addSSVGFDenoiser, Renderer* pRenderer, SSVGFDenoiser** ppDenoiser);
DECLARE_RENDERER_FUNCTION(void, removeSSVGFDenoiser, SSVGFDenoiser* pDenoiser);
DECLARE_RENDERER_FUNCTION(void, clearSSVGFDenoiserTemporalHistory, SSVGFDenoiser* pDenoiser);
DECLARE_RENDERER_FUNCTION(void, cmdSSVGFDenoise, Cmd* pCmd, SSVGFDenoiser* pDenoiser, Texture* pSourceTexture, Texture* pMotionVectorTexture, Texture* pDepthNormalTexture, Texture* pPreviousDepthNormalTexture, Texture** ppOut);
#endif
