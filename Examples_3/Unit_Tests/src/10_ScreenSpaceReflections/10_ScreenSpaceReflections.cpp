/*
*
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

// Unit Test for testing materials and pbr.

//tiny stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Utilities/Threading/Atomics.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

//Input
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#include "samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_2spp.cpp"

#include "Shaders/Shared.h"

#define DEFERRED_RT_COUNT 4
#define MAX_PLANES 4



#define FOREACH_SETTING(X) \
		X(UseTextureFallback)

#define GENERATE_ENUM(x) x,
#define GENERATE_STRING(x) #x,
#define GENERATE_STRUCT(x) uint32_t m##x = 0;

typedef enum ESettings
{
	FOREACH_SETTING(GENERATE_ENUM)
	Count
} ESettings;

const char* gSettingNames[] =
{
	FOREACH_SETTING(GENERATE_STRING)
};

// Useful for using names directly instead of subscripting an array
struct AppSettings
{
	FOREACH_SETTING(GENERATE_STRUCT)
} gSettings;

// Have a uniform for camera data
struct UniformCamData
{
	CameraMatrix mProjectView;
    CameraMatrix mPrevProjectView;
	vec3 mCamPos;
};

// Have a uniform for extended camera data
struct UniformExtendedCamData
{
	mat4 mViewMat;
    CameraMatrix mProjMat;
    CameraMatrix mViewProjMat;
    CameraMatrix mInvViewProjMat;

	vec4 mCameraWorldPos;
	vec4 mViewPortSize;
};

// Have a uniform for PPR properties
struct UniformPPRProData
{
	uint  renderMode;
	float useHolePatching;
	float useExpensiveHolePatching;
	float useNormalMap;

	float intensity;
	float useFadeEffect;
	float padding01;
	float padding02;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4  mWorldMat;
	mat4  mPrevWorldMat;
	float mRoughness = 0.04f;
	float mMetallic = 0.0f;
	int   pbrMaterials = -1;
};

struct Light
{
	vec4  mPos;
	vec4  mCol;
	float mRadius;
	float mIntensity;
	char  _pad[8];
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present
	Light mLights[16] = {};    // array of lights seem to be broken so just a single light for now
	int   mCurrAmountOfLights = 0;
};

struct DirectionalLight
{
	vec4 mPos;
	vec4 mCol;    //alpha is the intesity
	vec4 mDir;
};

struct UniformDirectionalLightData
{
	// Used to tell our shaders how many lights are currently present
	DirectionalLight mLights[16];    // array of lights seem to be broken so just a single light for now
	int              mCurrAmountOfDLights = 0;
};

struct PlaneInfo
{
	mat4 rotMat;
	vec4 centerPoint;
	vec4 size;
};

struct UniformPlaneInfoData
{
	PlaneInfo planeInfo[MAX_PLANES];
	uint32_t  numPlanes;
	uint32_t  pad00;
	uint32_t  pad01;
	uint32_t  pad02;
};

struct UniformSSSRConstantsData
{
    CameraMatrix g_inv_view_proj;
    CameraMatrix g_proj;
    CameraMatrix g_inv_proj;
	mat4 g_view;
	mat4 g_inv_view;
    CameraMatrix g_prev_view_proj;

	uint32_t g_frame_index;
	uint32_t g_max_traversal_intersections;
	uint32_t g_min_traversal_occupancy;
	uint32_t g_most_detailed_mip;
	float    g_temporal_stability_factor;
	float    g_depth_buffer_thickness;
	uint32_t g_samples_per_quad;
	uint32_t g_temporal_variance_guided_tracing_enabled;
	float    g_roughness_threshold;
	uint32_t g_skip_denoiser;
};

enum
{
	SCENE_ONLY = 0,
	REFLECTIONS_ONLY = 1,
	SCENE_WITH_REFLECTIONS = 2,
	SCENE_EXCLU_REFLECTIONS = 3,
};

enum
{
	PP_REFLECTION = 0,
	SSS_REFLECTION = 1,
};

static bool gUseHolePatching = true;
static bool gUseExpensiveHolePatching = true;

static bool gUseNormalMap = false;
static bool gUseFadeEffect = true;

static uint32_t gRenderMode = SCENE_WITH_REFLECTIONS;
static uint32_t gReflectionType = SSS_REFLECTION;
static uint32_t gLastReflectionType = gReflectionType;

static uint32_t gPlaneNumber = 1;
static float    gPlaneSize = 75.0f;
static float    gRRP_Intensity = 0.2f;

static bool gUseSPD = true;

static uint32_t gSSSR_MaxTravelsalIntersections = 128;
static uint32_t gSSSR_MinTravelsalOccupancy = 4;
static uint32_t gSSSR_MostDetailedMip = 1;
static float    pSSSR_TemporalStability = 0.99f;
static float    gSSSR_DepthThickness = 0.15f;
static int32_t  gSSSR_SamplesPerQuad = 1;
static int32_t  gSSSR_EAWPassCount = 1;
static bool     gSSSR_TemporalVarianceEnabled = true;
static float    gSSSR_RougnessThreshold = 0.1f;
static bool     gSSSR_SkipDenoiser = false;

bool gSSSRSupported = false;

const char* pMaterialImageFileNames[] = {
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/ao",

	//common
	"SponzaPBR_Textures/ao",
	"SponzaPBR_Textures/Dielectric_metallic",
	"SponzaPBR_Textures/Metallic_metallic",
	"SponzaPBR_Textures/gi_flag",

	//Background
	"SponzaPBR_Textures/Background/Background_Albedo",
	"SponzaPBR_Textures/Background/Background_Normal",
	"SponzaPBR_Textures/Background/Background_Roughness",

	//ChainTexture
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Albedo",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Metallic",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Normal",
	"SponzaPBR_Textures/ChainTexture/ChainTexture_Roughness",

	//Lion
	"SponzaPBR_Textures/Lion/Lion_Albedo",
	"SponzaPBR_Textures/Lion/Lion_Normal",
	"SponzaPBR_Textures/Lion/Lion_Roughness",

	//Sponza_Arch
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_diffuse",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_normal",
	"SponzaPBR_Textures/Sponza_Arch/Sponza_Arch_roughness",

	//Sponza_Bricks
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Albedo",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Normal",
	"SponzaPBR_Textures/Sponza_Bricks/Sponza_Bricks_a_Roughness",

	//Sponza_Ceiling
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_diffuse",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_normal",
	"SponzaPBR_Textures/Sponza_Ceiling/Sponza_Ceiling_roughness",

	//Sponza_Column
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_a_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_b_roughness",

	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_diffuse",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_normal",
	"SponzaPBR_Textures/Sponza_Column/Sponza_Column_c_roughness",

	//Sponza_Curtain
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Blue_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Green_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_Red_normal",

	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_metallic",
	"SponzaPBR_Textures/Sponza_Curtain/Sponza_Curtain_roughness",

	//Sponza_Details
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_diffuse",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_metallic",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_normal",
	"SponzaPBR_Textures/Sponza_Details/Sponza_Details_roughness",

	//Sponza_Fabric
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Blue_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Green_normal",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_metallic",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_roughness",

	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_diffuse",
	"SponzaPBR_Textures/Sponza_Fabric/Sponza_Fabric_Red_normal",

	//Sponza_FlagPole
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_diffuse",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_normal",
	"SponzaPBR_Textures/Sponza_FlagPole/Sponza_FlagPole_roughness",

	//Sponza_Floor
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_diffuse",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_normal",
	"SponzaPBR_Textures/Sponza_Floor/Sponza_Floor_roughness",

	//Sponza_Roof
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_diffuse",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_normal",
	"SponzaPBR_Textures/Sponza_Roof/Sponza_Roof_roughness",

	//Sponza_Thorn
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_diffuse",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_normal",
	"SponzaPBR_Textures/Sponza_Thorn/Sponza_Thorn_roughness",

	//Vase
	"SponzaPBR_Textures/Vase/Vase_diffuse",
	"SponzaPBR_Textures/Vase/Vase_normal",
	"SponzaPBR_Textures/Vase/Vase_roughness",

	//VaseHanging
	"SponzaPBR_Textures/VaseHanging/VaseHanging_diffuse",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_normal",
	"SponzaPBR_Textures/VaseHanging/VaseHanging_roughness",

	//VasePlant
	"SponzaPBR_Textures/VasePlant/VasePlant_diffuse",
	"SponzaPBR_Textures/VasePlant/VasePlant_normal",
	"SponzaPBR_Textures/VasePlant/VasePlant_roughness",

	//VaseRound
	"SponzaPBR_Textures/VaseRound/VaseRound_diffuse",
	"SponzaPBR_Textures/VaseRound/VaseRound_normal",
	"SponzaPBR_Textures/VaseRound/VaseRound_roughness",

	"lion/lion_albedo",
	"lion/lion_specular",
	"lion/lion_normal",

};

const uint32_t gImageCount = 3;
ProfileToken   gPPRGpuProfileToken;
ProfileToken   gSSSRGpuProfileToken;
ProfileToken   gCurrentGpuProfileToken;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain* pSwapChain = NULL;

RenderTarget* pRenderTargetDeferredPass[2][DEFERRED_RT_COUNT] = { { NULL }, { NULL } };

RenderTarget* pSceneBuffer = NULL;
RenderTarget* pReflectionBuffer = NULL;

RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*        pShaderBRDF = NULL;
Pipeline*      pPipelineBRDF = NULL;
RootSignature* pRootSigBRDF = NULL;
DescriptorSet* pDescriptorSetBRDF[2] = { NULL };

Buffer*        pSkyboxVertexBuffer = NULL;
Shader*        pSkyboxShader = NULL;
Pipeline*      pSkyboxPipeline = NULL;
Pipeline*      pSkyboxWithClearTexturesPipeline = NULL;
RootSignature* pSkyboxRootSignature = NULL;
DescriptorSet* pDescriptorSetSkybox[2] = { NULL };

Shader*        pPPR_ProjectionShader = NULL;
RootSignature* pPPR_ProjectionRootSignature = NULL;
Pipeline*      pPPR_ProjectionPipeline = NULL;
DescriptorSet* pDescriptorSetPPR_Projection[2] = { NULL };

Shader*        pPPR_ReflectionShader = NULL;
RootSignature* pPPR_ReflectionRootSignature = NULL;
Pipeline*      pPPR_ReflectionPipeline = NULL;
DescriptorSet* pDescriptorSetPPR_Reflection[2] = { NULL };

Shader*        pPPR_HolePatchingShader = NULL;
RootSignature* pPPR_HolePatchingRootSignature = NULL;
Pipeline*      pPPR_HolePatchingPipeline = NULL;
DescriptorSet* pDescriptorSetPPR__HolePatching[2] = { NULL };

Shader*        pCopyDepthShader = NULL;
RootSignature* pCopyDepthRootSignature = NULL;
Pipeline*      pCopyDepthPipeline = NULL;
DescriptorSet* pDescriptorCopyDepth = NULL;

Shader*        pGenerateMipShader = NULL;
RootSignature* pGenerateMipRootSignature = NULL;
Pipeline*      pGenerateMipPipeline = NULL;
DescriptorSet* pDescriptorGenerateMip = NULL;
uint32_t       gMipSizeRootConstantIndex = 0;

Shader*        pSPDShader = NULL;
RootSignature* pSPDRootSignature = NULL;
Pipeline*      pSPDPipeline = NULL;
DescriptorSet* pDescriptorSPD = NULL;

Shader*        pSSSR_ClassifyTilesShader = NULL;
RootSignature* pSSSR_ClassifyTilesRootSignature = NULL;
Pipeline*      pSSSR_ClassifyTilesPipeline = NULL;
DescriptorSet* pDescriptorSetSSSR_ClassifyTiles = NULL;

Shader*        pSSSR_PrepareIndirectArgsShader = NULL;
RootSignature* pSSSR_PrepareIndirectArgsRootSignature = NULL;
Pipeline*      pSSSR_PrepareIndirectArgsPipeline = NULL;
DescriptorSet* pDescriptorSetSSSR_PrepareIndirectArgs = NULL;

Shader*           pSSSR_IntersectShader = NULL;
RootSignature*    pSSSR_IntersectRootSignature = NULL;
Pipeline*         pSSSR_IntersectPipeline = NULL;
CommandSignature* pSSSR_IntersectCommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_Intersect = NULL;

Shader*           pSSSR_ResolveSpatialShader = NULL;
RootSignature*    pSSSR_ResolveSpatialRootSignature = NULL;
Pipeline*         pSSSR_ResolveSpatialPipeline = NULL;
CommandSignature* pSSSR_ResolveSpatialCommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_ResolveSpatial = NULL;

Shader*           pSSSR_ResolveTemporalShader = NULL;
RootSignature*    pSSSR_ResolveTemporalRootSignature = NULL;
Pipeline*         pSSSR_ResolveTemporalPipeline = NULL;
CommandSignature* pSSSR_ResolveTemporalCommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_ResolveTemporal = NULL;

Shader*           pSSSR_ResolveEAWShader = NULL;
RootSignature*    pSSSR_ResolveEAWRootSignature = NULL;
Pipeline*         pSSSR_ResolveEAWPipeline = NULL;
CommandSignature* pSSSR_ResolveEAWCommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_ResolveEAW = NULL;

Shader*           pSSSR_ResolveEAWStride2Shader = NULL;
RootSignature*    pSSSR_ResolveEAWStride2RootSignature = NULL;
Pipeline*         pSSSR_ResolveEAWStride2Pipeline = NULL;
CommandSignature* pSSSR_ResolveEAWStride2CommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_ResolveEAWStride2 = NULL;

Shader*           pSSSR_ResolveEAWStride4Shader = NULL;
RootSignature*    pSSSR_ResolveEAWStride4RootSignature = NULL;
Pipeline*         pSSSR_ResolveEAWStride4Pipeline = NULL;
CommandSignature* pSSSR_ResolveEAWStride4CommandSignature = NULL;
DescriptorSet*    pDescriptorSetSSSR_ResolveEAWStride4 = NULL;

Buffer*                  pSSSR_ConstantsBuffer[gImageCount] = { NULL };
UniformSSSRConstantsData gUniformSSSRConstantsData;

Buffer* pSPD_AtomicCounterBuffer = NULL;

Buffer*  pSSSR_RayListBuffer = NULL;
Buffer*  pSSSR_TileListBuffer = NULL;
Buffer*  pSSSR_RayCounterBuffer = NULL;
Buffer*  pSSSR_TileCounterBuffer = NULL;
Buffer*  pSSSR_IntersectArgsBuffer = NULL;
Buffer*  pSSSR_DenoiserArgsBuffer = NULL;
Buffer*  pSSSR_SobolBuffer = NULL;
Buffer*  pSSSR_RankingTileBuffer = NULL;
Buffer*  pSSSR_ScramblingTileBuffer = NULL;
RenderTarget* pSSSR_TemporalResults[2] = { NULL };
Texture* pSSSR_TemporalVariance = NULL;
RenderTarget* pSSSR_RayLength = NULL;
Texture* pSSSR_DepthHierarchy = NULL;

Buffer* pScreenQuadVertexBuffer = NULL;

Shader*        pShaderGbuffers = NULL;
Pipeline*      pPipelineGbuffers = NULL;
RootSignature* pRootSigGbuffers = NULL;
DescriptorSet* pDescriptorSetGbuffers[3] = { NULL };
uint32_t       gMapIDRootConstantIndex = 0;

Texture* pSkybox = NULL;
Texture* pBRDFIntegrationMap = NULL;
Texture* pIrradianceMap = NULL;
Texture* pSpecularMap = NULL;

Buffer* pIntermediateBuffer = NULL;

Texture* pMaterialTextures[TOTAL_IMGS];

eastl::vector<int> gSponzaTextureIndexforMaterial;

//For clearing Intermediate Buffer
eastl::vector<uint32_t> gInitializeVal;

UniformObjData gUniformDataMVP;

/************************************************************************/
// Vertex buffers for the model
/************************************************************************/

enum
{
	SPONZA_MODEL,
	LION_MODEL,
	MODEL_COUNT
};

Buffer* pSponzaBuffer;
Buffer* pLionBuffer;

Buffer*        pBufferUniformCamera[gImageCount] = { NULL };
UniformCamData gUniformDataCamera;

UniformCamData gUniformDataSky;

