/*
*
* Copyright (c) 2018-2019 Confetti Interactive Inc.
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

//assimp
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/Atomics.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"

//Input
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

#include "../../../../Common_3/OS/Core/ThreadSystem.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

const char* pszBases[FSR_Count] = {
	"../../../src/10_PixelProjectedReflections/",    // FSR_BinShaders
	"../../../src/10_PixelProjectedReflections/",    // FSR_SrcShaders
	"../../../../../Art/Sponza/",                    // FSR_Textures
	"../../../../../Art/Sponza/",                    // FSR_Meshes
	"../../../UnitTestResources/",                   // FSR_Builtin_Fonts
	"../../../src/10_PixelProjectedReflections/",    // FSR_GpuConfig
	"",                                              // FSR_Animation
	"",                                              // FSR_Audio
	"../../../UnitTestResources/Textures/",          // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",             // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",               // FSR_MIDDLEWARE_UI
};


#define DEFERRED_RT_COUNT 3
#define MAX_PLANES 4

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mProjectView;
	vec3 mCamPos;
};

// Have a uniform for extended camera data
struct UniformExtendedCamData
{
	mat4 mViewMat;
	mat4 mProjMat;
	mat4 mViewProjMat;
	mat4 mInvViewProjMat;

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
	float _pad0;
	float _pad1;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present
	Light mLights[16];    // array of lights seem to be broken so just a single light for now
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

enum
{
	SCENE_ONLY = 0,
	PPR_ONLY = 1,
	SCENE_WITH_PPR = 2,
	SCENE_EXCLU_PPR = 3,
};

static bool gUseHolePatching = true;
static bool gUseExpensiveHolePatching = true;

static bool gUseNormalMap = false;
static bool gUseFadeEffect = true;

static uint32_t gRenderMode = SCENE_WITH_PPR;

static uint32_t gPlaneNumber = 1;
static float    gPlaneSize = 75.0f;
static float    gRRP_Intensity = 0.2f;

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

// PBR Texture values (these values are mirrored on the shaders).
const uint32_t gBRDFIntegrationSize = 512;
const uint32_t gSkyboxSize = 1024;
const uint32_t gSkyboxMips = 11;
const uint32_t gIrradianceSize = 32;
const uint32_t gSpecularSize = 128;
const uint32_t gSpecularMips = 5;

const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

bool           gToggleVSync = false;

Renderer* pRenderer = NULL;
UIApp     gAppUI;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

CmdPool* pPreCmdPool = NULL;
Cmd**    pPrepCmds = NULL;

CmdPool* pBrdfCmdPool = NULL;
Cmd**    pBrdfCmds = NULL;

CmdPool* pPPR_ProjectionCmdPool = NULL;
Cmd**    pPPR_ProjectionCmds = NULL;

CmdPool* pPPR_ReflectionCmdPool = NULL;
Cmd**    pPPR_ReflectionCmds = NULL;

SwapChain* pSwapChain = NULL;

RenderTarget* pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { NULL };

RenderTarget* pSceneBuffer = NULL;
RenderTarget* pReflectionBuffer = NULL;

RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*           pShaderBRDF = NULL;
Pipeline*         pPipelineBRDF = NULL;
RootSignature*    pRootSigBRDF = NULL;

Buffer*           pSkyboxVertexBuffer = NULL;
Shader*           pSkyboxShader = NULL;
Pipeline*         pSkyboxPipeline = NULL;
RootSignature*    pSkyboxRootSignature = NULL;

Shader*           pPPR_ProjectionShader = NULL;
RootSignature*    pPPR_ProjectionRootSignature = NULL;
Pipeline*         pPPR_ProjectionPipeline = NULL;

Shader*           pPPR_ReflectionShader = NULL;
RootSignature*    pPPR_ReflectionRootSignature = NULL;
Pipeline*         pPPR_ReflectionPipeline = NULL;

Shader*           pPPR_HolePatchingShader = NULL;
RootSignature*    pPPR_HolePatchingRootSignature = NULL;
Pipeline*         pPPR_HolePatchingPipeline = NULL;

Buffer* pScreenQuadVertexBuffer = NULL;

Shader*           pShaderGbuffers = NULL;
Pipeline*         pPipelineGbuffers = NULL;
RootSignature*    pRootSigGbuffers = NULL;

DescriptorBinder* pDescriptorBinder = NULL;

Texture* pSkybox = NULL;
Texture* pBRDFIntegrationMap = NULL;
Texture* pIrradianceMap = NULL;
Texture* pSpecularMap = NULL;

Buffer* pIntermediateBuffer = NULL;

#define TOTAL_IMGS 84
Texture* pMaterialTextures[TOTAL_IMGS];

eastl::vector<int> gSponzaTextureIndexforMaterial;

//For clearing Intermediate Buffer
eastl::vector<uint32_t> gInitializeVal;

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

UniformObjData pUniformDataMVP;

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
UniformCamData pUniformDataCamera;

UniformCamData gUniformDataSky;

Buffer*                pBufferUniformExtendedCamera[gImageCount] = { NULL };
UniformExtendedCamData pUniformDataExtenedCamera;

Buffer* pBufferUniformCameraSky[gImageCount] = { NULL };

Buffer*           pBufferUniformPPRPro[gImageCount] = { NULL };
UniformPPRProData pUniformPPRProData;

Buffer*          pBufferUniformLights = NULL;
UniformLightData pUniformDataLights;

Buffer*                     pBufferUniformDirectionalLights = NULL;
UniformDirectionalLightData pUniformDataDirectionalLights;

Buffer*              pBufferUniformPlaneInfo[gImageCount] = { NULL };
UniformPlaneInfoData pUniformDataPlaneInfo;

Shader*   pShaderPostProc = NULL;
Pipeline* pPipelinePostProc = NULL;

DepthState*      pDepth = NULL;
RasterizerState* pRasterstateDefault = NULL;
Sampler*         pSamplerBilinear = NULL;
Sampler*         pSamplerLinear = NULL;

Sampler* pSamplerNearest = NULL;

uint32_t gFrameIndex = 0;

GpuProfiler* pGpuProfiler = NULL;

BlendState* pBlendStateOneZero = NULL;

eastl::vector<Buffer*> gSphereBuffers;

ICameraController* pCameraController = NULL;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

GuiComponent* pGui = NULL;
GuiComponent* pLoadingGui = NULL;

ThreadSystem* pIOThreads = NULL;

const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

static const size_t TextureLoadingProgress = 128;
static const size_t TextureGPUProgress = 64;
static const size_t TextureTotalProgress = TextureLoadingProgress + TextureGPUProgress;
static const size_t ModelLoadingProgress = 1024 * 64;
static const size_t ModelGPUProgress = 32;
static const size_t ModelTotalProgress = ModelLoadingProgress + ModelGPUProgress;

struct MeshVertex
{
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

struct Mesh
{
	Buffer*                   pVertexBuffer = NULL;
	Buffer*                   pIndexBuffer = NULL;
	eastl::vector<uint32_t> materialID;
	struct CmdParam
	{
		uint32_t indexCount, startIndex, startVertex;
	};
	eastl::vector<CmdParam> cmdArray;
};

const char* gModelNames[2] = { "sponza.obj", "lion.obj" };
Mesh        gModels[2];

void transitionRenderTargets()
{
	// Transition render targets to desired state
	const uint32_t numBarriers = gImageCount + 1;
	TextureBarrier rtBarriers[numBarriers] = {};
	for (uint32_t i = 0; i < gImageCount; ++i)
		rtBarriers[i] = { pSwapChain->ppSwapchainRenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
	rtBarriers[numBarriers - 1] = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
	beginCmd(ppCmds[0]);
	cmdResourceBarrier(ppCmds[0], 0, 0, numBarriers, rtBarriers, false);
	endCmd(ppCmds[0]);
	queueSubmit(pGraphicsQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
	waitForFences(pRenderer, 1, &pRenderCompleteFences[0]);
}

void assignSponzaTextures();

void unloadMesh(Mesh& mesh)
{
	if (mesh.pVertexBuffer)
		removeResource(mesh.pVertexBuffer);
	if (mesh.pIndexBuffer)
		removeResource(mesh.pIndexBuffer);
}

class PixelProjectedReflections: public IApp
{
	size_t mPrevProgressBarValue = 0, mProgressBarValue = 0, mProgressBarValueMax = 0;
	size_t mAtomicProgress = 0;

	public:
	PixelProjectedReflections()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
		initThreadSystem(&pIOThreads);

		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create command pool and create a cmd buffer for each swapchain image
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPreCmdPool);
		addCmd_n(pPreCmdPool, false, gImageCount, &pPrepCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pBrdfCmdPool);
		addCmd_n(pBrdfCmdPool, false, gImageCount, &pBrdfCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPPR_ProjectionCmdPool);
		addCmd_n(pPPR_ProjectionCmdPool, false, gImageCount, &pPPR_ProjectionCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pPPR_ReflectionCmdPool);
		addCmd_n(pPPR_ReflectionCmdPool, false, gImageCount, &pPPR_ReflectionCmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Textures))
			return false;
#endif

		initProfiler(pRenderer);
		profileRegisterInput();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");
		ComputePBRMaps();

		SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);

		SamplerDesc nearstSamplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
										  ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &nearstSamplerDesc, &pSamplerNearest);

		// GBuffer
		ShaderMacro    totalImagesShaderMacro = { "TOTAL_IMGS", eastl::string().sprintf("%i", TOTAL_IMGS) };
		ShaderLoadDesc gBuffersShaderDesc = {};
		gBuffersShaderDesc.mStages[0] = { "fillGbuffers.vert", NULL, 0, FSR_SrcShaders };
#ifndef TARGET_IOS
		gBuffersShaderDesc.mStages[1] = { "fillGbuffers.frag", &totalImagesShaderMacro, 1, FSR_SrcShaders };
#else
		gBuffersShaderDesc.mStages[1] = { "fillGbuffers_iOS.frag", NULL, 0, FSR_SrcShaders };
#endif
		addShader(pRenderer, &gBuffersShaderDesc, &pShaderGbuffers);

		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler*    pStaticSamplers[] = { pSamplerBilinear };

		RootSignatureDesc gBuffersRootDesc = { &pShaderGbuffers, 1 };
		gBuffersRootDesc.mStaticSamplerCount = 1;
		gBuffersRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		gBuffersRootDesc.ppStaticSamplers = pStaticSamplers;

#ifndef TARGET_IOS
		gBuffersRootDesc.mMaxBindlessTextures = TOTAL_IMGS;
#endif
		addRootSignature(pRenderer, &gBuffersRootDesc, &pRootSigGbuffers);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

		const char*       pSkyboxamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pSkyboxShader, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pSkyboxRootSignature);

		//BRDF
		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, FSR_SrcShaders };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

		const char* pStaticSampler2Names[] = { "envSampler", "defaultSampler" };
		Sampler*    pStaticSamplers2[] = { pSamplerBilinear, pSamplerNearest };

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 2;
		brdfRootDesc.ppStaticSamplerNames = pStaticSampler2Names;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers2;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		//PPR_Projection
		ShaderLoadDesc PPR_ProjectionShaderDesc = {};
		PPR_ProjectionShaderDesc.mStages[0] = { "PPR_Projection.comp", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &PPR_ProjectionShaderDesc, &pPPR_ProjectionShader);

		RootSignatureDesc PPR_PRootDesc = { &pPPR_ProjectionShader, 1 };
		addRootSignature(pRenderer, &PPR_PRootDesc, &pPPR_ProjectionRootSignature);

		//PPR_Reflection
		ShaderLoadDesc PPR_ReflectionShaderDesc = {};
		PPR_ReflectionShaderDesc.mStages[0] = { "PPR_Reflection.vert", NULL, 0, FSR_SrcShaders };
		PPR_ReflectionShaderDesc.mStages[1] = { "PPR_Reflection.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &PPR_ReflectionShaderDesc, &pPPR_ReflectionShader);

		RootSignatureDesc PPR_RRootDesc = { &pPPR_ReflectionShader, 1 };
		PPR_RRootDesc.mStaticSamplerCount = 1;
		PPR_RRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		PPR_RRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &PPR_RRootDesc, &pPPR_ReflectionRootSignature);

		//PPR_HolePatching
		ShaderLoadDesc PPR_HolePatchingShaderDesc = {};
		PPR_HolePatchingShaderDesc.mStages[0] = { "PPR_Holepatching.vert", NULL, 0, FSR_SrcShaders };
		PPR_HolePatchingShaderDesc.mStages[1] = { "PPR_Holepatching.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &PPR_HolePatchingShaderDesc, &pPPR_HolePatchingShader);

		const char* pStaticSamplerforHolePatchingNames[] = { "nearestSampler", "bilinearSampler" };
		Sampler*    pStaticSamplersforHolePatching[] = { pSamplerNearest, pSamplerBilinear };

		RootSignatureDesc PPR_HolePatchingRootDesc = { &pPPR_HolePatchingShader, 1 };
		PPR_HolePatchingRootDesc.mStaticSamplerCount = 2;
		PPR_HolePatchingRootDesc.ppStaticSamplerNames = pStaticSamplerforHolePatchingNames;
		PPR_HolePatchingRootDesc.ppStaticSamplers = pStaticSamplersforHolePatching;
		addRootSignature(pRenderer, &PPR_HolePatchingRootDesc, &pPPR_HolePatchingRootSignature);

		// Create descriptor binder
		DescriptorBinderDesc descriptorBinderDesc[] = {
			{ pRootSigGbuffers, 0, MODEL_COUNT },
			{ pSkyboxRootSignature },
			{ pRootSigBRDF, 1 },
			{ pPPR_ProjectionRootSignature },
			{ pPPR_ReflectionRootSignature },
			{ pPPR_HolePatchingRootSignature }
		};
		addDescriptorBinder(pRenderer, 0, sizeof(descriptorBinderDesc) / sizeof(*descriptorBinderDesc), descriptorBinderDesc, &pDescriptorBinder);

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterstateDefault);

		BufferLoadDesc sponza_buffDesc = {};
		sponza_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sponza_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sponza_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		sponza_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sponza_buffDesc.pData = NULL;
		sponza_buffDesc.ppBuffer = &pSponzaBuffer;
		addResource(&sponza_buffDesc);

		BufferLoadDesc lion_buffDesc = {};
		lion_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lion_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lion_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		lion_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lion_buffDesc.pData = NULL;
		lion_buffDesc.ppBuffer = &pLionBuffer;
		addResource(&lion_buffDesc);

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
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc);

		float screenQuadPoints[] = {
			-1.0f, 3.0f, 0.5f, 0.0f, -1.0f, -1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
		};

		uint64_t       screenQuadDataSize = 5 * 3 * sizeof(float);
		BufferLoadDesc screenQuadVbDesc = {};
		screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		screenQuadVbDesc.mDesc.mSize = screenQuadDataSize;
		screenQuadVbDesc.mDesc.mVertexStride = sizeof(float) * 5;
		screenQuadVbDesc.pData = screenQuadPoints;
		screenQuadVbDesc.ppBuffer = &pScreenQuadVertexBuffer;
		addResource(&screenQuadVbDesc);

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
			addResource(&ubCamDesc);
			ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
			addResource(&ubCamDesc);
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
			addResource(&ubECamDesc);
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
			addResource(&ubPPR_ProDesc);
		}

		// Uniform buffer for light data
		BufferLoadDesc ubLightsDesc = {};
		ubLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
		ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubLightsDesc.pData = NULL;
		ubLightsDesc.ppBuffer = &pBufferUniformLights;
		addResource(&ubLightsDesc);

		// Uniform buffer for DirectionalLight data
		BufferLoadDesc ubDLightsDesc = {};
		ubDLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ubDLightsDesc.mDesc.mSize = sizeof(UniformDirectionalLightData);
		ubDLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		ubDLightsDesc.pData = NULL;
		ubDLightsDesc.ppBuffer = &pBufferUniformDirectionalLights;
		addResource(&ubDLightsDesc);

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
			addResource(&ubPlaneInfoDesc);
		}

		finishResourceLoading();

		// prepare resources

		// Update the uniform buffer for the objects
		mat4 sponza_modelmat = mat4::translation(vec3(0.0f, -6.0f, 0.0f)) * mat4::scale(vec3(0.02f, 0.02f, 0.02f));
		pUniformDataMVP.mWorldMat = sponza_modelmat;
		pUniformDataMVP.mMetallic = 0;
		pUniformDataMVP.mRoughness = 0.5f;
		pUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc sponza_objBuffUpdateDesc = { pSponzaBuffer, &pUniformDataMVP };
		updateResource(&sponza_objBuffUpdateDesc);

		// Update the uniform buffer for the objects
		mat4 lion_modelmat = mat4::translation(vec3(0.0f, -6.0f, 1.0f)) * mat4::rotationY(-1.5708f) * mat4::scale(vec3(0.2f, 0.2f, 0.2f));
		pUniformDataMVP.mWorldMat = lion_modelmat;
		pUniformDataMVP.mMetallic = 0;
		pUniformDataMVP.mRoughness = 0.5f;
		pUniformDataMVP.pbrMaterials = 1;
		BufferUpdateDesc lion_objBuffUpdateDesc = { pLionBuffer, &pUniformDataMVP };
		updateResource(&lion_objBuffUpdateDesc);

		// Add light to scene

		//Point light
		Light light;
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[0] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(-12.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[1] = light;

		// Add light to scene
		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, 4.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[2] = light;

		light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
		light.mPos = vec4(9.5f, -3.5f, -3.7f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 400.0f;

		pUniformDataLights.mLights[3] = light;

		pUniformDataLights.mCurrAmountOfLights = 4;
		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights, &pUniformDataLights };
		updateResource(&lightBuffUpdateDesc);

		//Directional light
		DirectionalLight dLight;
		dLight.mCol = vec4(1.0f, 1.0f, 1.0f, 5.0f);
		dLight.mPos = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		dLight.mDir = vec4(-1.0f, -1.5f, 1.0f, 0.0f);

		pUniformDataDirectionalLights.mLights[0] = dLight;
		pUniformDataDirectionalLights.mCurrAmountOfDLights = 1;
		BufferUpdateDesc directionalLightBuffUpdateDesc = { pBufferUniformDirectionalLights, &pUniformDataDirectionalLights };
		updateResource(&directionalLightBuffUpdateDesc);

		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(370.0f / dpiScale, 320.0f / dpiScale);
		guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());

		pGui = gAppUI.AddGuiComponent("Pixel-Projected Reflections", &guiDesc);

    pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

		static const uint32_t enumRenderModes[] = { SCENE_ONLY, PPR_ONLY, SCENE_WITH_PPR, SCENE_EXCLU_PPR, 0 };

		static const char* enumRenderModeNames[] = { "Render Scene Only", "Render PPR Only", "Render Scene with PPR ",
													 "Render Scene with exclusive PPR", NULL };

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGui->AddWidget(CheckboxWidget("Toggle VSync", &gToggleVSync));
#endif

		pGui->AddWidget(DropdownWidget("Render Mode", &gRenderMode, enumRenderModeNames, enumRenderModes, 4));

		pGui->AddWidget(CheckboxWidget("Use Holepatching", &gUseHolePatching));
		pGui->AddWidget(CheckboxWidget("Use Expensive Holepatching", &gUseExpensiveHolePatching));

		//pGui->AddWidget(CheckboxWidget("Use Normalmap", &gUseNormalMap));

		pGui->AddWidget(CheckboxWidget("Use Fade Effect", &gUseFadeEffect));

		pGui->AddWidget(SliderFloatWidget("Intensity of PPR", &gRRP_Intensity, 0.0f, 1.0f));
		pGui->AddWidget(SliderUintWidget("Number of Planes", &gPlaneNumber, 1, 4));
		pGui->AddWidget(SliderFloatWidget("Size of Main Plane", &gPlaneSize, 5.0f, 100.0f));

		GuiDesc guiDesc2 = {};
		guiDesc2.mStartPosition = vec2(700.0f / dpiScale, 450.0f / dpiScale);
		pLoadingGui = gAppUI.AddGuiComponent("Loading", &guiDesc2);

		mProgressBarValueMax = 2 * ModelTotalProgress + TOTAL_IMGS * TextureTotalProgress;
		ProgressBarWidget ProgressBar("               [ProgressBar]               ", &mProgressBarValue, mProgressBarValueMax);
		pLoadingGui->AddWidget(ProgressBar);

		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 20.0f, -2.0f, 0.9f };
		vec3                   lookAt{ 0.0f, -2.0f, 0.9f };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif

		pCameraController->setMotionParameters(camParameters);

		InputSystem::RegisterInputEvent(cameraInputEvent);

		// In case copy queue should be externally synchronized(e.g. Vulkan with 1 queueu)
		// do resource streaming after resource loading completed.
		// Shared queue won't be used concurrently,
		// unless someone will try to update GPU only or GPU-to-CPU resource after this point
		addThreadSystemRangeTask(
			pIOThreads, &memberTaskFunc<PixelProjectedReflections, &PixelProjectedReflections::loadTexture>, this, TOTAL_IMGS);

		addThreadSystemRangeTask(
			pIOThreads, &memberTaskFunc<PixelProjectedReflections, &PixelProjectedReflections::loadMesh>, this, 2);

		assignSponzaTextures();

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);
		destroyCameraController(pCameraController);

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeResource(pSpecularMap);
		removeResource(pIrradianceMap);
		removeResource(pSkybox);
		removeResource(pBRDFIntegrationMap);

		removeResource(pSponzaBuffer);
		removeResource(pLionBuffer);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		removeGpuProfiler(pRenderer, pGpuProfiler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferUniformPlaneInfo[i]);
			removeResource(pBufferUniformPPRPro[i]);
			removeResource(pBufferUniformExtendedCamera[i]);
			removeResource(pBufferUniformCameraSky[i]);
			removeResource(pBufferUniformCamera[i]);
		}

		// Remove streamer before removing any actual resources
		// otherwise we might delete a resource while uploading to it.
		shutdownThreadSystem(pIOThreads);
		finishResourceLoading();

		processWaitQueue();

		removeResource(pBufferUniformLights);
		removeResource(pBufferUniformDirectionalLights);
		removeResource(pSkyboxVertexBuffer);
		removeResource(pScreenQuadVertexBuffer);

		for (Mesh& model : gModels)
			unloadMesh(model);

		gAppUI.Exit();

		removeShader(pRenderer, pPPR_HolePatchingShader);
		removeShader(pRenderer, pPPR_ReflectionShader);
		removeShader(pRenderer, pPPR_ProjectionShader);
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pSkyboxShader);
		removeShader(pRenderer, pShaderGbuffers);

		removeDepthState(pDepth);
		removeRasterizerState(pRasterstateDefault);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerNearest);

		removeRootSignature(pRenderer, pPPR_HolePatchingRootSignature);
		removeRootSignature(pRenderer, pPPR_ReflectionRootSignature);
		removeRootSignature(pRenderer, pPPR_ProjectionRootSignature);
		removeRootSignature(pRenderer, pRootSigBRDF);
		removeRootSignature(pRenderer, pSkyboxRootSignature);
		removeRootSignature(pRenderer, pRootSigGbuffers);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		// Remove commands and command pool&
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeQueue(pGraphicsQueue);

		removeCmd_n(pPreCmdPool, gImageCount, pPrepCmds);
		removeCmdPool(pRenderer, pPreCmdPool);

		removeCmd_n(pBrdfCmdPool, gImageCount, pBrdfCmds);
		removeCmdPool(pRenderer, pBrdfCmdPool);

		removeCmd_n(pPPR_ProjectionCmdPool, gImageCount, pPPR_ProjectionCmds);
		removeCmdPool(pRenderer, pPPR_ProjectionCmdPool);

		removeCmd_n(pPPR_ReflectionCmdPool, gImageCount, pPPR_ReflectionCmds);
		removeCmdPool(pRenderer, pPPR_ReflectionCmdPool);

		for (uint i = 0; i < TOTAL_IMGS; ++i)
		{
			if (pMaterialTextures[i])
			{
				removeResource(pMaterialTextures[i]);
			}
		}

		// Remove resource loader and renderer
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

		void ComputePBRMaps()
	{
		Texture*          pPanoSkybox = NULL;
		Shader*           pPanoToCubeShader = NULL;
		RootSignature*    pPanoToCubeRootSignature = NULL;
		Pipeline*         pPanoToCubePipeline = NULL;
		Shader*           pBRDFIntegrationShader = NULL;
		RootSignature*    pBRDFIntegrationRootSignature = NULL;
		Pipeline*         pBRDFIntegrationPipeline = NULL;
		Shader*           pIrradianceShader = NULL;
		RootSignature*    pIrradianceRootSignature = NULL;
		Pipeline*         pIrradiancePipeline = NULL;
		Shader*           pSpecularShader = NULL;
		RootSignature*    pSpecularRootSignature = NULL;
		Pipeline*         pSpecularPipeline = NULL;
		Sampler*          pSkyboxSampler = NULL;
		DescriptorBinder* pPBRDescriptorBinder = NULL;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		SyncToken       token = 0;
		TextureLoadDesc panoDesc = {};
		panoDesc.mRoot = FSR_OtherFiles;
		panoDesc.pFilename = "LA_Helipad";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, &token);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = ImageFormat::RGBA32F;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mSrgb = false;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pSkybox;
		addResource(&skyboxLoadDesc);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = ImageFormat::RGBA32F;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mSrgb = false;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pDebugName = L"irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pIrradianceMap;
		addResource(&irrLoadDesc);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = ImageFormat::RGBA32F;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mSrgb = false;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pDebugName = L"specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pSpecularMap;
		addResource(&specImgLoadDesc);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc     brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationDesc.mHostVisible = false;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

		GPUPresetLevel presetLevel = pRenderer->mGpuSettings->mGpuVendorPreset.mPresetLevel;
		uint32_t       importanceSampleCounts[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 64, 128, 256, 1024 };
		uint32_t       importanceSampleCount = importanceSampleCounts[presetLevel];
		ShaderMacro    importanceSampleMacro = { "IMPORTANCE_SAMPLE_COUNT", eastl::string().sprintf("%u", importanceSampleCount) };

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);

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
		addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);
		addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
		addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);

		DescriptorBinderDesc descriptorBinderDesc[4] = { { pPanoToCubeRootSignature, gSkyboxMips + 1, gSkyboxMips + 1 },
														 { pBRDFIntegrationRootSignature },
														 { pIrradianceRootSignature },
														 { pSpecularRootSignature, gSkyboxMips + 1, gSpecularMips + 1 } };

		addDescriptorBinder(pRenderer, 0, 4, descriptorBinderDesc, &pPBRDescriptorBinder);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &desc, &pPanoToCubePipeline);
		pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
		pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
		addPipeline(pRenderer, &desc, &pBRDFIntegrationPipeline);
		pipelineSettings.pShaderProgram = pIrradianceShader;
		pipelineSettings.pRootSignature = pIrradianceRootSignature;
		addPipeline(pRenderer, &desc, &pIrradiancePipeline);
		pipelineSettings.pShaderProgram = pSpecularShader;
		pipelineSettings.pRootSignature = pSpecularRootSignature;
		addPipeline(pRenderer, &desc, &pSpecularPipeline);

		// Compute the BRDF Integration map.
		beginCmd(ppCmds[0]);

		TextureBarrier uavBarriers[4] = {
			{ pSkybox, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS },
		};
		cmdResourceBarrier(ppCmds[0], 0, NULL, 4, uavBarriers, false);

		cmdBindPipeline(ppCmds[0], pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pBRDFIntegrationMap;
		cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pBRDFIntegrationRootSignature, 1, params);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(ppCmds[0], gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] = { { pBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE } };

		cmdResourceBarrier(ppCmds[0], 0, NULL, 1, srvBarrier, true);

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(ppCmds[0], pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pPanoToCubeRootSignature, 1, params);

		struct
		{
			uint32_t mip;
			uint32_t textureSize;
		} rootConstantData = { 0, gSkyboxSize };

		for (uint32_t i = 0; i < gSkyboxMips; ++i)
		{
			rootConstantData.mip = i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &rootConstantData;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pSkybox;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pPanoToCubeRootSignature, 2, params);

			pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				ppCmds[0], max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(ppCmds[0], 0, NULL, 1, srvBarriers, false);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(ppCmds[0], pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pIrradianceMap;
		cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pIrradianceRootSignature, 2, params);
		pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(ppCmds[0], gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);
		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(ppCmds[0], pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pSkybox;
		cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pSpecularRootSignature, 1, params);

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
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pSpecularMap;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(ppCmds[0], pPBRDescriptorBinder, pSpecularRootSignature, 2, params);
			pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				ppCmds[0], max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]),
				max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] = { { pIrradianceMap, RESOURCE_STATE_SHADER_RESOURCE },
										   { pSpecularMap, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(ppCmds[0], 0, NULL, 2, srvBarriers2, false);

		endCmd(ppCmds[0]);
		waitTokenCompleted(token);
		queueSubmit(pGraphicsQueue, 1, &ppCmds[0], NULL, 0, NULL, 0, NULL);
		waitQueueIdle(pGraphicsQueue);

		removePipeline(pRenderer, pSpecularPipeline);
		removeRootSignature(pRenderer, pSpecularRootSignature);
		removeShader(pRenderer, pSpecularShader);
		removePipeline(pRenderer, pIrradiancePipeline);
		removeRootSignature(pRenderer, pIrradianceRootSignature);
		removeShader(pRenderer, pIrradianceShader);
		removePipeline(pRenderer, pBRDFIntegrationPipeline);
		removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
		removeShader(pRenderer, pBRDFIntegrationShader);
		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);
		removeDescriptorBinder(pRenderer, pPBRDescriptorBinder);
		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);
	}


	void loadMesh(size_t index)
	{
		//Load Sponza
		AssimpImporter        importer;
		AssimpImporter::Model model;
		eastl::string       sceneFullPath = FileSystem::FixPath(gModelNames[index], FSRoot::FSR_Meshes);

		if (!importer.ImportModel(sceneFullPath.c_str(), &model))
		{
			ErrorMsg("Failed to load %s", FileSystem::GetFileNameAndExtension(sceneFullPath).c_str());
			return;
		}

		uint32_t meshCount = (uint32_t)model.mMeshArray.size();

		Mesh& mesh = gModels[index];

		mesh.materialID.resize(meshCount);
		mesh.cmdArray.resize(meshCount);

		MeshVertex* vertices = NULL;
		uint32_t*   indices = NULL;

		uint32_t totalVertexCount = 0;
		uint32_t totalIndexCount = 0;
		for (uint32_t i = 0; i < meshCount; i++)
		{
			AssimpImporter::Mesh subMesh = model.mMeshArray[i];

			mesh.materialID[i] = subMesh.mMaterialId;

			uint32_t vertexCount = (uint32_t)subMesh.mPositions.size();
			uint32_t indexCount = (uint32_t)subMesh.mIndices.size();

			mesh.cmdArray[i] = { indexCount, totalIndexCount, totalVertexCount };

			vertices = (MeshVertex*)conf_realloc(vertices, sizeof(MeshVertex) * (totalVertexCount + vertexCount));
			indices = (uint32_t*)conf_realloc(indices, sizeof(uint32_t) * (totalIndexCount + indexCount));

			for (uint32_t j = 0; j < vertexCount; j++)
			{
				vertices[totalVertexCount++] = { subMesh.mPositions[j], subMesh.mNormals[j], subMesh.mUvs[j] };
			}

			for (uint32_t j = 0; j < indexCount; j++)
			{
				indices[totalIndexCount++] = subMesh.mIndices[j];
			}
		}

		SyncToken token;

		// Vertex position buffer for the scene
		BufferLoadDesc vbPosDesc = {};
		vbPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbPosDesc.mDesc.mVertexStride = sizeof(MeshVertex);
		vbPosDesc.mDesc.mSize = totalVertexCount * sizeof(MeshVertex);
		vbPosDesc.pData = vertices;
		vbPosDesc.ppBuffer = &mesh.pVertexBuffer;
		vbPosDesc.mDesc.pDebugName = L"Vertex Position Buffer Desc for Sponza";
		addResource(&vbPosDesc, &token);
		addToWaitQueue({ token, { &freeData, vertices, 0 } });

		// Index buffer for the scene
		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		ibDesc.mDesc.mSize = sizeof(uint32_t) * totalIndexCount;
		ibDesc.pData = indices;
		ibDesc.ppBuffer = &mesh.pIndexBuffer;
		ibDesc.mDesc.pDebugName = L"Index Buffer Desc for Sponza";
		addResource(&ibDesc, &token);
		addToWaitQueue({ token, { &freeData, indices, 0 } });

		addToWaitQueue(
			{ token, { &methodCallback<PixelProjectedReflections, &PixelProjectedReflections::onResourceLoaded>, this, index } });
	}

	void loadTexture(size_t index)
	{
		TextureLoadDesc textureDesc;
		memset(&textureDesc, 0, sizeof(textureDesc));
		textureDesc.pFilename = pMaterialImageFileNames[index];
		textureDesc.ppTexture = &pMaterialTextures[index];
		textureDesc.mRoot = FSR_Textures;
		SyncToken token;
		addResource(&textureDesc, &token);
		addToWaitQueue(
			{ token, { &methodCallback<PixelProjectedReflections, &PixelProjectedReflections::onResourceLoaded>, this, index + 2 } });
	}

	static void freeData(void* userData, size_t) { conf_free(userData); }

	typedef struct OnCompletion
	{
		void (*mOnCompletion)(void* userData, size_t arg);
		void*  mUserData;
		size_t mArg;
	} OnCompletion;

	template <class T, void (T::*callback)(size_t)>
	static void methodCallback(void* userData, size_t arg)
	{
		T* pThis = static_cast<T*>(userData);
		(pThis->*callback)(arg);
	}

	struct ResourceWait
	{
		SyncToken    mToken;
		OnCompletion mCompletion;
	};
	eastl::vector<ResourceWait> mWaitQueue;
	Mutex                         mQueueMutex;

	void addToWaitQueue(ResourceWait wait)
	{
		mQueueMutex.Acquire();
		mWaitQueue.push_back(wait);
		mQueueMutex.Release();
	}

	void processWaitQueue()
	{
		const size_t MAX_COMPLETION_CHECKS = 16;
		size_t       index = 0, count = MAX_COMPLETION_CHECKS;
		mQueueMutex.Acquire();
		while (index < count && index < mWaitQueue.size())
		{
			ResourceWait& resourceWait = mWaitQueue[index];
			if (isTokenCompleted(resourceWait.mToken))
			{
				resourceWait.mCompletion.mOnCompletion(resourceWait.mCompletion.mUserData, resourceWait.mCompletion.mArg);
				mWaitQueue.erase(mWaitQueue.begin() + index);

				--count;
			}
			else
			{
				index++;
			}
		}
		mQueueMutex.Release();
	}

	void onResourceLoaded(size_t resourceID)
	{
		uint64_t value = resourceID < 2 ? ModelTotalProgress : TextureTotalProgress;
		mAtomicProgress += value;
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addSceneBuffer())
			return false;

		if (!addReflectionBuffer())
			return false;

		if (!addGBuffers())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!addIntermeditateBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif

		// fill Gbuffers
		// Create vertex layout
		VertexLayout vertexLayoutSphere = {};
		vertexLayoutSphere.mAttribCount = 3;

		vertexLayoutSphere.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSphere.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[0].mBinding = 0;
		vertexLayoutSphere.mAttribs[0].mLocation = 0;
		vertexLayoutSphere.mAttribs[0].mOffset = 0;

		//normals
		vertexLayoutSphere.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutSphere.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[1].mLocation = 1;

		vertexLayoutSphere.mAttribs[1].mBinding = 0;
		vertexLayoutSphere.mAttribs[1].mOffset = 3 * sizeof(float);

		//texture
		vertexLayoutSphere.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutSphere.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayoutSphere.mAttribs[2].mLocation = 2;
		vertexLayoutSphere.mAttribs[2].mBinding = 0;
		vertexLayoutSphere.mAttribs[2].mOffset = 6 * sizeof(float);    // first attribute contains 3 floats

		/************************************************************************/
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		ImageFormat::Enum deferredFormats[DEFERRED_RT_COUNT] = {};
		bool              deferredSrgb[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[i]->mDesc.mFormat;
			deferredSrgb[i] = pRenderTargetDeferredPass[i]->mDesc.mSrgb;
		}

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& deferredPassPipelineSettings = desc.mGraphicsDesc;
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = pDepth;

		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.pSrgbValues = deferredSrgb;

		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0]->mDesc.mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0]->mDesc.mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSigGbuffers;
		deferredPassPipelineSettings.pShaderProgram = pShaderGbuffers;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSphere;
		deferredPassPipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &desc, &pPipelineGbuffers);

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = NULL;

		deferredPassPipelineSettings.mRenderTargetCount = 1;
		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.pSrgbValues = deferredSrgb;
		deferredPassPipelineSettings.mSampleCount = pRenderTargetDeferredPass[0]->mDesc.mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pRenderTargetDeferredPass[0]->mDesc.mSampleQuality;

		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		deferredPassPipelineSettings.pRootSignature = pSkyboxRootSignature;
		deferredPassPipelineSettings.pShaderProgram = pSkyboxShader;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		deferredPassPipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &desc, &pSkyboxPipeline);

		// BRDF
		//Position
		VertexLayout vertexLayoutScreenQuad = {};
		vertexLayoutScreenQuad.mAttribCount = 2;

		vertexLayoutScreenQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutScreenQuad.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutScreenQuad.mAttribs[0].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[0].mLocation = 0;
		vertexLayoutScreenQuad.mAttribs[0].mOffset = 0;

		//Uv
		vertexLayoutScreenQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutScreenQuad.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayoutScreenQuad.mAttribs[1].mLocation = 1;
		vertexLayoutScreenQuad.mAttribs[1].mBinding = 0;
		vertexLayoutScreenQuad.mAttribs[1].mOffset = 3 * sizeof(float);    // first attribute contains 3 floats

		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSceneBuffer->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSceneBuffer->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSceneBuffer->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSceneBuffer->mDesc.mSampleQuality;

		// pipelineSettings.pDepthState is NULL, pipelineSettings.mDepthStencilFormat should be NONE
		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
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

		pipelineSettings.pColorFormats = &pReflectionBuffer->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pReflectionBuffer->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pReflectionBuffer->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pReflectionBuffer->mDesc.mSampleQuality;

		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pPPR_ReflectionRootSignature;
		pipelineSettings.pShaderProgram = pPPR_ReflectionShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &desc, &pPPR_ReflectionPipeline);

		//PPR_HolePatching -> Present
		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pPPR_HolePatchingRootSignature;
		pipelineSettings.pShaderProgram = pPPR_HolePatchingShader;
		pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &desc, &pPPR_HolePatchingPipeline);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfiler();
		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pSkyboxPipeline);
		removePipeline(pRenderer, pPPR_ProjectionPipeline);
		removePipeline(pRenderer, pPPR_ReflectionPipeline);
		removePipeline(pRenderer, pPPR_HolePatchingPipeline);
		removePipeline(pRenderer, pPipelineGbuffers);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pSceneBuffer);
		removeRenderTarget(pRenderer, pReflectionBuffer);
		removeResource(pIntermediateBuffer);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		processWaitQueue();

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		// Update camera
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		mat4 ViewProjMat = projMat * viewMat;

		pUniformDataCamera.mProjectView = ViewProjMat;
		pUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		viewMat.setTranslation(vec3(0));
		gUniformDataSky = pUniformDataCamera;
		gUniformDataSky.mProjectView = projMat * viewMat;

		//data uniforms
		pUniformDataExtenedCamera.mCameraWorldPos = vec4(pCameraController->getViewPosition(), 1.0);
		pUniformDataExtenedCamera.mViewMat = pCameraController->getViewMatrix();
		pUniformDataExtenedCamera.mProjMat = projMat;
		pUniformDataExtenedCamera.mViewProjMat = ViewProjMat;
		pUniformDataExtenedCamera.mInvViewProjMat = inverse(ViewProjMat);
		pUniformDataExtenedCamera.mViewPortSize =
			vec4(static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0, 0.0);

		//projection uniforms
		pUniformPPRProData.renderMode = gRenderMode;
		pUniformPPRProData.useHolePatching = gUseHolePatching == true ? 1.0f : 0.0f;
		pUniformPPRProData.useExpensiveHolePatching = gUseExpensiveHolePatching == true ? 1.0f : 0.0f;
		pUniformPPRProData.useNormalMap = gUseNormalMap == true ? 1.0f : 0.0f;
		pUniformPPRProData.useFadeEffect = gUseFadeEffect == true ? 1.0f : 0.0f;
		pUniformPPRProData.intensity = gRRP_Intensity;

		//Planes
		pUniformDataPlaneInfo.numPlanes = gPlaneNumber;
		pUniformDataPlaneInfo.planeInfo[0].centerPoint = vec4(0.0, -6.0f, 0.9f, 0.0);
		pUniformDataPlaneInfo.planeInfo[0].size = vec4(gPlaneSize);

		pUniformDataPlaneInfo.planeInfo[1].centerPoint = vec4(10.0, -5.0f, -1.25f, 0.0);
		pUniformDataPlaneInfo.planeInfo[1].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		pUniformDataPlaneInfo.planeInfo[2].centerPoint = vec4(10.0, -5.0f, 3.0f, 0.0);
		pUniformDataPlaneInfo.planeInfo[2].size = vec4(9.0f, 2.0f, 0.0f, 0.0f);

		pUniformDataPlaneInfo.planeInfo[3].centerPoint = vec4(10.0, 1.0f, 0.9f, 0.0);
		pUniformDataPlaneInfo.planeInfo[3].size = vec4(10.0f);

		mat4 basicMat;
		basicMat[0] = vec4(1.0, 0.0, 0.0, 0.0);     //tan
		basicMat[1] = vec4(0.0, 0.0, -1.0, 0.0);    //bitan
		basicMat[2] = vec4(0.0, 1.0, 0.0, 0.0);     //normal
		basicMat[3] = vec4(0.0, 0.0, 0.0, 1.0);

		pUniformDataPlaneInfo.planeInfo[0].rotMat = basicMat;

		pUniformDataPlaneInfo.planeInfo[1].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -80.0f);
		pUniformDataPlaneInfo.planeInfo[2].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * -100.0f);
		pUniformDataPlaneInfo.planeInfo[3].rotMat = basicMat.rotationX(0.01745329251994329576923690768489f * 90.0f);
		;

