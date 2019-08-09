#pragma once


#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/Renderer/IRenderer.h"


#if defined(METAL)
#include "Shaders/Metal/Shader_Defs.h"
#elif defined(DIRECT3D12) || defined(_DURANGO)
#define NO_HLSL_DEFINITIONS
#include "Shaders/D3D12/Shader_Defs.h"
#elif defined(VULKAN)
#define NO_GLSL_DEFINITIONS
#include "Shaders/Vulkan/Shader_Defs.h"
#endif

#include "SDF_Constant.h"
#include "ASMConstant.h"



#define SAN_MIGUEL_OFFSETX 150.f
#define MESH_COUNT 1
#define MESH_SCALE 10.f

#if defined(_DURANGO)
//const static eastl::string gGeneratedSDFBinaryDir = "SDF/";
const static eastl::string gGeneratedSDFBinaryDir = "";
#else
const static eastl::string gGeneratedSDFBinaryDir = "../../../UnitTestResources/SDF/";
#endif

struct MeshInfoUniformBlock
{
	mat4 mWorldViewProjMat;
	mat4 mWorldMat;

	CullingViewPort cullingViewports[2];

	MeshInfoUniformBlock()
	{
		mWorldViewProjMat = mat4::identity();
		mWorldMat = mat4::identity();
	}
};


struct VisibilityBufferConstants
{
	mat4 mWorldViewProjMat[2];
	CullingViewPort mCullingViewports[2];
	uint32_t mValidNumCull = 0;
};


struct ASMAtlasQuadsUniform
{
	vec4 mPosData;
	vec4 mMiscData;
	vec4 mTexCoordData;
};


struct ASMPackedAtlasQuadsUniform
{
	vec4 mQuadsData[PACKED_QUADS_ARRAY_REGS];
};


struct ASMUniformBlock
{
	mat4 mIndexTexMat;
	mat4 mPrerenderIndexTexMat;
	vec4 mSearchVector;
	vec4 mPrerenderSearchVector;
	vec4 mWarpVector;
	vec4 mPrerenderWarpVector;
	//X is for IsPrerenderAvailable or not
	//Y is for whether we are using parallax corrected or not;
	vec4 mMiscBool;
	float mPenumbraSize;
};


struct ASMCpuSettings
{
	bool mSunCanMove = false;
	bool mEnableParallax = true;
	bool mEnableCrossFade = true;
	float mPenumbraSize = 15.f;
	float mParallaxStepDistance = 50.f;
	float mParallaxStepBias = 80.f;
	bool mShowDebugTextures = false;
};



typedef struct BufferIndirectCommand
{
	// Metal does not use index buffer since there is no builtin primitive id
#if defined(METAL)
	IndirectDrawIndexArguments arg;
#else
	// Draw ID is sent as indirect argument through root constant in DX12
#if defined(DIRECT3D12)
	uint32_t                   drawId;
#endif
	IndirectDrawIndexArguments arg;
#if defined(DIRECT3D12)
	uint32_t                   _pad0, _pad1;
#else
	uint32_t _pad0, _pad1, _pad2;
#endif
#endif
} BufferIndirectCommand;


typedef struct MeshSDFConstants
{
	//TODO:
	//missing center of the object's bbox & the radius of the object	
	mat4 mWorldToVolumeMat[SDF_MAX_OBJECT_COUNT];
	vec4 mUVScaleAndVolumeScale[SDF_MAX_OBJECT_COUNT];
	//the local position of the extent in volume dimension space (aka TEXEL space of a 3d texture)
	vec4 mLocalPositionExtent[SDF_MAX_OBJECT_COUNT];
	vec4 mUVAddAndSelfShadowBias[SDF_MAX_OBJECT_COUNT];
	//store min & max distance for x y, for z it stores the two sided world space mesh distance bias
	vec4 mSDFMAD[SDF_MAX_OBJECT_COUNT];
	uint32_t mNumObjects = 0;
}MeshSDFConstants;


typedef struct SDFMeshVolumeDataConstants
{
	float mData[SDF_MAX_VOXEL_ONE_DIMENSION_X * SDF_MAX_VOXEL_ONE_DIMENSION_Y * SDF_MAX_VOXEL_ONE_DIMENSION_Z];
}SDFMeshVolumeDataConstants;


typedef struct UpdateSDFVolumeTextureAtlasConstants
{
	ivec3 mSourceAtlasVolumeMinCoord;
	ivec3 mSourceDimensionSize;
	ivec3 mSourceAtlasVolumeMaxCoord;
}UpdateSDFVolumeTextureAtlasConstants;