Buffer*                pBufferUniformExtendedCamera[gImageCount] = { NULL };
UniformExtendedCamData gUniformDataExtenedCamera;

Buffer* pBufferUniformCameraSky[gImageCount] = { NULL };

Buffer*           pBufferUniformPPRPro[gImageCount] = { NULL };
UniformPPRProData gUniformPPRProData;

Buffer*          pBufferUniformLights = NULL;
UniformLightData gUniformDataLights;

Buffer*                     pBufferUniformDirectionalLights = NULL;
UniformDirectionalLightData gUniformDataDirectionalLights;

Buffer*              pBufferUniformPlaneInfo[gImageCount] = { NULL };
UniformPlaneInfoData gUniformDataPlaneInfo;

Shader*   pShaderPostProc = NULL;
Pipeline* pPipelinePostProc = NULL;

Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerLinear = NULL;

Sampler* pSamplerNearest = NULL;

uint32_t gFrameIndex = 0;
uint32_t gFrameFlipFlop = 0;

eastl::vector<Buffer*> gSphereBuffers;

ICameraController* pCameraController = NULL;

FontDrawDesc gFrameTimeDraw; 
uint32_t     gFontID = 0; 

UIComponent*    pGui = NULL;
DynamicUIWidgets PPR_Widgets;
DynamicUIWidgets SSSR_Widgets;

SyncToken gResourceSyncStartToken = {};
SyncToken gResourceSyncToken = {};

const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

const char* gModelNames[2] = { "Sponza.gltf", "lion.gltf" };
Geometry*   gModels[2];
uint32_t    gMaterialIds[] = {
    0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10, 5, 7,  5, 6, 7,  6, 7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,
    5,  6,  5,  11, 5,  11, 5,  11, 5,  10, 5,  9, 8,  6, 12, 2, 5, 13, 0, 14, 15, 16, 14, 15, 14, 16, 15, 13, 17, 18, 19, 18, 19, 18, 17,
    19, 18, 17, 20, 21, 20, 21, 20, 21, 20, 21, 3, 1,  3, 1,  3, 1, 3,  1, 3,  1,  3,  1,  3,  1,  22, 23, 4,  23, 4,  5,  24, 5,
};

VertexLayout gVertexLayoutModel = {};

void assignSponzaTextures();

