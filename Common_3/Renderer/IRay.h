#pragma once

#include "IRenderer.h"

#ifdef __cplusplus
#ifndef MAKE_ENUM_FLAG

#define MAKE_ENUM_FLAG(TYPE,ENUM_TYPE) \
static inline ENUM_TYPE operator|(ENUM_TYPE a, ENUM_TYPE b) \
{ \
	return (ENUM_TYPE)((TYPE)(a) | (TYPE)(b)); \
} \
static inline ENUM_TYPE operator&(ENUM_TYPE a, ENUM_TYPE b) \
{ \
	return (ENUM_TYPE)((TYPE)(a) & (TYPE)(b)); \
} \
static inline ENUM_TYPE operator|=(ENUM_TYPE& a, ENUM_TYPE b) \
{ \
	return a = (a | b); \
} \
static inline ENUM_TYPE operator&=(ENUM_TYPE& a, ENUM_TYPE b) \
{ \
	return a = (a & b); \
} \

#endif
#else
#define MAKE_ENUM_FLAG(TYPE,ENUM_TYPE)
#endif

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
	AccelerationStructureGeometryFlags  mFlags;
	void*                               pVertexArray;
	uint32_t                            mVertexCount;
    union
	{
        uint32_t*                       pIndices32;
        uint16_t*                       pIndices16;
    };
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
/************************************************************************/
// Defines which shaders will be used by this hit group
// #pIntersectionShaderName - Name of shader used to test intersection with ray
//	  This will be NULL as long as user does not specify ACCELERATION_STRUCTURE_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS
// #pAnyHitShaderName - Name of shader executed when a ray hits
//	  This will be NULL if user specifies ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE in geometry creation
//	  This shader is usually used for work like alpha testing geometry to see if an intersection is valid
// #pClosestHitShaderName - Name of shader executed for the intersection closest to the ray
//	  This shader will do most of the work like shading
// #pHitGroupName - User defined name of the hit group. Use the same name when creating the shader table
/************************************************************************/
typedef struct RaytracingHitGroup
{
	RootSignature*	    pRootSignature;
	Shader*             pIntersectionShader;
	Shader*             pAnyHitShader;
	Shader*             pClosestHitShader;
	const char*			pHitGroupName;
} RaytracingHitGroup;

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
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
	VkPhysicalDeviceRayTracingPropertiesNV mRayTracingProperties;
#endif
#endif
};

API_INTERFACE bool FORGE_CALLCONV isRaytracingSupported(Renderer* pRenderer);
API_INTERFACE bool FORGE_CALLCONV initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing);
API_INTERFACE void FORGE_CALLCONV removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing);

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
API_INTERFACE void FORGE_CALLCONV addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure);
API_INTERFACE void FORGE_CALLCONV removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);
/// Free the scratch memory allocated by acceleration structure after it has been built completely
/// Does not free acceleration structure
API_INTERFACE void FORGE_CALLCONV removeAccelerationStructureScratch(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);

API_INTERFACE void FORGE_CALLCONV addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable);
API_INTERFACE void FORGE_CALLCONV removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable);

API_INTERFACE void FORGE_CALLCONV cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc);
API_INTERFACE void FORGE_CALLCONV cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc);

#ifdef METAL
API_INTERFACE void FORGE_CALLCONV addSSVGFDenoiser(Renderer* pRenderer, SSVGFDenoiser** ppDenoiser);
API_INTERFACE void FORGE_CALLCONV removeSSVGFDenoiser(SSVGFDenoiser* pDenoiser);
API_INTERFACE void FORGE_CALLCONV clearSSVGFDenoiserTemporalHistory(SSVGFDenoiser* pDenoiser);
API_INTERFACE void FORGE_CALLCONV cmdSSVGFDenoise(Cmd* pCmd, SSVGFDenoiser* pDenoiser, Texture* pSourceTexture, Texture* pMotionVectorTexture, Texture* pDepthNormalTexture, Texture* pPreviousDepthNormalTexture, Texture** ppOut);
#endif
