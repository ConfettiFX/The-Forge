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

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../../Common_3/OS/Core/ThreadSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Core/RingBuffer.h"
//GPU Profiler
#include "../../../../Common_3/Renderer/GpuProfiler.h"


#include "../../../../Common_3/ThirdParty/OpenSource/MicroProfile/ProfilerBase.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//input
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

#include "ASM.h"
#include "Constant.h"
#include "ASMTileCache.h"
#include "ASMFrustum.h"
#include "Geometry.h"
#include "SDFVolumeTextureAtlas.h"


#include "../../../../Common_3/OS/Interfaces/IMemory.h"


#define SAN_MIGUEL_ORIGINAL_SCALE 50.0f
#define SAN_MIGUEL_ORIGINAL_OFFSETX -20.f
//#define SAN_MIGUEL_ORIGINAL_SCALE 10.f
//#define SAN_MIGUEL_ORIGINAL_OFFSETX 150.f

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
	bool mActivateMicroProfiler = false;
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
	
	uint32_t mValidNumCull = 0;


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
	TextureLoadDesc desc = pTaskData->mDesc;
	desc.pFilename = pTaskData->mNames[i];
	desc.ppTexture = &pTaskData->textures[i];
	addResource(&desc, true);
}


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

struct UniformDataSkybox
{
	mat4 mProjectView;
	vec3 mCamPos;
};

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


/************************************************************************/
// ASM fill indirection Shader Pack
/************************************************************************/

Shader* pShaderASMFillIndirection = NULL;
Pipeline* pPipelineASMFillIndirection = NULL;
RootSignature* pRootSignatureASMFillIndirection = NULL;




/************************************************************************/
// ASM fill lod clamp Pack
/************************************************************************/

//Reuse pShaderASMFillIndirection since they pretty much has the same shader
Pipeline* pPipelineASMFillLodClamp = NULL;
RootSignature* pRootSignatureASMFillLodClamp = NULL;

/************************************************************************/
// ASM Copy DEM Shader Pack
/************************************************************************/

Shader* pShaderASMCopyDEM = NULL;
Pipeline* pPipelineASMCopyDEM = NULL;
RootSignature* pRootSignatureASMCopyDEM = NULL;


/************************************************************************/
// ASM generate DEM Shader Pack
/************************************************************************/

Shader* pShaderASMGenerateDEM = NULL;
Pipeline* pPipelineASMDEMAtlasToColor = NULL;
Pipeline* pPipelineASMDEMColorToAtlas = NULL;
RootSignature* pRootSignatureASMDEMAtlasToColor = NULL;
RootSignature* pRootSignatureASMDEMColorToAtlas = NULL;


/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVBBufferPass[gNumGeomSets] = {};
Pipeline*         pPipelineVBBufferPass[gNumGeomSets] = {};
RootSignature*    pRootSignatureVBPass = NULL;
CommandSignature* pCmdSignatureVBPass = NULL;
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*           pShaderVBShade = NULL;
Pipeline*         pPipelineVBShadeSrgb = NULL;
RootSignature*    pRootSignatureVBShade = NULL;

/************************************************************************/
// SDF draw update volume texture atlas pipeline
/************************************************************************/
Shader* pShaderUpdateSDFVolumeTextureAtlas = NULL;
Pipeline* pPipelineUpdateSDFVolumeTextureAtlas = NULL;
RootSignature* pRootSignatureUpdateSDFVolumeTextureAtlas = NULL;



/************************************************************************/
// SDF mesh visualization pipeline
/************************************************************************/

Shader* pShaderSDFMeshVisualization = NULL;
Pipeline* pPipelineSDFMeshVisualization = NULL;
RootSignature* pRootSignatureSDFMeshVisualization = NULL;



/************************************************************************/
// SDF baked mesh shadow pipeline
/************************************************************************/

Shader* pShaderSDFMeshShadow = NULL;
Pipeline* pPipelineSDFMeshShadow = NULL;
RootSignature* pRootSignatureSDFMeshShadow = NULL;



/************************************************************************/
// SDF upsample shadow texture pipeline
/************************************************************************/

Shader* pShaderUpsampleSDFShadow = NULL;
Pipeline* pPipelineUpsampleSDFShadow = NULL;
RootSignature* pRootSignatureUpsampleSDFShadow = NULL;


/************************************************************************/
// Display quad texture shader pack
/************************************************************************/

Shader* pShaderQuad = NULL;
Pipeline* pPipelineQuad = NULL;
RootSignature* pRootSignatureQuad = NULL;


/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*           pShaderPresentPass = NULL;
Pipeline*         pPipelinePresentPass = NULL;
RootSignature*    pRootSignaturePresentPass = NULL;

/************************************************************************/
// Descriptor Binder
/************************************************************************/
DescriptorBinder* pDescriptorBinder = NULL;



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



AlphaTestedImageMaps gAlphaTestedImageMaps;
AlphaTestedMaterialMaps gAlphaTestedMaterialMaps;


ASMCpuSettings gASMCpuSettings;

/************************************************************************/

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

ASM* pASM;


SDFVolumeTextureAtlas* pSDFVolumeTextureAtlas = NULL;

UpdateSDFVolumeTextureAtlasConstants gUpdateSDFVolumeTextureAtlasConstants = {};
MeshSDFConstants gMeshSDFConstants = {};

BakedSDFVolumeInstances gSDFVolumeInstances;



bool gBufferUpdateSDFMeshConstantFlags[3] = { true, true, true };


// Constants
uint32_t					gFrameIndex = 0;
GpuProfiler*				pGpuProfilerGraphics = NULL;
GpuProfiler*				pGpuProfilerCompute = NULL;

RenderSettingsUniformData gRenderSettings;

MeshInfoUniformBlock   gMeshInfoUniformData[MESH_COUNT];

PerFrameData gPerFrameData = {};

/************************************************************************/
// Triangle filtering data
/************************************************************************/

const uint32_t gSmallBatchChunkCount = max(1U, 512U / CLUSTER_SIZE) * 16U;
FilterBatchChunk* pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = {NULL};
GPURingBuffer* pBufferFilterBatchData[gImageCount] = { NULL };


///


MeshInfoUniformBlock   gMeshASMProjectionInfoUniformData[MESH_COUNT];

ASMUniformBlock gAsmModelUniformBlockData = {};

LightUniformBlock      gLightUniformData;
CameraUniform          gCameraUniformData;
ESMInputConstants      gESMUniformData;
//TODO remove this
QuadDataUniform gQuadUniformData;
MeshInfoStruct gMeshInfoData[MESH_COUNT] = {};
VisibilityBufferConstants gVisibilityBufferConstants = {};


vec3 gObjectsCenter = { SAN_MIGUEL_OFFSETX, 0, 0 };

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

Scene*        pScene = NULL;

uint64_t gFrameCount = 0;
/// UI
UIApp         gAppUI;
GuiComponent* pGuiWindow = NULL;
GuiComponent* pUIASMDebugTexturesWindow = NULL;
GuiComponent* pLoadingGui = NULL;
TextDrawDesc  gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

FileSystem gFileSystem;
Log gLogManager;


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

const char* gSDFModelNames[3] = { "SanMiguel_Opaque", "SanMiguel_AlphaTested", "SanMiguel_Flags" };
SDFMesh        gSDFMeshes[3] ={};
GenerateMissingSDFTaskData gGenerateMissingSDFTask[3] = {};
size_t gSDFProgressValue = 0;

const char* pszBases[FSR_Count] = {
	"../../../src/09_LightShadowPlayground/",    // FSR_BinShaders
	"../../../src/09_LightShadowPlayground/",    // FSR_SrcShaders
	"../../../../../Art/SanMiguel_3/Textures/",    // FSR_Textures
	"../../../../../Art/SanMiguel_3/Meshes/",      // FSR_Meshes
	"../../../UnitTestResources/",               // FSR_Builtin_Fonts
	"../../../src/09_LightShadowPlayground/",    // FSR_GpuConfig
	"",                                          // FSR_Animation
	"",                                          // FSR_Audio
	gGeneratedSDFBinaryDir.c_str(),            // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",         // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",           // FSR_MIDDLEWARE_UI
};


#ifdef _DURANGO
#include "../../../../Xbox/CommonXBOXOne_3/Renderer/Direct3D12/Direct3D12X.h"
#endif

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
	gMeshInfoData[0].mOffsetTranslation = float3(-SAN_MIGUEL_ORIGINAL_OFFSETX, 0.f, 0.f);
	gMeshInfoData[0].mTranslationMat = mat4::translation(vec3(gMeshInfoData[0].mTranslation.x, 
		gMeshInfoData[0].mTranslation.y, gMeshInfoData[0].mTranslation.z));
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
struct GuiController
{
	static void addGui();
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
		loadSDFMeshAlphaTested(pThreadSystem, gSDFModelNames[1], &gSDFMeshes[1], MESH_SCALE,
			SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gAlphaTestedImageMaps,
			gAlphaTestedMaterialMaps, gSDFVolumeInstances);

		loadSDFMesh(pThreadSystem, gSDFModelNames[0], &gSDFMeshes[0], MESH_SCALE, SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gSDFVolumeInstances);
		loadSDFMesh(pThreadSystem, gSDFModelNames[2], &gSDFMeshes[2], MESH_SCALE, SAN_MIGUEL_OFFSETX, ENABLE_SDF_MESH_GENERATION, gSDFVolumeInstances);