bool gHasReset = false;
const char* gTestScripts[] = { "Test_RenderScene.lua", "Test_RenderReflections.lua", "Test_RenderSceneReflections.lua", "Test_RenderSceneExReflections.lua" };
uint32_t gScriptIndexes[] = { 0, 1, 2, 3, 4 };
uint32_t gCurrentScriptIndex = 0;
void RunScript(void* pUserData)
{
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

bool gClearTextures = false;

class ScreenSpaceReflections: public IApp
{
	size_t mProgressBarValue = 0, mProgressBarValueMax = 1024;
	//size_t mAtomicProgress = 0;

	public:
	ScreenSpaceReflections() //-V832
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		ExtendedSettings extendedSettings = {};
		extendedSettings.numSettings = ESettings::Count;
		extendedSettings.pSettings = (uint32_t*)&gSettings;
		extendedSettings.ppSettingNames = gSettingNames;

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.pExtendedSettings = &extendedSettings;
		settings.mShaderTarget = shader_target_6_0;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		gSSSRSupported = (pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_BASIC_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_VOTE_BIT);

		gLastReflectionType = gReflectionType = gSSSRSupported ? SSS_REFLECTION : PP_REFLECTION;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create command pool and create a cmd buffer for each swapchain image
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		if (!gHasReset)
		{
			const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
			LuaScriptDesc scriptDescs[numScripts] = {};
			for (uint32_t i = 0; i < numScripts; ++i)
				scriptDescs[i].pScriptFileName = gTestScripts[i];
			luaDefineScripts(scriptDescs, numScripts);
		}

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
		uiCreateComponent("Screen Space Reflections", &guiDesc, &pGui);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gPPRGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
		gSSSRGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
		gCurrentGpuProfileToken = gSSSRGpuProfileToken;

		ComputePBRMaps();

		SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
					ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);

		SamplerDesc nearstSamplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
						ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &nearstSamplerDesc, &pSamplerNearest);

		BufferLoadDesc sponza_buffDesc = {};
		sponza_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sponza_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sponza_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		sponza_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sponza_buffDesc.pData = NULL;
		sponza_buffDesc.ppBuffer = &pSponzaBuffer;
		addResource(&sponza_buffDesc, NULL);

		BufferLoadDesc lion_buffDesc = {};
		lion_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lion_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lion_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		lion_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lion_buffDesc.pData = NULL;
		lion_buffDesc.ppBuffer = &pLionBuffer;
		addResource(&lion_buffDesc, NULL);

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
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
		skyboxVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc, NULL);

		float screenQuadPoints[] = {
			-1.0f, 3.0f, 0.5f, 0.0f, -1.0f, -1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		uint64_t       screenQuadDataSize = 5 * 3 * sizeof(float);
		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		screenQuadVbDesc.mDesc.mSize = screenQuadDataSize;
		screenQuadVbDesc.pData = screenQuadPoints;
		screenQuadVbDesc.ppBuffer = &pScreenQuadVertexBuffer;
		addResource(&screenQuadVbDesc, NULL);

		// Uniform buffer for camera data
		BufferLoadDesc ubCamDesc = {};
		ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
		ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubCamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubCamDesc.ppBuffer = &pBufferUniformCamera[i];
			addResource(&ubCamDesc, NULL);
			ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
			addResource(&ubCamDesc, NULL);
		}

		// Uniform buffer for extended camera data
		BufferLoadDesc ubECamDesc = {};
		ubECamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubECamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubECamDesc.mDesc.mSize = sizeof(UniformExtendedCamData);
		ubECamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubECamDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubECamDesc.ppBuffer = &pBufferUniformExtendedCamera[i];
			addResource(&ubECamDesc, NULL);
		}

		// Uniform buffer for PPR's properties
		BufferLoadDesc ubPPR_ProDesc = {};
		ubPPR_ProDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubPPR_ProDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubPPR_ProDesc.mDesc.mSize = sizeof(UniformPPRProData);
		ubPPR_ProDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubPPR_ProDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubPPR_ProDesc.ppBuffer = &pBufferUniformPPRPro[i];
			addResource(&ubPPR_ProDesc, NULL);
		}

		uint32_t zero = 0;

		BufferLoadDesc SPD_AtomicCounterDesc = {};
		SPD_AtomicCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		SPD_AtomicCounterDesc.mDesc.mElementCount = 1;
		SPD_AtomicCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
		SPD_AtomicCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SPD_AtomicCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		SPD_AtomicCounterDesc.mDesc.mSize = SPD_AtomicCounterDesc.mDesc.mStructStride * SPD_AtomicCounterDesc.mDesc.mElementCount;
		SPD_AtomicCounterDesc.mDesc.pName = "SPD_AtomicCounterBuffer";
		SPD_AtomicCounterDesc.pData = &zero;
		SPD_AtomicCounterDesc.ppBuffer = &pSPD_AtomicCounterBuffer;
		addResource(&SPD_AtomicCounterDesc, NULL);

		// SSSR
		BufferLoadDesc ubSSSR_ConstDesc = {};
		ubSSSR_ConstDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubSSSR_ConstDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubSSSR_ConstDesc.mDesc.mSize = sizeof(UniformSSSRConstantsData);
		ubSSSR_ConstDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubSSSR_ConstDesc.mDesc.pName = "pSSSR_ConstantsBuffer";
		ubSSSR_ConstDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubSSSR_ConstDesc.ppBuffer = &pSSSR_ConstantsBuffer[i];
			addResource(&ubSSSR_ConstDesc, NULL);
		}

		BufferLoadDesc SSSR_RayCounterDesc = {};
		SSSR_RayCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		SSSR_RayCounterDesc.mDesc.mElementCount = 1;
		SSSR_RayCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_RayCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_RayCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_RayCounterDesc.mDesc.mSize = SSSR_RayCounterDesc.mDesc.mStructStride * SSSR_RayCounterDesc.mDesc.mElementCount;
		SSSR_RayCounterDesc.mDesc.pName = "SSSR_RayCounterBuffer";
		SSSR_RayCounterDesc.pData = &zero;
		SSSR_RayCounterDesc.ppBuffer = &pSSSR_RayCounterBuffer;
		addResource(&SSSR_RayCounterDesc, NULL);

		BufferLoadDesc SSSR_TileCounterDesc = {};
		SSSR_TileCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		SSSR_TileCounterDesc.mDesc.mElementCount = 1;
		SSSR_TileCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_TileCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_TileCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_TileCounterDesc.mDesc.mSize = SSSR_TileCounterDesc.mDesc.mStructStride * SSSR_TileCounterDesc.mDesc.mElementCount;
		SSSR_TileCounterDesc.mDesc.pName = "SSSR_TileCounterBuffer";
		SSSR_TileCounterDesc.pData = &zero;
		SSSR_TileCounterDesc.ppBuffer = &pSSSR_TileCounterBuffer;
		addResource(&SSSR_TileCounterDesc, NULL);

		BufferLoadDesc SSSR_IntersectArgsDesc = {};
		SSSR_IntersectArgsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		SSSR_IntersectArgsDesc.mDesc.mElementCount = 3;
		SSSR_IntersectArgsDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_IntersectArgsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_IntersectArgsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_IntersectArgsDesc.mDesc.mStartState = RESOURCE_STATE_INDIRECT_ARGUMENT;
		SSSR_IntersectArgsDesc.mDesc.mSize = SSSR_IntersectArgsDesc.mDesc.mStructStride * SSSR_IntersectArgsDesc.mDesc.mElementCount;
		SSSR_IntersectArgsDesc.mDesc.pName = "SSSR_IntersectArgsBuffer";
		SSSR_IntersectArgsDesc.pData = NULL;
		SSSR_IntersectArgsDesc.ppBuffer = &pSSSR_IntersectArgsBuffer;
		addResource(&SSSR_IntersectArgsDesc, NULL);

		BufferLoadDesc SSSR_DenoiserArgsDesc = {};
		SSSR_DenoiserArgsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
		SSSR_DenoiserArgsDesc.mDesc.mElementCount = 3;
		SSSR_DenoiserArgsDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_DenoiserArgsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_DenoiserArgsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_DenoiserArgsDesc.mDesc.mStartState = RESOURCE_STATE_INDIRECT_ARGUMENT;
		SSSR_DenoiserArgsDesc.mDesc.mSize = SSSR_DenoiserArgsDesc.mDesc.mStructStride * SSSR_DenoiserArgsDesc.mDesc.mElementCount;
		SSSR_DenoiserArgsDesc.mDesc.pName = "SSSR_DenoiserArgsBuffer";
		SSSR_DenoiserArgsDesc.pData = NULL;
		SSSR_DenoiserArgsDesc.ppBuffer = &pSSSR_DenoiserArgsBuffer;
		addResource(&SSSR_DenoiserArgsDesc, NULL);

		BufferLoadDesc sobolDesc = {};
		sobolDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		sobolDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sobolDesc.mDesc.mStructStride = sizeof(sobol_256spp_256d[0]);
		sobolDesc.mDesc.mSize = sizeof(sobol_256spp_256d);
		sobolDesc.mDesc.mElementCount = sobolDesc.mDesc.mSize / sobolDesc.mDesc.mStructStride;
		sobolDesc.mDesc.pName = "SSSR_SobolBuffer";
		sobolDesc.pData = sobol_256spp_256d;
		sobolDesc.ppBuffer = &pSSSR_SobolBuffer;
		addResource(&sobolDesc, NULL);

		BufferLoadDesc rankingTileDesc = {};
		rankingTileDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		rankingTileDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		rankingTileDesc.mDesc.mStructStride = sizeof(rankingTile[0]);
		rankingTileDesc.mDesc.mSize = sizeof(rankingTile);
		rankingTileDesc.mDesc.mElementCount = rankingTileDesc.mDesc.mSize / rankingTileDesc.mDesc.mStructStride;
		rankingTileDesc.mDesc.pName = "SSSR_RankingTileBuffer";
		rankingTileDesc.pData = rankingTile;
		rankingTileDesc.ppBuffer = &pSSSR_RankingTileBuffer;
		addResource(&rankingTileDesc, NULL);

		BufferLoadDesc scramblingTileDesc = {};
		scramblingTileDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		scramblingTileDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		scramblingTileDesc.mDesc.mStructStride = sizeof(scramblingTile[0]);
		scramblingTileDesc.mDesc.mSize = sizeof(scramblingTile);
		scramblingTileDesc.mDesc.mElementCount = scramblingTileDesc.mDesc.mSize / scramblingTileDesc.mDesc.mStructStride;
		scramblingTileDesc.mDesc.pName = "SSSR_ScramblingTileBuffer";
		scramblingTileDesc.pData = scramblingTile;
		scramblingTileDesc.ppBuffer = &pSSSR_ScramblingTileBuffer;
		addResource(&scramblingTileDesc, NULL);

		// Uniform buffer for light data
		BufferLoadDesc ubLightsDesc = {};
		ubLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubLightsDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
		ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
		ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubLightsDesc.pData = NULL;
		ubLightsDesc.ppBuffer = &pBufferUniformLights;
		addResource(&ubLightsDesc, NULL);

		// Uniform buffer for DirectionalLight data
		BufferLoadDesc ubDLightsDesc = {};
		ubDLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubDLightsDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
		ubDLightsDesc.mDesc.mSize = sizeof(UniformDirectionalLightData);
		ubDLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubDLightsDesc.pData = NULL;
		ubDLightsDesc.ppBuffer = &pBufferUniformDirectionalLights;
		addResource(&ubDLightsDesc, NULL);

		// Uniform buffer for extended camera data
		BufferLoadDesc ubPlaneInfoDesc = {};
		ubPlaneInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubPlaneInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubPlaneInfoDesc.mDesc.mSize = sizeof(UniformPlaneInfoData);
		ubPlaneInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubPlaneInfoDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubPlaneInfoDesc.ppBuffer = &pBufferUniformPlaneInfo[i];
			addResource(&ubPlaneInfoDesc, NULL);
		}

		waitForAllResourceLoads();

		// prepare resources

		// Update the uniform buffer for the objects
		mat4 sponza_modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(0.02f, 0.02f, 0.02f));
		gUniformDataMVP.mWorldMat = sponza_modelmat;
		gUniformDataMVP.mPrevWorldMat = gUniformDataMVP.mWorldMat;
		gUniformDataMVP.mMetallic = 0;
		gUniformDataMVP.mRoughness = 0.5f;
		gUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc sponza_objBuffUpdateDesc = { pSponzaBuffer };
		beginUpdateResource(&sponza_objBuffUpdateDesc);
		*(UniformObjData*)sponza_objBuffUpdateDesc.pMappedData = gUniformDataMVP;
		endUpdateResource(&sponza_objBuffUpdateDesc, NULL);

		// Update the uniform buffer for the objects
		mat4 lion_modelmat = mat4::translation(vec3(0.0f, -6.0f, 1.0f)) * mat4::rotationY(-1.5708f) * mat4::scale(vec3(0.2f, 0.2f, -0.2f));
		gUniformDataMVP.mWorldMat = lion_modelmat;
		gUniformDataMVP.mPrevWorldMat = gUniformDataMVP.mWorldMat;
		gUniformDataMVP.mMetallic = 0;
		gUniformDataMVP.mRoughness = 0.5f;
		gUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc lion_objBuffUpdateDesc = { pLionBuffer };
		beginUpdateResource(&lion_objBuffUpdateDesc);
		*(UniformObjData*)lion_objBuffUpdateDesc.pMappedData = gUniformDataMVP;
		endUpdateResource(&lion_objBuffUpdateDesc, NULL);

		// Add light to scene

		//Point light
		Light light = {};
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[0] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		gUniformDataLights.mLights[3] = light;

		gUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights };
		beginUpdateResource(&lightBuffUpdateDesc);
		*(UniformLightData*)lightBuffUpdateDesc.pMappedData = gUniformDataLights;
		endUpdateResource(&lightBuffUpdateDesc, NULL);

		for (size_t i = 0; i < TOTAL_IMGS; i += 1)
		{
			loadTexture(i);
		}

		//Directional light
		DirectionalLight dLight;
		dLight.mCol = vec4(1.0f, 1.0f, 1.0f, 5.0f);
		dLight.mPos = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		dLight.mDir = vec4(-1.0f, -1.5f, 1.0f, 0.0f);

		gUniformDataDirectionalLights.mLights[0] = dLight;
		gUniformDataDirectionalLights.mCurrAmountOfDLights = 1;
		BufferUpdateDesc directionalLightBuffUpdateDesc = { pBufferUniformDirectionalLights };
		beginUpdateResource(&directionalLightBuffUpdateDesc);
		*(UniformDirectionalLightData*)directionalLightBuffUpdateDesc.pMappedData = gUniformDataDirectionalLights;
		endUpdateResource(&directionalLightBuffUpdateDesc, NULL);

		static const char* enumRenderModeNames[] = { "Render Scene Only", "Render Reflections Only", "Render Scene with Reflections",
								"Render Scene with exclusive Reflections" };

		static const char* enumReflectionTypeNames[] = { "Pixel Projected Reflections", "Stochastic Screen Space Reflections" };

		DropdownWidget ddRenderMode;
		ddRenderMode.pData = &gRenderMode;
		ddRenderMode.pNames = enumRenderModeNames;
		ddRenderMode.mCount = sizeof(enumRenderModeNames)/sizeof(enumRenderModeNames[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Render Mode", &ddRenderMode, WIDGET_TYPE_DROPDOWN));

		DropdownWidget ddReflType;
		ddReflType.pData = &gReflectionType;
		ddReflType.pNames = enumReflectionTypeNames;
		ddReflType.mCount = sizeof(enumReflectionTypeNames)/sizeof(enumReflectionTypeNames[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Reflection Type", &ddReflType, WIDGET_TYPE_DROPDOWN));

		CheckboxWidget holePatchCheck;
		holePatchCheck.pData = &gUseHolePatching;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Use Holepatching", &holePatchCheck, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget holePatchExpCheck;
		holePatchExpCheck.pData = &gUseExpensiveHolePatching;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Use Expensive Holepatching", &holePatchExpCheck, WIDGET_TYPE_CHECKBOX));

		//pGui->AddWidget(CheckboxWidget("Use Normalmap", &gUseNormalMap));

		CheckboxWidget fadeCheck;
		fadeCheck.pData = &gUseFadeEffect;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Use Fade Effect", &fadeCheck, WIDGET_TYPE_CHECKBOX));

		SliderFloatWidget pprIntensitySlider;
		pprIntensitySlider.pData = &gRRP_Intensity;
		pprIntensitySlider.mMin = 0.0f;
		pprIntensitySlider.mMax = 1.0f;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Intensity of PPR", &pprIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT));

		SliderUintWidget numPlanesSlider;
		numPlanesSlider.pData = &gPlaneNumber;
		numPlanesSlider.mMin = 1;
		numPlanesSlider.mMax = 4;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Number of Planes", &numPlanesSlider, WIDGET_TYPE_SLIDER_UINT));

		SliderFloatWidget mainPlaneSizeSlider;
		mainPlaneSizeSlider.pData = &gPlaneSize;
		mainPlaneSizeSlider.mMin = 5.0f;
		mainPlaneSizeSlider.mMax = 100.0f;
		luaRegisterWidget(uiCreateDynamicWidgets(&PPR_Widgets, "Size of Main Plane", &mainPlaneSizeSlider, WIDGET_TYPE_SLIDER_FLOAT));

		if (gSSSRSupported)
		{
			OneLineCheckboxWidget olCheckbox;
			olCheckbox.pData = &gUseSPD;
			olCheckbox.mColor = float4(1.f);
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Use Singlepass Downsampler", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

			olCheckbox.pData = &gSSSR_SkipDenoiser;
			olCheckbox.mColor = float4(1.f);
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Show Intersection Results", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

			SliderUintWidget uintSlider;
			uintSlider.pData = &gSSSR_MaxTravelsalIntersections;
			uintSlider.mMin = 0;
			uintSlider.mMax = 256;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Max Traversal Iterations", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

			uintSlider.pData = &gSSSR_MinTravelsalOccupancy;
			uintSlider.mMin = 0;
			uintSlider.mMax = 32;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Min Traversal Occupancy", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

			uintSlider.pData = &gSSSR_MostDetailedMip;
			uintSlider.mMin = 0;
			uintSlider.mMax = 5;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Most Detailed Level", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

			SliderFloatWidget floatSlider;
			floatSlider.pData = &gSSSR_DepthThickness;
			floatSlider.mMin = 0.0f;
			floatSlider.mMax = 0.3f;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Depth Buffer Thickness", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

			floatSlider.pData = &gSSSR_RougnessThreshold;
			floatSlider.mMin = 0.0f;
			floatSlider.mMax = 1.0f;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Roughness Threshold", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

			floatSlider.pData = &pSSSR_TemporalStability;
			floatSlider.mMin = 0.0f;
			floatSlider.mMax = 1.0f;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Temporal Stability", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

			olCheckbox.pData = &gSSSR_TemporalVarianceEnabled;
			olCheckbox.mColor = float4(1.f);
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Enable Variance Guided Tracing", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

			RadioButtonWidget radiobutton;
			radiobutton.pData = &gSSSR_SamplesPerQuad;
			radiobutton.mRadioId = 1;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "1 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

			radiobutton.pData = &gSSSR_SamplesPerQuad;
			radiobutton.mRadioId = 2;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "2 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

			radiobutton.pData = &gSSSR_SamplesPerQuad;
			radiobutton.mRadioId = 4;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "4 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

			radiobutton.pData = &gSSSR_EAWPassCount;
			radiobutton.mRadioId = 1;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "1 EAW Pass", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

			radiobutton.pData = &gSSSR_EAWPassCount;
			radiobutton.mRadioId = 3;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "3 EAW Pass", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));
		}
		else
		{
			LabelWidget notSupportedLabel;
			luaRegisterWidget(uiCreateDynamicWidgets(&SSSR_Widgets, "Not supported by your GPU", &notSupportedLabel, WIDGET_TYPE_LABEL));
		}

		DropdownWidget ddTestScripts;
		ddTestScripts.pData = &gCurrentScriptIndex;
		ddTestScripts.pNames = gTestScripts;
		ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

		ButtonWidget bRunScript;
		UIWidget* pRunScript = uiCreateComponentWidget(pGui, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
		luaRegisterWidget(pRunScript);

		if (gReflectionType == PP_REFLECTION)
		{
			uiShowDynamicWidgets(&PPR_Widgets, pGui);
		}
		else if (gReflectionType == SSS_REFLECTION)
		{
			uiShowDynamicWidgets(&SSSR_Widgets, pGui);
		}
		// fill Gbuffers
		// Create vertex layout
		gVertexLayoutModel.mAttribCount = 3;

		gVertexLayoutModel.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutModel.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[0].mBinding = 0;
		gVertexLayoutModel.mAttribs[0].mLocation = 0;
		gVertexLayoutModel.mAttribs[0].mOffset = 0;

		//normals
		gVertexLayoutModel.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutModel.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutModel.mAttribs[1].mLocation = 1;
		gVertexLayoutModel.mAttribs[1].mBinding = 0;
		gVertexLayoutModel.mAttribs[1].mOffset = 3 * sizeof(float);

		//texture
		gVertexLayoutModel.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutModel.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutModel.mAttribs[2].mLocation = 2;
		gVertexLayoutModel.mAttribs[2].mBinding = 0;
		gVertexLayoutModel.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats

		for (size_t i = 0; i < 2; i += 1)
		{
			loadMesh(i);
		}

		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 20.0f, -2.0f, 0.9f };
		vec3                   lookAt{ 0.0f, -2.0f, 0.9f };

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(camParameters);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onUIInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}
			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		assignSponzaTextures();

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();
		exitCameraController(pCameraController);
		gSponzaTextureIndexforMaterial.set_capacity(0);
		gInitializeVal.set_capacity(0);

		gFrameIndex = 0;
		gFrameFlipFlop = 0;

		removeGpuProfiler(gSSSRGpuProfileToken);
		removeGpuProfiler(gPPRGpuProfileToken);

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint i = 0; i < TOTAL_IMGS; ++i)
		{
			if (pMaterialTextures[i])
			{
				removeResource(pMaterialTextures[i]);
			}
		}

		removeResource(pSpecularMap);
		removeResource(pIrradianceMap);
		removeResource(pSkybox);
		removeResource(pBRDFIntegrationMap);

		removeResource(pSponzaBuffer);
		removeResource(pLionBuffer);

		removeResource(pSPD_AtomicCounterBuffer);
		removeResource(pSSSR_RayCounterBuffer);
		removeResource(pSSSR_TileCounterBuffer);
		removeResource(pSSSR_IntersectArgsBuffer);
		removeResource(pSSSR_DenoiserArgsBuffer);
		removeResource(pSSSR_SobolBuffer);
		removeResource(pSSSR_RankingTileBuffer);
		removeResource(pSSSR_ScramblingTileBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferUniformPlaneInfo[i]);
			removeResource(pBufferUniformPPRPro[i]);
			removeResource(pBufferUniformExtendedCamera[i]);
			removeResource(pBufferUniformCameraSky[i]);
			removeResource(pBufferUniformCamera[i]);
			removeResource(pSSSR_ConstantsBuffer[i]);
		}

		removeResource(pBufferUniformLights);
		removeResource(pBufferUniformDirectionalLights);
		removeResource(pSkyboxVertexBuffer);
		removeResource(pScreenQuadVertexBuffer);

		for (Geometry*& model : gModels)
			removeResource(model);

		uiDestroyDynamicWidgets(&PPR_Widgets);
		uiDestroyDynamicWidgets(&SSSR_Widgets);

		exitUserInterface();

		exitFontSystem(); 

		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerNearest);

		// Remove commands and command pool&
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		removeQueue(pRenderer, pGraphicsQueue);

		// Remove resource loader and renderer
		exitResourceLoaderInterface(pRenderer);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	void ComputePBRMaps()
	{
		Texture*       pPanoSkybox = NULL;
		Shader*        pPanoToCubeShader = NULL;
		RootSignature* pPanoToCubeRootSignature = NULL;
		Pipeline*      pPanoToCubePipeline = NULL;
		Shader*        pBRDFIntegrationShader = NULL;
		RootSignature* pBRDFIntegrationRootSignature = NULL;
		Pipeline*      pBRDFIntegrationPipeline = NULL;
		Shader*        pIrradianceShader = NULL;
		RootSignature* pIrradianceRootSignature = NULL;
		Pipeline*      pIrradiancePipeline = NULL;
		Shader*        pSpecularShader = NULL;
		RootSignature* pSpecularRootSignature = NULL;
		Pipeline*      pSpecularPipeline = NULL;
		Sampler*       pSkyboxSampler = NULL;
		DescriptorSet* pDescriptorSetBRDF = { NULL };
		DescriptorSet* pDescriptorSetPanoToCube[2] = { NULL };
		DescriptorSet* pDescriptorSetIrradiance = { NULL };
		DescriptorSet* pDescriptorSetSpecular[2] = { NULL };

		static const int skyboxIndex = 0;
		const char*      skyboxNames[] = {
            "LA_Helipad",
		};
		// PBR Texture values (these values are mirrored on the shaders).
		static const uint32_t gBRDFIntegrationSize = 512;
		static const uint32_t gSkyboxSize = 1024;
		static const uint32_t gSkyboxMips = 11;
		static const uint32_t gIrradianceSize = 32;
		static const uint32_t gSpecularSize = 128;
		static const uint32_t gSpecularMips = (uint)log2(gSpecularSize) + 1;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, false, 0.0f, 0.0f, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		SyncToken       token = {};
		TextureLoadDesc panoDesc = {};
		panoDesc.pFileName = skyboxNames[skyboxIndex];
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, &token);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pName = "skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pSkybox;
		addResource(&skyboxLoadDesc, NULL);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pName = "irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pIrradianceMap;
		addResource(&irrLoadDesc, NULL);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pName = "specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pSpecularMap;
		addResource(&specImgLoadDesc, NULL);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc     brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = TinyImageFormat_R32G32_SFLOAT;
		brdfIntegrationDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc, NULL);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0 };

		GPUPresetLevel presetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;

		const char* brdfIntegrationShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"BRDFIntegration_SAMPLES_0.comp",
			"BRDFIntegration_SAMPLES_0.comp",
			"BRDFIntegration_SAMPLES_64.comp",
			"BRDFIntegration_SAMPLES_128.comp",
			"BRDFIntegration_SAMPLES_256.comp",
			"BRDFIntegration_SAMPLES_1024.comp"
		};

		const char* irradianceShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp"
		};

		const char* specularShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"computeSpecularMap_SAMPLES_0.comp",
			"computeSpecularMap_SAMPLES_0.comp",
			"computeSpecularMap_SAMPLES_64.comp",
			"computeSpecularMap_SAMPLES_128.comp",
			"computeSpecularMap_SAMPLES_256.comp",
			"computeSpecularMap_SAMPLES_1024.comp"
		};

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { brdfIntegrationShaders[presetLevel], NULL, 0 };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { irradianceShaders[presetLevel], NULL, 0 };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { specularShaders[presetLevel], NULL, 0 };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
		irradianceRootDesc.mStaticSamplerCount = 1;
		irradianceRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		irradianceRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
		specularRootDesc.mStaticSamplerCount = 1;
		specularRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		specularRootDesc.ppStaticSamplers = &pSkyboxSampler;
		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
		addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
		addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);
		addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);

		DescriptorSetDesc setDesc = { pBRDFIntegrationRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[0]);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[1]);
		setDesc = { pIrradianceRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIrradiance);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[0]);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[1]);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &desc, &pPanoToCubePipeline);
		pipelineSettings.pShaderProgram = pIrradianceShader;
		pipelineSettings.pRootSignature = pIrradianceRootSignature;
		addPipeline(pRenderer, &desc, &pIrradiancePipeline);
		pipelineSettings.pShaderProgram = pSpecularShader;
		pipelineSettings.pRootSignature = pSpecularRootSignature;
		addPipeline(pRenderer, &desc, &pSpecularPipeline);
		pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
		pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
		addPipeline(pRenderer, &desc, &pBRDFIntegrationPipeline);

		waitForToken(&token);

		Cmd* pCmd = pCmds[0];

		// Compute the BRDF Integration map.
		resetCmdPool(pRenderer, pCmdPools[0]);
		beginCmd(pCmd);

		cmdBindPipeline(pCmd, pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pBRDFIntegrationMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetBRDF, 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetBRDF);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] = { { pBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };

		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarrier, 0, NULL);

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(pCmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetPanoToCube[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPanoToCube[0]);

		struct
		{
			uint32_t mip;
			uint32_t textureSize;
		} rootConstantData = { 0, gSkyboxSize };

		for (uint32_t i = 0; i < gSkyboxMips; ++i)
		{
			rootConstantData.mip = i;
			cmdBindPushConstants(pCmd, pPanoToCubeRootSignature, getDescriptorIndexFromName(pPanoToCubeRootSignature, "RootConstant"), &rootConstantData);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pSkybox;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetPanoToCube[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetPanoToCube[1]);

			pThreadGroupSize = pPanoToCubeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				pCmd, max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pSkybox, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarriers, 0, NULL);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(pCmd, pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pIrradianceMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetIrradiance, 2, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetIrradiance);
		pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);

		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(pCmd, pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSpecular[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetSpecular[0]);

		struct PrecomputeSkySpecularData
		{
			uint  mipSize;
			float roughness;
		};

		for (uint32_t i = 0; i < gSpecularMips; i++)
		{
			PrecomputeSkySpecularData data = {};
			data.roughness = (float)i / (float)(gSpecularMips - 1);
			data.mipSize = gSpecularSize >> i;
			cmdBindPushConstants(pCmd, pSpecularRootSignature, getDescriptorIndexFromName(pSpecularRootSignature, "RootConstant"), &data);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pSpecularMap;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetSpecular[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetSpecular[1]);
			pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(pCmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] = { { pIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
										   { pSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 2, srvBarriers2, 0, NULL);

		endCmd(pCmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;
		submitDesc.pSignalFence = pRenderCompleteFences[0];
		submitDesc.mSubmitDone = true;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);

		removeDescriptorSet(pRenderer, pDescriptorSetBRDF);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIrradiance);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[1]);
		removePipeline(pRenderer, pSpecularPipeline);
		removeRootSignature(pRenderer, pSpecularRootSignature);
		removeShader(pRenderer, pSpecularShader);
		removePipeline(pRenderer, pIrradiancePipeline);
		removeRootSignature(pRenderer, pIrradianceRootSignature);
		removeShader(pRenderer, pIrradianceShader);
		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removePipeline(pRenderer, pBRDFIntegrationPipeline);
		removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
		removeShader(pRenderer, pBRDFIntegrationShader);
		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);

		resetCmdPool(pRenderer, pCmdPools[0]);
	}

	void loadMesh(size_t index)
	{
		//Load Sponza
		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = gModelNames[index];
		loadDesc.ppGeometry = &gModels[index];
		loadDesc.pVertexLayout = &gVertexLayoutModel;
		addResource(&loadDesc, &gResourceSyncToken);
	}

	void loadTexture(size_t index)
	{
		TextureLoadDesc textureDesc = {};
		textureDesc.pFileName = pMaterialImageFileNames[index];
		textureDesc.ppTexture = &pMaterialTextures[index];
		if (strstr(pMaterialImageFileNames[index], "Albedo") || strstr(pMaterialImageFileNames[index], "diffuse"))
		{
			// Textures representing color should be stored in SRGB or HDR format
			textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		}
		addResource(&textureDesc, &gResourceSyncToken);
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		gResourceSyncStartToken = getLastTokenCompleted();

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			if (!addSwapChain())
				return false;

			addRenderTargets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

		waitForAllResourceLoads();
		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		FontSystemLoadDesc fontLoad = {};
		fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			waitForToken(&gResourceSyncToken);
			waitForAllResourceLoads();

			gResourceSyncToken = 0;

			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTargets();
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		// Update camera
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		CameraMatrix ViewProjMat = projMat * viewMat;

		gUniformDataCamera.mPrevProjectView = gUniformDataCamera.mProjectView;
		gUniformDataCamera.mProjectView = ViewProjMat;
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		viewMat.setTranslation(vec3(0));
		gUniformDataSky = gUniformDataCamera;
		gUniformDataSky.mProjectView = projMat * viewMat;

		//data uniforms
		gUniformDataExtenedCamera.mCameraWorldPos = vec4(pCameraController->getViewPosition(), 1.0);
		gUniformDataExtenedCamera.mViewMat = pCameraController->getViewMatrix();
		gUniformDataExtenedCamera.mProjMat = projMat;
		gUniformDataExtenedCamera.mViewProjMat = ViewProjMat;
		gUniformDataExtenedCamera.mInvViewProjMat = CameraMatrix::inverse(ViewProjMat);
		gUniformDataExtenedCamera.mViewPortSize =
			vec4(static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0, 0.0);

		//projection uniforms
		gUniformPPRProData.renderMode = gRenderMode;
		gUniformPPRProData.useHolePatching =
			((gReflectionType == PP_REFLECTION || !gSSSRSupported) && gUseHolePatching == true) ? 1.0f : 0.0f;
		gUniformPPRProData.useExpensiveHolePatching = gUseExpensiveHolePatching == true ? 1.0f : 0.0f;
		gUniformPPRProData.useNormalMap = gUseNormalMap == true ? 1.0f : 0.0f;
		gUniformPPRProData.useFadeEffect = gUseFadeEffect == true ? 1.0f : 0.0f;
		gUniformPPRProData.intensity = (gReflectionType == PP_REFLECTION || !gSSSRSupported) ? gRRP_Intensity : 1.0f;

		//Planes
		gUniformDataPlaneInfo.numPlanes = gPlaneNumber;
		gUniformDataPlaneInfo.planeInfo[0].centerPoint = vec4(0.0, -6.0f, 0.9f, 0.0);
		gUniformDataPlaneInfo.planeInfo[0].size = vec4(gPlaneSize);

		gUniformDataPlaneInfo.planeInfo[1].centerPoint = vec4(10.0, -5.0f, -1.25f, 0.0);
		gUniformDataPlaneInfo.planeInfo[1].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		gUniformDataPlaneInfo.planeInfo[2].centerPoint = vec4(10.0, -5.0f, 3.0f, 0.0);
		gUniformDataPlaneInfo.planeInfo[2].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		gUniformDataPlaneInfo.planeInfo[3].centerPoint = vec4(10.0, 1.0f, 0.9f, 0.0);
		gUniformDataPlaneInfo.planeInfo[3].size = vec4(10.0f);

		mat4 basicMat;
		basicMat[0] = vec4(1.0, 0.0, 0.0, 0.0);     //tan
		basicMat[1] = vec4(0.0, 0.0, -1.0, 0.0);    //bitan
		basicMat[2] = vec4(0.0, 1.0, 0.0, 0.0);     //normal
		basicMat[3] = vec4(0.0, 0.0, 0.0, 1.0);

		gUniformDataPlaneInfo.planeInfo[0].rotMat = basicMat;

		gUniformDataPlaneInfo.planeInfo[1].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -80.0f);
		gUniformDataPlaneInfo.planeInfo[2].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -100.0f);
		gUniformDataPlaneInfo.planeInfo[3].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * 90.0f);

		gUniformSSSRConstantsData.g_prev_view_proj =
            CameraMatrix::transpose(CameraMatrix::transpose(gUniformSSSRConstantsData.g_proj) * transpose(gUniformSSSRConstantsData.g_view));
		gUniformSSSRConstantsData.g_inv_view_proj = CameraMatrix::transpose(gUniformDataExtenedCamera.mInvViewProjMat);
		gUniformSSSRConstantsData.g_proj = CameraMatrix::transpose(projMat);
		gUniformSSSRConstantsData.g_inv_proj = CameraMatrix::transpose(CameraMatrix::inverse(projMat));
		gUniformSSSRConstantsData.g_view = transpose(gUniformDataExtenedCamera.mViewMat);
		gUniformSSSRConstantsData.g_inv_view = transpose(inverse(gUniformDataExtenedCamera.mViewMat));

		gUniformSSSRConstantsData.g_frame_index++;
		gUniformSSSRConstantsData.g_max_traversal_intersections = gSSSR_MaxTravelsalIntersections;
		gUniformSSSRConstantsData.g_min_traversal_occupancy = gSSSR_MinTravelsalOccupancy;
		gUniformSSSRConstantsData.g_most_detailed_mip = gSSSR_MostDetailedMip;
		gUniformSSSRConstantsData.g_temporal_stability_factor = pSSSR_TemporalStability;
		gUniformSSSRConstantsData.g_depth_buffer_thickness = gSSSR_DepthThickness;
		gUniformSSSRConstantsData.g_samples_per_quad = gSSSR_SamplesPerQuad;
		gUniformSSSRConstantsData.g_temporal_variance_guided_tracing_enabled = gSSSR_TemporalVarianceEnabled;
		gUniformSSSRConstantsData.g_roughness_threshold = gSSSR_RougnessThreshold;
		gUniformSSSRConstantsData.g_skip_denoiser = gSSSR_SkipDenoiser;

		SyncToken currentProgress = getLastTokenCompleted();
		double    progress = (double)(currentProgress - gResourceSyncStartToken) / (double)(gResourceSyncToken - gResourceSyncStartToken);
		mProgressBarValue = (size_t)(mProgressBarValueMax * progress);

		if (gReflectionType != gLastReflectionType)
		{
			if (gReflectionType == PP_REFLECTION)
			{
				uiShowDynamicWidgets(&PPR_Widgets, pGui);
				uiHideDynamicWidgets(&SSSR_Widgets, pGui);
			}
			else if (gReflectionType == SSS_REFLECTION)
			{
				uiShowDynamicWidgets(&SSSR_Widgets, pGui);
				uiHideDynamicWidgets(&PPR_Widgets, pGui);
			}
			gLastReflectionType = gReflectionType;
		}
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		// This will acquire the next swapchain image
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex] };
		beginUpdateResource(&camBuffUpdateDesc);
		*(UniformCamData*)camBuffUpdateDesc.pMappedData = gUniformDataCamera;
		endUpdateResource(&camBuffUpdateDesc, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(UniformCamData*)skyboxViewProjCbv.pMappedData = gUniformDataSky;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		BufferUpdateDesc CbvExtendedCamera = { pBufferUniformExtendedCamera[gFrameIndex] };
		beginUpdateResource(&CbvExtendedCamera);
		*(UniformExtendedCamData*)CbvExtendedCamera.pMappedData = gUniformDataExtenedCamera;
		endUpdateResource(&CbvExtendedCamera, NULL);

		BufferUpdateDesc CbPPR_Prop = { pBufferUniformPPRPro[gFrameIndex] };
		beginUpdateResource(&CbPPR_Prop);
		*(UniformPPRProData*)CbPPR_Prop.pMappedData = gUniformPPRProData;
		endUpdateResource(&CbPPR_Prop, NULL);

		BufferUpdateDesc planeInfoBuffUpdateDesc = { pBufferUniformPlaneInfo[gFrameIndex] };
		beginUpdateResource(&planeInfoBuffUpdateDesc);
		*(UniformPlaneInfoData*)planeInfoBuffUpdateDesc.pMappedData = gUniformDataPlaneInfo;
		endUpdateResource(&planeInfoBuffUpdateDesc, NULL);

		BufferUpdateDesc SSSR_ConstantsBuffUpdateDesc = { pSSSR_ConstantsBuffer[gFrameIndex] };
		beginUpdateResource(&SSSR_ConstantsBuffUpdateDesc);
		*(UniformSSSRConstantsData*)SSSR_ConstantsBuffUpdateDesc.pMappedData = gUniformSSSRConstantsData;
		endUpdateResource(&SSSR_ConstantsBuffUpdateDesc, NULL);

		// Draw G-buffers
		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		gCurrentGpuProfileToken = gReflectionType == PP_REFLECTION ? gPPRGpuProfileToken : gSSSRGpuProfileToken;

		cmdBeginGpuFrameProfile(cmd, gCurrentGpuProfileToken);

		//Clear G-buffers and Depth buffer
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetDeferredPass[0][0]->mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { { 1.0f, 0.0f } };    // Clear depth to the far plane and stencil to 0

		// Transfer G-buffers to render target state for each buffer
		RenderTargetBarrier rtBarriers[DEFERRED_RT_COUNT + 4] = {};
		uint32_t rtBarriersCount = DEFERRED_RT_COUNT + 1;
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE };
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			rtBarriers[1 + i] = { pRenderTargetDeferredPass[gFrameFlipFlop][i], RESOURCE_STATE_SHADER_RESOURCE,
								  RESOURCE_STATE_RENDER_TARGET };
		}

		RenderTarget* renderTargets[4] = {};
		uint32_t rtCount = 1;
		renderTargets[0] = *(pRenderTargetDeferredPass[gFrameFlipFlop]);

		Pipeline* skyboxPipeline = pSkyboxPipeline;

		if (gClearTextures)
		{
			for (uint32_t i = 0; i < 3; ++i)
			{
				loadActions.mLoadActionsColor[1 + i] = LOAD_ACTION_CLEAR;
				loadActions.mClearColorValues[1 + i] = { { 0.0f, 0.0f, 0.0f, 0.0f } };
			}

			rtBarriers[DEFERRED_RT_COUNT + 1] = { pSSSR_TemporalResults[0], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
			rtBarriers[DEFERRED_RT_COUNT + 2] = { pSSSR_TemporalResults[1], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
			rtBarriers[DEFERRED_RT_COUNT + 3] = { pSSSR_RayLength , RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
			rtBarriersCount += 3;

			renderTargets[1] = pSSSR_TemporalResults[0];
			renderTargets[2] = pSSSR_TemporalResults[1];
			renderTargets[3] = pSSSR_RayLength;
			rtCount += 3;

			skyboxPipeline = pSkyboxWithClearTexturesPipeline;
		}

		// Transfer DepthBuffer to a DephtWrite State
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, rtBarriersCount, rtBarriers);

		cmdBindRenderTargets(cmd, rtCount, renderTargets, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 1.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0][0]->mWidth, pRenderTargetDeferredPass[0][0]->mHeight);

		// Draw the skybox.
		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Render SkyBox");

		const uint32_t skyboxStride = sizeof(float) * 4;
		cmdBindPipeline(cmd, skyboxPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox[1]);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
		cmdDraw(cmd, 36, 0);

		if (gClearTextures)
		{
			cmdBindRenderTargets(cmd, 0, NULL, NULL, &loadActions, NULL, NULL, -1, -1);
			
			RenderTargetBarrier barriers[3] = {};
			barriers[0] = { pSSSR_TemporalResults[0], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS };
			barriers[1] = { pSSSR_TemporalResults[1], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS };
			barriers[2] = { pSSSR_RayLength, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, barriers);

			gClearTextures = false;
		}

		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 0.0f, 1.0f);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		for (uint32_t i = 1; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[0][i]->mClearValue;
		}
		cmdBindRenderTargets(
			cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass[gFrameFlipFlop], pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0][0]->mWidth, (float)pRenderTargetDeferredPass[0][0]->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0][0]->mWidth, pRenderTargetDeferredPass[0][0]->mHeight);

		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
		// Draw Sponza
		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Fill GBuffers");
		//The default code path we have if not iOS uses an array of texture of size 81
		//iOS only supports 31 max texture units in a fragment shader for most devices.
		//so fallback to binding every texture for every draw call (max of 5 textures)

		//SyncToken lastCompletedToken = getLastTokenCompleted();
		bool dataLoaded = isTokenCompleted(&gResourceSyncToken);
		if (dataLoaded)
		{
			Geometry& sponzaMesh = *gModels[SPONZA_MODEL];

			cmdBindIndexBuffer(cmd, sponzaMesh.pIndexBuffer, sponzaMesh.mIndexType, 0);
			Buffer* pSponzaVertexBuffers[] = { sponzaMesh.pVertexBuffers[0] };
			cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);

			struct MaterialMaps
			{
				uint textureMaps;
			} data;

			const uint32_t drawCount = (uint32_t)sponzaMesh.mDrawArgCount;

			if (gSettings.mUseTextureFallback)
			{
				cmdBindPipeline(cmd, pPipelineGbuffers);
				cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGbuffers[1]);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[2]);
				DescriptorData params[8] = {};

				for (uint32_t i = 0; i < drawCount; ++i)
				{
					int materialID = gMaterialIds[i];
					materialID *= 5;    //because it uses 5 basic textures for redering BRDF

					for (int j = 0; j < 5; ++j)
					{
						params[j].pName = pTextureName[j];
						params[j].ppTextures = &pMaterialTextures[gSponzaTextureIndexforMaterial[materialID + j]];
					}
					updateDescriptorSet(pRenderer, i, pDescriptorSetGbuffers[0], 5, params);
					cmdBindDescriptorSet(cmd, i, pDescriptorSetGbuffers[0]);

					IndirectDrawIndexArguments& cmdData = sponzaMesh.pDrawArgs[i];
					cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
				}
			}
			else
			{
				cmdBindPipeline(cmd, pPipelineGbuffers);
				cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, sponzaMesh.mVertexStrides, NULL);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGbuffers[1]);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetGbuffers[2]);

				for (uint32_t i = 0; i < drawCount; ++i)
				{
					int materialID = gMaterialIds[i];
					materialID *= 5;    //because it uses 5 basic textures for redering BRDF

					data.textureMaps = ((gSponzaTextureIndexforMaterial[materialID + 0] & 0xFF) << 0) |
									   ((gSponzaTextureIndexforMaterial[materialID + 1] & 0xFF) << 8) |
									   ((gSponzaTextureIndexforMaterial[materialID + 2] & 0xFF) << 16) |
									   ((gSponzaTextureIndexforMaterial[materialID + 3] & 0xFF) << 24);

					cmdBindPushConstants(cmd, pRootSigGbuffers, gMapIDRootConstantIndex, &data);
					IndirectDrawIndexArguments& cmdData = sponzaMesh.pDrawArgs[i];
					cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
				}
			}

			Geometry& lionMesh = *gModels[LION_MODEL];

			//Draw Lion
			if (gSettings.mUseTextureFallback)
			{
				DescriptorData params[5] = {};

				params[0].pName = pTextureName[0];
				params[0].ppTextures = &pMaterialTextures[81];

				params[1].pName = pTextureName[1];
				params[1].ppTextures = &pMaterialTextures[83];

				params[2].pName = pTextureName[2];
				params[2].ppTextures = &pMaterialTextures[6];

				params[3].pName = pTextureName[3];
				params[3].ppTextures = &pMaterialTextures[6];

				params[4].pName = pTextureName[4];
				params[4].ppTextures = &pMaterialTextures[0];

				updateDescriptorSet(pRenderer, drawCount + 1, pDescriptorSetGbuffers[0], 5, params);
				cmdBindDescriptorSet(cmd, drawCount + 1, pDescriptorSetGbuffers[0]);
			}
			else
			{
				data.textureMaps = ((81 & 0xFF) << 0) | ((83 & 0xFF) << 8) | ((6 & 0xFF) << 16) | ((6 & 0xFF) << 24);

				cmdBindPushConstants(cmd, pRootSigGbuffers, gMapIDRootConstantIndex, &data);
			}

			cmdBindDescriptorSet(cmd, 1, pDescriptorSetGbuffers[2]);

			Buffer* pLionVertexBuffers[] = { lionMesh.pVertexBuffers[0] };
			cmdBindVertexBuffer(cmd, 1, pLionVertexBuffers, lionMesh.mVertexStrides, NULL);
			cmdBindIndexBuffer(cmd, lionMesh.pIndexBuffer, lionMesh.mIndexType, 0);

			for (uint32_t i = 0; i < (uint32_t)lionMesh.mDrawArgCount; ++i)
			{
				IndirectDrawIndexArguments& cmdData = lionMesh.pDrawArgs[i];
				cmdDrawIndexed(cmd, cmdData.mIndexCount, cmdData.mStartIndex, cmdData.mVertexOffset);
			}
		}

		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transfer DepthBuffer to a Shader resource state
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
		// Transfer current render target to a render target state
		rtBarriers[1] = { pSceneBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
		// Transfer G-buffers to a Shader resource state
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			rtBarriers[2 + i] = { pRenderTargetDeferredPass[gFrameFlipFlop][i], RESOURCE_STATE_RENDER_TARGET,
								  RESOURCE_STATE_SHADER_RESOURCE };
		}

		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 2, rtBarriers);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pSceneBuffer->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pSceneBuffer, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mWidth, (float)pSceneBuffer->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneBuffer->mWidth, pSceneBuffer->mHeight);

		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Render BRDF");

		const uint32_t quadStride = sizeof(float) * 5;
		cmdBindPipeline(cmd, pPipelineBRDF);
		cmdBindDescriptorSet(cmd, gFrameFlipFlop, pDescriptorSetBRDF[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetBRDF[1]);
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
		cmdDraw(cmd, 3, 0);
		//#endif
		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		if (gReflectionType == PP_REFLECTION || !gSSSRSupported)
		{
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Pixel-Projected Reflections");

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "ProjectionPass");

			cmdBindPipeline(cmd, pPPR_ProjectionPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR_Projection[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPPR_Projection[1]);
			const uint32_t* pThreadGroupSize = pPPR_ProjectionShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(cmd, (mSettings.mWidth * mSettings.mHeight / pThreadGroupSize[0]) + 1, 1, 1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			// Transfer current render target to a render target state
			rtBarriers[0] = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
			rtBarriers[1] = { pReflectionBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

			loadActions = {};

			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = pReflectionBuffer->mClearValue;

			cmdBindRenderTargets(cmd, 1, &pReflectionBuffer, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pReflectionBuffer->mWidth, (float)pReflectionBuffer->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pReflectionBuffer->mWidth, pReflectionBuffer->mHeight);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "ReflectionPass");
			cmdBindPipeline(cmd, pPPR_ReflectionPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR_Reflection[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPPR_Reflection[1]);
			cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			//End ReflectionPass
			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "HolePatching");
			rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
		}
		else if (gReflectionType == SSS_REFLECTION)
		{
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Stochastic Screen Space Reflections");

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Depth mips generation");

			uint32_t dim_x = (pDepthBuffer->mWidth + 7) / 8;
			uint32_t dim_y = (pDepthBuffer->mHeight + 7) / 8;

			TextureBarrier textureBarriers[4] = {};
			textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

			if (gUseSPD)
			{
				cmdBindPipeline(cmd, pSPDPipeline);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSPD);
				cmdDispatch(cmd, (pDepthBuffer->mWidth + 63) / 64, (pDepthBuffer->mHeight + 63) / 64, pRenderTargetDeferredPass[0][0]->mArraySize);
			}
			else
			{
				cmdBindPipeline(cmd, pCopyDepthPipeline);
				cmdBindDescriptorSet(cmd, 0, pDescriptorCopyDepth);
				cmdDispatch(cmd, dim_x, dim_y, 1);

				uint32_t mipSizeX = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mWidth));
				uint32_t mipSizeY = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mHeight));
				cmdBindPipeline(cmd, pGenerateMipPipeline);
				for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
				{
					mipSizeX >>= 1;
					mipSizeY >>= 1;
					uint mipSize[2] = { mipSizeX, mipSizeY };
					cmdBindPushConstants(cmd, pGenerateMipRootSignature, gMipSizeRootConstantIndex, mipSize);
					cmdBindDescriptorSet(cmd, i - 1, pDescriptorGenerateMip);
					textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

					uint32_t groupCountX = mipSizeX / 16;
					uint32_t groupCountY = mipSizeY / 16;
					if (groupCountX == 0)
						groupCountX = 1;
					if (groupCountY == 0)
						groupCountY = 1;
					cmdDispatch(cmd, groupCountX, groupCountY, pRenderTargetDeferredPass[0][0]->mArraySize);
				}
			}

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Classify");

			BufferBarrier bufferBarriers[4] = {};
			bufferBarriers[0] = { pSSSR_RayCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[1] = { pSSSR_TileCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[2] = { pSSSR_RayListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[3] = { pSSSR_TileListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			textureBarriers[0] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			textureBarriers[1] = { pSSSR_TemporalResults[gFrameFlipFlop]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
								   RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 4, bufferBarriers, 2, textureBarriers, 1, rtBarriers);

			cmdBindPipeline(cmd, pSSSR_ClassifyTilesPipeline);
			cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ClassifyTiles);
			cmdDispatch(cmd, dim_x, dim_y, pRenderTargetDeferredPass[0][0]->mArraySize);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Prepare Indirect");

			bufferBarriers[0] = { pSSSR_RayCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[1] = { pSSSR_TileCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[2] = { pSSSR_IntersectArgsBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS };
			bufferBarriers[3] = { pSSSR_DenoiserArgsBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS };

			cmdResourceBarrier(cmd, 4, bufferBarriers, 0, NULL, 0, NULL);

			cmdBindPipeline(cmd, pSSSR_PrepareIndirectArgsPipeline);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetSSSR_PrepareIndirectArgs);
			cmdDispatch(cmd, 1, 1, 1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Intersect");

			bufferBarriers[0] = { pSSSR_IntersectArgsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT };
			bufferBarriers[1] = { pSSSR_DenoiserArgsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT };
			bufferBarriers[2] = { pSSSR_RayListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			textureBarriers[1] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			rtBarriers[1] = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 3, bufferBarriers, 2, textureBarriers, 2, rtBarriers);

			cmdBindPipeline(cmd, pSSSR_IntersectPipeline);
			cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_Intersect);
			cmdExecuteIndirect(cmd, pSSSR_IntersectCommandSignature, 1, pSSSR_IntersectArgsBuffer, 0, NULL, 0);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			if (!gSSSR_SkipDenoiser)
			{
				cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Spatial Denoise");

				bufferBarriers[0] = { pSSSR_TileListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[0] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[1] = { pSSSR_TemporalResults[gFrameFlipFlop]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
									   RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[2] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(cmd, 1, bufferBarriers, 3, textureBarriers, 1, rtBarriers);

				cmdBindPipeline(cmd, pSSSR_ResolveSpatialPipeline);
				cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ResolveSpatial);
				cmdExecuteIndirect(cmd, pSSSR_ResolveSpatialCommandSignature, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

				cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

				cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Temporal Denoise");

				textureBarriers[0] = { pSSSR_TemporalResults[0]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[1] = { pSSSR_TemporalResults[1]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[2] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[3] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(cmd, 0, NULL, 4, textureBarriers, 1, rtBarriers);

				cmdBindPipeline(cmd, pSSSR_ResolveTemporalPipeline);
				cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ResolveTemporal);
				cmdExecuteIndirect(cmd, pSSSR_ResolveTemporalCommandSignature, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

				cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

				cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 1");

				textureBarriers[0] = { pSSSR_TemporalResults[gFrameFlipFlop]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
									   RESOURCE_STATE_UNORDERED_ACCESS };
				rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

				cmdBindPipeline(cmd, pSSSR_ResolveEAWPipeline);
				cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ResolveEAW);
				cmdExecuteIndirect(cmd, pSSSR_ResolveEAWCommandSignature, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

				cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

				if (gSSSR_EAWPassCount == 3)
				{
					cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 2");

					textureBarriers[0] = { pSSSR_TemporalResults[gFrameFlipFlop]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
										   RESOURCE_STATE_UNORDERED_ACCESS };
					rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

					cmdBindPipeline(cmd, pSSSR_ResolveEAWStride2Pipeline);
					cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ResolveEAWStride2);
					cmdExecuteIndirect(cmd, pSSSR_ResolveEAWStride2CommandSignature, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

					cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

					cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 3");

					textureBarriers[0] = { pSSSR_TemporalResults[gFrameFlipFlop]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
										   RESOURCE_STATE_UNORDERED_ACCESS };
					rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

					cmdBindPipeline(cmd, pSSSR_ResolveEAWStride4Pipeline);
					cmdBindDescriptorSet(cmd, gFrameFlipFlop * gImageCount + gFrameIndex, pDescriptorSetSSSR_ResolveEAWStride4);
					cmdExecuteIndirect(cmd, pSSSR_ResolveEAWStride4CommandSignature, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

					cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
				}
			}
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Apply Reflections");
			rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
		}

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

		rtBarriers[1] = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPPR_HolePatchingPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR__HolePatching[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPPR__HolePatching[1]);
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
		// End Reflections
		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
		float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
		cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f),gCurrentGpuProfileToken, &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);
		cmdEndDebugMarker(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transition our texture to present state
		rtBarriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);
		cmdEndGpuFrameProfile(cmd, gCurrentGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;
		queuePresent(pGraphicsQueue, &presentDesc);

		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
		gFrameFlipFlop ^= 1;
	}

	const char* GetName() { return "10_ScreenSpaceReflections"; }

	void prepareDescriptorSets()
	{
		DescriptorData skyParams[1] = {};
		skyParams[0].pName = "skyboxTex";
		skyParams[0].ppTextures = &pSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			skyParams[0].pName = "uniformBlock";
			skyParams[0].ppBuffers = &pBufferUniformCameraSky[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyParams);
		}

		// GBuffer
		{
			DescriptorData params[8] = {};

			if (!gSettings.mUseTextureFallback)
			{
				params[0].pName = "textureMaps";
				params[0].ppTextures = pMaterialTextures;
				params[0].mCount = TOTAL_IMGS;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetGbuffers[0], 1, params);
			}

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0] = {};
				params[0].pName = "cbCamera";
				params[0].ppBuffers = &pBufferUniformCamera[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetGbuffers[1], 1, params);
			}
			params[0].pName = "cbObject";
			params[0].ppBuffers = &pSponzaBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetGbuffers[2], 1, params);
			params[0].ppBuffers = &pLionBuffer;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetGbuffers[2], 1, params);
		}
		// Shading
		{
			DescriptorData BRDFParams[12] = {};
			BRDFParams[0].pName = "cbLights";
			BRDFParams[0].ppBuffers = &pBufferUniformLights;
			BRDFParams[1].pName = "cbDLights";
			BRDFParams[1].ppBuffers = &pBufferUniformDirectionalLights;
			BRDFParams[2].pName = "brdfIntegrationMap";
			BRDFParams[2].ppTextures = &pBRDFIntegrationMap;
			BRDFParams[3].pName = "irradianceMap";
			BRDFParams[3].ppTextures = &pIrradianceMap;
			BRDFParams[4].pName = "specularMap";
			BRDFParams[4].ppTextures = &pSpecularMap;
			BRDFParams[5].pName = "DepthTexture";
			BRDFParams[5].ppTextures = &pDepthBuffer->pTexture;
			for (uint32_t i = 0; i < 2; ++i)
			{
				BRDFParams[6].pName = "AlbedoTexture";
				BRDFParams[6].ppTextures = &pRenderTargetDeferredPass[i][0]->pTexture;
				BRDFParams[7].pName = "NormalTexture";
				BRDFParams[7].ppTextures = &pRenderTargetDeferredPass[i][1]->pTexture;
				BRDFParams[8].pName = "RoughnessTexture";
				BRDFParams[8].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
				updateDescriptorSet(pRenderer, i, pDescriptorSetBRDF[0], 9, BRDFParams);
			}
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				BRDFParams[0].pName = "cbExtendCamera";
				BRDFParams[0].ppBuffers = &pBufferUniformExtendedCamera[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetBRDF[1], 1, BRDFParams);
			}
		}
		// PPR Projection
		{
			DescriptorData PPR_ProjectionParams[2] = {};
			PPR_ProjectionParams[0].pName = "IntermediateBuffer";
			PPR_ProjectionParams[0].ppBuffers = &pIntermediateBuffer;
			PPR_ProjectionParams[1].pName = "DepthTexture";
			PPR_ProjectionParams[1].ppTextures = &pDepthBuffer->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetPPR_Projection[0], 2, PPR_ProjectionParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				PPR_ProjectionParams[0].pName = "cbExtendCamera";
				PPR_ProjectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[i];
				PPR_ProjectionParams[1].pName = "planeInfoBuffer";
				PPR_ProjectionParams[1].ppBuffers = &pBufferUniformPlaneInfo[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetPPR_Projection[1], 2, PPR_ProjectionParams);
			}
		}
		// PPR Reflection
		{
			DescriptorData PPR_ReflectionParams[3] = {};
			PPR_ReflectionParams[0].pName = "SceneTexture";
			PPR_ReflectionParams[0].ppTextures = &pSceneBuffer->pTexture;
			PPR_ReflectionParams[1].pName = "IntermediateBuffer";
			PPR_ReflectionParams[1].ppBuffers = &pIntermediateBuffer;
			PPR_ReflectionParams[2].pName = "DepthTexture";
			PPR_ReflectionParams[2].ppTextures = &pDepthBuffer->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetPPR_Reflection[0], 3, PPR_ReflectionParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				PPR_ReflectionParams[0].pName = "cbExtendCamera";
				PPR_ReflectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[i];
				PPR_ReflectionParams[1].pName = "planeInfoBuffer";
				PPR_ReflectionParams[1].ppBuffers = &pBufferUniformPlaneInfo[i];
				PPR_ReflectionParams[2].pName = "cbProperties";
				PPR_ReflectionParams[2].ppBuffers = &pBufferUniformPPRPro[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetPPR_Reflection[1], 3, PPR_ReflectionParams);
			}
		}
		// PPR Hole Patching
		{
			DescriptorData PPR_HolePatchingParams[2] = {};
			PPR_HolePatchingParams[0].pName = "SceneTexture";
			PPR_HolePatchingParams[0].ppTextures = &pSceneBuffer->pTexture;
			PPR_HolePatchingParams[1].pName = "SSRTexture";
			PPR_HolePatchingParams[1].ppTextures = &pReflectionBuffer->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetPPR__HolePatching[0], 2, PPR_HolePatchingParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				PPR_HolePatchingParams[0].pName = "cbExtendCamera";
				PPR_HolePatchingParams[0].ppBuffers = &pBufferUniformExtendedCamera[i];
				PPR_HolePatchingParams[1].pName = "cbProperties";
				PPR_HolePatchingParams[1].ppBuffers = &pBufferUniformPPRPro[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetPPR__HolePatching[1], 2, PPR_HolePatchingParams);
			}
		}
		if (gSSSRSupported)
		{
			{
				DescriptorData params[3] = {};
				params[0].pName = "g_depth_buffer";
				params[0].ppTextures = &pDepthBuffer->pTexture;
				params[1].pName = "g_downsampled_depth_buffer";
				params[1].ppTextures = &pSSSR_DepthHierarchy;
				params[1].mBindMipChain = true;
				params[2].pName = "g_global_atomic";
				params[2].ppBuffers = &pSPD_AtomicCounterBuffer;
				updateDescriptorSet(pRenderer, 0, pDescriptorSPD, 3, params);
			}
			{
				DescriptorData params[2] = {};
				params[0].pName = "Source";
				params[0].ppTextures = &pDepthBuffer->pTexture;
				params[1].pName = "Destination";
				params[1].ppTextures = &pSSSR_DepthHierarchy;
				params[1].mUAVMipSlice = 0;
				updateDescriptorSet(pRenderer, 0, pDescriptorCopyDepth, 2, params);
			}
			// Depth downsample
			{
				for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
				{
					DescriptorData params[2] = {};
					params[0].pName = "Source";
					params[0].ppTextures = &pSSSR_DepthHierarchy;
					params[1].pName = "Destination";
					params[1].ppTextures = &pSSSR_DepthHierarchy;
					params[1].mUAVMipSlice = i;
					updateDescriptorSet(pRenderer, i - 1, pDescriptorGenerateMip, 2, params);
				}
			}
			//  SSSR Classify Tiles
			{
				DescriptorData SSSR_ClassifyTilesParams[10] = {};
				SSSR_ClassifyTilesParams[0].pName = "g_ray_list";
				SSSR_ClassifyTilesParams[0].ppBuffers = &pSSSR_RayListBuffer;
				SSSR_ClassifyTilesParams[1].pName = "g_tile_list";
				SSSR_ClassifyTilesParams[1].ppBuffers = &pSSSR_TileListBuffer;
				SSSR_ClassifyTilesParams[2].pName = "g_ray_counter";
				SSSR_ClassifyTilesParams[2].ppBuffers = &pSSSR_RayCounterBuffer;
				SSSR_ClassifyTilesParams[3].pName = "g_tile_counter";
				SSSR_ClassifyTilesParams[3].ppBuffers = &pSSSR_TileCounterBuffer;
				SSSR_ClassifyTilesParams[4].pName = "g_ray_lengths";
				SSSR_ClassifyTilesParams[4].ppTextures = &pSSSR_RayLength->pTexture;
				SSSR_ClassifyTilesParams[5].pName = "g_temporal_variance";
				SSSR_ClassifyTilesParams[5].ppTextures = &pSSSR_TemporalVariance;
				SSSR_ClassifyTilesParams[6].pName = "g_denoised_reflections";
				SSSR_ClassifyTilesParams[6].ppTextures = &pReflectionBuffer->pTexture;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ClassifyTilesParams[7].pName = "g_temporally_denoised_reflections";
					SSSR_ClassifyTilesParams[7].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					SSSR_ClassifyTilesParams[8].pName = "g_roughness";
					SSSR_ClassifyTilesParams[8].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ClassifyTilesParams[9].pName = "Constants";
						SSSR_ClassifyTilesParams[9].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ClassifyTiles, 10, SSSR_ClassifyTilesParams);
					}
				}
			}
			// SSSR Prepare
			{
				DescriptorData SSSR_PrepareIndirectArgsParams[4] = {};
				SSSR_PrepareIndirectArgsParams[0].pName = "g_tile_counter";
				SSSR_PrepareIndirectArgsParams[0].ppBuffers = &pSSSR_TileCounterBuffer;
				SSSR_PrepareIndirectArgsParams[1].pName = "g_ray_counter";
				SSSR_PrepareIndirectArgsParams[1].ppBuffers = &pSSSR_RayCounterBuffer;
				SSSR_PrepareIndirectArgsParams[2].pName = "g_intersect_args";
				SSSR_PrepareIndirectArgsParams[2].ppBuffers = &pSSSR_IntersectArgsBuffer;
				SSSR_PrepareIndirectArgsParams[3].pName = "g_denoiser_args";
				SSSR_PrepareIndirectArgsParams[3].ppBuffers = &pSSSR_DenoiserArgsBuffer;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetSSSR_PrepareIndirectArgs, 4, SSSR_PrepareIndirectArgsParams);
			}
			{
				DescriptorData SSSR_IntersectParams[12] = {};
				SSSR_IntersectParams[0].pName = "g_lit_scene";
				SSSR_IntersectParams[0].ppTextures = &pSceneBuffer->pTexture;
				SSSR_IntersectParams[1].pName = "g_depth_buffer_hierarchy";
				SSSR_IntersectParams[1].ppTextures = &pSSSR_DepthHierarchy;
				SSSR_IntersectParams[2].pName = "g_sobol_buffer";
				SSSR_IntersectParams[2].ppBuffers = &pSSSR_SobolBuffer;
				SSSR_IntersectParams[3].pName = "g_ranking_tile_buffer";
				SSSR_IntersectParams[3].ppBuffers = &pSSSR_RankingTileBuffer;
				SSSR_IntersectParams[4].pName = "g_scrambling_tile_buffer";
				SSSR_IntersectParams[4].ppBuffers = &pSSSR_ScramblingTileBuffer;
				SSSR_IntersectParams[5].pName = "g_ray_lengths";
				SSSR_IntersectParams[5].ppTextures = &pSSSR_RayLength->pTexture;
				SSSR_IntersectParams[6].pName = "g_denoised_reflections";
				SSSR_IntersectParams[6].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_IntersectParams[7].pName = "g_ray_list";
				SSSR_IntersectParams[7].ppBuffers = &pSSSR_RayListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_IntersectParams[8].pName = "g_intersection_result";
					SSSR_IntersectParams[8].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					SSSR_IntersectParams[9].pName = "g_normal";
					SSSR_IntersectParams[9].ppTextures = &pRenderTargetDeferredPass[i][1]->pTexture;
					SSSR_IntersectParams[10].pName = "g_roughness";
					SSSR_IntersectParams[10].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_IntersectParams[11].pName = "Constants";
						SSSR_IntersectParams[11].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(pRenderer, i * gImageCount + j, pDescriptorSetSSSR_Intersect, 12, SSSR_IntersectParams);
					}
				}
			}
			{
				DescriptorData SSSR_ResolveSpatialParams[9] = {};
				SSSR_ResolveSpatialParams[0].pName = "g_depth_buffer";
				SSSR_ResolveSpatialParams[0].ppTextures = &pSSSR_DepthHierarchy;
				SSSR_ResolveSpatialParams[1].pName = "g_spatially_denoised_reflections";
				SSSR_ResolveSpatialParams[1].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_ResolveSpatialParams[2].pName = "g_ray_lengths";
				SSSR_ResolveSpatialParams[2].ppTextures = &pSSSR_RayLength->pTexture;
				SSSR_ResolveSpatialParams[3].pName = "g_has_ray";
				SSSR_ResolveSpatialParams[3].ppTextures = &pSSSR_TemporalVariance;
				SSSR_ResolveSpatialParams[4].pName = "g_tile_list";
				SSSR_ResolveSpatialParams[4].ppBuffers = &pSSSR_TileListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ResolveSpatialParams[5].pName = "g_intersection_result";
					SSSR_ResolveSpatialParams[5].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					SSSR_ResolveSpatialParams[6].pName = "g_normal";
					SSSR_ResolveSpatialParams[6].ppTextures = &pRenderTargetDeferredPass[i][1]->pTexture;
					SSSR_ResolveSpatialParams[7].pName = "g_roughness";
					SSSR_ResolveSpatialParams[7].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ResolveSpatialParams[8].pName = "Constants";
						SSSR_ResolveSpatialParams[8].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(
							pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ResolveSpatial, 9, SSSR_ResolveSpatialParams);
					}
				}
			}
			{
				DescriptorData SSSR_ResolveTemporalParams[13] = {};
				SSSR_ResolveTemporalParams[0].pName = "g_depth_buffer";
				SSSR_ResolveTemporalParams[0].ppTextures = &pSSSR_DepthHierarchy;
				SSSR_ResolveTemporalParams[1].pName = "g_spatially_denoised_reflections";
				SSSR_ResolveTemporalParams[1].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_ResolveTemporalParams[2].pName = "g_temporal_variance";
				SSSR_ResolveTemporalParams[2].ppTextures = &pSSSR_TemporalVariance;
				SSSR_ResolveTemporalParams[3].pName = "g_ray_lengths";
				SSSR_ResolveTemporalParams[3].ppTextures = &pSSSR_RayLength->pTexture;
				SSSR_ResolveTemporalParams[4].pName = "g_tile_list";
				SSSR_ResolveTemporalParams[4].ppBuffers = &pSSSR_TileListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ResolveTemporalParams[5].pName = "g_normal";
					SSSR_ResolveTemporalParams[5].ppTextures = &pRenderTargetDeferredPass[i][1]->pTexture;
					SSSR_ResolveTemporalParams[6].pName = "g_roughness";
					SSSR_ResolveTemporalParams[6].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					SSSR_ResolveTemporalParams[7].pName = "g_normal_history";
					SSSR_ResolveTemporalParams[7].ppTextures = &pRenderTargetDeferredPass[1 - i][1]->pTexture;
					SSSR_ResolveTemporalParams[8].pName = "g_roughness_history";
					SSSR_ResolveTemporalParams[8].ppTextures = &pRenderTargetDeferredPass[1 - i][2]->pTexture;
					SSSR_ResolveTemporalParams[9].pName = "g_motion_vectors";
					SSSR_ResolveTemporalParams[9].ppTextures = &pRenderTargetDeferredPass[i][3]->pTexture;
					SSSR_ResolveTemporalParams[10].pName = "g_temporally_denoised_reflections";
					SSSR_ResolveTemporalParams[10].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					SSSR_ResolveTemporalParams[11].pName = "g_temporally_denoised_reflections_history";
					SSSR_ResolveTemporalParams[11].ppTextures = &pSSSR_TemporalResults[1 - i]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ResolveTemporalParams[12].pName = "Constants";
						SSSR_ResolveTemporalParams[12].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(
							pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ResolveTemporal, 13, SSSR_ResolveTemporalParams);
					}
				}
			}
			{
				DescriptorData SSSR_ResolveEAWParams[5] = {};
				SSSR_ResolveEAWParams[0].pName = "g_denoised_reflections";
				SSSR_ResolveEAWParams[0].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_ResolveEAWParams[1].pName = "g_tile_list";
				SSSR_ResolveEAWParams[1].ppBuffers = &pSSSR_TileListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ResolveEAWParams[2].pName = "g_roughness";
					SSSR_ResolveEAWParams[2].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					SSSR_ResolveEAWParams[3].pName = "g_temporally_denoised_reflections";
					SSSR_ResolveEAWParams[3].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ResolveEAWParams[4].pName = "Constants";
						SSSR_ResolveEAWParams[4].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ResolveEAW, 5, SSSR_ResolveEAWParams);
					}
				}
			}
			{
				DescriptorData SSSR_ResolveEAWStride2Params[5] = {};
				SSSR_ResolveEAWStride2Params[0].pName = "g_temporally_denoised_reflections";
				SSSR_ResolveEAWStride2Params[0].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_ResolveEAWStride2Params[1].pName = "g_tile_list";
				SSSR_ResolveEAWStride2Params[1].ppBuffers = &pSSSR_TileListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ResolveEAWStride2Params[2].pName = "g_roughness";
					SSSR_ResolveEAWStride2Params[2].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					SSSR_ResolveEAWStride2Params[3].pName = "g_denoised_reflections";
					SSSR_ResolveEAWStride2Params[3].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ResolveEAWStride2Params[4].pName = "Constants";
						SSSR_ResolveEAWStride2Params[4].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(
							pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ResolveEAWStride2, 5, SSSR_ResolveEAWStride2Params);
					}
				}
			}
			{
				DescriptorData SSSR_ResolveEAWStride4Params[5] = {};
				SSSR_ResolveEAWStride4Params[0].pName = "g_denoised_reflections";
				SSSR_ResolveEAWStride4Params[0].ppTextures = &pReflectionBuffer->pTexture;
				SSSR_ResolveEAWStride4Params[1].pName = "g_tile_list";
				SSSR_ResolveEAWStride4Params[1].ppBuffers = &pSSSR_TileListBuffer;
				for (uint32_t i = 0; i < 2; ++i)
				{
					SSSR_ResolveEAWStride4Params[2].pName = "g_roughness";
					SSSR_ResolveEAWStride4Params[2].ppTextures = &pRenderTargetDeferredPass[i][2]->pTexture;
					SSSR_ResolveEAWStride4Params[3].pName = "g_temporally_denoised_reflections";
					SSSR_ResolveEAWStride4Params[3].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
					for (uint32_t j = 0; j < gImageCount; ++j)
					{
						SSSR_ResolveEAWStride4Params[4].pName = "Constants";
						SSSR_ResolveEAWStride4Params[4].ppBuffers = &pSSSR_ConstantsBuffer[j];
						updateDescriptorSet(
							pRenderer, i * gImageCount + j, pDescriptorSetSSSR_ResolveEAWStride4, 5, SSSR_ResolveEAWStride4Params);
					}
				}
			}
		}
	}

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		if (gSSSRSupported)
		{
			// This desc set contains mip level data which is dependent on window dimensions
			DescriptorSetDesc setDesc = { pSPDRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSPD);
		}


		// Skybox
		DescriptorSetDesc setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pSkyboxRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		// GBuffer
		if (gSettings.mUseTextureFallback)
		{
			setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_NONE, 512 };
		}
		else
		{
			setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		}
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[0]);
		setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[1]);
		setDesc = { pRootSigGbuffers, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGbuffers[2]);
		// BRDF
		setDesc = { pRootSigBRDF, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[0]);
		setDesc = { pRootSigBRDF, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[1]);
		// PPR Projection
		setDesc = { pPPR_ProjectionRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR_Projection[0]);
		setDesc = { pPPR_ProjectionRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR_Projection[1]);
		// PPR Reflection
		setDesc = { pPPR_ReflectionRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR_Reflection[0]);
		setDesc = { pPPR_ReflectionRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR_Reflection[1]);
		// PPR Hole Patching
		setDesc = { pPPR_HolePatchingRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR__HolePatching[0]);
		setDesc = { pPPR_HolePatchingRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR__HolePatching[1]);

		if (gSSSRSupported)
		{
			// Copy depth
			setDesc = { pCopyDepthRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorCopyDepth);
			// DepthDownsample
			setDesc = { pGenerateMipRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 13 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorGenerateMip);
			// SSSR
			setDesc = { pSSSR_ClassifyTilesRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ClassifyTiles);
			setDesc = { pSSSR_PrepareIndirectArgsRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_PrepareIndirectArgs);
			setDesc = { pSSSR_IntersectRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_Intersect);
			setDesc = { pSSSR_ResolveSpatialRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ResolveSpatial);
			setDesc = { pSSSR_ResolveTemporalRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ResolveTemporal);
			setDesc = { pSSSR_ResolveEAWRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ResolveEAW);
			setDesc = { pSSSR_ResolveEAWStride2RootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ResolveEAWStride2);
			setDesc = { pSSSR_ResolveEAWStride4RootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 };
			addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR_ResolveEAWStride4);
		}
	}

	void removeDescriptorSets()
	{
		if (gSSSRSupported)
			removeDescriptorSet(pRenderer, pDescriptorSPD);

		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetGbuffers[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR_Projection[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR_Projection[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR_Reflection[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR_Reflection[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR__HolePatching[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPPR__HolePatching[1]);

		if (gSSSRSupported)
		{
			removeDescriptorSet(pRenderer, pDescriptorCopyDepth);
			removeDescriptorSet(pRenderer, pDescriptorGenerateMip);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ClassifyTiles);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_PrepareIndirectArgs);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_Intersect);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ResolveSpatial);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ResolveTemporal);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ResolveEAW);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ResolveEAWStride2);
			removeDescriptorSet(pRenderer, pDescriptorSetSSSR_ResolveEAWStride4);
		}
	}

	void addRootSignatures()
	{
		// GBuffer
		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler*    pStaticSamplers[] = { pSamplerBilinear };

		RootSignatureDesc gBuffersRootDesc = { &pShaderGbuffers, 1 };
		gBuffersRootDesc.mStaticSamplerCount = 1;
		gBuffersRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		gBuffersRootDesc.ppStaticSamplers = pStaticSamplers;

		if (!gSettings.mUseTextureFallback)
		{
			gBuffersRootDesc.mMaxBindlessTextures = TOTAL_IMGS;
		}
		addRootSignature(pRenderer, &gBuffersRootDesc, &pRootSigGbuffers);
		gMapIDRootConstantIndex = getDescriptorIndexFromName(pRootSigGbuffers, "cbTextureRootConstants");

		const char*       pSkyboxamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pSkyboxRootSignature);

		//BRDF
		const char* pStaticSampler2Names[] = { "envSampler", "defaultSampler" };
		Sampler*    pStaticSamplers2[] = { pSamplerBilinear, pSamplerNearest };

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 2;
		brdfRootDesc.ppStaticSamplerNames = pStaticSampler2Names;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers2;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		//PPR_Projection
		RootSignatureDesc PPR_PRootDesc = { &pPPR_ProjectionShader, 1 };
		PPR_PRootDesc.mStaticSamplerCount = 1;
		PPR_PRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		PPR_PRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &PPR_PRootDesc, &pPPR_ProjectionRootSignature);

		//PPR_Reflection
		RootSignatureDesc PPR_RRootDesc = { &pPPR_ReflectionShader, 1 };
		PPR_RRootDesc.mStaticSamplerCount = 1;
		PPR_RRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		PPR_RRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &PPR_RRootDesc, &pPPR_ReflectionRootSignature);

		//PPR_HolePatching
		const char* pStaticSamplerforHolePatchingNames[] = { "nearestSampler", "bilinearSampler" };
		Sampler*    pStaticSamplersforHolePatching[] = { pSamplerNearest, pSamplerBilinear };

		RootSignatureDesc PPR_HolePatchingRootDesc = { &pPPR_HolePatchingShader, 1 };
		PPR_HolePatchingRootDesc.mStaticSamplerCount = 2;
		PPR_HolePatchingRootDesc.ppStaticSamplerNames = pStaticSamplerforHolePatchingNames;
		PPR_HolePatchingRootDesc.ppStaticSamplers = pStaticSamplersforHolePatching;
		addRootSignature(pRenderer, &PPR_HolePatchingRootDesc, &pPPR_HolePatchingRootSignature);

		if (gSSSRSupported)
		{
			RootSignatureDesc SPDDescRootDesc = { &pSPDShader, 1 };
			SPDDescRootDesc.mMaxBindlessTextures = 13;
			addRootSignature(pRenderer, &SPDDescRootDesc, &pSPDRootSignature);

			RootSignatureDesc CopyDepthShaderDescRootDesc = { &pCopyDepthShader, 1 };
			addRootSignature(pRenderer, &CopyDepthShaderDescRootDesc, &pCopyDepthRootSignature);

			RootSignatureDesc GenerateMipShaderDescRootDesc = { &pGenerateMipShader, 1 };
			addRootSignature(pRenderer, &GenerateMipShaderDescRootDesc, &pGenerateMipRootSignature);
			gMipSizeRootConstantIndex = getDescriptorIndexFromName(pGenerateMipRootSignature, "RootConstant");

			// SSSR
			RootSignatureDesc SSSR_ClassifyTilesRootDesc = { &pSSSR_ClassifyTilesShader, 1 };
			addRootSignature(pRenderer, &SSSR_ClassifyTilesRootDesc, &pSSSR_ClassifyTilesRootSignature);

			RootSignatureDesc SSSR_PrepareIndirectArgsRootDesc = { &pSSSR_PrepareIndirectArgsShader, 1 };
			addRootSignature(pRenderer, &SSSR_PrepareIndirectArgsRootDesc, &pSSSR_PrepareIndirectArgsRootSignature);

			RootSignatureDesc SSSR_IntersectRootDesc = { &pSSSR_IntersectShader, 1 };
			addRootSignature(pRenderer, &SSSR_IntersectRootDesc, &pSSSR_IntersectRootSignature);

			IndirectArgumentDescriptor indirectArgDescs[1] = {};
			indirectArgDescs[0].mType = INDIRECT_DISPATCH;

			CommandSignatureDesc cmdSignatureDesc = { pSSSR_IntersectRootSignature,
											indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdSignatureDesc, &pSSSR_IntersectCommandSignature);

			RootSignatureDesc SSSR_ResolveSpatialRootDesc = { &pSSSR_ResolveSpatialShader, 1 };
			addRootSignature(pRenderer, &SSSR_ResolveSpatialRootDesc, &pSSSR_ResolveSpatialRootSignature);

			CommandSignatureDesc cmdResolveSpatialSignatureDesc = { pSSSR_ResolveSpatialRootSignature,
											indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdResolveSpatialSignatureDesc, &pSSSR_ResolveSpatialCommandSignature);

			RootSignatureDesc SSSR_ResolveTemporalRootDesc = { &pSSSR_ResolveTemporalShader, 1 };
			addRootSignature(pRenderer, &SSSR_ResolveTemporalRootDesc, &pSSSR_ResolveTemporalRootSignature);

			CommandSignatureDesc cmdResolveTemporalSignatureDesc = { pSSSR_ResolveTemporalRootSignature,
											indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdResolveTemporalSignatureDesc, &pSSSR_ResolveTemporalCommandSignature);

			RootSignatureDesc SSSR_ResolveEAWRootDesc = { &pSSSR_ResolveEAWShader, 1 };
			addRootSignature(pRenderer, &SSSR_ResolveEAWRootDesc, &pSSSR_ResolveEAWRootSignature);

			CommandSignatureDesc cmdResolveEAWSignatureDesc = { pSSSR_ResolveEAWRootSignature,
										indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdResolveEAWSignatureDesc, &pSSSR_ResolveEAWCommandSignature);

			RootSignatureDesc SSSR_ResolveEAWStride2RootDesc = { &pSSSR_ResolveEAWStride2Shader, 1 };
			addRootSignature(pRenderer, &SSSR_ResolveEAWStride2RootDesc, &pSSSR_ResolveEAWStride2RootSignature);

			CommandSignatureDesc cmdResolveEAWStride2SignatureDesc = { pSSSR_ResolveEAWStride2RootSignature,
											indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdResolveEAWStride2SignatureDesc, &pSSSR_ResolveEAWStride2CommandSignature);

			RootSignatureDesc SSSR_ResolveEAWStride4RootDesc = { &pSSSR_ResolveEAWStride4Shader, 1 };
			addRootSignature(pRenderer, &SSSR_ResolveEAWStride4RootDesc, &pSSSR_ResolveEAWStride4RootSignature);

			CommandSignatureDesc cmdResolveEAWStride4SignatureDesc = { pSSSR_ResolveEAWStride4RootSignature,
											indirectArgDescs, sizeof(indirectArgDescs) / sizeof(indirectArgDescs[0]), true };
			addIndirectCommandSignature(pRenderer, &cmdResolveEAWStride4SignatureDesc, &pSSSR_ResolveEAWStride4CommandSignature);
		}
	}

	void removeRootSignatures()
	{
		if (gSSSRSupported)
		{
			removeIndirectCommandSignature(pRenderer, pSSSR_ResolveEAWStride4CommandSignature);
			removeRootSignature(pRenderer, pSSSR_ResolveEAWStride4RootSignature);
			removeIndirectCommandSignature(pRenderer, pSSSR_ResolveEAWStride2CommandSignature);
			removeRootSignature(pRenderer, pSSSR_ResolveEAWStride2RootSignature);
			removeIndirectCommandSignature(pRenderer, pSSSR_ResolveEAWCommandSignature);
			removeRootSignature(pRenderer, pSSSR_ResolveEAWRootSignature);
			removeIndirectCommandSignature(pRenderer, pSSSR_ResolveTemporalCommandSignature);
			removeRootSignature(pRenderer, pSSSR_ResolveTemporalRootSignature);
			removeIndirectCommandSignature(pRenderer, pSSSR_ResolveSpatialCommandSignature);
			removeRootSignature(pRenderer, pSSSR_ResolveSpatialRootSignature);
			removeIndirectCommandSignature(pRenderer, pSSSR_IntersectCommandSignature);
			removeRootSignature(pRenderer, pSSSR_IntersectRootSignature);
			removeRootSignature(pRenderer, pSSSR_PrepareIndirectArgsRootSignature);
			removeRootSignature(pRenderer, pSSSR_ClassifyTilesRootSignature);
			removeRootSignature(pRenderer, pSPDRootSignature);
			removeRootSignature(pRenderer, pGenerateMipRootSignature);
			removeRootSignature(pRenderer, pCopyDepthRootSignature);
		}

		removeRootSignature(pRenderer, pPPR_HolePatchingRootSignature);
		removeRootSignature(pRenderer, pPPR_ReflectionRootSignature);
		removeRootSignature(pRenderer, pPPR_ProjectionRootSignature);
		removeRootSignature(pRenderer, pRootSigBRDF);
		removeRootSignature(pRenderer, pSkyboxRootSignature);
		removeRootSignature(pRenderer, pRootSigGbuffers);
	}

	void addShaders()
	{
		// GBuffer
		ShaderLoadDesc gBuffersShaderDesc = {};
		gBuffersShaderDesc.mStages[0] = { "fillGbuffers.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		gBuffersShaderDesc.mStages[1] = { "fillGbuffers.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		if (gSettings.mUseTextureFallback)
		{
			gBuffersShaderDesc.mStages[0].pFileName = "fillGbuffers_TEX_FALLBACK.vert";
			gBuffersShaderDesc.mStages[1].pFileName = "fillGbuffers_TEX_FALLBACK.frag";
		}
		addShader(pRenderer, &gBuffersShaderDesc, &pShaderGbuffers);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

		//BRDF
		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

		//PPR_Projection
		ShaderLoadDesc PPR_ProjectionShaderDesc = {};
		PPR_ProjectionShaderDesc.mStages[0] = { "PPR_Projection.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &PPR_ProjectionShaderDesc, &pPPR_ProjectionShader);

		//PPR_Reflection
		ShaderLoadDesc PPR_ReflectionShaderDesc = {};
		PPR_ReflectionShaderDesc.mStages[0] = { "PPR_Reflection.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		PPR_ReflectionShaderDesc.mStages[1] = { "PPR_Reflection.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &PPR_ReflectionShaderDesc, &pPPR_ReflectionShader);

		//PPR_HolePatching
		ShaderLoadDesc PPR_HolePatchingShaderDesc = {};
		PPR_HolePatchingShaderDesc.mStages[0] = { "PPR_Holepatching.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		PPR_HolePatchingShaderDesc.mStages[1] = { "PPR_Holepatching.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &PPR_HolePatchingShaderDesc, &pPPR_HolePatchingShader);

		if (gSSSRSupported)
		{
			ShaderLoadDesc SPDDesc = {};
			SPDDesc.mStages[0] = { "DepthDownsample.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			SPDDesc.mTarget = shader_target_6_0;
			addShader(pRenderer, &SPDDesc, &pSPDShader);

			ShaderLoadDesc CopyDepthShaderDesc = {};
			CopyDepthShaderDesc.mStages[0] = { "copyDepth.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &CopyDepthShaderDesc, &pCopyDepthShader);

			ShaderLoadDesc GenerateMipShaderDesc = {};
			GenerateMipShaderDesc.mStages[0] = { "generateMips.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &GenerateMipShaderDesc, &pGenerateMipShader);

			// SSSR
			ShaderLoadDesc SSSR_ClassifyTilesShaderDesc = {};
			SSSR_ClassifyTilesShaderDesc.mStages[0] = { "SSSR_ClassifyTiles.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			SSSR_ClassifyTilesShaderDesc.mTarget = shader_target_6_0;
			addShader(pRenderer, &SSSR_ClassifyTilesShaderDesc, &pSSSR_ClassifyTilesShader);
			
			ShaderLoadDesc SSSR_PrepareIndirectArgsShaderDesc = {};
			SSSR_PrepareIndirectArgsShaderDesc.mStages[0] = { "SSSR_PrepareIndirectArgs.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &SSSR_PrepareIndirectArgsShaderDesc, &pSSSR_PrepareIndirectArgsShader);

			ShaderLoadDesc SSSR_IntersectShaderDesc = {};
			SSSR_IntersectShaderDesc.mStages[0] = { "SSSR_Intersect.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			SSSR_IntersectShaderDesc.mTarget = shader_target_6_0;
			addShader(pRenderer, &SSSR_IntersectShaderDesc, &pSSSR_IntersectShader);

			ShaderLoadDesc SSSR_ResolveSpatialShaderDesc = {};
			SSSR_ResolveSpatialShaderDesc.mStages[0] = { "SSSR_ResolveSpatial.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			SSSR_ResolveSpatialShaderDesc.mTarget = shader_target_6_0;
			addShader(pRenderer, &SSSR_ResolveSpatialShaderDesc, &pSSSR_ResolveSpatialShader);

			ShaderLoadDesc SSSR_ResolveTemporalShaderDesc = {};
			SSSR_ResolveTemporalShaderDesc.mStages[0] = { "SSSR_ResolveTemporal.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &SSSR_ResolveTemporalShaderDesc, &pSSSR_ResolveTemporalShader);

			ShaderLoadDesc SSSR_ResolveEAWShaderDesc = {};
			SSSR_ResolveEAWShaderDesc.mStages[0] = { "SSSR_ResolveEaw.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &SSSR_ResolveEAWShaderDesc, &pSSSR_ResolveEAWShader);

			ShaderLoadDesc SSSR_ResolveEAWStride2ShaderDesc = {};
			SSSR_ResolveEAWStride2ShaderDesc.mStages[0] = { "SSSR_ResolveEawStride_2.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &SSSR_ResolveEAWStride2ShaderDesc, &pSSSR_ResolveEAWStride2Shader);

			ShaderLoadDesc SSSR_ResolveEAWStride4ShaderDesc = {};
			SSSR_ResolveEAWStride4ShaderDesc.mStages[0] = { "SSSR_ResolveEawStride_4.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			addShader(pRenderer, &SSSR_ResolveEAWStride4ShaderDesc, &pSSSR_ResolveEAWStride4Shader);
		}
	}
	
	void removeShaders()
	{
		if (gSSSRSupported)
		{
			removeShader(pRenderer, pSSSR_ResolveEAWStride4Shader);
			removeShader(pRenderer, pSSSR_ResolveEAWStride2Shader);
			removeShader(pRenderer, pSSSR_ResolveEAWShader);
			removeShader(pRenderer, pSSSR_ResolveTemporalShader);
			removeShader(pRenderer, pSSSR_ResolveSpatialShader);
			removeShader(pRenderer, pSSSR_IntersectShader);
			removeShader(pRenderer, pSSSR_PrepareIndirectArgsShader);
			removeShader(pRenderer, pSSSR_ClassifyTilesShader);
			removeShader(pRenderer, pSPDShader);
			removeShader(pRenderer, pGenerateMipShader);
			removeShader(pRenderer, pCopyDepthShader);
		}

		removeShader(pRenderer, pPPR_HolePatchingShader);
		removeShader(pRenderer, pPPR_ReflectionShader);
		removeShader(pRenderer, pPPR_ProjectionShader);
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pShaderGbuffers);
	}

	void addPipelines()
	{
		/************************************************************************/
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		TinyImageFormat deferredFormats[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[0][i]->mFormat;
		}

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& deferredPassPipelineSettings = desc.mGraphicsDesc;
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = &depthStateDesc;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;

		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0][0]->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0][0]->mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSigGbuffers;
		deferredPassPipelineSettings.pShaderProgram = pShaderGbuffers;
		deferredPassPipelineSettings.pVertexLayout = &gVertexLayoutModel;
		deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPipelineGbuffers);

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.pDepthState = NULL;

		deferredPassPipelineSettings.mRenderTargetCount = 1;
		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0][0]->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0][0]->mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		deferredPassPipelineSettings.pRootSignature = pSkyboxRootSignature;
		deferredPassPipelineSettings.pShaderProgram = pSkyboxShader;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		deferredPassPipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pSkyboxPipeline);

		TinyImageFormat formats[4] = {};
		formats[0] = pRenderTargetDeferredPass[0][0]->mFormat;
		formats[1] = pSSSR_TemporalResults[0]->mFormat;
		formats[2] = pSSSR_TemporalResults[1]->mFormat;
		formats[3] = pSSSR_RayLength->mFormat;

		deferredPassPipelineSettings.mRenderTargetCount = 4;
		deferredPassPipelineSettings.pColorFormats = formats;
		addPipeline(pRenderer, &desc, &pSkyboxWithClearTexturesPipeline);

		// BRDF
		//Position
		VertexLayout vertexLayoutScreenQuad = {};
		vertexLayoutScreenQuad.mAttribCount = 2;

		vertexLayoutScreenQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutScreenQuad.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutScreenQuad.mAttribs[0].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[0].mLocation = 0;
		vertexLayoutScreenQuad.mAttribs[0].mOffset = 0;

		//Uv
		vertexLayoutScreenQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutScreenQuad.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayoutScreenQuad.mAttribs[1].mLocation = 1;
		vertexLayoutScreenQuad.mAttribs[1].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[1].mOffset = 3 * sizeof(float);    // first attribute contains 3 floats

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSceneBuffer->mFormat;
		pipelineSettings.mSampleCount = pSceneBuffer->mSampleCount;
		pipelineSettings.mSampleQuality = pSceneBuffer->mSampleQuality;

		// pipelineSettings.pDepthState is NULL, pipelineSettings.mDepthStencilFormat should be NONE
		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPipelineBRDF);

		//PPR_Projection
		PipelineDesc computeDesc = {};
		computeDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& cpipelineSettings = computeDesc.mComputeDesc;
		cpipelineSettings.pShaderProgram = pPPR_ProjectionShader;
		cpipelineSettings.pRootSignature = pPPR_ProjectionRootSignature;
		addPipeline(pRenderer, &computeDesc, &pPPR_ProjectionPipeline);

		//PPR_Reflection
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pReflectionBuffer->mFormat;
		pipelineSettings.mSampleCount = pReflectionBuffer->mSampleCount;
		pipelineSettings.mSampleQuality = pReflectionBuffer->mSampleQuality;

		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pPPR_ReflectionRootSignature;
		pipelineSettings.pShaderProgram = pPPR_ReflectionShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPPR_ReflectionPipeline);

		//PPR_HolePatching -> Present
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pPPR_HolePatchingRootSignature;
		pipelineSettings.pShaderProgram = pPPR_HolePatchingShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		addPipeline(pRenderer, &desc, &pPPR_HolePatchingPipeline);

		if (gSSSRSupported)
		{
			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSPDShader;
			cpipelineSettings.pRootSignature = pSPDRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSPDPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pCopyDepthShader;
			cpipelineSettings.pRootSignature = pCopyDepthRootSignature;
			addPipeline(pRenderer, &computeDesc, &pCopyDepthPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pGenerateMipShader;
			cpipelineSettings.pRootSignature = pGenerateMipRootSignature;
			addPipeline(pRenderer, &computeDesc, &pGenerateMipPipeline);

			// SSSR
			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ClassifyTilesShader;
			cpipelineSettings.pRootSignature = pSSSR_ClassifyTilesRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ClassifyTilesPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_PrepareIndirectArgsShader;
			cpipelineSettings.pRootSignature = pSSSR_PrepareIndirectArgsRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_PrepareIndirectArgsPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_IntersectShader;
			cpipelineSettings.pRootSignature = pSSSR_IntersectRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_IntersectPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ResolveSpatialShader;
			cpipelineSettings.pRootSignature = pSSSR_ResolveSpatialRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveSpatialPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ResolveTemporalShader;
			cpipelineSettings.pRootSignature = pSSSR_ResolveTemporalRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveTemporalPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWShader;
			cpipelineSettings.pRootSignature = pSSSR_ResolveEAWRootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWPipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWStride2Shader;
			cpipelineSettings.pRootSignature = pSSSR_ResolveEAWStride2RootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWStride2Pipeline);

			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWStride4Shader;
			cpipelineSettings.pRootSignature = pSSSR_ResolveEAWStride4RootSignature;
			addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWStride4Pipeline);
		}
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pSkyboxWithClearTexturesPipeline);
		removePipeline(pRenderer, pSkyboxPipeline);
		removePipeline(pRenderer, pPPR_ProjectionPipeline);
		removePipeline(pRenderer, pPPR_ReflectionPipeline);
		removePipeline(pRenderer, pPPR_HolePatchingPipeline);
		if (gSSSRSupported)
		{
			removePipeline(pRenderer, pSPDPipeline);
			removePipeline(pRenderer, pCopyDepthPipeline);
			removePipeline(pRenderer, pGenerateMipPipeline);
			removePipeline(pRenderer, pSSSR_ClassifyTilesPipeline);
			removePipeline(pRenderer, pSSSR_PrepareIndirectArgsPipeline);
			removePipeline(pRenderer, pSSSR_IntersectPipeline);
			removePipeline(pRenderer, pSSSR_ResolveSpatialPipeline);
			removePipeline(pRenderer, pSSSR_ResolveTemporalPipeline);
			removePipeline(pRenderer, pSSSR_ResolveEAWPipeline);
			removePipeline(pRenderer, pSSSR_ResolveEAWStride2Pipeline);
			removePipeline(pRenderer, pSSSR_ResolveEAWStride4Pipeline);
		}
		removePipeline(pRenderer, pPipelineGbuffers);
	}

	void addRenderTargets()
	{
		addSceneBuffer();

		addReflectionBuffer();

		addGBuffers();

		addDepthBuffer();

		addIntermeditateBuffer();
	}

	void removeRenderTargets()
	{
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pSceneBuffer);
		removeRenderTarget(pRenderer, pReflectionBuffer);
		removeResource(pIntermediateBuffer);
		removeRenderTarget(pRenderer, pSSSR_TemporalResults[0]);
		removeRenderTarget(pRenderer, pSSSR_TemporalResults[1]);
		removeResource(pSSSR_DepthHierarchy);
		removeResource(pSSSR_TemporalVariance);
		removeRenderTarget(pRenderer, pSSSR_RayLength);
		removeResource(pSSSR_RayListBuffer);
		removeResource(pSSSR_TileListBuffer);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[0][i]);

		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[1][1]);
		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[1][2]);
	}

	bool addSceneBuffer()
	{
		RenderTargetDesc sceneRT = {};
		sceneRT.mArraySize = 1;
		sceneRT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		sceneRT.mDepth = 1;
		sceneRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		sceneRT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		sceneRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

		sceneRT.mHeight = mSettings.mHeight;
		sceneRT.mWidth = mSettings.mWidth;

		sceneRT.mSampleCount = SAMPLE_COUNT_1;
		sceneRT.mSampleQuality = 0;
        sceneRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		sceneRT.pName = "Scene Buffer";

		addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

		return pSceneBuffer != NULL;
	}

	bool addReflectionBuffer()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		RT.mDepth = 1;
		RT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		RT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

		RT.mHeight = mSettings.mHeight;
		RT.mWidth = mSettings.mWidth;

		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
        RT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		RT.pName = "Reflection Buffer";

		addRenderTarget(pRenderer, &RT, &pReflectionBuffer);

		return pReflectionBuffer != NULL;
	}

	bool addGBuffers()
	{
		ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;
		deferredRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		deferredRTDesc.mWidth = mSettings.mWidth;
		deferredRTDesc.mHeight = mSettings.mHeight;
		deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        deferredRTDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		deferredRTDesc.pName = "G-Buffer RTs";

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			if (i == 1 || i == 2)
				deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			else if (i == 3)
				deferredRTDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
			else
				deferredRTDesc.mFormat = getRecommendedSwapchainFormat(true, true);

			if (i == 2)
				deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
			else if (i == 3)
				deferredRTDesc.mClearValue = optimizedColorClearBlack;

			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[0][i]);
			pRenderTargetDeferredPass[1][i] = pRenderTargetDeferredPass[0][i];
		}

		deferredRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][1]);

		deferredRTDesc.mClearValue = { { 1.0f, 0.0f, 0.0f, 0.0f } };
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[1][2]);

		for (int i1 = 0; i1 < 2; i1++)
		{
			for (int i2 = 0; i2 < DEFERRED_RT_COUNT; i2++)
			{
				if (pRenderTargetDeferredPass[i1][i2] == NULL)
					return false;
			}
		}

		return true;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { { 1.0f, 0 } };
		depthRT.mDepth = 1;
		depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		depthRT.pName = "Depth Buffer";
		//fixes flickering issues related to depth buffer being recycled.