#if defined(DIRECT3D12) || defined(_DURANGO)

		// Need to check why this should be transposed on DX12
		// Even view or proj matrices work well....

		pUniformDataPlaneInfo.planeInfo[0].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[0].rotMat);
		pUniformDataPlaneInfo.planeInfo[1].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[1].rotMat);
		pUniformDataPlaneInfo.planeInfo[2].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[2].rotMat);
		pUniformDataPlaneInfo.planeInfo[3].rotMat = transpose(pUniformDataPlaneInfo.planeInfo[3].rotMat);

#endif

		mPrevProgressBarValue = mProgressBarValue;
		mProgressBarValue = mAtomicProgress;

    // ProfileSetDisplayMode()
    // TODO: need to change this better way 
    if (bToggleMicroProfiler != bPrevToggleMicroProfiler)
    {
      Profile& S = *ProfileGet();
      int nValue = bToggleMicroProfiler ? 1 : 0;
      nValue = nValue >= 0 && nValue < P_DRAW_SIZE ? nValue : S.nDisplay;
      S.nDisplay = nValue;

      bPrevToggleMicroProfiler = bToggleMicroProfiler;
    }

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// This will acquire the next swapchain image
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		eastl::vector<Cmd*> allCmds;

		BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex], &pUniformDataCamera };
		updateResource(&camBuffUpdateDesc);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[gFrameIndex], &gUniformDataSky };
		updateResource(&skyboxViewProjCbv);

		BufferUpdateDesc CbvExtendedCamera = { pBufferUniformExtendedCamera[gFrameIndex], &pUniformDataExtenedCamera };
		updateResource(&CbvExtendedCamera);

		BufferUpdateDesc CbPPR_Prop = { pBufferUniformPPRPro[gFrameIndex], &pUniformPPRProData };
		updateResource(&CbPPR_Prop);

		BufferUpdateDesc planeInfoBuffUpdateDesc = { pBufferUniformPlaneInfo[gFrameIndex], &pUniformDataPlaneInfo };
		updateResource(&planeInfoBuffUpdateDesc);

		// Draw G-buffers
		Cmd* cmd = pPrepCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		//Clear G-buffers and Depth buffer
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[i]->mDesc.mClearValue;
		}

		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0.0f };    // Clear depth to the far plane and stencil to 0

		// Transfer G-buffers to render target state for each buffer
		TextureBarrier barrier;
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			barrier = { pRenderTargetDeferredPass[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
			cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		}

		// Transfer DepthBuffer to a DephtWrite State
		barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		cmdBindRenderTargets(cmd, 1, pRenderTargetDeferredPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);

		// Draw the skybox.
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render SkyBox", true);

		cmdBindPipeline(cmd, pSkyboxPipeline);
		DescriptorData skyParams[2] = {};
		skyParams[0].pName = "uniformBlock";
		skyParams[0].ppBuffers = &pBufferUniformCameraSky[gFrameIndex];
		skyParams[1].pName = "skyboxTex";
		skyParams[1].ppTextures = &pSkybox;
		cmdBindDescriptors(cmd, pDescriptorBinder, pSkyboxRootSignature, 2, skyParams);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, NULL);
		cmdDraw(cmd, 36, 0);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		// Draw Sponza
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Fill GBuffers", true);
		//The default code path we have if not iOS uses an array of texture of size 81
		//iOS only supports 31 max texture units in a fragment shader for most devices.
		//so fallback to binding every texture for every draw call (max of 5 textures)

		bool dataLoaded = isThreadSystemIdle(pIOThreads) && isBatchCompleted();
		if (dataLoaded)
		{
			Mesh& sponzaMesh = gModels[SPONZA_MODEL];

			Buffer* pSponzaVertexBuffers[] = { sponzaMesh.pVertexBuffer };
			cmdBindVertexBuffer(cmd, 1, pSponzaVertexBuffers, NULL);
			cmdBindIndexBuffer(cmd, sponzaMesh.pIndexBuffer, 0);

#ifdef TARGET_IOS
			cmdBindPipeline(cmd, pPipelineGbuffers);
			DescriptorData params[8] = {};
			params[0].pName = "cbCamera";
			params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];

			for (uint32_t i = 0; i < (uint32_t)sponzaMesh.cmdArray.size(); ++i)
			{
				int materialID = sponzaMesh.materialID[i];
				materialID *= 5;    //because it uses 5 basic textures for redering BRDF

				params[1].pName = "cbObject";
				params[1].ppBuffers = &pSponzaBuffer;

				for (int j = 0; j < 5; ++j)
				{
					//added
					params[2 + j].pName = pTextureName[j];
					params[2 + j].ppTextures = &pMaterialTextures[gSponzaTextureIndexforMaterial[materialID + j]];
				}

				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigGbuffers, 7, params);

				Mesh::CmdParam& cmdData = sponzaMesh.cmdArray[i];
				cmdDrawIndexed(cmd, cmdData.indexCount, cmdData.startIndex, cmdData.startVertex);
			}

