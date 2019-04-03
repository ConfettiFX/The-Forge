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

#if defined(VULKAN)
#define ApiExport //extern "C"
#else
#define ApiExport
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
MAKE_ENUM_FLAG(unsigned, AccelerationStructureBuildFlags)

//Rustam: check if this can be mapped to Metal
typedef enum AccelerationStructureGeometryFlags
{
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NONE = 0,
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE = 0x1,
	ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION = 0x2
} AccelerationStructureGeometryFlags;
MAKE_ENUM_FLAG(unsigned, AccelerationStructureGeometryFlags)

//Rustam: check if this can be mapped to Metal
typedef enum AccelerationStructureInstanceFlags
{
	ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE = 0,
	ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE = 0x1,
	ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE = 0x2,
	ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_OPAQUE = 0x4,
	ACCELERATION_STRUCTURE_INSTANCE_FLAG_FORCE_NON_OPAQUE = 0x8
} AccelerationStructureInstanceFlags;
MAKE_ENUM_FLAG(unsigned, AccelerationStructureInstanceFlags)

typedef struct AccelerationStructureInstanceDesc
{
    unsigned                mAccelerationStructureIndex;
	/// Row major affine transform for transforming the vertices in the geometry stored in pAccelerationStructure
	float				   mTransform[12];
	/// User defined instanced ID which can be queried in the shader
	unsigned				mInstanceID;
	unsigned				mInstanceMask;
	unsigned				mInstanceContributionToHitGroupIndex;
	AccelerationStructureInstanceFlags  mFlags;
} AccelerationStructureInstanceDesc;

typedef struct AccelerationStructureGeometryDesc
{
	AccelerationStructureGeometryFlags  mFlags;
    float3*     pVertexArray;
    unsigned    vertexCount;
    union{
        uint32_t*       pIndices32;
        uint16_t*       pIndices16;
    };
    unsigned    indicesCount;
    IndexType   indexType;
} AccelerationStructureGeometryDesc;
/************************************************************************/
//	  Bottom Level Structures define the geometry data such as vertex buffers, index buffers
//	  Top Level Structures define the instance data for the geometry such as instance matrix, instance ID, ...
// #mDescCount - Number of geometries or instances in this structure
/************************************************************************/
typedef struct AccelerationStructureDescBottom
{
	AccelerationStructureBuildFlags		    mFlags;
	/// Number of geometries / instances in thie acceleration structure
	unsigned								mDescCount;
    /// Array of geometries in the bottom level acceleration structure
    AccelerationStructureGeometryDesc*  pGeometryDescs;
} AccelerationStructureDescBottom;

typedef struct AccelerationStructureDescTop
{
    AccelerationStructureBuildFlags         mFlags;
    unsigned                                mInstancesDescCount;
    AccelerationStructureInstanceDesc*      pInstanceDescs;
    unsigned                                mBottomASDescsCount;
    AccelerationStructureDescBottom*        mBottomASDescs;
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

typedef struct RaytracingShaderTableRecordDesc
{
	const char*		  pName;
    bool              mInvokeTraceRay;
    uint32_t        mHitShaderIndex;
    uint32_t        mMissShaderIndex;
} RaytracingShaderTableRecordDesc;

typedef struct RaytracingShaderTableDesc
{
	Pipeline*						    pPipeline;
	RootSignature*						pEmptyRootSignature;
	DescriptorBinder*					pDescriptorBinder;
	RaytracingShaderTableRecordDesc*	pRayGenShader;
	RaytracingShaderTableRecordDesc*	pMissShaders;
	RaytracingShaderTableRecordDesc*	pHitGroups;
	unsigned							mMissShaderCount;
	unsigned							mHitGroupCount;
} RaytracingShaderTableDesc;

typedef struct RaytracingDispatchDesc
{
	uint32_t				mWidth;
	uint32_t				mHeight;
    uint32_t                mRootSignatureDescriptorsCount;
	AccelerationStructure*  pTopLevelAccelerationStructure;
	RaytracingShaderTable*  pShaderTable;
    DescriptorData*         pRootSignatureDescriptorData;
	RootSignature*          pRootSignature;
	DescriptorBinder*		pDescriptorBinder;
    Pipeline*				pPipeline;
} RaytracingDispatchDesc;

typedef struct RaytracingBuildASDesc
{
	AccelerationStructure* pAccelerationStructure;
	unsigned  mBottomASIndicesCount;
	unsigned* pBottomASIndices;
} RaytracingBuildASDesc;

struct Raytracing
{
	Renderer*		pRenderer;

#ifdef DIRECT3D12
	ID3D12Device5*	pDxrDevice;
	uint64_t		mDescriptorsAllocated;
#endif
#ifdef METAL
    MPSRayIntersector* pIntersector;
#endif

#ifdef VULKAN
#ifdef VK_NV_RAY_TRACING_SPEC_VERSION
	VkPhysicalDeviceRayTracingPropertiesNV* pRayTracingProperties;
#endif
#endif
};

const static char* RaytracingShaderSettingsBufferName = "gSettings";
const uint32_t RaytracingUserdataStartBufferRegister = 10;

#if !defined(ENABLE_RENDERER_RUNTIME_SWITCH)

bool isRaytracingSupported(Renderer* pRenderer);
bool initRaytracing(Renderer* pRenderer, Raytracing** ppRaytracing);
void removeRaytracing(Renderer* pRenderer, Raytracing* pRaytracing);

/// pScratchBufferSize - Holds the size of scratch buffer to be passed to cmdBuildAccelerationStructure
void addAccelerationStructure(Raytracing* pRaytracing, const AccelerationStructureDescTop* pDesc, AccelerationStructure** ppAccelerationStructure);
void removeAccelerationStructure(Raytracing* pRaytracing, AccelerationStructure* pAccelerationStructure);

void addRaytracingShaderTable(Raytracing* pRaytracing, const RaytracingShaderTableDesc* pDesc, RaytracingShaderTable** ppTable);
void removeRaytracingShaderTable(Raytracing* pRaytracing, RaytracingShaderTable* pTable);

void cmdBuildAccelerationStructure(Cmd* pCmd, Raytracing* pRaytracing, RaytracingBuildASDesc* pDesc);
void cmdDispatchRays(Cmd* pCmd, Raytracing* pRaytracing, const RaytracingDispatchDesc* pDesc);

#endif