#ifdef METAL
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	bool addIntermeditateBuffer()
	{
        int bufferSize = mSettings.mWidth * mSettings.mHeight * pSwapChain->ppRenderTargets[0]->mArraySize;

		// Add Intermediate buffer
		BufferLoadDesc IntermediateBufferDesc = {};
		IntermediateBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		IntermediateBufferDesc.mDesc.mElementCount = bufferSize;
		IntermediateBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		IntermediateBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		IntermediateBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		IntermediateBufferDesc.mDesc.mSize = IntermediateBufferDesc.mDesc.mStructStride * bufferSize;

		gInitializeVal.clear();
		for (int i = 0; i < bufferSize; i++)
		{
			gInitializeVal.push_back(UINT32_MAX);
		}

		IntermediateBufferDesc.pData = gInitializeVal.data();
		IntermediateBufferDesc.ppBuffer = &pIntermediateBuffer;
		addResource(&IntermediateBufferDesc, NULL);

		if (pIntermediateBuffer == NULL)
			return false;

		TextureDesc depthHierarchyDesc = {};
		depthHierarchyDesc.mArraySize = 1;
		depthHierarchyDesc.mDepth = 1;
		depthHierarchyDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		depthHierarchyDesc.mHeight = mSettings.mHeight;
		depthHierarchyDesc.mWidth = mSettings.mWidth;
		depthHierarchyDesc.mMipLevels = static_cast<uint32_t>(log2(eastl::max(mSettings.mWidth, mSettings.mHeight))) + 1;
		depthHierarchyDesc.mSampleCount = SAMPLE_COUNT_1;
		depthHierarchyDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		depthHierarchyDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        depthHierarchyDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		depthHierarchyDesc.pName = "SSSR_DepthHierarchy";

		TextureLoadDesc depthHierarchyLoadDesc = {};
		depthHierarchyLoadDesc.pDesc = &depthHierarchyDesc;
		depthHierarchyLoadDesc.ppTexture = &pSSSR_DepthHierarchy;
		addResource(&depthHierarchyLoadDesc, NULL);

		if (pSSSR_DepthHierarchy == NULL)
			return false;

		RenderTargetDesc intersectResultsDesc = {};
		intersectResultsDesc.mArraySize = 1;
		intersectResultsDesc.mDepth = 1;
		intersectResultsDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		intersectResultsDesc.mHeight = mSettings.mHeight;
		intersectResultsDesc.mWidth = mSettings.mWidth;
		intersectResultsDesc.mMipLevels = 1;
		intersectResultsDesc.mSampleCount = SAMPLE_COUNT_1;
		intersectResultsDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		intersectResultsDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
		intersectResultsDesc.pName = "pSSSR_TemporalResults";
		intersectResultsDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        intersectResultsDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &intersectResultsDesc, &pSSSR_TemporalResults[0]);

		if (pSSSR_TemporalResults[0] == NULL)
			return false;

		addRenderTarget(pRenderer, &intersectResultsDesc, &pSSSR_TemporalResults[1]);

		if (pSSSR_TemporalResults[1] == NULL)
			return false;

		TextureDesc temporalVarianceDesc = {};
		temporalVarianceDesc.mArraySize = 1;
		temporalVarianceDesc.mDepth = 1;
		temporalVarianceDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		temporalVarianceDesc.mHeight = mSettings.mHeight;
		temporalVarianceDesc.mWidth = mSettings.mWidth;
		temporalVarianceDesc.mMipLevels = 1;
		temporalVarianceDesc.mSampleCount = SAMPLE_COUNT_1;
		temporalVarianceDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		temporalVarianceDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        temporalVarianceDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		temporalVarianceDesc.pName = "SSSR_TemporalVariance";

		TextureLoadDesc temporalVarianceLoadDesc = {};
		temporalVarianceLoadDesc.pDesc = &temporalVarianceDesc;
		temporalVarianceLoadDesc.ppTexture = &pSSSR_TemporalVariance;
		addResource(&temporalVarianceLoadDesc, NULL);

		if (pSSSR_TemporalVariance == NULL)
			return false;

		RenderTargetDesc rayLengthDesc = {};
		rayLengthDesc.mArraySize = 1;
		rayLengthDesc.mDepth = 1;
		rayLengthDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		rayLengthDesc.mHeight = mSettings.mHeight;
		rayLengthDesc.mWidth = mSettings.mWidth;
		rayLengthDesc.mMipLevels = 1;
		rayLengthDesc.mSampleCount = SAMPLE_COUNT_1;
		rayLengthDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		rayLengthDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
		rayLengthDesc.pName = "SSSR_RayLength";
		rayLengthDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        rayLengthDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &rayLengthDesc, &pSSSR_RayLength);

		if (pSSSR_RayLength == NULL)
			return false;

		gClearTextures = true;

		BufferLoadDesc SSSR_RayListDesc = {};
		SSSR_RayListDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		SSSR_RayListDesc.mDesc.mElementCount = bufferSize;
		SSSR_RayListDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_RayListDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_RayListDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_RayListDesc.mDesc.mSize = SSSR_RayListDesc.mDesc.mStructStride * bufferSize;
		SSSR_RayListDesc.mDesc.pName = "SSSR_RayListBuffer";
		SSSR_RayListDesc.pData = NULL;
		SSSR_RayListDesc.ppBuffer = &pSSSR_RayListBuffer;
		addResource(&SSSR_RayListDesc, NULL);

		if (pSSSR_RayListBuffer == NULL)
			return false;

		BufferLoadDesc SSSR_TileListDesc = {};
		SSSR_TileListDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		SSSR_TileListDesc.mDesc.mElementCount = ((mSettings.mWidth * mSettings.mHeight + 63) / 64) * pSwapChain->ppRenderTargets[0]->mArraySize;
		SSSR_TileListDesc.mDesc.mStructStride = sizeof(uint32_t);
		SSSR_TileListDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		SSSR_TileListDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		SSSR_TileListDesc.mDesc.mSize = SSSR_TileListDesc.mDesc.mStructStride * SSSR_TileListDesc.mDesc.mElementCount;
		SSSR_TileListDesc.mDesc.pName = "SSSR_TileListBuffer";
		SSSR_TileListDesc.pData = NULL;
		SSSR_TileListDesc.ppBuffer = &pSSSR_TileListBuffer;
		addResource(&SSSR_TileListDesc, NULL);

		return pSSSR_TileListBuffer != NULL;
	}
};