#else

			cmdBindPipeline(cmd, pPipelineGbuffers);
			DescriptorData params[8] = {};
			params[0].pName = "cbCamera";
			params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];
			params[1].pName = "cbObject";
			params[1].ppBuffers = &pSponzaBuffer;
			params[2].pName = "textureMaps";
			params[2].ppTextures = pMaterialTextures;
			params[2].mCount = TOTAL_IMGS;
			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigGbuffers, 3, params);

			struct MaterialMaps
			{
				uint mapIDs[5];
			} data;

			for (uint32_t i = 0; i < (uint32_t)sponzaMesh.cmdArray.size(); ++i)
			{
				int materialID = sponzaMesh.materialID[i];
				materialID *= 5;    //because it uses 5 basic textures for redering BRDF

				for (int j = 0; j < 5; ++j)
				{
					//added
					data.mapIDs[j] = gSponzaTextureIndexforMaterial[materialID + j];
				}
				params[0].pName = "cbTextureRootConstants";
				params[0].pRootConstant = &data;
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigGbuffers, 1, params);

				Mesh::CmdParam& cmdData = sponzaMesh.cmdArray[i];
				cmdDrawIndexed(cmd, cmdData.indexCount, cmdData.startIndex, cmdData.startVertex);
			}
#endif

			Mesh& lionMesh = gModels[LION_MODEL];

			//Draw Lion