		gGenerateMissingSDFTask[0] = { pThreadSystem, &gSDFMeshes[0], &gSDFVolumeInstances };
		gGenerateMissingSDFTask[1] = { pThreadSystem, &gSDFMeshes[1], &gSDFVolumeInstances };
		gGenerateMissingSDFTask[2] = { pThreadSystem, &gSDFMeshes[2], &gSDFVolumeInstances };
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
		for (int i = 0; i < 3; ++i)
		{
			gSDFProgressValue += gSDFMeshes[i].mTotalGeneratedSDFMeshes;
		}
	}

	static uint32_t getMaxSDFMeshesProgress()
	{
		uint32_t max = 0;
		for (int i = 0; i < 3; ++i)
		{
			max += gSDFMeshes[i].mTotalSDFMeshes;
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
		initThreadSystem(&pThreadSystem);

		// Overwrite rootpath is required because Textures and meshes are not in /Textures and /Meshes.
		// We need to set the modified root path so that filesystem can find the meshes and textures.
		FileSystem::SetRootPath(FSRoot::FSR_Meshes, "/");
		FileSystem::SetRootPath(FSRoot::FSR_Textures, "/");
#ifndef _DURANGO
		FileSystem::CreateDir(gGeneratedSDFBinaryDir);
#endif

		initAlphaTestedImageMaps(gAlphaTestedImageMaps);
		initAlphaTestedMaterialTexturesMaps(gAlphaTestedMaterialMaps);

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

		initProfiler(pRenderer);
		profileRegisterInput();
			   		

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

		

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif
		eastl::string str_formatter = "";

	
		
		
		ShaderLoadDesc indirectDepthPassShaderDesc = {};
		indirectDepthPassShaderDesc.mStages[0] = {
			"meshDepthPass.vert",  NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc indirectAlphaDepthPassShaderDesc = {};
		indirectAlphaDepthPassShaderDesc.mStages[0] = {
			"meshDepthPassAlpha.vert", NULL, 0, FSR_SrcShaders };
		indirectAlphaDepthPassShaderDesc.mStages[1] = {
			"meshDepthPassAlpha.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc ASMCopyDepthQuadsShaderDesc = {};
		ASMCopyDepthQuadsShaderDesc.mStages[0] = {"copyDepthQuads.vert", NULL, 0, FSR_SrcShaders};
		ASMCopyDepthQuadsShaderDesc.mStages[1] = { "copyDepthQuads.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc quadShaderDesc = {};
		quadShaderDesc.mStages[0] = { "quad.vert", NULL, 0, FSR_SrcShaders };
		quadShaderDesc.mStages[1] = { "quad.frag", NULL, 0, FSR_SrcShaders };


		ShaderLoadDesc ASMFillIndirectionShaderDesc = {};
		ASMFillIndirectionShaderDesc.mStages[0] = { "fill_Indirection.vert", 
			NULL, 0, FSR_SrcShaders };
		ASMFillIndirectionShaderDesc.mStages[1] = { "fill_Indirection.frag",
			NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc ASMCopyDEMQuadsShaderDesc = {};
		ASMCopyDEMQuadsShaderDesc.mStages[0] = { "copyDEMQuads.vert", NULL, 0, FSR_SrcShaders };
		ASMCopyDEMQuadsShaderDesc.mStages[1] = { "copyDEMQuads.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &ASMCopyDEMQuadsShaderDesc, &pShaderASMCopyDEM);
		

		ShaderLoadDesc ASMGenerateDEMShaderDesc = {};
		ASMGenerateDEMShaderDesc.mStages[0] = { "generateAsmDEM.vert", NULL, 0, FSR_SrcShaders };
		ASMGenerateDEMShaderDesc.mStages[1] = { "generateAsmDEM.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &ASMGenerateDEMShaderDesc, &pShaderASMGenerateDEM);


		


		ShaderLoadDesc visibilityBufferPassShaderDesc = {};
		visibilityBufferPassShaderDesc.mStages[0] = { "visibilityBufferPass.vert", NULL, 0, FSR_SrcShaders };
		visibilityBufferPassShaderDesc.mStages[1] = { "visibilityBufferPass.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &visibilityBufferPassShaderDesc, &pShaderVBBufferPass[GEOMSET_OPAQUE]);

		ShaderLoadDesc visibilityBufferPassAlphaShaderDesc = {};
		visibilityBufferPassAlphaShaderDesc.mStages[0] = { "visibilityBufferPassAlpha.vert", NULL, 0, FSR_SrcShaders };
		visibilityBufferPassAlphaShaderDesc.mStages[1] = { "visibilityBufferPassAlpha.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &visibilityBufferPassAlphaShaderDesc, &pShaderVBBufferPass[GEOMSET_ALPHATESTED]);
	
		ShaderLoadDesc clearBuffersShaderDesc = {};
		clearBuffersShaderDesc.mStages[0] = { "clearVisibilityBuffers.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &clearBuffersShaderDesc, &pShaderClearBuffers);


		ShaderLoadDesc triangleFilteringShaderDesc = {};
		triangleFilteringShaderDesc.mStages[0] = { "triangleFiltering.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &triangleFilteringShaderDesc, &pShaderTriangleFiltering);

		ShaderLoadDesc batchCompactionShaderDesc = {};
		batchCompactionShaderDesc.mStages[0] = { "batchCompaction.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &batchCompactionShaderDesc, &pShaderBatchCompaction);

		ShaderLoadDesc updateSDFVolumeTextureAtlasShaderDesc = {};
		updateSDFVolumeTextureAtlasShaderDesc.mStages[0] = { "updateRegion3DTexture.comp", NULL, 0, FSR_SrcShaders };
		

		addShader(pRenderer, &updateSDFVolumeTextureAtlasShaderDesc, &pShaderUpdateSDFVolumeTextureAtlas);

		ShaderLoadDesc meshSDFVisualizationShaderDesc = {};
		meshSDFVisualizationShaderDesc.mStages[0] = { "visualizeSDFMesh.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &meshSDFVisualizationShaderDesc, &pShaderSDFMeshVisualization);

		ShaderLoadDesc sdfShadowMeshShaderDesc = {};
		sdfShadowMeshShaderDesc.mStages[0] = { "bakedSDFMeshShadow.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &sdfShadowMeshShaderDesc, &pShaderSDFMeshShadow);

		ShaderLoadDesc upSampleSDFShadowShaderDesc = {};
		upSampleSDFShadowShaderDesc.mStages[0] = { "upsampleSDFShadow.vert", NULL, 0, FSR_SrcShaders };
		upSampleSDFShadowShaderDesc.mStages[1] = { "upsampleSDFShadow.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &upSampleSDFShadowShaderDesc, &pShaderUpsampleSDFShadow);


		ShaderLoadDesc presentShaderDesc = {};
		presentShaderDesc.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		presentShaderDesc.mStages[1] = { "display.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);


		ShaderLoadDesc visibilityBufferShadeShaderDesc = {};
		visibilityBufferShadeShaderDesc.mStages[0] = { "visibilityBufferShade.vert", NULL, 0, FSR_SrcShaders };
		visibilityBufferShadeShaderDesc.mStages[1] = { "visibilityBufferShade.frag", NULL, 0, FSR_SrcShaders };


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
		clampMiplessSamplerDesc.mMipLosBias = 0.0f;
		clampMiplessSamplerDesc.mMaxAnisotropy = 0.0f;
		addSampler(pRenderer, &clampMiplessSamplerDesc, &pSamplerMiplessSampler);


		SamplerDesc samplerTrilinearAnisoDesc = {};
		samplerTrilinearAnisoDesc.mAddressU = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressV = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressW = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mMinFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMagFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		samplerTrilinearAnisoDesc.mMipLosBias = 0.0f;
		samplerTrilinearAnisoDesc.mMaxAnisotropy = 8.0f;
		addSampler(pRenderer, &samplerTrilinearAnisoDesc, &pSamplerTrilinearAniso);

		SamplerDesc miplessNearSamplerDesc = {};
		miplessNearSamplerDesc.mMinFilter = FILTER_NEAREST;
		miplessNearSamplerDesc.mMagFilter = FILTER_NEAREST;
		miplessNearSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		miplessNearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessNearSamplerDesc.mMipLosBias = 0.f;
		miplessNearSamplerDesc.mMaxAnisotropy = 0.f;
		addSampler(pRenderer, &miplessNearSamplerDesc, &pSamplerMiplessNear);


		SamplerDesc miplessLinearSamplerDesc = {};
		miplessLinearSamplerDesc.mMinFilter = FILTER_LINEAR;
		miplessLinearSamplerDesc.mMagFilter = FILTER_LINEAR;
		miplessLinearSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		miplessLinearSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		miplessLinearSamplerDesc.mMipLosBias = 0.f;
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
		miplessClampToBorderNearSamplerDesc.mMipLosBias = 0.f;
		addSampler(pRenderer, &miplessClampToBorderNearSamplerDesc, &pSamplerMiplessClampToBorderNear);


		SamplerDesc billinearRepeatDesc = {};
		billinearRepeatDesc.mMinFilter = FILTER_LINEAR;
		billinearRepeatDesc.mMagFilter = FILTER_LINEAR;
		billinearRepeatDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		billinearRepeatDesc.mAddressU = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mAddressV = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mAddressW = ADDRESS_MODE_REPEAT;
		billinearRepeatDesc.mMaxAnisotropy = 0.f;
		billinearRepeatDesc.mMipLosBias = 0.f;
		addSampler(pRenderer, &billinearRepeatDesc, &pSamplerLinearRepeat);

		
		if (!gAppUI.Init(pRenderer))
			return false;
		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		/************************************************************************/
		// Load resources for skybox
		/************************************************************************/
		addThreadSystemTask(pThreadSystem, memberTaskFunc0<LightShadowPlayground, &LightShadowPlayground::LoadSkybox>, this);

		initSDFMeshes();

		eastl::string sceneFullPath = FileSystem::FixPath(gSceneName, FSRoot::FSR_Meshes);
		//pScene = loadScene(sceneFullPath.c_str(), MESH_SCALE, SAN_MIGUEL_OFFSETX, 0.0f, 0.0f);
		pScene = loadScene(sceneFullPath.c_str(), SAN_MIGUEL_ORIGINAL_SCALE, SAN_MIGUEL_ORIGINAL_OFFSETX, 0.0f, 0.0f);
		Scene& sanMiguelMeshes = *pScene;
		
		
		
		gDiffuseMaps.resize(sanMiguelMeshes.numMaterials);
		gNormalMaps.resize(sanMiguelMeshes.numMaterials);
		gSpecularMaps.resize(sanMiguelMeshes.numMaterials);


		TextureLoadDesc loadTextureDesc = {};
		loadTextureDesc.mRoot = FSR_Textures;
		loadTextureDesc.mSrgb = false;


		TextureLoadTaskData loadTextureDiffuseData = {};
		loadTextureDiffuseData.textures = gDiffuseMaps.data();
		loadTextureDiffuseData.mNames = (const char **)(sanMiguelMeshes.textures);
		loadTextureDiffuseData.mDesc = loadTextureDesc;

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTextureDiffuseData, sanMiguelMeshes.numMaterials);

		TextureLoadTaskData loadTextureNormalMap = {};
		loadTextureNormalMap.textures = gNormalMaps.data();
		loadTextureNormalMap.mNames = (const char **)(sanMiguelMeshes.normalMaps);
		loadTextureNormalMap.mDesc = loadTextureDesc;

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTextureNormalMap, sanMiguelMeshes.numMaterials);

		TextureLoadTaskData loadTexturesSpecularMap = {};
		loadTexturesSpecularMap.mDesc = loadTextureDesc;
		loadTexturesSpecularMap.mNames = (const char **)(sanMiguelMeshes.specularMaps);
		loadTexturesSpecularMap.textures = gSpecularMaps.data();

		addThreadSystemRangeTask(pThreadSystem, loadTexturesTask,
			&loadTexturesSpecularMap, sanMiguelMeshes.numMaterials);

		// Cluster creation
		/************************************************************************/
		// Calculate clusters


		for (uint32_t i = 0; i < sanMiguelMeshes.numMeshes; ++i)
		{
			//MeshInstance*   subMesh = &sanMiguelMeshes.meshes[i];
			MeshIn*   subMesh = &sanMiguelMeshes.meshes[i];
			Material* material = sanMiguelMeshes.materials + subMesh->materialId;
			createClusters(material->twoSided, &sanMiguelMeshes, subMesh);
		}


		MeshConstants* meshConstants =
			(MeshConstants*)conf_malloc(sanMiguelMeshes.numMeshes * sizeof(MeshConstants));


		for (uint32_t i = 0; i < sanMiguelMeshes.numMeshes; ++i)
		{
			meshConstants[i].faceCount = sanMiguelMeshes.meshes[i].indexCount / 3;
			meshConstants[i].indexOffset = sanMiguelMeshes.meshes[i].startIndex;
			meshConstants[i].materialID = sanMiguelMeshes.meshes[i].materialId;
			meshConstants[i].twoSided =
				sanMiguelMeshes.materials[sanMiguelMeshes.meshes[i].materialId].twoSided ? 1 : 0;
		}

		BufferLoadDesc meshConstantDesc = {};
		meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		meshConstantDesc.mDesc.mElementCount = sanMiguelMeshes.numMeshes;
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
		vbShadeRootDesc.mMaxBindlessTextures = sanMiguelMeshes.numMaterials;
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
		vbPassRootDesc.mMaxBindlessTextures = sanMiguelMeshes.numMaterials;
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


		//RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
		RootSignatureDesc triangleFilteringRootDesc = { pCullingShaders, 3 };


#if defined(VULKAN)
		const char* pBatchBufferName = "batchData";
		triangleFilteringRootDesc.mDynamicUniformBufferCount = 1;
		triangleFilteringRootDesc.ppDynamicUniformBufferNames = &pBatchBufferName;
#endif


		RootSignatureDesc batchCompactionRootDesc = {&pShaderBatchCompaction, 1};


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

		DescriptorBinderDesc descriptorBinderDesc[] = {
			{ pRootSignatureSkybox, 0, 1 },
			//{ pRootSignatureASMIndirectDepthPass},
			{ pRootSignatureASMCopyDepthQuadPass, 0, 1},
			{ pRootSignatureQuad, 0, 1 },
			{ pRootSignatureASMFillIndirection, 0, 30},
			//{pRootSignatureASMFillIndirection},
			{ pRootSignatureASMCopyDEM, 0 , 2},
			{ pRootSignatureASMDEMColorToAtlas, 0 , 1},
			{ pRootSignatureASMDEMAtlasToColor, 0 , 1},		
			{ pRootSignatureASMFillLodClamp, 0 , 10},
			//{pRootSignatureASMFillLodClamp},
			{pRootSignatureVBPass},
			{pRootSignatureVBPass},
			{pRootSignatureTriangleFiltering},
			{pRootSignatureTriangleFiltering},
			{pRootSignatureTriangleFiltering},
			{pRootSignatureTriangleFiltering},
			{pRootSignatureVBShade, 0, 1},


			{pRootSignatureUpdateSDFVolumeTextureAtlas},
			{pRootSignatureSDFMeshVisualization},
			{pRootSignatureUpsampleSDFShadow},
			{pRootSignatureSDFMeshShadow},

			{pRootSignaturePresentPass}
		};
		const uint32_t descBinderSize = sizeof(descriptorBinderDesc) / sizeof(*descriptorBinderDesc);
		addDescriptorBinder(pRenderer, 0, descBinderSize, descriptorBinderDesc, &pDescriptorBinder);

#if defined(DIRECT3D12)
		const DescriptorInfo*      pDrawId = NULL;
		IndirectArgumentDescriptor indirectArgs[2] = {};
		indirectArgs[0].mType = INDIRECT_CONSTANT;
		indirectArgs[0].mCount = 1;
		indirectArgs[1].mType = INDIRECT_DRAW_INDEX;


		CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 2, indirectArgs };
		pDrawId =
			&pRootSignatureVBPass->pDescriptors[pRootSignatureVBPass->pDescriptorNameToIndexMap["indirectRootConstant"]];
		indirectArgs[0].mRootParameterIndex = pRootSignatureVBPass->pDxRootConstantRootIndices[pDrawId->mIndexInParent];
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
			sanMiguelMeshes.numMaterials * sizeof(uint32_t));

		for (uint32_t i = 0; i < sanMiguelMeshes.numMaterials; ++i)
		{
			materialAlphaData[i] = sanMiguelMeshes.materials[i].alphaTested ? 1 : 0;
		}

		BufferLoadDesc materialPropDesc = {};
		materialPropDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		materialPropDesc.mDesc.mElementCount = sanMiguelMeshes.numMaterials;
		materialPropDesc.mDesc.mStructStride = sizeof(uint32_t);
		materialPropDesc.mDesc.mSize = materialPropDesc.mDesc.mElementCount * 
			materialPropDesc.mDesc.mStructStride;
		materialPropDesc.pData = materialAlphaData;
		materialPropDesc.ppBuffer = &pBufferMaterialProperty;
		materialPropDesc.mDesc.pDebugName = L"Material Prop Desc";
		addResource(&materialPropDesc);

		conf_free(materialAlphaData);


		const uint32_t numBatches = (const uint32_t)sanMiguelMeshes.numMeshes;
		eastl::vector<uint32_t> materialIDPerDrawCall(MATERIAL_BUFFER_SIZE);
		eastl::vector<BufferIndirectCommand> indirectArgsNoAlpha(MAX_DRAWS_INDIRECT, BufferIndirectCommand{ 0 });
		eastl::vector<BufferIndirectCommand> indirectArgsAlpha(MAX_DRAWS_INDIRECT, BufferIndirectCommand{ 0 });
		uint32_t iAlpha = 0, iNoAlpha = 0;

		for (uint32_t i = 0; i < numBatches; ++i)
		{
			uint matID = sanMiguelMeshes.meshes[i].materialId;
			Material* mat = &sanMiguelMeshes.materials[matID];
			uint32 numIDX = sanMiguelMeshes.meshes[i].indexCount;
			uint32 startIDX = sanMiguelMeshes.meshes[i].startIndex;

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


			gPerFrameData.gDrawCount[GEOMSET_OPAQUE] = iNoAlpha;
			gPerFrameData.gDrawCount[GEOMSET_ALPHATESTED] = iAlpha;

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
		filterIbDesc.mDesc.mElementCount = sanMiguelMeshes.totalTriangles * 3;
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
			if (i < sanMiguelMeshes.numMeshes)
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
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			uint32_t bufferSizeTotal = 0;
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				const uint32_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
				bufferSizeTotal += bufferSize;
				pFilterBatchChunk[i][j] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));

				pFilterBatchChunk[i][j]->batches = (FilterBatchData*)conf_calloc(1, bufferSize);
				pFilterBatchChunk[i][j]->currentBatchCount = 0;
				pFilterBatchChunk[i][j]->currentDrawCallCount = 0;
			}
			addUniformGPURingBuffer(pRenderer, bufferSizeTotal, &pBufferFilterBatchData[i]);
		}
		
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
		sdfVolumeTextureAtlasDesc.mFormat = ImageFormat::R16F;
		sdfVolumeTextureAtlasDesc.mWidth = SDF_VOLUME_TEXTURE_ATLAS_WIDTH;
		sdfVolumeTextureAtlasDesc.mHeight = SDF_VOLUME_TEXTURE_ATLAS_HEIGHT;
		sdfVolumeTextureAtlasDesc.mMipLevels = 1;
		sdfVolumeTextureAtlasDesc.mSrgb = false;
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

		//TODO: clean this up
		//camPos = vec3(210.f, 68.f, 22.f);
		//lookAt = camPos + vec3(-0.54f, -0.83f, -0.03f);
#ifdef _DURANGO
		if (gAppSettings.mAsyncCompute)
		{
			setResourcesToComputeCompliantState(0, true);
		}
#endif
		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		pCameraController->setMotionParameters(cmp);
		InputSystem::RegisterInputEvent(cameraInputEvent);
		return true;
	}

	void LoadSkybox()
	{
		Texture*          pPanoSkybox = NULL;
		Shader*           pPanoToCubeShader = NULL;
		RootSignature*    pPanoToCubeRootSignature = NULL;
		Pipeline*         pPanoToCubePipeline = NULL;
		DescriptorBinder* pPanoToCubeDescriptorBinder = NULL;

		Sampler* pSkyboxSampler = NULL;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = ImageFormat::RGBA16F;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mSrgb = false;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pTextureSkybox;
		addResource(&skyboxLoadDesc, true);

		// Load the skybox panorama texture.
		TextureLoadDesc panoDesc = {};
#ifndef TARGET_IOS
		panoDesc.mRoot = FSR_Textures;
#else
		panoDesc.mRoot = FSRoot::FSR_Absolute;    // Resources on iOS are bundled with the application.
#endif
		panoDesc.pFilename = "daytime";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, true);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;

		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);

		DescriptorBinderDesc descriptorBinderDesc = { pPanoToCubeRootSignature, 0, gSkyboxMips + 1 };
		addDescriptorBinder(pRenderer, 0, 1, &descriptorBinderDesc, &pPanoToCubeDescriptorBinder);

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
		cmdResourceBarrier(cmd, 0, NULL, 1, uavBarriers, false);

		DescriptorData params[2] = {};

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(cmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		cmdBindDescriptors(cmd, pPanoToCubeDescriptorBinder, pPanoToCubeRootSignature, 1, params);

		struct Data
		{
			uint mip;
			uint textureSize;
		} data = { 0, gSkyboxSize };

		for (int i = 0; i < gSkyboxMips; i++)
		{
			data.mip = i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pTextureSkybox;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pPanoToCubeDescriptorBinder, pPanoToCubeRootSignature, 2, params);

			const uint32_t* pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers, false);

		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[1] = { { pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers2, false);

		endCmd(cmd);

		waitBatchCompleted();
		queueSubmit(pGraphicsQueue, 1, &cmd, pTransitionFences, 0, 0, 0, 0);
		waitForFences(pRenderer, 1, &pTransitionFences);

		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);
		removeDescriptorBinder(pRenderer, pPanoToCubeDescriptorBinder);

		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };

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
	}
	//
	void setResourcesToComputeCompliantState(uint32_t frameIdx, bool submitAndWait)
	{
		if (submitAndWait)
		{
			beginCmd(ppCmds[frameIdx]);
		}
		
		BufferBarrier barrier[] = { { pScene->m_pIndirectPosBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pScene->m_pIndirectIndexBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferMeshConstants, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferMaterialProperty, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
			{ pBufferFilterIndirectMaterial[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS },
			{ pBufferUncompactedDrawArguments[frameIdx][VIEW_SHADOW], RESOURCE_STATE_UNORDERED_ACCESS },
			{ pBufferUncompactedDrawArguments[frameIdx][VIEW_CAMERA], RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(ppCmds[frameIdx], 7, barrier, 0, NULL, false);

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
		cmdResourceBarrier(ppCmds[frameIdx], gNumGeomSets * NUM_CULLING_VIEWPORTS, indirectDrawBarriers, 0, NULL, true);

		BufferBarrier filteredIndicesBarriers[NUM_CULLING_VIEWPORTS] = {};
		for (uint32_t j = 0; j < NUM_CULLING_VIEWPORTS; j++)
		{
			filteredIndicesBarriers[j].pBuffer = pBufferFilteredIndex[frameIdx][j];
			filteredIndicesBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			filteredIndicesBarriers[j].mSplit = false;
		}
		cmdResourceBarrier(ppCmds[frameIdx], NUM_CULLING_VIEWPORTS, filteredIndicesBarriers, 0, NULL, true);

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
		for (int i = 0; i < gs_ASMMaxRefinement + 1; ++i)
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
		shutdownThreadSystem(pThreadSystem);
		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

		exitProfiler();

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

		for (AlphaTestedImageMaps::iterator startIter = gAlphaTestedImageMaps.begin();
			startIter != gAlphaTestedImageMaps.end(); ++startIter)
		{
			if (startIter->second)
			{
				conf_delete(startIter->second);
			}
		}

		for (uint32_t i = 0; i < pScene->numMaterials; ++i)
		{
			removeResource(gDiffuseMaps[i]);
			removeResource(gNormalMaps[i]);
			removeResource(gSpecularMaps[i]);
		}

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

			for (int k = 0; k < MESH_COUNT; ++k)
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

			removeGPURingBuffer(pBufferFilterBatchData[i]);

		}

		removeScene(pScene);
		for (uint32_t i = 0; i < (uint32_t)gSDFVolumeInstances.size(); ++i)
		{
			if (gSDFVolumeInstances[i])
			{
				conf_delete(gSDFVolumeInstances[i]);
			}
		}

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
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


		for (int i = 0; i < gNumGeomSets; ++i)
		{
			removeShader(pRenderer, pShaderVBBufferPass[i]);
		}


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
		removeDescriptorBinder(pRenderer, pDescriptorBinder);

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
		ASM::RenderTargets asmRenderTargets = {};
		asmRenderTargets.m_pASMDepthAtlas = pRenderTargetASMDepthAtlas;
		asmRenderTargets.m_pASMDEMAtlas = pRenderTargetASMDEMAtlas;
		for (int i = 0; i <= gs_ASMMaxRefinement; ++i)
		{
			asmRenderTargets.m_pASMIndirectionMips.push_back(pRenderTargetASMIndirection[i]);
			asmRenderTargets.m_pASMPrerenderIndirectionMips.push_back(pRenderTargetASMPrerenderIndirection[i]);
		}
		asmRenderTargets.m_pRenderTargetASMLodClamp = pRenderTargetASMLodClamp;
		asmRenderTargets.m_pRenderTargetASMPrerenderLodClamp = pRenderTargetASMPrerenderLodClamp;
		//asmRenderTargets.m_pASMsdsdsdsIndirection = pRenderTargetASMIndirection;
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

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif
		Load_ASM_RenderTargets();
		/************************************************************************/
		// Setup vertex layout for all shaders
		/************************************************************************/
#if defined(__linux__) || defined(METAL)
		VertexLayout vertexLayoutCompleteModel = {};
		vertexLayoutCompleteModel.mAttribCount = 4;
		vertexLayoutCompleteModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutCompleteModel.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutCompleteModel.mAttribs[0].mBinding = 0;
		vertexLayoutCompleteModel.mAttribs[0].mLocation = 0;
		vertexLayoutCompleteModel.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutCompleteModel.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayoutCompleteModel.mAttribs[1].mBinding = 1;
		vertexLayoutCompleteModel.mAttribs[1].mLocation = 1;
		vertexLayoutCompleteModel.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutCompleteModel.mAttribs[2].mFormat = ImageFormat::RGB32F;
		vertexLayoutCompleteModel.mAttribs[2].mBinding = 2;
		vertexLayoutCompleteModel.mAttribs[2].mLocation = 2;
		vertexLayoutCompleteModel.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayoutCompleteModel.mAttribs[3].mFormat = ImageFormat::RGB32F;
		vertexLayoutCompleteModel.mAttribs[3].mBinding = 3;
		vertexLayoutCompleteModel.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

		// Position only vertex stream that is used in shadow opaque pass
		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;
#else

		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;


		VertexLayout vertexLayoutCompleteModel = {};
		vertexLayoutCompleteModel.mAttribCount = 4;
		vertexLayoutCompleteModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutCompleteModel.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutCompleteModel.mAttribs[0].mBinding = 0;
		vertexLayoutCompleteModel.mAttribs[0].mLocation = 0;
		vertexLayoutCompleteModel.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutCompleteModel.mAttribs[1].mFormat = ImageFormat::R32UI;
		vertexLayoutCompleteModel.mAttribs[1].mBinding = 1;
		vertexLayoutCompleteModel.mAttribs[1].mLocation = 1;
		vertexLayoutCompleteModel.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutCompleteModel.mAttribs[2].mFormat = ImageFormat::R32UI;
		vertexLayoutCompleteModel.mAttribs[2].mBinding = 2;
		vertexLayoutCompleteModel.mAttribs[2].mLocation = 2;
		vertexLayoutCompleteModel.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayoutCompleteModel.mAttribs[3].mFormat = ImageFormat::R32UI;
		vertexLayoutCompleteModel.mAttribs[3].mBinding = 3;
		vertexLayoutCompleteModel.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = ImageFormat::R32UI;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

#endif


		//layout and pipeline for sphere draw
		VertexLayout vertexLayoutRegular = {};
		vertexLayoutRegular.mAttribCount = 2;
		vertexLayoutRegular.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutRegular.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutRegular.mAttribs[0].mBinding = 0;
		vertexLayoutRegular.mAttribs[0].mLocation = 0;
		vertexLayoutRegular.mAttribs[0].mOffset = 0;
		vertexLayoutRegular.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutRegular.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutRegular.mAttribs[1].mBinding = 0;
		vertexLayoutRegular.mAttribs[1].mLocation = 1;
		vertexLayoutRegular.mAttribs[1].mOffset = 3 * sizeof(float);
#if FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA
		VertexLayout vertexLayoutZPass = {};
		vertexLayoutZPass.mAttribCount = 1;
		vertexLayoutZPass.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutZPass.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutZPass.mAttribs[0].mBinding = 0;
		vertexLayoutZPass.mAttribs[0].mLocation = 0;
		vertexLayoutZPass.mAttribs[0].mOffset = 0;
#endif
		VertexLayout vertexLayoutQuad = {};
		vertexLayoutQuad.mAttribCount = 2;
		vertexLayoutQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutQuad.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutQuad.mAttribs[0].mBinding = 0;
		vertexLayoutQuad.mAttribs[0].mLocation = 0;
		vertexLayoutQuad.mAttribs[0].mOffset = 0;

		vertexLayoutQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutQuad.mAttribs[1].mFormat = ImageFormat::RG32F;
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
		//upSampleSDFShadowPipelineSettings.pDepthState = pDepthStateDisable;
		upSampleSDFShadowPipelineSettings.pDepthState = NULL;
		upSampleSDFShadowPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		upSampleSDFShadowPipelineSettings.pRootSignature = pRootSignatureUpsampleSDFShadow;
		upSampleSDFShadowPipelineSettings.pShaderProgram = pShaderUpsampleSDFShadow;
		upSampleSDFShadowPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		upSampleSDFShadowPipelineSettings.pColorFormats = &pRenderTargetUpSampleSDFShadow->mDesc.mFormat;
		upSampleSDFShadowPipelineSettings.pSrgbValues = &pRenderTargetUpSampleSDFShadow->mDesc.mSrgb;
		upSampleSDFShadowPipelineSettings.mSampleQuality = pRenderTargetUpSampleSDFShadow->mDesc.mSampleQuality;
		//upSampleSDFShadowPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		upSampleSDFShadowPipelineSettings.pVertexLayout = &vertexLayoutQuad;

		addPipeline(pRenderer, &desc, &pPipelineUpsampleSDFShadow);

		/************************************************************************/
		// Setup the resources needed for the Deferred Pass Pipeline
		/******************************/

		

		


		/************************************************************************/
		// Setup the resources needed for the Visibility Buffer Pipeline
		/******************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& vbPassPipelineSettings = desc.mGraphicsDesc;
		vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbPassPipelineSettings.mRenderTargetCount = 1;
		vbPassPipelineSettings.pDepthState = pDepthStateEnable;
		vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mDesc.mFormat;
		vbPassPipelineSettings.pSrgbValues = &pRenderTargetVBPass->mDesc.mSrgb;
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
		vbShadePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
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

		//vbPassPipelineSettings.pColorFormats = &



		/************************************************************************/
		// Setup the resources needed for the Forward (CHANGE TO DEFERRED LATER) asm model Pipeline
		/******************************/




		/************************************************************************/
		// Setup the resources needed for shadow map
		/************************************************************************/
		desc.mGraphicsDesc = {};
		
		/*-----------------------------------------------------------*/
		// Setup the resources needed for the ESM Blur Compute Pipeline
		/*-----------------------------------------------------------*/
		desc.mType = PIPELINE_TYPE_COMPUTE;
		/*-----------------------------------------------------------*/
		// Setup the resources needed for the buffer copy pipeline
		/*-----------------------------------------------------------*/
		desc.mComputeDesc = {};

		ComputePipelineDesc& clearBufferPipelineSettings = desc.mComputeDesc;
		clearBufferPipelineSettings.pShaderProgram = pShaderClearBuffers;
		//clearBufferPipelineSettings.pRootSignature = pRootSignatureClearBuffers;
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
		//batchCompactionPipelineSettings.pRootSignature = pRootSignatureBatchCompaction;
		batchCompactionPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &desc, &pPipelineBatchCompaction);

		/*-----------------------------------------------------------*/
		// Setup the resources needed for the generate mipmap copy pipeline
		/*-----------------------------------------------------------*/
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
		// Setup the resources needed for Skybox
		/************************************************************************/
		/*desc.mType = PIPELINE_TYPE_GRAPHICS;
		desc.mGraphicsDesc = forwardGraphicsDesc.mGraphicsDesc;
		GraphicsPipelineDesc& skyboxPipelineSettings = desc.mGraphicsDesc;
		skyboxPipelineSettings.pDepthState = NULL;
		skyboxPipelineSettings.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineSettings.pShaderProgram = pShaderSkybox;
		skyboxPipelineSettings.pVertexLayout = NULL;
		addPipeline(pRenderer, &desc, &pPipelineSkybox);*/

		/************************************************************************/
		// Setup Skybox pipeline
		/************************************************************************/

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
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
		skyboxPipelineSettings.pSrgbValues = &pRenderTargetIntermediate->mDesc.mSrgb;
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
		//pDepthStateLEQUALEnable
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
		quadPipelineDesc.pSrgbValues = &pRenderTargetIntermediate->mDesc.mSrgb;
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
		ASMCopyDepthQuadPipelineDesc.pSrgbValues = &pRenderTargetASMDepthAtlas->mDesc.mSrgb;
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
		ASMCopyDEMQuadPipelineDesc.pSrgbValues = &pRenderTargetASMDEMAtlas->mDesc.mSrgb;
		ASMCopyDEMQuadPipelineDesc.mSampleCount = pRenderTargetASMDEMAtlas->mDesc.mSampleCount;
		ASMCopyDEMQuadPipelineDesc.mSampleQuality = pRenderTargetASMDEMAtlas->mDesc.mSampleQuality;
		ASMCopyDEMQuadPipelineDesc.pRootSignature = pRootSignatureASMCopyDEM;
		ASMCopyDEMQuadPipelineDesc.pShaderProgram = pShaderASMCopyDEM;
		ASMCopyDEMQuadPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		//ASMCopyDEMQuadPipelineDesc.pVertexLayout = &vertexLayoutQuad;
		addPipeline(pRenderer, &desc, &pPipelineASMCopyDEM);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMAtlasToColorPipelineDesc = desc.mGraphicsDesc;
		ASMAtlasToColorPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMAtlasToColorPipelineDesc.mRenderTargetCount = 1;
		ASMAtlasToColorPipelineDesc.pDepthState = NULL;
		ASMAtlasToColorPipelineDesc.pColorFormats = &pRenderTargetASMColorPass->mDesc.mFormat;
		ASMAtlasToColorPipelineDesc.pSrgbValues = &pRenderTargetASMColorPass->mDesc.mSrgb;
		ASMAtlasToColorPipelineDesc.mSampleCount = pRenderTargetASMColorPass->mDesc.mSampleCount;
		ASMAtlasToColorPipelineDesc.mSampleQuality = pRenderTargetASMColorPass->mDesc.mSampleQuality;
		ASMAtlasToColorPipelineDesc.pRootSignature = pRootSignatureASMDEMAtlasToColor;
		ASMAtlasToColorPipelineDesc.pShaderProgram = pShaderASMGenerateDEM;
		ASMAtlasToColorPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		//ASMAtlasToColorPipelineDesc.pVertexLayout = &vertexLayoutQuad;
		addPipeline(pRenderer, &desc, &pPipelineASMDEMAtlasToColor);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMColorToAtlasPipelineDesc = desc.mGraphicsDesc;
		ASMColorToAtlasPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMColorToAtlasPipelineDesc.mRenderTargetCount = 1;
		ASMColorToAtlasPipelineDesc.pDepthState = NULL;
		ASMColorToAtlasPipelineDesc.pColorFormats = &pRenderTargetASMDEMAtlas->mDesc.mFormat;
		ASMColorToAtlasPipelineDesc.pSrgbValues = &pRenderTargetASMDEMAtlas->mDesc.mSrgb;
		ASMColorToAtlasPipelineDesc.mSampleCount = pRenderTargetASMDEMAtlas->mDesc.mSampleCount;
		ASMColorToAtlasPipelineDesc.mSampleQuality = pRenderTargetASMDEMAtlas->mDesc.mSampleQuality;
		ASMColorToAtlasPipelineDesc.pRootSignature = pRootSignatureASMDEMColorToAtlas;
		ASMColorToAtlasPipelineDesc.pShaderProgram = pShaderASMGenerateDEM;
		ASMColorToAtlasPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		//ASMColorToAtlasPipelineDesc.pVertexLayout = &vertexLayoutQuad;
		addPipeline(pRenderer, &desc, &pPipelineASMDEMColorToAtlas);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMIndirectionPipelineDesc = desc.mGraphicsDesc;
		ASMIndirectionPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMIndirectionPipelineDesc.mRenderTargetCount = 1;
		ASMIndirectionPipelineDesc.pDepthState = NULL;
		ASMIndirectionPipelineDesc.pColorFormats = &pRenderTargetASMIndirection[0]->mDesc.mFormat;
		ASMIndirectionPipelineDesc.pSrgbValues = &pRenderTargetASMIndirection[0]->mDesc.mSrgb;
		//ASMCopyDepthQuadPipelineDesc.mDepthStencilFormat = pRenderTargetASMDepthAtlas->mDesc.mFormat;
		ASMIndirectionPipelineDesc.mSampleCount = pRenderTargetASMIndirection[0]->mDesc.mSampleCount;
		ASMIndirectionPipelineDesc.mSampleQuality = pRenderTargetASMIndirection[0]->mDesc.mSampleQuality;
		ASMIndirectionPipelineDesc.pRootSignature = pRootSignatureASMFillIndirection;
		ASMIndirectionPipelineDesc.pShaderProgram = pShaderASMFillIndirection;
		ASMIndirectionPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		//ASMIndirectionPipelineDesc.pVertexLayout = &vertexLayoutQuad;
		addPipeline(pRenderer, &desc, &pPipelineASMFillIndirection);

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& ASMFillLodClampPipelineDesc = desc.mGraphicsDesc;
		ASMFillLodClampPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ASMFillLodClampPipelineDesc.mRenderTargetCount = 1;
		ASMFillLodClampPipelineDesc.pDepthState = NULL;
		ASMFillLodClampPipelineDesc.pColorFormats = &pRenderTargetASMLodClamp->mDesc.mFormat;
		ASMFillLodClampPipelineDesc.pSrgbValues = &pRenderTargetASMLodClamp->mDesc.mSrgb;
		//ASMCopyDepthQuadPipelineDesc.mDepthStencilFormat = pRenderTargetASMDepthAtlas->mDesc.mFormat;
		ASMFillLodClampPipelineDesc.mSampleCount = pRenderTargetASMLodClamp->mDesc.mSampleCount;
		ASMFillLodClampPipelineDesc.mSampleQuality = pRenderTargetASMLodClamp->mDesc.mSampleQuality;
		ASMFillLodClampPipelineDesc.pRootSignature = pRootSignatureASMFillLodClamp;
		ASMFillLodClampPipelineDesc.pShaderProgram = pShaderASMFillIndirection;
		ASMFillLodClampPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		//ASMFillLodClampPipelineDesc.pVertexLayout = &vertexLayoutQuad;
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
		pipelineSettingsFinalPass.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsFinalPass.pVertexLayout = &vertexLayoutCopyShaders;
		pipelineSettingsFinalPass.pRootSignature = pRootSignaturePresentPass;
		pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;

		addPipeline(pRenderer, &desc, &pPipelinePresentPass);


		SetupASMDebugTextures();
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

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();
		unloadProfiler();

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

		for (int i = 0; i < gNumGeomSets; ++i)
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


			const AABBox& sdfVolumeBBox = sdfVolumeData.mLocalBoundingBox;
			const ivec3& sdfVolumeDimensionSize = sdfVolumeData.mSDFVolumeSize;

			//mat4 volumeToWorldMat = meshModelMat * mat4::translation(sdfVolumeData.mLocalBoundingBox.GetCenter()) 
				//*  mat4::scale(sdfVolumeData.mLocalBoundingBox.GetExtent());

			float maxExtentValue = Helper::GetMaxElem(sdfVolumeBBox.GetExtent());
					   
			mat4 uniformScaleVolumeToWorld = meshModelMat * mat4::translation(sdfVolumeBBox.GetCenter())
				*  mat4::scale(vec3(maxExtentValue));

			vec3 invSDFVolumeDimSize
			(
				1.f / sdfVolumeDimensionSize.getX(),
				1.f / sdfVolumeDimensionSize.getY(),
				1.f / sdfVolumeDimensionSize.getZ()
			);
			gMeshSDFConstants.mWorldToVolumeMat[i] = inverse(uniformScaleVolumeToWorld);
					   
			//get the extent position in the 0.... 1 scale
			vec3 localPositionExtent = sdfVolumeBBox.GetExtent() / maxExtentValue;


			vec3 uvScale = Helper::Piecewise_Prod(
				Helper::ivec3ToVec3f(sdfVolumeDimensionSize), inverseSDFTextureAtlasSize);


			float maximumVolumeScale = Helper::GetMatrixMaximumScale(uniformScaleVolumeToWorld);

			gMeshSDFConstants.mLocalPositionExtent[i] = vec4(localPositionExtent - invSDFVolumeDimSize, 1.f);

			vec3 initialUV = Helper::Piecewise_Prod(
				Helper::ivec3ToVec3f(sdfVolumeDimensionSize), inverseSDFTextureAtlasSize) * 0.5f;

			vec3 newUV = Helper::Piecewise_Division(initialUV, localPositionExtent);


			maximumVolumeScale *= (sdfVolumeData.mIsTwoSided ? -1.f : 1.0f);
			gMeshSDFConstants.mUVScaleAndVolumeScale[i] = vec4(newUV, maximumVolumeScale);

			vec3 offsetUV = Helper::Piecewise_Prod(
				Helper::ivec3ToVec3f(sdfVolumeData.mSDFVolumeTextureNode.mAtlasAllocationCoord),
				inverseSDFTextureAtlasSize);

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

		// Dynamic UI elements
		
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
		constexpr float farValue = 4000.f;
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
		//only for view camera, for shadow it depends on the alggorithm being uysed
		gPerFrameData.gEyeObjectSpace[VIEW_CAMERA] = (gCameraUniformData.mInvView
			* vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();
		
		gVisibilityBufferConstants.mWorldViewProjMat[VIEW_CAMERA] = gCameraUniformData.mViewProject * gMeshInfoUniformData[0].mWorldMat;
		gVisibilityBufferConstants.mCullingViewports[VIEW_CAMERA].mWindowSize = { (float)width, (float)height };
		gVisibilityBufferConstants.mCullingViewports[VIEW_CAMERA].mSampleCount = 1;
		gPerFrameData.mValidNumCull = 1;

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


		if (gCurrentShadowType == SHADOW_TYPE_ESM)
		{
			gESMUniformData.mEsmControl = gEsmCpuSettings.mEsmControl;
		}

		//
		/************************************************************************/
		// ASM Update - for shadow map
		/************************************************************************/
	
		if (gCurrentShadowType == SHADOW_TYPE_ASM)
		{
			mat4 nextRotation = mat4::rotationXY(gLightCpuSettings.mSunControl.x, gLightCpuSettings.mSunControl.y + (PI / 2.f));
			vec3 lightDirDest = -(inverse(nextRotation) * vec4(0, 0, 1, 0)).getXYZ();

			float f = float((static_cast<unsigned int>(asmCurrentTime) >> 5) & 0xfff) / 8096.0f;
			vec3 asmLightDir = normalize(lerp(newLightDir, lightDirDest, fabsf(f * 2.f)));

			

			unsigned int newDelta = static_cast<unsigned int>(deltaTime * 1000.f);
			unsigned int updateDeltaTime = 4500;
			unsigned int  halfWayTime = static_cast<unsigned int>(asmCurrentTime) + (updateDeltaTime >> 1);		
			
			float f_half = float((static_cast<unsigned int>(halfWayTime) >> 5) & 0xfff) / 8096.0f;
			vec3 halfWayLightDir = normalize(lerp(newLightDir, lightDirDest, fabsf(f_half * 2.f)));

			pASM->Tick(gASMCpuSettings, pLightView,  asmLightDir, halfWayLightDir, static_cast<unsigned int>(currentTime),
				newDelta, false, false, updateDeltaTime);
			
		}


		for (int i = 0; i < MESH_COUNT; ++i)
		{
			gMeshInfoData[i].mTranslationMat = mat4::translation(vec3(gMeshInfoData[i].mTranslation.x,
				gMeshInfoData[i].mTranslation.y, gMeshInfoData[i].mTranslation.z));

			gMeshInfoData[i].mScaleMat = mat4::scale(vec3(gMeshInfoData[i].mScale.x, 
				gMeshInfoData[i].mScale.y, gMeshInfoData[i].mScale.z));

			mat4 offsetTranslationMat = mat4::translation(f3Tov3(gMeshInfoData[i].mOffsetTranslation));

			gMeshInfoUniformData[i].mWorldMat =  gMeshInfoData[i].mTranslationMat * gMeshInfoData[i].mScaleMat * offsetTranslationMat;
			gMeshInfoUniformData[i].mWorldViewProjMat = gCameraUniformData.mViewProject * gMeshInfoUniformData[i].mWorldMat;

			gMeshASMProjectionInfoUniformData[i].mWorldMat = gMeshInfoUniformData[i].mWorldMat;

			if (gCurrentShadowType == SHADOW_TYPE_ASM)
			{
				gMeshASMProjectionInfoUniformData[i].mWorldViewProjMat = mat4::identity();
			}
			else if (gCurrentShadowType == SHADOW_TYPE_ESM)
			{
				gMeshASMProjectionInfoUniformData[i].mWorldViewProjMat = gLightUniformData.mLightViewProj * gMeshInfoUniformData[i].mWorldMat;
			}
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
	static void drawEsmShadowMap(Cmd* cmd)
	{
		BufferUpdateDesc bufferUpdate = { pBufferMeshShadowProjectionTransforms[0][gFrameIndex], 
			&gMeshASMProjectionInfoUniformData[0] };
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


		cmdBindVertexBuffer(cmd, 1, &pScene->m_pIndirectPosBuffer, NULL);
		cmdBindPipeline(cmd, pPipelineESMIndirectDepthPass);
		
#ifndef METAL
		cmdBindDescriptors(cmd, pDescriptorBinder,
						   pRootSignatureVBPass, 3, alphaTestedParams);
#else
		cmdBindDescriptors(cmd, pDescriptorBinder,
						   pRootSignatureVBPass, 1, alphaTestedParams);
#endif
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass,
			gPerFrameData.gDrawCount[GEOMSET_OPAQUE],
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			0,
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW],
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);

		Buffer* pVertexBuffersPosTex[] = { pScene->m_pIndirectPosBuffer,
			pScene->m_pIndirectTexCoordBuffer };
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);

		cmdBindPipeline(cmd, pPipelineESMIndirectAlphaDepthPass);
		
#ifdef METAL

		cmdBindDescriptors(cmd, pDescriptorBinder,
						   pRootSignatureVBPass, 3, alphaTestedParams);

#endif
		
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass,
			gPerFrameData.gDrawCount[GEOMSET_ALPHATESTED],
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

		// Transfer our render target to a render target state
		TextureBarrier barrier[] = { { pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier, true);
		cmdFlushBarriers(cmd);

		setRenderTarget(cmd, 1, &pRenderTargetScreen, NULL, NULL);

		// Draw the skybox
		cmdBindPipeline(cmd, pPipelineSkybox);

		DescriptorData skyParams[2] = {};
		skyParams[0].pName = "RootConstantCameraSky";
		skyParams[0].pRootConstant = &gUniformDataSky;
		skyParams[1].pName = "skyboxTex";
		skyParams[1].ppTextures = &pTextureSkybox;

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureSkybox, 2, skyParams);
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

		//Texture* volumeTextureAtlas = pSDFVolumeTextureAtlas->mVolumeTextureAtlasRT->pTexture;
		Texture* volumeTextureAtlas = pTextureSDFVolumeAtlas;
		//Texture* volumeTextureAtlas = pTexture3DTest;
	
		TextureBarrier textureBarriers[] = { {volumeTextureAtlas, RESOURCE_STATE_UNORDERED_ACCESS } };

		cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);

		cmdBindPipeline(cmd, pPipelineUpdateSDFVolumeTextureAtlas);
		
		DescriptorData params[3] = {};
		params[0].pName = "SDFVolumeTextureAtlas";
		params[0].ppTextures = &volumeTextureAtlas;
		params[1].pName = "SDFVolumeDataBuffer";
		params[1].ppBuffers = &pBufferSDFVolumeData[gFrameIndex];
		
		params[2].pName = "UpdateSDFVolumeTextureAtlasCB";
		params[2].ppBuffers = &pBufferUpdateSDFVolumeTextureAtlasConstants[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureUpdateSDFVolumeTextureAtlas, 3, params);


		uint32_t* threadGroup = pShaderUpdateSDFVolumeTextureAtlas->mReflection.mStageReflections[0].mNumThreadsPerGroup;

		cmdDispatch(cmd, 
			volumeTextureAtlas->mDesc.mWidth / threadGroup[0],
			volumeTextureAtlas->mDesc.mHeight / threadGroup[1],
			volumeTextureAtlas->mDesc.mDepth / threadGroup[2]);
			   
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
				//pDepthCopyTexture,
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers, false);

		cmdBindPipeline(cmd, pPipelineSDFMeshVisualization);

		DescriptorData params[6] = {};
		
		
		params[0].pName = "OutTexture";
		params[0].ppTextures = &pRenderTargetSDFMeshVisualization->pTexture;
		
		params[1].pName = "SDFVolumeTextureAtlas";
		params[1].ppTextures = &pTextureSDFVolumeAtlas;

		params[2].pName = "cameraUniformBlock";
		params[2].ppBuffers = &pBufferCameraUniform[gFrameIndex];

		params[3].pName = "meshSDFUniformBlock";
		params[3].ppBuffers = &pBufferMeshSDFConstants[gFrameIndex];

		params[4].pName = "DepthTexture";
		//params[4].ppTextures = &pDepthCopyTexture;
		params[4].ppTextures = &pRenderTargetDepth->pTexture;

		cmdBindDescriptors(cmd, pDescriptorBinder, 
			pRootSignatureSDFMeshVisualization, 5, params);
		
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
				//pRenderTargetSDFVolumeAtlas->pTexture,
				pTextureSDFVolumeAtlas,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				//pDepthCopyTexture,
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers, true);
		cmdFlushBarriers(cmd);

		cmdBindPipeline(cmd, pPipelineSDFMeshShadow);

		DescriptorData params[6] = {};

		params[0].pName = "OutTexture";
		params[0].ppTextures = &pRenderTargetSDFMeshShadow->pTexture;

		params[1].pName = "SDFVolumeTextureAtlas";
		params[1].ppTextures = &pTextureSDFVolumeAtlas;
		params[2].pName = "cameraUniformBlock";
		params[2].ppBuffers = &pBufferCameraUniform[gFrameIndex];

		params[3].pName = "meshSDFUniformBlock";
		params[3].ppBuffers = &pBufferMeshSDFConstants[gFrameIndex];

		params[4].pName = "DepthTexture";
		params[4].ppTextures = &pRenderTargetDepth->pTexture;

		params[5].pName = "lightUniformBlock";
		params[5].ppBuffers = &pBufferLightUniform[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureSDFMeshShadow, 6, params);

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
				//pDepthCopyTexture,
				pRenderTargetDepth->pTexture,
				RESOURCE_STATE_SHADER_RESOURCE
			},
			{
				pRenderTargetUpSampleSDFShadow->pTexture,
				RESOURCE_STATE_RENDER_TARGET
			}
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, textureBarriers, false);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetUpSampleSDFShadow->mDesc.mClearValue;

		setRenderTarget(cmd, 1, &pRenderTargetUpSampleSDFShadow, NULL, &loadActions);


		cmdBindPipeline(cmd, pPipelineUpsampleSDFShadow);
		cmdBindVertexBuffer(cmd, 1, &pBufferQuadVertex, NULL);
		
		DescriptorData params[3] = {};

		params[0].pName = "SDFShadowTexture";
		params[0].ppTextures = &pRenderTargetSDFMeshShadow->pTexture;

		params[1].pName = "cameraUniformBlock";
		params[1].ppBuffers = &pBufferCameraUniform[gFrameIndex];

		params[2].pName = "DepthTexture";
		params[2].ppTextures = &pRenderTargetDepth->pTexture;

		cmdBindDescriptors(cmd, pDescriptorBinder,
			pRootSignatureUpsampleSDFShadow, 3, params);

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
		{
			uavBarriers[i] = {
				pBufferUncompactedDrawArguments[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS};
		}
		cmdResourceBarrier(cmd, NUM_CULLING_VIEWPORTS, uavBarriers, 0, NULL, false);

		
	

		
		/************************************************************************/
		// Clear previous indirect arguments
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Clear Buffers", true);
		DescriptorData clearParams[3] = {};
		clearParams[0].pName = "indirectDrawArgsBufferAlpha";
		clearParams[0].mCount = NUM_CULLING_VIEWPORTS;
		clearParams[0].ppBuffers = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_ALPHATESTED];
		clearParams[1].pName = "indirectDrawArgsBufferNoAlpha";
		clearParams[1].mCount = NUM_CULLING_VIEWPORTS;
		clearParams[1].ppBuffers = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_OPAQUE];
		clearParams[2].pName = "uncompactedDrawArgsRW";
		clearParams[2].mCount = NUM_CULLING_VIEWPORTS;
		clearParams[2].ppBuffers = pBufferUncompactedDrawArguments[frameIdx];
		//cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureClearBuffers, 3, clearParams);
		cmdBindPipeline(cmd, pPipelineClearBuffers);
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureTriangleFiltering, 3, clearParams);
		
		uint32_t numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		
		/************************************************************************/
		// Synchronization
		/************************************************************************/

		uint32_t numBarriers = (NUM_CULLING_VIEWPORTS * gNumGeomSets) + NUM_CULLING_VIEWPORTS;
		Buffer** clearBarriers = (Buffer**)alloca(numBarriers * sizeof(Buffer*));

		uint32_t index = 0;
		for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
		{
			clearBarriers[index++] = pBufferUncompactedDrawArguments[frameIdx][i];
			clearBarriers[index++] = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_ALPHATESTED][i];
			clearBarriers[index++] = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_OPAQUE][i];
		}
		cmdSynchronizeResources(cmd, numBarriers, clearBarriers, 0, NULL, false);
		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/

		uint32_t currentSmallBatchChunk = 0;
		uint accumDrawCount = 0;
		uint accumNumTriangles = 0;
		uint accumNumTrianglesAtStartOfBatch = 0;
		uint batchStart = 0;

		



		resetGPURingBuffer(pBufferFilterBatchData[frameIdx]);
		uint64_t batchSize = BATCH_COUNT * sizeof(SmallBatchData);


		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Filter Triangles", true);
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		DescriptorData filterParams[7] = {};
		filterParams[0].pName = "vertexDataBuffer";
		filterParams[0].ppBuffers = &pScene->m_pIndirectPosBuffer;
		filterParams[1].pName = "indexDataBuffer";
		filterParams[1].ppBuffers = &pScene->m_pIndirectIndexBuffer;
		filterParams[2].pName = "meshConstantsBuffer";
		filterParams[2].ppBuffers = &pBufferMeshConstants;
		filterParams[3].pName = "filteredIndicesBuffer";
		filterParams[3].mCount = NUM_CULLING_VIEWPORTS;
		filterParams[3].ppBuffers = pBufferFilteredIndex[frameIdx];
		filterParams[4].pName = "uncompactedDrawArgsRW";
		filterParams[4].mCount = NUM_CULLING_VIEWPORTS;
		filterParams[4].ppBuffers = pBufferUncompactedDrawArguments[frameIdx];
		filterParams[5].pName = "visibilityBufferConstants";
		filterParams[5].ppBuffers = &pBufferVisibilityBufferConstants[frameIdx];
		filterParams[6].pName = "batchData";
		filterParams[6].ppBuffers = &pBufferFilterBatchData[frameIdx]->pBuffer;
		filterParams[6].pSizes = &batchSize;
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureTriangleFiltering, 7, filterParams);
			   
		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			//MeshInstance* drawBatch = &gModels[0].meshes[i];
			MeshIn* drawBatch = &pScene->meshes[i];
			FilterBatchChunk* batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
			for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
			{
				const ClusterCompact* clusterCompactInfo = &drawBatch->clusterCompacts[j];

				{
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
		gPerFrameData.gDrawCount[GEOMSET_OPAQUE] = accumDrawCount;
		gPerFrameData.gDrawCount[GEOMSET_ALPHATESTED] = accumDrawCount;
		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx][currentSmallBatchChunk]);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);

		for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
		{
			uavBarriers[i] = { 
				pBufferUncompactedDrawArguments[frameIdx][i], RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE   };
		}

		cmdResourceBarrier(cmd, NUM_CULLING_VIEWPORTS, uavBarriers, 0, NULL, false);

		
		/************************************************************************/
		// Batch compaction
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Batch Compaction", true);

		cmdBindPipeline(cmd, pPipelineBatchCompaction);
		DescriptorData compactParams[6] = {};
		compactParams[0].pName = "materialProps";
		compactParams[0].ppBuffers = &pBufferMaterialProperty; //resource
		compactParams[1].pName = "indirectMaterialBuffer";
		compactParams[1].ppBuffers = &pBufferFilterIndirectMaterial[frameIdx];
		compactParams[2].pName = "indirectDrawArgsBufferAlpha";
		compactParams[2].mCount = NUM_CULLING_VIEWPORTS;
		compactParams[2].ppBuffers = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_ALPHATESTED];
		compactParams[3].pName = "indirectDrawArgsBufferNoAlpha";
		compactParams[3].mCount = NUM_CULLING_VIEWPORTS;
		compactParams[3].ppBuffers = pBufferFilteredIndirectDrawArguments[frameIdx][GEOMSET_OPAQUE];
		compactParams[4].pName = "uncompactedDrawArgs";
		compactParams[4].mCount = NUM_CULLING_VIEWPORTS;
		compactParams[4].ppBuffers = pBufferUncompactedDrawArguments[frameIdx]; 

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureTriangleFiltering, 5, compactParams);
		numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);


		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);


		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
	}

	void filterTriangles(Cmd* cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk)
	{
		UNREF_PARAM(frameIdx);
		// Check if there are batches to filter
		if (batchChunk->currentBatchCount == 0)
		{
			return;
		}

		uint32_t batchSize = batchChunk->currentBatchCount * sizeof(SmallBatchData);
		GPURingBufferOffset offset = getGPURingBufferOffset(pBufferFilterBatchData[frameIdx], batchSize);
		BufferUpdateDesc updateDesc = { offset.pBuffer, batchChunk->batches, 0, offset.mOffset, batchSize };
		updateResource(&updateDesc, true);

		uint64_t size = BATCH_COUNT * sizeof(SmallBatchData);
		DescriptorData params[1] = {};
		params[0].pName = "batchData";
		params[0].pOffsets = &offset.mOffset;
		params[0].ppBuffers = &offset.pBuffer;
		params[0].pSizes = &size;
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureTriangleFiltering, 1, params);
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

		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, true);
		cmdFlushBarriers(cmd);

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
		
		
		Buffer* pVertexBuffersPosTex[] = { pScene->m_pIndirectPosBuffer,
			pScene->m_pIndirectTexCoordBuffer };
			   		

		DescriptorData vbPassParams[3] = {};
		vbPassParams[0].pName = "objectUniformBlock";
		vbPassParams[0].ppBuffers = &pBufferMeshTransforms[0][gFrameIndex];
		vbPassParams[1].pName = "diffuseMaps";
		vbPassParams[1].mCount = (uint32_t)gDiffuseMaps.size();
		vbPassParams[1].ppTextures = gDiffuseMaps.data();
		vbPassParams[2].pName = "indirectMaterialBuffer";
		vbPassParams[2].ppBuffers = &pIndirectMaterialBuffer;
		
		
		cmdBindPipeline(cmd, pPipelineVBBufferPass[GEOMSET_OPAQUE]);