void assignSponzaTextures()
{
	int AO = 5;
	int NoMetallic = 6;

	//00 : leaf
	gSponzaTextureIndexforMaterial.push_back(66);
	gSponzaTextureIndexforMaterial.push_back(67);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(68);
	gSponzaTextureIndexforMaterial.push_back(AO);

	//01 : vase_round
	gSponzaTextureIndexforMaterial.push_back(78);
	gSponzaTextureIndexforMaterial.push_back(79);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(80);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 02 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
	gSponzaTextureIndexforMaterial.push_back(AO);

	//03 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 05 : bricks
	gSponzaTextureIndexforMaterial.push_back(22);
	gSponzaTextureIndexforMaterial.push_back(23);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(24);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 06 :  arch
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 07 : ceiling
	gSponzaTextureIndexforMaterial.push_back(25);
	gSponzaTextureIndexforMaterial.push_back(26);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(27);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 08 : column_a
	gSponzaTextureIndexforMaterial.push_back(28);
	gSponzaTextureIndexforMaterial.push_back(29);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(30);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 09 : Floor
	gSponzaTextureIndexforMaterial.push_back(60);
	gSponzaTextureIndexforMaterial.push_back(61);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(6);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 10 : column_c
	gSponzaTextureIndexforMaterial.push_back(34);
	gSponzaTextureIndexforMaterial.push_back(35);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(36);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 11 : details
	gSponzaTextureIndexforMaterial.push_back(45);
	gSponzaTextureIndexforMaterial.push_back(47);
	gSponzaTextureIndexforMaterial.push_back(46);
	gSponzaTextureIndexforMaterial.push_back(48);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 12 : column_b
	gSponzaTextureIndexforMaterial.push_back(31);
	gSponzaTextureIndexforMaterial.push_back(32);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(33);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 13 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

DEFINE_APPLICATION_MAIN(ScreenSpaceReflections)