#ifdef TARGET_IOS
			params[0].pName = "cbCamera";
			params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];

			params[1].pName = "cbObject";
			params[1].ppBuffers = &pLionBuffer;

			params[2].pName = pTextureName[0];
			params[2].ppTextures = &pMaterialTextures[81];

			params[3].pName = pTextureName[1];
			params[3].ppTextures = &pMaterialTextures[83];

			params[4].pName = pTextureName[2];
			params[4].ppTextures = &pMaterialTextures[6];

			params[5].pName = pTextureName[3];
			params[5].ppTextures = &pMaterialTextures[6];

			params[6].pName = pTextureName[4];
			params[6].ppTextures = &pMaterialTextures[0];

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigGbuffers, 7, params);

#else
			data.mapIDs[0] = 81;
			data.mapIDs[1] = 83;
			data.mapIDs[2] = 6;
			data.mapIDs[3] = 6;
			data.mapIDs[4] = 0;

			params[0].pName = "cbObject";
			params[0].ppBuffers = &pLionBuffer;

			params[1].pName = "cbTextureRootConstants";
			params[1].pRootConstant = &data;

			cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigGbuffers, 2, params);
#endif

			Buffer* pLionVertexBuffers[] = { lionMesh.pVertexBuffer };
			cmdBindVertexBuffer(cmd, 1, pLionVertexBuffers, NULL);
			cmdBindIndexBuffer(cmd, lionMesh.pIndexBuffer, 0);

			for (uint32_t i = 0; i < (uint32_t)lionMesh.cmdArray.size(); ++i)
			{
				Mesh::CmdParam& cmdData = lionMesh.cmdArray[i];
				cmdDrawIndexed(cmd, cmdData.indexCount, cmdData.startIndex, cmdData.startVertex);
			}
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);

		// Render BRDF
		cmd = pBrdfCmds[gFrameIndex];
		beginCmd(cmd);

		// Transfer G-buffers to a Shader resource state
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			barrier = { pRenderTargetDeferredPass[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		}

		// Transfer current render target to a render target state
		barrier = { pSceneBuffer->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		// Transfer DepthBuffer to a Shader resource state
		barrier = { pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pSceneBuffer->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pSceneBuffer, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mDesc.mWidth, (float)pSceneBuffer->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pSceneBuffer->mDesc.mWidth, pSceneBuffer->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render BRDF", false);

		cmdBindPipeline(cmd, pPipelineBRDF);
		DescriptorData BRDFParams[12] = {};

		BRDFParams[0].pName = "cbExtendCamera";
		BRDFParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		BRDFParams[1].pName = "cbLights";
		BRDFParams[1].ppBuffers = &pBufferUniformLights;

		BRDFParams[2].pName = "cbDLights";
		BRDFParams[2].ppBuffers = &pBufferUniformDirectionalLights;

		BRDFParams[3].pName = "brdfIntegrationMap";
		BRDFParams[3].ppTextures = &pBRDFIntegrationMap;

		BRDFParams[4].pName = "irradianceMap";
		BRDFParams[4].ppTextures = &pIrradianceMap;

		BRDFParams[5].pName = "specularMap";
		BRDFParams[5].ppTextures = &pSpecularMap;

		BRDFParams[6].pName = "AlbedoTexture";
		BRDFParams[6].ppTextures = &pRenderTargetDeferredPass[0]->pTexture;

		BRDFParams[7].pName = "NormalTexture";
		BRDFParams[7].ppTextures = &pRenderTargetDeferredPass[1]->pTexture;

		BRDFParams[8].pName = "RoughnessTexture";
		BRDFParams[8].ppTextures = &pRenderTargetDeferredPass[2]->pTexture;

		BRDFParams[9].pName = "DepthTexture";
		BRDFParams[9].ppTextures = &pDepthBuffer->pTexture;

		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, NULL);

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSigBRDF, 10, BRDFParams);

		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);

		// PixelProjectReflections_ProjectionPass
		cmd = pPPR_ProjectionCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Pixel-Projected Reflections", false);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "ProjectionPass", true);

		cmdBindPipeline(cmd, pPPR_ProjectionPipeline);

		DescriptorData PPR_ProjectionParams[5] = {};

		PPR_ProjectionParams[0].pName = "cbExtendCamera";
		PPR_ProjectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		PPR_ProjectionParams[1].pName = "IntermediateBuffer";
		PPR_ProjectionParams[1].ppBuffers = &pIntermediateBuffer;

		PPR_ProjectionParams[2].pName = "DepthTexture";
		PPR_ProjectionParams[2].ppTextures = &pDepthBuffer->pTexture;

		PPR_ProjectionParams[3].pName = "planeInfoBuffer";
		PPR_ProjectionParams[3].ppBuffers = &pBufferUniformPlaneInfo[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pPPR_ProjectionRootSignature, 4, PPR_ProjectionParams);

		const uint32_t* pThreadGroupSize = pPPR_ProjectionShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, (mSettings.mWidth * mSettings.mHeight / pThreadGroupSize[0]) + 1, 1, 1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);

		// PixelProjectReflections_ReflectionPass
		cmd = pPPR_ReflectionCmds[gFrameIndex];
		beginCmd(cmd);

		RenderTarget* pRenderTarget = pReflectionBuffer;

		barrier = { pSceneBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		// Transfer current render target to a render target state
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "ReflectionPass", true);
		cmdBindPipeline(cmd, pPPR_ReflectionPipeline);
		DescriptorData PPR_ReflectionParams[7] = {};

		PPR_ReflectionParams[0].pName = "cbExtendCamera";
		PPR_ReflectionParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		PPR_ReflectionParams[1].pName = "SceneTexture";
		PPR_ReflectionParams[1].ppTextures = &pSceneBuffer->pTexture;

		PPR_ReflectionParams[2].pName = "planeInfoBuffer";
		PPR_ReflectionParams[2].ppBuffers = &pBufferUniformPlaneInfo[gFrameIndex];

		PPR_ReflectionParams[3].pName = "IntermediateBuffer";
		PPR_ReflectionParams[3].ppBuffers = &pIntermediateBuffer;

		PPR_ReflectionParams[4].pName = "DepthTexture";
		PPR_ReflectionParams[4].ppTextures = &pDepthBuffer->pTexture;

		PPR_ReflectionParams[5].pName = "cbProperties";
		PPR_ReflectionParams[5].ppBuffers = &pBufferUniformPPRPro[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pPPR_ReflectionRootSignature, 6, PPR_ReflectionParams);
		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, NULL);
		cmdDraw(cmd, 3, 0);

		//End ReflectionPass
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		endCmd(cmd);
		allCmds.push_back(cmd);

		//Present
		cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		barrier = { pReflectionBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		loadActions = {};

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "HolePatching", true);
		cmdBindPipeline(cmd, pPPR_HolePatchingPipeline);

		DescriptorData PPR_HolePatchingParams[6] = {};

		PPR_HolePatchingParams[0].pName = "cbExtendCamera";
		PPR_HolePatchingParams[0].ppBuffers = &pBufferUniformExtendedCamera[gFrameIndex];

		PPR_HolePatchingParams[1].pName = "SceneTexture";
		PPR_HolePatchingParams[1].ppTextures = &pSceneBuffer->pTexture;

		PPR_HolePatchingParams[2].pName = "SSRTexture";
		PPR_HolePatchingParams[2].ppTextures = &pReflectionBuffer->pTexture;

		PPR_HolePatchingParams[3].pName = "cbProperties";
		PPR_HolePatchingParams[3].ppBuffers = &pBufferUniformPPRPro[gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pPPR_HolePatchingRootSignature, 4, PPR_HolePatchingParams);

		cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, NULL);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		//End Pixel-Projected Reflections
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);

		gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);

		if (!dataLoaded)
			gAppUI.Gui(pLoadingGui);