#ifndef METAL
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureVBPass, 3, vbPassParams);
#endif
		
#ifdef METAL
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureVBPass, 1, vbPassParams);
#endif

		
		
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, profileNames[0], true);
		
		Buffer* pIndirectBufferPositionOnly = pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_CAMERA];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrameData.gDrawCount[GEOMSET_OPAQUE], pIndirectBufferPositionOnly, 0, pIndirectBufferPositionOnly,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);

		
		
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, profileNames[1], true);
		cmdBindPipeline(cmd, pPipelineVBBufferPass[GEOMSET_ALPHATESTED]);
		
#ifdef METAL
		cmdBindVertexBuffer(cmd, 2, pVertexBuffersPosTex, NULL);
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureVBPass, 3, vbPassParams);
#endif
		
		
		Buffer* pIndirectBufferPositionAndTex = 
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_CAMERA];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrameData.gDrawCount[GEOMSET_ALPHATESTED],
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
		{
			updateASMUniform();
		}

#if ENABLE_SDF_SHADOW_DOWNSAMPLE
		Texture* sdfShadowTexture = pRenderTargetUpSampleSDFShadow->pTexture;
#else
		Texture* sdfShadowTexture = pRenderTargetSDFMeshShadow->pTexture;
#endif
		Texture* esmShadowMap = pRenderTargetShadowMap->pTexture;

		TextureBarrier textureBarriers[] = {
			{sdfShadowTexture, RESOURCE_STATE_SHADER_RESOURCE},
			{esmShadowMap, RESOURCE_STATE_SHADER_RESOURCE}
		};

		cmdResourceBarrier(cmd, 0, NULL, 2, textureBarriers, true);
		cmdFlushBarriers(cmd);
		


		TextureBarrier barrier[] = { 
			{pRenderTargetVBPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE} };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier, false);

		Buffer* pIndirectBuffers[gNumGeomSets] = { NULL };
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			pIndirectBuffers[i] = pBufferFilteredIndirectDrawArguments[frameIdx][i][VIEW_CAMERA];
		}

		BufferBarrier bufferBarriers[] = { 
			{pBufferMeshConstants, RESOURCE_STATE_SHADER_RESOURCE},

		{pScene->m_pIndirectPosBuffer, RESOURCE_STATE_SHADER_RESOURCE},
		{pIndirectBuffers[0], RESOURCE_STATE_SHADER_RESOURCE},
		{pIndirectBuffers[1], RESOURCE_STATE_SHADER_RESOURCE} ,
		{pBufferFilterIndirectMaterial[frameIdx], RESOURCE_STATE_SHADER_RESOURCE}
		};
		cmdResourceBarrier(cmd, 5, bufferBarriers, 0, NULL, false);


		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "VB Shade Pass");

		RenderTarget* pDestRenderTarget = pRenderTargetScreen;
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestRenderTarget->mDesc.mClearValue;

		setRenderTarget(cmd, 1, &pDestRenderTarget, NULL, &loadActions);

		cmdBindPipeline(cmd, pPipelineVBShadeSrgb);

		

		DescriptorData vbShadeParams[24] = {};
		vbShadeParams[0].pName = "vbPassTexture";
		vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
		vbShadeParams[1].pName = "diffuseMaps";
		vbShadeParams[1].mCount = (uint32_t)gDiffuseMapsPacked.size();
		vbShadeParams[1].ppTextures = gDiffuseMapsPacked.data();
		vbShadeParams[2].pName = "normalMaps";
		vbShadeParams[2].mCount = (uint32_t)gNormalMapsPacked.size();
		vbShadeParams[2].ppTextures = gNormalMapsPacked.data();
		vbShadeParams[3].pName = "vertexPos";
		vbShadeParams[3].ppBuffers = &pScene->m_pIndirectPosBuffer;
		vbShadeParams[4].pName = "vertexTexCoord";
		vbShadeParams[4].ppBuffers = &pScene->m_pIndirectTexCoordBuffer;
		vbShadeParams[5].pName = "vertexNormal";
		vbShadeParams[5].ppBuffers = &pScene->m_pIndirectNormalBuffer;
		vbShadeParams[6].pName = "vertexTangent";
		vbShadeParams[6].ppBuffers = &pScene->m_pIndirectTangentBuffer;
		vbShadeParams[7].pName = "indirectDrawArgs";
		vbShadeParams[7].mCount = gNumGeomSets;
		vbShadeParams[7].ppBuffers = pIndirectBuffers;
		vbShadeParams[8].pName = "objectUniformBlock";
		vbShadeParams[8].ppBuffers = &pBufferMeshTransforms[0][frameIdx];
		vbShadeParams[9].pName = "indirectMaterialBuffer";
		vbShadeParams[9].ppBuffers = &pBufferFilterIndirectMaterial[frameIdx];
		vbShadeParams[10].pName = "filteredIndexBuffer";
		vbShadeParams[10].ppBuffers = &pBufferFilteredIndex[frameIdx][VIEW_CAMERA];
		vbShadeParams[11].pName = "meshConstantsBuffer";
		vbShadeParams[11].ppBuffers = &pBufferMeshConstants;

		vbShadeParams[12].pName = "lightUniformBlock";
		vbShadeParams[12].ppBuffers = &pBufferLightUniform[gFrameIndex];

		vbShadeParams[13].pName = "cameraUniformBlock";
		vbShadeParams[13].ppBuffers = &pBufferCameraUniform[gFrameIndex];

		vbShadeParams[14].pName = "DepthAtlasTexture";
		vbShadeParams[14].ppTextures = &pRenderTargetASMDepthAtlas->pTexture;

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
		vbShadeParams[15].pName = "IndexTexture";
		vbShadeParams[15].mCount = (gs_ASMMaxRefinement + 1) * 2;
		vbShadeParams[15].ppTextures = entireTextureList;

		vbShadeParams[16].pName = "ASMUniformBlock";
		vbShadeParams[16].ppBuffers = &pBufferASMDataUniform[gFrameIndex];

		vbShadeParams[17].pName = "DEMTexture";
		vbShadeParams[17].ppTextures = &pRenderTargetASMDEMAtlas->pTexture;

		vbShadeParams[18].pName = "PrerenderLodClampTexture";
		vbShadeParams[18].ppTextures = &pASM->m_longRangePreRender->
			m_lodClampTexture->pTexture;

		vbShadeParams[19].pName = "renderSettingUniformBlock";
		vbShadeParams[19].ppBuffers = &pBufferRenderSettings[gFrameIndex];

		vbShadeParams[20].pName = "ESMInputConstants";
		vbShadeParams[20].ppBuffers = &pBufferESMUniform[gFrameIndex];

		vbShadeParams[21].pName = "ESMShadowTexture";
		vbShadeParams[21].ppTextures = &esmShadowMap;

		vbShadeParams[22].pName = "SDFShadowTexture";
		vbShadeParams[22].ppTextures = &sdfShadowTexture;

		vbShadeParams[23].pName = "specularMaps";
		vbShadeParams[23].mCount = (uint32_t)gSpecularMaps.size();
		vbShadeParams[23].ppTextures = gSpecularMaps.data();
		
		


		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureVBShade, 24, vbShadeParams);
		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfilerGraphics);
		
		drawSkybox(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}
	
	void prepareASM()
	{
		ASM::TickData tickData;


		ASM::TileIndirectMeshSceneRenderData& indirectMeshRenderData =
			tickData.mTileIndirectMeshSceneRenderData;

		indirectMeshRenderData.m_PScene = pScene;

		indirectMeshRenderData.m_pBufferObjectInfoUniform = pBufferMeshShadowProjectionTransforms[0][gFrameIndex];
		//indirectMeshRenderData.m_pObjectInfoUniformBlock = &gMeshASMProjectionInfoUniformData[0];
		indirectMeshRenderData.mObjectInfoUniformBlock = gMeshASMProjectionInfoUniformData[0];
		indirectMeshRenderData.m_pGraphicsPipeline[GEOMSET_OPAQUE] = pPipelineIndirectDepthPass;
		indirectMeshRenderData.m_pGraphicsPipeline[GEOMSET_ALPHATESTED] = pPipelineIndirectAlphaDepthPass;
		//indirectMeshRenderData.m_pRootSignature[GEOMSET_OPAQUE] = pRootSignatureASMIndirectDepthPass;
		//indirectMeshRenderData.m_pRootSignature[GEOMSET_ALPHATESTED] = pRootSignatureASMIndirectAlphaDepthPass;
		//indirectMeshRenderData.m_pRootSignature = pRootSignatureASMIndirectDepthPass;
		indirectMeshRenderData.m_pRootSignature = pRootSignatureVBPass;

		indirectMeshRenderData.mDrawCount[0] = gPerFrameData.gDrawCount[GEOMSET_OPAQUE];
		indirectMeshRenderData.mDrawCount[1] = gPerFrameData.gDrawCount[GEOMSET_ALPHATESTED];
		//WARNING:
		//indirectMeshRenderData.m_pCmdSignatureVBPass = pCmdSignatureASMDepthPass;
		indirectMeshRenderData.m_pCmdSignatureVBPass = pCmdSignatureVBPass;
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[GEOMSET_OPAQUE] = //pBufferIndirectDrawArgumentsAll[GEOMSET_OPAQUE];
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_OPAQUE][VIEW_SHADOW];
		indirectMeshRenderData.m_pIndirectDrawArgsBuffer[GEOMSET_ALPHATESTED] = //pBufferIndirectDrawArgumentsAll[GEOMSET_ALPHATESTED];
			pBufferFilteredIndirectDrawArguments[gFrameIndex][GEOMSET_ALPHATESTED][VIEW_SHADOW];
		indirectMeshRenderData.m_pIndirectIndexBuffer = pBufferFilteredIndex[gFrameIndex][VIEW_SHADOW];  //gModels[0].m_pIndirectIndexBuffer;
		indirectMeshRenderData.m_pIndirectMaterialBuffer = pBufferFilterIndirectMaterial[gFrameIndex]; // pBufferIndirectMaterialAll;
		indirectMeshRenderData.mDiffuseMaps = &gDiffuseMaps;

		ASM::GenerateLodClampRenderData& generateLodClampRenderData =
			tickData.mGenerateLodClampRenderData;

		generateLodClampRenderData.pBufferLodClampPackedQuadsUniform = pBufferASMLodClampPackedQuadsUniform[gFrameIndex];
		generateLodClampRenderData.m_pGraphicsPipeline = pPipelineASMFillLodClamp;
		generateLodClampRenderData.m_pRootSignature = pRootSignatureASMFillLodClamp;


		ASM::GenerateDEMAtlasToColorRenderData& DEMAtlasToColorRenderData
			= tickData.mDEMAtlasToColorRenderData;

		DEMAtlasToColorRenderData.m_pGraphicsPipeline = pPipelineASMDEMAtlasToColor;
		DEMAtlasToColorRenderData.pBufferASMAtlasToColorPackedQuadsUniform =
			pBufferASMAtlasToColorPackedQuadsUniform[gFrameIndex];
		DEMAtlasToColorRenderData.m_pRootSignature = pRootSignatureASMDEMAtlasToColor;


		ASM::GenerateDEMColorToAtlasRenderData& DEMColorToAtlasRenderData
			= tickData.mDEMColorToAtlasRenderData;

		DEMColorToAtlasRenderData.m_pGraphicsPipeline = pPipelineASMDEMColorToAtlas;
		DEMColorToAtlasRenderData.pBufferASMColorToAtlasPackedQuadsUniform =
			pBufferASMColorToAtlasPackedQuadsUniform[gFrameIndex];
		DEMColorToAtlasRenderData.m_pRootSignature = pRootSignatureASMDEMColorToAtlas;


		ASM::CopyDEMAtlasRenderData& copyDEMAtlasRenderData = tickData.mCopyDEMRenderData;
		copyDEMAtlasRenderData.pBufferASMCopyDEMPackedQuadsUniform = pBufferASMCopyDEMPackedQuadsUniform[gFrameIndex];
		copyDEMAtlasRenderData.m_pGraphicsPipeline = pPipelineASMCopyDEM;
		copyDEMAtlasRenderData.m_pRootSignature = pRootSignatureASMCopyDEM;


		ASM::CopyDepthAtlasRenderData& copyDepthAtlasRenderData = tickData.mCopyDepthRenderData;
		copyDepthAtlasRenderData.pBufferASMAtlasQuadsUniform = pBufferASMAtlasQuadsUniform[gFrameIndex];
		copyDepthAtlasRenderData.m_pGraphicsPipeline = pPipelineASMCopyDepthQuadPass;
		copyDepthAtlasRenderData.m_pRootSignature = pRootSignatureASMCopyDepthQuadPass;


		ASM::IndirectionRenderData& indirectionRenderData = tickData.mIndirectionRenderData;
		for (int i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			indirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[i] =
				pBufferASMPackedIndirectionQuadsUniform[i][gFrameIndex];
		}
		indirectionRenderData.pBufferASMClearIndirectionQuadsUniform = pBufferASMClearIndirectionQuadsUniform[gFrameIndex];
		indirectionRenderData.m_pGraphicsPipeline = pPipelineASMFillIndirection;
		indirectionRenderData.m_pRootSignature = pRootSignatureASMFillIndirection;


		ASM::IndirectionRenderData& prerenderIndirectionRenderData = tickData.mPrerenderIndirectionRenderData;

		for (int i = 0; i < gs_ASMMaxRefinement + 1; ++i)
		{
			prerenderIndirectionRenderData.pBufferASMPackedIndirectionQuadsUniform[i] =
				pBufferASMPackedPrerenderIndirectionQuadsUniform[i][gFrameIndex];
		}
		prerenderIndirectionRenderData.pBufferASMClearIndirectionQuadsUniform = pBufferASMClearIndirectionQuadsUniform[gFrameIndex];
		prerenderIndirectionRenderData.m_pGraphicsPipeline = pPipelineASMFillIndirection;
		prerenderIndirectionRenderData.m_pRootSignature = pRootSignatureASMFillIndirection;


		pASM->Update_Tick_Data(tickData);


		Camera mainViewCamera;
		mainViewCamera.SetViewMatrix(gCameraUniformData.mView);
		mainViewCamera.SetProjection(gCameraUniformData.mProject);
		
		pASM->PrepareRender(mainViewCamera, false);



		const Camera* firstRenderBatchCamera = pASM->m_cache->GetFirstRenderBatchCamera();
		if (firstRenderBatchCamera)
		{
			gPerFrameData.gEyeObjectSpace[VIEW_SHADOW] =
				(firstRenderBatchCamera->GetViewMatrixInverse() * vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();

			gVisibilityBufferConstants.mWorldViewProjMat[VIEW_SHADOW] =
				firstRenderBatchCamera->GetViewProjection() * gMeshInfoUniformData[0].mWorldMat;
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mSampleCount = 1;


			vec2 windowSize = vec2(gs_ASMDepthAtlasTextureWidth, gs_ASMDepthAtlasTextureHeight);
#if defined(DIRECT3D12) || defined(METAL)
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mWindowSize = v2ToF2(windowSize);
#elif defined(VULKAN)
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mWindowSize = windowSize;
#endif
			++gPerFrameData.mValidNumCull;

		}
		gVisibilityBufferConstants.mValidNumCull = gPerFrameData.mValidNumCull;
	}

	void drawASM(Cmd* cmd)
	{
		Camera mainViewCamera;
		mainViewCamera.SetViewMatrix(gCameraUniformData.mView);
		mainViewCamera.SetProjection(gCameraUniformData.mProject);

		
		RendererContext rendererContext;
		rendererContext.m_pCmd = cmd;
		rendererContext.m_pDescriptorBinder = pDescriptorBinder;
		rendererContext.m_pRenderer = pRenderer;
		rendererContext.m_pGpuProfiler = pGpuProfilerGraphics;


		pASM->Render(pRenderTargetASMDepthPass, pRenderTargetASMColorPass, rendererContext,  &mainViewCamera);
	}

	void updateASMUniform()
	{
		gAsmModelUniformBlockData.mIndexTexMat = pASM->m_longRangeShadows->m_indexTexMat;
		gAsmModelUniformBlockData.mPrerenderIndexTexMat = pASM->m_longRangePreRender->m_indexTexMat;
		gAsmModelUniformBlockData.mWarpVector =
			Helper::Vec3_To_Vec4(pASM->m_longRangeShadows->m_receiverWarpVector, 0.0);
		gAsmModelUniformBlockData.mPrerenderWarpVector =
			Helper::Vec3_To_Vec4(pASM->m_longRangePreRender->m_receiverWarpVector, 0.0);
		
		gAsmModelUniformBlockData.mSearchVector =
			Helper::Vec3_To_Vec4(pASM->m_longRangeShadows->m_blockerSearchVector, 0.0);
		gAsmModelUniformBlockData.mPrerenderSearchVector =
			Helper::Vec3_To_Vec4(pASM->m_longRangePreRender->m_blockerSearchVector, 0.0);

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



		cmdResourceBarrier(cmd, 0, NULL, 2, quadBarriers, false);

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

		DescriptorData descriptorData[2] = {};
		descriptorData[0].pName = "screenTexture";
		descriptorData[0].ppTextures = &toDisplayTexture;
		descriptorData[1].pName = "UniformQuadData";
		descriptorData[1].ppBuffers = &pBufferQuadUniform[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureQuad, 2, descriptorData);

		cmdDraw(cmd, 6, 0);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);

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

		if (gCurrentShadowType == SHADOW_TYPE_ASM)
		{
			prepareASM();
		}
		else if (gCurrentShadowType == SHADOW_TYPE_ESM)
		{
			gPerFrameData.gEyeObjectSpace[VIEW_SHADOW] =
				(inverse(gLightUniformData.mLightViewProj) * vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();

			gVisibilityBufferConstants.mWorldViewProjMat[VIEW_SHADOW] =
				gLightUniformData.mLightViewProj * gMeshInfoUniformData[0].mWorldMat;
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mSampleCount = 1;

			vec2 windowSize(ESM_SHADOWMAP_RES, ESM_SHADOWMAP_RES);
#if defined(DIRECT3D12) || defined(METAL)
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mWindowSize = v2ToF2(windowSize);
#elif defined(VULKAN)
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW].mWindowSize = windowSize;
#endif
			++gPerFrameData.mValidNumCull;
			gVisibilityBufferConstants.mValidNumCull = gPerFrameData.mValidNumCull;
		}
		else if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF)
		{
			gPerFrameData.gEyeObjectSpace[VIEW_SHADOW] = gPerFrameData.gEyeObjectSpace[VIEW_CAMERA];

			gVisibilityBufferConstants.mWorldViewProjMat[VIEW_SHADOW] =
				gVisibilityBufferConstants.mWorldViewProjMat[VIEW_CAMERA];
			gVisibilityBufferConstants.mCullingViewports[VIEW_SHADOW] = gVisibilityBufferConstants.mCullingViewports[VIEW_CAMERA];
			++gPerFrameData.mValidNumCull;
		}

		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/

		BufferUpdateDesc renderSettingCbv = { pBufferRenderSettings[gFrameIndex], &gRenderSettings };
		updateResource(&renderSettingCbv);


		for (uint32_t j = 0; j < MESH_COUNT; ++j)
		{
			BufferUpdateDesc viewProjCbv = { pBufferMeshTransforms[j][gFrameIndex], gMeshInfoUniformData + j };
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
				pBufferVisibilityBufferConstants[gFrameIndex], &gVisibilityBufferConstants };

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
					pBufferVisibilityBufferConstants[gFrameIndex], &gVisibilityBufferConstants };

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
			cmdResourceBarrier(cmd, index, barriers2, 0, NULL, true);
			cmdFlushBarriers(cmd);
			

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
				cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
				cmdFlushBarriers(cmd);
				drawEsmShadowMap(cmd);
				barriers.clear();
				barriers.emplace_back(TextureBarrier{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
				cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
				cmdFlushBarriers(cmd);
			}
			// Draw To Screen
			barriers.clear();
			barriers.emplace_back(TextureBarrier{ pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET });
			barriers.emplace_back(TextureBarrier{ pRenderTargetASMDepthPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
			barriers.emplace_back(TextureBarrier{ pRenderTargetASMDepthAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE });

			for (int i = 0; i <= gs_ASMMaxRefinement; ++i)
			{
				barriers.emplace_back(TextureBarrier{ pRenderTargetASMIndirection[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
				barriers.emplace_back(TextureBarrier{ pRenderTargetASMPrerenderIndirection[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
			}

			barriers.emplace_back(TextureBarrier{ pASM->m_longRangePreRender->m_lodClampTexture->pTexture, RESOURCE_STATE_SHADER_RESOURCE });

			barriers.emplace_back(TextureBarrier{
				pRenderTargetASMDEMAtlas->pTexture, RESOURCE_STATE_SHADER_RESOURCE });


			cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
			cmdFlushBarriers(cmd);

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
			
			if (gCurrentShadowType == SHADOW_TYPE_MESH_BAKED_SDF && gBakedSDFMeshSettings.mDrawSDFMeshVisualization)
			{
				presentImage(cmd, pRenderTargetSDFMeshVisualization->pTexture, 
					pSwapChain->ppSwapchainRenderTargets[gFrameIndex]);
			}
			else
			{
				presentImage(cmd, pRenderTargetScreen->pTexture, 
					pSwapChain->ppSwapchainRenderTargets[gFrameIndex]);
			}


			drawGUI(cmd, gFrameIndex);

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

		if (gAppSettings.mActivateMicroProfiler)
		{
			cmdDrawProfiler(cmd);
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

	void presentImage(Cmd* const cmd, Texture* pSrc, RenderTarget* pDstCol)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfilerGraphics, "Present Image", true);

		TextureBarrier barrier[] = { { pSrc, RESOURCE_STATE_SHADER_RESOURCE }, 
		 { pDstCol->pTexture, RESOURCE_STATE_RENDER_TARGET } };

		cmdResourceBarrier(cmd, 0, NULL, 2, barrier, true);
		cmdFlushBarriers(cmd);

		cmdBindRenderTargets(cmd, 1, &pDstCol, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDstCol->mDesc.mWidth, (float)pDstCol->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDstCol->mDesc.mWidth, pDstCol->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelinePresentPass);
		DescriptorData params[2] = {};
		params[0].pName = "SourceTexture";
		params[0].ppTextures = &pSrc;
		params[1].pName = "repeatBillinearSampler";
		params[1].ppSamplers = &pSamplerLinearRepeat;

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignaturePresentPass, 2, params);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier barrierPresent = { pDstCol->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrierPresent, true);

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
		swapChainDesc.mColorFormat = ImageFormat::BGRA8;
		swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };
		swapChainDesc.mSrgb = false;

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
#define STENCIL_SDF_OPTIMIZATION
#ifdef STENCIL_SDF_OPTIMIZATION
		// When using stencil optimization, use a packed depth-stencil format.
		// However mobile doesn't support D32S8, while AMD does not support D24S8 under Vulkan, etc.
		if (isImageFormatSupported(ImageFormat::D32S8))
			depthRT.mFormat = ImageFormat::D32S8;
		else if (isImageFormatSupported(ImageFormat::D24S8))
			depthRT.mFormat = ImageFormat::D24S8;
		else if (isImageFormatSupported(ImageFormat::X8D24PAX32))
			depthRT.mFormat = ImageFormat::X8D24PAX32;
		else if (isImageFormatSupported(ImageFormat::D16S8))
			depthRT.mFormat = ImageFormat::D16S8;
		else
			ASSERT(false); // no supported packed depth stencil format to use
#else
		depthRT.mFormat = ImageFormat::D32F;
#endif
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
		postProcRTDesc.mFormat = ImageFormat::RGBA8;
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
		shadowRTDesc.mFormat = ImageFormat::D32F;
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
		sdfMeshVisualizationRTDesc.mFormat = ImageFormat::RGBA32F;
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
		sdfMeshShadowRTDesc.mFormat = ImageFormat::RG32F;
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
		upSampleSDFShadowRTDesc.mFormat = ImageFormat::R16F;
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
		ASMDepthPassRT.mFormat = ImageFormat::D32F;
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
		ASMColorPassRT.mFormat = ImageFormat::R32F;
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
		vbRTDesc.mFormat = ImageFormat::RGBA8;
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
		depthAtlasRTDesc.mFormat = ImageFormat::R32F;
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
		DEMAtlasRTDesc.mFormat = ImageFormat::R32F;
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
		indirectionRTDesc.mFormat = ImageFormat::RGBA32F;
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

		for (int i = 0; i <= gs_ASMMaxRefinement; ++i)
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
		lodClampRTDesc.mFormat = ImageFormat::R16F;
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

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0)) const
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > maxDistance * maxDistance)
		{
			d *= maxDistance / sqrtf(lenSqr);
		}

		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
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

	static bool wasMicroProfileActivated = gAppSettings.mActivateMicroProfiler;
	if (wasMicroProfileActivated != gAppSettings.mActivateMicroProfiler)
	{
		wasMicroProfileActivated = gAppSettings.mActivateMicroProfiler;

		// ProfileSetDisplayMode()
		// TODO: need to change this better way 

		Profile& S = *ProfileGet();
		int nValue = wasMicroProfileActivated ? 1 : 0;
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

	CheckboxWidget microprofile("Activate Microprofile", &gAppSettings.mActivateMicroProfiler);
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

DEFINE_APPLICATION_MAIN(LightShadowPlayground)
