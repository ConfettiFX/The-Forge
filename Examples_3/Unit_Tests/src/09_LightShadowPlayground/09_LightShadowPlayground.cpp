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
#define ESM_MSAA_SAMPLES 1
#define ESM_SHADOWMAP_RES 2048u


//assimp
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

//ea stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/queue.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../../Common_3/OS/Core/ThreadSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Core/RingBuffer.h"
//GPU Profiler
#include "../../../../Common_3/Renderer/GpuProfiler.h"


#include "../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "Geometry.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"


#define Epilson (1.e-4f)

#define SAN_MIGUEL_ORIGINAL_SCALE 50.0f
#define SAN_MIGUEL_ORIGINAL_OFFSETX -20.f


#define SAN_MIGUEL_OFFSETX 150.f
#define MESH_COUNT 1
#define MESH_SCALE 10.f


// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = 2;
const uint32_t GEOMSET_OPAQUE = 0;
const uint32_t GEOMSET_ALPHATESTED = 1;

enum ShadowType
{
	SHADOW_TYPE_ESM,    //Exponential Shadow Map
	SHADOW_TYPE_ASM, //Adaptive Shadow Map, has Parallax Corrected Cache algorithm that approximate moving sun's shadow
	SHADOW_TYPE_MESH_BAKED_SDF, // Signed Distance field shadow for mesh using generated baked data
	SHADOW_TYPE_COUNT
};
typedef struct RenderSettingsUniformData
{
	vec4 mWindowDimension = { 1, 1, 0, 0 };    //only first two are used to represents window width and height, z and w are paddings
	uint32_t mShadowType = SHADOW_TYPE_ASM;
} RenderSettingsUniformData;

enum Projections
{
	MAIN_CAMERA,    // primary view
	SHADOWMAP,
	PROJECTION_ASM,
	PROJECTION_COUNT
};

struct 
{
	bool mHoldFilteredTriangles = false;
	bool mAsyncCompute = true;
	bool mIsGeneratingSDF = false;
	bool mMicroProfiler = false;
	bool mToggleVsync = false;
}gAppSettings;




typedef struct ObjectInfoStruct
{
	vec4 mColor;
	vec3 mTranslation;
	float3 mScale;
	mat4 mTranslationMat;
	mat4 mScaleMat;
} ObjectInfoStruct;

typedef struct MeshInfoStruct
{
	vec4 mColor;
	float3 mTranslation;
	float3 mOffsetTranslation;
	float3 mScale;
	mat4 mTranslationMat;
	mat4 mScaleMat;
} MeshInfoStruct;


struct PerFrameData
{
	// Stores the camera/eye position in object space for cluster culling
	vec3              gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
	uint32_t gDrawCount[gNumGeomSets] = { 0 };
};

typedef struct LightUniformBlock
{
	mat4  mLightViewProj;
	vec4  mLightPosition;
	vec4  mLightColor = { 1, 0, 0, 1 };
	vec4  mLightUpVec;
	vec4 mTanLightAngleAndThresholdValue;
	vec3 mLightDir;
} LightUniformBlock;

typedef struct CameraUniform
{
	mat4  mView;
	mat4  mProject;
	mat4  mViewProject;
	mat4  mInvView;
	mat4  mInvProj;
	mat4  mInvViewProject;
	vec4  mCameraPos;
	float mNear;
	float mFarNearDiff;
	float mFarNear;
	float paddingForAlignment0;
	vec2 mTwoOverRes;
	float _pad1;
	float _pad2;
	vec2 mWindowSize;
	float _pad3;
	float _pad4;
	vec4 mDeviceZToWorldZ;
} CameraUniform;

typedef struct ESMInputConstants
{
	float mEsmControl = 80.f;
} ESMInputConstants;


struct QuadDataUniform
{
	mat4 mModelMat;
};

struct TextureLoadTaskData
{
	Texture**       textures;
	const char**    mNames;
	TextureLoadDesc mDesc;
};

void loadTexturesTask(void* data, uintptr_t i)
{
    TextureLoadTaskData* pTaskData = (TextureLoadTaskData*)data;
    PathHandle texturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, pTaskData->mNames[i]);
	TextureLoadDesc desc = pTaskData->mDesc;
	desc.pFilePath = texturePath;
	desc.ppTexture = &pTaskData->textures[i];
	addResource(&desc, true);
}



struct UniformDataSkybox
{
	mat4 mProjectView;
	vec3 mCamPos;
};

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


typedef struct UpdateSDFVolumeTextureAtlasConstants
{
	ivec3 mSourceAtlasVolumeMinCoord;
	ivec3 mSourceDimensionSize;
	ivec3 mSourceAtlasVolumeMaxCoord;
}UpdateSDFVolumeTextureAtlasConstants;


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

UniformDataSkybox gUniformDataSky;
const uint32_t    gSkyboxSize = 1024;
const uint32_t    gSkyboxMips = 9;

ThreadSystem* pThreadSystem = NULL;

static float asmCurrentTime = 0.0f;
constexpr uint32_t gImageCount = 3;
const char* gSceneName = "SanMiguel.obj";


/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pRenderTargetScreen;
RenderTarget* pRenderTargetDepth = NULL;
RenderTarget* pRenderTargetShadowMap = NULL;

RenderTarget* pRenderTargetASMColorPass = NULL;
RenderTarget* pRenderTargetASMDepthPass = NULL;


RenderTarget* pRenderTargetASMDepthAtlas = NULL;
RenderTarget* pRenderTargetASMDEMAtlas = NULL;

RenderTarget* pRenderTargetASMIndirection[gs_ASMMaxRefinement + 1] = { NULL };
RenderTarget* pRenderTargetASMPrerenderIndirection[gs_ASMMaxRefinement + 1] = { NULL };


RenderTarget* pRenderTargetASMLodClamp = NULL;
RenderTarget* pRenderTargetASMPrerenderLodClamp = NULL;



RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pRenderTargetIntermediate = NULL;

Texture* pTextureSkybox = NULL;

/************************************************************************/

Buffer* pBufferSkyboxVertex = NULL;
Buffer* pBufferSkyboxUniform[gImageCount] = { NULL };
Buffer* pBufferQuadVertex = NULL;
Buffer* pBufferBoxIndex = NULL;



const float gQuadVertices[] ={
	// positions   // texCoords
	-1.0f, 1.0f, 0.f, 0.f,  0.0f, 0.0f,
	-1.0f, -1.0f, 0.f, 0.f,  0.0f, 1.0f,
	1.0f, -1.0f, 0.f, 0.f,  1.0f, 1.0f,

	1.0f, -1.0f, 0.f, 0.f,  1.0f, 1.0f,
	1.0f, 1.0f, 0.f, 0.f,  1.0f, 0.0f,
	-1.0f, 1.0f, 0.f, 0.f, 0.0f, 0.0f,
};


// Warning these indices are not good indices for cubes that want correct normals
// (a.k.a. all vertices are shared)
const uint16_t gBoxIndices[36] = {
	0, 1, 4, 4, 1, 5,    //y-
	0, 4, 2, 2, 4, 6,    //x-
	0, 2, 1, 1, 2, 3,    //z-
	2, 6, 3, 3, 6, 7,    //y+
	1, 3, 5, 5, 3, 7,    //x+
	4, 5, 6, 6, 5, 7     //z+
};

/************************************************************************/
// Skybox Shader Pack
/************************************************************************/
Shader*           pShaderSkybox = NULL;
Pipeline*         pPipelineSkybox = NULL;
RootSignature*    pRootSignatureSkybox = NULL;
DescriptorSet*    pDescriptorSetSkybox[2] = { NULL };
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*           pShaderClearBuffers = NULL;
Pipeline*         pPipelineClearBuffers = NULL;
//RootSignature*    pRootSignatureClearBuffers = NULL;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*           pShaderTriangleFiltering = NULL;
Pipeline*         pPipelineTriangleFiltering = NULL;
RootSignature*    pRootSignatureTriangleFiltering = NULL;
DescriptorSet*    pDescriptorSetTriangleFiltering[2] = { NULL };
/************************************************************************/
// Batch compaction pipeline
/************************************************************************/

Shader*           pShaderBatchCompaction = NULL;
Pipeline*         pPipelineBatchCompaction = NULL;
//RootSignature*    pRootSignatureBatchCompaction = NULL;




/************************************************************************/
// indirect vib buffer depth pass Shader Pack
/************************************************************************/
Shader* pShaderIndirectDepthPass = NULL;
Pipeline* pPipelineIndirectDepthPass = NULL;

Pipeline* pPipelineESMIndirectDepthPass = NULL;
/************************************************************************/
// indirect vib buffer alpha depth pass Shader Pack
/************************************************************************/
Shader* pShaderIndirectAlphaDepthPass = NULL;
Pipeline* pPipelineIndirectAlphaDepthPass = NULL;

Pipeline* pPipelineESMIndirectAlphaDepthPass = NULL;
/************************************************************************/
// ASM copy quads pass Shader Pack
/************************************************************************/
Shader* pShaderASMCopyDepthQuadPass = NULL;
Pipeline* pPipelineASMCopyDepthQuadPass = NULL;
RootSignature* pRootSignatureASMCopyDepthQuadPass = NULL;
DescriptorSet* pDescriptorSetASMCopyDepthQuadPass[2] = { NULL };
/************************************************************************/
// ASM fill indirection Shader Pack
/************************************************************************/
Shader* pShaderASMFillIndirection = NULL;
Pipeline* pPipelineASMFillIndirection = NULL;
RootSignature* pRootSignatureASMFillIndirection = NULL;
DescriptorSet* pDescriptorSetASMFillIndirection[3] = { NULL };
/************************************************************************/
// ASM fill lod clamp Pack
/************************************************************************/
//Reuse pShaderASMFillIndirection since they pretty much has the same shader
Pipeline* pPipelineASMFillLodClamp = NULL;
RootSignature* pRootSignatureASMFillLodClamp = NULL;
DescriptorSet* pDescriptorSetASMFillLodClamp = NULL;
/************************************************************************/
// ASM Copy DEM Shader Pack
/************************************************************************/
Shader* pShaderASMCopyDEM = NULL;
Pipeline* pPipelineASMCopyDEM = NULL;
RootSignature* pRootSignatureASMCopyDEM = NULL;
DescriptorSet* pDescriptorSetASMCopyDEM[2] = { NULL };
/************************************************************************/
// ASM generate DEM Shader Pack
/************************************************************************/
Shader* pShaderASMGenerateDEM = NULL;
Pipeline* pPipelineASMDEMAtlasToColor = NULL;
Pipeline* pPipelineASMDEMColorToAtlas = NULL;
RootSignature* pRootSignatureASMDEMAtlasToColor = NULL;
DescriptorSet* pDescriptorSetASMDEMAtlasToColor[2] = { NULL };
RootSignature* pRootSignatureASMDEMColorToAtlas = NULL;
DescriptorSet* pDescriptorSetASMDEMColorToAtlas[2] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVBBufferPass[gNumGeomSets] = {};
Pipeline*         pPipelineVBBufferPass[gNumGeomSets] = {};
RootSignature*    pRootSignatureVBPass = NULL;
DescriptorSet*    pDescriptorSetVBPass[3] = { NULL };
CommandSignature* pCmdSignatureVBPass = NULL;
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*           pShaderVBShade = NULL;
Pipeline*         pPipelineVBShadeSrgb = NULL;
RootSignature*    pRootSignatureVBShade = NULL;
DescriptorSet*    pDescriptorSetVBShade[2] = { NULL };
/************************************************************************/
// SDF draw update volume texture atlas pipeline
/************************************************************************/
Shader* pShaderUpdateSDFVolumeTextureAtlas = NULL;
Pipeline* pPipelineUpdateSDFVolumeTextureAtlas = NULL;
RootSignature* pRootSignatureUpdateSDFVolumeTextureAtlas = NULL;
DescriptorSet* pDescriptorSetUpdateSDFVolumeTextureAtlas[2] = { NULL };
/************************************************************************/
// SDF mesh visualization pipeline
/************************************************************************/
Shader* pShaderSDFMeshVisualization = NULL;
Pipeline* pPipelineSDFMeshVisualization = NULL;
RootSignature* pRootSignatureSDFMeshVisualization = NULL;
DescriptorSet* pDescriptorSetSDFMeshVisualization[2] = { NULL };
/************************************************************************/
// SDF baked mesh shadow pipeline
/************************************************************************/
Shader* pShaderSDFMeshShadow = NULL;
Pipeline* pPipelineSDFMeshShadow = NULL;
RootSignature* pRootSignatureSDFMeshShadow = NULL;
DescriptorSet* pDescriptorSetSDFMeshShadow[2] = { NULL };
/************************************************************************/
// SDF upsample shadow texture pipeline
/************************************************************************/
Shader* pShaderUpsampleSDFShadow = NULL;
Pipeline* pPipelineUpsampleSDFShadow = NULL;
RootSignature* pRootSignatureUpsampleSDFShadow = NULL;
DescriptorSet* pDescriptorSetUpsampleSDFShadow[2] = { NULL };
/************************************************************************/
// Display quad texture shader pack
/************************************************************************/
Shader* pShaderQuad = NULL;
Pipeline* pPipelineQuad = NULL;
RootSignature* pRootSignatureQuad = NULL;
DescriptorSet* pDescriptorSetQuad[2] = { NULL };
/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*           pShaderPresentPass = NULL;
Pipeline*         pPipelinePresentPass = NULL;
RootSignature*    pRootSignaturePresentPass = NULL;
DescriptorSet*    pDescriptorSetPresentPass = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerMiplessSampler = NULL;
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerMiplessNear = NULL;
Sampler* pSamplerMiplessLinear = NULL;
Sampler* pSamplerComparisonShadow = NULL;
Sampler* pSamplerMiplessClampToBorderNear = NULL;
Sampler* pSamplerLinearRepeat = NULL;
/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullFront = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;
RasterizerState* pRasterizerStateNonBiasCullFront = NULL;
RasterizerState* pRasterizerStateCullBack = NULL;

/************************************************************************/
// Blend states
/************************************************************************/
BlendState* pBlendStateSkyBox = NULL;

/************************************************************************/
// Constant buffers
/************************************************************************/

Buffer* pBufferMeshTransforms[MESH_COUNT][gImageCount] = { NULL };
Buffer* pBufferMeshShadowProjectionTransforms[MESH_COUNT][gImageCount] = { NULL };

Buffer* pBufferLightUniform[gImageCount] = { NULL };
Buffer* pBufferESMUniform[gImageCount] = { NULL };
Buffer* pBufferRenderSettings[gImageCount] = { NULL };
Buffer* pBufferCameraUniform[gImageCount] = { NULL };

Buffer* pBufferASMAtlasQuadsUniform[gImageCount] = { NULL };

Buffer* pBufferASMCopyDEMPackedQuadsUniform[gImageCount] = { NULL };
Buffer* pBufferASMAtlasToColorPackedQuadsUniform[gImageCount] = { NULL };
Buffer* pBufferASMColorToAtlasPackedQuadsUniform[gImageCount] = { NULL };

Buffer* pBufferASMLodClampPackedQuadsUniform[gImageCount] = { NULL };

Buffer* pBufferASMPackedIndirectionQuadsUniform[gs_ASMMaxRefinement + 1][gImageCount] = { NULL };
Buffer* pBufferASMPackedPrerenderIndirectionQuadsUniform[gs_ASMMaxRefinement + 1][gImageCount] = { NULL };
Buffer* pBufferASMClearIndirectionQuadsUniform[gImageCount] = { NULL };

Buffer* pBufferASMDataUniform[gImageCount] = { NULL };


Buffer* pBufferIndirectDrawArgumentsAll[gNumGeomSets] = { NULL };
Buffer* pBufferIndirectMaterialAll = NULL;
Buffer* pBufferMeshConstants = NULL;

Buffer* pBufferFilteredIndirectDrawArguments[gImageCount][gNumGeomSets][NUM_CULLING_VIEWPORTS] = { NULL };
Buffer* pBufferUncompactedDrawArguments[gImageCount][NUM_CULLING_VIEWPORTS] = { NULL };
Buffer* pBufferFilterIndirectMaterial[gImageCount] = { NULL };

Buffer* pBufferMaterialProperty = NULL;

Buffer* pBufferFilteredIndex[gImageCount][NUM_CULLING_VIEWPORTS] = {};

Buffer* pBufferQuadUniform[gImageCount] = { NULL };
Buffer* pBufferVisibilityBufferConstants[gImageCount] = { NULL };


/************************************************************************/
//Constants for SDF Mesh
/************************************************************************/
Buffer* pBufferMeshSDFConstants[gImageCount] = { NULL };
Buffer* pBufferUpdateSDFVolumeTextureAtlasConstants[gImageCount] = { NULL };

Buffer* pBufferSDFVolumeData[gImageCount] = { NULL };
//Buffer* pBufferSDFVolumeData = { NULL };

/************************************************************************/
//Textures/rendertargets for SDF Algorithm
/************************************************************************/

RenderTarget* pRenderTargetSDFMeshVisualization = NULL;
RenderTarget* pRenderTargetSDFMeshShadow = NULL;

RenderTarget* pRenderTargetUpSampleSDFShadow = NULL;

Texture* pTextureSDFVolumeAtlas = NULL;

Buffer* pBufferSDFVolumeAtlas[gImageCount] = { NULL };

/************************************************************************/
// Depth State
/************************************************************************/
DepthState* pDepthStateEnable = NULL;
DepthState* pDepthStateDisable = NULL;
DepthState* pDepthStateTestOnly = NULL;
DepthState* pDepthStateStencilShadow = NULL;
DepthState* pDepthStateLEQUALEnable = NULL;


/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture* gDiffuseMapsStorage = NULL;
Texture* gNormalMapsStorage = NULL;
Texture* gSpecularMapsStorage = NULL;

eastl::vector<Texture*> gDiffuseMaps;
eastl::vector<Texture*> gNormalMaps;
eastl::vector<Texture*> gSpecularMaps;

eastl::vector<Texture*> gDiffuseMapsPacked;
eastl::vector<Texture*> gNormalMapsPacked;
eastl::vector<Texture*> gSpecularMapsPacked;

Buffer* pIndirectPosBuffer = NULL;
Buffer* pIndirectTexCoordBuffer = NULL;
Buffer* pIndirectNormalBuffer = NULL;
Buffer* pIndirectTangentBuffer = NULL;
Buffer* pIndirectIndexBuffer = NULL;
/************************************************************************/
// Render control variables
/************************************************************************/
struct
{
	uint32 mFilterWidth = 2U;
	float mEsmControl = 100.f;
} gEsmCpuSettings;

struct
{
	float mSourceAngle = 1.0f;
	//only used for ESM shadow
	//float2 mSunControl = { -2.1f, -0.213f };
	float2 mSunControl = { -2.1f, -0.961f };
	float mSunSpeedY = 0.025f;
	//only for SDF shadow now
	bool mAutomaticSunMovement = false;
} gLightCpuSettings;


struct
{
	bool mDrawSDFMeshVisualization = false;
}gBakedSDFMeshSettings;


ASMCpuSettings gASMCpuSettings;

/************************************************************************/

VirtualJoystickUI gVirtualJoystick;








bool gBufferUpdateSDFMeshConstantFlags[3] = { true, true, true };


// Constants
uint32_t					gFrameIndex = 0;
GpuProfiler*				pGpuProfilerGraphics = NULL;
GpuProfiler*				pGpuProfilerCompute = NULL;

RenderSettingsUniformData gRenderSettings;

MeshInfoUniformBlock   gMeshInfoUniformData[MESH_COUNT][gImageCount];

PerFrameData gPerFrameData[gImageCount] = {};

/************************************************************************/
// Triangle filtering data
/************************************************************************/

const uint32_t gSmallBatchChunkCount = max(1U, 512U / CLUSTER_SIZE) * 16U;
FilterBatchChunk* pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = {NULL};
GPURingBuffer* pBufferFilterBatchData = NULL;
///
MeshInfoUniformBlock   gMeshASMProjectionInfoUniformData[MESH_COUNT][gImageCount];

ASMUniformBlock gAsmModelUniformBlockData = {};

LightUniformBlock      gLightUniformData;
CameraUniform          gCameraUniformData;
ESMInputConstants      gESMUniformData;
//TODO remove this
QuadDataUniform gQuadUniformData;
MeshInfoStruct gMeshInfoData[MESH_COUNT] = {};
VisibilityBufferConstants gVisibilityBufferConstants[gImageCount] = {};

vec3 gObjectsCenter = { SAN_MIGUEL_OFFSETX, 0, 0 };

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

MeshIn*       pMeshes = NULL;
uint32_t      gMeshCount = 0;
uint32_t      gMaterialCount = 0;

uint64_t gFrameCount = 0;
/// UI
UIApp         gAppUI;
GuiComponent* pGuiWindow = NULL;
GuiComponent* pUIASMDebugTexturesWindow = NULL;
GuiComponent* pLoadingGui = NULL;
TextDrawDesc  gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;


Queue*   pComputeQueue = NULL;
CmdPool* pComputeCmdPool = NULL;
Cmd**    ppComputeCmds = NULL;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Fence*     pComputeCompleteFences[gImageCount] = { NULL };
Fence*     pTransitionFences = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pComputeCompleteSemaphores[gImageCount] = { NULL };

HiresTimer gTimer;

uint32_t gCurrentShadowType = SHADOW_TYPE_ASM;

static void setRenderTarget(
	Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions,
	const vec2& viewPortLoc, const vec2& viewPortSize)
{
	if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	else
	{
		cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
		// sets the rectangles to match with first attachment, I know that it's not very portable.
		RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
		cmdSetViewport(cmd, viewPortLoc.getX(), viewPortLoc.getY(), viewPortSize.getX(), viewPortSize.getY(), 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
	}
}

static void setRenderTarget(
	Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions)
{
	if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
	{
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}
	else
	{
		cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
		RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSizeTarget->mDesc.mWidth, (float)pSizeTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
	}
}


#ifdef _DURANGO
#include "../../../../Xbox/Common_3/Renderer/Direct3D12/Direct3D12X.h"
#endif




struct Ray
{
	Ray(const vec3& startPos,
		const vec3& dir)
		:startPos(startPos),
		dir(dir)
	{
	}

	vec3 Eval(float t) const
	{
		return startPos + (dir * t);
	}

	vec3 GetInvDir() const
	{
		return vec3(1.f / dir.getX(), 1.f / dir.getY(), 1.f / dir.getZ());
	}

	vec3 startPos;
	vec3 dir;
};


struct Triangle
{
	Triangle() {};
	Triangle(const vec3& v0, const vec3& v1, const vec3& v2,
		const vec3& n0, const vec3& n1, const vec3& n2)
		:
		mV0(v0),
		mV1(v1),
		mV2(v2),
		mN0(n0),
		mN1(n1),
		mN2(n2)
	{
		
		mE1 = mV1 - mV0;
		mE2 = mV2 - mV0;

	}


	void Init(const vec3& v0, const vec3& v1, const vec3& v2,
		const vec3& n0, const vec3& n1, const vec3& n2,
		const vec2& uv0, const vec2& uv1, const vec2& uv2)
	{
		mV0 = v0;
		mV1 = v1;
		mV2 = v2;
		mN0 = n0;
		mN1 = n1;
		mN2 = n2;

		mE1 = mV1 - mV0;
		mE2 = mV2 - mV0;

		mUV0 = uv0;
		mUV1 = uv1;
		mUV2 = uv2;
	}

	//triangle vertices
	vec3 mV0;
	vec3 mV1;
	vec3 mV2;

	//triangle edges
	vec3 mE1;
	vec3 mE2;

	//vertices normal
	vec3 mN0;
	vec3 mN1;
	vec3 mN2;

	vec2 mUV0;
	vec2 mUV1;
	vec2 mUV2;

	SDFMeshInstance* m_pMeshInstance;
};

struct Intersection
{
	Intersection(const vec3& hitted_pos,
		const vec3& hitted_normal,
		float t_intersection)
		:mIsIntersected(false),
		mHittedPos(hitted_pos),
		mHittedNormal(hitted_normal),
		mIntersection_TVal(t_intersection)
	{

	}
	Intersection()
		:mIsIntersected(false),
		mHittedPos(),
		mHittedNormal(),
		mIntersection_TVal(FLT_MAX),
		mHittedUV()
	{}


	bool mIsIntersected;
	vec3 mHittedPos;
	vec3 mHittedNormal;
	vec2 mHittedUV;
	float mIntersection_TVal;
	const Triangle* mIntersectedTriangle;
};


void RayIntersectTriangle(const Ray& ray, const Triangle& triangle, Intersection& outIntersection)
{
	vec3 P_V = cross(ray.dir, triangle.mE2);

	float P_dot_E1 = dot(P_V, triangle.mE1);

	if (P_dot_E1 == 0.f)
	{
		return;
	}

	vec3 S_V = ray.startPos - triangle.mV0;

	float u_val = dot(P_V, S_V) / P_dot_E1;

	if (u_val < Epilson || u_val > 1.f)
	{
		return;
	}

	vec3 Q_V = cross(S_V, triangle.mE1);

	float v_val = dot(ray.dir, Q_V) / P_dot_E1;

	if (v_val < Epilson || (v_val + u_val) > 1.f)
	{
		return;
	}

	float t_val = dot(triangle.mE2, Q_V) / P_dot_E1;

	if (t_val < Epilson)
	{
		return;
	}

	if (t_val < outIntersection.mIntersection_TVal)
	{
		vec2 barycentricUV = ((1.f - u_val - v_val) * 
			triangle.mUV0 + u_val * triangle.mUV1 + v_val * triangle.mUV2);

		outIntersection.mIsIntersected = true;
		outIntersection.mIntersection_TVal = t_val;
		outIntersection.mHittedPos = ray.Eval(t_val);
		outIntersection.mHittedNormal = normalize
		(
			((1.f - u_val - v_val) * triangle.mN0 + u_val * triangle.mN1 + v_val * triangle.mN2)
		);


		outIntersection.mHittedUV = barycentricUV;

		outIntersection.mIntersectedTriangle = &triangle;
	}

}

struct BVHAABBox
{
	AABB mAABB;
	vec3  Center;
	Triangle mTriangle;

	int32_t   InstanceID;
	float SurfaceAreaLeft;
	float SurfaceAreaRight;

	BVHAABBox()
	{
		mAABB.minBounds = vec3(FLT_MAX);
		mAABB.maxBounds = vec3(-FLT_MAX);
		InstanceID = 0;
	}

	void Expand(vec3& point)
	{
		mAABB.minBounds = vec3(
			fmin(mAABB.minBounds.getX(), point.getX()),
			fmin(mAABB.minBounds.getY(), point.getY()),
			fmin(mAABB.minBounds.getZ(), point.getZ())
		);

		mAABB.maxBounds = vec3(
			fmax(mAABB.maxBounds.getX(), point.getX()),
			fmax(mAABB.maxBounds.getY(), point.getY()),
			fmax(mAABB.maxBounds.getZ(), point.getZ())
		);

		Center = 0.5f * (mAABB.maxBounds + mAABB.minBounds);
	}

	void Expand(BVHAABBox& aabox)
	{
		Expand(aabox.mAABB.minBounds);
		Expand(aabox.mAABB.maxBounds);
	}
};



struct BVHNode
{
	float    SplitCost;
	BVHAABBox   BoundingBox;
	BVHNode* Left;
	BVHNode* Right;
};


struct BVHTree
{

	BVHTree() :mRootNode(NULL), mBVHNodeCount(0), mTransitionNodeCount(0)
	{
		mBBOXDataList.reserve(1000000);
	}


	eastl::vector<BVHAABBox> mBBOXDataList;
	BVHNode* mRootNode;

	uint32_t mBVHNodeCount;
	uint32_t mTransitionNodeCount;

};


bool RayIntersectsBox(const vec3& origin, const vec3& rayDirInv, const vec3& BboxMin, const vec3& BboxMax)
{
	const vec3 nonInvT0 = (BboxMin - origin);
	const vec3 nonInvT1 = (BboxMax - origin);
	const vec3 t0 = vec3(nonInvT0.getX() * rayDirInv.getX(), nonInvT0.getY() * rayDirInv.getY(), nonInvT0.getZ() * rayDirInv.getZ());
	const vec3 t1 = vec3(nonInvT1.getX() * rayDirInv.getX(), nonInvT1.getY() * rayDirInv.getY(), nonInvT1.getZ() * rayDirInv.getZ());

	const vec3 tmax = vec3(fmax(t0.getX(), t1.getX()), fmax(t0.getY(), t1.getY()), fmax(t0.getZ(), t1.getZ()));
	const vec3 tmin = vec3(fmin(t0.getX(), t1.getX()), fmin(t0.getY(), t1.getY()), fmin(t0.getZ(), t1.getZ()));

	const float a1 = fmin(tmax.getX(), fmin(tmax.getY(), tmax.getZ()));
	const float a0 = fmax(fmax(tmin.getX(), tmin.getY()), fmax(tmin.getZ(), 0.0f));

	return a1 >= a0;
}


void BVHTreeIntersectRayAux(BVHNode* rootNode, BVHNode* node,
	const Ray& ray, Intersection& outIntersection)
{
	if (!node)
	{
		return;
	}

	if (node->BoundingBox.InstanceID < 0.f)
	{
		bool intersects = RayIntersectsBox(ray.startPos, ray.GetInvDir(),
			node->BoundingBox.mAABB.minBounds, node->BoundingBox.mAABB.maxBounds);

		if (intersects)
		{
			BVHTreeIntersectRayAux(rootNode, node->Left, ray, outIntersection);
			BVHTreeIntersectRayAux(rootNode, node->Right, ray, outIntersection);
		}
	}
	else
	{
		RayIntersectTriangle(ray, node->BoundingBox.mTriangle, outIntersection);
	}
}

void AddMeshInstanceToBBOX(BVHTree* bvhTree, const mat4& meshWorldMat, SDFMesh* mesh,
	SDFMeshInstance* meshInst)
{
	if (!meshInst->mStackInstances.empty())
	{
		for (uint32_t stackIndex = 0; stackIndex <
			meshInst->mStackInstances.size(); ++stackIndex)
		{
			SDFMeshInstance* stackInstance = &meshInst->mStackInstances[stackIndex];

			int32_t lastIndex = stackInstance->mIndexCount + stackInstance->mStartIndex;
			for (int32_t index = stackInstance->mStartIndex; index < lastIndex; index += 3)
			{
				int32_t index0 = mesh->mIndices[index + 0];
				int32_t index1 = mesh->mIndices[index + 1];
				int32_t index2 = mesh->mIndices[index + 2];

				vec3 v0 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index0]), 1.f)).getXYZ();
				vec3 v1 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index1]), 1.f)).getXYZ();
				vec3 v2 = (meshWorldMat * vec4(
					SceneVertexPos::ToVec3(mesh->mPositions[stackInstance->mStartVertex + index2]), 1.f)).getXYZ();

				//TODO: multiply normal by world mat
				vec3 n0 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index0];
				vec3 n1 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index1];
				vec3 n2 = mesh->mUncompressedNormals[stackInstance->mStartVertex + index2];


				vec2 UV0 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index0];
				vec2 UV1 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index1];
				vec2 UV2 = mesh->mUncompressedTexCoords[stackInstance->mStartVertex + index2];

				bvhTree->mBBOXDataList.push_back(BVHAABBox());
				BVHAABBox& bvhAABBOX = bvhTree->mBBOXDataList.back();

				bvhAABBOX.mTriangle.Init(v0, v1, v2, n0, n1, n2, UV0, UV1, UV2);
				bvhAABBOX.mTriangle.m_pMeshInstance = stackInstance;
				bvhAABBOX.Expand(v0);
				bvhAABBOX.Expand(v1);
				bvhAABBOX.Expand(v2);

				bvhAABBOX.InstanceID = 0;
			}
		}
	}
	else
	{
		int32_t lastIndex = meshInst->mIndexCount + meshInst->mStartIndex;
		for (int32_t index = meshInst->mStartIndex; index < lastIndex; index += 3)
		{
			int32_t index0 = mesh->mIndices[index + 0];
			int32_t index1 = mesh->mIndices[index + 1];
			int32_t index2 = mesh->mIndices[index + 2];

			vec3 v0 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index0]), 1.f)).getXYZ();
			vec3 v1 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index1]), 1.f)).getXYZ();
			vec3 v2 = (meshWorldMat * vec4(
				SceneVertexPos::ToVec3(mesh->mPositions[meshInst->mStartVertex + index2]), 1.f)).getXYZ();

			//TODO: multiply normal by world mat
			vec3 n0 = mesh->mUncompressedNormals[meshInst->mStartVertex + index0];
			vec3 n1 = mesh->mUncompressedNormals[meshInst->mStartVertex + index1];
			vec3 n2 = mesh->mUncompressedNormals[meshInst->mStartVertex + index2];


			vec2 UV0 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index0];
			vec2 UV1 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index1];
			vec2 UV2 = mesh->mUncompressedTexCoords[meshInst->mStartVertex + index2];

			bvhTree->mBBOXDataList.push_back(BVHAABBox());
			BVHAABBox& bvhAABBOX = bvhTree->mBBOXDataList.back();

			bvhAABBOX.mTriangle.Init(v0, v1, v2, n0, n1, n2, UV0, UV1, UV2);
			bvhAABBOX.mTriangle.m_pMeshInstance = meshInst;
			bvhAABBOX.Expand(v0);
			bvhAABBOX.Expand(v1);
			bvhAABBOX.Expand(v2);

			bvhAABBOX.InstanceID = 0;
		}
	}
}



void SortAlongAxis(BVHTree* bvhTree, int32_t begin, int32_t end, int32_t axis)
{
	BVHAABBox* data = bvhTree->mBBOXDataList.data() + begin;
	int32_t     count = end - begin + 1;

	if (axis == 0)
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[0];
		float midPointB = arg2->Center[0];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
	else if (axis == 1)
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[1];
		float midPointB = arg2->Center[1];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
	else
		std::qsort(data, count, sizeof(BVHAABBox), [](const void* a, const void* b) {
		const BVHAABBox* arg1 = static_cast<const BVHAABBox*>(a);
		const BVHAABBox* arg2 = static_cast<const BVHAABBox*>(b);

		float midPointA = arg1->Center[2];
		float midPointB = arg2->Center[2];

		if (midPointA < midPointB)
			return -1;
		else if (midPointA > midPointB)
			return 1;

		return 0;
	});
}


float CalculateSurfaceArea(const BVHAABBox& bbox)
{
	vec3 extents = bbox.mAABB.maxBounds - bbox.mAABB.minBounds;
	return (extents[0] * extents[1] + extents[1] * extents[2] + extents[2] * extents[0]) * 2.f;
}

void FindBestSplit(BVHTree* bvhTree,  int32_t begin, int32_t end, int32_t& split, int32_t& axis, float& splitCost)
{
	int32_t count = end - begin + 1;
	int32_t bestSplit = begin;
	int32_t globalBestSplit = begin;
	splitCost = FLT_MAX;

	split = begin;
	axis = 0;

	for (int32_t i = 0; i < 3; i++)
	{
		SortAlongAxis(bvhTree, begin, end, i);

		BVHAABBox boundsLeft;
		BVHAABBox boundsRight;

		for (int32_t indexLeft = 0; indexLeft < count; ++indexLeft)
		{
			int32_t indexRight = count - indexLeft - 1;

			
			boundsLeft.Expand(bvhTree->mBBOXDataList[begin + indexLeft].mAABB.minBounds);
			boundsLeft.Expand(bvhTree->mBBOXDataList[begin + indexLeft].mAABB.maxBounds);

			
			boundsRight.Expand(bvhTree->mBBOXDataList[begin + indexRight].mAABB.minBounds);
			boundsRight.Expand(bvhTree->mBBOXDataList[begin + indexRight].mAABB.maxBounds);


			float surfaceAreaLeft = CalculateSurfaceArea(boundsLeft);
			float surfaceAreaRight = CalculateSurfaceArea(boundsRight);

			bvhTree->mBBOXDataList[begin + indexLeft].SurfaceAreaLeft = surfaceAreaLeft;
			bvhTree->mBBOXDataList[begin + indexRight].SurfaceAreaRight = surfaceAreaRight;
		}

		float bestCost = FLT_MAX;
		for (int32_t mid = begin + 1; mid <= end; ++mid)
		{
			float surfaceAreaLeft = bvhTree->mBBOXDataList[mid - 1].SurfaceAreaLeft;
			float surfaceAreaRight = bvhTree->mBBOXDataList[mid].SurfaceAreaRight;

			int32_t countLeft = mid - begin;
			int32_t countRight = end - mid;

			float costLeft = surfaceAreaLeft * (float)countLeft;
			float costRight = surfaceAreaRight * (float)countRight;

			float cost = costLeft + costRight;
			if (cost < bestCost)
			{
				bestSplit = mid;
				bestCost = cost;
			}
		}

		if (bestCost < splitCost)
		{
			split = bestSplit;
			splitCost = bestCost;
			axis = i;
		}
	}
}
void CalculateBounds(BVHTree* bvhTree, int32_t begin, int32_t end, vec3& outMinBounds, vec3& outMaxBounds)
{
	outMinBounds = vec3(FLT_MAX);
	outMaxBounds = vec3(-FLT_MAX);

	for (int32_t i = begin; i <= end; ++i)
	{
		const vec3& memberMinBounds = bvhTree->mBBOXDataList[i].mAABB.minBounds;
		const vec3& memberMaxBounds = bvhTree->mBBOXDataList[i].mAABB.maxBounds;
		outMinBounds = vec3(
			fmin(memberMinBounds.getX(), outMinBounds.getX()),
			fmin(memberMinBounds.getY(), outMinBounds.getY()),
			fmin(memberMinBounds.getZ(), outMinBounds.getZ()));

		outMaxBounds = vec3(
			fmax(memberMaxBounds.getX(), outMaxBounds.getX()),
			fmax(memberMaxBounds.getY(), outMaxBounds.getY()),
			fmax(memberMaxBounds.getZ(), outMaxBounds.getZ())
		);
	}
}




BVHNode* CreateBVHNodeSHA(BVHTree* bvhTree, int32_t begin, int32_t end, float parentSplitCost)
{
	int32_t count = end - begin + 1;

	vec3 minBounds;
	vec3 maxBounds;

	CalculateBounds(bvhTree, begin, end, minBounds, maxBounds);

	BVHNode* node = (BVHNode*)conf_placement_new<BVHNode>(conf_calloc(1, sizeof(BVHNode)));


	++bvhTree->mBVHNodeCount;

	node->BoundingBox.Expand(minBounds);
	node->BoundingBox.Expand(maxBounds);

	if (count == 1)
	{
		//this is a leaf node
		node->Left = NULL;
		node->Right = NULL;

		node->BoundingBox.InstanceID = bvhTree->mBBOXDataList[begin].InstanceID;
		node->BoundingBox.mTriangle = bvhTree->mBBOXDataList[begin].mTriangle;
	}
	else
	{
		++bvhTree->mTransitionNodeCount;

		int32_t   split;
		int32_t   axis;
		float splitCost;

		//find the best axis to sort along and where the split should be according to SAH
		FindBestSplit(bvhTree, begin, end, split, axis, splitCost);

		//sort along that axis
		SortAlongAxis(bvhTree, begin, end, axis);

		//create the two branches
		node->Left = CreateBVHNodeSHA(bvhTree, begin, split - 1, splitCost);
		node->Right = CreateBVHNodeSHA(bvhTree, split, end, splitCost);

		//Access the child with the largest probability of collision first.
		float surfaceAreaLeft = CalculateSurfaceArea(node->Left->BoundingBox);
		float surfaceAreaRight = CalculateSurfaceArea(node->Right->BoundingBox);

		if (surfaceAreaRight > surfaceAreaLeft)
		{
			BVHNode* temp = node->Right;
			node->Right = node->Left;
			node->Left = temp;
		}


		//this is an intermediate Node
		node->BoundingBox.InstanceID = -1;
	}

	return node;
}

void DeleteBVHTree(BVHNode* node)
{
	if (node)
	{
		if (node->Left)
		{
			DeleteBVHTree(node->Left);
		}

		if (node->Right)
		{
			DeleteBVHTree(node->Right);
		}

		node->~BVHNode();
		conf_free(node);
	}
}


struct SDFTextureLayoutNode
{
	SDFTextureLayoutNode(const ivec3& nodeCoord, const ivec3& nodeSize)
		:mNodeCoord(nodeCoord),
		mNodeSize(nodeSize)
	{
	}
	//node coord not in texel space but in raw volume dimension space
	ivec3 mNodeCoord;
	ivec3 mNodeSize;
	bool mUsed;
};

struct SDFVolumeTextureAtlasLayout
{
	SDFVolumeTextureAtlasLayout(const ivec3& atlasLayoutSize)
		:mAtlasLayoutSize(atlasLayoutSize)
	{
		mAllocationCoord = ivec3(-SDF_MAX_VOXEL_ONE_DIMENSION_X, 0, SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z * 3);
		mDoubleAllocationCoord = ivec3(-SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X, 0, 0);
	}


	bool AddNewNode(const ivec3& volumeDimension, ivec3& outCoord)
	{
		if (volumeDimension.getX() <= SDF_MAX_VOXEL_ONE_DIMENSION_X &&
			volumeDimension.getY() <= SDF_MAX_VOXEL_ONE_DIMENSION_Y &&
			volumeDimension.getZ() <= SDF_MAX_VOXEL_ONE_DIMENSION_Z)
		{
			return AddNormalNode(volumeDimension, outCoord);
		}
		return AddDoubleNode(volumeDimension, outCoord);
	}

	bool AddNormalNode(const ivec3& volumeDimension, ivec3& outCoord)
	{
		if ((mAllocationCoord.getX() + (SDF_MAX_VOXEL_ONE_DIMENSION_X * 2)) <= mAtlasLayoutSize.getX())
		{
			mAllocationCoord.setX(mAllocationCoord.getX() + SDF_MAX_VOXEL_ONE_DIMENSION_X);
			mNodes.push_back(SDFTextureLayoutNode(
				mAllocationCoord, volumeDimension));

		}
		else if ((mAllocationCoord.getY() + (SDF_MAX_VOXEL_ONE_DIMENSION_Y * 2)) <= mAtlasLayoutSize.getY())
		{
			mAllocationCoord.setX(0);
			mAllocationCoord.setY(mAllocationCoord.getY() + SDF_MAX_VOXEL_ONE_DIMENSION_Y);

			mNodes.push_back(SDFTextureLayoutNode(
				mAllocationCoord, volumeDimension));
		}
		else if ((mAllocationCoord.getZ() + (SDF_MAX_VOXEL_ONE_DIMENSION_Z * 2)) <= mAtlasLayoutSize.getZ())
		{
			mAllocationCoord.setX(0);
			mAllocationCoord.setY(0);
			mAllocationCoord.setZ(mAllocationCoord.getZ() + SDF_MAX_VOXEL_ONE_DIMENSION_Z);

			mNodes.push_back(SDFTextureLayoutNode(
				mAllocationCoord, volumeDimension));
		}
		else
		{
			return false;
		}
		outCoord = mNodes.back().mNodeCoord;
		return true;
	}
	bool AddDoubleNode(const ivec3& volumeDimension, ivec3& outCoord)
	{
		if ((mDoubleAllocationCoord.getX() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X * 2)) <= mAtlasLayoutSize.getX())
		{
			mDoubleAllocationCoord.setX(mDoubleAllocationCoord.getX() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X);
			mNodes.push_back(SDFTextureLayoutNode(
				mDoubleAllocationCoord, volumeDimension));

		}
		else if ((mDoubleAllocationCoord.getY() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y * 2)) <= mAtlasLayoutSize.getY())
		{
			mDoubleAllocationCoord.setX(0);
			mDoubleAllocationCoord.setY(mDoubleAllocationCoord.getY() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y);

			mNodes.push_back(SDFTextureLayoutNode(
				mDoubleAllocationCoord, volumeDimension));
		}
		else if ((mDoubleAllocationCoord.getZ() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z * 2)) <= mAtlasLayoutSize.getZ())
		{
			mDoubleAllocationCoord.setX(0);
			mDoubleAllocationCoord.setY(0);
			mDoubleAllocationCoord.setZ(mDoubleAllocationCoord.getZ() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);

			mNodes.push_back(SDFTextureLayoutNode(
				mDoubleAllocationCoord, volumeDimension));
		}

		else
		{
			return false;
		}
		outCoord = mNodes.back().mNodeCoord;
		return true;
	}



	ivec3 mAtlasLayoutSize;
	eastl::vector<SDFTextureLayoutNode> mNodes;

	ivec3 mAllocationCoord;
	ivec3 mDoubleAllocationCoord;

};


struct SDFVolumeTextureNode
{
	SDFVolumeTextureNode(SDFVolumeData* sdfVolumeData, SDFMesh* mainMesh, SDFMeshInstance* meshInstance)
		:mSDFVolumeData(sdfVolumeData),
		mAtlasAllocationCoord(-1, -1, -1),
		mMainMesh(mainMesh),
		mMeshInstance(meshInstance),
		mHasBeenAdded(false)
	{
	}


	SDFVolumeData* mSDFVolumeData;
	SDFMesh* mMainMesh;
	SDFMeshInstance* mMeshInstance;

	//the coordinate of this node inside the volume texture atlases
	//not in texel space
	ivec3 mAtlasAllocationCoord;
	bool mHasBeenAdded;
};





struct SDFVolumeData
{
	typedef eastl::vector<vec3> SampleDirectionsList;
	typedef eastl::vector<float> SDFVolumeList;
	typedef eastl::vector<Triangle> TriangeList;

	SDFVolumeData(SDFMesh* mainMesh, SDFMeshInstance* meshInstance)
		:mSDFVolumeSize(0),
		mLocalBoundingBox(),
		mDistMinMax(FLT_MAX, FLT_MIN),
		mIsTwoSided(false),
		mSDFVolumeTextureNode(this, mainMesh, meshInstance),
		mTwoSidedWorldSpaceBias(0.f),
		mSubMeshName("")
	{

	}
	SDFVolumeData()
		:mSDFVolumeSize(0),
		mLocalBoundingBox(),
		mDistMinMax(FLT_MAX, FLT_MIN),
		mIsTwoSided(false),
		mSDFVolumeTextureNode(this, NULL, NULL),
		mTwoSidedWorldSpaceBias(0.f),
		mSubMeshName("")
	{

	}

	~SDFVolumeData() {}

	//
	SDFVolumeList mSDFVolumeList;
	//
	//Size of the distance volume
	ivec3 mSDFVolumeSize;
	//
	//Local Space of the Bounding Box volume
	AABB mLocalBoundingBox;

	//stores the min & the maximum distances found in the volume
	//in the space of the world voxel volume
	//x stores the minimum while y stores the maximum
	vec2 mDistMinMax;
	//
	bool mIsTwoSided;
	//
	float mTwoSidedWorldSpaceBias;

	SDFVolumeTextureNode mSDFVolumeTextureNode;

	eastl::string mSubMeshName;
};

struct SDFVolumeTextureAtlas
{
	SDFVolumeTextureAtlas(const ivec3& atlasSize)
		:mSDFVolumeAtlasLayout(atlasSize)
	{
	}

	void AddVolumeTextureNode(SDFVolumeTextureNode* volumeTextureNode)
	{
		ivec3 atlasCoord = volumeTextureNode->mAtlasAllocationCoord;

		if (volumeTextureNode->mHasBeenAdded)
		{
			return;
		}

		mSDFVolumeAtlasLayout.AddNewNode(volumeTextureNode->mSDFVolumeData->mSDFVolumeSize,
			volumeTextureNode->mAtlasAllocationCoord);
		mPendingNodeQueue.push(volumeTextureNode);
		volumeTextureNode->mHasBeenAdded = true;
	}

	SDFVolumeTextureNode* ProcessQueuedNode()
	{
		if (mPendingNodeQueue.empty())
		{
			return NULL;
		}

		SDFVolumeTextureNode* node = mPendingNodeQueue.front();
		mCurrentNodeList.push_back(node);
		mPendingNodeQueue.pop();
		return node;
	}

	SDFVolumeTextureAtlasLayout mSDFVolumeAtlasLayout;

	eastl::queue<SDFVolumeTextureNode*> mPendingNodeQueue;
	eastl::vector<SDFVolumeTextureNode*> mCurrentNodeList;
};

float GenerateRandomFloat()
{
	return (float)rand() / (float)RAND_MAX;
}

void GenerateSampleDirections(int32_t thetaSteps, int32_t phiSteps, 
	SDFVolumeData::SampleDirectionsList& outDirectionsList, int32_t finalThetaModifier = 1)
{

	for (int32_t theta = 0; theta < thetaSteps; ++theta)
	{
		for (int32_t phi = 0; phi < phiSteps; ++phi)
		{
			float random1 = GenerateRandomFloat();
			float random2 = GenerateRandomFloat();

			float thetaFrac = (theta + random1) / (float)thetaSteps;
			float phiFrac = (phi + random2) / (float)phiSteps;

			float rVal = sqrt(1.0f - thetaFrac * thetaFrac);

			const float finalPhi = 2.0f * (float)PI * phiFrac;

			outDirectionsList.push_back(vec3(cos(finalPhi) * rVal,
				sin(finalPhi) * rVal, thetaFrac * finalThetaModifier));
		}
	}
}


struct CalculateMeshSDFTask
{
	const SDFVolumeData::SampleDirectionsList* mDirectionsList;
	const SDFVolumeData::TriangeList* mMeshTrianglesList;
	const AABB* mSDFVolumeBounds;
	const ivec3*  mSDFVolumeDimension;
	int32_t mZIndex;
	float mSDFVolumeMaxDist;
	BVHTree* mBVHTree;
	SDFVolumeData::SDFVolumeList* mSDFVolumeList;
	bool mIsTwoSided;
};


void DoCalculateMeshSDFTask(void* dataPtr, uintptr_t index)
{
	CalculateMeshSDFTask* task = (CalculateMeshSDFTask*)(dataPtr);

	const AABB& sdfVolumeBounds = *task->mSDFVolumeBounds;
	const ivec3& sdfVolumeDimension = *task->mSDFVolumeDimension;
	int32_t zIndex = task->mZIndex;
	float sdfVolumeMaxDist = task->mSDFVolumeMaxDist;

	const SDFVolumeData::SampleDirectionsList& directionsList = *task->mDirectionsList;
	//const SDFVolumeData::TriangeList& meshTrianglesList = *task->mMeshTrianglesList;

	SDFVolumeData::SDFVolumeList& sdfVolumeList = *task->mSDFVolumeList;

	BVHTree* bvhTree = task->mBVHTree;


	vec3 floatSDFVolumeDimension = vec3((float)sdfVolumeDimension.getX(), (float)sdfVolumeDimension.getY(), (float)sdfVolumeDimension.getZ());
	
	vec3 sdfVolumeBoundsSize = calculateAABBSize(&sdfVolumeBounds);
	
	vec3 sdfVoxelSize
	(
		sdfVolumeBoundsSize.getX() / sdfVolumeDimension.getX(),
		sdfVolumeBoundsSize.getY() / sdfVolumeDimension.getY(), 
		sdfVolumeBoundsSize.getZ() / sdfVolumeDimension.getZ()
	);

	float voxelDiameterSquared = dot(sdfVoxelSize, sdfVoxelSize);

	for (int32_t yIndex = 0; yIndex < sdfVolumeDimension.getY(); ++yIndex)
	{
		for (int32_t xIndex = 0; xIndex < sdfVolumeDimension.getX(); ++xIndex)
		{
			vec3 offsettedIndex = vec3((float)(xIndex)+0.5f, float(yIndex) + 0.5f, float(zIndex) + 0.5f);

			vec3 voxelPos = vec3(offsettedIndex.getX() * sdfVoxelSize.getX(),
				offsettedIndex.getY() * sdfVoxelSize.getY(), offsettedIndex.getZ() * sdfVoxelSize.getZ()) + sdfVolumeBounds.minBounds;

			int32 outIndex = (zIndex * sdfVolumeDimension.getY() *
				sdfVolumeDimension.getX() + yIndex * sdfVolumeDimension.getX() + xIndex);


			float minDistance = sdfVolumeMaxDist;
			int32 hit = 0;
			int32 hitBack = 0;

			for (int32_t sampleIndex = 0; sampleIndex < directionsList.size(); ++sampleIndex)
			{
				vec3 rayDir = directionsList[sampleIndex];
				vec3 endPos = voxelPos + rayDir * sdfVolumeMaxDist;

				Ray newRay(voxelPos, rayDir);

			
				bool intersectWithBbox = RayIntersectsBox(newRay.startPos, 
					newRay.GetInvDir(), sdfVolumeBounds.minBounds, sdfVolumeBounds.maxBounds);

				//if we pass the cheap bbox testing
				if (intersectWithBbox)
				{
					Intersection meshTriangleIntersect;
					//optimized version
					BVHTreeIntersectRayAux(bvhTree->mRootNode, bvhTree->mRootNode, newRay, meshTriangleIntersect);
					if (meshTriangleIntersect.mIsIntersected)
					{
						++hit;
						const vec3& hitNormal = meshTriangleIntersect.mHittedNormal;
						if (dot(rayDir, hitNormal) > 0 && !task->mIsTwoSided)
						{
							++hitBack;
						}

						const vec3 finalEndPos = newRay.Eval(
							meshTriangleIntersect.mIntersection_TVal);

						float newDist = length(newRay.startPos - finalEndPos);

						if (newDist < minDistance)
						{
							minDistance = newDist;
						}
					}

				}

			}

			//


			float unsignedDist = minDistance;

			//if 50% hit backface, we consider the voxel sdf value to be inside the mesh
			minDistance *= (hit == 0 || hitBack < (directionsList.size() * 0.5f)) ? 1 : -1;

			//if we are very close to the surface and 95% of our rays hit backfaces, the sdf value
			//is inside the mesh
			if ((unsignedDist * unsignedDist) < voxelDiameterSquared && hitBack > 0.95f * hit)
			{
				minDistance = -unsignedDist;
			}

			minDistance = fmin(minDistance, sdfVolumeMaxDist);
			//float maxExtent = fmax(fmax(sdfVolumeBounds.GetExtent().getX(),
				//sdfVolumeBounds.GetExtent().getY()), sdfVolumeBounds.GetExtent().getZ());
			vec3 sdfVolumeBoundsExtent = calculateAABBExtent(&sdfVolumeBounds);
			float maxExtent = maxElem(sdfVolumeBoundsExtent);
			
			float volumeSpaceDist = minDistance / maxExtent;



			sdfVolumeList[outIndex] = volumeSpaceDist;
		}
	}

}


bool GenerateVolumeDataFromFile(SDFVolumeData** outVolumeDataPP, const Path* filePath, const eastl::string& meshName,
	float twoSidedWorldSpaceBias)
{
    FileStream* newBakedFile = fsOpenFile(filePath, FM_READ_BINARY);
	if (!newBakedFile)
	{
		return false;
	}


	*outVolumeDataPP = conf_new(SDFVolumeData);
	SDFVolumeData& outVolumeData = **outVolumeDataPP;
	outVolumeData.mSubMeshName = meshName;

	outVolumeData.mSDFVolumeSize.setX(fsReadFromStreamInt32(newBakedFile));
	outVolumeData.mSDFVolumeSize.setY(fsReadFromStreamInt32(newBakedFile));
	outVolumeData.mSDFVolumeSize.setZ(fsReadFromStreamInt32(newBakedFile));

	uint32_t finalSDFVolumeDataCount = outVolumeData.mSDFVolumeSize.getX() * outVolumeData.mSDFVolumeSize.getY()
		* outVolumeData.mSDFVolumeSize.getZ();

	outVolumeData.mSDFVolumeList.resize(finalSDFVolumeDataCount);

    fsReadFromStream(newBakedFile, &outVolumeData.mSDFVolumeList[0], finalSDFVolumeDataCount * sizeof(float));
	outVolumeData.mLocalBoundingBox.minBounds = f3Tov3(fsReadFromStreamFloat3(newBakedFile));
	outVolumeData.mLocalBoundingBox.maxBounds = f3Tov3(fsReadFromStreamFloat3(newBakedFile));
    outVolumeData.mIsTwoSided = fsReadFromStreamBool(newBakedFile);
	//outVolumeData.mTwoSidedWorldSpaceBias = newBakedFile.ReadFloat();
	outVolumeData.mTwoSidedWorldSpaceBias = twoSidedWorldSpaceBias;
	/*
	only uses the minimum & maximum of SDF if we ever want to quantized the SDF data
	for (int32 index = 0; index < outVolumeData.mSDFVolumeList.size(); ++index)
	{
		const float volumeSpaceDist = outVolumeData.mSDFVolumeList[index];
		outVolumeData.mDistMinMax.setX(fmin(volumeSpaceDist, outVolumeData.mDistMinMax.getX()));
		outVolumeData.mDistMinMax.setY(fmax(volumeSpaceDist, outVolumeData.mDistMinMax.getY()));
	}*/
    fsCloseStream(newBakedFile);

	LOGF(LogLevel::eINFO, "SDF binary data for %s found & parsed", fsGetPathAsNativeString(filePath));
	return true;
}

void GenerateVolumeDataFromMesh(ThreadSystem* threadSystem, SDFMesh* mainMesh, SDFMeshInstance* subMesh,
	float sdfResolutionScale, bool generateAsIfTwoSided, SDFVolumeData** outVolumeDataPP,
	const eastl::string& subMeshName, float twoSidedWorldSpaceBias = 0.4f,
	const ivec3& specialMaxVoxelValue = ivec3(0))
{
	PathHandle newCompleteCacheFilePath = GetSDFBakedFilePath(subMeshName);

	if (GenerateVolumeDataFromFile(outVolumeDataPP, newCompleteCacheFilePath, subMeshName, twoSidedWorldSpaceBias))
	{
		return;
	}

	LOGF(LogLevel::eINFO, "Generating SDF binary data for %s", fsGetPathAsNativeString(newCompleteCacheFilePath));

	*outVolumeDataPP = conf_new(SDFVolumeData, mainMesh, subMesh);

	SDFVolumeData& outVolumeData = **outVolumeDataPP;
	outVolumeData.mSubMeshName = subMeshName;

	//for now assume all triangles are valid and useable
	ivec3 maxNumVoxelsOneDimension;
	ivec3 minNumVoxelsOneDimension;

	if (specialMaxVoxelValue.getX() == 0.f || specialMaxVoxelValue.getY() == 0.f || specialMaxVoxelValue.getZ() == 0.f)
	{
		maxNumVoxelsOneDimension = ivec3(
			SDF_MAX_VOXEL_ONE_DIMENSION_X,
			SDF_MAX_VOXEL_ONE_DIMENSION_Y,
			SDF_MAX_VOXEL_ONE_DIMENSION_Z);
		minNumVoxelsOneDimension = ivec3(
			SDF_MIN_VOXEL_ONE_DIMENSION_X,
			SDF_MIN_VOXEL_ONE_DIMENSION_Y,
			SDF_MIN_VOXEL_ONE_DIMENSION_Z);
	}
	else
	{
		maxNumVoxelsOneDimension = specialMaxVoxelValue;
		minNumVoxelsOneDimension = specialMaxVoxelValue;
	}


	const float voxelDensity = 1.0f;

	const float numVoxelPerLocalSpaceUnit = voxelDensity * sdfResolutionScale;

	AABB subMeshBBox = subMesh->mLocalBoundingBox;


	vec3 subMeshExtent = 0.5f * (subMeshBBox.maxBounds - subMeshBBox.minBounds);

	float maxExtentSize = maxElem(subMeshExtent);

	vec3 minNewExtent(0.2f* maxExtentSize);
	vec3 standardExtentSize = 4.f * subMeshExtent;
	vec3 dynamicNewExtent(standardExtentSize.getX() / minNumVoxelsOneDimension.getX(),
		standardExtentSize.getY() / minNumVoxelsOneDimension.getY(), standardExtentSize.getZ() / minNumVoxelsOneDimension.getZ());

	vec3 finalNewExtent = subMeshExtent + vec3( fmax(minNewExtent.getX(), dynamicNewExtent.getX()),
		fmax(minNewExtent.getY(), dynamicNewExtent.getY()), fmax(minNewExtent.getZ(), dynamicNewExtent.getZ()));

	vec3 subMeshBBoxCenter = (subMeshBBox.maxBounds + subMeshBBox.minBounds) * 0.5f;

	AABB newSDFVolumeBound;
	newSDFVolumeBound.minBounds = subMeshBBoxCenter - finalNewExtent;
	newSDFVolumeBound.maxBounds = subMeshBBoxCenter + finalNewExtent;

	vec3 newSDFVolumeBoundSize = newSDFVolumeBound.maxBounds - newSDFVolumeBound.minBounds;
	vec3 newSDFVolumeBoundExtent = 0.5f * (newSDFVolumeBound.maxBounds - newSDFVolumeBound.minBounds);

	float newSDFVolumeMaxDistance = length(newSDFVolumeBoundExtent);
	vec3 dynamicDimension = vec3(
		newSDFVolumeBoundSize.getX() * numVoxelPerLocalSpaceUnit,
		newSDFVolumeBoundSize.getY() * numVoxelPerLocalSpaceUnit,
		newSDFVolumeBoundSize.getZ() * numVoxelPerLocalSpaceUnit);


	ivec3 finalSDFVolumeDimension
	(
		clamp((int32_t)(dynamicDimension.getX()), minNumVoxelsOneDimension.getX(), maxNumVoxelsOneDimension.getX()),
		clamp((int32_t)(dynamicDimension.getY()), minNumVoxelsOneDimension.getY(), maxNumVoxelsOneDimension.getY()),
		clamp((int32_t)(dynamicDimension.getZ()), minNumVoxelsOneDimension.getZ(), maxNumVoxelsOneDimension.getZ())
	);

	uint32_t finalSDFVolumeDataCount = finalSDFVolumeDimension.getX() *
		finalSDFVolumeDimension.getY() *
		finalSDFVolumeDimension.getZ();

	outVolumeData.mSDFVolumeList.resize
	(
		finalSDFVolumeDimension.getX() *
		finalSDFVolumeDimension.getY() *
		finalSDFVolumeDimension.getZ()
	);

	BVHTree bvhTree;
	AddMeshInstanceToBBOX(&bvhTree, mat4::identity(), mainMesh, subMesh);
	bvhTree.mRootNode = CreateBVHNodeSHA(&bvhTree, 0, (int32_t)bvhTree.mBBOXDataList.size() - 1, FLT_MAX);


	// here we begin our stratified sampling calculation
	const uint32_t numVoxelDistanceSample = SDF_STRATIFIED_DIRECTIONS_NUM;

	SDFVolumeData::SampleDirectionsList sampleDirectionsList;

	int32_t thetaStep = (int32_t)floor((sqrt((float)numVoxelDistanceSample / (PI * 2.f))));
	int32 phiStep = (int32_t)floor((float)thetaStep * PI);


	sampleDirectionsList.reserve(thetaStep * phiStep * 2);

	GenerateSampleDirections(thetaStep, phiStep, sampleDirectionsList);

	SDFVolumeData::SampleDirectionsList otherHemisphereSampleDirectionList;
	GenerateSampleDirections(thetaStep, phiStep, sampleDirectionsList, -1);


	CalculateMeshSDFTask calculateMeshSDFTask = {};
	calculateMeshSDFTask.mDirectionsList = &sampleDirectionsList;
	calculateMeshSDFTask.mSDFVolumeBounds = &newSDFVolumeBound;
	calculateMeshSDFTask.mSDFVolumeDimension = &finalSDFVolumeDimension;
	calculateMeshSDFTask.mSDFVolumeMaxDist = newSDFVolumeMaxDistance;
	calculateMeshSDFTask.mSDFVolumeList = &outVolumeData.mSDFVolumeList;
	calculateMeshSDFTask.mBVHTree = &bvhTree;
	calculateMeshSDFTask.mIsTwoSided = generateAsIfTwoSided;


	for (int32_t zIndex = 0; zIndex < finalSDFVolumeDimension.getZ(); ++zIndex)
	{
		calculateMeshSDFTask.mZIndex = zIndex;
		DoCalculateMeshSDFTask(&calculateMeshSDFTask, 0);
	}

	DeleteBVHTree(bvhTree.mRootNode);

    FileStream* portDataFile = fsOpenFile(newCompleteCacheFilePath, FM_WRITE_BINARY);
    fsWriteToStreamInt32(portDataFile, finalSDFVolumeDimension.getX());
    fsWriteToStreamInt32(portDataFile, finalSDFVolumeDimension.getY());
    fsWriteToStreamInt32(portDataFile, finalSDFVolumeDimension.getZ());
    fsWriteToStream(portDataFile, &outVolumeData.mSDFVolumeList[0], finalSDFVolumeDataCount * sizeof(float));
    fsWriteToStreamFloat3(portDataFile, v3ToF3(newSDFVolumeBound.minBounds));
    fsWriteToStreamFloat3(portDataFile, v3ToF3(newSDFVolumeBound.maxBounds));
    fsWriteToStreamBool(portDataFile, generateAsIfTwoSided);
	//portDataFile.WriteFloat(twoSidedWorldSpaceBias);
    fsCloseStream(portDataFile);

	float minVolumeDist = 1.0f;
	float maxVolumeDist = -1.0f;

	//we can probably move the calculation of the minimum & maximum distance of the SDF value
	//into the CalculateMeshSDFValue function
	for (int32 index = 0; index < outVolumeData.mSDFVolumeList.size(); ++index)
	{
		const float volumeSpaceDist = outVolumeData.mSDFVolumeList[index];
		minVolumeDist = fmin(volumeSpaceDist, minVolumeDist);
		maxVolumeDist = fmax(volumeSpaceDist, maxVolumeDist);
	}

	//TODO, not every mesh is going to be closed
	//do the check sometime in the future
	outVolumeData.mIsTwoSided = generateAsIfTwoSided;
	outVolumeData.mSDFVolumeSize = finalSDFVolumeDimension;
	outVolumeData.mLocalBoundingBox = newSDFVolumeBound;
	outVolumeData.mDistMinMax = vec2(minVolumeDist, maxVolumeDist);
	outVolumeData.mTwoSidedWorldSpaceBias = twoSidedWorldSpaceBias;
}


void generateMissingSDF(ThreadSystem* threadSystem, SDFMesh* sdfMesh, BakedSDFVolumeInstances& sdfVolumeInstances)
{
	const CustomSDFSubMeshDataList& customList = sdfMesh->mCustomSubMeshDataList;

	for (uint32_t i = 0; i < customList.size(); ++i)
	{
		const SDFCustomSubMeshData& customSubMesh = customList[i];
		if (!customSubMesh.mIsSDFMesh)
		{
			continue;
		}
		SDFMeshInstance& sdfMeshInstance = sdfMesh->mMeshInstances[i];

		if (sdfMeshInstance.mHasGeneratedSDFVolumeData)
		{
			continue;
		}

		bool twoSided = customSubMesh.mIsTwoSided;

		SDFVolumeData* volumeData = NULL;

		ivec3 specialVoxelSizeValue(0);

		if (customSubMesh.mUseDoubleVoxelSize)
		{
			specialVoxelSizeValue = ivec3(
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X,
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y,
				SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);
		}
		GenerateVolumeDataFromMesh(threadSystem, sdfMesh,
			&sdfMeshInstance, 1.f, twoSided, &volumeData,
			customSubMesh.mMeshName, customSubMesh.mTwoSidedWorldSpaceBias, specialVoxelSizeValue);

		if (volumeData)
		{
			sdfVolumeInstances.push_back(volumeData);
			sdfMeshInstance.mHasGeneratedSDFVolumeData = true;
			++sdfMesh->mTotalGeneratedSDFMeshes;
		}
	}
}


SDFVolumeTextureAtlas* pSDFVolumeTextureAtlas = NULL;

UpdateSDFVolumeTextureAtlasConstants gUpdateSDFVolumeTextureAtlasConstants = {};
MeshSDFConstants gMeshSDFConstants = {};

BakedSDFVolumeInstances gSDFVolumeInstances;




struct GenerateMissingSDFTaskData
{
	ThreadSystem* pThreadSystem;
	SDFMesh* pSDFMesh;
	BakedSDFVolumeInstances* sdfVolumeInstances;
};


void DoGenerateMissingSDFTaskData(void* dataPtr, uintptr_t index)
{
	GenerateMissingSDFTaskData* taskData = (GenerateMissingSDFTaskData*)(dataPtr);
	generateMissingSDF(taskData->pThreadSystem, taskData->pSDFMesh, *taskData->sdfVolumeInstances);
}

const char* gSDFModelNames[3] = { "SanMiguel_Opaque.obj", "SanMiguel_AlphaTested.obj", "SanMiguel_Flags.obj" };
SDFMesh*        pSDFMeshes[3] = {};
GenerateMissingSDFTaskData gGenerateMissingSDFTask[3] = {};
size_t gSDFProgressValue = 0;



/*--------------------ASM main logic-------------*/

#define SQR(a) ( (a) * (a) )

static const float lightDirUpdateThreshold = 0.999f;
static const float maxWarpAngleCos = 0.994f;
static uint32_t asmFrameCounter;



class QuadTreeNode;
class ASMFrustum;
class ASMTileCache;

vec3 Project(const vec3& v, float w, const mat4& projMat)
{
	vec4 newV = projMat * vec4(v, w);
	return vec3(newV.getX() / newV.getW(), newV.getY() / newV.getW(), newV.getZ() / newV.getW());
}

float GetRefinementDistanceSq(const AABB& BBox, const vec2& refinementPos)
{
	vec3 bboxCenter = calculateAABBCenter(&BBox);
	return lengthSqr(vec2(bboxCenter.getX(),
		bboxCenter.getY()) - refinementPos);
}

float Get3DRefinementDistanceSq(const AABB& BBox, const vec2& refinementPos)
{
	vec3 bboxCenter = calculateAABBCenter(&BBox);
	return lengthSqr((bboxCenter) - vec3(refinementPos.getX(), refinementPos.getY(), 0.f));
}


struct ASMProjectionData
{
	mat4 mViewMat;
	mat4 mInvViewMat;
	mat4 mProjMat;
	mat4 mInvProjMat;
	mat4 mViewProjMat;
	mat4 mInvViewProjMat;
};



class ConvexHull2D
{
public:
	static const uint32_t MAX_VERTICES = 5;
	vec2 m_vertices[MAX_VERTICES] = { vec2(0.f), vec2(0.f), vec2(0.f), vec2(0.f), vec2(0.f) };
	int32_t m_size;

	ConvexHull2D()
	{
		Reset();
	}

	ConvexHull2D(int32_t numVertices, const vec2* pVertices)
	{
		m_size = FindConvexHull(numVertices, pVertices, m_vertices);
	}

	void Reset()
	{
		m_size = 0;
	}

	const vec2 FindFrustumConvexHull(const ASMProjectionData& projection, float frustumZMaxOverride, const mat4& viewProj)
	{
		static const uint32_t numVertices = 5;
		vec3 frustumPos = projection.mInvViewMat.getCol3().getXYZ();

		vec3 projectedFrustumPos = Project(frustumPos, 1.f, viewProj);
		vec2 vertices[numVertices] = { vec2(projectedFrustumPos.getX(), projectedFrustumPos.getY()) };

		mat4 projMat = projection.mProjMat;

		float hz = Project(vec3(0, 0, frustumZMaxOverride), 1.f, projMat).getZ();

		const vec3 frustumCorners[] =
		{
			vec3(-1.0f, -1.0f, hz),
			vec3(+1.0f, -1.0f, hz),
			vec3(+1.0f, +1.0f, hz),
			vec3(-1.0f, +1.0f, hz),
		};


		mat4 tm = viewProj * projection.mInvViewProjMat;
		for (uint32_t i = 1; i < numVertices; ++i)
		{
			vec3 indxProjectedFrustumPos = Project(frustumCorners[i - 1], 1.f, tm);
			vertices[i] = vec2(indxProjectedFrustumPos.getX(), indxProjectedFrustumPos.getY());
		}

		m_size = FindConvexHull(numVertices, vertices, m_vertices);


		return vertices[0];
	}

	bool Intersects(const AABB& BBox) const
	{
		if (m_size == 0)
		{
			return false;
		}

		static const vec2 normals[] =
		{
			vec2(1, 0),
			vec2(0, 1),
			vec2(-1, 0),
			vec2(0,-1),
		};

		vec2 vb1[MAX_VERTICES * 2];
		vec2 vb2[MAX_VERTICES * 2];

		const vec2* v = m_vertices;
		int32_t n = m_size;



		int32_t j, index[2];
		float d[2];
		for (int32_t i = 0; i < 4; ++i)
		{
			float pw = -dot(vec3(normals[i].getX(), normals[i].getY(), 0.f), i < 2 ? BBox.minBounds : BBox.maxBounds);
			index[1] = n - 1;
			d[1] = dot(normals[i], v[index[1]]) + pw;
			for (j = 0; j < n; j++)
			{
				index[0] = index[1];
				index[1] = j;
				d[0] = d[1];
				d[1] = dot(normals[i], v[index[1]]) + pw;
				if (d[1] > 0 && d[0] < 0) break;
			}
			if (j < n)
			{
				int32_t k = 0;
				vec2* tmp = v == vb1 ? vb2 : vb1;

				vec3 lerpedVal = lerp(
					vec3(v[index[1]].getX(), v[index[1]].getY(), 0.f),
					vec3(v[index[0]].getX(), v[index[0]].getY(), 0.f),
					d[1] / (d[1] - d[0])
				);
				tmp[k++] = vec2(lerpedVal.getX(), lerpedVal.getY());
				do
				{
					index[0] = index[1];
					index[1] = (index[1] + 1) % n;
					d[0] = d[1];
					d[1] = dot(normals[i], v[index[1]]) + pw;
					tmp[k++] = v[index[0]];
				} while (d[1] > 0);

				vec3 lerpedVal2 = lerp(
					vec3(v[index[1]].getX(), v[index[1]].getY(), 0.f),
					vec3(v[index[0]].getX(), v[index[0]].getY(), 0.f),
					d[1] / (d[1] - d[0])
				);

				tmp[k++] = vec2(lerpedVal2.getX(), lerpedVal2.getY());
				n = k;
				v = tmp;
			}
			else
			{
				if (d[1] < 0) return false;
			}
		}
		return n > 0;
	}

	static int32_t FindConvexHull(int32_t numVertices, const vec2* pVertices, vec2* pHull)
	{
		//_ASSERT(numVertices <= MAX_VERTICES);
		const float eps = 1e-5f;
		const float epsSq = eps * eps;
		int32_t leftmostIndex = 0;
		for (int32_t i = 1; i < numVertices; ++i)
		{
			float f = pVertices[leftmostIndex].getX() - pVertices[i].getX();
			if (fabsf(f) < epsSq)
			{
				if (pVertices[leftmostIndex].getY() > pVertices[i].getY())
					leftmostIndex = i;
			}
			else if (f > 0)
			{
				leftmostIndex = i;
			}
		}
		vec2 dir0(0, -1);
		int32_t hullSize = 0;
		int32_t index0 = leftmostIndex;
		do
		{
			float maxCos = -FLT_MAX;
			int32_t index1 = -1;
			vec2 dir1;
			for (int32_t j = 1; j < numVertices; ++j)
			{
				int32_t k = (index0 + j) % numVertices;
				vec2 v = pVertices[k] - pVertices[index0];


				float l = lengthSqr(v);
				if (l > epsSq)
				{
					vec2 d = normalize(v);
					float f = dot(d, dir0);
					if (maxCos < f)
					{
						maxCos = f;
						index1 = k;
						dir1 = d;
					}
				}
			}
			if (index1 < 0 || hullSize >= numVertices)
			{
				//_ASSERT(!"epic fail");
				return 0;
			}
			pHull[hullSize++] = pVertices[index1];
			index0 = index1;
			dir0 = dir1;
		} while (lengthSqr(pVertices[index0] - pVertices[leftmostIndex]) > epsSq);
		return hullSize;
	}
};

struct ASMRendererContext
{
	Renderer* m_pRenderer;
	Cmd* m_pCmd;
	GpuProfiler* m_pGpuProfiler;
};


struct ASMRenderTargets
{
	RenderTarget* m_pRenderTargetASMLodClamp;
	RenderTarget* m_pRenderTargetASMPrerenderLodClamp;

	eastl::vector<RenderTarget*> m_pASMPrerenderIndirectionMips;
	eastl::vector<RenderTarget*> m_pASMIndirectionMips;
};

struct ASMSShadowMapPrepareRenderContext
{
	const vec3* m_worldCenter;
};

struct ASMSShadowMapRenderContext
{
	ASMRendererContext* m_pRendererContext;
	ASMProjectionData* m_pASMProjectionData;
};


struct IndirectionRenderData
{
	Buffer* pBufferASMPackedIndirectionQuadsUniform[gs_ASMMaxRefinement + 1];
	Buffer* pBufferASMClearIndirectionQuadsUniform;
	Pipeline* m_pGraphicsPipeline;
	RootSignature* m_pRootSignature;


	IndirectionRenderData& operator=(const IndirectionRenderData& right)
	{
		for (int32_t i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			this->pBufferASMPackedIndirectionQuadsUniform[i] =
				right.pBufferASMPackedIndirectionQuadsUniform[i];
		}
		this->pBufferASMClearIndirectionQuadsUniform =
			right.pBufferASMClearIndirectionQuadsUniform;

		this->m_pGraphicsPipeline = right.m_pGraphicsPipeline;
		this->m_pRootSignature = right.m_pRootSignature;

		return *this;
	}
};

struct ASMTickData
{
	IndirectionRenderData mIndirectionRenderData;
	IndirectionRenderData mPrerenderIndirectionRenderData;
};

ASMTickData gASMTickData;


class CIntrusiveUnorderedSetItemHandle
{
public:
	CIntrusiveUnorderedSetItemHandle() : mIndex(-1) { }
	bool IsInserted() const { return mIndex >= 0; }

protected:
	int32_t mIndex;
};

template< class CContainer, class CItem, CIntrusiveUnorderedSetItemHandle& (CItem::*GetHandle)() >
class CIntrusiveUnorderedPtrSet
{
	struct SHandleSetter : public CIntrusiveUnorderedSetItemHandle
	{
		void Set(int32_t i) { mIndex = i; }
		void Set_Size_t(size_t i) { mIndex = static_cast<int32_t>(i); }
		int32_t Get() const { return mIndex; }
	};

public:
	void Add(CItem* pItem, bool mayBeAlreadyInserted = false)
	{
		SHandleSetter& handle = static_cast<SHandleSetter&>((pItem->*GetHandle)());
		CContainer& container = *static_cast<CContainer*>(this);
		if (handle.IsInserted())
		{

		}
		else
		{
			handle.Set_Size_t(container.size());
			container.push_back(pItem);
		}
	}
	void Remove(CItem* pItem, bool mayBeNotInserted = false)
	{
		SHandleSetter& handle = static_cast<SHandleSetter&>((pItem->*GetHandle)());
		CContainer& container = *static_cast<CContainer*>(this);
		if (handle.IsInserted())
		{
			CItem* pLastItem = container.back();
			(pLastItem->*GetHandle)() = handle;
			container[handle.Get()] = pLastItem;
			container.pop_back();
			handle.Set(-1);
		}
		else
		{

		}
	}
};

template< class CItem, CIntrusiveUnorderedSetItemHandle& (CItem::*GetHandle)() >
class CVectorBasedIntrusiveUnorderedPtrSet :
	public eastl::vector< CItem* >,
	public CIntrusiveUnorderedPtrSet< CVectorBasedIntrusiveUnorderedPtrSet< CItem, GetHandle >, CItem, GetHandle >
{
};



struct SQuad
{
	vec4 m_pos;

	static const SQuad Get(
		int32_t dstRectW, int32_t dstRectH,
		int32_t dstX, int32_t dstY, int32_t dstW, int32_t dstH)
	{
		SQuad q;

		q.m_pos.setX(float(dstRectW) / float(dstW));
		q.m_pos.setY(float(dstRectH) / float(dstH));
		q.m_pos.setZ(q.m_pos.getX() + 2.0f*float(dstX) / float(dstW) - 1.0f);
		q.m_pos.setW(1.0f - q.m_pos.getY() - 2.0f*float(dstY) / float(dstH));

		return q;
	}
};

struct SFillQuad : public SQuad
{
	vec4 m_misc;

	static const SFillQuad Get(
		const vec4& miscParams,
		int32_t dstRectW, int32_t dstRectH,
		int32_t dstX, int32_t dstY, int32_t dstW, int32_t dstH)
	{
		SFillQuad q;

		static_cast<SQuad&>(q) = SQuad::Get(dstRectW, dstRectH, dstX, dstY, dstW, dstH);

		q.m_misc = miscParams;

		return q;
	}
};

struct SCopyQuad : public SFillQuad
{
	vec4 m_texCoord;

	static const SCopyQuad Get(
		const vec4& miscParams,
		int32_t dstRectW, int32_t dstRectH,
		int32_t dstX, int32_t dstY, int32_t dstW, int32_t dstH,
		int32_t srcRectW, int32_t srcRectH,
		int32_t srcX, int32_t srcY, int32_t srcW, int32_t srcH)
	{
		SCopyQuad q;

		static_cast<SFillQuad&>(q) = SFillQuad::Get(miscParams, dstRectW, dstRectH, dstX, dstY, dstW, dstH);

		// Align with pixel center @ (0.5, 0.5).
		q.m_pos.setZ(q.m_pos.getZ() + 1.0f / float(dstW));
		q.m_pos.setW(q.m_pos.getW() - 1.0f / float(dstH));

		q.m_texCoord.setX(float(srcRectW) / float(srcW));
		q.m_texCoord.setY(float(srcRectH) / float(srcH));
		q.m_texCoord.setZ((float(srcX) + 0.5f) / float(srcW));
		q.m_texCoord.setW((float(srcY) + 0.5f) / float(srcH));

		return q;
	}
};



class ASMTileCacheEntry
{
public:
	struct SViewport { int32_t x, y, w, h; } m_viewport;
	uint8_t m_refinement;

	AABB m_BBox;
	QuadTreeNode* m_pOwner;
	ASMFrustum* m_pFrustum;
	uint32_t m_lastFrameUsed;
	uint32_t m_frustumID;
	bool m_isLayer;
	float m_fadeInFactor;

	ASMProjectionData mRenderProjectionData;

	ASMTileCacheEntry(ASMTileCache* pCache, int32_t x, int32_t y);
	~ASMTileCacheEntry();

	template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
	void Allocate(QuadTreeNode* pOwner, ASMFrustum* pFrustum);

	void Free();
	void Invalidate()
	{
		m_BBox = AABB();
		m_refinement = gs_ASMMaxRefinement;
		m_lastFrameUsed = asmFrameCounter - 0x7fFFffFF;
		m_frustumID = 0;
	}
	void MarkReady();
	void MarkNotReady();

	bool IsReady() const { return m_readyTilesPos.IsInserted(); }
	ASMTileCache* GetCache() const { return m_pCache; }
	bool IsAllocated() const { return m_pOwner != nullptr; }
	bool IsBeingUpdated() const { return m_updateQueuePos.IsInserted(); }
	bool IsQueuedForRendering() const { return m_renderQueuePos.IsInserted(); }

	static const ivec4 GetRect(const SViewport& vp, int32_t border)
	{
		return ivec4(vp.x - border,
			vp.y - border,
			vp.x + vp.w + border,
			vp.y + vp.h + border);
	}

protected:
	ASMTileCache* m_pCache;

	CIntrusiveUnorderedSetItemHandle m_tilesPos;
	CIntrusiveUnorderedSetItemHandle m_freeTilesPos;
	CIntrusiveUnorderedSetItemHandle m_renderQueuePos;
	CIntrusiveUnorderedSetItemHandle m_readyTilesPos;
	CIntrusiveUnorderedSetItemHandle m_demQueuePos;
	CIntrusiveUnorderedSetItemHandle m_renderBatchPos;
	CIntrusiveUnorderedSetItemHandle m_updateQueuePos;

	CIntrusiveUnorderedSetItemHandle& GetTilesPos() { return m_tilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetFreeTilesPos() { return m_freeTilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetRenderQueuePos() { return m_renderQueuePos; }
	CIntrusiveUnorderedSetItemHandle& GetReadyTilesPos() { return m_readyTilesPos; }
	CIntrusiveUnorderedSetItemHandle& GetDemQueuePos() { return m_demQueuePos; }
	CIntrusiveUnorderedSetItemHandle& GetRenderBatchPos() { return m_renderBatchPos; }
	CIntrusiveUnorderedSetItemHandle& GetUpdateQueuePos() { return m_updateQueuePos; }

	void PrepareRender(const ASMSShadowMapPrepareRenderContext& context);
	

	friend class ASMTileCache;
};

class ASMTileCache
{
public:
	ASMTileCache()
		:m_cacheHits(0),
		m_tileAllocs(0),
		m_numTilesRendered(0),
		m_numTilesUpdated(0),
		m_depthAtlasWidth(0),
		m_depthAtlasHeight(0),
		m_demAtlasWidth(0),
		m_demAtlasHeight(0),
		mDEMFirstTimeRender(true),
		mDepthFirstTimeRender(true)
	{
		m_depthAtlasWidth = gs_ASMDepthAtlasTextureWidth;
		m_depthAtlasHeight = gs_ASMDepthAtlasTextureHeight;

		m_demAtlasWidth = gs_ASMDEMAtlasTextureWidth;
		m_demAtlasHeight = gs_ASMDEMAtlasTextureHeight;


		int32_t gridWidth = m_depthAtlasWidth / gs_ASMTileSize;
		int32_t gridHeight = m_depthAtlasHeight / gs_ASMTileSize;

		for (int32_t i = 0; i < gridHeight; ++i)
		{
			for (int32_t j = 0; j < gridWidth; ++j)
			{
				conf_new(ASMTileCacheEntry, this, j * gs_ASMTileSize, i * gs_ASMTileSize);
			}
		}
	}

	~ASMTileCache()
	{
		for (size_t i = m_tiles.size(); i > 0; --i)
		{
			conf_delete(m_tiles[i - 1]);
		}
	}

	void Tick(float deltaTime)
	{
		for (int32_t i = 0; i < m_readyTiles.size(); ++i)
		{
			ASMTileCacheEntry* pTile = m_readyTiles[i];
			pTile->m_fadeInFactor = max(0.0f, pTile->m_fadeInFactor - deltaTime);
		}
	}


	static float CalcDepthBias(
		const mat4& orthoProjMat,
		const vec3& kernelSize,
		int32_t viewportWidth,
		int32_t viewportHeight,
		int32_t depthBitsPerPixel)
	{
		vec3 texelSizeWS(
			fabsf(2.0f / (orthoProjMat.getCol0().getX() * float(viewportWidth))),
			fabsf(2.0f / (orthoProjMat.getCol1().getY() * float(viewportHeight))),
			fabsf(1.0f / (orthoProjMat.getCol2().getZ() * float(1 << depthBitsPerPixel))));
		vec3 kernelSizeWS = vec3(texelSizeWS.getX() * kernelSize.getX(),
			texelSizeWS.getY() * kernelSize.getY(), texelSizeWS.getZ() * kernelSize.getZ());
		float kernelSizeMax = fmax(fmax(kernelSizeWS.getX(), kernelSizeWS.getY()), kernelSizeWS.getZ());
		return kernelSizeMax * fabsf(orthoProjMat.getCol2().getZ());
	}


	template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
	ASMTileCacheEntry* Allocate(QuadTreeNode* pNode, ASMFrustum* pFrustum);

	int32_t AddTileFromRenderQueueToRenderBatch(
		ASMFrustum* pFrustum,
		int32_t maxRefinement,
		bool isLayer)
	{
		return AddTileToRenderBatch(
			m_renderQueue,
			pFrustum,
			maxRefinement,
			isLayer);
	}

	int32_t AddTileFromUpdateQueueToRenderBatch(
		ASMFrustum* pFrustum,
		int32_t maxRefinement,
		bool isLayer)
	{
		return AddTileToRenderBatch(
			m_updateQueue,
			pFrustum,
			maxRefinement,
			isLayer);
	}

	bool PrepareRenderTilesBatch(const ASMSShadowMapPrepareRenderContext& context)
	{
		for (int32_t i = 0; i < m_renderBatch.size(); ++i)
		{
			ASMTileCacheEntry* pTile = m_renderBatch[i];
			pTile->PrepareRender(context);
		}
		return !m_renderBatch.empty();
	}

	void RenderTilesBatch(
		RenderTarget* workBufferDepth,
		RenderTarget* workBufferColor,
		ASMSShadowMapRenderContext& context)
	{
		if (!m_renderBatch.empty())
		{
			RenderTiles(static_cast<uint32_t>(m_renderBatch.size()),
				&m_renderBatch[0], workBufferDepth,
				workBufferColor, context, true);
		}

		for (size_t i = m_renderBatch.size(); i > 0; --i)
		{
			ASMTileCacheEntry* pTile = m_renderBatch[i - 1];
			m_renderBatch.Remove(pTile);

			if (!pTile->IsReady())
			{
				pTile->MarkReady();
				++m_numTilesRendered;
			}
			else
			{
				++m_numTilesRendered;
			}
		}
	}

	void CreateDEM(
		RenderTarget* demWorkBufferColor,
		const ASMSShadowMapRenderContext& context,
		bool createDemForLayerRendering)
	{
		if (m_demQueue.empty())
		{
			return;
		}

		ASMRendererContext* rendererContext = context.m_pRendererContext;
		Cmd* pCurCmd = rendererContext->m_pCmd;

		uint32_t workBufferWidth = demWorkBufferColor->mDesc.mWidth;
		uint32_t workBufferHeight = demWorkBufferColor->mDesc.mHeight;
		uint32_t numTilesW = workBufferWidth / gs_ASMDEMTileSize;
		uint32_t numTilesH = workBufferHeight / gs_ASMDEMTileSize;
		uint32_t maxTilesPerPass = numTilesW * numTilesH;


		SCopyQuad* atlasToBulkQuads = (SCopyQuad*) conf_malloc(
			(sizeof(SCopyQuad) * 2 + sizeof(ASMTileCacheEntry*)) * maxTilesPerPass);

		SCopyQuad* bulkToAtlasQuads = atlasToBulkQuads + maxTilesPerPass;
		ASMTileCacheEntry** tilesToUpdate = reinterpret_cast<ASMTileCacheEntry**>(bulkToAtlasQuads + maxTilesPerPass);


		while (true)
		{
			uint32_t numTiles = 0;
			for (int32_t i = 0; i < m_demQueue.size() && numTiles < maxTilesPerPass; ++i)
			{
				ASMTileCacheEntry* pTile = m_demQueue[i];

				bool isDemForLayerRendering = pTile->m_refinement > 0 && !pTile->m_isLayer;
				if (isDemForLayerRendering == createDemForLayerRendering)
				{
					static const uint32_t rectSize = gs_ASMDEMTileSize - 2;

					uint32_t workX = (numTiles % numTilesW) * gs_ASMDEMTileSize;
					uint32_t workY = (numTiles / numTilesW) * gs_ASMDEMTileSize;

					uint32_t atlasX = ((pTile->m_viewport.x - gs_ASMTileBorderTexels) >> gs_ASMDEMDownsampleLevel) + 1;
					uint32_t atlasY = ((pTile->m_viewport.y - gs_ASMTileBorderTexels) >> gs_ASMDEMDownsampleLevel) + 1;

					if (createDemForLayerRendering)
					{
					}
					else
					{
						atlasToBulkQuads[numTiles] = SCopyQuad::Get(
							vec4(1.0f / float(m_demAtlasWidth), 1.0f / float(m_demAtlasHeight), 0.0f, 0.0f),
							rectSize, rectSize, workX + 1, workY + 1, workBufferWidth, workBufferHeight,
							rectSize, rectSize, atlasX, atlasY, m_demAtlasWidth, m_demAtlasHeight);

						float zTest = atlasToBulkQuads[numTiles].m_misc.getZ();

					}

					bulkToAtlasQuads[numTiles] = SCopyQuad::Get(
						vec4(1.0f / float(workBufferWidth), 1.0f / float(workBufferHeight), 0.0f, 0.0f),
						rectSize, rectSize, atlasX, atlasY, m_demAtlasWidth, m_demAtlasHeight,
						rectSize, rectSize, workX + 1, workY + 1, workBufferWidth, workBufferHeight);

					tilesToUpdate[numTiles++] = pTile;
				}
			}

			if (numTiles == 0)
			{
				break;
			}

#ifdef _DURANGO
			//On Xbox somehow the texture is initialized with garbage value, so texture that isn't cleared every frame
			//needs to be cleared at the begininng for xbox.
			if (mDEMFirstTimeRender)
			{
				TextureBarrier demRenderTargetBarrier[] = { {pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET} };
				cmdResourceBarrier(pCurCmd, 0, NULL, 1, demRenderTargetBarrier);


				LoadActionsDesc clearDEMLoadActions = {};
				clearDEMLoadActions.mClearColorValues[0] = pRenderTargetASMDEMAtlas->mDesc.mClearValue;
				clearDEMLoadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
				clearDEMLoadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
				clearDEMLoadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

				setRenderTarget(pCurCmd, 1, &pRenderTargetASMDEMAtlas, NULL, &clearDEMLoadActions,
					vec2(0.f, 0.f), vec2((float)pRenderTargetASMDEMAtlas->pTexture->mDesc.mWidth, (float)pRenderTargetASMDEMAtlas->pTexture->mDesc.mHeight));

				setRenderTarget(pCurCmd, 0, NULL, NULL, NULL, vec2(0.f), vec2(0.f));
				mDEMFirstTimeRender = false;
			}
#endif

			cmdBeginGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, "DEM Atlas To Color", true);

			TextureBarrier asmAtlasToColorBarrier[] = {
				{  demWorkBufferColor->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE} };

			cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmAtlasToColorBarrier);

			LoadActionsDesc atlasToColorLoadAction = {};
			atlasToColorLoadAction.mClearColorValues[0] = demWorkBufferColor->mDesc.mClearValue;
			atlasToColorLoadAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			//atlasToColorLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
			atlasToColorLoadAction.mClearDepth = pRenderTargetASMDepthAtlas->mDesc.mClearValue;
			atlasToColorLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;
			atlasToColorLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

			setRenderTarget(pCurCmd, 1, &demWorkBufferColor, NULL,
				&atlasToColorLoadAction, vec2(0.f, 0.f),
				vec2((float)demWorkBufferColor->mDesc.mWidth,
				(float)demWorkBufferColor->mDesc.mHeight));

			//GenerateDEMAtlasToColorRenderData& atlasToColorRenderData = gASMTickData.mDEMAtlasToColorRenderData;

			cmdBindPipeline(pCurCmd, pPipelineASMDEMAtlasToColor);

			BufferUpdateDesc atlasToColorUpdateDesc = {};
			atlasToColorUpdateDesc.pBuffer = pBufferASMAtlasToColorPackedQuadsUniform[gFrameIndex];
			atlasToColorUpdateDesc.pData = &atlasToBulkQuads[0];
			atlasToColorUpdateDesc.mSize = sizeof(vec4) * 3 * numTiles;
			updateResource(&atlasToColorUpdateDesc);

			cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetASMDEMAtlasToColor[0]);
			cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetASMDEMAtlasToColor[1]);

			cmdDraw(pCurCmd, numTiles * 6, 0);


			cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, NULL);

			cmdBeginGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, "DEM Color To Atlas", true);

			LoadActionsDesc colorToAtlasLoadAction = {};
			colorToAtlasLoadAction.mClearColorValues[0] = pRenderTargetASMDEMAtlas->mDesc.mClearValue;
			colorToAtlasLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
			colorToAtlasLoadAction.mClearDepth = pRenderTargetASMDEMAtlas->mDesc.mClearValue;
			colorToAtlasLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;
			colorToAtlasLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;


			TextureBarrier asmColorToAtlasBarriers[] = {
				{  demWorkBufferColor->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET} };

			cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmColorToAtlasBarriers);

			setRenderTarget(pCurCmd, 1, &pRenderTargetASMDEMAtlas, NULL,
				&colorToAtlasLoadAction, vec2(0.f, 0.f), vec2((float)pRenderTargetASMDEMAtlas->mDesc.mWidth,
				(float)pRenderTargetASMDEMAtlas->mDesc.mHeight));

			cmdBindPipeline(pCurCmd, pPipelineASMDEMColorToAtlas);

			BufferUpdateDesc colorToAtlasBufferUbDesc = {};
			colorToAtlasBufferUbDesc.pBuffer =
				pBufferASMColorToAtlasPackedQuadsUniform[gFrameIndex];
			colorToAtlasBufferUbDesc.pData = &bulkToAtlasQuads[0];
			colorToAtlasBufferUbDesc.mSize = numTiles * sizeof(vec4) * 3;
			updateResource(&colorToAtlasBufferUbDesc);

			cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetASMDEMColorToAtlas[0]);
			cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetASMDEMColorToAtlas[1]);

			cmdDraw(pCurCmd, numTiles * 6, 0);

			for (uint32_t i = 0; i < numTiles; ++i)
			{
				m_demQueue.Remove(tilesToUpdate[i]);
			}

			cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(pCurCmd, rendererContext->m_pGpuProfiler, NULL);
		}

		conf_free(atlasToBulkQuads);
	}

	const ASMProjectionData* GetFirstRenderBatchProjection() const
	{
		if (m_renderBatch.empty())
		{
			return NULL;
		}
		return &m_renderBatch[0]->mRenderProjectionData;
	}

	void RenderIndirectModelSceneTile(
		const vec2& viewPortLoc,
		const vec2& viewPortSize,
		const ASMProjectionData& renderProjectionData,
		bool isLayer,
		ASMSShadowMapRenderContext& renderContext)
	{
		Cmd* pCurCmd = renderContext.m_pRendererContext->m_pCmd;
		

		cmdBindIndexBuffer(pCurCmd,  pBufferFilteredIndex[gFrameIndex][VIEW_SHADOW], 0);
			   
		cmdSetViewport(pCurCmd,
			static_cast<float>(viewPortLoc.getX()),
			static_cast<float>(viewPortLoc.getY()),
			static_cast<float>(viewPortSize.getX()),
			static_cast<float>(viewPortSize.getY()), 0.f, 1.f);

		for (int32_t i = 0; i < MESH_COUNT; ++i)
		{
			mat4& worldMat = gMeshASMProjectionInfoUniformData[0][gFrameIndex].mWorldMat;
			gMeshASMProjectionInfoUniformData[0][gFrameIndex].mWorldViewProjMat = renderProjectionData.mViewProjMat * worldMat;
		}

		BufferUpdateDesc updateDesc = { pBufferMeshShadowProjectionTransforms[0][gFrameIndex],
			&gMeshASMProjectionInfoUniformData[0][gFrameIndex] };
		updateResource(&updateDesc);

		cmdBindVertexBuffer(pCurCmd, 1, &pIndirectPosBuffer, NULL);
		cmdBindPipeline(pCurCmd, pPipelineIndirectDepthPass);

		cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex * 3 + 2, pDescriptorSetVBPass[2]);

		cmdExecuteIndirect(
			pCurCmd, pCmdSignatureVBPass,
			gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_OPAQUE],
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			0,
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

		Buffer* pVertexBuffersPosTex[] = { pIndirectPosBuffer,
			pIndirectTexCoordBuffer };
		cmdBindVertexBuffer(pCurCmd, 2, pVertexBuffersPosTex, NULL);

		cmdBindPipeline(pCurCmd, pPipelineIndirectAlphaDepthPass);
		
#ifdef METAL
		// #TODO: Automate this inside the Metal renderer
		cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex * 3 + 2, pDescriptorSetVBPass[2]);
#endif

		cmdExecuteIndirect(
			pCurCmd, pCmdSignatureVBPass,
			gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_ALPHATESTED],
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_SHADOW],
			0,
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_SHADOW],
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
	}

	bool NothingToRender() const { return m_renderBatch.empty(); }
	bool IsFadeInFinished(const ASMFrustum* pFrustum) const;

	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetTilesPos > m_tiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetFreeTilesPos > m_freeTiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetRenderQueuePos > m_renderQueue;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetReadyTilesPos > m_readyTiles;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetDemQueuePos > m_demQueue;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetRenderBatchPos > m_renderBatch;
	CVectorBasedIntrusiveUnorderedPtrSet< ASMTileCacheEntry, &ASMTileCacheEntry::GetUpdateQueuePos > m_updateQueue;

	uint32_t m_cacheHits;
	uint32_t m_tileAllocs;
	uint32_t m_numTilesRendered;
	uint32_t m_numTilesUpdated;
	uint32_t m_depthAtlasWidth;
	uint32_t m_depthAtlasHeight;
	uint32_t m_demAtlasWidth;
	uint32_t m_demAtlasHeight;


	template< class T >
	int32_t AddTileToRenderBatch(
		T& tilesQueue,
		ASMFrustum* pFrustum,
		int32_t maxRefinement,
							 bool isLayer);

	void RenderTiles(
		uint32_t numTiles,
		ASMTileCacheEntry** tiles,
		RenderTarget* workBufferDepth,
		RenderTarget* workBufferColor,
		ASMSShadowMapRenderContext& context,
		bool allowDEM);

	void StartDEM(ASMTileCacheEntry* pTile, SCopyQuad& copyDEMQuad)
	{
		m_demQueue.Add(pTile, true);

		int32_t demAtlasX = (pTile->m_viewport.x - gs_ASMTileBorderTexels) >> gs_ASMDEMDownsampleLevel;
		int32_t demAtlasY = (pTile->m_viewport.y - gs_ASMTileBorderTexels) >> gs_ASMDEMDownsampleLevel;

		copyDEMQuad = SCopyQuad::Get(
			vec4(0.f),
			gs_ASMDEMTileSize, gs_ASMDEMTileSize,
			demAtlasX, demAtlasY,
			m_demAtlasWidth, m_demAtlasHeight,
			gs_ASMTileSize, gs_ASMTileSize,
			pTile->m_viewport.x - gs_ASMTileBorderTexels,
			pTile->m_viewport.y - gs_ASMTileBorderTexels,
			m_depthAtlasWidth, m_depthAtlasHeight
		);
	}

	friend class ASMTileCacheEntry;

	bool mDEMFirstTimeRender;
	bool mDepthFirstTimeRender;
};




void ASMTileCacheEntry::MarkReady()
{
	m_pCache->m_readyTiles.Add(this);
	m_fadeInFactor = 0.5f;
}
void ASMTileCacheEntry::MarkNotReady()
{
	m_pCache->m_readyTiles.Remove(this);
}



ASMTileCacheEntry::ASMTileCacheEntry(ASMTileCache* pCache, int32_t x, int32_t y)
	:m_pCache(pCache),
	m_pOwner(NULL),
	m_pFrustum(NULL),
	m_lastFrameUsed(0),
	m_frustumID(0),
	m_isLayer(false),
	m_fadeInFactor(0.f)
{
	m_viewport.x = x + gs_ASMTileBorderTexels;
	m_viewport.w = gs_ASMBorderlessTileSize;
	m_viewport.y = y + gs_ASMTileBorderTexels;
	m_viewport.h = gs_ASMBorderlessTileSize;

	Invalidate();

	m_pCache->m_tiles.Add(this);
	m_pCache->m_freeTiles.Add(this);
}
ASMTileCacheEntry::~ASMTileCacheEntry()
{
	if (IsAllocated())
	{
		Free();
	}

	m_pCache->m_tiles.Remove(this);
	m_pCache->m_freeTiles.Remove(this);
}



class ASMQuadTree
{
public:
	ASMQuadTree()
	{
		mRoots.reserve(32);
	}
	~ASMQuadTree()
	{
		Reset();
	}

	void Reset()
	{
		while (!mRoots.empty())
		{
			conf_delete(mRoots.back());
		}
	}

	QuadTreeNode* FindRoot(const AABB& bbox);

	const eastl::vector<QuadTreeNode*>& GetRoots() const { return mRoots; }

	eastl::vector<QuadTreeNode*> mRoots;

};

class QuadTreeNode
{
public:
	AABB mBBox;
	uint32_t mLastFrameVerified;

	QuadTreeNode* m_pParent;
	QuadTreeNode* mChildren[4];
	ASMTileCacheEntry* m_pTile;
	ASMTileCacheEntry* m_pLayerTile;
	uint8_t mRefinement;
	uint8_t mNumChildren;

	QuadTreeNode(ASMQuadTree* pQuadTree, QuadTreeNode* pParent)
		:m_pQuadTree(pQuadTree),
		m_pParent(pParent),
		mLastFrameVerified(0),
		mNumChildren(0),
		m_pTile(NULL),
		m_pLayerTile(NULL)
	{
		memset(mChildren, 0, sizeof(mChildren));

		if (m_pParent != NULL)
		{
			mRefinement = m_pParent->mRefinement + 1;
			mRootNodesIndex = -1;
		}
		else
		{
			mRefinement = 0;
			mRootNodesIndex = static_cast<int32_t>(m_pQuadTree->mRoots.size());
			m_pQuadTree->mRoots.push_back(this);
		}
	}
	~QuadTreeNode()
	{
		if (m_pTile)
		{
			m_pTile->Free();
		}

		if (m_pLayerTile)
		{
			m_pLayerTile->Free();
		}

		for (int32_t i = 0; i < 4; ++i)
		{
			if (mChildren[i])
			{
				conf_delete(mChildren[i]);
			}
		}

		if (m_pParent)
		{
			for (int32_t i = 0; i < 4; ++i)
			{
				if (m_pParent->mChildren[i] == this)
				{
					m_pParent->mChildren[i] = NULL;
					--m_pParent->mNumChildren;
					break;
				}
			}
		}
		else
		{
			QuadTreeNode* pLast = m_pQuadTree->mRoots.back();
			pLast->mRootNodesIndex = mRootNodesIndex;
			m_pQuadTree->mRoots[mRootNodesIndex] = pLast;
			m_pQuadTree->mRoots.pop_back();
		}
	}

	const AABB GetChildBBox(int32_t childIndex)
	{
		static const vec3 quadrantOffsets[] =
		{
			vec3(0.0f, 0.0f, 0.0f),
			vec3(1.0f, 0.0f, 0.0f),
			vec3(1.0f, 1.0f, 0.0f),
			vec3(0.0f, 1.0f, 0.0f),
		};
		vec3 halfSize = 0.5f * (mBBox.maxBounds - mBBox.minBounds);
		const vec3& curQuadIndxOffset = quadrantOffsets[childIndex];
		vec3 bboxMin = mBBox.minBounds + vec3(curQuadIndxOffset.getX() * halfSize.getX(),
			curQuadIndxOffset.getY() * halfSize.getY(), curQuadIndxOffset.getZ() * halfSize.getZ());

		vec3 bboxMax = bboxMin + halfSize;
		return AABB(bboxMin, bboxMax);
	}
	QuadTreeNode* AddChild(int32_t childIndex)
	{
		if (!mChildren[childIndex])
		{
			mChildren[childIndex] = conf_new(QuadTreeNode, m_pQuadTree, this);
			++mNumChildren;
		}
		return mChildren[childIndex];
	}

	ASMTileCacheEntry*& GetTile() { return m_pTile; }
	ASMTileCacheEntry*& GetLayerTile() { return m_pLayerTile; }

	ASMTileCacheEntry* GetTile(bool isLayer) const { return isLayer ? m_pLayerTile : m_pTile; }

private:
	ASMQuadTree* m_pQuadTree;
	int32_t mRootNodesIndex;
};

QuadTreeNode* ASMQuadTree::FindRoot(const AABB& bbox)
{
	for (int32_t i = 0; i < mRoots.size(); ++i)
	{
		if (!(mRoots[i]->mBBox.minBounds != bbox.minBounds 
			|| mRoots[i]->mBBox.maxBounds != bbox.maxBounds))
		{
			return mRoots[i];
		}
	}
	return NULL;
}

void ASMTileCacheEntry::Free()
{
	if (m_renderQueuePos.IsInserted() ||
		m_renderBatchPos.IsInserted() ||
		m_updateQueuePos.IsInserted() ||
		m_demQueuePos.IsInserted())
	{
		m_pCache->m_renderQueue.Remove(this, true);
		m_pCache->m_renderBatch.Remove(this, true);
		m_pCache->m_updateQueue.Remove(this, true);
		m_pCache->m_demQueue.Remove(this, true);
		m_pCache->m_readyTiles.Remove(this, true);
		Invalidate();
	}
	else
	{
		MarkNotReady();
		m_lastFrameUsed = asmFrameCounter;
	}
	m_pCache->m_freeTiles.Add(this);
	(m_isLayer ? m_pOwner->m_pLayerTile : m_pOwner->m_pTile) = nullptr;
	m_pOwner = NULL;
	m_pFrustum = NULL;
}


static bool IsTileAcceptableForIndexing(const ASMTileCacheEntry* pTile)
{
	return pTile && pTile->IsReady();
}
static bool IsNodeAcceptableForIndexing(const QuadTreeNode* pNode)
{
	return IsTileAcceptableForIndexing(pNode->m_pTile);
}

class ASMFrustum
{
public:
	struct Config
	{
		float m_largestTileWorldSize;
		float m_shadowDistance;
		int32_t m_maxRefinement;
		int32_t m_minRefinementForLayer;
		int32_t m_indexSize;
		bool m_forceImpostors;
		float m_refinementDistanceSq[gs_ASMMaxRefinement + 2];
		float m_minExtentLS;
	};

	ASMFrustum(const Config& cfg, bool useMRF, bool isPreRender)
		:m_cfg(cfg),
		mIsPrerender(isPreRender),
		mFirstTimeRender(true)
	{
		m_demMinRefinement[0] = useMRF ? (UseLayers() ? 1 : 0) : -1;
		m_demMinRefinement[1] = m_cfg.m_minRefinementForLayer;
		m_indirectionTextureSize = (1 << m_cfg.m_maxRefinement) * m_cfg.m_indexSize;
		Reset();
	}

	~ASMFrustum()
	{
	}



	bool mIsPrerender;
	uint32_t m_ID;
	vec3 m_lightDir;
	mat4 m_lightRotMat;
	mat4 m_invLightRotMat;
	vec2 m_refinementPoint;

	vec3 m_receiverWarpVector;
	vec3 m_blockerSearchVector;
	bool m_disableWarping;

	ConvexHull2D m_frustumHull;
	ConvexHull2D m_largerHull;
	ConvexHull2D m_prevLargerHull;

	ASMQuadTree m_quadTree;

	Config m_cfg;
	int32_t m_indirectionTextureSize;
	int32_t m_demMinRefinement[2];

	eastl::vector<RenderTarget*> m_indirectionTexturesMips;
	RenderTarget* m_lodClampTexture;

	mat4 m_indexTexMat;
	mat4 m_indexViewMat;



	void Load(const ASMRenderTargets& renderTargets, bool isPreRender)
	{
		m_indirectionTexturesMips.clear();
		if (!isPreRender)
		{
			m_indirectionTexturesMips = renderTargets.m_pASMIndirectionMips;
			m_lodClampTexture = renderTargets.m_pRenderTargetASMLodClamp;
		}
		else
		{
			m_indirectionTexturesMips = renderTargets.m_pASMPrerenderIndirectionMips;
			m_lodClampTexture = renderTargets.m_pRenderTargetASMPrerenderLodClamp;
		}
	}

	bool IsLightDirDifferent(const vec3& lightDir) const
	{
		return dot(m_lightDir, lightDir) < lightDirUpdateThreshold;
	}
	void Set(ICameraController* lightCameraController, const vec3& lightDir)
	{
		Reset();
		m_lightDir = lightDir;

		lightCameraController->moveTo(vec3(0.f));
		lightCameraController->lookAt(-m_lightDir);

		m_lightRotMat = lightCameraController->getViewMatrix();
		m_invLightRotMat = inverse(m_lightRotMat);


		static uint32_t s_IDGen = 1;
		m_ID = s_IDGen; s_IDGen += 2;
	}
	void Reset()
	{
		m_quadTree.Reset();
		m_ID = 0;
		m_lightDir = vec3(0.0);
		m_lightRotMat = mat4::identity();
		m_invLightRotMat = mat4::identity();

		m_indexTexMat = mat4::identity();
		m_indexViewMat = mat4::identity();

		m_refinementPoint = vec2(0.0);

		m_frustumHull.Reset();
		m_largerHull.Reset();
		m_prevLargerHull.Reset();

		m_receiverWarpVector = vec3(0.0);
		m_blockerSearchVector = vec3(0.0);
		m_disableWarping = false;

		ResetIndirectionTextureData();
	}

	void CreateTiles(ASMTileCache* pCache, const ASMProjectionData& mainProjection)
	{
		if (!IsValid() || IsLightBelowHorizon())
		{
			return;
		}

		m_refinementPoint = m_frustumHull.FindFrustumConvexHull(mainProjection,
			m_cfg.m_shadowDistance, m_lightRotMat);

		m_prevLargerHull = m_largerHull;

		m_largerHull.FindFrustumConvexHull(mainProjection,
			1.01f * m_cfg.m_shadowDistance, m_lightRotMat);

		for (size_t i = m_quadTree.GetRoots().size(); i > 0; --i)
		{
			RemoveNonIntersectingNodes(m_quadTree.GetRoots()[i - 1]);
		}

		AABB hullBBox(vec3(FLT_MAX), vec3(-FLT_MAX));

		for (int32_t i = 0; i < m_frustumHull.m_size; ++i)
		{
			adjustAABB(&hullBBox, vec3(m_frustumHull.m_vertices[i].getX(), m_frustumHull.m_vertices[i].getY(), 0.f));
		}

		adjustAABB(&hullBBox, vec3(m_refinementPoint.getX(), m_refinementPoint.getY(), 0.f) + vec3(m_cfg.m_minExtentLS, m_cfg.m_minExtentLS, 0.f));
		adjustAABB(&hullBBox, vec3(m_refinementPoint.getX(), m_refinementPoint.getY(), 0.f) - vec3(m_cfg.m_minExtentLS, m_cfg.m_minExtentLS, 0.f));

		alignAABB(&hullBBox, m_cfg.m_largestTileWorldSize);


		AABB nodeBBox(vec3(0.f), vec3(0.f));
		float minY = nodeBBox.minBounds.getY();
		float minX = nodeBBox.minBounds.getX();
		for (minY = hullBBox.minBounds.getY(); minY < hullBBox.maxBounds.getY(); minY += m_cfg.m_largestTileWorldSize)
		{

			for (minX = hullBBox.minBounds.getX(); minX < hullBBox.maxBounds.getX(); minX += m_cfg.m_largestTileWorldSize)
			{
				nodeBBox.minBounds.setY(minY);
				nodeBBox.minBounds.setX(minX);
				nodeBBox.maxBounds = nodeBBox.minBounds + vec3(m_cfg.m_largestTileWorldSize, m_cfg.m_largestTileWorldSize, 0.0f);
				if (ShouldNodeExist(nodeBBox, 0))
				{
					QuadTreeNode* pNode = m_quadTree.FindRoot(nodeBBox);
					if (pNode == nullptr)
					{
						QuadTreeNode* temp = (QuadTreeNode*)conf_malloc(sizeof(QuadTreeNode));
						pNode = conf_placement_new<QuadTreeNode>(temp, &m_quadTree, (QuadTreeNode*)0);
						pNode->mBBox = nodeBBox;
					}

					RefineNode < ASMFrustum, &ASMFrustum::RefineAgainstFrustum >(pNode, m_cfg.m_maxRefinement, *this);
				}
			}
		}

		for (auto it = m_quadTree.GetRoots().begin(); it != m_quadTree.GetRoots().end(); ++it)
		{
			AllocateTiles(pCache, *it);
		}
	}

	void BuildTextures(const ASMSShadowMapRenderContext& context, bool isPreRender)
	{
		FindIndexedNodes();
		ResetIndirectionTextureData();
		FillIndirectionTextureData(false);
		UpdateIndirectionTexture(NULL, context, isPreRender, isPreRender);

		if (isPreRender)
		{
			FillLODClampTextureData();
			UpdateLODClampTexture(m_lodClampTexture, context);
		}

		if (UseLayers())
		{
			ResetIndirectionTextureData();
			FillIndirectionTextureData(true);
			UpdateIndirectionTexture(m_layerIndirectionTexture, context, isPreRender, isPreRender);
		}
	}
	bool IsValid() const { return m_ID != 0; }
	bool UseLayers() const { return m_cfg.m_minRefinementForLayer <= m_cfg.m_maxRefinement; }
	int32_t GetDEMMinRefinement(bool isLayer) const { return m_demMinRefinement[isLayer]; }
	bool IsLightBelowHorizon() const { return false; }//IsValid() && m_lightDir.y < 0; }

	const RenderTarget* GetLODClampTexture() const { return m_lodClampTexture; }
	const RenderTarget* GetLayerIndirectionTexture() const { return m_layerIndirectionTexture; }

	const ASMProjectionData CalcCamera(const vec3& cameraPos, const AABB& BBoxLS,
		const vec2& viewportScaleFactor, bool reverseZ = true, bool customCamera = false) const
	{
		mat4 viewMat = mat4::lookAt(Point3(cameraPos), Point3(cameraPos + m_lightDir), vec3(0.f, 1.f, 0.f));
		ASMProjectionData renderProjection;
		renderProjection.mViewMat = viewMat;

		vec3 bboxSize = calculateAABBSize(&BBoxLS);

		float hw = 0.5f * bboxSize.getX() * viewportScaleFactor.getX();
		float hh = 0.5f * bboxSize.getY() * viewportScaleFactor.getY();

		float farPlane = gs_ASMTileFarPlane;

		if (reverseZ)
		{
			renderProjection.mProjMat = mat4::orthographic(-hw, hw, -hh, hh, farPlane, 0);
		}
		else
		{
			renderProjection.mProjMat = mat4::orthographic(-hw, hw, -hh, hh, 0, farPlane);
		}

		renderProjection.mInvViewMat = inverse(viewMat);
		renderProjection.mInvProjMat = inverse(renderProjection.mProjMat);
		renderProjection.mViewProjMat = renderProjection.mProjMat * viewMat;
		renderProjection.mInvViewProjMat = renderProjection.mInvViewMat * renderProjection.mInvProjMat;

		return renderProjection;
	}

	const ASMProjectionData CalcCamera(const AABB& BBoxLS,
		const vec3& worldCenter, const vec2& viewportScaleFactor, bool customCamera = false) const
	{
		vec3 aabbMin = worldCenter + vec3(-800.0f, -200.0f, -800.0f);
		vec3 aabbMax = worldCenter + vec3(800.0f, 500.0f, 800.0f);

		float minZ = FLT_MAX;
		for (int32_t i = 0; i < 8; ++i)
		{
			vec3 aabbCorner(
				i & 1 ? aabbMin.getX() : aabbMax.getX(),
				i & 2 ? aabbMin.getY() : aabbMax.getY(),
				i & 4 ? aabbMin.getZ() : aabbMax.getZ());
			minZ = fmin(minZ, -dot(aabbCorner, m_lightDir));
		}

		vec3 cameraPos = (m_invLightRotMat * vec4(calculateAABBCenter(&BBoxLS), 1.f)).getXYZ() - minZ * m_lightDir;

		static const vec3 boundsN[] =
		{
			 vec3(-1.0f, 0.0f, 0.0f),
			 vec3(0.0f,-1.0f, 0.0f),
			 vec3(0.0f, 0.0f,-1.0f),
			 vec3(1.0f, 0.0f, 0.0f),
			 vec3(0.0f, 1.0f, 0.0f),
			 vec3(0.0f, 0.0f, 1.0f),
		};

		float boundsD[] =
		{
			 aabbMax.getX(),  aabbMax.getY(),  aabbMax.getZ(),
			-aabbMin.getX(), -aabbMin.getY(), -aabbMin.getZ(),
		};

		float minF = 0;
		for (uint32_t i = 0; i < 6; ++i)
		{
			float f1 = dot(boundsN[i], cameraPos) + boundsD[i];
			float f2 = dot(boundsN[i], m_lightDir);
			if (f1 <= 0 && f2 < 0)
			{
				minF = max(minF, f1 / f2);
			}
		}

		return CalcCamera(cameraPos - minF * m_lightDir, BBoxLS, viewportScaleFactor, true, customCamera);
	}

	void UpdateWarpVector(const ASMCpuSettings& asmCpuSettings, const vec3& lightDir, bool disableWarping)
	{
		if (!IsValid())
		{
			return;
		}
		m_disableWarping |= disableWarping;
		if (m_disableWarping)
		{
			return;
		}

		if (dot(m_lightDir, lightDir) < maxWarpAngleCos)
		{
			return;
		}

		vec3 shadowDir = -m_lightDir;
		vec3 dir = lightDir - 2.0f * dot(shadowDir, lightDir) * shadowDir;

		float warpBias = 1.0f - 0.9f * length(dir - shadowDir);
		m_receiverWarpVector = warpBias * dir - shadowDir;


		vec3 warpDirVS = (m_indexViewMat *
			vec4(m_receiverWarpVector, 0.f)).getXYZ();


		float stepDistance = asmCpuSettings.mParallaxStepDistance;
		float stepBias = asmCpuSettings.mParallaxStepBias;
		m_blockerSearchVector = vec3(
			stepDistance * warpDirVS.getX() / gs_ASMDEMAtlasTextureWidth,
			stepDistance * warpDirVS.getY() / gs_ASMDEMAtlasTextureHeight,
			-stepBias / gs_ASMTileFarPlane);
	}

	void GetIndirectionTextureData(ASMTileCacheEntry* pTile, vec4& packedData, ivec4& dstCoord)
	{
		float invAtlasWidth = 1.f / float(gs_ASMDepthAtlasTextureWidth);
		float invAtlasHeight = 1.f / float(gs_ASMDepthAtlasTextureHeight);

		vec3 tileMin(0.f, 1.f, 0.f);
		vec3 tileMax(1.f, 0.f, 0.f);

		vec3 renderProjPos = pTile->mRenderProjectionData.mInvViewMat.getCol3().getXYZ();

		vec3 indexMin = ProjectToTS(pTile->m_BBox.minBounds, m_indexBBox,
			renderProjPos - m_indexCameraPos);
		vec3 indexMax = ProjectToTS(pTile->m_BBox.maxBounds, m_indexBBox,
			renderProjPos - m_indexCameraPos);

		int32_t x0 = static_cast<int32_t>(indexMin.getX() *
			static_cast<float>(m_indirectionTextureSize) + 0.25f);
		int32_t y0 = static_cast<int32_t>(indexMax.getY() *
			static_cast<float>(m_indirectionTextureSize) + 0.25f);

		int32_t x1 = static_cast<int32_t>(indexMax.getX() *
			static_cast<float>(m_indirectionTextureSize) - 0.25f);
		int32_t y1 = static_cast<int32_t>(indexMin.getY() *
			static_cast<float>(m_indirectionTextureSize) - 0.25f);

		const int32_t mipMask = (1 << (m_cfg.m_maxRefinement - pTile->m_refinement)) - 1;

		// Compute affine transform (scale and offset) from index normalized cube to tile normalized cube.
		vec3 scale1(
			(tileMax.getX() - tileMin.getX()) / (indexMax.getX() - indexMin.getX()),
			(tileMax.getY() - tileMin.getY()) / (indexMax.getY() - indexMin.getY()),
			1.0f);
		vec3 offset1 = tileMin - vec3(indexMin.getX() * scale1.getX(),
			indexMin.getY() * scale1.getY(), indexMin.getZ() * scale1.getZ());

		// Compute affine transform (scale and offset) from tile normalized cube to shadowmap atlas.
		vec3 scale2(
			float(pTile->m_viewport.w) * invAtlasWidth,
			float(pTile->m_viewport.h) * invAtlasHeight,
			1.0f);
		vec3 offset2(
			(float(pTile->m_viewport.x) + 0.5f) * invAtlasWidth,
			(float(pTile->m_viewport.y) + 0.5f) * invAtlasHeight,
			0.0f);

		// Compute combined affine transform from index normalized cube to shadowmap atlas.
		vec3 scale = vec3(scale1.getX() * scale2.getX(), scale1.getY() * scale2.getY(), scale1.getZ() * scale2.getZ());
		vec3 offset = vec3(offset1.getX() * scale2.getX(), offset1.getY() *
			scale2.getY(), offset1.getZ() * scale2.getZ()) + offset2;

		// Assemble data for indirection texture:
		//   packedData.xyz contains transform from view frustum of index texture to view frustum of individual tile
		//   packedData.w contains packed data: integer part is refinement-dependent factor for texcoords computation,
		//      fractional part is bias for smooth tile transition unpacked via getFadeInConstant() in shader,
		//      sign indicates if the tile is a layer tile or just a regular tile.
		packedData.setX(offset.getX());
		packedData.setY(offset.getY());
		packedData.setZ(offset.getZ());
		packedData.setW(
			float((1 << pTile->m_refinement) * gs_ASMBorderlessTileSize * m_cfg.m_indexSize)
		);

		dstCoord = ivec4(x0, y0, x1, y1);
	}

private:
	RenderTarget* m_layerIndirectionTexture;

	eastl::vector< SFillQuad > m_quads;
	uint32_t m_quadsCnt[gs_ASMMaxRefinement + 1];

	eastl::vector< SFillQuad > m_lodClampQuads;

	eastl::vector< QuadTreeNode* > m_indexedNodes;
	AABB m_indexBBox;
	vec3 m_indexCameraPos;

	static bool RefineAgainstFrustum(
		const AABB& childbbox,
		const QuadTreeNode* pParent,
		const ASMFrustum& frustum)
	{
		return frustum.ShouldNodeExist(childbbox, pParent->mRefinement + 1);
	}

	template< class T, bool(*isRefinable)(const AABB&, const QuadTreeNode*, const T&) >
	static void RefineNode(QuadTreeNode* pParent, int32_t maxRefinement, const T& userData)
	{
		if (pParent->mRefinement < maxRefinement)
		{
			for (int32_t i = 0; i < 4; ++i)
			{
				if (pParent->mChildren[i])
				{
					RefineNode <T, isRefinable>(
						pParent->mChildren[i], maxRefinement, userData);
				}
				else
				{
					//Here we check if any of the nodes requires new 
					//child node or not
					AABB childBBox = pParent->GetChildBBox(i);
					if (isRefinable(childBBox, pParent, userData))
					{
						QuadTreeNode* pNode = pParent->AddChild(i);
						pNode->mBBox = childBBox;
						RefineNode <T, isRefinable>(pNode, maxRefinement, userData);
					}
				}
			}
		}
	}

	void AllocateTiles(ASMTileCache* pCache, QuadTreeNode* pNode)
	{
		for (int32_t i = 0; i < 4; ++i)
		{
			if (pNode->mChildren[i])
			{
				AllocateTiles(pCache, pNode->mChildren[i]);
			}
		}

		if (!pNode->m_pTile)
		{
			pCache->Allocate<&QuadTreeNode::GetTile, false>(pNode, this);
		}
	}
	void RemoveNonIntersectingNodes(QuadTreeNode* pNode)
	{
		for (int32_t i = 0; i < 4; ++i)
		{
			if (pNode->mChildren[i])
			{
				RemoveNonIntersectingNodes(pNode->mChildren[i]);
			}
		}

		if (pNode->mLastFrameVerified != asmFrameCounter)
		{
			pNode->mLastFrameVerified = asmFrameCounter;

			if (ShouldNodeExist(pNode->mBBox, pNode->mRefinement))
			{
				if (pNode->m_pParent)
				{
					pNode->m_pParent->mLastFrameVerified = asmFrameCounter;
				}
				return;
			}
			conf_delete(pNode);
		}
	}

	static void SortNodes(
		const vec2& refinementPoint,
		const vec2& sortRegionMaxSize,
		float tileSize,
		const eastl::vector<QuadTreeNode*>& nodes,
		eastl::vector<QuadTreeNode*>& sortedNodes,
		AABB& sortedBBox)
	{
		struct SortStruct
		{
			QuadTreeNode* m_pNode;
			float mKey;
		};

		eastl::vector<SortStruct> nodesToSort;
		nodesToSort.reserve(nodes.size());

		//SortStruct* nodesToSort = (SortStruct*)conf_malloc(
			//sizeof(SortStruct) * nodes.size());

		vec2 distMax = sortRegionMaxSize + vec2(tileSize, tileSize);
		float distMaxSq = dot(distMax, distMax);

		uint32_t numNodesToSort = 0;

		for (int32_t i = 0; i < nodes.size(); ++i)
		{
			QuadTreeNode* pNode = nodes[i];
			if (IsNodeAcceptableForIndexing(pNode))
			{
				AABB& bbox = pNode->mBBox;
				vec3 bboxCenter = calculateAABBCenter(&bbox);
				vec3 bboxSize = calculateAABBSize(&bbox);
				float dx = fmax(fabsf(refinementPoint.getX() - bboxCenter.getX()) - bboxSize.getX() * 0.5f, 0.0f);
				float dy = fmax(fabsf(refinementPoint.getY() - bboxCenter.getY()) - bboxSize.getY() * 0.5f, 0.0f);

				float distSq = dx * dx + dy * dy;
				if (distSq < distMaxSq)
				{
					nodesToSort.push_back(SortStruct());
					SortStruct& ss = nodesToSort[numNodesToSort++];
					ss.mKey = fabsf(bbox.minBounds.getX() - refinementPoint.getX());
					ss.mKey = fmax(fabsf(bbox.minBounds.getY() - refinementPoint.getY()), ss.mKey);
					ss.mKey = fmax(fabsf(bbox.maxBounds.getX() - refinementPoint.getX()), ss.mKey);
					ss.mKey = fmax(fabsf(bbox.maxBounds.getY() - refinementPoint.getY()), ss.mKey);
					ss.m_pNode = pNode;
				}
			}
		}


		SortStruct* data = nodesToSort.data();
		int32_t    count = (int32_t)nodesToSort.size();

		std::qsort(data, count, sizeof(SortStruct), [](const void* a, const void* b) {
			const SortStruct* left = (SortStruct*)(a);
			const SortStruct* right = (SortStruct*)(b);

			if (left->mKey < right->mKey)
			{
				return -1;
			}
			else if (left->mKey > right->mKey)
			{
				return 1;
			}

			return 0;
		});

		sortedBBox = AABB(vec3(refinementPoint.getX(), refinementPoint.getY(), 0.f),
			vec3(refinementPoint.getX(), refinementPoint.getY(), 0.f));
		alignAABB(&sortedBBox, tileSize);

		sortedNodes.resize(0);

		for (uint32_t i = 0; i < numNodesToSort; ++i)
		{

			SortStruct& ss = nodesToSort[i];
			const AABB& nodeBBox = ss.m_pNode->mBBox;
			vec3 testMin(min(sortedBBox.minBounds.getX(), nodeBBox.minBounds.getX()),
				min(sortedBBox.minBounds.getY(), nodeBBox.minBounds.getY()), 0.f);
			vec3 testMax(max(sortedBBox.maxBounds.getX(), nodeBBox.maxBounds.getX()),
				max(sortedBBox.maxBounds.getY(), nodeBBox.maxBounds.getY()), 0.f);

			if ((testMax.getX() - testMin.getX()) > sortRegionMaxSize.getX()
				|| (testMax.getY() - testMin.getY()) > sortRegionMaxSize.getY())
			{
				if (ss.mKey > distMax.getX())
				{
					break;
				}
			}
			else
			{
				sortedBBox = AABB(testMin, testMax);
				sortedNodes.push_back(ss.m_pNode);
			}
		}
	}

	void FindIndexedNodes()
	{
		if (!IsValid())
		{
			return;
		}

		float sortRegionSizeMax = static_cast<float>(m_cfg.m_indexSize)
			* m_cfg.m_largestTileWorldSize;

		SortNodes(
			m_refinementPoint,
			vec2(sortRegionSizeMax),
			m_cfg.m_largestTileWorldSize,
			m_quadTree.GetRoots(),
			m_indexedNodes,
			m_indexBBox
		);

		m_indexBBox = AABB(m_indexBBox.minBounds, m_indexBBox.minBounds +
			vec3(sortRegionSizeMax, sortRegionSizeMax, 0.f));

		if (!m_indexedNodes.empty())
		{
			float offset = -FLT_MAX;

			for (int32_t i = 0; i < m_indexedNodes.size(); ++i)
			{
				QuadTreeNode* indexedNode = m_indexedNodes[i];
				offset = fmax(offset, dot(m_lightDir,
					indexedNode->m_pTile->mRenderProjectionData.mInvViewMat.getCol3().getXYZ()));
			}
			m_indexCameraPos = (m_invLightRotMat * vec4(
				calculateAABBCenter(&m_indexBBox), 1.f)).getXYZ() + offset * m_lightDir;

			
			ASMProjectionData renderProjection = CalcCamera(m_indexCameraPos, m_indexBBox, vec2(1.0f), true);
			m_indexViewMat = renderProjection.mViewMat;


			static const mat4 screenToTexCoordMatrix = mat4::translation(vec3(0.5f, 0.5f, 0.f)) * mat4::scale(vec3(0.5f, -0.5f, 1.f));
			m_indexTexMat = screenToTexCoordMatrix * renderProjection.mViewProjMat;
		}
	}
	void FillIndirectionTextureData(bool processLayers)
	{
		if (!IsValid())
		{
			return;
		}

		if (m_indexedNodes.empty())
		{
			return;
		}

		size_t numIndexedNodes = m_indexedNodes.size();
		uint32_t i = 0;

		for (int32_t z = m_cfg.m_maxRefinement; z >= 0; --z)
		{
			size_t numNodes = m_indexedNodes.size();
			for (; i < numNodes; ++i)
			{
				QuadTreeNode* pNode = m_indexedNodes[i];
				ASMTileCacheEntry* pTile = pNode->m_pTile;
				bool useRegularShadowMapAsLayer = false;
				if (processLayers)
				{
					if (!IsTileAcceptableForIndexing(pNode->m_pLayerTile))
					{

					}
					else
					{
						pTile = pNode->m_pLayerTile;
					}
				}

				vec4 packedData;
				ivec4 destCoord;
				GetIndirectionTextureData(pTile, packedData, destCoord);

				packedData.setW(packedData.getW() + pTile->m_fadeInFactor);

				if (useRegularShadowMapAsLayer)
				{
					packedData.setW(-packedData.getW());
				}

				m_quads.push_back(SFillQuad::Get(
					packedData,
					destCoord.getZ() - destCoord.getX() + 1,
					destCoord.getW() - destCoord.getY() + 1,
					destCoord.getX(),
					destCoord.getY(),
					m_indirectionTextureSize, m_indirectionTextureSize
				));

				++m_quadsCnt[z];

				for (int32_t j = 0; j < 4; ++j)
				{
					QuadTreeNode* pChild = pNode->mChildren[j];
					if (pChild && IsNodeAcceptableForIndexing(pChild))
					{
						m_indexedNodes.push_back(pChild);
					}
				}

			}
		}
		m_indexedNodes.resize(numIndexedNodes);
	}
	void ResetIndirectionTextureData()
	{
		memset(m_quadsCnt, 0, sizeof(m_quadsCnt));
		m_quads.resize(0);
		m_quads.reserve(PACKED_QUADS_ARRAY_REGS);

		m_lodClampQuads.resize(1);
		m_lodClampQuads[0] = SFillQuad::Get(vec4(1.f), m_indirectionTextureSize,
			m_indirectionTextureSize, 0, 0, m_indirectionTextureSize, m_indirectionTextureSize);
	}

	const vec3 ProjectToTS(const vec3& pointLS, const AABB& bboxLS, const vec3& cameraOffset)
	{
		vec3 bboxLSSize = calculateAABBSize(&bboxLS);
		return vec3(
			(pointLS.getX() - bboxLS.minBounds.getX()) / bboxLSSize.getX(),
			1.0f - (pointLS.getY() - bboxLS.minBounds.getY()) / bboxLSSize.getY(),
			-dot(m_lightDir, (m_invLightRotMat * vec4(pointLS, 1.0)).getXYZ() + cameraOffset) / gs_ASMTileFarPlane);
	}

	bool ShouldNodeExist(const AABB& bbox, uint8_t refinement) const
	{
		return Get3DRefinementDistanceSq(bbox, m_refinementPoint) < fabsf(m_cfg.m_refinementDistanceSq[refinement]) ?
			(m_cfg.m_refinementDistanceSq[refinement] < 0 || m_frustumHull.Intersects(bbox)) : false;
	}

	void FillLODClampTextureData()
	{
		if (!IsValid() || m_indexedNodes.empty())
		{
			return;
		}

		size_t numIndexedNodes = m_indexedNodes.size();
		uint32_t i = 0;

		for (int32_t z = m_cfg.m_maxRefinement; z >= 0; --z)
		{
			float clampValue = static_cast<float>(z) /
				static_cast<float>(gs_ASMMaxRefinement);

			size_t numNodes = m_indexedNodes.size();

			for (; i < numNodes; ++i)
			{
				QuadTreeNode* pNode = m_indexedNodes[i];
				ASMTileCacheEntry* pTile = pNode->m_pTile;

				if (z < m_cfg.m_maxRefinement)
				{
					vec4 packedData;
					ivec4 destCoord;
					GetIndirectionTextureData(pTile, packedData, destCoord);

					m_lodClampQuads.push_back(SFillQuad::Get(
						vec4(clampValue),
						destCoord.getZ() - destCoord.getX() + 1,
						destCoord.getW() - destCoord.getY() + 1,
						destCoord.getX(),
						destCoord.getY(),
						m_indirectionTextureSize, m_indirectionTextureSize
					));
				}

				for (int32_t j = 0; j < 4; ++j)
				{
					QuadTreeNode* pChild = pNode->mChildren[j];
					if (pChild && pChild->m_pTile)
					{
						m_indexedNodes.push_back(pChild);
					}
				}

			}
		}
		m_indexedNodes.resize(numIndexedNodes);
	}
	void UpdateIndirectionTexture(RenderTarget* indirectionTexture,
		const ASMSShadowMapRenderContext& context, bool disableHierarchy, bool isPreRender)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
#ifdef _DURANGO
		if (mFirstTimeRender)
		{
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = m_indirectionTexturesMips[m_cfg.m_maxRefinement]->mDesc.mClearValue;
			mFirstTimeRender = false;
		}
#endif
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		ASMRendererContext* curRendererContext = context.m_pRendererContext;

		IndirectionRenderData* finalIndirectionRenderData = disableHierarchy ?
			(&gASMTickData.mPrerenderIndirectionRenderData) :
			(&gASMTickData.mIndirectionRenderData);



		IndirectionRenderData& indirectionRenderData = *finalIndirectionRenderData;

		SFillQuad clearQuad = SFillQuad::Get(vec4(0.f),
			m_indirectionTextureSize, m_indirectionTextureSize, 0, 0, m_indirectionTextureSize, m_indirectionTextureSize);


		ASMPackedAtlasQuadsUniform packedClearAtlasQuads = {};
		packedClearAtlasQuads = *(ASMPackedAtlasQuadsUniform*)&clearQuad;


		uint64_t fullPackedBufferSize = PACKED_QUADS_ARRAY_REGS * sizeof(float) * 4;

		BufferUpdateDesc clearUpdateUbDesc =
		{ indirectionRenderData.pBufferASMClearIndirectionQuadsUniform,
			&packedClearAtlasQuads };


		updateResource(&clearUpdateUbDesc);

		uint32_t firstQuad = 0;
		uint32_t numQuads = 0;

		for (int32_t mip = m_cfg.m_maxRefinement; mip >= 0; --mip)
		{
			numQuads += m_quadsCnt[mip];

			TextureBarrier targetBarrier[] = {
				{m_indirectionTexturesMips[mip]->pTexture, RESOURCE_STATE_RENDER_TARGET} };

			cmdResourceBarrier(curRendererContext->m_pCmd, 0, NULL, 1, targetBarrier);

			setRenderTarget(curRendererContext->m_pCmd, 1, &m_indirectionTexturesMips[mip],
				NULL, &loadActions, vec2(0.f),
				vec2((float)m_indirectionTexturesMips[mip]->mDesc.mWidth,
				(float)m_indirectionTexturesMips[mip]->mDesc.mHeight)
			);

			//------------------Clear ASM indirection quad
			cmdBindPipeline(curRendererContext->m_pCmd, indirectionRenderData.m_pGraphicsPipeline);
			cmdBindDescriptorSet(curRendererContext->m_pCmd, gFrameIndex, pDescriptorSetASMFillIndirection[0]);
			cmdDraw(curRendererContext->m_pCmd, 6, 0);

			if (numQuads > 0)
			{
				BufferUpdateDesc updateIndirectionUBDesc =
				{
					indirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[mip],
					&m_quads[firstQuad]
				};
				uint64_t curBufferSize = sizeof(vec4) * 2 * numQuads;
				updateIndirectionUBDesc.mSize = curBufferSize;

				updateResource(&updateIndirectionUBDesc);

				cmdBindDescriptorSet(curRendererContext->m_pCmd, gFrameIndex * (gs_ASMMaxRefinement + 1) + mip, pDescriptorSetASMFillIndirection[disableHierarchy ? 2 : 1]);
				cmdDraw(curRendererContext->m_pCmd, 6 * numQuads, 0);
			}

			if (disableHierarchy)
			{
				firstQuad += numQuads;
				numQuads = 0;
			}

			cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		}

	}
	void UpdateLODClampTexture(RenderTarget* lodClampTexture,
		const ASMSShadowMapRenderContext& context)
	{
		ASMRendererContext* rendererContext = context.m_pRendererContext;
		Cmd* pCurCmd = rendererContext->m_pCmd;

		
		TextureBarrier lodBarrier[] = {
			{lodClampTexture->pTexture, RESOURCE_STATE_RENDER_TARGET} };

		cmdResourceBarrier(pCurCmd, 0, NULL, 1, lodBarrier);


		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		setRenderTarget(pCurCmd, 1, &lodClampTexture, NULL, &loadActions,
			vec2(0.f, 0.f), vec2((float)lodClampTexture->mDesc.mWidth,
			(float)lodClampTexture->mDesc.mHeight));


		BufferUpdateDesc updateBufferDesc = {};
		updateBufferDesc.pBuffer = pBufferASMLodClampPackedQuadsUniform[gFrameIndex];
		updateBufferDesc.pData = &m_lodClampQuads[0];
		updateBufferDesc.mSize =
			sizeof(SFillQuad) * m_lodClampQuads.size();

		updateResource(&updateBufferDesc);

		cmdBindPipeline(pCurCmd, pPipelineASMFillLodClamp);
		cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetASMFillLodClamp);
		cmdDraw(pCurCmd, static_cast<uint32_t>(m_lodClampQuads.size()) * 6u, 0);

		cmdBindRenderTargets(pCurCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

private:
	bool mFirstTimeRender;
};



void ASMTileCache::RenderTiles(
	uint32_t numTiles,
	ASMTileCacheEntry** tiles,
	RenderTarget* workBufferDepth,
	RenderTarget* workBufferColor,
	ASMSShadowMapRenderContext& context,
	bool allowDEM)
{
	if (!numTiles)
		return;

	ASMRendererContext* curRendererContext = context.m_pRendererContext;

	Cmd* pCurCmd = curRendererContext->m_pCmd;
	Renderer* pRenderer = curRendererContext->m_pRenderer;

	uint32_t workBufferWidth = workBufferDepth->mDesc.mWidth;
	uint32_t workBufferHeight = workBufferDepth->mDesc.mHeight;
	uint32_t numTilesW = workBufferWidth / gs_ASMTileSize;
	uint32_t numTilesH = workBufferHeight / gs_ASMTileSize;
	uint32_t maxTilesPerPass = numTilesW * numTilesH;

	//basically this code changes pixel center from DX10 to DX9 (DX9 pixel center is integer while DX10 is (0.5, 0.5)
	mat4 pixelCenterOffsetMatrix = mat4::translation(vec3(1.f / static_cast<float>(workBufferWidth),
		-1.f / static_cast<float>(workBufferHeight), 0.f));


	SCopyQuad* copyDepthQuads = (SCopyQuad*)conf_malloc(sizeof(SCopyQuad) * (maxTilesPerPass + numTiles));
	SCopyQuad* copyDEMQuads = copyDepthQuads + maxTilesPerPass;

	float invAtlasWidth = 1.0f / float(m_depthAtlasWidth);
	float invAtlasHeight = 1.0f / float(m_depthAtlasHeight);

	uint32_t numCopyDEMQuads = 0;
	for (uint32_t i = 0; i < numTiles;)
	{
		uint32_t tilesToRender = min(maxTilesPerPass, numTiles - i);

		LoadActionsDesc loadActions = {};
		loadActions.mClearDepth = workBufferDepth->mDesc.mClearValue;
		loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;

		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;





		if (tilesToRender > 0)
		{
			
			TextureBarrier textureBarriers[] = { {
					workBufferDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE} };

			BufferBarrier bufferBarriers[] = { {pIndirectPosBuffer, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER} };
			cmdResourceBarrier(pCurCmd, 1, bufferBarriers, 1, textureBarriers);

			setRenderTarget(context.m_pRendererContext->m_pCmd, 0, NULL,
				workBufferDepth, &loadActions, vec2(0.f, 0.f), vec2((float)workBufferDepth->mDesc.mWidth, (float)workBufferDepth->mDesc.mHeight));
		}

		for (uint32_t j = 0; j < tilesToRender; ++j)
		{
			ASMTileCacheEntry* pTile = tiles[i + j];

			//_ASSERT( !pTile->m_isLayer || pTile->m_pOwner->m_pTile->IsReady() );

			vec2 viewPortLoc(static_cast<float>((j % numTilesW) * gs_ASMTileSize),
				static_cast<float>((j / numTilesW) * gs_ASMTileSize));
			vec2 viewPortSize(gs_ASMTileSize);

			ASMProjectionData renderProjection;
			renderProjection.mViewMat = pTile->mRenderProjectionData.mViewMat;
			renderProjection.mProjMat = pixelCenterOffsetMatrix * pTile->mRenderProjectionData.mProjMat;

			renderProjection.mViewProjMat = renderProjection.mProjMat * renderProjection.mViewMat;

			if (pTile->m_isLayer)
			{

			}
			else
			{
				RenderIndirectModelSceneTile(viewPortLoc, viewPortSize,
					renderProjection, false, context);
			}
			//WARNING -1 multiplied cause reversed z buffer
			const float depthBias =
				-1.0f * CalcDepthBias(pTile->mRenderProjectionData.mProjMat,
					vec3(3.5f, 3.5f, 1.0f),
					gs_ASMTileSize,
					gs_ASMTileSize,
					16);
			copyDepthQuads[j] = SCopyQuad::Get(
				vec4(0, 0, depthBias, 0),
				gs_ASMTileSize, gs_ASMTileSize,
				pTile->m_viewport.x - gs_ASMTileBorderTexels,
				pTile->m_viewport.y - gs_ASMTileBorderTexels,
				m_depthAtlasWidth, m_depthAtlasHeight,
				gs_ASMTileSize, gs_ASMTileSize,
				static_cast<uint32_t>(viewPortLoc.getX()), static_cast<uint32_t>(viewPortLoc.getY()),
				workBufferWidth, workBufferHeight);

			bool generateDEM = pTile->m_refinement <= pTile->m_pFrustum->GetDEMMinRefinement(pTile->m_isLayer);
			if (generateDEM && (allowDEM || pTile->IsReady()))
			{
				StartDEM(pTile, copyDEMQuads[numCopyDEMQuads++]);
			}
		}


		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier copyDepthBarrier[] = {
			{ pRenderTargetASMDepthAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ workBufferDepth->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
		};

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, copyDepthBarrier);

		LoadActionsDesc copyDepthQuadLoadAction = {};
		copyDepthQuadLoadAction.mClearColorValues[0] = pRenderTargetASMDepthAtlas->mDesc.mClearValue;
		copyDepthQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;

#ifdef _DURANGO
		if (mDepthFirstTimeRender)
		{
			copyDepthQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			mDepthFirstTimeRender = false;
	}
#endif

		copyDepthQuadLoadAction.mClearDepth = pRenderTargetASMDepthAtlas->mDesc.mClearValue;
		copyDepthQuadLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		copyDepthQuadLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		setRenderTarget(pCurCmd, 1, &pRenderTargetASMDepthAtlas, NULL,
			&copyDepthQuadLoadAction, vec2(0.f, 0.f),
			vec2((float)pRenderTargetASMDepthAtlas->mDesc.mWidth, (float)pRenderTargetASMDepthAtlas->mDesc.mHeight));


			   

		ASMAtlasQuadsUniform asmAtlasQuadsData = {};

		//WARNING: only using one buffer, but there is a possibility of multiple tile copying
		//this code won't work if tilesToRender exceeded more than one
		//however based on sample code tilesToRender never exceed 1, this assumption may be wrong
		//or they maybe correct if we follow exactly as the sample code settings
		for (uint32_t i = 0; i < tilesToRender; ++i)
		{
			const SCopyQuad& quad = copyDepthQuads[i];

			asmAtlasQuadsData.mMiscData = quad.m_misc;
			asmAtlasQuadsData.mPosData = quad.m_pos;
			asmAtlasQuadsData.mTexCoordData = quad.m_texCoord;

			BufferUpdateDesc updateUbDesc = {
				pBufferASMAtlasQuadsUniform[gFrameIndex],
				&asmAtlasQuadsData
			};
			updateResource(&updateUbDesc);
		}

		cmdBindPipeline(pCurCmd, pPipelineASMCopyDepthQuadPass);
		cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetASMCopyDepthQuadPass[0]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetASMCopyDepthQuadPass[1]);
		cmdDraw(pCurCmd, 6, 0);

		i += tilesToRender;

		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
}

	if (numCopyDEMQuads > 0)
	{

		TextureBarrier asmCopyDEMBarrier[] = { {  pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_RENDER_TARGET},
		{pRenderTargetASMDepthAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE} };

		cmdResourceBarrier(pCurCmd, 0, NULL, 2, asmCopyDEMBarrier);

		LoadActionsDesc copyDEMQuadLoadAction = {};
		copyDEMQuadLoadAction.mClearColorValues[0] = pRenderTargetASMDepthAtlas->mDesc.mClearValue;
		copyDEMQuadLoadAction.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		copyDEMQuadLoadAction.mClearDepth = pRenderTargetASMDepthAtlas->mDesc.mClearValue;
		copyDEMQuadLoadAction.mLoadActionStencil = LOAD_ACTION_DONTCARE;

		copyDEMQuadLoadAction.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		setRenderTarget(pCurCmd, 1, &pRenderTargetASMDEMAtlas, NULL,
			&copyDEMQuadLoadAction, vec2(0.f, 0.f), vec2((float)pRenderTargetASMDEMAtlas->mDesc.mWidth,
			(float)pRenderTargetASMDEMAtlas->mDesc.mHeight));

	

		BufferUpdateDesc copyDEMQuadUpdateUbDesc = {};
		copyDEMQuadUpdateUbDesc.pBuffer = pBufferASMCopyDEMPackedQuadsUniform[gFrameIndex];
		copyDEMQuadUpdateUbDesc.pData = &copyDEMQuads[0];
		copyDEMQuadUpdateUbDesc.mSize = sizeof(SCopyQuad) * numCopyDEMQuads;

		updateResource(&copyDEMQuadUpdateUbDesc);

		cmdBindPipeline(pCurCmd, pPipelineASMCopyDEM);
		cmdBindDescriptorSet(pCurCmd, 0, pDescriptorSetASMCopyDEM[0]);
		cmdBindDescriptorSet(pCurCmd, gFrameIndex, pDescriptorSetASMCopyDEM[1]);
		cmdDraw(pCurCmd, numCopyDEMQuads * 6, 0);

		cmdBindRenderTargets(curRendererContext->m_pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

	}
	conf_free(copyDepthQuads);
}

void ASMTileCacheEntry::PrepareRender(const ASMSShadowMapPrepareRenderContext& context)
{
	mRenderProjectionData = m_pFrustum->CalcCamera(m_BBox, *context.m_worldCenter,
		vec2((static_cast<float> (gs_ASMTileSize) / static_cast<float>(m_viewport.w)))
	);
}


class ASM
{
public:
	ASMTileCache* m_cache;
	ASMFrustum* m_longRangeShadows;
	ASMFrustum* m_longRangePreRender;
public:
	
	friend class ASMTileCacheEntry;
	friend class ASMTileCache;
	friend class ASMFrustum;

	friend class ASMFrustum;
	friend class ASMTileCacheEntry;
public:
	ASM()
		:m_preRenderDone(false)
	{
		m_cache = conf_new(ASMTileCache);
		static const ASMFrustum::Config longRangeCfg =
		{
			gs_ASMLargestTileWorldSize, gs_ASMDistanceMax, gs_ASMMaxRefinement, INT_MAX, gsASMIndexSize, true,
			{ SQR(gs_ASMDistanceMax), SQR(120.0f), SQR(60.0f), SQR(30.0f), SQR(10.0f) }
		};
		m_longRangeShadows = conf_new(ASMFrustum, longRangeCfg, true, false);
		m_longRangePreRender = conf_new(ASMFrustum, longRangeCfg, true, true);
		Reset();
	}
	~ASM()
	{
		conf_delete(m_cache);
		conf_delete(m_longRangeShadows);
		conf_delete(m_longRangePreRender);
	}


	void Load(const ASMRenderTargets& renderTargets)
	{
		m_longRangePreRender->Load(renderTargets, true);
		m_longRangeShadows->Load(renderTargets, false);
	}

	bool PrepareRender(
		const ASMProjectionData& mainViewProjection,
		bool disablePreRender)
	{
		m_longRangeShadows->CreateTiles(m_cache, mainViewProjection);
		m_longRangePreRender->CreateTiles(m_cache, mainViewProjection);

		if (m_cache->NothingToRender())
		{
			bool keepRendering = true;

			for (uint32_t i = 0; i <= gs_ASMMaxRefinement && keepRendering; ++i)
			{
				keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, i, false) < 0;

				if (keepRendering)
				{
					keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, i, true) < 0;
				}
				if (keepRendering && m_longRangePreRender->IsValid() && i == 0 && !disablePreRender)
					keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, i, false) < 0;
			}

			if (keepRendering && m_longRangePreRender->IsValid() && !disablePreRender)
			{

				keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, INT_MAX, false) < 0;

				if (keepRendering)
				{
					keepRendering = m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangePreRender, INT_MAX, true) < 0;
				}
				if (keepRendering)
				{
					m_preRenderDone = true;
				}

			}

			if (keepRendering)
			{
				m_cache->AddTileFromRenderQueueToRenderBatch(m_longRangeShadows, INT_MAX, false);
			}
		}

		vec3 mainViewCameraPosition = mainViewProjection.mInvViewMat.getCol3().getXYZ();
		ASMSShadowMapPrepareRenderContext context = { &mainViewCameraPosition };
		return m_cache->PrepareRenderTilesBatch(context);
	}

	void Render(
		RenderTarget* pRenderTargetDepth,
		RenderTarget* pRenderTargetColor,
		ASMRendererContext& renderContext,
		ASMProjectionData* projectionRender)
	{
		ASMSShadowMapRenderContext context = { &renderContext,  projectionRender };

		if (!m_cache->NothingToRender())
		{
			m_cache->RenderTilesBatch(
				pRenderTargetDepth,
				pRenderTargetColor,
				context);
		}
		
		m_cache->CreateDEM(pRenderTargetColor, context, false);
		m_cache->CreateDEM(pRenderTargetColor, context, true);

		m_longRangeShadows->BuildTextures(context, false);

		if (m_longRangePreRender->IsValid())
		{
			m_longRangePreRender->BuildTextures(context, true);
		}

	}

	void Reset()
	{
		m_longRangeShadows->Reset();
		m_longRangePreRender->Reset();
		m_preRenderDone = false;
	}


	void Tick(const ASMCpuSettings& asmCpuSettings,
		ICameraController* lightCameraController, const vec3& lightDir,
		const vec3& halfwayLightDir, uint32_t currentTime, uint32_t dt,
		bool disableWarping, bool forceUpdate, uint32_t updateDeltaTime)
	{
		//mTickData = tickData;

		vec3 sunDir = lightDir;

		//vec3 sunDir = GetLightDirection(currentTime);

		float deltaTime = float(dt) * 0.001f;

		bool isUpdated = false;
		if (!m_longRangeShadows->IsValid())
		{
			m_longRangeShadows->Set(lightCameraController, sunDir);
			isUpdated = true;
		}
		else if (forceUpdate)
		{
			m_longRangePreRender->Reset();

			m_longRangePreRender->Set(lightCameraController, sunDir);
			m_preRenderDone = false;

			isUpdated = true;
		}
		else if (!m_longRangePreRender->IsValid())
		{
			//vec3 nextSunDir = GetLightDirection(currentTime + (updateDeltaTime >> 1));
			//vec3 nextSunDir = lightDir;
			vec3 nextSunDir = halfwayLightDir;
			isUpdated = m_longRangeShadows->IsLightDirDifferent(nextSunDir);

			if (isUpdated)
			{
				m_longRangePreRender->Set(lightCameraController, nextSunDir);
				m_preRenderDone = false;
			}
		}
		m_longRangeShadows->UpdateWarpVector(asmCpuSettings, sunDir, disableWarping);
		m_longRangePreRender->UpdateWarpVector(asmCpuSettings, sunDir, disableWarping);

		m_cache->Tick(deltaTime);

		if (m_longRangePreRender->IsValid() && m_preRenderDone && m_cache->IsFadeInFinished(m_longRangePreRender))
		{
			eastl::swap(m_longRangeShadows, m_longRangePreRender);

			m_longRangePreRender->Reset();
			m_preRenderDone = false;
		}

		++asmFrameCounter;
	}

	bool NothingToRender() const
	{
		return m_cache->NothingToRender();
	}

	bool PreRenderAvailable() const
	{
		return m_longRangePreRender->IsValid();
	}

protected:
	bool m_preRenderDone;
};

bool ASMTileCache::IsFadeInFinished(const ASMFrustum* pFrustum) const
{
	for (int32_t i = 0; i < m_readyTiles.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_readyTiles[i];
		if (pTile->m_frustumID == pFrustum->m_ID && pTile->m_fadeInFactor > 0)
		{
			return false;
		}
	}
	return true;
}

template< class T >
int32_t ASMTileCache::AddTileToRenderBatch(
	T& tilesQueue,
	ASMFrustum* pFrustum,
	int32_t maxRefinement,
	bool isLayer)
{
	if (!pFrustum->IsValid())
	{
		return -1;
	}

	ASMTileCacheEntry* pTileToRender = nullptr;
	float minDistSq = FLT_MAX;
	uint8_t refinement = UCHAR_MAX;
	for (uint32_t i = 0; i < tilesQueue.size(); ++i)
	{
		ASMTileCacheEntry* pTile = tilesQueue[i];
		if (pFrustum == pTile->m_pFrustum && isLayer == pTile->m_isLayer &&
			(!pTile->m_isLayer || pTile->m_pOwner->m_pTile->IsReady()))
		{
			float distSq = GetRefinementDistanceSq(pTile->m_BBox, pFrustum->m_refinementPoint);
			if (pTile->m_refinement < refinement ||
				(refinement == pTile->m_refinement && distSq < minDistSq))
			{
				refinement = pTile->m_refinement;
				minDistSq = distSq;
				pTileToRender = pTile;
			}
		}
	}

	if (pTileToRender == nullptr ||
		pTileToRender->m_refinement > maxRefinement)
	{
		return -1;
	}

	tilesQueue.Remove(pTileToRender);
	m_renderBatch.Add(pTileToRender);
	return pTileToRender->m_refinement;
}

template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
void ASMTileCacheEntry::Allocate(QuadTreeNode* pOwner, ASMFrustum* pFrustum)
{
	m_pCache->m_freeTiles.Remove(this);
	m_pOwner = pOwner;
	m_pFrustum = pFrustum;
	m_refinement = pOwner->mRefinement;

	(pOwner->*TileAccessor)() = this;

	if (m_frustumID == pFrustum->m_ID && !(m_BBox.minBounds != pOwner->mBBox.minBounds || m_BBox.maxBounds != pOwner->mBBox.maxBounds) && m_isLayer == isLayer)
	{
		MarkReady();
	}
	else
	{
		m_frustumID = pFrustum->m_ID;
		m_BBox = pOwner->mBBox;
		m_isLayer = isLayer;
		m_pCache->m_renderQueue.Add(this);
	}
}
template< ASMTileCacheEntry*& (QuadTreeNode::*TileAccessor)(), bool isLayer >
ASMTileCacheEntry* ASMTileCache::Allocate(QuadTreeNode* pNode, ASMFrustum* pFrustum)
{
	
	ASMTileCacheEntry* pTileToAlloc = NULL;

	if (m_freeTiles.empty())
	{
		uint8_t minRefinement = pNode->mRefinement;
		float minDistSq = GetRefinementDistanceSq(pNode->mBBox, pFrustum->m_refinementPoint);


		// try to free visually less important tile (the one further from viewer or deeper in hierarchy)
		for (uint32_t i = 0; i < m_tiles.size(); ++i)
		{

			ASMTileCacheEntry* pTile = m_tiles[i];

			if (pTile->m_refinement < minRefinement)
			{
				continue;
			}

			float distSq = GetRefinementDistanceSq(pTile->m_BBox,
				pTile->m_pFrustum->m_refinementPoint);

			if (pTile->m_refinement == minRefinement)
			{
				if ((distSq == minDistSq && !pTile->m_isLayer) || distSq < minDistSq)
				{
					continue;
				}
			}

			pTileToAlloc = pTile;
			minRefinement = pTile->m_refinement;
			minDistSq = distSq;
		}

		if (!pTileToAlloc)
		{
			return NULL;
		}
		pTileToAlloc->Free();
	}

	for (uint32_t i = 0; i < m_freeTiles.size(); ++i)
	{
		ASMTileCacheEntry* pTile = m_freeTiles[i];

		if (pTile->m_frustumID == pFrustum->m_ID &&
			!(pTile->m_BBox.minBounds != pNode->mBBox.minBounds || pTile->m_BBox.maxBounds != pNode->mBBox.maxBounds ) &&
			pTile->m_isLayer == isLayer)
		{
			pTileToAlloc = pTile;
			++m_cacheHits;
			break;
		}
	}

	if (!pTileToAlloc)
	{
		uint8_t refinement = 0;
		uint32_t LRUdt = 0;

		for (uint32_t i = 0; i < m_freeTiles.size(); ++i)
		{
			ASMTileCacheEntry* pTile = m_freeTiles[i];
			if (pTile->m_refinement < refinement)
			{
				continue;
			}
			uint32_t dt = asmFrameCounter - pTile->m_lastFrameUsed;
			if (pTile->m_refinement == refinement && dt < LRUdt)
			{
				continue;
			}
			pTileToAlloc = pTile;
			refinement = pTile->m_refinement;
			LRUdt = dt;
		}

		if (pTileToAlloc)
		{
			pTileToAlloc->Invalidate();
		}
	}

	if (pTileToAlloc)
	{
		pTileToAlloc->Allocate<TileAccessor, isLayer>(pNode, pFrustum);
		++m_tileAllocs;
	}
	return pTileToAlloc;

}


ASM* pASM;



void SetupASMDebugTextures()
{
	if (!gASMCpuSettings.mShowDebugTextures)
	{
		if (pUIASMDebugTexturesWindow)
		{
			pUIASMDebugTexturesWindow->mActive = false;
		}
	}
	else
	{
		float scale = 0.15f;
		float2 screenSize = { (float)pRenderTargetVBPass->mDesc.mWidth,
			(float)pRenderTargetVBPass->mDesc.mHeight };
		float2 texSize = screenSize * scale;

		if (!pUIASMDebugTexturesWindow)
		{
			GuiDesc guiDesc = {};
			guiDesc.mStartSize = vec2(guiDesc.mStartSize.getX(), guiDesc.mStartSize.getY());
			guiDesc.mStartPosition.setY(screenSize.getY() - texSize.getY() - 50.f);
			pUIASMDebugTexturesWindow = gAppUI.AddGuiComponent("ASM Debug Textures Info", &guiDesc);
			ASSERT(pUIASMDebugTexturesWindow);

			DebugTexturesWidget widget("Debug RTs");
			pUIASMDebugTexturesWindow->AddWidget(widget);


			eastl::vector<Texture*> rts;

			rts.push_back(pRenderTargetASMDepthAtlas->pTexture);
			rts.push_back(pRenderTargetASMIndirection[0]->pTexture);

			((DebugTexturesWidget*)(pUIASMDebugTexturesWindow->mWidgets[0]))->SetTextures(rts, texSize);
		}

		pUIASMDebugTexturesWindow->mActive = true;
	}
}


static void createScene()
{
	/************************************************************************/
	// Initialize Models
	/************************************************************************/
	gMeshInfoData[0].mColor = vec4(1.f);
	gMeshInfoData[0].mScale = float3(MESH_SCALE / SAN_MIGUEL_ORIGINAL_SCALE);
	gMeshInfoData[0].mScaleMat = mat4::scale(vec3( gMeshInfoData[0].mScale.x, gMeshInfoData[0].mScale.y, gMeshInfoData[0].mScale.z) );
	float finalXTranslation = SAN_MIGUEL_OFFSETX;
	gMeshInfoData[0].mTranslation = float3(finalXTranslation, 0.f, 0.f);
	gMeshInfoData[0].mOffsetTranslation = float3(-(SAN_MIGUEL_ORIGINAL_OFFSETX), 0.f, 0.f);
	gMeshInfoData[0].mTranslationMat = mat4::translation(vec3(gMeshInfoData[0].mTranslation.x, 
		gMeshInfoData[0].mTranslation.y, gMeshInfoData[0].mTranslation.z));
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
struct GuiController
{
	static void addGui();
	static void removeGui();
	static void updateDynamicUI();

	static DynamicUIWidgets esmDynamicWidgets;
	static DynamicUIWidgets sdfDynamicWidgets;
	static DynamicUIWidgets asmDynamicWidgets;
	static DynamicUIWidgets bakedSDFDynamicWidgets;
	static SliderFloat3Widget* mLightPosWidget;

	static ShadowType currentlyShadowType;
}; 
ShadowType        GuiController::currentlyShadowType;
DynamicUIWidgets GuiController::esmDynamicWidgets;
DynamicUIWidgets GuiController::sdfDynamicWidgets;
DynamicUIWidgets GuiController::asmDynamicWidgets;
DynamicUIWidgets GuiController::bakedSDFDynamicWidgets;
SliderFloat3Widget* GuiController::mLightPosWidget = NULL;
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
class LightShadowPlayground: public IApp
{
	public:
	LightShadowPlayground()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	static void refreshASM()
	{
		pASM->Reset();
	}

	static void resetLightDir()
	{
		asmCurrentTime = 0.f;
		refreshASM();
	}

	void initSDFMeshes()
	{
		pSDFMeshes[0] = conf_new(SDFMesh);
		pSDFMeshes[1] = conf_new(SDFMesh);
		pSDFMeshes[2] = conf_new(SDFMesh);
        PathHandle model1Path = fsCopyPathInResourceDirectory(RD_MESHES, gSDFModelNames[1]);
		loadSDFMeshAlphaTested(pThreadSystem, model1Path, pSDFMeshes[1], MESH_SCALE,
			SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gSDFVolumeInstances, &GenerateVolumeDataFromFile);

        PathHandle model0Path = fsCopyPathInResourceDirectory(RD_MESHES, gSDFModelNames[0]);
		loadSDFMesh(pThreadSystem, model0Path, pSDFMeshes[0], MESH_SCALE,
			SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gSDFVolumeInstances, &GenerateVolumeDataFromFile);

        PathHandle model2Path = fsCopyPathInResourceDirectory(RD_MESHES, gSDFModelNames[2]);
		loadSDFMesh(pThreadSystem, model2Path, pSDFMeshes[2], MESH_SCALE,
			SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gSDFVolumeInstances, &GenerateVolumeDataFromFile);

		gGenerateMissingSDFTask[0] = { pThreadSystem, pSDFMeshes[0], &gSDFVolumeInstances };
		gGenerateMissingSDFTask[1] = { pThreadSystem, pSDFMeshes[1], &gSDFVolumeInstances };
		gGenerateMissingSDFTask[2] = { pThreadSystem, pSDFMeshes[2], &gSDFVolumeInstances };
	}

	void destroySDFMeshes()
	{
		conf_delete(pSDFMeshes[0]);
		conf_delete(pSDFMeshes[1]);
		conf_delete(pSDFMeshes[2]);
	}

	static void checkForMissingSDFData()
	{
		if (gAppSettings.mIsGeneratingSDF)
		{
			LOGF(LogLevel::eINFO, "Generating missing SDF has been executed...");
			return;
		}
		addThreadSystemTask(pThreadSystem, DoGenerateMissingSDFTaskData, &gGenerateMissingSDFTask[0], 0);
		addThreadSystemTask(pThreadSystem, DoGenerateMissingSDFTaskData, &gGenerateMissingSDFTask[1], 0);
		addThreadSystemTask(pThreadSystem, DoGenerateMissingSDFTaskData, &gGenerateMissingSDFTask[2], 0);
	}

	static void calculateCurSDFMeshesProgress()
	{
		gSDFProgressValue = 0;
		for (int32_t i = 0; i < 3; ++i)
		{
			gSDFProgressValue += pSDFMeshes[i]->mTotalGeneratedSDFMeshes;
		}
	}

	static uint32_t getMaxSDFMeshesProgress()
	{
		uint32_t max = 0;
		for (int32_t i = 0; i < 3; ++i)
		{
			max += pSDFMeshes[i]->mTotalSDFMeshes;
		}
		return max;
	}
	
	static void initSDFVolumeTextureAtlasData()
	{
		for (int32_t i = 0; i < gSDFVolumeInstances.size(); ++i)
		{
			if (!gSDFVolumeInstances[i])
			{
				LOGF(LogLevel::eINFO, "SDF volume data index %d in Init_SDF_Volume_Texture_Atlas_Data NULL", i);
				continue;
			}
			pSDFVolumeTextureAtlas->AddVolumeTextureNode(&gSDFVolumeInstances[i]->mSDFVolumeTextureNode);
		}
	}

	bool Init() override
	{
        // FILE PATHS
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/09_LightShadowPlayground");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,        "../../../../Art/SanMiguel_3/Textures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,          "../../../../Art/SanMiguel_3/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,    "../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,      "../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_OTHER_FILES,      "../../UnitTestResources/SDF");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,  "../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,    "../../../../Middleware_3/UI");
        }
        
		initThreadSystem(&pThreadSystem);

		// Overwrite rootpath is required because Textures and meshes are not in /Textures and /Meshes.
		// We need to set the modified root path so that filesystem can find the meshes and textures.
        PathHandle mainDirPath = fsCopyPathForResourceDirectory(RD_ROOT);
#ifndef _DURANGO
        PathHandle sdfDirPath = fsCopyPathForResourceDirectory(RD_OTHER_FILES);
        fsCreateDirectory(sdfDirPath);
#endif


		RendererDesc settings = { NULL };
		initRenderer(GetName(), &settings, &pRenderer);

		

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);


		QueueDesc computeQueueDesc = {};
		computeQueueDesc.mType = CMD_POOL_COMPUTE;
		addQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

		addCmdPool(pRenderer, pComputeQueue, false, &pComputeCmdPool);
		addCmd_n(pComputeCmdPool, false, gImageCount, &ppComputeCmds);



		DepthStateDesc depthStateEnabledDesc = {};
		depthStateEnabledDesc.mDepthFunc = CMP_GEQUAL;
		depthStateEnabledDesc.mDepthWrite = true;
		depthStateEnabledDesc.mDepthTest = true;

		DepthStateDesc depthStateLEQUALEnabledDesc = {};
		depthStateLEQUALEnabledDesc.mDepthFunc = CMP_LEQUAL;
		depthStateLEQUALEnabledDesc.mDepthWrite = true;
		depthStateLEQUALEnabledDesc.mDepthTest = true;

		DepthStateDesc depthStateTestOnlyDesc = {};
		depthStateTestOnlyDesc.mDepthFunc = CMP_EQUAL;
		depthStateTestOnlyDesc.mDepthWrite = false;
		depthStateTestOnlyDesc.mDepthTest = true;

		DepthStateDesc depthStateStencilShadow = {};
		depthStateStencilShadow.mDepthFunc = CMP_LESS;
		depthStateStencilShadow.mDepthWrite = false;
		depthStateStencilShadow.mDepthTest = true;

		DepthStateDesc depthStateDisableDesc = {};
		
		
		depthStateStencilShadow.mStencilTest = false;

		addDepthState(pRenderer, &depthStateEnabledDesc, &pDepthStateEnable);
		addDepthState(pRenderer, &depthStateTestOnlyDesc, &pDepthStateTestOnly);
		addDepthState(pRenderer, &depthStateStencilShadow, &pDepthStateStencilShadow);
		addDepthState(pRenderer, &depthStateLEQUALEnabledDesc, &pDepthStateLEQUALEnable);
		addDepthState(pRenderer, &depthStateDisableDesc, &pDepthStateDisable);

		addFence(pRenderer, &pTransitionFences);
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

    if (!gAppUI.Init(pRenderer))
      return false;
    gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		initProfiler();
			   		

		/************************************************************************/
		// Geometry data for the scene
		/************************************************************************/

		BufferLoadDesc boxIbDesc = {};
		boxIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		boxIbDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
		boxIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boxIbDesc.mDesc.mSize = sizeof(gBoxIndices);
		boxIbDesc.pData = gBoxIndices;
		boxIbDesc.ppBuffer = &pBufferBoxIndex;
		addResource(&boxIbDesc);

		uint64_t quadDataSize = sizeof(gQuadVertices);
		BufferLoadDesc quadVbDesc = {};
		quadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadVbDesc.mDesc.mSize = quadDataSize;
		quadVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		quadVbDesc.pData = gQuadVertices;
		quadVbDesc.ppBuffer = &pBufferQuadVertex;
		addResource(&quadVbDesc);

		/************************************************************************/
		// Setup constant buffer data
		/************************************************************************/
		BufferLoadDesc vbConstantUBDesc = {};
		vbConstantUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vbConstantUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		vbConstantUBDesc.mDesc.mSize = sizeof(VisibilityBufferConstants);
		vbConstantUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		vbConstantUBDesc.mDesc.pDebugName = L"Visibility constant Buffer Desc";
		vbConstantUBDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			vbConstantUBDesc.ppBuffer = &pBufferVisibilityBufferConstants[i];
			addResource(&vbConstantUBDesc);
		}

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(MeshInfoUniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#ifdef METAL
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		ubDesc.pData = NULL;

		for (uint32_t j = 0; j < MESH_COUNT; ++j)
		{
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				ubDesc.ppBuffer = &pBufferMeshTransforms[j][i];
				addResource(&ubDesc);

				ubDesc.ppBuffer = &pBufferMeshShadowProjectionTransforms[j][i];
				addResource(&ubDesc);
			}
		}
		BufferLoadDesc ubEsmBlurDesc = {};
		ubEsmBlurDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubEsmBlurDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubEsmBlurDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubEsmBlurDesc.mDesc.mSize = sizeof(ESMInputConstants);
		ubEsmBlurDesc.pData = &gESMUniformData;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubEsmBlurDesc.ppBuffer = &pBufferESMUniform[i];
			addResource(&ubEsmBlurDesc);
		}



		BufferLoadDesc quadUbDesc = {};
		quadUbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		quadUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		quadUbDesc.mDesc.mSize = sizeof(QuadDataUniform);
		quadUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		quadUbDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			quadUbDesc.ppBuffer = &pBufferQuadUniform[i];
			addResource(&quadUbDesc);
		}

		BufferLoadDesc asmAtlasQuadsUbDesc = {};
		asmAtlasQuadsUbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		asmAtlasQuadsUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		asmAtlasQuadsUbDesc.mDesc.mSize = sizeof(ASMAtlasQuadsUniform);
		asmAtlasQuadsUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#ifdef METAL
		asmAtlasQuadsUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		asmAtlasQuadsUbDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			asmAtlasQuadsUbDesc.mDesc.mSize = sizeof(ASMAtlasQuadsUniform);
			
			asmAtlasQuadsUbDesc.ppBuffer = &pBufferASMAtlasQuadsUniform[i];
			addResource(&asmAtlasQuadsUbDesc);
			
			asmAtlasQuadsUbDesc.mDesc.mSize = sizeof(ASMPackedAtlasQuadsUniform);

			asmAtlasQuadsUbDesc.ppBuffer = &pBufferASMClearIndirectionQuadsUniform[i];
			addResource(&asmAtlasQuadsUbDesc);
		}


		BufferLoadDesc asmPackedAtlasQuadsUbDesc = {};
		asmPackedAtlasQuadsUbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		asmPackedAtlasQuadsUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		asmPackedAtlasQuadsUbDesc.mDesc.mSize = sizeof(ASMPackedAtlasQuadsUniform);
		asmPackedAtlasQuadsUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#ifdef METAL
		asmPackedAtlasQuadsUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		for (uint32_t i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			for (uint32_t k = 0; k < gImageCount; ++k)
			{
				asmPackedAtlasQuadsUbDesc.ppBuffer = 
					&pBufferASMPackedIndirectionQuadsUniform[i][k];
				addResource(&asmPackedAtlasQuadsUbDesc);

				asmPackedAtlasQuadsUbDesc.ppBuffer =
					&pBufferASMPackedPrerenderIndirectionQuadsUniform[i][k];
				addResource(&asmPackedAtlasQuadsUbDesc);
			}
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			asmPackedAtlasQuadsUbDesc.ppBuffer = &pBufferASMCopyDEMPackedQuadsUniform[i];
			addResource(&asmPackedAtlasQuadsUbDesc);

			asmPackedAtlasQuadsUbDesc.ppBuffer = &pBufferASMColorToAtlasPackedQuadsUniform[i];
			addResource(&asmPackedAtlasQuadsUbDesc);

			asmPackedAtlasQuadsUbDesc.ppBuffer = &pBufferASMAtlasToColorPackedQuadsUniform[i];
			addResource(&asmPackedAtlasQuadsUbDesc);

			asmPackedAtlasQuadsUbDesc.ppBuffer = &pBufferASMLodClampPackedQuadsUniform[i];
			addResource(&asmPackedAtlasQuadsUbDesc);
		}

		BufferLoadDesc asmDataUbDesc = {};
		asmDataUbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		asmDataUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		asmDataUbDesc.mDesc.mSize = sizeof(ASMUniformBlock);
		asmDataUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#ifdef METAL
		asmDataUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
#endif
		
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			asmDataUbDesc.ppBuffer = &pBufferASMDataUniform[i];
			addResource(&asmDataUbDesc);
		}

		BufferLoadDesc camUniDesc = {};
		camUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		camUniDesc.mDesc.mSize = sizeof(CameraUniform);
		camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT ;
		camUniDesc.pData = &gCameraUniformData;
#ifdef METAL
		camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			camUniDesc.ppBuffer = &pBufferCameraUniform[i];
			addResource(&camUniDesc);
		}
		BufferLoadDesc renderSettingsDesc = {};
		renderSettingsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		renderSettingsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		renderSettingsDesc.mDesc.mSize = sizeof(RenderSettingsUniformData);
		renderSettingsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		renderSettingsDesc.pData = &gRenderSettings;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			renderSettingsDesc.ppBuffer = &pBufferRenderSettings[i];
			addResource(&renderSettingsDesc);
		}

		BufferLoadDesc lightUniformDesc = {};
		lightUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightUniformDesc.mDesc.mSize = sizeof(LightUniformBlock);
		lightUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightUniformDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			lightUniformDesc.ppBuffer = &pBufferLightUniform[i];
			addResource(&lightUniformDesc);
		}
		

		BufferLoadDesc meshSDFUniformDesc = {};
		meshSDFUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		meshSDFUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		meshSDFUniformDesc.mDesc.mSize = sizeof(MeshSDFConstants);
		meshSDFUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		meshSDFUniformDesc.pData = NULL;

		for (uint32 i = 0; i < gImageCount; ++i)
		{
			meshSDFUniformDesc.ppBuffer = &pBufferMeshSDFConstants[i];
			addResource(&meshSDFUniformDesc);
		}


		BufferLoadDesc updateSDFVolumeTextureAtlasUniformDesc = {};
		updateSDFVolumeTextureAtlasUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		updateSDFVolumeTextureAtlasUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		updateSDFVolumeTextureAtlasUniformDesc.mDesc.mSize = sizeof(UpdateSDFVolumeTextureAtlasConstants);
		updateSDFVolumeTextureAtlasUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		updateSDFVolumeTextureAtlasUniformDesc.pData = NULL;

		for (uint32 i = 0; i < gImageCount; ++i)
		{
			updateSDFVolumeTextureAtlasUniformDesc.ppBuffer = 
				&pBufferUpdateSDFVolumeTextureAtlasConstants[i];
			addResource(&updateSDFVolumeTextureAtlasUniformDesc);
		}

		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", RD_TEXTURES))
			return false;

		eastl::string str_formatter = "";
		
		ShaderLoadDesc indirectDepthPassShaderDesc = {};
		indirectDepthPassShaderDesc.mStages[0] = {
			"meshDepthPass.vert",  NULL, 0, RD_SHADER_SOURCES };

		ShaderLoadDesc indirectAlphaDepthPassShaderDesc = {};
		indirectAlphaDepthPassShaderDesc.mStages[0] = {
			"meshDepthPassAlpha.vert", NULL, 0, RD_SHADER_SOURCES };
		indirectAlphaDepthPassShaderDesc.mStages[1] = {
			"meshDepthPassAlpha.frag", NULL, 0, RD_SHADER_SOURCES };

		ShaderLoadDesc ASMCopyDepthQuadsShaderDesc = {};
		ASMCopyDepthQuadsShaderDesc.mStages[0] = {"copyDepthQuads.vert", NULL, 0, RD_SHADER_SOURCES};
		ASMCopyDepthQuadsShaderDesc.mStages[1] = { "copyDepthQuads.frag", NULL, 0, RD_SHADER_SOURCES };

		ShaderLoadDesc quadShaderDesc = {};
		quadShaderDesc.mStages[0] = { "quad.vert", NULL, 0, RD_SHADER_SOURCES };
		quadShaderDesc.mStages[1] = { "quad.frag", NULL, 0, RD_SHADER_SOURCES };


		ShaderLoadDesc ASMFillIndirectionShaderDesc = {};
		ASMFillIndirectionShaderDesc.mStages[0] = { "fill_Indirection.vert", 
			NULL, 0, RD_SHADER_SOURCES };
		ASMFillIndirectionShaderDesc.mStages[1] = { "fill_Indirection.frag",
			NULL, 0, RD_SHADER_SOURCES };

		ShaderLoadDesc ASMCopyDEMQuadsShaderDesc = {};
		ASMCopyDEMQuadsShaderDesc.mStages[0] = { "copyDEMQuads.vert", NULL, 0, RD_SHADER_SOURCES };
		ASMCopyDEMQuadsShaderDesc.mStages[1] = { "copyDEMQuads.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &ASMCopyDEMQuadsShaderDesc, &pShaderASMCopyDEM);
		

		ShaderLoadDesc ASMGenerateDEMShaderDesc = {};
		ASMGenerateDEMShaderDesc.mStages[0] = { "generateAsmDEM.vert", NULL, 0, RD_SHADER_SOURCES };
		ASMGenerateDEMShaderDesc.mStages[1] = { "generateAsmDEM.frag", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &ASMGenerateDEMShaderDesc, &pShaderASMGenerateDEM);


		


		ShaderLoadDesc visibilityBufferPassShaderDesc = {};
		visibilityBufferPassShaderDesc.mStages[0] = { "visibilityBufferPass.vert", NULL, 0, RD_SHADER_SOURCES };
		visibilityBufferPassShaderDesc.mStages[1] = { "visibilityBufferPass.frag", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &visibilityBufferPassShaderDesc, &pShaderVBBufferPass[GEOMSET_OPAQUE]);

		ShaderLoadDesc visibilityBufferPassAlphaShaderDesc = {};
		visibilityBufferPassAlphaShaderDesc.mStages[0] = { "visibilityBufferPassAlpha.vert", NULL, 0, RD_SHADER_SOURCES };
		visibilityBufferPassAlphaShaderDesc.mStages[1] = { "visibilityBufferPassAlpha.frag", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &visibilityBufferPassAlphaShaderDesc, &pShaderVBBufferPass[GEOMSET_ALPHATESTED]);
	
		ShaderLoadDesc clearBuffersShaderDesc = {};
		clearBuffersShaderDesc.mStages[0] = { "clearVisibilityBuffers.comp", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &clearBuffersShaderDesc, &pShaderClearBuffers);


		ShaderLoadDesc triangleFilteringShaderDesc = {};
		triangleFilteringShaderDesc.mStages[0] = { "triangleFiltering.comp", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &triangleFilteringShaderDesc, &pShaderTriangleFiltering);

		ShaderLoadDesc batchCompactionShaderDesc = {};
		batchCompactionShaderDesc.mStages[0] = { "batchCompaction.comp", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &batchCompactionShaderDesc, &pShaderBatchCompaction);

		ShaderLoadDesc updateSDFVolumeTextureAtlasShaderDesc = {};
		updateSDFVolumeTextureAtlasShaderDesc.mStages[0] = { "updateRegion3DTexture.comp", NULL, 0, RD_SHADER_SOURCES };
		

		addShader(pRenderer, &updateSDFVolumeTextureAtlasShaderDesc, &pShaderUpdateSDFVolumeTextureAtlas);

		ShaderLoadDesc meshSDFVisualizationShaderDesc = {};
		meshSDFVisualizationShaderDesc.mStages[0] = { "visualizeSDFMesh.comp", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &meshSDFVisualizationShaderDesc, &pShaderSDFMeshVisualization);

		ShaderLoadDesc sdfShadowMeshShaderDesc = {};
		sdfShadowMeshShaderDesc.mStages[0] = { "bakedSDFMeshShadow.comp", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &sdfShadowMeshShaderDesc, &pShaderSDFMeshShadow);

		ShaderLoadDesc upSampleSDFShadowShaderDesc = {};
		upSampleSDFShadowShaderDesc.mStages[0] = { "upsampleSDFShadow.vert", NULL, 0, RD_SHADER_SOURCES };
		upSampleSDFShadowShaderDesc.mStages[1] = { "upsampleSDFShadow.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &upSampleSDFShadowShaderDesc, &pShaderUpsampleSDFShadow);


		ShaderLoadDesc presentShaderDesc = {};
		presentShaderDesc.mStages[0] = { "display.vert", NULL, 0, RD_SHADER_SOURCES };
		presentShaderDesc.mStages[1] = { "display.frag", NULL, 0, RD_SHADER_SOURCES };
		addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);


		ShaderLoadDesc visibilityBufferShadeShaderDesc = {};
		visibilityBufferShadeShaderDesc.mStages[0] = { "visibilityBufferShade.vert", NULL, 0, RD_SHADER_SOURCES };
		visibilityBufferShadeShaderDesc.mStages[1] = { "visibilityBufferShade.frag", NULL, 0, RD_SHADER_SOURCES };


		/************************************************************************/
		// Add shaders
		/************************************************************************/
		addShader(pRenderer, &indirectAlphaDepthPassShaderDesc, &pShaderIndirectAlphaDepthPass);
		addShader(pRenderer, &indirectDepthPassShaderDesc, &pShaderIndirectDepthPass);

		addShader(pRenderer, &ASMCopyDepthQuadsShaderDesc, &pShaderASMCopyDepthQuadPass);
		addShader(pRenderer, &ASMFillIndirectionShaderDesc, &pShaderASMFillIndirection);
		addShader(pRenderer, &visibilityBufferShadeShaderDesc, &pShaderVBShade);

		addShader(pRenderer, &quadShaderDesc, &pShaderQuad);

		/************************************************************************/
		// Add GPU profiler
		/************************************************************************/
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfilerGraphics, "GpuProfiler");
		addGpuProfiler(pRenderer, pComputeQueue, &pGpuProfilerCompute, "ComputeGpuProfiler");

		/************************************************************************/
		// Add samplers
		/************************************************************************/
		SamplerDesc clampMiplessSamplerDesc = {};
		clampMiplessSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mMinFilter = FILTER_LINEAR;
		clampMiplessSamplerDesc.mMagFilter = FILTER_LINEAR;
		clampMiplessSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		clampMiplessSamplerDesc.mMipLodBias = 0.0f;
		clampMiplessSamplerDesc.mMaxAnisotropy = 0.0f;
		addSampler(pRenderer, &clampMiplessSamplerDesc, &pSamplerMiplessSampler);


		SamplerDesc samplerTrilinearAnisoDesc = {};
		samplerTrilinearAnisoDesc.mAddressU = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressV = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressW = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mMinFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMagFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		samplerTrilinearAnisoDesc.mMipLodBias = 0.0f;
		samplerTrilinearAnisoDesc.mMaxAnisotropy = 8.0f;
		addSampler(pRenderer, &samplerTrilinearAnisoDesc, &pSamplerTrilinearAniso);

		SamplerDesc miplessNearSamplerDesc = {};
		miplessNearSamplerDesc.mMinFilter = FILTER_NEAREST;
		miplessNearSamplerDesc.mMagFilter = FILTER_NEAREST;
		miplessNearSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		miplessNearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mMipLodBias = 0.f;
		miplessNearSamplerDesc.mMaxAnisotropy = 0.f;
		addSampler(pRenderer, &miplessNearSamplerDesc, &pSamplerMiplessNear);


		SamplerDesc miplessLinearSamplerDesc = {};
		miplessLinearSamplerDesc.mMinFilter = FILTER_LINEAR;
		miplessLinearSamplerDesc.mMagFilter = FILTER_LINEAR;
		miplessLinearSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		miplessLinearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mMipLodBias = 0.f;
		miplessLinearSamplerDesc.mMaxAnisotropy = 0.f;
		addSampler(pRenderer, &miplessLinearSamplerDesc, &pSamplerMiplessLinear);
		miplessLinearSamplerDesc.mCompareFunc = CompareMode::CMP_LEQUAL;
		miplessLinearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
		miplessLinearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
		miplessLinearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
		addSampler(pRenderer, &miplessLinearSamplerDesc, &pSamplerComparisonShadow);


		SamplerDesc miplessClampToBorderNearSamplerDesc = {};
		miplessClampToBorderNearSamplerDesc.mMinFilter = FILTER_NEAREST;
		miplessClampToBorderNearSamplerDesc.mMagFilter = FILTER_NEAREST;
		miplessClampToBorderNearSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		miplessClampToBorderNearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
		miplessClampToBorderNearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
		miplessClampToBorderNearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessClampToBorderNearSamplerDesc.mMaxAnisotropy = 0.f;
		miplessClampToBorderNearSamplerDesc.mMipLodBias = 0.f;
		addSampler(pRenderer, &miplessClampToBorderNearSamplerDesc, &pSamplerMiplessClampToBorderNear);


		SamplerDesc billinearRepeatDesc = {};
		billinearRepeatDesc.mMinFilter = FILTER_LINEAR;
		billinearRepeatDesc.mMagFilter = FILTER_LINEAR;
		billinearRepeatDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		billinearRepeatDesc.mAddressU = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mAddressV = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mAddressW = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mMaxAnisotropy = 0.f;
		billinearRepeatDesc.mMipLodBias = 0.f;
		addSampler(pRenderer, &billinearRepeatDesc, &pSamplerLinearRepeat);

		/************************************************************************/
		// Load resources for skybox
		/************************************************************************/
		addThreadSystemTask(pThreadSystem, memberTaskFunc0<LightShadowPlayground, &LightShadowPlayground::LoadSkybox>, this);

		initSDFMeshes();

        PathHandle sceneFullPath = fsCopyPathInResourceDirectory(RD_MESHES, gSceneName);
		//pScene = loadScene(sceneFullPath.c_str(), MESH_SCALE, SAN_MIGUEL_OFFSETX, 0.0f, 0.0f);
		Scene* pScene = loadScene(sceneFullPath, SAN_MIGUEL_ORIGINAL_SCALE, SAN_MIGUEL_ORIGINAL_OFFSETX, 0.0f, 0.0f);
		
		gMeshCount = pScene->numMeshes;
		gMaterialCount = pScene->numMaterials;
		pMeshes = (MeshIn*)conf_malloc(pScene->numMeshes * sizeof(MeshIn));
		memcpy(pMeshes, pScene->meshes, pScene->numMeshes * sizeof(MeshIn));
		
		BufferLoadDesc indirectVBPosDesc = {};
		indirectVBPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectVBPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectVBPosDesc.mDesc.mVertexStride = sizeof(SceneVertexPos);
		indirectVBPosDesc.mDesc.mElementCount = pScene->totalVertices;
		indirectVBPosDesc.mDesc.mStructStride = sizeof(SceneVertexPos);
		indirectVBPosDesc.mDesc.mSize = indirectVBPosDesc.mDesc.mElementCount * indirectVBPosDesc.mDesc.mStructStride;
		indirectVBPosDesc.pData = pScene->positions;
		indirectVBPosDesc.ppBuffer = &pIndirectPosBuffer;
		indirectVBPosDesc.mDesc.pDebugName = L"Indirect Vertex Position Buffer Desc";
		addResource(&indirectVBPosDesc, true);

		// Vertex texcoord buffer for the scene
		BufferLoadDesc indirectVBTexCoordDesc = {};
		indirectVBTexCoordDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectVBTexCoordDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectVBTexCoordDesc.mDesc.mVertexStride = sizeof(SceneVertexTexCoord);
		indirectVBTexCoordDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexTexCoord) / sizeof(uint32_t));
		indirectVBTexCoordDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectVBTexCoordDesc.mDesc.mSize = indirectVBTexCoordDesc.mDesc.mElementCount * indirectVBTexCoordDesc.mDesc.mStructStride;
		indirectVBTexCoordDesc.pData = pScene->texCoords;
		indirectVBTexCoordDesc.ppBuffer = &pIndirectTexCoordBuffer;
		indirectVBTexCoordDesc.mDesc.pDebugName = L"Indirect Vertex TexCoord Buffer Desc";
		addResource(&indirectVBTexCoordDesc, true);

		// Vertex normal buffer for the scene
		BufferLoadDesc indirectVBNormalDesc = {};
		indirectVBNormalDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectVBNormalDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectVBNormalDesc.mDesc.mVertexStride = sizeof(SceneVertexNormal);
		indirectVBNormalDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexNormal) / sizeof(uint32_t));
		indirectVBNormalDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectVBNormalDesc.mDesc.mSize = indirectVBNormalDesc.mDesc.mElementCount *
			indirectVBNormalDesc.mDesc.mStructStride;
		indirectVBNormalDesc.pData = pScene->normals;
		indirectVBNormalDesc.ppBuffer = &pIndirectNormalBuffer;
		indirectVBNormalDesc.mDesc.pDebugName = L"Indirect Vertex Normal Buffer Desc";
		addResource(&indirectVBNormalDesc, true);

		// Vertex tangent buffer for the scene
		BufferLoadDesc indirectVBTangentDesc = {};
		indirectVBTangentDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectVBTangentDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectVBTangentDesc.mDesc.mVertexStride = sizeof(SceneVertexTangent);
		indirectVBTangentDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexTangent) / sizeof(uint32_t));
		indirectVBTangentDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectVBTangentDesc.mDesc.mSize = indirectVBTangentDesc.mDesc.mElementCount * indirectVBTangentDesc.mDesc.mStructStride;
		indirectVBTangentDesc.pData = pScene->tangents;
		indirectVBTangentDesc.ppBuffer = &pIndirectTangentBuffer;
		indirectVBTangentDesc.mDesc.pDebugName = L"Indirect Vertex Tangent Buffer Desc";
		addResource(&indirectVBTangentDesc, true);


		BufferLoadDesc indirectIBDesc = {};
		indirectIBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectIBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectIBDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		indirectIBDesc.mDesc.mElementCount = pScene->totalTriangles * 3;
		indirectIBDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectIBDesc.mDesc.mSize = indirectIBDesc.mDesc.mElementCount * indirectIBDesc.mDesc.mStructStride;
		indirectIBDesc.pData = pScene->indices;
		indirectIBDesc.ppBuffer = &pIndirectIndexBuffer;
		indirectIBDesc.mDesc.pDebugName = L"Indirect Non-filtered Index Buffer Desc";
		addResource(&indirectIBDesc, true);
		
		
		gDiffuseMaps.resize(gMaterialCount);
		gNormalMaps.resize(gMaterialCount);
		gSpecularMaps.resize(gMaterialCount);


		TextureLoadDesc loadTextureDesc = {};

		TextureLoadTaskData loadTextureDiffuseData = {};
		loadTextureDiffuseData.textures = gDiffuseMaps.data();
		loadTextureDiffuseData.mNames = (const char **)(pScene->textures);
		loadTextureDiffuseData.mDesc = loadTextureDesc;

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTextureDiffuseData, gMaterialCount);

		TextureLoadTaskData loadTextureNormalMap = {};
		loadTextureNormalMap.textures = gNormalMaps.data();
		loadTextureNormalMap.mNames = (const char **)(pScene->normalMaps);
		loadTextureNormalMap.mDesc = loadTextureDesc;

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTextureNormalMap, gMaterialCount);

		TextureLoadTaskData loadTexturesSpecularMap = {};
		loadTexturesSpecularMap.mDesc = loadTextureDesc;
		loadTexturesSpecularMap.mNames = (const char **)(pScene->specularMaps);
		loadTexturesSpecularMap.textures = gSpecularMaps.data();

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTexturesSpecularMap, gMaterialCount);

		// Cluster creation
		/************************************************************************/
		// Calculate clusters


		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			//MeshInstance*   subMesh = &pMeshes[i];
			MeshIn*   subMesh = &pMeshes[i];
			Material* material = pScene->materials + subMesh->materialId;
			createClusters(material->twoSided, pScene, subMesh);
		}


		MeshConstants* meshConstants =
			(MeshConstants*)conf_malloc(gMeshCount * sizeof(MeshConstants));


		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			meshConstants[i].faceCount = pMeshes[i].indexCount / 3;
			meshConstants[i].indexOffset = pMeshes[i].startIndex;
			meshConstants[i].materialID = pMeshes[i].materialId;
			meshConstants[i].twoSided =
				pScene->materials[pMeshes[i].materialId].twoSided ? 1 : 0;
		}

		BufferLoadDesc meshConstantDesc = {};
		meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		meshConstantDesc.mDesc.mElementCount = gMeshCount;
		meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
		meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
		meshConstantDesc.ppBuffer = &pBufferMeshConstants;
		meshConstantDesc.pData = meshConstants;
		meshConstantDesc.mDesc.pDebugName = L"Mesh Constant desc";
		addResource(&meshConstantDesc);

		conf_free(meshConstants);


		


		/************************************************************************/
		// setup root signitures
		/************************************************************************/

		RootSignatureDesc ASMCopyDepthQuadsRootDesc = {};
		ASMCopyDepthQuadsRootDesc.mShaderCount = 1;
		ASMCopyDepthQuadsRootDesc.ppShaders = &pShaderASMCopyDepthQuadPass;
		ASMCopyDepthQuadsRootDesc.mStaticSamplerCount = 1;
		ASMCopyDepthQuadsRootDesc.ppStaticSamplers = &pSamplerMiplessNear;
		const char* copyDepthQuadsSamplerNames[] = { "clampToEdgeNearSampler" };
		ASMCopyDepthQuadsRootDesc.ppStaticSamplerNames = copyDepthQuadsSamplerNames;


		RootSignatureDesc ASMCopyDEMQuadsRootDesc = {};
		ASMCopyDEMQuadsRootDesc.mShaderCount = 1;
		ASMCopyDEMQuadsRootDesc.ppShaders = &pShaderASMCopyDEM;
		ASMCopyDEMQuadsRootDesc.mStaticSamplerCount = 1;
		ASMCopyDEMQuadsRootDesc.ppStaticSamplers = &pSamplerMiplessNear;
		const char* copyDEMQuadsSamplerNames[] = { "clampToEdgeNearSampler" };
		ASMCopyDEMQuadsRootDesc.ppStaticSamplerNames = copyDEMQuadsSamplerNames;


		RootSignatureDesc quadRootDesc = {};
		quadRootDesc.ppShaders = &pShaderQuad;
		quadRootDesc.mShaderCount = 1;
		quadRootDesc.ppStaticSamplers = &pSamplerMiplessNear;
		quadRootDesc.mStaticSamplerCount = 1;
		const char*  quadRootSamplerNames[] = { "clampNearSampler" };
		quadRootDesc.ppStaticSamplerNames = quadRootSamplerNames;


		RootSignatureDesc ASMFillIndirectionRootDesc = {};
		ASMFillIndirectionRootDesc.mShaderCount = 1;
		ASMFillIndirectionRootDesc.ppShaders = &pShaderASMFillIndirection;
		ASMFillIndirectionRootDesc.mStaticSamplerCount = 0;
		ASMFillIndirectionRootDesc.ppStaticSamplers = NULL;
		ASMFillIndirectionRootDesc.ppStaticSamplerNames = NULL;

		RootSignatureDesc ASMGenerateDEMRootDesc = {};
		ASMGenerateDEMRootDesc.mShaderCount = 1;
		ASMGenerateDEMRootDesc.ppShaders = &pShaderASMGenerateDEM;
		ASMGenerateDEMRootDesc.mStaticSamplerCount = 1;
		ASMGenerateDEMRootDesc.ppStaticSamplers = &pSamplerMiplessLinear;
		ASMGenerateDEMRootDesc.ppStaticSamplerNames = copyDEMQuadsSamplerNames;

		RootSignatureDesc ASMFillLodClampRootDesc = {};
		ASMFillLodClampRootDesc.mShaderCount = 1;
		ASMFillLodClampRootDesc.ppShaders = &pShaderASMFillIndirection;
		ASMFillLodClampRootDesc.mStaticSamplerCount = 0;
		ASMFillLodClampRootDesc.ppStaticSamplers = NULL;
		ASMFillLodClampRootDesc.ppStaticSamplerNames = NULL;

		Sampler* asmSceneSamplers[] = { 
			pSamplerTrilinearAniso,
			pSamplerMiplessNear,
			pSamplerMiplessLinear,
			pSamplerMiplessClampToBorderNear,
			pSamplerComparisonShadow};


		const char* asmSceneSamplersNames[] = { 
			"textureSampler", 
			"clampMiplessNearSampler", 
			"clampMiplessLinearSampler", 
			"clampBorderNearSampler",
			"ShadowCmpSampler" };

		Sampler* vbShadeSceneSamplers[] = {
			pSamplerTrilinearAniso,
			pSamplerMiplessNear,
			pSamplerMiplessLinear,
			pSamplerMiplessClampToBorderNear,
			pSamplerComparisonShadow};


		const char* vbShadeSceneSamplersNames[] = {
			"textureSampler",
			"clampMiplessNearSampler",
			"clampMiplessLinearSampler",
			"clampBorderNearSampler",
			"ShadowCmpSampler"};

		RootSignatureDesc vbShadeRootDesc = {};
		vbShadeRootDesc.mShaderCount = 1;
		vbShadeRootDesc.ppShaders = &pShaderVBShade;
		vbShadeRootDesc.mStaticSamplerCount = 5;
		vbShadeRootDesc.ppStaticSamplers = vbShadeSceneSamplers;
		vbShadeRootDesc.ppStaticSamplerNames = vbShadeSceneSamplersNames;
		vbShadeRootDesc.mMaxBindlessTextures = gMaterialCount;
		//ASMVBShadeRootDesc.mSignatureType = ROOT_SIGN


		Shader* pVisibilityBufferPassListShaders[gNumGeomSets * 2] = {};
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			pVisibilityBufferPassListShaders[i] = pShaderVBBufferPass[i];
			
		}
		pVisibilityBufferPassListShaders[2] = pShaderIndirectDepthPass;
		pVisibilityBufferPassListShaders[3] = pShaderIndirectAlphaDepthPass;


		const char* vbPassSamplerNames[] = { "nearClampSampler" };
		RootSignatureDesc vbPassRootDesc = { pVisibilityBufferPassListShaders, gNumGeomSets * 2 };
		vbPassRootDesc.mMaxBindlessTextures = gMaterialCount;
		//vbPassRootDesc.ppStaticSamplerNames = indirectSamplerNames;
		vbPassRootDesc.ppStaticSamplerNames = vbPassSamplerNames;
		vbPassRootDesc.mStaticSamplerCount = 1;
		//vbPassRootDesc.ppStaticSamplers = &pSamplerTrilinearAniso;
		vbPassRootDesc.ppStaticSamplers = &pSamplerMiplessNear;


		Shader* pShadowPassBufferSets[gNumGeomSets] = { pShaderIndirectDepthPass, 
			pShaderIndirectAlphaDepthPass };
		

		const char* indirectSamplerNames[] = { "trillinearSampler" };

		RootSignatureDesc clearBuffersRootDesc = {&pShaderClearBuffers, 1};
		

		Shader* pCullingShaders[] = { pShaderClearBuffers, pShaderTriangleFiltering, pShaderBatchCompaction };
		RootSignatureDesc triangleFilteringRootDesc = { pCullingShaders, 3 };

		RootSignatureDesc updateSDFVolumeTextureAtlasRootDesc = {&pShaderUpdateSDFVolumeTextureAtlas, 1};
		
		const char* visualizeSDFMeshSamplerNames[] = { "clampToEdgeTrillinearSampler", "clampToEdgeNearSampler" };
		Sampler* visualizeSDFMeshSamplers[] = { pSamplerTrilinearAniso, pSamplerMiplessNear };
		RootSignatureDesc visualizeSDFMeshRootDesc = {&pShaderSDFMeshVisualization, 1 };
		visualizeSDFMeshRootDesc.ppStaticSamplerNames = visualizeSDFMeshSamplerNames;
		visualizeSDFMeshRootDesc.mStaticSamplerCount = 2;
		visualizeSDFMeshRootDesc.ppStaticSamplers = visualizeSDFMeshSamplers;


		RootSignatureDesc sdfMeshShadowRootDesc = {&pShaderSDFMeshShadow, 1};
		sdfMeshShadowRootDesc.ppStaticSamplerNames = visualizeSDFMeshSamplerNames;
		sdfMeshShadowRootDesc.mStaticSamplerCount = 2;
		sdfMeshShadowRootDesc.ppStaticSamplers = visualizeSDFMeshSamplers;

		RootSignatureDesc upSampleSDFShadowRootDesc = { &pShaderUpsampleSDFShadow, 1 };
		const char* upSamplerSDFShadowSamplerNames[] = 
		{ "clampMiplessNearSampler", "clampMiplessLinearSampler" };
		upSampleSDFShadowRootDesc.ppStaticSamplerNames = upSamplerSDFShadowSamplerNames;
		upSampleSDFShadowRootDesc.mStaticSamplerCount = 2;
		Sampler* upSampleSDFShadowSamplers[] = {  pSamplerMiplessNear, pSamplerMiplessLinear };
		upSampleSDFShadowRootDesc.ppStaticSamplers = upSampleSDFShadowSamplers;


		RootSignatureDesc finalShaderRootSigDesc = { &pShaderPresentPass, 1 };
		addRootSignature(pRenderer, &finalShaderRootSigDesc, &pRootSignaturePresentPass);
		addRootSignature(pRenderer, &ASMCopyDepthQuadsRootDesc, &pRootSignatureASMCopyDepthQuadPass);
		addRootSignature(pRenderer, &ASMFillIndirectionRootDesc, &pRootSignatureASMFillIndirection);

		addRootSignature(pRenderer, &ASMCopyDEMQuadsRootDesc, &pRootSignatureASMCopyDEM);
		addRootSignature(pRenderer, &ASMGenerateDEMRootDesc, &pRootSignatureASMDEMAtlasToColor);
		addRootSignature(pRenderer, &ASMGenerateDEMRootDesc, &pRootSignatureASMDEMColorToAtlas);
		addRootSignature(pRenderer, &ASMFillLodClampRootDesc, &pRootSignatureASMFillLodClamp);
	


		addRootSignature(pRenderer, &vbPassRootDesc, &pRootSignatureVBPass);

		//addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);
		addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);
		//addRootSignature(pRenderer, &batchCompactionRootDesc, &pRootSignatureBatchCompaction);
		addRootSignature(pRenderer, &vbShadeRootDesc, &pRootSignatureVBShade);


		addRootSignature(pRenderer, &updateSDFVolumeTextureAtlasRootDesc, &pRootSignatureUpdateSDFVolumeTextureAtlas);
		addRootSignature(pRenderer, &visualizeSDFMeshRootDesc, &pRootSignatureSDFMeshVisualization);
		addRootSignature(pRenderer, &sdfMeshShadowRootDesc, &pRootSignatureSDFMeshShadow);

	
		addRootSignature(pRenderer, 
			&upSampleSDFShadowRootDesc, &pRootSignatureUpsampleSDFShadow);

		addRootSignature(pRenderer, &quadRootDesc, &pRootSignatureQuad);

#if defined(DIRECT3D12)
		IndirectArgumentDescriptor indirectArgs[2] = {};
		indirectArgs[0].mType = INDIRECT_CONSTANT;
		indirectArgs[0].mCount = 1;
		indirectArgs[0].pName = "indirectRootConstant";
		indirectArgs[1].mType = INDIRECT_DRAW_INDEX;
		CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 2, indirectArgs };
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
#else
		// Indicate the renderer that we want to use non-indexed geometry.
		IndirectArgumentDescriptor indirectArgs[1] = {};

		indirectArgs[0].mType = INDIRECT_DRAW_INDEX;

		CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 1, indirectArgs };	
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
		
#endif
		/************************************************************************/
		// setup Rasterizer State
		/************************************************************************/
		RasterizerStateDesc rasterStateDesc = {};
		rasterStateDesc.mCullMode = CULL_MODE_FRONT;
		rasterStateDesc.mSlopeScaledDepthBias = -3.0;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullFront);

		RasterizerStateDesc nonDepthBiasRasterFrontStateDesc = {};
		nonDepthBiasRasterFrontStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &nonDepthBiasRasterFrontStateDesc, &pRasterizerStateNonBiasCullFront);

		RasterizerStateDesc rasterBackStateDesc = {};
		rasterBackStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterBackStateDesc, &pRasterizerStateCullBack);

		rasterStateDesc.mCullMode = CULL_MODE_NONE;
		rasterStateDesc.mSlopeScaledDepthBias = 0.f;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullNone);

		BlendStateDesc blendStateSkyBoxDesc = {};
		blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
		blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
		blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;
		blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateSkyBoxDesc.mMasks[0] = ALL;
		blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		//blendStateSkyBoxDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateSkyBoxDesc, &pBlendStateSkyBox);
		/************************************************************************/
		waitThreadSystemIdle(pThreadSystem);
		finishResourceLoading();


		gDiffuseMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gDiffuseMaps.size());
		gNormalMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gNormalMaps.size());
		gSpecularMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gSpecularMaps.size());

		for (uint32_t i = 0; i < (uint32_t)gDiffuseMaps.size(); ++i)
		{
			memcpy(&gDiffuseMapsStorage[i], gDiffuseMaps[i], sizeof(Texture));
			gDiffuseMapsPacked.push_back(&gDiffuseMapsStorage[i]);
		}
		for (uint32_t i = 0; i < (uint32_t)gNormalMaps.size(); ++i)
		{
			memcpy(&gNormalMapsStorage[i], gNormalMaps[i], sizeof(Texture));
			gNormalMapsPacked.push_back(&gNormalMapsStorage[i]);
		}
		for (uint32_t i = 0; i < (uint32_t)gSpecularMaps.size(); ++i)
		{
			memcpy(&gSpecularMapsStorage[i], gSpecularMaps[i], sizeof(Texture));
			gSpecularMapsPacked.push_back(&gSpecularMapsStorage[i]);
		}
		/************************************************************************/
		// Indirect data for the scene
		/************************************************************************/
		uint32_t* materialAlphaData = (uint32_t*)conf_malloc(
			gMaterialCount * sizeof(uint32_t));

		for (uint32_t i = 0; i < gMaterialCount; ++i)
		{
			materialAlphaData[i] = pScene->materials[i].alphaTested ? 1 : 0;
		}

		BufferLoadDesc materialPropDesc = {};
		materialPropDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		materialPropDesc.mDesc.mElementCount = gMaterialCount;
		materialPropDesc.mDesc.mStructStride = sizeof(uint32_t);
		materialPropDesc.mDesc.mSize = materialPropDesc.mDesc.mElementCount * 
			materialPropDesc.mDesc.mStructStride;
		materialPropDesc.pData = materialAlphaData;
		materialPropDesc.ppBuffer = &pBufferMaterialProperty;
		materialPropDesc.mDesc.pDebugName = L"Material Prop Desc";
		addResource(&materialPropDesc);

		conf_free(materialAlphaData);


		const uint32_t numBatches = (const uint32_t)gMeshCount;
		eastl::vector<uint32_t> materialIDPerDrawCall(MATERIAL_BUFFER_SIZE);
		eastl::vector<BufferIndirectCommand> indirectArgsNoAlpha(MAX_DRAWS_INDIRECT, BufferIndirectCommand{ 0 });
		eastl::vector<BufferIndirectCommand> indirectArgsAlpha(MAX_DRAWS_INDIRECT, BufferIndirectCommand{ 0 });
		uint32_t iAlpha = 0, iNoAlpha = 0;

		for (uint32_t i = 0; i < numBatches; ++i)
		{
			uint matID = pMeshes[i].materialId;
			Material* mat = &pScene->materials[matID];
			uint32 numIDX = pMeshes[i].indexCount;
			uint32 startIDX = pMeshes[i].startIndex;

			if (mat->alphaTested)
			{
#if defined(DIRECT3D12)
				indirectArgsAlpha[iAlpha].drawId = iAlpha;
#endif
				indirectArgsAlpha[iAlpha].arg.mInstanceCount = 1;
				indirectArgsAlpha[iAlpha].arg.mIndexCount = numIDX;
				indirectArgsAlpha[iAlpha].arg.mStartIndex = startIDX;

				for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; ++j)
				{
					materialIDPerDrawCall[BaseMaterialBuffer(true, j) + iAlpha] = matID;
				}
				++iAlpha;
			}
			else
			{
#if defined(DIRECT3D12)
				indirectArgsNoAlpha[iNoAlpha].drawId = iNoAlpha;
#endif
				indirectArgsNoAlpha[iNoAlpha].arg.mInstanceCount = 1;
				indirectArgsNoAlpha[iNoAlpha].arg.mIndexCount = numIDX;
				indirectArgsNoAlpha[iNoAlpha].arg.mStartIndex = startIDX;

				for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; ++j)
				{
					materialIDPerDrawCall[BaseMaterialBuffer(false, j) + iNoAlpha] = matID;
				}
				++iNoAlpha;

			}
			*(((UINT*)indirectArgsAlpha.data()) + DRAW_COUNTER_SLOT_POS) = iAlpha;
			*(((UINT*)indirectArgsNoAlpha.data()) + DRAW_COUNTER_SLOT_POS) = iNoAlpha;

			for(int32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			{
				gPerFrameData[frameIdx].gDrawCount[GEOMSET_OPAQUE] = iNoAlpha;
				gPerFrameData[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = iAlpha;
			}
		}

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			BufferLoadDesc indirectBufferDesc = {};
			indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(BufferIndirectCommand) / sizeof(uint32_t));
			indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
			indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount
				*  indirectBufferDesc.mDesc.mStructStride;
			indirectBufferDesc.pData = i == 0 ? indirectArgsNoAlpha.data() : indirectArgsAlpha.data();
			indirectBufferDesc.ppBuffer = &pBufferIndirectDrawArgumentsAll[i];
			indirectBufferDesc.mDesc.pDebugName = L"Indirect Draw args buffer desc";
			addResource(&indirectBufferDesc);
		}

		BufferLoadDesc indirectDesc = {};
		indirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
		indirectDesc.pData = materialIDPerDrawCall.data();
		indirectDesc.ppBuffer = &pBufferIndirectMaterialAll;
		indirectDesc.mDesc.pDebugName = L"Indirect Desc";
		addResource(&indirectDesc);


		/************************************************************************/
		// Indirect buffers for culling
		/************************************************************************/
		BufferLoadDesc filterIbDesc = {};
		filterIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIbDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		filterIbDesc.mDesc.mElementCount = pScene->totalTriangles * 3;
		filterIbDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIbDesc.mDesc.mSize = filterIbDesc.mDesc.mElementCount * filterIbDesc.mDesc.mStructStride;
		filterIbDesc.mDesc.pDebugName = L"Filtered IB Desc";
		filterIbDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; ++j)
			{
				filterIbDesc.ppBuffer = &pBufferFilteredIndex[i][j];
				addResource(&filterIbDesc);
			}
		}

		BufferIndirectCommand* indirectDrawArguments = (BufferIndirectCommand*)
			conf_malloc(MAX_DRAWS_INDIRECT * sizeof(BufferIndirectCommand));

		memset(indirectDrawArguments, 0, MAX_DRAWS_INDIRECT * sizeof(BufferIndirectCommand));

		for (uint32_t i = 0; i < MAX_DRAWS_INDIRECT; ++i)
		{
#if defined(DIRECT3D12)
			indirectDrawArguments[i].drawId = i;
#endif
			if (i < gMeshCount)
			{
				indirectDrawArguments[i].arg.mInstanceCount = 1;
			}
		}

		BufferLoadDesc filterIndirectDesc = {};
		filterIndirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIndirectDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(BufferIndirectCommand) / sizeof(uint32_t));
		filterIndirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
		filterIndirectDesc.mDesc.pDebugName = L"Filtered Indirect Desc";
		filterIndirectDesc.pData = indirectDrawArguments;

		BufferLoadDesc uncompactedDesc = {};
		uncompactedDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		uncompactedDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		uncompactedDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT;
		uncompactedDesc.mDesc.mStructStride = sizeof(UncompactedDrawArguments);
		uncompactedDesc.mDesc.mSize = uncompactedDesc.mDesc.mElementCount * uncompactedDesc.mDesc.mStructStride;
		uncompactedDesc.mDesc.pDebugName = L"Uncompacted Draw Arguments Desc";
		uncompactedDesc.pData = NULL;

		BufferLoadDesc filterMaterialDesc = {};
		filterMaterialDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterMaterialDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterMaterialDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		filterMaterialDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterMaterialDesc.mDesc.mSize = filterMaterialDesc.mDesc.mElementCount 
			* filterMaterialDesc.mDesc.mStructStride;
		filterMaterialDesc.mDesc.pDebugName = L"Filtered Indirect Material Desc";
		filterMaterialDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			filterMaterialDesc.ppBuffer = &pBufferFilterIndirectMaterial[i];
			addResource(&filterMaterialDesc);

			for (uint32_t view = 0; view < NUM_CULLING_VIEWPORTS; ++view)
			{
				uncompactedDesc.ppBuffer = &pBufferUncompactedDrawArguments[i][view];
				addResource(&uncompactedDesc);

				for (uint32_t geom = 0; geom < gNumGeomSets; ++geom)
				{
					filterIndirectDesc.ppBuffer = &pBufferFilteredIndirectDrawArguments[i][geom][view];
					addResource(&filterIndirectDesc);
				}
			}
		}

		conf_free(indirectDrawArguments);

		/************************************************************************/
		// Triangle filtering buffers
		/************************************************************************/
		// Create buffers to store the list of filtered triangles. These buffers
		// contain the triangle IDs of the triangles that passed the culling tests.
		// One buffer per back buffer image is created for triple buffering.
		uint32_t bufferSizeTotal = 0;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				const uint32_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
				bufferSizeTotal += bufferSize;
				pFilterBatchChunk[i][j] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));

				pFilterBatchChunk[i][j]->batches = (FilterBatchData*)conf_calloc(1, bufferSize);
				pFilterBatchChunk[i][j]->currentBatchCount = 0;
				pFilterBatchChunk[i][j]->currentDrawCallCount = 0;
			}
		}
		addUniformGPURingBuffer(pRenderer, bufferSizeTotal, &pBufferFilterBatchData);
		/************************************************************************/
		////////////////////////////////////////////////

		/************************************************************************/
		// Initialize Resources
		/************************************************************************/
		gESMUniformData.mEsmControl = gEsmCpuSettings.mEsmControl;
		
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			BufferUpdateDesc esmBlurBufferCbv = { pBufferESMUniform[i], &gESMUniformData };
			updateResource(&esmBlurBufferCbv);
		}
		createScene();

		/************************************************************************/
		// Initialize ASM's render data
		/************************************************************************/
		pASM = conf_new(ASM);

		pSDFVolumeTextureAtlas = conf_new
			(SDFVolumeTextureAtlas,
			ivec3(
				SDF_VOLUME_TEXTURE_ATLAS_WIDTH,
				SDF_VOLUME_TEXTURE_ATLAS_HEIGHT, 
				SDF_VOLUME_TEXTURE_ATLAS_DEPTH)
			);

		initSDFVolumeTextureAtlasData();
		
		uint32_t volumeBufferElementCount = SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X *
			SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y * SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z;

		BufferLoadDesc sdfMeshVolumeDataUniformDesc = {};
		sdfMeshVolumeDataUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		sdfMeshVolumeDataUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sdfMeshVolumeDataUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		sdfMeshVolumeDataUniformDesc.mDesc.mStartState = RESOURCE_STATE_COPY_DEST;
		sdfMeshVolumeDataUniformDesc.mDesc.mStructStride = sizeof(float);
		sdfMeshVolumeDataUniformDesc.mDesc.mElementCount = volumeBufferElementCount;
		sdfMeshVolumeDataUniformDesc.mDesc.mSize = sdfMeshVolumeDataUniformDesc.mDesc.mStructStride *
			sdfMeshVolumeDataUniformDesc.mDesc.mElementCount;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			sdfMeshVolumeDataUniformDesc.ppBuffer = &pBufferSDFVolumeData[i];
			addResource(&sdfMeshVolumeDataUniformDesc);
		}



		/************************************************************************/
		// SDF volume atlas Texture
		/************************************************************************/
		TextureDesc sdfVolumeTextureAtlasDesc = {};
		sdfVolumeTextureAtlasDesc.mArraySize = 1;
		sdfVolumeTextureAtlasDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		sdfVolumeTextureAtlasDesc.mClearValue = ClearValue{0.f, 0.f, 0.f, 1.f};
		sdfVolumeTextureAtlasDesc.mDepth = SDF_VOLUME_TEXTURE_ATLAS_DEPTH;
		sdfVolumeTextureAtlasDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		sdfVolumeTextureAtlasDesc.mWidth = SDF_VOLUME_TEXTURE_ATLAS_WIDTH;
		sdfVolumeTextureAtlasDesc.mHeight = SDF_VOLUME_TEXTURE_ATLAS_HEIGHT;
		sdfVolumeTextureAtlasDesc.mMipLevels = 1;
		sdfVolumeTextureAtlasDesc.mSampleCount = SAMPLE_COUNT_1;
		sdfVolumeTextureAtlasDesc.mSampleQuality = 0;
		sdfVolumeTextureAtlasDesc.pDebugName = L"SDF Volume Texture Atlas";
		sdfVolumeTextureAtlasDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		//sdfVolumeTextureAtlasDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;

		TextureLoadDesc sdfVolumeTextureAtlasLoadDesc = {};
		sdfVolumeTextureAtlasLoadDesc.pDesc = &sdfVolumeTextureAtlasDesc;
		sdfVolumeTextureAtlasLoadDesc.ppTexture = &pTextureSDFVolumeAtlas;
		addResource(&sdfVolumeTextureAtlasLoadDesc, pTextureSDFVolumeAtlas);


		/*************************************************/
		//					UI
		/*************************************************/

		calculateCurSDFMeshesProgress();

		float   dpiScale = getDpiScale().x;

		GuiDesc guiDesc2 = {};
		guiDesc2.mStartPosition = vec2(700.0f / dpiScale, 450.0f / dpiScale);
		pLoadingGui = gAppUI.AddGuiComponent("Generating SDF", &guiDesc2);
		ProgressBarWidget ProgressBar("               [ProgressBar]               ", &gSDFProgressValue, LightShadowPlayground::getMaxSDFMeshesProgress());
		pLoadingGui->AddWidget(ProgressBar);

		GuiDesc guiDesc = {};
		
		guiDesc.mStartPosition = vec2(5, 200.0f) / dpiScale;
		guiDesc.mStartSize = vec2(450, 600) / dpiScale;
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		GuiController::addGui();

		CameraMotionParameters cmp{ 146.0f, 300.0f, 140.0f };
		vec3                   camPos{};
		vec3                   lookAt{};

		camPos = vec3(120.f + SAN_MIGUEL_OFFSETX, 98.f, 14.f);
		lookAt = camPos + vec3(-1.0f - 0.0f, 0.1f, 0.0f);

#ifdef _DURANGO
		if (gAppSettings.mAsyncCompute)
		{
			setResourcesToComputeCompliantState(0, true);
		}
#endif
		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

		// App Actions
    InputActionDesc actionDesc = { InputBindings::BUTTON_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
		addInputAction(&actionDesc);
		actionDesc =
		{
			InputBindings::BUTTON_ANY, [](InputActionContext* ctx)
			{
				bool capture = gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition);
				setEnableCaptureInput(capture && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		typedef bool (*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (!gAppSettings.mMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);
		
		removeScene(pScene);

		AddDescriptorSets();

		return true;
	}

	void LoadSkybox()
	{
		Texture*          pPanoSkybox = NULL;
		Shader*           pPanoToCubeShader = NULL;
		RootSignature*    pPanoToCubeRootSignature = NULL;
		Pipeline*         pPanoToCubePipeline = NULL;
		DescriptorSet*    pPanoToCubeDescriptorSet[2] = { NULL };

		Sampler* pSkyboxSampler = NULL;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pTextureSkybox;
		addResource(&skyboxLoadDesc, true);

		// Load the skybox panorama texture.
        PathHandle panoTexturePath = fsCopyPathInResourceDirectory(RD_TEXTURES, "daytime");
		TextureLoadDesc panoDesc = {};
		panoDesc.pFilePath = panoTexturePath;
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, true);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;

		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
		DescriptorSetDesc setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pPanoToCubeDescriptorSet[0]);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pPanoToCubeDescriptorSet[1]);

		PipelineDesc pipelineDesc = {};
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = pipelineDesc.mComputeDesc;
		pipelineSettings = { 0 };
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &pipelineDesc, &pPanoToCubePipeline);

		// Since this happens on iniatilization, use the first cmd/fence pair available.
		Cmd* cmd = ppCmds[0];

		// Compute the BRDF Integration map.
		beginCmd(cmd);

		TextureBarrier uavBarriers[1] = { { pTextureSkybox, RESOURCE_STATE_UNORDERED_ACCESS } };
		cmdResourceBarrier(cmd, 0, NULL, 1, uavBarriers);

		DescriptorData params[1] = {};

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(cmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		updateDescriptorSet(pRenderer, 0, pPanoToCubeDescriptorSet[0], 1, params);
		cmdBindDescriptorSet(cmd, 0, pPanoToCubeDescriptorSet[0]);

		struct Data
		{
			uint mip;
			uint textureSize;
		} data = { 0, gSkyboxSize };

		for (uint32_t i = 0; i < gSkyboxMips; ++i)
		{
			data.mip = i;
			cmdBindPushConstants(cmd, pPanoToCubeRootSignature, "RootConstant", &data);

			params[0].pName = "dstTexture";
			params[0].ppTextures = &pTextureSkybox;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pPanoToCubeDescriptorSet[1], 1, params);
			cmdBindDescriptorSet(cmd, i, pPanoToCubeDescriptorSet[1]);

			const uint32_t* pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers);
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[1] = { { pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers2);

		endCmd(cmd);

		waitBatchCompleted();
		queueSubmit(pGraphicsQueue, 1, &cmd, pTransitionFences, 0, 0, 0, 0);
		waitForFences(pRenderer, 1, &pTransitionFences);

		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);
		removeDescriptorSet(pRenderer, pPanoToCubeDescriptorSet[0]);
		removeDescriptorSet(pRenderer, pPanoToCubeDescriptorSet[1]);

		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, RD_SHADER_SOURCES };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

		const char*       pSkyboxSamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxSamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerLinearRepeat;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);

		//Generate sky box vertex buffer
		static const float skyBoxPoints[] = {
			0.5f,  -0.5f, -0.5f, 1.0f,    // -z
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    //-x
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			0.5f,  -0.5f, -0.5f, 1.0f,    //+x
			0.5f,  -0.5f, 0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    // +z
			-0.5f, 0.5f,  0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			-0.5f, 0.5f,  -0.5f, 1.0f,    //+y
			0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,

			0.5f,  -0.5f, 0.5f,  1.0f,    //-y
			0.5f,  -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f,
			-0.5f, 1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,
		};

		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		skyboxVbDesc.ppBuffer = &pBufferSkyboxVertex;
		addResource(&skyboxVbDesc, true);

		BufferLoadDesc skyboxUBDesc = {};
		skyboxUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		skyboxUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		skyboxUBDesc.mDesc.mSize = sizeof(UniformDataSkybox);
		skyboxUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			skyboxUBDesc.ppBuffer = &pBufferSkyboxUniform[i];
			addResource(&skyboxUBDesc, true);
		}
	}
	//
	void setResourcesToComputeCompliantState(uint32_t frameIdx, bool submitAndWait)
	{
		if (submitAndWait)
		{
			beginCmd(ppCmds[frameIdx]);
		}
		
		BufferBarrier barrier[] = { { pIndirectPosBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pIndirectIndexBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferMeshConstants, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferMaterialProperty, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferFilterIndirectMaterial[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS },
			{ pBufferUncompactedDrawArguments[frameIdx][VIEW_SHADOW], RESOURCE_STATE_UNORDERED_ACCESS },
			{ pBufferUncompactedDrawArguments[frameIdx][VIEW_CAMERA], RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(ppCmds[frameIdx], 7, barrier, 0, NULL);

		BufferBarrier indirectDrawBarriers[gNumGeomSets * NUM_CULLING_VIEWPORTS] = {};
		for (uint32_t i = 0, k = 0; i < gNumGeomSets; i++)
		{
			for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; j++, k++)
			{
				indirectDrawBarriers[k].pBuffer = 
					pBufferFilteredIndirectDrawArguments[frameIdx][i][j];
				indirectDrawBarriers[k].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
				indirectDrawBarriers[k].mSplit = false;
			}
		}
		cmdResourceBarrier(ppCmds[frameIdx], gNumGeomSets * NUM_CULLING_VIEWPORTS, indirectDrawBarriers, 0, NULL);

		BufferBarrier filteredIndicesBarriers[NUM_CULLING_VIEWPORTS] = {};
		for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; j++)
		{
			filteredIndicesBarriers[j].pBuffer = pBufferFilteredIndex[frameIdx][j];
			filteredIndicesBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			filteredIndicesBarriers[j].mSplit = false;
		}
		cmdResourceBarrier(ppCmds[frameIdx], NUM_CULLING_VIEWPORTS, filteredIndicesBarriers, 0, NULL);

		if (submitAndWait)
		{
			endCmd(ppCmds[frameIdx]);
			queueSubmit(pGraphicsQueue, 1, ppCmds, pTransitionFences, 0, NULL, 0, NULL);
			waitForFences(pRenderer, 1, &pTransitionFences);
		}
	}


	void RemoveRenderTargetsAndSwapChain()
	{
		removeRenderTarget(pRenderer, pRenderTargetIntermediate);
		removeRenderTarget(pRenderer, pRenderTargetASMColorPass);
		removeRenderTarget(pRenderer, pRenderTargetASMDEMAtlas);
		removeRenderTarget(pRenderer, pRenderTargetASMDepthAtlas);
		removeRenderTarget(pRenderer, pRenderTargetASMDepthPass);
		for (int32_t i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			removeRenderTarget(pRenderer, pRenderTargetASMIndirection[i]);
			removeRenderTarget(pRenderer, pRenderTargetASMPrerenderIndirection[i]);
		}
		removeRenderTarget(pRenderer, pRenderTargetASMLodClamp);
		removeRenderTarget(pRenderer, pRenderTargetASMPrerenderLodClamp);


		removeRenderTarget(pRenderer, pRenderTargetVBPass);

		removeRenderTarget(pRenderer, pRenderTargetDepth);
		removeRenderTarget(pRenderer, pRenderTargetShadowMap);

		removeRenderTarget(pRenderer, pRenderTargetSDFMeshVisualization);
		removeRenderTarget(pRenderer, pRenderTargetSDFMeshShadow);
		removeRenderTarget(pRenderer, pRenderTargetUpSampleSDFShadow);

		removeSwapChain(pRenderer, pSwapChain);


		
	}
	void Exit() override
	{
		exitInputSystem();
		shutdownThreadSystem(pThreadSystem);

		GuiController::removeGui();
		destroySDFMeshes();

		RemoveDescriptorSets();

		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

		exitProfiler();

		removeResource(pIndirectIndexBuffer);
		removeResource(pIndirectNormalBuffer);
		removeResource(pIndirectPosBuffer);
		removeResource(pIndirectTangentBuffer);
		removeResource(pIndirectTexCoordBuffer);

		conf_delete(pASM);
		conf_delete(pSDFVolumeTextureAtlas);

		conf_free(gDiffuseMapsStorage);
		conf_free(gNormalMapsStorage);
		conf_free(gSpecularMapsStorage);

		if (pTextureSDFVolumeAtlas)
		{
			removeResource(pTextureSDFVolumeAtlas);
			pTextureSDFVolumeAtlas = NULL;
		}
		for (uint32_t i = 0; i < gMaterialCount; ++i)
		{
			removeResource(gDiffuseMaps[i]);
			removeResource(gNormalMaps[i]);
			removeResource(gSpecularMaps[i]);
		}

		gDiffuseMaps.set_capacity(0);
		gNormalMaps.set_capacity(0);
		gSpecularMaps.set_capacity(0);

		gDiffuseMapsPacked.set_capacity(0);
		gNormalMapsPacked.set_capacity(0);
		gSpecularMapsPacked.set_capacity(0);

		gAppUI.Exit();
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferLightUniform[i]);
			removeResource(pBufferESMUniform[i]);
			removeResource(pBufferRenderSettings[i]);
			removeResource(pBufferCameraUniform[i]);

			removeResource(pBufferVisibilityBufferConstants[i]);


			removeResource(pBufferASMAtlasQuadsUniform[i]);
			removeResource(pBufferASMAtlasToColorPackedQuadsUniform[i]);
			removeResource(pBufferASMClearIndirectionQuadsUniform[i]);
			removeResource(pBufferASMColorToAtlasPackedQuadsUniform[i]);
			removeResource(pBufferASMCopyDEMPackedQuadsUniform[i]);
			removeResource(pBufferASMDataUniform[i]);
			removeResource(pBufferASMLodClampPackedQuadsUniform[i]);
			removeResource(pBufferQuadUniform[i]);

			for (int32_t k = 0; k < MESH_COUNT; ++k)
			{
				removeResource(pBufferMeshTransforms[k][i]);
				removeResource(pBufferMeshShadowProjectionTransforms[k][i]);
			}


			for (uint32_t k = 0; k <= gs_ASMMaxRefinement; ++k)
			{
				removeResource(pBufferASMPackedIndirectionQuadsUniform[k][i]);
				removeResource(pBufferASMPackedPrerenderIndirectionQuadsUniform[k][i]);
			}


			removeResource(pBufferMeshSDFConstants[i]);
			removeResource(pBufferUpdateSDFVolumeTextureAtlasConstants[i]);
			removeResource(pBufferSDFVolumeData[i]);
		}
		//removeResource(pBufferSDFVolumeData);
		removeResource(pBufferBoxIndex);
		removeResource(pBufferMaterialProperty);
		removeResource(pBufferMeshConstants);
		removeResource(pBufferQuadVertex);
		removeResource(pBufferSkyboxVertex);
		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pBufferSkyboxUniform[i]);

		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect 
		//is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			removeResource(pBufferIndirectDrawArgumentsAll[i]);
		}

		removeResource(pBufferIndirectMaterialAll);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; ++j)
			{
				removeResource(pBufferFilteredIndex[i][j]);
			}
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferFilterIndirectMaterial[i]);
			for (uint32_t view = 0; view < NUM_CULLING_VIEWPORTS; ++view)
			{
				removeResource(pBufferUncompactedDrawArguments[i][view]);
				for (uint32_t geom = 0; geom < gNumGeomSets; ++geom)
				{
					removeResource(pBufferFilteredIndirectDrawArguments[i][geom][view]);
				}
			}
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				conf_free(pFilterBatchChunk[i][j]->batches);
				conf_free(pFilterBatchChunk[i][j]);
			}
		}

		removeGPURingBuffer(pBufferFilterBatchData);
		
		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			destroyClusters(&pMeshes[i]);
		}
		conf_free(pMeshes);

		for (uint32_t i = 0; i < (uint32_t)gSDFVolumeInstances.size(); ++i)
		{
			if (gSDFVolumeInstances[i])
			{
				conf_delete(gSDFVolumeInstances[i]);
			}
		}

		gVirtualJoystick.Exit();

		removeGpuProfiler(pRenderer, pGpuProfilerGraphics);
		removeGpuProfiler(pRenderer, pGpuProfilerCompute);

		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerMiplessSampler);

		removeSampler(pRenderer, pSamplerComparisonShadow);
		removeSampler(pRenderer, pSamplerMiplessLinear);
		removeSampler(pRenderer, pSamplerMiplessNear);

		removeSampler(pRenderer, pSamplerMiplessClampToBorderNear);
		removeSampler(pRenderer, pSamplerLinearRepeat);

		removeShader(pRenderer, pShaderPresentPass);
		removeShader(pRenderer, pShaderSkybox);
		

		removeShader(pRenderer, pShaderASMCopyDEM);
		removeShader(pRenderer, pShaderASMCopyDepthQuadPass);
		removeShader(pRenderer, pShaderIndirectDepthPass);
		removeShader(pRenderer, pShaderIndirectAlphaDepthPass);

		removeShader(pRenderer, pShaderASMFillIndirection);
		removeShader(pRenderer, pShaderASMGenerateDEM);
		removeShader(pRenderer, pShaderQuad);
		removeShader(pRenderer, pShaderVBShade);

		for (int32_t i = 0; i < gNumGeomSets; ++i)
			removeShader(pRenderer, pShaderVBBufferPass[i]);

		removeShader(pRenderer, pShaderClearBuffers);
		removeShader(pRenderer, pShaderTriangleFiltering);
		removeShader(pRenderer, pShaderBatchCompaction);

		removeShader(pRenderer, pShaderUpdateSDFVolumeTextureAtlas);
		removeShader(pRenderer, pShaderSDFMeshVisualization);
		removeShader(pRenderer, pShaderSDFMeshShadow);
		

		removeShader(pRenderer, pShaderUpsampleSDFShadow);

		removeRootSignature(pRenderer, pRootSignaturePresentPass);
		removeRootSignature(pRenderer, pRootSignatureSkybox);

		removeRootSignature(pRenderer, pRootSignatureASMCopyDEM);
		removeRootSignature(pRenderer, pRootSignatureASMCopyDepthQuadPass);
		removeRootSignature(pRenderer, pRootSignatureASMDEMAtlasToColor);
		removeRootSignature(pRenderer, pRootSignatureASMDEMColorToAtlas);
		

		removeRootSignature(pRenderer, pRootSignatureASMFillIndirection);
		removeRootSignature(pRenderer, pRootSignatureASMFillLodClamp);
		removeRootSignature(pRenderer, pRootSignatureQuad);

		removeRootSignature(pRenderer, pRootSignatureVBPass);

		removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);


		removeRootSignature(pRenderer, pRootSignatureUpdateSDFVolumeTextureAtlas);
		removeRootSignature(pRenderer, pRootSignatureSDFMeshVisualization);
		removeRootSignature(pRenderer, pRootSignatureSDFMeshShadow);

		
		removeRootSignature(pRenderer, pRootSignatureUpsampleSDFShadow);

		removeRootSignature(pRenderer, pRootSignatureVBShade);

		removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);

		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateTestOnly);
		removeDepthState(pDepthStateStencilShadow);
		removeDepthState(pDepthStateDisable);
		removeDepthState(pDepthStateLEQUALEnable);
		removeBlendState(pBlendStateSkyBox);

		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);
		removeRasterizerState(pRasterizerStateCullBack);
		removeRasterizerState(pRasterizerStateNonBiasCullFront);


		removeResource(pTextureSkybox);
		removeFence(pRenderer, pTransitionFences);
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeCmd_n(pComputeCmdPool, gImageCount, ppComputeCmds);
		removeCmdPool(pRenderer, pComputeCmdPool);
		removeQueue(pComputeQueue);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	void Load_ASM_RenderTargets()
	{
		ASMRenderTargets asmRenderTargets = {};
		for (int32_t i = 0; i <= gs_ASMMaxRefinement; ++i)
		{
			asmRenderTargets.m_pASMIndirectionMips.push_back(pRenderTargetASMIndirection[i]);
			asmRenderTargets.m_pASMPrerenderIndirectionMips.push_back(pRenderTargetASMPrerenderIndirection[i]);
		}
		asmRenderTargets.m_pRenderTargetASMLodClamp = pRenderTargetASMLodClamp;
		asmRenderTargets.m_pRenderTargetASMPrerenderLodClamp = pRenderTargetASMPrerenderLodClamp;
		pASM->Load(asmRenderTargets);
		pASM->Reset();
	}

	bool Load() override
	{

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addFence(pRenderer, &pComputeCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
			addSemaphore(pRenderer, &pComputeCompleteSemaphores[i]);
		}
		gFrameCount = 0;

		if (!AddRenderTargetsAndSwapChain())
			return false;
		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		Load_ASM_RenderTargets();
		/************************************************************************/
		// Setup vertex layout for all shaders
		/************************************************************************/
#if defined(__linux__) || defined(METAL)
		VertexLayout vertexLayoutCompleteModel = {};
		vertexLayoutCompleteModel.mAttribCount = 4;
		vertexLayoutCompleteModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutCompleteModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutCompleteModel.mAttribs[0].mBinding = 0;
		vertexLayoutCompleteModel.mAttribs[0].mLocation = 0;
		vertexLayoutCompleteModel.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutCompleteModel.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayoutCompleteModel.mAttribs[1].mBinding = 1;
		vertexLayoutCompleteModel.mAttribs[1].mLocation = 1;
		vertexLayoutCompleteModel.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutCompleteModel.mAttribs[2].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutCompleteModel.mAttribs[2].mBinding = 2;
		vertexLayoutCompleteModel.mAttribs[2].mLocation = 2;
		vertexLayoutCompleteModel.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayoutCompleteModel.mAttribs[3].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutCompleteModel.mAttribs[3].mBinding = 3;
		vertexLayoutCompleteModel.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

		// Position only vertex stream that is used in shadow opaque pass
		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;
#else

		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutPositionOnly.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;


		VertexLayout vertexLayoutCompleteModel = {};
		vertexLayoutCompleteModel.mAttribCount = 4;
		vertexLayoutCompleteModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutCompleteModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutCompleteModel.mAttribs[0].mBinding = 0;
		vertexLayoutCompleteModel.mAttribs[0].mLocation = 0;
		vertexLayoutCompleteModel.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutCompleteModel.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutCompleteModel.mAttribs[1].mBinding = 1;
		vertexLayoutCompleteModel.mAttribs[1].mLocation = 1;
		vertexLayoutCompleteModel.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutCompleteModel.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutCompleteModel.mAttribs[2].mBinding = 2;
		vertexLayoutCompleteModel.mAttribs[2].mLocation = 2;
		vertexLayoutCompleteModel.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayoutCompleteModel.mAttribs[3].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutCompleteModel.mAttribs[3].mBinding = 3;
		vertexLayoutCompleteModel.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

#endif


		VertexLayout vertexLayoutRegular = {};
		vertexLayoutRegular.mAttribCount = 2;
		vertexLayoutRegular.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutRegular.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutRegular.mAttribs[0].mBinding = 0;
		vertexLayoutRegular.mAttribs[0].mLocation = 0;
		vertexLayoutRegular.mAttribs[0].mOffset = 0;
		vertexLayoutRegular.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutRegular.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutRegular.mAttribs[1].mBinding = 0;
		vertexLayoutRegular.mAttribs[1].mLocation = 1;
		vertexLayoutRegular.mAttribs[1].mOffset = 3 * sizeof(float);
#if FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA
		VertexLayout vertexLayoutZPass = {};
		vertexLayoutZPass.mAttribCount = 1;
		vertexLayoutZPass.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutZPass.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutZPass.mAttribs[0].mBinding = 0;
		vertexLayoutZPass.mAttribs[0].mLocation = 0;
		vertexLayoutZPass.mAttribs[0].mOffset = 0;
#endif
		VertexLayout vertexLayoutQuad = {};
		vertexLayoutQuad.mAttribCount = 2;
		vertexLayoutQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutQuad.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayoutQuad.mAttribs[0].mBinding = 0;
		vertexLayoutQuad.mAttribs[0].mLocation = 0;
		vertexLayoutQuad.mAttribs[0].mOffset = 0;

		vertexLayoutQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutQuad.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayoutQuad.mAttribs[1].mBinding = 0;
		vertexLayoutQuad.mAttribs[1].mLocation = 1;
		vertexLayoutQuad.mAttribs[1].mOffset = 4 * sizeof(float);


		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		
		/************************************************************************/
		// Setup the resources needed for upsaming sdf model scene
		/******************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& upSampleSDFShadowPipelineSettings = desc.mGraphicsDesc;
		upSampleSDFShadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		upSampleSDFShadowPipelineSettings.mRenderTargetCount = 1;
		upSampleSDFShadowPipelineSettings.pDepthState = NULL;
		upSampleSDFShadowPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		upSampleSDFShadowPipelineSettings.pRootSignature = pRootSignatureUpsampleSDFShadow;
		upSampleSDFShadowPipelineSettings.pShaderProgram = pShaderUpsampleSDFShadow;
		upSampleSDFShadowPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		upSampleSDFShadowPipelineSettings.pColorFormats = &pRenderTargetUpSampleSDFShadow->mDesc.mFormat;
		upSampleSDFShadowPipelineSettings.mSampleQuality = pRenderTargetUpSampleSDFShadow->mDesc.mSampleQuality;
		upSampleSDFShadowPipelineSettings.pVertexLayout = &vertexLayoutQuad;

		addPipeline(pRenderer, &desc, &pPipelineUpsampleSDFShadow);

		


		/************************************************************************/
		// Setup the resources needed for the Visibility Buffer Pipeline
		/******************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& vbPassPipelineSettings = desc.mGraphicsDesc;
		vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbPassPipelineSettings.mRenderTargetCount = 1;
		vbPassPipelineSettings.pDepthState = pDepthStateEnable;
		vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mDesc.mFormat;
		vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mDesc.mSampleCount;
		vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mDesc.mSampleQuality;
		vbPassPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
		vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			vbPassPipelineSettings.pVertexLayout = (i == GEOMSET_ALPHATESTED) ?
				&vertexLayoutPosAndTex : &vertexLayoutPositionOnly;


			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ?
				pRasterizerStateCullNone : pRasterizerStateCullFront;

			vbPassPipelineSettings.pShaderProgram = pShaderVBBufferPass[i];

#if defined(_DURANGO)
			ExtendedGraphicsPipelineDesc edescs[2] = {};

			edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
			initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
			edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

			edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
			edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;

			if (i == 0)
				edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;

			addPipelineExt(pRenderer, &vbPassPipelineSettings, _countof(edescs), edescs, &pPipelineVBBufferPass[i]);
#else
			addPipeline(pRenderer, &desc, &pPipelineVBBufferPass[i]);
#endif

		}
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& vbShadePipelineSettings = desc.mGraphicsDesc;
		vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbShadePipelineSettings.mRenderTargetCount = 1;
		vbShadePipelineSettings.pDepthState = pDepthStateDisable;
		vbShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;
		vbShadePipelineSettings.pShaderProgram = pShaderVBShade;
		vbShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		vbShadePipelineSettings.pColorFormats = &pRenderTargetIntermediate->mDesc.mFormat;
		vbShadePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

#if defined(_DURANGO) && 1
		ExtendedGraphicsPipelineDesc edescs[2];
		memset(edescs, 0, sizeof(edescs));

		edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
		initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
		//edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

		edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
		edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;

		edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;
		addPipelineExt(pRenderer, &vbShadePipelineSettings, _countof(edescs), edescs, &pPipelineVBShadeSrgb);
#else
		addPipeline(pRenderer, &desc, &pPipelineVBShadeSrgb);
#endif
		desc.mGraphicsDesc = {};
		
		desc.mType = PIPELINE_TYPE_COMPUTE;
		desc.mComputeDesc = {};

		ComputePipelineDesc& clearBufferPipelineSettings = desc.mComputeDesc;
		clearBufferPipelineSettings.pShaderProgram = pShaderClearBuffers;
		clearBufferPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &desc, &pPipelineClearBuffers);




		desc.mComputeDesc = {};
		ComputePipelineDesc& triangleFilteringPipelineSettings = desc.mComputeDesc;
		triangleFilteringPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
		triangleFilteringPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &desc, &pPipelineTriangleFiltering);

		desc.mComputeDesc = {};
		ComputePipelineDesc& batchCompactionPipelineSettings = desc.mComputeDesc;
		batchCompactionPipelineSettings.pShaderProgram = pShaderBatchCompaction;
		batchCompactionPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &desc, &pPipelineBatchCompaction);

		
		desc.mComputeDesc = {};
		/*-----------------------------------------------------------*/
		// Setup the resources needed SDF volume texture update
		/*-----------------------------------------------------------*/
		desc.mComputeDesc = {};
		ComputePipelineDesc& updateSDFVolumeTexturePipeline = desc.mComputeDesc;
		updateSDFVolumeTexturePipeline.pShaderProgram = pShaderUpdateSDFVolumeTextureAtlas;
		updateSDFVolumeTexturePipeline.pRootSignature = pRootSignatureUpdateSDFVolumeTextureAtlas;
		addPipeline(pRenderer, &desc, &pPipelineUpdateSDFVolumeTextureAtlas);

		/*-----------------------------------------------------------*/
		// Setup the resources needed SDF mesh visualization
		/*-----------------------------------------------------------*/
		desc.mComputeDesc = {};
		ComputePipelineDesc& sdfMeshVisualizationDesc = desc.mComputeDesc;
		sdfMeshVisualizationDesc.pShaderProgram = pShaderSDFMeshVisualization;
		sdfMeshVisualizationDesc.pRootSignature = pRootSignatureSDFMeshVisualization;
		addPipeline(pRenderer, &desc, &pPipelineSDFMeshVisualization);


		desc.mComputeDesc = {};

		ComputePipelineDesc& sdfMeshShadowDesc = desc.mComputeDesc;
		sdfMeshShadowDesc.pShaderProgram = pShaderSDFMeshShadow;
		sdfMeshShadowDesc.pRootSignature = pRootSignatureSDFMeshShadow;
		addPipeline(pRenderer, &desc, &pPipelineSDFMeshShadow);


		
		/************************************************************************/
		// Setup Skybox pipeline
		/************************************************************************/

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		desc.mType = PIPELINE_TYPE_GRAPHICS;
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& skyboxPipelineSettings = desc.mGraphicsDesc;
		skyboxPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		skyboxPipelineSettings.mRenderTargetCount = 1;
		skyboxPipelineSettings.pDepthState = NULL;

		skyboxPipelineSettings.pBlendState = pBlendStateSkyBox;

		skyboxPipelineSettings.pColorFormats = &pRenderTargetIntermediate->mDesc.mFormat;
		skyboxPipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		skyboxPipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		skyboxPipelineSettings.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineSettings.pShaderProgram = pShaderSkybox;
		skyboxPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		skyboxPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineSkybox);



		/************************************************************************/
		// Setup the resources needed SDF volume texture update
		/************************************************************************/

		

		/************************************************************************/
		// Setup the resources needed for Sdf box
		/************************************************************************/
		desc.mGraphicsDesc = {};
		
		GraphicsPipelineDesc& ASMIndirectDepthPassPipelineDesc = desc.mGraphicsDesc;
		ASMIndirectDepthPassPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMIndirectDepthPassPipelineDesc.mRenderTargetCount = 0;
		ASMIndirectDepthPassPipelineDesc.pDepthState = pDepthStateEnable;
		ASMIndirectDepthPassPipelineDesc.mDepthStencilFormat = pRenderTargetASMDepthPass->mDesc.mFormat;
		ASMIndirectDepthPassPipelineDesc.mSampleCount = pRenderTargetASMDepthPass->mDesc.mSampleCount;
		ASMIndirectDepthPassPipelineDesc.mSampleQuality = pRenderTargetASMDepthPass->mDesc.mSampleQuality;
		ASMIndirectDepthPassPipelineDesc.pRootSignature = pRootSignatureVBPass;
		ASMIndirectDepthPassPipelineDesc.pShaderProgram = pShaderIndirectDepthPass;
		ASMIndirectDepthPassPipelineDesc.pRasterizerState = pRasterizerStateCullFront;
		ASMIndirectDepthPassPipelineDesc.pVertexLayout = &vertexLayoutPositionOnly;
		addPipeline(pRenderer, &desc, &pPipelineIndirectDepthPass);

		ASMIndirectDepthPassPipelineDesc.pShaderProgram = pShaderIndirectAlphaDepthPass;
		ASMIndirectDepthPassPipelineDesc.pVertexLayout = &vertexLayoutPosAndTex;
		ASMIndirectDepthPassPipelineDesc.pRasterizerState = pRasterizerStateCullNone;

		addPipeline(pRenderer, &desc, &pPipelineIndirectAlphaDepthPass);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& indirectESMDepthPassPipelineDesc = desc.mGraphicsDesc;
		indirectESMDepthPassPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		indirectESMDepthPassPipelineDesc.mRenderTargetCount = 0;
		indirectESMDepthPassPipelineDesc.pDepthState = pDepthStateLEQUALEnable;
		indirectESMDepthPassPipelineDesc.mDepthStencilFormat = pRenderTargetShadowMap->mDesc.mFormat;
		indirectESMDepthPassPipelineDesc.mSampleCount = pRenderTargetShadowMap->mDesc.mSampleCount;
		indirectESMDepthPassPipelineDesc.mSampleQuality = pRenderTargetShadowMap->mDesc.mSampleQuality;
		indirectESMDepthPassPipelineDesc.pRootSignature = pRootSignatureVBPass;
		indirectESMDepthPassPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		indirectESMDepthPassPipelineDesc.pVertexLayout = &vertexLayoutPositionOnly;
		indirectESMDepthPassPipelineDesc.pShaderProgram = pShaderIndirectDepthPass;

		addPipeline(pRenderer, &desc, &pPipelineESMIndirectDepthPass);
		

		indirectESMDepthPassPipelineDesc.pShaderProgram = pShaderIndirectAlphaDepthPass;
		indirectESMDepthPassPipelineDesc.pVertexLayout = &vertexLayoutPosAndTex;

		addPipeline(pRenderer, &desc, &pPipelineESMIndirectAlphaDepthPass);


		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& quadPipelineDesc = desc.mGraphicsDesc;
		quadPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		quadPipelineDesc.mRenderTargetCount = 1;
		quadPipelineDesc.pDepthState = pDepthStateDisable;
		quadPipelineDesc.pColorFormats = &pRenderTargetIntermediate->mDesc.mFormat;
		quadPipelineDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		quadPipelineDesc.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		quadPipelineDesc.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		quadPipelineDesc.pRootSignature = pRootSignatureQuad;
		// END COMMON DATA

		quadPipelineDesc.pShaderProgram = pShaderQuad;
		quadPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		quadPipelineDesc.pVertexLayout = &vertexLayoutQuad;

		addPipeline(pRenderer, &desc, &pPipelineQuad);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMCopyDepthQuadPipelineDesc = desc.mGraphicsDesc;
		ASMCopyDepthQuadPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMCopyDepthQuadPipelineDesc.mRenderTargetCount = 1;
		ASMCopyDepthQuadPipelineDesc.pDepthState = NULL;
		ASMCopyDepthQuadPipelineDesc.pColorFormats = &pRenderTargetASMDepthAtlas->mDesc.mFormat;
		ASMCopyDepthQuadPipelineDesc.mSampleCount = pRenderTargetASMDepthAtlas->mDesc.mSampleCount;
		ASMCopyDepthQuadPipelineDesc.mSampleQuality = pRenderTargetASMDepthAtlas->mDesc.mSampleQuality;
		ASMCopyDepthQuadPipelineDesc.pRootSignature = pRootSignatureASMCopyDepthQuadPass;
		ASMCopyDepthQuadPipelineDesc.pShaderProgram = pShaderASMCopyDepthQuadPass;
		ASMCopyDepthQuadPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMCopyDepthQuadPass);


		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMCopyDEMQuadPipelineDesc = desc.mGraphicsDesc;
		ASMCopyDEMQuadPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMCopyDEMQuadPipelineDesc.mRenderTargetCount = 1;
		ASMCopyDEMQuadPipelineDesc.pDepthState = NULL;
		ASMCopyDEMQuadPipelineDesc.pColorFormats = &pRenderTargetASMDEMAtlas->mDesc.mFormat;
		ASMCopyDEMQuadPipelineDesc.mSampleCount = pRenderTargetASMDEMAtlas->mDesc.mSampleCount;
		ASMCopyDEMQuadPipelineDesc.mSampleQuality = pRenderTargetASMDEMAtlas->mDesc.mSampleQuality;
		ASMCopyDEMQuadPipelineDesc.pRootSignature = pRootSignatureASMCopyDEM;
		ASMCopyDEMQuadPipelineDesc.pShaderProgram = pShaderASMCopyDEM;
		ASMCopyDEMQuadPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMCopyDEM);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMAtlasToColorPipelineDesc = desc.mGraphicsDesc;
		ASMAtlasToColorPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMAtlasToColorPipelineDesc.mRenderTargetCount = 1;
		ASMAtlasToColorPipelineDesc.pDepthState = NULL;
		ASMAtlasToColorPipelineDesc.pColorFormats = &pRenderTargetASMColorPass->mDesc.mFormat;
		ASMAtlasToColorPipelineDesc.mSampleCount = pRenderTargetASMColorPass->mDesc.mSampleCount;
		ASMAtlasToColorPipelineDesc.mSampleQuality = pRenderTargetASMColorPass->mDesc.mSampleQuality;
		ASMAtlasToColorPipelineDesc.pRootSignature = pRootSignatureASMDEMAtlasToColor;
		ASMAtlasToColorPipelineDesc.pShaderProgram = pShaderASMGenerateDEM;
		ASMAtlasToColorPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMDEMAtlasToColor);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMColorToAtlasPipelineDesc = desc.mGraphicsDesc;
		ASMColorToAtlasPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMColorToAtlasPipelineDesc.mRenderTargetCount = 1;
		ASMColorToAtlasPipelineDesc.pDepthState = NULL;
		ASMColorToAtlasPipelineDesc.pColorFormats = &pRenderTargetASMDEMAtlas->mDesc.mFormat;
        ASMColorToAtlasPipelineDesc.mSampleCount = pRenderTargetASMDEMAtlas->mDesc.mSampleCount;
		ASMColorToAtlasPipelineDesc.mSampleQuality = pRenderTargetASMDEMAtlas->mDesc.mSampleQuality;
		ASMColorToAtlasPipelineDesc.pRootSignature = pRootSignatureASMDEMColorToAtlas;
		ASMColorToAtlasPipelineDesc.pShaderProgram = pShaderASMGenerateDEM;
		ASMColorToAtlasPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMDEMColorToAtlas);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMIndirectionPipelineDesc = desc.mGraphicsDesc;
		ASMIndirectionPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMIndirectionPipelineDesc.mRenderTargetCount = 1;
		ASMIndirectionPipelineDesc.pDepthState = NULL;
		ASMIndirectionPipelineDesc.pColorFormats = &pRenderTargetASMIndirection[0]->mDesc.mFormat;
		ASMIndirectionPipelineDesc.mSampleCount = pRenderTargetASMIndirection[0]->mDesc.mSampleCount;
		ASMIndirectionPipelineDesc.mSampleQuality = pRenderTargetASMIndirection[0]->mDesc.mSampleQuality;
		ASMIndirectionPipelineDesc.pRootSignature = pRootSignatureASMFillIndirection;
		ASMIndirectionPipelineDesc.pShaderProgram = pShaderASMFillIndirection;
		ASMIndirectionPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMFillIndirection);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMFillLodClampPipelineDesc = desc.mGraphicsDesc;
		ASMFillLodClampPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMFillLodClampPipelineDesc.mRenderTargetCount = 1;
		ASMFillLodClampPipelineDesc.pDepthState = NULL;
		ASMFillLodClampPipelineDesc.pColorFormats = &pRenderTargetASMLodClamp->mDesc.mFormat;
		ASMFillLodClampPipelineDesc.mSampleCount = pRenderTargetASMLodClamp->mDesc.mSampleCount;
		ASMFillLodClampPipelineDesc.mSampleQuality = pRenderTargetASMLodClamp->mDesc.mSampleQuality;
		ASMFillLodClampPipelineDesc.pRootSignature = pRootSignatureASMFillLodClamp;
		ASMFillLodClampPipelineDesc.pShaderProgram = pShaderASMFillIndirection;
		ASMFillLodClampPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineASMFillLodClamp);

		VertexLayout vertexLayoutCopyShaders = {};
		vertexLayoutCopyShaders.mAttribCount = 0;

		/************************************************************************/
		// Setup Present pipeline
		/************************************************************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& pipelineSettingsFinalPass = desc.mGraphicsDesc;
		pipelineSettingsFinalPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsFinalPass.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettingsFinalPass.mRenderTargetCount = 1;
		pipelineSettingsFinalPass.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsFinalPass.pVertexLayout = &vertexLayoutCopyShaders;
		pipelineSettingsFinalPass.pRootSignature = pRootSignaturePresentPass;
		pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;

		addPipeline(pRenderer, &desc, &pPipelinePresentPass);


		SetupASMDebugTextures();

		PrepareDescriptorSets();

		return true;
	}

	void Unload() override
	{
		waitQueueIdle(pGraphicsQueue);
		waitQueueIdle(pComputeQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
			removeSemaphore(pRenderer, pComputeCompleteSemaphores[i]);
		}

		gVirtualJoystick.Unload();

    unloadProfiler();
		gAppUI.Unload();

		removePipeline(pRenderer, pPipelinePresentPass);
		removePipeline(pRenderer, pPipelineSkybox);

		removePipeline(pRenderer, pPipelineESMIndirectDepthPass);
		removePipeline(pRenderer, pPipelineESMIndirectAlphaDepthPass);

		
		removePipeline(pRenderer, pPipelineASMCopyDEM);
		removePipeline(pRenderer, pPipelineASMCopyDepthQuadPass);
		removePipeline(pRenderer, pPipelineASMDEMAtlasToColor);
		removePipeline(pRenderer, pPipelineASMDEMColorToAtlas);
		removePipeline(pRenderer, pPipelineIndirectAlphaDepthPass);
		removePipeline(pRenderer, pPipelineIndirectDepthPass);

		removePipeline(pRenderer, pPipelineASMFillIndirection);
		removePipeline(pRenderer, pPipelineASMFillLodClamp);
		removePipeline(pRenderer, pPipelineVBShadeSrgb);

		for (int32_t i = 0; i < gNumGeomSets; ++i)
		{
			removePipeline(pRenderer, pPipelineVBBufferPass[i]);
		}
		removePipeline(pRenderer, pPipelineClearBuffers);
		removePipeline(pRenderer, pPipelineTriangleFiltering);
		removePipeline(pRenderer, pPipelineBatchCompaction);

		removePipeline(pRenderer, pPipelineUpdateSDFVolumeTextureAtlas);
		removePipeline(pRenderer, pPipelineSDFMeshVisualization);
		removePipeline(pRenderer, pPipelineSDFMeshShadow);
		removePipeline(pRenderer, pPipelineQuad);
		removePipeline(pRenderer, pPipelineUpsampleSDFShadow);

		if (pUIASMDebugTexturesWindow)
		{
			gAppUI.RemoveGuiComponent(pUIASMDebugTexturesWindow);
			pUIASMDebugTexturesWindow = NULL;
		}

		RemoveRenderTargetsAndSwapChain();
	}


	void UpdateQuadData()
	{
		gQuadUniformData.mModelMat = mat4::translation(vec3(-0.5, -0.5, 0.0)) * mat4::scale(vec3(0.25f));
	}

	
	
	void UpdateMeshSDFConstants()
	{
		const vec3 inverseSDFTextureAtlasSize
		(
			1.f / (float) SDF_VOLUME_TEXTURE_ATLAS_WIDTH,
			1.f / (float) SDF_VOLUME_TEXTURE_ATLAS_HEIGHT,
			1.f / (float) SDF_VOLUME_TEXTURE_ATLAS_DEPTH
		);

		const BakedSDFVolumeInstances& sdfVolumeInstances = gSDFVolumeInstances;
		gMeshSDFConstants.mNumObjects = (uint32_t)sdfVolumeInstances.size() - (uint32_t)pSDFVolumeTextureAtlas->mPendingNodeQueue.size();
		for (int32_t i = 0; i < sdfVolumeInstances.size(); ++i)
		{
			//const mat4& meshModelMat = gMeshInfoUniformData[0].mWorldMat;
			const mat4 meshModelMat = mat4::identity();
			if (!sdfVolumeInstances[i])
			{
				continue;
			}
			const SDFVolumeData& sdfVolumeData = *sdfVolumeInstances[i];


			const AABB& sdfVolumeBBox = sdfVolumeData.mLocalBoundingBox;
			const ivec3& sdfVolumeDimensionSize = sdfVolumeData.mSDFVolumeSize;

			//mat4 volumeToWorldMat = meshModelMat * mat4::translation(sdfVolumeData.mLocalBoundingBox.GetCenter()) 
				//*  mat4::scale(sdfVolumeData.mLocalBoundingBox.GetExtent());
			vec3 sdfVolumeBBoxExtent = calculateAABBExtent(&sdfVolumeBBox);
			float maxExtentValue = maxElem(sdfVolumeBBoxExtent);
					   
			mat4 uniformScaleVolumeToWorld = meshModelMat * mat4::translation(calculateAABBCenter(&sdfVolumeBBox))
				*  mat4::scale(vec3(maxExtentValue));

			vec3 invSDFVolumeDimSize
			(
				1.f / sdfVolumeDimensionSize.getX(),
				1.f / sdfVolumeDimensionSize.getY(),
				1.f / sdfVolumeDimensionSize.getZ()
			);
			gMeshSDFConstants.mWorldToVolumeMat[i] = inverse(uniformScaleVolumeToWorld);
					   
			//get the extent position in the 0.... 1 scale
			vec3 localPositionExtent = sdfVolumeBBoxExtent / maxExtentValue;

			vec3 uvScale = vec3(sdfVolumeDimensionSize.getX() * inverseSDFTextureAtlasSize.getX(),
				sdfVolumeDimensionSize.getY() * inverseSDFTextureAtlasSize.getY(), 
				sdfVolumeDimensionSize.getZ() * inverseSDFTextureAtlasSize.getZ());

			vec3 col0Scale = uniformScaleVolumeToWorld.getCol0().getXYZ();
			vec3 col1Scale = uniformScaleVolumeToWorld.getCol1().getXYZ();
			vec3 col2Scale = uniformScaleVolumeToWorld.getCol2().getXYZ();

			float col0SquaredLength = dot(col0Scale, col0Scale);
			float col1SquaredLength = dot(col1Scale, col1Scale);
			float col2SquaredLength = dot(col2Scale, col2Scale);

			float finalColSquaredLength = fmax(
				fmax(col0SquaredLength, col1SquaredLength), col2SquaredLength);

			float maximumVolumeScale = sqrt(finalColSquaredLength);

			gMeshSDFConstants.mLocalPositionExtent[i] = vec4(localPositionExtent - invSDFVolumeDimSize, 1.f);


			vec3 initialUV = vec3(sdfVolumeDimensionSize.getX() * inverseSDFTextureAtlasSize.getX(),
				sdfVolumeDimensionSize.getY() * inverseSDFTextureAtlasSize.getY(),
				sdfVolumeDimensionSize.getZ() * inverseSDFTextureAtlasSize.getZ()) * 0.5f;
			
			vec3 newUV = vec3(
				initialUV.getX() / localPositionExtent.getX(), 
				initialUV.getY() / localPositionExtent.getY(), 
				initialUV.getZ() / localPositionExtent.getZ());
			
			maximumVolumeScale *= (sdfVolumeData.mIsTwoSided ? -1.f : 1.0f);
			gMeshSDFConstants.mUVScaleAndVolumeScale[i] = vec4(newUV, maximumVolumeScale);

			const ivec3& atlasAllocationCoord = sdfVolumeData.mSDFVolumeTextureNode.mAtlasAllocationCoord;

			vec3 offsetUV = vec3(atlasAllocationCoord.getX() * inverseSDFTextureAtlasSize.getX(),
				atlasAllocationCoord.getY() * inverseSDFTextureAtlasSize.getY(),
				atlasAllocationCoord.getZ() * inverseSDFTextureAtlasSize.getZ());

			offsetUV += (0.5f * uvScale);
			gMeshSDFConstants.mUVAddAndSelfShadowBias[i] = vec4(offsetUV, 0.f);

			gMeshSDFConstants.mSDFMAD[i] = vec4(
				sdfVolumeData.mDistMinMax.getY() - sdfVolumeData.mDistMinMax.getX(),
				sdfVolumeData.mDistMinMax.getX(),
				sdfVolumeData.mTwoSidedWorldSpaceBias,
				0.f
			);

		}
	}

	void Update(float deltaTime) override
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

#if !defined(TARGET_IOS)
		if (pSwapChain->mDesc.mEnableVsync != gAppSettings.mToggleVsync)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif
		if (gLightCpuSettings.mAutomaticSunMovement && gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			gLightCpuSettings.mSunControl.y += deltaTime * gLightCpuSettings.mSunSpeedY;
			if (gLightCpuSettings.mSunControl.y >= (PI - Epilson))
			{
				gLightCpuSettings.mSunControl.y = -(PI);
			}
		}

		pCameraController->update(deltaTime);


		gAppUI.Update(deltaTime);
		GuiController::updateDynamicUI();


		gCurrentShadowType = gRenderSettings.mShadowType;

		if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			calculateCurSDFMeshesProgress();
			gAppSettings.mIsGeneratingSDF = !isThreadSystemIdle(pThreadSystem);
			initSDFVolumeTextureAtlasData();
		}


		/************************************************************************/
		// Scene Render Settings
		/************************************************************************/

		gRenderSettings.mWindowDimension.setX((float)mSettings.mWidth);
		gRenderSettings.mWindowDimension.setY((float)mSettings.mHeight);

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;

		currentTime += deltaTime * 1000.0f;

		if (gCurrentShadowType == SHADOW_TYPE_ASM && gASMCpuSettings.mSunCanMove)
		{
			asmCurrentTime += deltaTime * 1000.0f;
		}
		
		if (gCurrentShadowType == SHADOW_TYPE_ESM)
		{
			gESMUniformData.mEsmControl = gEsmCpuSettings.mEsmControl;
		}
		
		Point3 lightSourcePos(10.f, 000.0f, 10.f);
		lightSourcePos[0] += (20.f);
		lightSourcePos[0] += (SAN_MIGUEL_OFFSETX);
		// directional light rotation & translation
		mat4 rotation = mat4::rotationXY(gLightCpuSettings.mSunControl.x,
			gLightCpuSettings.mSunControl.y);
		mat4 translation = mat4::translation(-vec3(lightSourcePos));


		vec3 newLightDir = vec4((inverse(rotation) * vec4(0, 0, 1, 0))).getXYZ() * -1.f;
		mat4 lightProjMat = mat4::orthographic(-140, 140, -210, 90, -220, 100);
		mat4 lightView = rotation * translation;

		//
		/************************************************************************/
		// ASM Update - for shadow map
		/************************************************************************/
	
		if (gCurrentShadowType == SHADOW_TYPE_ASM)
		{
			mat4 nextRotation = mat4::rotationXY(gLightCpuSettings.mSunControl.x, gLightCpuSettings.mSunControl.y + (PI / 2.f));
			vec3 lightDirDest = -(inverse(nextRotation) * vec4(0, 0, 1, 0)).getXYZ();

			float f = float((static_cast<uint32_t>(asmCurrentTime) >> 5) & 0xfff) / 8096.0f;
			vec3 asmLightDir = normalize(lerp(newLightDir, lightDirDest, fabsf(f * 2.f)));

			

			uint32_t newDelta = static_cast<uint32_t>(deltaTime * 1000.f);
			uint32_t updateDeltaTime = 4500;
			uint32_t  halfWayTime = static_cast<uint32_t>(asmCurrentTime) + (updateDeltaTime >> 1);		
			
			float f_half = float((static_cast<uint32_t>(halfWayTime) >> 5) & 0xfff) / 8096.0f;
			vec3 halfWayLightDir = normalize(lerp(newLightDir, lightDirDest, fabsf(f_half * 2.f)));

			pASM->Tick(gASMCpuSettings, pLightView,  asmLightDir, halfWayLightDir, static_cast<uint32_t>(currentTime),
				newDelta, false, false, updateDeltaTime);
			
		}

	}


	static void drawEsmShadowMap(Cmd* cmd)
	{
		BufferUpdateDesc bufferUpdate = { pBufferMeshShadowProjectionTransforms[0][gFrameIndex], 
			&gMeshASMProjectionInfoUniformData[0][gFrameIndex] };
		updateResource(&bufferUpdate);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadowMap->mDesc.mClearValue;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Draw ESM Shadow Map", true);
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0, NULL, pRenderTargetShadowMap, &loadActions);
		
		cmdBindIndexBuffer(cmd, pBufferFilteredIndex[gFrameIndex][VIEW_SHADOW], 0);

		DescriptorData alphaTestedParams[3] = {};
		alphaTestedParams[0].pName = "objectUniformBlock";
		alphaTestedParams[0].ppBuffers = &pBufferMeshShadowProjectionTransforms[0][gFrameIndex];
		alphaTestedParams[1].pName = "diffuseMaps";
		alphaTestedParams[1].mCount = (uint32_t)gDiffuseMaps.size();
		alphaTestedParams[1].ppTextures = gDiffuseMaps.data();
		alphaTestedParams[2].pName = "indirectMaterialBuffer";
		alphaTestedParams[2].ppBuffers = &pBufferFilterIndirectMaterial[gFrameIndex];


		cmdBindVertexBuffer(cmd, 1, &pIndirectPosBuffer, NULL);
		cmdBindPipeline(cmd, pPipelineESMIndirectDepthPass);

		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(cmd, gFrameIndex * 3 + 1, pDescriptorSetVBPass[2]);

		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass,
			gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_OPAQUE],
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			0,
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

		Buffer* pVertexBuffersPosTex[] = { pIndirectPosBuffer,
			pIndirectTexCoordBuffer };
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);

		cmdBindPipeline(cmd, pPipelineESMIndirectAlphaDepthPass);
		
#ifdef METAL
		// #TODO: Automate this inside the Metal renderer
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(cmd, gFrameIndex * 3 + 2, pDescriptorSetVBPass[2]);
#endif
		
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass,
			gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_ALPHATESTED],
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_SHADOW],
			0,
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_SHADOW],
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}
	static void drawSkybox(Cmd* cmd)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Draw Skybox", true);

		BufferUpdateDesc updateDesc = { pBufferSkyboxUniform[gFrameIndex], &gUniformDataSky, 0, 0, sizeof(gUniformDataSky) };
		updateResource(&updateDesc);

		// Transfer our render target to a render target state
		TextureBarrier barrier[] = { { pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier);

		setRenderTarget(cmd, 1, &pRenderTargetScreen, NULL, NULL);

		// Draw the skybox
		cmdBindPipeline(cmd, pPipelineSkybox);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox[1]);
		cmdBindVertexBuffer(cmd, 1, &pBufferSkyboxVertex, NULL);
		cmdDraw(cmd, 36, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	static void drawSDFVolumeTextureAtlas(Cmd* cmd, SDFVolumeTextureNode* node)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Draw update texture atlas");

		BufferUpdateDesc updateDesc = { pBufferSDFVolumeData[gFrameIndex], 
			&node->mSDFVolumeData->mSDFVolumeList[0] };

		updateDesc.mSize = node->mSDFVolumeData->mSDFVolumeList.size() * sizeof(float);

		updateResource(&updateDesc);

			
		gUpdateSDFVolumeTextureAtlasConstants.mSourceAtlasVolumeMinCoord =
			node->mAtlasAllocationCoord;
		gUpdateSDFVolumeTextureAtlasConstants.mSourceDimensionSize = node->mSDFVolumeData->mSDFVolumeSize;
		gUpdateSDFVolumeTextureAtlasConstants.mSourceAtlasVolumeMaxCoord = node->mAtlasAllocationCoord + (node->mSDFVolumeData->mSDFVolumeSize - ivec3(1));

		BufferUpdateDesc meshSDFConstantUpdate = 
		{ pBufferUpdateSDFVolumeTextureAtlasConstants[gFrameIndex], &gUpdateSDFVolumeTextureAtlasConstants };

		updateResource(&meshSDFConstantUpdate);
	
		TextureBarrier textureBarriers[] = { { pTextureSDFVolumeAtlas, RESOURCE_STATE_UNORDERED_ACCESS } };

		cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers);

		cmdBindPipeline(cmd, pPipelineUpdateSDFVolumeTextureAtlas);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetUpdateSDFVolumeTextureAtlas[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUpdateSDFVolumeTextureAtlas[1]);

		uint32_t* threadGroup = pShaderUpdateSDFVolumeTextureAtlas->mReflection.mStageReflections[0].mNumThreadsPerGroup;

		cmdDispatch(cmd, 
			pTextureSDFVolumeAtlas->mDesc.mWidth / threadGroup[0],
			pTextureSDFVolumeAtlas->mDesc.mHeight / threadGroup[1],
			pTextureSDFVolumeAtlas->mDesc.mDepth / threadGroup[2]);
			   
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	void drawSDFMeshVisualizationOnScene(Cmd* cmd, GpuProfiler* pGpuProfiler)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Visualize SDF Geometry On The Scene");
		TextureBarrier textureBarriers[] = 
		{ 
			{
				pRenderTargetSDFMeshVisualization->pTexture,
				RESOURCE_STATE_UNORDERED_ACCESS
			},
			{
				pTextureSDFVolumeAtlas,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers);

		cmdBindPipeline(cmd, pPipelineSDFMeshVisualization);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSDFMeshVisualization[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSDFMeshVisualization[1]);		
		cmdDispatch(cmd,
			(uint32_t) ceil((float)(pRenderTargetSDFMeshVisualization->pTexture->mDesc.mWidth) / (float)(SDF_MESH_VISUALIZATION_THREAD_X)),
			(uint32_t) ceil((float)(pRenderTargetSDFMeshVisualization->pTexture->mDesc.mHeight) / (float)(SDF_MESH_VISUALIZATION_THREAD_Y)),
			1);
				
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}


	void drawSDFMeshShadow(Cmd* cmd, GpuProfiler* pGpuProfiler)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw SDF mesh shadow");
		
		TextureBarrier textureBarriers[] =
		{
			{
				pRenderTargetSDFMeshShadow->pTexture,
				RESOURCE_STATE_UNORDERED_ACCESS
			},
			{
				pTextureSDFVolumeAtlas,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers);

		cmdBindPipeline(cmd, pPipelineSDFMeshShadow);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSDFMeshShadow[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSDFMeshShadow[1]);

		cmdDispatch(cmd,
			(uint32_t) ceil ((float)(pRenderTargetSDFMeshShadow->pTexture->mDesc.mWidth) / (float)(SDF_MESH_SHADOW_THREAD_X) ),
			(uint32_t) ceil ((float)(pRenderTargetSDFMeshShadow->pTexture->mDesc.mHeight) / (float)(SDF_MESH_SHADOW_THREAD_Y) ),
			1);
		

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	void upSampleSDFShadow(Cmd* cmd)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Up Sample SDF Mesh Shadow");
		
		TextureBarrier textureBarriers[] =
		{
			{
				pRenderTargetSDFMeshShadow->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				pRenderTargetUpSampleSDFShadow->pTexture,
				RESOURCE_STATE_RENDER_TARGET
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetUpSampleSDFShadow->mDesc.mClearValue;

		setRenderTarget(cmd, 1, &pRenderTargetUpSampleSDFShadow, NULL, &loadActions);


		cmdBindPipeline(cmd, pPipelineUpsampleSDFShadow);
		cmdBindVertexBuffer(cmd, 1, &pBufferQuadVertex, NULL);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetUpsampleSDFShadow[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUpsampleSDFShadow[1]);
		cmdDraw(cmd, 6, 0);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	void triangleFilteringPass(Cmd* cmd, GpuProfiler* pGpuProfilerGraphics, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Triangle Filtering Pass", true);

		/************************************************************************/
		// Barriers to transition uncompacted draw buffer to uav
		/************************************************************************/
		BufferBarrier uavBarriers[NUM_CULLING_VIEWPORTS] = {};
		for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
			uavBarriers[i] = { pBufferUncompactedDrawArguments[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, NUM_CULLING_VIEWPORTS, uavBarriers, 0, NULL);
		/************************************************************************/
		// Clear previous indirect arguments
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Clear Buffers", true);
		cmdBindPipeline(cmd, pPipelineClearBuffers);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTriangleFiltering[0]);
		cmdBindDescriptorSet(cmd, frameIdx * 3 + 0, pDescriptorSetTriangleFiltering[1]);
		uint32_t numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Clear Buffers Synchronization", true);
		uint32_t numBarriers = (NUM_CULLING_VIEWPORTS * gNumGeomSets) + NUM_CULLING_VIEWPORTS;
		BufferBarrier* clearBarriers = (BufferBarrier*)alloca(numBarriers * sizeof(BufferBarrier));
		uint32_t index = 0;
		for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
		{
			clearBarriers[index++] = { pBufferUncompactedDrawArguments[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_ALPHATESTED][i], RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_OPAQUE][i], RESOURCE_STATE_UNORDERED_ACCESS };
		}
		cmdResourceBarrier(cmd, numBarriers, clearBarriers, 0, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/
		uint32_t currentSmallBatchChunk = 0;
		uint accumDrawCount = 0;
		uint accumNumTriangles = 0;
		uint accumNumTrianglesAtStartOfBatch = 0;
		uint batchStart = 0;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Filter Triangles", true);
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		cmdBindDescriptorSet(cmd, frameIdx * 3 + 1, pDescriptorSetTriangleFiltering[1]);

		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			MeshIn*           drawBatch = &pMeshes[i];
			FilterBatchChunk* batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
			for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
			{
				const ClusterCompact* clusterCompactInfo = &drawBatch->clusterCompacts[j];
				{
					// cluster culling passed or is turned off
					// We will now add the cluster to the batch to be triangle filtered
					addClusterToBatchChunk(clusterCompactInfo, batchStart, accumDrawCount, accumNumTrianglesAtStartOfBatch, i, batchChunk);
					accumNumTriangles += clusterCompactInfo->triangleCount;
				}

				// check to see if we filled the batch
				if (batchChunk->currentBatchCount >= BATCH_COUNT)
				{
					++accumDrawCount;

					// run the triangle filtering and switch to the next small batch chunk
					filterTriangles(cmd, frameIdx, batchChunk);
					currentSmallBatchChunk = (currentSmallBatchChunk + 1) % gSmallBatchChunkCount;
					batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];

					batchStart = 0;
					accumNumTrianglesAtStartOfBatch = accumNumTriangles;
				}
			}

			// end of that mash, set it up so we can add the next mesh to this culling batch
			if (batchChunk->currentBatchCount > 0)
			{
				FilterBatchChunk* batchChunk2 = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
				++accumDrawCount;

				batchStart = batchChunk2->currentBatchCount;
				accumNumTrianglesAtStartOfBatch = accumNumTriangles;
			}
		}

		gPerFrameData[frameIdx].gDrawCount[GEOMSET_OPAQUE] = accumDrawCount;
		gPerFrameData[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = accumDrawCount;

		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx][currentSmallBatchChunk]);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
			uavBarriers[i] = { pBufferUncompactedDrawArguments[frameIdx][i], RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, NUM_CULLING_VIEWPORTS, uavBarriers, 0, NULL);
		/************************************************************************/
		// Batch compaction
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Batch Compaction", true);
		cmdBindPipeline(cmd, pPipelineBatchCompaction);
		cmdBindDescriptorSet(cmd, frameIdx * 3 + 2, pDescriptorSetTriangleFiltering[1]);
		numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		/************************************************************************/
		/************************************************************************/

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	void filterTriangles(Cmd* cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk)
	{
		UNREF_PARAM(frameIdx);
		// Check if there are batches to filter
		if (batchChunk->currentBatchCount == 0)
			return;

		uint64_t size = BATCH_COUNT * sizeof(SmallBatchData);
		uint32_t batchSize = batchChunk->currentBatchCount * sizeof(SmallBatchData);
		GPURingBufferOffset offset = getGPURingBufferOffset(pBufferFilterBatchData, batchSize, (uint32_t)size);
		BufferUpdateDesc updateDesc = { offset.pBuffer, batchChunk->batches, 0, offset.mOffset, batchSize };
		updateResource(&updateDesc, true);

		DescriptorData params[1] = {};
		params[0].pName = "batchData_rootcbv";
		params[0].pOffsets = &offset.mOffset;
		params[0].pSizes = &size;
		params[0].ppBuffers = &offset.pBuffer;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], 1, params);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTriangleFiltering[0]);
		cmdDispatch(cmd, batchChunk->currentBatchCount, 1, 1);

		// Reset batch chunk to start adding triangles to it
		batchChunk->currentBatchCount = 0;
		batchChunk->currentDrawCallCount = 0;
	}

	// Determines if the cluster can be safely culled performing quick cone-based test on the CPU.
	// Since the triangle filtering kernel operates with 2 views in the same pass, this method must
	// only cull those clusters that are not visible from ANY of the views (camera and shadow views).
	bool cullCluster(const Cluster* cluster, vec3 eyes[NUM_CULLING_VIEWPORTS], uint32_t validNum)
	{
		// Invalid clusters can't be safely culled using the cone based test
		if (cluster->valid)
		{
			uint visibility = 0;
			for (uint32_t i = 0; i < validNum; i++)
			{
				// We move camera position into object space
				vec3 testVec = normalize(eyes[i] - f3Tov3(cluster->coneCenter));

				// Check if we are inside the cone
				if (dot(testVec, f3Tov3(cluster->coneAxis)) < cluster->coneAngleCosine)
				{
					visibility |= (1 << i);
				}
			}
			return (visibility == 0);
		}
		return false;
	}

	void drawVisibilityBufferPass(Cmd* cmd)
	{
		TextureBarrier barriers[] = { 
			{pRenderTargetVBPass->pTexture, RESOURCE_STATE_RENDER_TARGET},
		{pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE} };

		cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

		const char* profileNames[gNumGeomSets] = { "VB pass Opaque", "VB pass Alpha" };
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetVBPass->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		setRenderTarget(cmd, 1, &pRenderTargetVBPass, pRenderTargetDepth, &loadActions);


		Buffer* pIndexBuffer = pBufferFilteredIndex[gFrameIndex][VIEW_CAMERA];
		Buffer* pIndirectMaterialBuffer = pBufferFilterIndirectMaterial[gFrameIndex];

		cmdBindIndexBuffer(cmd, pIndexBuffer, 0);
		
		
		Buffer* pVertexBuffersPosTex[] = { pIndirectPosBuffer,
			pIndirectTexCoordBuffer };
		
		cmdBindPipeline(cmd, pPipelineVBBufferPass[GEOMSET_OPAQUE]);
		
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(cmd, gFrameIndex * 3 + 0, pDescriptorSetVBPass[2]);
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, profileNames[0], true);
		
		Buffer* pIndirectBufferPositionOnly = pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_CAMERA];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_OPAQUE], pIndirectBufferPositionOnly, 0, pIndirectBufferPositionOnly,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, profileNames[1], true);
		cmdBindPipeline(cmd, pPipelineVBBufferPass[GEOMSET_ALPHATESTED]);
		
#ifdef METAL
		// #TODO: Automate this inside the Metal renderer
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVBPass[1]);
		cmdBindDescriptorSet(cmd, gFrameIndex * 3 + 2, pDescriptorSetVBPass[2]);
#endif
		
		Buffer* pIndirectBufferPositionAndTex = 
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_CAMERA];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrameData[gFrameIndex].gDrawCount[GEOMSET_ALPHATESTED],
			pIndirectBufferPositionAndTex, 0, pIndirectBufferPositionAndTex,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}


	//Render a fullscreen triangle to evaluate shading for every pixel.This render step uses the render target generated by DrawVisibilityBufferPass
	// to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index buffer because
	// the triangle positions are calculated internally using vertex_id.
	void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
	{
		if (gCurrentShadowType == SHADOW_TYPE_ASM)
			updateASMUniform();

#if ENABLE_SDF_SHADOW_DOWNSAMPLE
		Texture* sdfShadowTexture = pRenderTargetUpSampleSDFShadow->pTexture;
#else
		Texture* sdfShadowTexture = pRenderTargetSDFMeshShadow->pTexture;
#endif
		Texture* esmShadowMap = pRenderTargetShadowMap->pTexture;

		TextureBarrier textureBarriers[] = {
			{ sdfShadowTexture, RESOURCE_STATE_SHADER_RESOURCE },
			{ esmShadowMap, RESOURCE_STATE_SHADER_RESOURCE },
			{ pRenderTargetVBPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE }
		};
		Buffer* pIndirectBuffers[gNumGeomSets] = { NULL };
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			pIndirectBuffers[i] = pBufferFilteredIndirectDrawArguments[frameIdx][i][VIEW_CAMERA];
		}

		BufferBarrier bufferBarriers[] =
		{
			{ pBufferMeshConstants, RESOURCE_STATE_SHADER_RESOURCE} ,
			{ pIndirectPosBuffer, RESOURCE_STATE_SHADER_RESOURCE },
			{ pIndirectBuffers[0], RESOURCE_STATE_SHADER_RESOURCE },
			{ pIndirectBuffers[1], RESOURCE_STATE_SHADER_RESOURCE } ,
			{ pBufferFilterIndirectMaterial[frameIdx], RESOURCE_STATE_SHADER_RESOURCE }
		};
		cmdResourceBarrier(cmd, 5, bufferBarriers, 3, textureBarriers);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "VB Shade Pass");

		RenderTarget* pDestRenderTarget = pRenderTargetScreen;
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestRenderTarget->mDesc.mClearValue;

		setRenderTarget(cmd, 1, &pDestRenderTarget, NULL, &loadActions);

		cmdBindPipeline(cmd, pPipelineVBShadeSrgb);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBShade[1]);

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		
		drawSkybox(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}
	
	void prepareASM()
	{
		ASMTickData& tickData = gASMTickData;
		IndirectionRenderData& indirectionRenderData = tickData.mIndirectionRenderData;
		for (int32_t i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			indirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[i] =
				pBufferASMPackedIndirectionQuadsUniform[i][gFrameIndex];
		}
		indirectionRenderData.pBufferASMClearIndirectionQuadsUniform = pBufferASMClearIndirectionQuadsUniform[gFrameIndex];
		indirectionRenderData.m_pGraphicsPipeline = pPipelineASMFillIndirection;
		indirectionRenderData.m_pRootSignature = pRootSignatureASMFillIndirection;


		IndirectionRenderData& prerenderIndirectionRenderData = tickData.mPrerenderIndirectionRenderData;

		for (int32_t i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			prerenderIndirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[i] =
				pBufferASMPackedPrerenderIndirectionQuadsUniform[i][gFrameIndex];
		}
		prerenderIndirectionRenderData.pBufferASMClearIndirectionQuadsUniform = pBufferASMClearIndirectionQuadsUniform[gFrameIndex];
		prerenderIndirectionRenderData.m_pGraphicsPipeline = pPipelineASMFillIndirection;
		prerenderIndirectionRenderData.m_pRootSignature = pRootSignatureASMFillIndirection;



		ASMProjectionData mainViewProjection;
		mainViewProjection.mViewMat = gCameraUniformData.mView;
		mainViewProjection.mProjMat = gCameraUniformData.mProject;
		mainViewProjection.mInvViewMat = inverse(mainViewProjection.mViewMat);
		mainViewProjection.mInvProjMat = inverse(mainViewProjection.mProjMat);
		mainViewProjection.mViewProjMat = mainViewProjection.mProjMat * mainViewProjection.mViewMat;
		mainViewProjection.mInvViewProjMat = mainViewProjection.mInvViewMat * mainViewProjection.mInvProjMat;
		
		pASM->PrepareRender(mainViewProjection, false);

		const ASMProjectionData* firstRenderBatchProjection = pASM->m_cache->GetFirstRenderBatchProjection();
		if (firstRenderBatchProjection)
		{
			gPerFrameData[gFrameIndex].gEyeObjectSpace[VIEW_SHADOW] =
				(firstRenderBatchProjection->mInvViewProjMat * vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();

			gVisibilityBufferConstants[gFrameIndex].mWorldViewProjMat[VIEW_SHADOW] =
				firstRenderBatchProjection->mViewProjMat * gMeshInfoUniformData[0][gFrameIndex].mWorldMat;
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mSampleCount = 1;


			vec2 windowSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
#if defined(DIRECT3D12) || defined(METAL)
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mWindowSize = v2ToF2(windowSize);
#elif defined(VULKAN)
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mWindowSize = windowSize;
#endif
		}
	}

	void drawASM(Cmd* cmd)
	{
		ASMProjectionData mainViewProjection;
		mainViewProjection.mViewMat = gCameraUniformData.mView;
		mainViewProjection.mProjMat = gCameraUniformData.mProject;
		mainViewProjection.mInvViewMat = inverse(mainViewProjection.mViewMat);
		mainViewProjection.mInvProjMat = inverse(mainViewProjection.mProjMat);
		mainViewProjection.mViewProjMat = mainViewProjection.mProjMat * mainViewProjection.mViewMat;
		mainViewProjection.mInvViewProjMat = mainViewProjection.mInvViewMat * mainViewProjection.mInvProjMat;

		ASMRendererContext rendererContext;
		rendererContext.m_pCmd = cmd;
		rendererContext.m_pRenderer = pRenderer;
		rendererContext.m_pGpuProfiler = pGpuProfilerGraphics;

		pASM->Render(pRenderTargetASMDepthPass, pRenderTargetASMColorPass, rendererContext,  &mainViewProjection);
	}

	void updateASMUniform()
	{
		gAsmModelUniformBlockData.mIndexTexMat = pASM->m_longRangeShadows->m_indexTexMat;
		gAsmModelUniformBlockData.mPrerenderIndexTexMat = pASM->m_longRangePreRender->m_indexTexMat;
		gAsmModelUniformBlockData.mWarpVector =
			vec4(pASM->m_longRangeShadows->m_receiverWarpVector, 0.0);
		gAsmModelUniformBlockData.mPrerenderWarpVector =
			vec4(pASM->m_longRangePreRender->m_receiverWarpVector, 0.0);
		
		gAsmModelUniformBlockData.mSearchVector =
			vec4(pASM->m_longRangeShadows->m_blockerSearchVector, 0.0);
		gAsmModelUniformBlockData.mPrerenderSearchVector =
			vec4(pASM->m_longRangePreRender->m_blockerSearchVector, 0.0);

		gAsmModelUniformBlockData.mMiscBool.setX(pASM->PreRenderAvailable());
		gAsmModelUniformBlockData.mMiscBool.setY(gASMCpuSettings.mEnableParallax);
		gAsmModelUniformBlockData.mPenumbraSize = gASMCpuSettings.mPenumbraSize;
		BufferUpdateDesc asmUpdateUbDesc =
		{
			pBufferASMDataUniform[gFrameIndex],
			&gAsmModelUniformBlockData
		};
		updateResource(&asmUpdateUbDesc);
	}

	   
	void drawQuad(Cmd* cmd)
	{
		Texture* toDisplayTexture = pRenderTargetASMDepthAtlas->pTexture;
		TextureBarrier quadBarriers[] = {
		{
			toDisplayTexture, RESOURCE_STATE_SHADER_RESOURCE},
			{pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE}
		};

		cmdResourceBarrier(cmd, 0, NULL, 2, quadBarriers);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		setRenderTarget(cmd, 1, &pRenderTargetScreen, pRenderTargetDepth, &loadActions);

		cmdBindPipeline(cmd, pPipelineQuad);
		cmdBindVertexBuffer(cmd, 1, &pBufferQuadVertex, NULL);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetQuad[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetQuad[1]);

		cmdDraw(cmd, 6, 0);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

	}

	void UpdateInDraw()
	{
		// update camera with time
			mat4 viewMat = pCameraController->getViewMatrix();

		
			/************************************************************************/
			// Update Camera
			/************************************************************************/
			const uint32_t width = pSwapChain->mDesc.mWidth;
			const uint32_t height = pSwapChain->mDesc.mHeight;

			float aspectInverse = (float)height / (float)width;
			constexpr float horizontal_fov = PI / 2.0f;
			constexpr float nearValue = 0.1f;
			constexpr float farValue = 1000.f;
			mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, farValue, nearValue);
			

			gCameraUniformData.mView = viewMat;
			gCameraUniformData.mProject = projMat;
			gCameraUniformData.mViewProject = projMat * viewMat;
			gCameraUniformData.mInvProj = inverse(projMat);
			gCameraUniformData.mInvView = inverse(viewMat);
			gCameraUniformData.mInvViewProject = inverse(gCameraUniformData.mViewProject);
			gCameraUniformData.mNear = nearValue;
			gCameraUniformData.mFarNearDiff = farValue - nearValue;    // if OpenGL convention was used this would be 2x the value
			gCameraUniformData.mFarNear = nearValue * farValue;
			gCameraUniformData.mCameraPos = vec4(pCameraController->getViewPosition(), 1.f);

			gCameraUniformData.mTwoOverRes = vec2(1.5f / width, 1.5f / height);
			
			float depthMul = projMat[2][2];
			float depthAdd = projMat[3][2];

			if (depthAdd == 0.f)
			{
				//avoid dividing by 0 in this case
				depthAdd = 0.00000001f;
			}

			if (projMat[3][3] < 1.0f)
			{
				float subtractValue = depthMul / depthAdd;
				subtractValue -= 0.00000001f;
				gCameraUniformData.mDeviceZToWorldZ = vec4(0.f, 0.f, 1.f / depthAdd, subtractValue);
			}
			gCameraUniformData.mWindowSize = vec2((float)width, (float)height);
			

			/************************************************************************/
			// Skybox
			/************************************************************************/
			viewMat.setTranslation(vec3(0));
			gUniformDataSky.mCamPos = pCameraController->getViewPosition();
			gUniformDataSky.mProjectView = mat4::perspective(horizontal_fov, aspectInverse, nearValue, farValue) * viewMat;

			/************************************************************************/
			// Light Matrix Update
			/************************************************************************/
			Point3 lightSourcePos(10.f, 000.0f, 10.f);
			lightSourcePos[0] += (20.f);
			lightSourcePos[0] += (SAN_MIGUEL_OFFSETX);
			// directional light rotation & translation
			mat4 rotation = mat4::rotationXY(gLightCpuSettings.mSunControl.x,
				gLightCpuSettings.mSunControl.y);
			mat4 translation = mat4::translation(-vec3(lightSourcePos));


			vec3 newLightDir = vec4((inverse(rotation) * vec4(0, 0, 1, 0))).getXYZ() * -1.f;
			mat4 lightProjMat = mat4::orthographic(-140, 140, -210, 90, -220, 100);
			mat4 lightView = rotation * translation;

			

			gLightUniformData.mLightPosition = vec4(0.f);
			gLightUniformData.mLightViewProj = lightProjMat * lightView;
			gLightUniformData.mLightColor = vec4(1, 1, 1, 1);
			gLightUniformData.mLightUpVec = transpose(lightView)[1];
			gLightUniformData.mLightDir = newLightDir;
			

			const float lightSourceAngle = clamp(gLightCpuSettings.mSourceAngle, 0.001f, 4.0f) * PI / 180.0f;
			gLightUniformData.mTanLightAngleAndThresholdValue = vec4(tan(lightSourceAngle),
				cos(PI / 2 + lightSourceAngle), SDF_LIGHT_THERESHOLD_VAL, 0.f);


		
		
		
		for (int32_t i = 0; i < MESH_COUNT; ++i)
		{
			gMeshInfoData[i].mTranslationMat = mat4::translation(vec3(gMeshInfoData[i].mTranslation.x,
				gMeshInfoData[i].mTranslation.y, gMeshInfoData[i].mTranslation.z));

			gMeshInfoData[i].mScaleMat = mat4::scale(vec3(gMeshInfoData[i].mScale.x,
				gMeshInfoData[i].mScale.y, gMeshInfoData[i].mScale.z));

			mat4 offsetTranslationMat = mat4::translation(f3Tov3(gMeshInfoData[i].mOffsetTranslation));

			gMeshInfoUniformData[i][gFrameIndex].mWorldMat =  gMeshInfoData[i].mTranslationMat * gMeshInfoData[i].mScaleMat * offsetTranslationMat;
			
			gMeshInfoUniformData[i][gFrameIndex].mWorldViewProjMat = gCameraUniformData.mViewProject * gMeshInfoUniformData[i][gFrameIndex].mWorldMat;

			gMeshASMProjectionInfoUniformData[i][gFrameIndex].mWorldMat = gMeshInfoUniformData[i][gFrameIndex].mWorldMat;

			if (gCurrentShadowType == SHADOW_TYPE_ASM)
			{
				gMeshASMProjectionInfoUniformData[i][gFrameIndex].mWorldViewProjMat = mat4::identity();
			}
			else if (gCurrentShadowType == SHADOW_TYPE_ESM)
			{
				gMeshASMProjectionInfoUniformData[i][gFrameIndex].mWorldViewProjMat = gLightUniformData.mLightViewProj * gMeshInfoUniformData[i][gFrameIndex].mWorldMat;
			}
		}
		
		//only for view camera, for shadow it depends on the alggorithm being uysed
		gPerFrameData[gFrameIndex].gEyeObjectSpace[VIEW_CAMERA] = (gCameraUniformData.mInvView
			* vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();
		
		gVisibilityBufferConstants[gFrameIndex].mWorldViewProjMat[VIEW_CAMERA] = gCameraUniformData.mViewProject * gMeshInfoUniformData[0][gFrameIndex].mWorldMat;
		gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_CAMERA].mWindowSize = { (float)pSwapChain->mDesc.mWidth, (float)pSwapChain->mDesc.mHeight };
		gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_CAMERA].mSampleCount = 1;
	}

	void Draw() override
	{
		if (!gAppSettings.mAsyncCompute)
		{
			acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

			Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
			Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

			// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pRenderCompleteFence);
		}
		else
		{
			if (gFrameCount < gImageCount)
			{
				gFrameIndex = (uint)gFrameCount;
				pRenderer->mCurrentFrameIdx =
					(pRenderer->mCurrentFrameIdx + 1) % pSwapChain->mDesc.mImageCount;
			}
			else
			{
				acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
			}


			Fence* pComputeFence = pComputeCompleteFences[gFrameIndex];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pComputeFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			{
				waitForFences(pRenderer, 1, &pComputeFence);
			}

			if (gFrameCount >= gImageCount)
			{
				Fence* pRenderFence = pRenderCompleteFences[gFrameIndex];
				FenceStatus fenceStatus;
				getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
				if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				{
					waitForFences(pRenderer, 1, &pRenderFence);
				}
			}

		}
		UpdateInDraw();
		if (gCurrentShadowType == SHADOW_TYPE_ASM)
		{
			prepareASM();
		}
		else if (gCurrentShadowType == SHADOW_TYPE_ESM)
		{
			gPerFrameData[gFrameIndex].gEyeObjectSpace[VIEW_SHADOW] =
				(inverse(gLightUniformData.mLightViewProj) * vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();

			gVisibilityBufferConstants[gFrameIndex].mWorldViewProjMat[VIEW_SHADOW] =
				gLightUniformData.mLightViewProj * gMeshInfoUniformData[0][gFrameIndex].mWorldMat;
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mSampleCount = 1;

			vec2 windowSize(ESM_SHADOWMAP_RES, ESM_SHADOWMAP_RES);
#if defined(DIRECT3D12) || defined(METAL)
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mWindowSize = v2ToF2(windowSize);
#elif defined(VULKAN)
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW].mWindowSize = windowSize;
#endif
		}
		else if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			gPerFrameData[gFrameIndex].gEyeObjectSpace[VIEW_SHADOW] = gPerFrameData[gFrameIndex].gEyeObjectSpace[VIEW_CAMERA];

			gVisibilityBufferConstants[gFrameIndex].mWorldViewProjMat[VIEW_SHADOW] =
				gVisibilityBufferConstants[gFrameIndex].mWorldViewProjMat[VIEW_CAMERA];
			gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_SHADOW] = gVisibilityBufferConstants[gFrameIndex].mCullingViewports[VIEW_CAMERA];
		}

		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/

		BufferUpdateDesc renderSettingCbv = { pBufferRenderSettings[gFrameIndex], &gRenderSettings };
		updateResource(&renderSettingCbv);


		for (uint32_t j = 0; j < MESH_COUNT; ++j)
		{
			BufferUpdateDesc viewProjCbv = { pBufferMeshTransforms[j][gFrameIndex], &gMeshInfoUniformData[j][gFrameIndex] };
			updateResource(&viewProjCbv);
		}

		BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex], &gCameraUniformData };
		updateResource(&cameraCbv);

		BufferUpdateDesc lightBufferCbv = { pBufferLightUniform[gFrameIndex], &gLightUniformData };
		updateResource(&lightBufferCbv);

		if (gCurrentShadowType == SHADOW_TYPE_ESM)
		{
			BufferUpdateDesc esmBlurCbv = { pBufferESMUniform[gFrameIndex], &gESMUniformData };
			updateResource(&esmBlurCbv);
		}


		BufferUpdateDesc quadUniformCbv = { pBufferQuadUniform[gFrameIndex], &gQuadUniformData };
		updateResource(&quadUniformCbv);


		/************************************************************************/
		// Compute pass
		/************************************************************************/
		if (gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredTriangles)
		{
			BufferUpdateDesc updateVisibilityBufferConstantDesc = {
				pBufferVisibilityBufferConstants[gFrameIndex], &gVisibilityBufferConstants[gFrameIndex] };

			updateResource(&updateVisibilityBufferConstantDesc);

			/************************************************************************/
			// Triangle filtering async compute pass
			/************************************************************************/
			Cmd* computeCmd = ppComputeCmds[gFrameIndex];

			beginCmd(computeCmd);
			cmdBeginGpuFrameProfile(computeCmd, pGpuProfilerCompute, true);

			triangleFilteringPass(computeCmd, pGpuProfilerCompute, gFrameIndex);

			cmdEndGpuFrameProfile(computeCmd, pGpuProfilerCompute);
			endCmd(computeCmd);
			queueSubmit(
				pComputeQueue, 1, &computeCmd, pComputeCompleteFences[gFrameIndex], 0, NULL, 1,
				&pComputeCompleteSemaphores[gFrameIndex]);
			/************************************************************************/
			/************************************************************************/
		}
		else
		{
			if (gFrameIndex != -1)
			{
				BufferUpdateDesc updateVisibilityBufferConstantDesc = {
					pBufferVisibilityBufferConstants[gFrameIndex], &gVisibilityBufferConstants[gFrameIndex] };

				updateResource(&updateVisibilityBufferConstantDesc);
			}
		}

		/************************************************************************/
		// Rendering
		/************************************************************************/
		// Get command list to store rendering commands for this frame

		if (!gAppSettings.mAsyncCompute || gFrameCount >= gImageCount)
		{

			Cmd* cmd = ppCmds[gFrameIndex];
			pRenderTargetScreen = pRenderTargetIntermediate;
			beginCmd(cmd);



			cmdBeginGpuFrameProfile(cmd, pGpuProfilerGraphics);

			if (!gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredTriangles)
			{
				triangleFilteringPass(cmd, pGpuProfilerGraphics, gFrameIndex);
			}
			{
				const uint32_t numBarriers = (gNumGeomSets * NUM_CULLING_VIEWPORTS) +
					NUM_CULLING_VIEWPORTS + 1 + 2;
				uint32_t       index = 0;
				BufferBarrier  barriers2[numBarriers] = {};
				for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
				{
					barriers2[index++] = { pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[index++] = { pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[index++] = { pBufferFilteredIndex[gFrameIndex][i],
						RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
				}
				barriers2[index++] = { pBufferFilterIndirectMaterial[gFrameIndex], RESOURCE_STATE_SHADER_RESOURCE };
				cmdResourceBarrier(cmd, index, barriers2, 0, NULL);
			}
			eastl::vector<TextureBarrier> barriers(30);
			barriers.clear();

			if (gCurrentShadowType == SHADOW_TYPE_ASM)
			{
				drawASM(cmd);
				setRenderTarget(cmd, 0, NULL, NULL, NULL);
			}
			else if (gCurrentShadowType == SHADOW_TYPE_ESM)
			{
				barriers.emplace_back(TextureBarrier{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_DEPTH_WRITE });
				cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data());
				drawEsmShadowMap(cmd);
				barriers.clear();
				barriers.emplace_back(TextureBarrier{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
				cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data());
			}
			// Draw To Screen
			barriers.clear();
			barriers.emplace_back(TextureBarrier{ pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET });
			barriers.emplace_back(TextureBarrier{ pRenderTargetASMDepthPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
			barriers.emplace_back(TextureBarrier{ pRenderTargetASMDepthAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE });

			for (int32_t i = 0; i <= gs_ASMMaxRefinement; ++i)
			{
				barriers.emplace_back(TextureBarrier{ pRenderTargetASMIndirection[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
				barriers.emplace_back(TextureBarrier{ pRenderTargetASMPrerenderIndirection[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
			}

			barriers.emplace_back(TextureBarrier{ pASM->m_longRangePreRender->m_lodClampTexture->pTexture, RESOURCE_STATE_SHADER_RESOURCE });

			barriers.emplace_back(TextureBarrier{
				pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE });

			cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data());

			if (gCurrentShadowType == SHADOW_TYPE_ASM || gCurrentShadowType == SHADOW_TYPE_ESM)
			{
				drawVisibilityBufferPass(cmd);
				drawVisibilityBufferShade(cmd, gFrameIndex);
			}
			else if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
			{
				SDFVolumeTextureNode* volumeTextureNode = pSDFVolumeTextureAtlas->ProcessQueuedNode();

				if (volumeTextureNode)
				{
					drawSDFVolumeTextureAtlas(cmd, volumeTextureNode);
					UpdateMeshSDFConstants();

					gBufferUpdateSDFMeshConstantFlags[0] = true;
					gBufferUpdateSDFMeshConstantFlags[1] = true;
					gBufferUpdateSDFMeshConstantFlags[2] = true;
				}


				drawVisibilityBufferPass(cmd);				
				if (volumeTextureNode || gBufferUpdateSDFMeshConstantFlags[gFrameIndex])
				{
					BufferUpdateDesc sdfMeshConstantsUniformCbv =
					{ pBufferMeshSDFConstants[gFrameIndex], &gMeshSDFConstants };
					updateResource(&sdfMeshConstantsUniformCbv);
					if (!volumeTextureNode)
					{
						gBufferUpdateSDFMeshConstantFlags[gFrameIndex] = false;
					}
				}


				if (gBakedSDFMeshSettings.mDrawSDFMeshVisualization)
				{
					drawSDFMeshVisualizationOnScene(cmd, pGpuProfilerGraphics);
				}
				
				else
				{
					drawSDFMeshShadow(cmd, pGpuProfilerGraphics);

#if ENABLE_SDF_SHADOW_DOWNSAMPLE
					upSampleSDFShadow(cmd);
#endif
				}

				drawVisibilityBufferShade(cmd, gFrameIndex);
			}
			
			RenderTarget* pSrcRT = NULL;
			uint32_t index = 0;
			RenderTarget* pDstRT = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
			if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF && gBakedSDFMeshSettings.mDrawSDFMeshVisualization)
			{
				index = 1;
				pSrcRT = pRenderTargetSDFMeshVisualization;
			}
			else
			{
				pSrcRT = pRenderTargetScreen;
			}

			TextureBarrier barrier[] =
			{
				{ pSrcRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDstRT->pTexture, RESOURCE_STATE_RENDER_TARGET } };

			cmdResourceBarrier(cmd, 0, NULL, 2, barrier);
			presentImage(cmd, pSrcRT->pTexture, index, pDstRT);

			drawGUI(cmd, gFrameIndex);

#ifdef _DURANGO
			// When async compute is on, we need to transition some resources in the graphics queue
			// because they can't be transitioned by the compute queue (incompatible)
			if (gAppSettings.mAsyncCompute)
				setResourcesToComputeCompliantState(gFrameIndex, false);
#else
			{
				const uint32_t numBarriers = (gNumGeomSets * NUM_CULLING_VIEWPORTS) + NUM_CULLING_VIEWPORTS + 1;
				uint32_t index = 0;
				TextureBarrier barrierPresent = { pDstRT->pTexture, RESOURCE_STATE_PRESENT };
				BufferBarrier barriers2[numBarriers] = {};
				for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
				{
					barriers2[index++] = { pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[index++] = { pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[index++] = { pBufferFilteredIndex[gFrameIndex][i], RESOURCE_STATE_UNORDERED_ACCESS };
				}
				barriers2[index++] = { pBufferFilterIndirectMaterial[gFrameIndex], RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(cmd, numBarriers, barriers2, 1, &barrierPresent);
			}
#endif
			cmdEndGpuFrameProfile(cmd, pGpuProfilerGraphics);
			endCmd(cmd);

			if (gAppSettings.mAsyncCompute)
			{
				Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, 
					pComputeCompleteSemaphores[gFrameIndex] };

				queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFences[gFrameIndex], 2,
					pWaitSemaphores, 1, &pRenderCompleteSemaphores[gFrameIndex]);
			}
			else
			{
				queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFences[gFrameIndex], 1, &pImageAcquiredSemaphore, 1,
					&pRenderCompleteSemaphores[gFrameIndex]);
			}
			queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1,
				&pRenderCompleteSemaphores[gFrameIndex]);
			flipProfiler();
		}
		++gFrameCount;
	}

	void drawGUI(Cmd* cmd, uint32_t frameIdx)
	{

		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Draw UI");

		UNREF_PARAM(frameIdx);
		pRenderTargetScreen = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
#if !defined(TARGET_IOS)
		cmdBindRenderTargets(cmd, 1, &pRenderTargetScreen, NULL, NULL, NULL, NULL, -1, -1);

		if (gAppSettings.mMicroProfiler)
		{
			cmdDrawProfiler();
		}
		else
		{
			gTimer.GetUSec(true);
			gAppUI.DrawText(
				cmd, float2(8.0f, 15.0f), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

			if (gAppSettings.mAsyncCompute)
			{
				if ( !gAppSettings.mHoldFilteredTriangles)
				{
					float time =
						 fmax((float)pGpuProfilerGraphics->mCumulativeTime * 1000.0f, (float)pGpuProfilerCompute->mCumulativeTime * 1000.0f);
					gAppUI.DrawText(cmd, float2(8.0f, 40.0f), eastl::string().sprintf("GPU %f ms", time).c_str(), &gFrameTimeDraw);

					gAppUI.DrawText(
						cmd, float2(8.0f, 65.0f),
						eastl::string().sprintf("Compute Queue %f ms", (float)pGpuProfilerCompute->mCumulativeTime * 1000.0f).c_str(),
						&gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8.0f, 90.0f), pGpuProfilerCompute, NULL);
					gAppUI.DrawText(
						cmd, float2(8.0f, 300.0f),
						eastl::string().sprintf("Graphics Queue %f ms", (float)pGpuProfilerGraphics->mCumulativeTime * 1000.0f).c_str(),
						&gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8.0f, 325.0f), pGpuProfilerGraphics, NULL);
				}
				else
				{
					float time = (float)pGpuProfilerGraphics->mCumulativeTime * 1000.0f;
					gAppUI.DrawText(cmd, float2(8.0f, 40.0f), eastl::string().sprintf("GPU %f ms", time).c_str(), &gFrameTimeDraw);
					gAppUI.DrawDebugGpuProfile(cmd, float2(8.0f, 65.0f), pGpuProfilerGraphics, NULL);
				}
			}
			else
			{

#if 1
				// NOTE: Realtime GPU Profiling is not supported on Metal.

				gAppUI.DrawText(
					cmd, float2(8.0f, 40.0f),
					eastl::string().sprintf("GPU %f ms", (float)pGpuProfilerGraphics->mCumulativeTime * 1000.0f).c_str(), &gFrameTimeDraw);
				gAppUI.DrawDebugGpuProfile(cmd, float2(8.0f, 65.0f), pGpuProfilerGraphics, NULL);
			}
		}


		gAppUI.Gui(pGuiWindow);

		if (gAppSettings.mIsGeneratingSDF)
		{
			gAppUI.Gui(pLoadingGui);
		}

		if (pUIASMDebugTexturesWindow)
		{
			gAppUI.Gui(pUIASMDebugTexturesWindow);
		}
		
#endif

		gAppUI.Draw(cmd);
#endif

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	void presentImage(Cmd* cmd, Texture* pSrc, uint32_t index, RenderTarget* pDstCol)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Present Image", true);

		cmdBindRenderTargets(cmd, 1, &pDstCol, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDstCol->mDesc.mWidth, (float)pDstCol->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDstCol->mDesc.mWidth, pDstCol->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelinePresentPass);
		cmdBindDescriptorSet(cmd, index, pDescriptorSetPresentPass);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	const char* GetName() override { return "09_LightShadowPlayground"; }

	bool addSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;
		SwapChainDesc  swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = width;
		swapChainDesc.mHeight = height;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };

		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		return pSwapChain != NULL;
	}

	bool AddRenderTargetsAndSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		const ClearValue depthStencilClear = { 0.0f, 0 };
		//Used for ESM render target shadow
		const ClearValue lessEqualDepthStencilClear = { 1.f, 0 };

		const ClearValue reverseDepthStencilClear = { 1.0f, 0 };
		const ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };
		const ClearValue colorClearWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

		addSwapChain();

		/************************************************************************/
		// Main depth buffer
		/************************************************************************/
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = depthStencilClear;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mWidth = width;
		depthRT.mHeight = height;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.pDebugName = L"Depth RT";
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
#ifdef METAL
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);

		

		/************************************************************************/
		// Intermediate render target
		/************************************************************************/
		RenderTargetDesc postProcRTDesc = {};
		postProcRTDesc.mArraySize = 1;
		postProcRTDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		postProcRTDesc.mDepth = 1;
		postProcRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		postProcRTDesc.mHeight = mSettings.mHeight;
		postProcRTDesc.mWidth = mSettings.mWidth;
		postProcRTDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		postProcRTDesc.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		postProcRTDesc.pDebugName = L"pIntermediateRenderTarget";
		addRenderTarget(pRenderer, &postProcRTDesc, &pRenderTargetIntermediate);
			   
		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/
		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue.depth = lessEqualDepthStencilClear.depth;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		shadowRTDesc.mWidth = ESM_SHADOWMAP_RES;
		shadowRTDesc.mHeight = ESM_SHADOWMAP_RES;
		shadowRTDesc.mSampleCount = (SampleCount)ESM_MSAA_SAMPLES;
		shadowRTDesc.mSampleQuality = 0;  
		shadowRTDesc.pDebugName = L"Shadow Map RT";

		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadowMap);



		/*************************************/
		//SDF mesh visualization render target
		/*************************************/

		RenderTargetDesc sdfMeshVisualizationRTDesc = {};
		sdfMeshVisualizationRTDesc.mArraySize = 1;
		sdfMeshVisualizationRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		sdfMeshVisualizationRTDesc.mClearValue = colorClearBlack;
		sdfMeshVisualizationRTDesc.mDepth = 1;
		sdfMeshVisualizationRTDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		sdfMeshVisualizationRTDesc.mWidth = mSettings.mWidth / SDF_SHADOW_DOWNSAMPLE_VALUE;
		sdfMeshVisualizationRTDesc.mHeight = mSettings.mHeight / SDF_SHADOW_DOWNSAMPLE_VALUE;
		sdfMeshVisualizationRTDesc.mMipLevels = 1;
		sdfMeshVisualizationRTDesc.mSampleCount = SAMPLE_COUNT_1;
		sdfMeshVisualizationRTDesc.mSampleQuality = 0;
		sdfMeshVisualizationRTDesc.pDebugName = L"SDF Mesh Visualization RT";
		addRenderTarget(pRenderer, &sdfMeshVisualizationRTDesc, &pRenderTargetSDFMeshVisualization);


		RenderTargetDesc sdfMeshShadowRTDesc = {};
		sdfMeshShadowRTDesc.mArraySize = 1;
		sdfMeshShadowRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		sdfMeshShadowRTDesc.mClearValue = colorClearBlack;
		sdfMeshShadowRTDesc.mDepth = 1;
		sdfMeshShadowRTDesc.mFormat = TinyImageFormat_R32G32_SFLOAT;
#if ENABLE_SDF_SHADOW_DOWNSAMPLE
		sdfMeshShadowRTDesc.mWidth = mSettings.mWidth / SDF_SHADOW_DOWNSAMPLE_VALUE;
		sdfMeshShadowRTDesc.mHeight = mSettings.mHeight / SDF_SHADOW_DOWNSAMPLE_VALUE;
#else
		sdfMeshShadowRTDesc.mWidth = mSettings.mWidth;
		sdfMeshShadowRTDesc.mHeight = mSettings.mHeight;
#endif
		sdfMeshShadowRTDesc.mMipLevels = 1;
		sdfMeshShadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
		sdfMeshShadowRTDesc.mSampleQuality = 0;
		sdfMeshShadowRTDesc.pDebugName = L"SDF Mesh Shadow Pass RT";
		addRenderTarget(pRenderer, &sdfMeshShadowRTDesc, &pRenderTargetSDFMeshShadow);


		RenderTargetDesc upSampleSDFShadowRTDesc = {};
		upSampleSDFShadowRTDesc.mArraySize = 1;
		upSampleSDFShadowRTDesc.mClearValue = colorClearBlack;
		upSampleSDFShadowRTDesc.mDepth = 1;
		upSampleSDFShadowRTDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		upSampleSDFShadowRTDesc.mWidth = mSettings.mWidth;
		upSampleSDFShadowRTDesc.mHeight = mSettings.mHeight;
		upSampleSDFShadowRTDesc.mMipLevels = 1;
		upSampleSDFShadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
		upSampleSDFShadowRTDesc.mSampleQuality = 0;
		upSampleSDFShadowRTDesc.pDebugName = L"Upsample SDF Mesh Shadow";
		addRenderTarget(pRenderer, &upSampleSDFShadowRTDesc, &pRenderTargetUpSampleSDFShadow);


		/************************************************************************/
		// ASM Depth Pass Render Target
		/************************************************************************/

		RenderTargetDesc ASMDepthPassRT = {};
		ASMDepthPassRT.mArraySize = 1;
		ASMDepthPassRT.mClearValue.depth = depthStencilClear.depth;
		ASMDepthPassRT.mDepth = 1;
		ASMDepthPassRT.mFormat = TinyImageFormat_D32_SFLOAT;
		ASMDepthPassRT.mMipLevels = 1;
		ASMDepthPassRT.mSampleCount = SAMPLE_COUNT_1;
		ASMDepthPassRT.mSampleQuality = 0;
		ASMDepthPassRT.mWidth = ASM_WORK_BUFFER_DEPTH_PASS_WIDTH;
		ASMDepthPassRT.mHeight = ASM_WORK_BUFFER_DEPTH_PASS_HEIGHT;
		ASMDepthPassRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
#ifdef METAL
		ASMDepthPassRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &ASMDepthPassRT, &pRenderTargetASMDepthPass);


		/*************************************************************************/
		// ASM Color Pass Render Target
		/*************************************************************************/

		RenderTargetDesc ASMColorPassRT = {};
		ASMColorPassRT.mArraySize = 1;
		ASMColorPassRT.mClearValue = colorClearWhite;
		ASMColorPassRT.mDepth = 1;
		ASMColorPassRT.mFormat = TinyImageFormat_R32_SFLOAT;
		ASMColorPassRT.mMipLevels = 1;
		ASMColorPassRT.mSampleCount = SAMPLE_COUNT_1;
		ASMColorPassRT.mSampleQuality = 0;
		ASMColorPassRT.mWidth = ASM_WORK_BUFFER_COLOR_PASS_WIDTH;
		ASMColorPassRT.mHeight = ASM_WORK_BUFFER_COLOR_PASS_HEIGHT;
		ASMColorPassRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		addRenderTarget(pRenderer, &ASMColorPassRT, &pRenderTargetASMColorPass);


		/************************************************************************/
		// Visibility buffer Pass Render Target
		/************************************************************************/
		RenderTargetDesc vbRTDesc = {};
		vbRTDesc.mArraySize = 1;
		vbRTDesc.mClearValue = colorClearWhite;
		vbRTDesc.mDepth = 1;
		vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		vbRTDesc.mWidth = width;
		vbRTDesc.mHeight = height;
		vbRTDesc.mSampleCount = SAMPLE_COUNT_1;
		vbRTDesc.mSampleQuality = 0;
		vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		vbRTDesc.pDebugName = L"VB RT";
		addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);



		/************************************************************************/
		// ASM Depth Atlas Render Target
		/************************************************************************/

		RenderTargetDesc depthAtlasRTDesc = {};
		depthAtlasRTDesc.mArraySize = 1;
		depthAtlasRTDesc.mClearValue = colorClearBlack;
		depthAtlasRTDesc.mDepth = 1;
		depthAtlasRTDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		depthAtlasRTDesc.mWidth = gs_ASMDepthAtlasTextureWidth;
		depthAtlasRTDesc.mHeight = gs_ASMDepthAtlasTextureHeight;
		depthAtlasRTDesc.mSampleCount = SAMPLE_COUNT_1;
		depthAtlasRTDesc.mSampleQuality = 0;
		depthAtlasRTDesc.pDebugName = L"ASM Depth Atlas RT";
		depthAtlasRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
#ifdef METAL
		depthAtlasRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthAtlasRTDesc, &pRenderTargetASMDepthAtlas);


		RenderTargetDesc DEMAtlasRTDesc = {};
		DEMAtlasRTDesc.mArraySize = 1;
		DEMAtlasRTDesc.mClearValue = colorClearBlack;
		DEMAtlasRTDesc.mDepth = 1;
		DEMAtlasRTDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		DEMAtlasRTDesc.mWidth = gs_ASMDEMAtlasTextureWidth;
		DEMAtlasRTDesc.mHeight = gs_ASMDEMAtlasTextureHeight;
		DEMAtlasRTDesc.mSampleCount = SAMPLE_COUNT_1;
		DEMAtlasRTDesc.mSampleQuality = 0;
		DEMAtlasRTDesc.pDebugName = L"ASM DEM Atlas RT";
		DEMAtlasRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
#ifdef METAL
		DEMAtlasRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		
		addRenderTarget(pRenderer, &DEMAtlasRTDesc, &pRenderTargetASMDEMAtlas);

		/************************************************************************/
		// ASM Indirection texture Render Target
		/************************************************************************/

		uint32 indirectionTextureSize = (1 << gs_ASMMaxRefinement) * gsASMIndexSize;

		RenderTargetDesc indirectionRTDesc = {};
		indirectionRTDesc.mArraySize = 1;
		indirectionRTDesc.mClearValue = colorClearBlack;
		indirectionRTDesc.mDepth = 1;
		indirectionRTDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		indirectionRTDesc.mWidth = indirectionTextureSize;
		indirectionRTDesc.mHeight = indirectionTextureSize;
		indirectionRTDesc.mSampleCount = SAMPLE_COUNT_1;
		indirectionRTDesc.mSampleQuality = 0;
		indirectionRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		indirectionRTDesc.mMipLevels = 1;
#ifdef METAL
		indirectionRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif

		indirectionRTDesc.pDebugName = L"ASM Indirection RT";

		for (int32_t i = 0; i <= gs_ASMMaxRefinement; ++i)
		{
			uint32 mewIndirectionTextureSize = (indirectionTextureSize >> i);
			indirectionRTDesc.mWidth = mewIndirectionTextureSize;
			indirectionRTDesc.mHeight = mewIndirectionTextureSize;
			addRenderTarget(pRenderer, &indirectionRTDesc, &pRenderTargetASMIndirection[i]);
			addRenderTarget(pRenderer, &indirectionRTDesc, &pRenderTargetASMPrerenderIndirection[i]);
		}

		RenderTargetDesc lodClampRTDesc = {};
		lodClampRTDesc.mArraySize = 1;
		lodClampRTDesc.mClearValue = colorClearWhite;
		lodClampRTDesc.mDepth = 1;
		lodClampRTDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		lodClampRTDesc.mWidth = indirectionTextureSize;
		lodClampRTDesc.mHeight = indirectionTextureSize;
		lodClampRTDesc.mSampleCount = SAMPLE_COUNT_1;
		lodClampRTDesc.mSampleQuality = 0;
		lodClampRTDesc.mMipLevels = 1;
		lodClampRTDesc.pDebugName = L"ASM Lod Clamp RT";
		lodClampRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		
#ifdef METAL
		lodClampRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif

		addRenderTarget(pRenderer, &lodClampRTDesc, &pRenderTargetASMLodClamp);
		addRenderTarget(pRenderer, &lodClampRTDesc, &pRenderTargetASMPrerenderLodClamp);
		return true;
	}

	bool AddDescriptorSets()
	{
		// Triangle filtering
		DescriptorSetDesc setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);
		setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 3 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);
		// VB Shade
		setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[0]);
		setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[1]);
		// Quad
		setDesc = { pRootSignatureQuad, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetQuad[0]);
		setDesc = { pRootSignatureQuad, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetQuad[1]);
		// Present Pass
		setDesc = { pRootSignaturePresentPass, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPresentPass);
		// ASM Atlas to Color
		setDesc = { pRootSignatureASMDEMAtlasToColor, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMDEMAtlasToColor[0]);
		setDesc = { pRootSignatureASMDEMAtlasToColor, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMDEMAtlasToColor[1]);
		// ASM Color to Atlas
		setDesc = { pRootSignatureASMDEMColorToAtlas, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMDEMColorToAtlas[0]);
		setDesc = { pRootSignatureASMDEMColorToAtlas, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMDEMColorToAtlas[1]);
		// ASM Depth, VB Pass
		setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[0]);
		setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[1]);
		setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gImageCount * 3 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[2]);
		// ASM Fill LOD Clamp
		setDesc = { pRootSignatureASMFillLodClamp, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMFillLodClamp);
		// ASM Copy Depth
		setDesc = { pRootSignatureASMCopyDepthQuadPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMCopyDepthQuadPass[0]);
		setDesc = { pRootSignatureASMCopyDepthQuadPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMCopyDepthQuadPass[1]);
		// ASM Copy DEM
		setDesc = { pRootSignatureASMCopyDEM, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMCopyDEM[0]);
		setDesc = { pRootSignatureASMCopyDEM, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMCopyDEM[1]);
		// Skybox
		setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		// Update SDF
		setDesc = { pRootSignatureUpdateSDFVolumeTextureAtlas, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUpdateSDFVolumeTextureAtlas[0]);
		setDesc = { pRootSignatureUpdateSDFVolumeTextureAtlas, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUpdateSDFVolumeTextureAtlas[1]);
		// SDF Visualization
		setDesc = { pRootSignatureSDFMeshVisualization, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSDFMeshVisualization[0]);
		setDesc = { pRootSignatureSDFMeshVisualization, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSDFMeshVisualization[1]);
		// SDF Shadow
		setDesc = { pRootSignatureSDFMeshShadow, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSDFMeshShadow[0]);
		setDesc = { pRootSignatureSDFMeshShadow, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSDFMeshShadow[1]);
		// Upsample SDF
		setDesc = { pRootSignatureUpsampleSDFShadow, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUpsampleSDFShadow[0]);
		setDesc = { pRootSignatureUpsampleSDFShadow, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUpsampleSDFShadow[1]);
		// ASM indirection
		setDesc = { pRootSignatureASMFillIndirection, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMFillIndirection[0]);
		setDesc = { pRootSignatureASMFillIndirection, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * (gs_ASMMaxRefinement + 1) };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMFillIndirection[1]);
		setDesc = { pRootSignatureASMFillIndirection, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * (gs_ASMMaxRefinement + 1) };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetASMFillIndirection[2]);

		return true;
	}

	void RemoveDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetVBShade[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetVBShade[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetQuad[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetQuad[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetPresentPass);
		removeDescriptorSet(pRenderer, pDescriptorSetASMDEMAtlasToColor[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMDEMAtlasToColor[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMDEMColorToAtlas[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMDEMColorToAtlas[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetVBPass[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetVBPass[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetVBPass[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMFillLodClamp);
		removeDescriptorSet(pRenderer, pDescriptorSetASMCopyDepthQuadPass[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMCopyDepthQuadPass[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMCopyDEM[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMCopyDEM[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetUpdateSDFVolumeTextureAtlas[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetUpdateSDFVolumeTextureAtlas[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetSDFMeshShadow[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSDFMeshShadow[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetSDFMeshVisualization[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSDFMeshVisualization[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetUpsampleSDFShadow[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetUpsampleSDFShadow[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMFillIndirection[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMFillIndirection[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetASMFillIndirection[2]);

	}

	void PrepareDescriptorSets()
	{
		// Triangle Filtering
		{
			uint64_t size = BATCH_COUNT * sizeof(SmallBatchData);
			DescriptorData filterParams[5] = {};
			filterParams[0].pName = "vertexDataBuffer";
			filterParams[0].ppBuffers = &pIndirectPosBuffer;
			filterParams[1].pName = "indexDataBuffer";
			filterParams[1].ppBuffers = &pIndirectIndexBuffer;
			filterParams[2].pName = "meshConstantsBuffer";
			filterParams[2].ppBuffers = &pBufferMeshConstants;
			filterParams[3].pName = "materialProps";
			filterParams[3].ppBuffers = &pBufferMaterialProperty;
			filterParams[4].pName = "batchData_rootcbv";
			filterParams[4].ppBuffers = &pBufferFilterBatchData->pBuffer;
			filterParams[4].pSizes = &size;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], 5, filterParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData clearParams[3] = {};
				clearParams[0].pName = "indirectDrawArgsBufferAlpha";
				clearParams[0].mCount = NUM_CULLING_VIEWPORTS;
				clearParams[0].ppBuffers = pBufferFilteredIndirectDrawArguments[i][GEOMSET_ALPHATESTED];
				clearParams[1].pName = "indirectDrawArgsBufferNoAlpha";
				clearParams[1].mCount = NUM_CULLING_VIEWPORTS;
				clearParams[1].ppBuffers = pBufferFilteredIndirectDrawArguments[i][GEOMSET_OPAQUE];
				clearParams[2].pName = "uncompactedDrawArgsRW";
				clearParams[2].mCount = NUM_CULLING_VIEWPORTS;
				clearParams[2].ppBuffers = pBufferUncompactedDrawArguments[i];
				updateDescriptorSet(pRenderer, i * 3 + 0, pDescriptorSetTriangleFiltering[1], 3, clearParams);

				DescriptorData filterParams[3] = {};
				filterParams[0].pName = "filteredIndicesBuffer";
				filterParams[0].mCount = NUM_CULLING_VIEWPORTS;
				filterParams[0].ppBuffers = pBufferFilteredIndex[i];
				filterParams[1].pName = "uncompactedDrawArgsRW";
				filterParams[1].mCount = NUM_CULLING_VIEWPORTS;
				filterParams[1].ppBuffers = pBufferUncompactedDrawArguments[i];
				filterParams[2].pName = "visibilityBufferConstants";
				filterParams[2].ppBuffers = &pBufferVisibilityBufferConstants[i];
				updateDescriptorSet(pRenderer, i * 3 + 1, pDescriptorSetTriangleFiltering[1], 3, filterParams);

				DescriptorData compactParams[4] = {};
				compactParams[0].pName = "indirectMaterialBuffer";
				compactParams[0].ppBuffers = &pBufferFilterIndirectMaterial[i];
				compactParams[1].pName = "indirectDrawArgsBufferAlpha";
				compactParams[1].mCount = NUM_CULLING_VIEWPORTS;
				compactParams[1].ppBuffers = pBufferFilteredIndirectDrawArguments[i][GEOMSET_ALPHATESTED];
				compactParams[2].pName = "indirectDrawArgsBufferNoAlpha";
				compactParams[2].mCount = NUM_CULLING_VIEWPORTS;
				compactParams[2].ppBuffers = pBufferFilteredIndirectDrawArguments[i][GEOMSET_OPAQUE];
				compactParams[3].pName = "uncompactedDrawArgs";
				compactParams[3].mCount = NUM_CULLING_VIEWPORTS;
				compactParams[3].ppBuffers = pBufferUncompactedDrawArguments[i];
				updateDescriptorSet(pRenderer, i * 3 + 2, pDescriptorSetTriangleFiltering[1], 4, compactParams);
			}
		}
		// VB Shade
		{
#if ENABLE_SDF_SHADOW_DOWNSAMPLE
			Texture* sdfShadowTexture = pRenderTargetUpSampleSDFShadow->pTexture;
#else
			Texture* sdfShadowTexture = pRenderTargetSDFMeshShadow->pTexture;
#endif
			Texture* esmShadowMap = pRenderTargetShadowMap->pTexture;

			eastl::vector<RenderTarget*>& indirectionTexMips =
				pASM->m_longRangeShadows->m_indirectionTexturesMips;

			eastl::vector<RenderTarget*>& prerenderIndirectionTexMips =
				pASM->m_longRangePreRender->m_indirectionTexturesMips;

			Texture* entireTextureList[] = {
				indirectionTexMips[0]->pTexture,
				indirectionTexMips[1]->pTexture,
				indirectionTexMips[2]->pTexture,
				indirectionTexMips[3]->pTexture,
				indirectionTexMips[4]->pTexture,
				prerenderIndirectionTexMips[0]->pTexture,
				prerenderIndirectionTexMips[1]->pTexture,
				prerenderIndirectionTexMips[2]->pTexture,
				prerenderIndirectionTexMips[3]->pTexture,
				prerenderIndirectionTexMips[4]->pTexture
			};

			DescriptorData vbShadeParams[15] = {};
			vbShadeParams[0].pName = "vbPassTexture";
			vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
			vbShadeParams[1].pName = "diffuseMaps";
			vbShadeParams[1].mCount = (uint32_t)gDiffuseMapsPacked.size();
			vbShadeParams[1].ppTextures = gDiffuseMapsPacked.data();
			vbShadeParams[2].pName = "normalMaps";
			vbShadeParams[2].mCount = (uint32_t)gNormalMapsPacked.size();
			vbShadeParams[2].ppTextures = gNormalMapsPacked.data();
			vbShadeParams[3].pName = "specularMaps";
			vbShadeParams[3].mCount = (uint32_t)gSpecularMapsPacked.size();
			vbShadeParams[3].ppTextures = gSpecularMapsPacked.data();
			vbShadeParams[4].pName = "vertexPos";
			vbShadeParams[4].ppBuffers = &pIndirectPosBuffer;
			vbShadeParams[5].pName = "vertexTexCoord";
			vbShadeParams[5].ppBuffers = &pIndirectTexCoordBuffer;
			vbShadeParams[6].pName = "vertexNormal";
			vbShadeParams[6].ppBuffers = &pIndirectNormalBuffer;
			vbShadeParams[7].pName = "vertexTangent";
			vbShadeParams[7].ppBuffers = &pIndirectTangentBuffer;
			vbShadeParams[8].pName = "meshConstantsBuffer";
			vbShadeParams[8].ppBuffers = &pBufferMeshConstants;
			vbShadeParams[9].pName = "DepthAtlasTexture";
			vbShadeParams[9].ppTextures = &pRenderTargetASMDepthAtlas->pTexture;
			vbShadeParams[10].pName = "IndexTexture";
			vbShadeParams[10].mCount = (gs_ASMMaxRefinement + 1) * 2;
			vbShadeParams[10].ppTextures = entireTextureList;
			vbShadeParams[11].pName = "DEMTexture";
			vbShadeParams[11].ppTextures = &pRenderTargetASMDEMAtlas->pTexture;
			vbShadeParams[12].pName = "PrerenderLodClampTexture";
			vbShadeParams[12].ppTextures = &pASM->m_longRangePreRender->m_lodClampTexture->pTexture;
			vbShadeParams[13].pName = "ESMShadowTexture";
			vbShadeParams[13].ppTextures = &esmShadowMap;
			vbShadeParams[14].pName = "SDFShadowTexture";
			vbShadeParams[14].ppTextures = &sdfShadowTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 15, vbShadeParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				Buffer* pIndirectBuffers[gNumGeomSets] = { NULL };
				for (uint32_t j = 0; j < gNumGeomSets; ++j)
					pIndirectBuffers[j] = pBufferFilteredIndirectDrawArguments[i][j][VIEW_CAMERA];

				DescriptorData vbShadeParams[15] = {};
				vbShadeParams[0].pName = "indirectDrawArgs";
				vbShadeParams[0].mCount = gNumGeomSets;
				vbShadeParams[0].ppBuffers = pIndirectBuffers;
				vbShadeParams[1].pName = "objectUniformBlock";
				vbShadeParams[1].ppBuffers = &pBufferMeshTransforms[0][i];
				vbShadeParams[2].pName = "indirectMaterialBuffer";
				vbShadeParams[2].ppBuffers = &pBufferFilterIndirectMaterial[i];
				vbShadeParams[3].pName = "filteredIndexBuffer";
				vbShadeParams[3].ppBuffers = &pBufferFilteredIndex[i][VIEW_CAMERA];
				vbShadeParams[4].pName = "cameraUniformBlock";
				vbShadeParams[4].ppBuffers = &pBufferCameraUniform[i];
				vbShadeParams[5].pName = "lightUniformBlock";
				vbShadeParams[5].ppBuffers = &pBufferLightUniform[i];
				vbShadeParams[6].pName = "ASMUniformBlock";
				vbShadeParams[6].ppBuffers = &pBufferASMDataUniform[i];
				vbShadeParams[7].pName = "renderSettingUniformBlock";
				vbShadeParams[7].ppBuffers = &pBufferRenderSettings[i];
				vbShadeParams[8].pName = "ESMInputConstants";
				vbShadeParams[8].ppBuffers = &pBufferESMUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetVBShade[1], 9, vbShadeParams);
			}
		}
		// Quad
		{
			DescriptorData descriptorData[2] = {};
			descriptorData[0].pName = "screenTexture";
			descriptorData[0].ppTextures = &pRenderTargetASMDepthAtlas->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetQuad[0], 1, descriptorData);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				descriptorData[0].pName = "UniformQuadData";
				descriptorData[0].ppBuffers = &pBufferQuadUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetQuad[1], 1, descriptorData);
			}
		}

		// Present Pass
		{
			DescriptorData params[1] = {};
			params[0].pName = "SourceTexture";
			params[0].ppTextures = &pRenderTargetIntermediate->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetPresentPass, 1, params);
			params[0].ppTextures = &pRenderTargetSDFMeshVisualization->pTexture;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetPresentPass, 1, params);
		}
		// ASM Atlas to Color
		{
			DescriptorData params[1] = {};
			params[0].pName = "DepthPassTexture";
			params[0].ppTextures = &pRenderTargetASMDEMAtlas->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetASMDEMAtlasToColor[0], 1, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "PackedAtlasQuads_CB";
				params[0].ppBuffers = &pBufferASMAtlasToColorPackedQuadsUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetASMDEMAtlasToColor[1], 1, params);
			}
		}
		// ASM Color to Atlas
		{
			DescriptorData colorToAtlasParams[2] = {};
			colorToAtlasParams[0].pName = "DepthPassTexture";
			colorToAtlasParams[0].ppTextures = &pRenderTargetASMColorPass->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetASMDEMColorToAtlas[0], 1, colorToAtlasParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				colorToAtlasParams[0].pName = "PackedAtlasQuads_CB";
				colorToAtlasParams[0].ppBuffers = &pBufferASMColorToAtlasPackedQuadsUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetASMDEMColorToAtlas[1], 1, colorToAtlasParams);
			}
		}
		// ASM Depth, VB Pass
		{
			DescriptorData depthPassParams[1] = {};
			depthPassParams[0].pName = "diffuseMaps";
			depthPassParams[0].mCount = (uint32_t)gDiffuseMaps.size();
			depthPassParams[0].ppTextures = gDiffuseMaps.data();
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 1, depthPassParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData objectParams[1] = {};
				objectParams[0].pName = "indirectMaterialBuffer";
				objectParams[0].ppBuffers = &pBufferFilterIndirectMaterial[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetVBPass[1], 1, objectParams);

				objectParams[0].pName = "objectUniformBlock";
				objectParams[0].ppBuffers = &pBufferMeshTransforms[0][i];
				updateDescriptorSet(pRenderer, i * 3 + 0, pDescriptorSetVBPass[2], 1, objectParams);

				objectParams[0].pName = "objectUniformBlock";
				objectParams[0].ppBuffers = &pBufferMeshShadowProjectionTransforms[0][i];
				updateDescriptorSet(pRenderer, i * 3 + 1, pDescriptorSetVBPass[2], 1, objectParams);

				objectParams[0].pName = "objectUniformBlock";
				objectParams[0].ppBuffers = &pBufferMeshShadowProjectionTransforms[0][i];
				updateDescriptorSet(pRenderer, i * 3 + 2, pDescriptorSetVBPass[2], 1, objectParams);
			}
		}
		// ASM Fill LOD Clamp
		{
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData params[1] = {};
				params[0].pName = "PackedAtlasQuads_CB";
				params[0].ppBuffers = &pBufferASMLodClampPackedQuadsUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetASMFillLodClamp, 1, params);
			}
		}
		// ASM Copy Depth
		{
			DescriptorData params[1] = {};
			params[0].pName = "DepthPassTexture";
			params[0].ppTextures = &pRenderTargetASMDepthPass->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetASMCopyDepthQuadPass[0], 1, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "AtlasQuads_CB";
				params[0].ppBuffers = &pBufferASMAtlasQuadsUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetASMCopyDepthQuadPass[1], 1, params);
			}
		}
		// Copy DEM
		{
			DescriptorData params[1] = {};
			params[0].pName = "DepthPassTexture";
			params[0].ppTextures = &pRenderTargetASMDepthAtlas->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetASMCopyDEM[0], 1, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "PackedAtlasQuads_CB";
				params[0].ppBuffers = &pBufferASMCopyDEMPackedQuadsUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetASMCopyDEM[1], 1, params);
			}
		}
		// Skybox
		{
			DescriptorData skyParams[2] = {};
			skyParams[0].pName = "skyboxTex";
			skyParams[0].ppTextures = &pTextureSkybox;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				skyParams[0].pName = "UniformCameraSky";
				skyParams[0].ppBuffers = &pBufferSkyboxUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyParams);
			}
		}
		// Update SDF
		{
			DescriptorData params[2] = {};
			params[0].pName = "SDFVolumeTextureAtlas";
			params[0].ppTextures = &pTextureSDFVolumeAtlas;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetUpdateSDFVolumeTextureAtlas[0], 1, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "SDFVolumeDataBuffer";
				params[0].ppBuffers = &pBufferSDFVolumeData[i];
				params[1].pName = "UpdateSDFVolumeTextureAtlasCB";
				params[1].ppBuffers = &pBufferUpdateSDFVolumeTextureAtlasConstants[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetUpdateSDFVolumeTextureAtlas[1], 2, params);
			}
		}
		// SDF Mesh Visualization
		{
			DescriptorData params[3] = {};
			params[0].pName = "OutTexture";
			params[0].ppTextures = &pRenderTargetSDFMeshVisualization->pTexture;
			params[1].pName = "SDFVolumeTextureAtlas";
			params[1].ppTextures = &pTextureSDFVolumeAtlas;
			params[2].pName = "DepthTexture";
			params[2].ppTextures = &pRenderTargetDepth->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetSDFMeshVisualization[0], 3, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cameraUniformBlock";
				params[0].ppBuffers = &pBufferCameraUniform[i];
				params[1].pName = "meshSDFUniformBlock";
				params[1].ppBuffers = &pBufferMeshSDFConstants[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetSDFMeshVisualization[1], 2, params);
			}
		}
		// SDF Mesh Shadow
		{
			DescriptorData params[3] = {};
			params[0].pName = "OutTexture";
			params[0].ppTextures = &pRenderTargetSDFMeshShadow->pTexture;
			params[1].pName = "SDFVolumeTextureAtlas";
			params[1].ppTextures = &pTextureSDFVolumeAtlas;
			params[2].pName = "DepthTexture";
			params[2].ppTextures = &pRenderTargetDepth->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetSDFMeshShadow[0], 3, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cameraUniformBlock";
				params[0].ppBuffers = &pBufferCameraUniform[i];
				params[1].pName = "meshSDFUniformBlock";
				params[1].ppBuffers = &pBufferMeshSDFConstants[i];
				params[2].pName = "lightUniformBlock";
				params[2].ppBuffers = &pBufferLightUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetSDFMeshShadow[1], 3, params);
			}
		}
		// Upsample SDF
		{
			DescriptorData params[3] = {};
			params[0].pName = "SDFShadowTexture";
			params[0].ppTextures = &pRenderTargetSDFMeshShadow->pTexture;
			params[1].pName = "DepthTexture";
			params[1].ppTextures = &pRenderTargetDepth->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetUpsampleSDFShadow[0], 2, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cameraUniformBlock";
				params[0].ppBuffers = &pBufferCameraUniform[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetUpsampleSDFShadow[1], 1, params);
			}
		}
		// ASM Fill Indirection
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "PackedAtlasQuads_CB";
			params[0].ppBuffers = &pBufferASMClearIndirectionQuadsUniform[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetASMFillIndirection[0], 1, params);

			for (uint32_t j = 0; j < gs_ASMMaxRefinement + 1; ++j)
			{
				params[0].ppBuffers = &pBufferASMPackedIndirectionQuadsUniform[j][i];
				updateDescriptorSet(pRenderer, i * (gs_ASMMaxRefinement + 1) + j, pDescriptorSetASMFillIndirection[1], 1, params);

				params[0].ppBuffers = &pBufferASMPackedPrerenderIndirectionQuadsUniform[j][i];
				updateDescriptorSet(pRenderer, i * (gs_ASMMaxRefinement + 1) + j, pDescriptorSetASMFillIndirection[2], 1, params);
			}
		}
	}
};

void GuiController::updateDynamicUI()
{
	if (gRenderSettings.mShadowType != GuiController::currentlyShadowType)
	{
		if (GuiController::currentlyShadowType == SHADOW_TYPE_ESM)
			GuiController::esmDynamicWidgets.HideWidgets(pGuiWindow);
		else if (GuiController::currentlyShadowType == SHADOW_TYPE_ASM)
			GuiController::asmDynamicWidgets.HideWidgets(pGuiWindow);
		else if (GuiController::currentlyShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
			GuiController::bakedSDFDynamicWidgets.HideWidgets(pGuiWindow);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			GuiController::esmDynamicWidgets.ShowWidgets(pGuiWindow);
		}
		
		
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_ASM)
		{
			GuiController::asmDynamicWidgets.ShowWidgets(pGuiWindow);
			LightShadowPlayground::refreshASM();
			LightShadowPlayground::resetLightDir();
		}
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			GuiController::bakedSDFDynamicWidgets.ShowWidgets(pGuiWindow);
		}

		GuiController::currentlyShadowType = (ShadowType)gRenderSettings.mShadowType;
	}

	static bool wasMicroProfileActivated = gAppSettings.mMicroProfiler;
	if (wasMicroProfileActivated != gAppSettings.mMicroProfiler)
	{
		wasMicroProfileActivated = gAppSettings.mMicroProfiler;

		// ProfileSetDisplayMode()
		// TODO: need to change this better way 

		Profile& S = *ProfileGet();
		int32_t nValue = wasMicroProfileActivated ? 1 : 0;
		nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
		S.nDisplay = nValue;

		//ActivateMicroProfile(&gAppUI, gAppSettings.mActivateMicroProfiler);
		//ProfileSetDisplayMode(P_DRAW_BARS);
	}
}



void GuiController::addGui()
{
	const float lightPosBound = 300.0f;
	const float minusXPosBias = -150.f;

	static const char* shadowTypeNames[] = {
		"(ESM) Exponential Shadow Mapping",  "(ASM) Adaptive Shadow Map", "(SDF) Signed Distance Field Mesh Shadow",
		NULL    //needed for unix
	};
	static const uint32_t shadowTypeValues[] = {
		SHADOW_TYPE_ESM, SHADOW_TYPE_ASM, SHADOW_TYPE_MESH_BAKED_SDF,
		0    //needed for unix
	};
	
	SliderFloat2Widget sunX("Sun Control", &gLightCpuSettings.mSunControl,
		float2(-PI), float2(PI), float2(0.00001f));
	SliderFloatWidget esmControlUI("ESM Control", &gEsmCpuSettings.mEsmControl, 1.f, 300.f);

	CheckboxWidget microprofile("Activate Microprofile", &gAppSettings.mMicroProfiler);
	pGuiWindow->AddWidget(microprofile);
	pGuiWindow->AddWidget(CheckboxWidget("Hold triangles", &gAppSettings.mHoldFilteredTriangles));
	pGuiWindow->AddWidget(CheckboxWidget("Async Compute", &gAppSettings.mAsyncCompute));
#if !defined(TARGET_IOS)
	CheckboxWidget vsyncProp("Toggle VSync", &gAppSettings.mToggleVsync);
	pGuiWindow->AddWidget(vsyncProp);
#endif

	pGuiWindow->AddWidget(DropdownWidget("Shadow Type", &gRenderSettings.mShadowType, shadowTypeNames, shadowTypeValues, 3));
	//pGuiWindow->AddWidget(sunX);

	{
		
		GuiController::esmDynamicWidgets.AddWidget(sunX);
		GuiController::esmDynamicWidgets.AddWidget(esmControlUI);
	}
	
	{
		sunX.pOnActive = LightShadowPlayground::refreshASM;
		GuiController::asmDynamicWidgets.AddWidget(sunX);
		ButtonWidget button("Refresh Cache");
		button.pOnEdited = LightShadowPlayground::refreshASM;

		GuiController::asmDynamicWidgets.AddWidget(button);
		GuiController::asmDynamicWidgets.AddWidget(CheckboxWidget("Sun can move", &gASMCpuSettings.mSunCanMove));
		GuiController::asmDynamicWidgets.AddWidget(CheckboxWidget("Parallax corrected", &gASMCpuSettings.mEnableParallax));
		
		CheckboxWidget debugTexturesWidgets("Display ASM Debug Textures",
			&gASMCpuSettings.mShowDebugTextures);
		debugTexturesWidgets.pOnDeactivatedAfterEdit = SetupASMDebugTextures;
		

		ButtonWidget button_reset("Reset Light Dir");
		button_reset.pOnEdited = LightShadowPlayground::resetLightDir;
		GuiController::asmDynamicWidgets.AddWidget(button_reset);
		GuiController::asmDynamicWidgets.AddWidget(SliderFloatWidget("Penumbra Size", &gASMCpuSettings.mPenumbraSize, 1.f, 150.f));
		GuiController::asmDynamicWidgets.AddWidget(SliderFloatWidget("Parallax Step Distance", &gASMCpuSettings.mParallaxStepDistance, 1.f, 100.f));
		GuiController::asmDynamicWidgets.AddWidget(SliderFloatWidget("Parallax Step Z Bias", &gASMCpuSettings.mParallaxStepBias, 1.f, 200.f));
		GuiController::asmDynamicWidgets.AddWidget(debugTexturesWidgets);
	
	}
	{
		GuiController::bakedSDFDynamicWidgets.AddWidget(sunX);
		SeparatorWidget separatorWidget;
		GuiController::bakedSDFDynamicWidgets.AddWidget(separatorWidget);
		ButtonWidget generateSDFButtonWidget("Generate Missing SDF");
		generateSDFButtonWidget.pOnEdited = LightShadowPlayground::checkForMissingSDFData;
		GuiController::bakedSDFDynamicWidgets.AddWidget(generateSDFButtonWidget);
		GuiController::bakedSDFDynamicWidgets.AddWidget(CheckboxWidget("Automatic Sun Movement", &gLightCpuSettings.mAutomaticSunMovement));
		GuiController::bakedSDFDynamicWidgets.AddWidget(SliderFloatWidget("Light Source Angle", &gLightCpuSettings.mSourceAngle, 0.001f, 4.f));
		GuiController::bakedSDFDynamicWidgets.AddWidget(CheckboxWidget("Display baked SDF mesh data on the screen",
			&gBakedSDFMeshSettings.mDrawSDFMeshVisualization));
	}

	if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_ESM;
		GuiController::esmDynamicWidgets.ShowWidgets(pGuiWindow);
	}	
	else if (gRenderSettings.mShadowType == SHADOW_TYPE_ASM)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_ASM;
		GuiController::asmDynamicWidgets.ShowWidgets(pGuiWindow);
	}
	else if (gRenderSettings.mShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_MESH_BAKED_SDF;
		GuiController::bakedSDFDynamicWidgets.ShowWidgets(pGuiWindow);
	}
}

void GuiController::removeGui()
{
	esmDynamicWidgets.Destroy();
	sdfDynamicWidgets.Destroy();
	asmDynamicWidgets.Destroy();
	bakedSDFDynamicWidgets.Destroy();
}

DEFINE_APPLICATION_MAIN(LightShadowPlayground)