#ifndef TARGET_IOS
		else
			gAppUI.Gui(pGui);
#endif

		cmdDrawProfiler(cmd);

		gAppUI.Draw(cmd);
		cmdEndDebugMarker(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transition our texture to present state
		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);
		allCmds.push_back(cmd);

		queueSubmit(
			pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
			&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
	}

	const char* GetName() { return "10_PixelProjectedReflections"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addSceneBuffer()
	{
		RenderTargetDesc sceneRT = {};
		sceneRT.mArraySize = 1;
		sceneRT.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		sceneRT.mDepth = 1;
		sceneRT.mFormat = ImageFormat::RGBA8;

		sceneRT.mHeight = mSettings.mHeight;
		sceneRT.mWidth = mSettings.mWidth;

		sceneRT.mSampleCount = SAMPLE_COUNT_1;
		sceneRT.mSampleQuality = 0;
		sceneRT.pDebugName = L"Scene Buffer";

		addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

		return pSceneBuffer != NULL;
	}

	bool addReflectionBuffer()
	{
		RenderTargetDesc RT = {};
		RT.mArraySize = 1;
		RT.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		RT.mDepth = 1;
		RT.mFormat = ImageFormat::RGBA8;

		RT.mHeight = mSettings.mHeight;
		RT.mWidth = mSettings.mWidth;

		RT.mSampleCount = SAMPLE_COUNT_1;
		RT.mSampleQuality = 0;
		RT.pDebugName = L"Reflection Buffer";

		addRenderTarget(pRenderer, &RT, &pReflectionBuffer);

		return pReflectionBuffer != NULL;
	}

	bool addGBuffers()
	{
		ClearValue optimizedColorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };

		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;

		deferredRTDesc.mWidth = mSettings.mWidth;
		deferredRTDesc.mHeight = mSettings.mHeight;
		deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.pDebugName = L"G-Buffer RTs";

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			if (i == 1 || i == 2)
				deferredRTDesc.mFormat = ImageFormat::RGBA16F;
			else
				deferredRTDesc.mFormat = ImageFormat::RGBA8;

			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
		}

		return pRenderTargetDeferredPass[0] != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 1.0f, 0 };
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		//fixes flickering issues related to depth buffer being recycled.
#ifdef METAL
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	bool addIntermeditateBuffer()
	{
		// Add Intermediate buffer
		BufferLoadDesc IntermediateBufferDesc = {};
		IntermediateBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		IntermediateBufferDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight;
		IntermediateBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		IntermediateBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;

#ifdef METAL
		IntermediateBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
#endif

		IntermediateBufferDesc.mDesc.mSize = IntermediateBufferDesc.mDesc.mStructStride * IntermediateBufferDesc.mDesc.mElementCount;

		gInitializeVal.clear();
		for (int i = 0; i < mSettings.mWidth * mSettings.mHeight; i++)
		{
			gInitializeVal.push_back(UINT32_MAX);
		}

		IntermediateBufferDesc.pData = gInitializeVal.data();
		IntermediateBufferDesc.ppBuffer = &pIntermediateBuffer;
		addResource(&IntermediateBufferDesc);

		return pIntermediateBuffer != NULL;
	}

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
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

	//02 : Material__57 (Plant)
	gSponzaTextureIndexforMaterial.push_back(75);
	gSponzaTextureIndexforMaterial.push_back(76);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(77);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 03 : Material__298
	gSponzaTextureIndexforMaterial.push_back(9);
	gSponzaTextureIndexforMaterial.push_back(10);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(11);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 04 : 16___Default (gi_flag)
	gSponzaTextureIndexforMaterial.push_back(8);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!!
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(8);    // !!!!!
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
	gSponzaTextureIndexforMaterial.push_back(62);
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

	// 13 : Material__47 - it seems missing
	gSponzaTextureIndexforMaterial.push_back(19);
	gSponzaTextureIndexforMaterial.push_back(20);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(21);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 14 : flagpole
	gSponzaTextureIndexforMaterial.push_back(57);
	gSponzaTextureIndexforMaterial.push_back(58);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(59);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 15 : fabric_e (green)
	gSponzaTextureIndexforMaterial.push_back(51);
	gSponzaTextureIndexforMaterial.push_back(52);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 16 : fabric_d (blue)
	gSponzaTextureIndexforMaterial.push_back(49);
	gSponzaTextureIndexforMaterial.push_back(50);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 17 : fabric_a (red)
	gSponzaTextureIndexforMaterial.push_back(55);
	gSponzaTextureIndexforMaterial.push_back(56);
	gSponzaTextureIndexforMaterial.push_back(53);
	gSponzaTextureIndexforMaterial.push_back(54);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 18 : fabric_g (curtain_blue)
	gSponzaTextureIndexforMaterial.push_back(37);
	gSponzaTextureIndexforMaterial.push_back(38);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 19 : fabric_c (curtain_red)
	gSponzaTextureIndexforMaterial.push_back(41);
	gSponzaTextureIndexforMaterial.push_back(42);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 20 : fabric_f (curtain_green)
	gSponzaTextureIndexforMaterial.push_back(39);
	gSponzaTextureIndexforMaterial.push_back(40);
	gSponzaTextureIndexforMaterial.push_back(43);
	gSponzaTextureIndexforMaterial.push_back(44);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 21 : chain
	gSponzaTextureIndexforMaterial.push_back(12);
	gSponzaTextureIndexforMaterial.push_back(14);
	gSponzaTextureIndexforMaterial.push_back(13);
	gSponzaTextureIndexforMaterial.push_back(15);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 22 : vase_hanging
	gSponzaTextureIndexforMaterial.push_back(72);
	gSponzaTextureIndexforMaterial.push_back(73);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(74);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 23 : vase
	gSponzaTextureIndexforMaterial.push_back(69);
	gSponzaTextureIndexforMaterial.push_back(70);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(71);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 24 : Material__25 (lion)
	gSponzaTextureIndexforMaterial.push_back(16);
	gSponzaTextureIndexforMaterial.push_back(17);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(18);
	gSponzaTextureIndexforMaterial.push_back(AO);

	// 25 : roof
	gSponzaTextureIndexforMaterial.push_back(63);
	gSponzaTextureIndexforMaterial.push_back(64);
	gSponzaTextureIndexforMaterial.push_back(NoMetallic);
	gSponzaTextureIndexforMaterial.push_back(65);
	gSponzaTextureIndexforMaterial.push_back(AO);
}

DEFINE_APPLICATION_MAIN(PixelProjectedReflections)
