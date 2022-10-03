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

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/shader_defs.h"
#include "Geometry.h"

#define DEFERRED_RT_COUNT 3

struct GTAOConsts
{
	vec2 viewportPixelSize;
	vec2 ndcToViewMul;
	vec2 ndcToViewAdd;
	vec2 ndcToViewMul_x_PixelSize;
	float effectRadius;
	float effectFalloffRange;
	float radiusMultiplier;
	float sampleDistributionPower;
	float thinOccluderCompensation;
	float sliceCount;
	float stepsPerSlice;
	float depthMIPSamplingOffset;
	float finalValuePower;
	unsigned noiseIndex;
	float denoiseBlurBeta;
};

// Have a uniform for camera data
struct UniformCamData
{
	mat4 mView;
	mat4 mProjectView;
	mat4 mPrevProjectView;
	mat4 mProj;
	mat4 mInvProjectView;
	vec4 mCamPos;
	float zNear;
	float zFar;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4  mWorldMat;
};

struct UniformTAAData
{
	mat4 ReprojectionMatrix;
	vec2 ViewportPixelSize;
};

GTAOConsts gGTAOConsts;
Buffer* gGTAOConstsBuffer;

// Fonts
FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

// Rendering
const uint32_t gImageCount = 3;
const uint32_t gNumGeometrySets = 2;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain*    pSwapChain = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

RenderTarget* pRenderTargetVisibilityBuffer = NULL;
RenderTarget* pDepthBuffer = NULL;
RenderTarget* pTAAHistoryRT[2] = { NULL };

Sampler* pLinearClampSampler = NULL;
Sampler* pPointClampSampler = NULL;
Sampler* pDepthSampler = NULL;

Shader*           pVisibilityBufferShader[gNumGeometrySets] = { NULL };
Pipeline*         pPipelineVisibilityBuffer[gNumGeometrySets] = { NULL };
RootSignature*    pRootSigVisibilityBuffer = NULL;
CommandSignature* pCmdSignatureVisibilityBuffer = NULL;
DescriptorSet*    pDescriptorSetVisibilityBuffer[3] = { NULL };
Buffer*           pIndirectDrawArgumentsBuffer[2] = { NULL };
Buffer*			  pIndirectMaterialBufferAll = NULL;

Shader*        pFillFSQShader = NULL;
Pipeline*      pPipelineFSQ = NULL;
RootSignature* pRootSigFSQ = NULL;
DescriptorSet* pDescriptorSetFSQ[4] = { NULL };

Shader*        pPrefilterDepthsShader = NULL;
Pipeline*      pPipelinePrefilterDepths = NULL;
RootSignature* pRootSigPrefilterDepths = NULL;
DescriptorSet* pDescriptorSetPrefilterDepths[2] = { NULL };
uint32_t       gRootConstantIndexPrefilterDepths = 0;

Shader*        pGTAOShader = NULL;
Pipeline*      pPipelineGTAO = NULL;
RootSignature* pRootSigGTAO = NULL;
DescriptorSet* pDescriptorSetGTAO[2] = { NULL };
uint32_t       gUseMipLevelsRootConstantIndex = 0;
Buffer*        pIsTwoSidedMaterialBuffer = NULL;
bool           gUseMipLevels = true;

Shader*        pDenoiseShader = NULL;
Pipeline*      pPipelineDenoise = NULL;
RootSignature* pRootSigDenoise = NULL;
DescriptorSet* pDescriptorSetDenoise = NULL;

Shader*        pGenerateMipsShader = NULL;
Pipeline*      pPipelineGenerateMips = NULL;
RootSignature* pRootSigGenerateMips = NULL;
DescriptorSet* pDescriptorSetGenerateMips[2] = { NULL };
uint32_t       gMipSizeRootConstantIndex = 0;

Shader*        pTAAShader = NULL;
Pipeline*      pPipelineTAA = NULL;
RootSignature* pRootSigTAA = NULL;
DescriptorSet* pDescriptorSetTAA[4] = { NULL };

Texture* pFilteredDepths = NULL;
Texture* pAOTermTex = NULL;
Texture* pFinalAOTermTex = NULL;
Texture* pEdgesTex = NULL;
Texture* pDebugViewspacePositions = NULL;
Texture* pDebugViewNormals = NULL;
Texture* pDebugViewNormalsNegated = NULL;

UniformObjData gUniformDataMVP;

SyncToken gResourceSyncToken = {};
SyncToken gResourceSyncStartToken = {};

const char* gSceneName = "SanMiguel.gltf";

Texture** gDiffuseMapsStorage = NULL;

Geometry* pGeom = NULL;
uint32_t  gMeshCount = 0;
uint32_t  gMaterialCount = 0;
// for opaque and alpha tested draw calls
uint32_t  gDrawCount[gNumGeometrySets] = {};

Buffer* pCameraUniformBuffer[gImageCount] = { NULL };
Buffer* pTAAUniformBuffer[gImageCount] = { NULL };
Buffer* pSanMiguelUniformBuffer;

uint32_t gFrameIndex = 0;
uint32_t gFrameFlipFlop = 0;
uint32_t gFrameCounter = 0;

UniformCamData gUniformDataCamera;
ICameraController* pCameraController = NULL;

const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

ProfileToken   gGpuProfileToken;

bool gClearTextures = false;

static const uint32_t TEMPORAL_AA_JITTER_SAMPLES = 8;

bool				bEnableTemporalAA = true;
bool				gCurrentTemporalAARenderTarget = 0;
uint32_t			gCurrentTemporalAAJitterSample = 0;

vec2				gTemporalAAJitterSamples[TEMPORAL_AA_JITTER_SAMPLES] = {
	vec2(-0.5F,    0.33333337F),
	vec2(0.5F,   -0.7777778F),
	vec2(-0.75F,  -0.111111104F),
	vec2(0.25F,   0.5555556F),
	vec2(-0.25F,  -0.5555556F),
	vec2(0.75F,   0.111111164F),
	vec2(-0.875F,  0.7777778F),
	vec2(0.125F, -0.9259259F),
};

mat4	gTemporalAAPreviousViewProjection = mat4::identity();
mat4	gTemporalAAReprojection = mat4::identity();

enum
{
	PRESET_LOW    = 0,
	PRESET_MEDIUM = 1,
	PRESET_HIGH   = 2,
	PRESET_ULTRA  = 3,
};

UIComponent*     pGui = NULL;
UIComponent*     pDebugTexturesWindow = NULL;
DynamicUIWidgets GTAO_Widgets;
uint32_t         gPresetQuality = PRESET_MEDIUM;
uint32_t         gDenoisePasses = 3;
bool             gShowDebugTargets = true;
bool             bOddDenoisePassesLastFrame = gDenoisePasses % 2 != 0;

class AmbientOcclusion_GTAO : public IApp
{
public:
	bool Init() override
	{
		// used by XeGTAO
		gGTAOConsts.effectRadius = 0.3f;
		gGTAOConsts.effectFalloffRange = 0.615f;     // distant samples contribute less
		gGTAOConsts.radiusMultiplier = 1.457f;       // allows us to use different value as compared to ground truth radius to counter inherent screen space biases
		gGTAOConsts.sampleDistributionPower = 2.0f;  // small crevices more important than big surfaces
		gGTAOConsts.thinOccluderCompensation = 0.0f; // the new 'thickness heuristic' approach
		gGTAOConsts.depthMIPSamplingOffset = 3.30f;  // main trade-off between performance (memory bandwidth) and quality (temporal stability is the first affected, thin objects next)
		gGTAOConsts.finalValuePower = 2.2f;          // modifies the final ambient occlusion value using power function - this allows some of the above heuristics to do different things
		gGTAOConsts.denoiseBlurBeta = 1.2f;          // high value disables denoise - more elegant & correct way would be do set all edges to 0

		// "medium" quality in XeGTAO
		gGTAOConsts.sliceCount = 2;
		gGTAOConsts.stepsPerSlice = 2;

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mShaderTarget = shader_target_6_0;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

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
		{
			DLOGF(LogLevel::eERROR, "Failed to init font system");
			return false;
		}

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);
		
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		SamplerDesc samplerDesc = { FILTER_LINEAR,
							FILTER_LINEAR,
							MIPMAP_MODE_LINEAR,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE };

		SamplerDesc pointDesc = { FILTER_NEAREST,
							FILTER_NEAREST,
							MIPMAP_MODE_NEAREST,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE };

		SamplerDesc depthSamplerDesc = { FILTER_NEAREST,
							FILTER_NEAREST,
							MIPMAP_MODE_LINEAR,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE,
							ADDRESS_MODE_CLAMP_TO_EDGE };

		addSampler(pRenderer, &samplerDesc, &pLinearClampSampler);
		addSampler(pRenderer, &pointDesc, &pPointClampSampler);
		addSampler(pRenderer, &depthSamplerDesc, &pDepthSampler);

		/************************************************************************/
		// Load the scene using the SceneLoader class
		/************************************************************************/

		Scene* pScene = addScene(gSceneName, gResourceSyncToken);
		if (!pScene)
			return false;

		gMeshCount = pScene->geom->mDrawArgCount;
		gMaterialCount = pScene->geom->mDrawArgCount;
		pGeom = pScene->geom;
		/************************************************************************/
		// Texture loading
		/************************************************************************/
		gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);

		for (uint32_t i = 0; i < gMaterialCount; ++i)
		{
			TextureLoadDesc desc = {};
			desc.pFileName = pScene->textures[i];
			desc.ppTexture = &gDiffuseMapsStorage[i];
			addResource(&desc, NULL);
		}

		// Load Buffers
		BufferLoadDesc sanMiguel_buffDesc = {};
		sanMiguel_buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sanMiguel_buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sanMiguel_buffDesc.mDesc.mSize = sizeof(UniformObjData);
		sanMiguel_buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sanMiguel_buffDesc.pData = NULL;
		sanMiguel_buffDesc.ppBuffer = &pSanMiguelUniformBuffer;
		addResource(&sanMiguel_buffDesc, NULL);

		// Uniform buffer for camera data
		gUniformDataCamera.zNear = 0.1f;  // near plane
		gUniformDataCamera.zFar = 1000.f; // far plane

		BufferLoadDesc ubCamDesc = {};
		ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
		ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubCamDesc.mDesc.pName = "Uniform buffer for cam data";
		ubCamDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubCamDesc.ppBuffer = &pCameraUniformBuffer[i];
			addResource(&ubCamDesc, NULL);
		}

		// Uniform buffer for TAA data
		BufferLoadDesc ubTAADesc = {};
		ubTAADesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubTAADesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubTAADesc.mDesc.mSize = sizeof(UniformTAAData);
		ubTAADesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubTAADesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubTAADesc.ppBuffer = &pTAAUniformBuffer[i];
			addResource(&ubTAADesc, NULL);
		}

		// prepare resources

		// Update the uniform buffer for the objects
		mat4 sanMiguel_modelmat = mat4::identity();
		gUniformDataMVP.mWorldMat = sanMiguel_modelmat;
		BufferUpdateDesc sanMiguel_objBuffUpdateDesc = { pSanMiguelUniformBuffer };
		beginUpdateResource(&sanMiguel_objBuffUpdateDesc);
		*(UniformObjData*)sanMiguel_objBuffUpdateDesc.pMappedData = gUniformDataMVP;
		endUpdateResource(&sanMiguel_objBuffUpdateDesc, NULL);

		waitForAllResourceLoads();

		addIndirectBuffers(pScene);

		// Camera setup
		CameraMotionParameters cmp{ 1.0f, 1.0f, 140.0f };
		vec3 camPos = vec3(1.f, 5.f, 1.f);
		vec3 lookAt = camPos + vec3(0.0f, -0.0f, 1.0f);

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		InitGUI();

		// App Actions
		InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer };
		addInputAction(&actionDesc);
		actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this };
		addInputAction(&actionDesc);
		actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; } };
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
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL };
		addInputAction(&actionDesc);
		actionDesc = { DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL };
		addInputAction(&actionDesc);
		actionDesc = { DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL };
		addInputAction(&actionDesc);
		actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0;

		removeScene(pScene);

		return true;
	}

	void Exit() override
	{
		exitInputSystem();
		exitCameraController(pCameraController);

		gFrameIndex = 0;
		gFrameFlipFlop = 0;

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeResource(pSanMiguelUniformBuffer);
		removeResource(pIndirectDrawArgumentsBuffer[0]);
		removeResource(pIndirectDrawArgumentsBuffer[1]);
		removeResource(pIndirectMaterialBufferAll);
		removeResource(pIsTwoSidedMaterialBuffer);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pTAAUniformBuffer[i]);
			removeResource(pCameraUniformBuffer[i]);
		}

		removeResource(pGeom);
		// Remove Textures
		for (uint32_t i = 0; i < gMaterialCount; ++i)
		{
			removeResource(gDiffuseMapsStorage[i]);
		}

		tf_free(gDiffuseMapsStorage);

		uiDestroyDynamicWidgets(&GTAO_Widgets);

		exitUserInterface();

		exitFontSystem();
		
		removeSampler(pRenderer, pDepthSampler);
		removeSampler(pRenderer, pPointClampSampler);
		removeSampler(pRenderer, pLinearClampSampler);

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

	bool Load(ReloadDesc* pReloadDesc) override
	{
		if (!addTAAHistoryRTs())
			return false;

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			gResourceSyncStartToken = getLastTokenCompleted();

			if (!addSwapChain())
				return false;

			if (!addVisibilityBuffer())
				return false;

			if (!addDepthBuffer())
				return false;

			if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
			{
				gGTAOConsts.viewportPixelSize = { 1.0f / mSettings.mWidth, 1.0f / mSettings.mHeight };
				if (!addIntermediateResources())
					return false;

				waitForAllResourceLoads();

				SetupDebugTexturesWindow();
			}
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

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

	void Unload(ReloadDesc* pReloadDesc) override
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		removeRenderTarget(pRenderer, pTAAHistoryRT[0]);
		removeRenderTarget(pRenderer, pTAAHistoryRT[1]);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			// Remove streamer before removing any actual resources
			// otherwise we might delete a resource while uploading to it.
			waitForToken(&gResourceSyncToken);
			waitForAllResourceLoads();

			gResourceSyncToken = 0;

			removeRenderTarget(pRenderer, pDepthBuffer);
			removeRenderTarget(pRenderer, pRenderTargetVisibilityBuffer);

			removeSwapChain(pRenderer, pSwapChain);

			if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
			{
				if (pDebugTexturesWindow)
				{
					uiDestroyComponent(pDebugTexturesWindow);
					pDebugTexturesWindow = NULL;
				}

				removeIntermediateResources();
			}
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime) override
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);
		// Update camera
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, gUniformDataCamera.zNear, gUniformDataCamera.zFar);

		if (bEnableTemporalAA)
		{
			vec2 jitterSample = gTemporalAAJitterSamples[gCurrentTemporalAAJitterSample];

			mat4 projMatNoJitter = projMat;

			projMat[2][0] += jitterSample.getX() / float(mSettings.mWidth);
			projMat[2][1] += jitterSample.getY() / float(mSettings.mHeight);

			mat4 projViewNoJitter = projMatNoJitter * viewMat;
			gTemporalAAReprojection = gTemporalAAPreviousViewProjection * inverse(projViewNoJitter);
			gTemporalAAPreviousViewProjection = projViewNoJitter;
		}

		mat4 ViewProjMat = projMat * viewMat;

		gUniformDataCamera.mPrevProjectView = gUniformDataCamera.mProjectView;
		gUniformDataCamera.mView = viewMat;
		gUniformDataCamera.mProj = projMat;
		gUniformDataCamera.mProjectView = ViewProjMat;
		gUniformDataCamera.mInvProjectView = inverse(ViewProjMat);
		gUniformDataCamera.mCamPos = vec4(pCameraController->getViewPosition(), 0.0f);

		UpdateGTAOConsts(projMat);

		uiSetComponentActive(pDebugTexturesWindow, gShowDebugTargets);
	}
	void Draw() override
	{
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

		BufferUpdateDesc camBuffUpdateDesc = { pCameraUniformBuffer[gFrameIndex] };
		beginUpdateResource(&camBuffUpdateDesc);
		*(UniformCamData*)camBuffUpdateDesc.pMappedData = gUniformDataCamera;
		endUpdateResource(&camBuffUpdateDesc, NULL);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);
		
		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);
		
		// First Pass
		// Render to Visibility Buffer

		//Clear Visibility buffer and Depth buffer
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { { 1.0f, 0.0f } };    // Clear depth to the far plane and stencil to 0
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetVisibilityBuffer->mClearValue;

		// Transfer vis buffer to render target state
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Generate Visibility Buffer");
		
		const uint32_t rtBarriersCount = 2;
		RenderTargetBarrier rtBarriers[rtBarriersCount] = {};
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE };
		rtBarriers[1] = { pRenderTargetVisibilityBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, rtBarriersCount, rtBarriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTargetVisibilityBuffer, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetVisibilityBuffer->mWidth, (float)pRenderTargetVisibilityBuffer->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVisibilityBuffer->mWidth, pRenderTargetVisibilityBuffer->mHeight);

		// Draw SanMiguel
		bool dataLoaded = isTokenCompleted(&gResourceSyncToken);
		if (dataLoaded)
		{
			Geometry& sanMiguelMesh = *pGeom;
			for (uint32_t i = 0; i < gNumGeometrySets; ++i) // 2 geometry sets
			{
				cmdBindPipeline(cmd, pPipelineVisibilityBuffer[i]);
				cmdBindIndexBuffer(cmd, sanMiguelMesh.pIndexBuffer, sanMiguelMesh.mIndexType, 0);
				Buffer* pSanMiguelVertexBuffers[] = { sanMiguelMesh.pVertexBuffers[0], sanMiguelMesh.pVertexBuffers[1] };
				cmdBindVertexBuffer(cmd, 2, pSanMiguelVertexBuffers, sanMiguelMesh.mVertexStrides, NULL);
				cmdBindDescriptorSet(cmd, 0, pDescriptorSetVisibilityBuffer[0]);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetVisibilityBuffer[1]);
				if (i == 1)
					cmdBindDescriptorSet(cmd, 0, pDescriptorSetVisibilityBuffer[2]);

				cmdExecuteIndirect(cmd, pCmdSignatureVisibilityBuffer, gDrawCount[i], pIndirectDrawArgumentsBuffer[i], 0, pIndirectDrawArgumentsBuffer[i],
					DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			}
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// Second Pass

		// Prefilter depths to viewspace
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Prefilter Depths");
		
		// Transfer DepthBuffer and Visibility Buffer to a Shader resource state
		rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
		rtBarriers[1] = { pRenderTargetVisibilityBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
		// And Filtered Depth texture to a write state for the compute shader
		TextureBarrier textureBarriers[6] = {};
		textureBarriers[0] = { pFilteredDepths, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, rtBarriersCount, rtBarriers);

		cmdBindPipeline(cmd, pPipelinePrefilterDepths);
		cmdBindPushConstants(cmd, pRootSigPrefilterDepths, gRootConstantIndexPrefilterDepths, &gGTAOConsts.viewportPixelSize);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetPrefilterDepths[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPrefilterDepths[1]);
		cmdDispatch(cmd, (pDepthBuffer->mWidth + 16 - 1) / 16, (pDepthBuffer->mHeight + 16 - 1) / 16, 1);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		
		// generate mips
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Generate Depth Mips");
		
		uint32_t mipSizeX = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mWidth));
		uint32_t mipSizeY = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mHeight));
		cmdBindPipeline(cmd, pPipelineGenerateMips);
		for (uint32_t i = 1; i < pFilteredDepths->mMipLevels; ++i)
		{
			mipSizeX >>= 1;
			mipSizeY >>= 1;
			uint mipSize[2] = { mipSizeX, mipSizeY };
			cmdBindPushConstants(cmd, pRootSigGenerateMips, gMipSizeRootConstantIndex, mipSize);
			cmdBindDescriptorSet(cmd, i - 1, pDescriptorSetGenerateMips[0]);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetGenerateMips[1]);
			textureBarriers[0] = { pFilteredDepths, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

			uint32_t groupCountX = mipSizeX / 16;
			uint32_t groupCountY = mipSizeY / 16;
			if (groupCountX == 0)
				groupCountX = 1;
			if (groupCountY == 0)
				groupCountY = 1;
			cmdDispatch(cmd, groupCountX, groupCountY, 1);
		}
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// GTAO pass
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "GTAO");
		
		textureBarriers[0] = { pFilteredDepths, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		textureBarriers[1] = { pAOTermTex, bOddDenoisePassesLastFrame ? RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		textureBarriers[2] = { pEdgesTex, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		textureBarriers[3] = { pDebugViewspacePositions, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		textureBarriers[4] = { pDebugViewNormals, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		textureBarriers[5] = { pDebugViewNormalsNegated, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 6, textureBarriers, 0, NULL);
		
		cmdBindPipeline(cmd, pPipelineGTAO);
		float useMipLevelsFlag = gUseMipLevels ? 1.0f : 0.0f;
		cmdBindPushConstants(cmd, pRootSigGTAO, gUseMipLevelsRootConstantIndex, &useMipLevelsFlag);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetGTAO[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetGTAO[1]);
		cmdDispatch(cmd, (pDepthBuffer->mWidth + 8 - 1) / 8, (pDepthBuffer->mHeight + 8 - 1) / 8, 1);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		
		// Denoise pass
		bool oddDenoisePasses = gDenoisePasses % 2 != 0;
		
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Denoise AO term");
		
		textureBarriers[0] = { pAOTermTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		textureBarriers[1] = { pFinalAOTermTex, bOddDenoisePassesLastFrame ? RESOURCE_STATE_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		textureBarriers[2] = { pEdgesTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		textureBarriers[3] = { pDebugViewspacePositions, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
		textureBarriers[4] = { pDebugViewNormals, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
		textureBarriers[5] = { pDebugViewNormalsNegated, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 6, textureBarriers, 0, NULL);

		for (unsigned i = 0; i < gDenoisePasses; i++)
		{
			cmdBindPipeline(cmd, pPipelineDenoise);
			cmdBindDescriptorSet(cmd, i % 2, pDescriptorSetDenoise);
			cmdDispatch(cmd, (pDepthBuffer->mWidth + 16 - 1) / 16, (pDepthBuffer->mHeight + 8 - 1) / 8, 1);

			if (i % 2 == 0)
			{
				textureBarriers[0] = { pAOTermTex, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
				textureBarriers[1] = { pFinalAOTermTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			}
			else
			{
				textureBarriers[0] = { pAOTermTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
				textureBarriers[1] = { pFinalAOTermTex, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			}
			cmdResourceBarrier(cmd, 0, NULL, 2, textureBarriers, 0, NULL);
		}

		if (oddDenoisePasses)
		{
			textureBarriers[0] = { pAOTermTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			textureBarriers[1] = { pFinalAOTermTex, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
			if (!bOddDenoisePassesLastFrame)
				bOddDenoisePassesLastFrame = true;
		}
		else
		{
			textureBarriers[0] = { pAOTermTex, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
			textureBarriers[1] = { pFinalAOTermTex, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
			if (bOddDenoisePassesLastFrame)
				bOddDenoisePassesLastFrame = false;
		}
		cmdResourceBarrier(cmd, 0, NULL, 2, textureBarriers, 0, NULL);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		
		// TAA
		if (bEnableTemporalAA)
		{
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Temporal AA");
			
			BufferUpdateDesc bufferUpdate = { pTAAUniformBuffer[gFrameIndex] };
			beginUpdateResource(&bufferUpdate);
			*(UniformTAAData*)bufferUpdate.pMappedData = { gTemporalAAReprojection, gGTAOConsts.viewportPixelSize };
			endUpdateResource(&bufferUpdate, NULL);

			RenderTarget* historyInput = pTAAHistoryRT[gCurrentTemporalAARenderTarget];
			RenderTarget* historyOutput = pTAAHistoryRT[!gCurrentTemporalAARenderTarget];

			RenderTarget* pRenderTarget = historyOutput;

			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;
			loadActions.mLoadActionStencil = LOAD_ACTION_DONTCARE;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBindPipeline(cmd, pPipelineTAA);
			cmdBindDescriptorSet(cmd, gFrameIndex * 2 + uint32_t(!oddDenoisePasses), pDescriptorSetTAA[gCurrentTemporalAARenderTarget]);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, 0, NULL, NULL, NULL, -1, -1);

			RenderTargetBarrier temporalAABarriers[] = {
				{ historyInput, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ historyOutput, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE }
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, temporalAABarriers);
			
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

			gCurrentTemporalAAJitterSample = (gCurrentTemporalAAJitterSample + 1) % TEMPORAL_AA_JITTER_SAMPLES;
			gCurrentTemporalAARenderTarget = !gCurrentTemporalAARenderTarget;
		}

		// prepare current swapchain image to be rendered to
		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;

		// Draw FSQ
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Display Final AO");
		
		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPipelineFSQ);
		if (bEnableTemporalAA)
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetFSQ[gCurrentTemporalAARenderTarget]);
		else
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetFSQ[2 + uint32_t(!oddDenoisePasses)]);
		cmdDraw(cmd, 3, 0);
		
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// Draw UI
		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
		cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);
		
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// Transition our texture back to present state
		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
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
		gFrameCounter++;
	}

	const char* GetName() override { return "36_AmbientOcclusion_GTAO"; }

	void InitGUI()
	{
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
		uiCreateComponent("GTAO", &guiDesc, &pGui);

		static const char* enumPresetsNames[] = { "Low", "Medium", "High", "Ultra" };
		static const uint32_t enumPresetsCount = sizeof(enumPresetsNames) / sizeof(enumPresetsNames[0]);

		DropdownWidget ddPreset;
		ddPreset.pData = &gPresetQuality;
		ddPreset.pNames = enumPresetsNames;
		ddPreset.mCount = enumPresetsCount;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Preset Quality", &ddPreset, WIDGET_TYPE_DROPDOWN));

		SliderUintWidget denoisePassesSlider;
		denoisePassesSlider.pData = &gDenoisePasses;
		denoisePassesSlider.mMin = 0;
		denoisePassesSlider.mMax = 10;
		luaRegisterWidget(uiCreateComponentWidget(pGui, "Denoise Passes", &denoisePassesSlider, WIDGET_TYPE_SLIDER_UINT));

		CheckboxWidget useMipLevelsCheck;
		useMipLevelsCheck.pData = &gUseMipLevels;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Use Mip Levels", &useMipLevelsCheck, WIDGET_TYPE_CHECKBOX));

		SliderFloatWidget effectRadiusSlider;
		effectRadiusSlider.pData = &gGTAOConsts.effectRadius;
		effectRadiusSlider.mMin = 0.01f;
		effectRadiusSlider.mMax = 10.0f;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Effect Radius", &effectRadiusSlider, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget effectFalloffRangeSlider;
		effectFalloffRangeSlider.pData = &gGTAOConsts.effectFalloffRange;
		effectFalloffRangeSlider.mMin = 0.01f;
		effectFalloffRangeSlider.mMax = 5.0f;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Effect Falloff Range", &effectFalloffRangeSlider, WIDGET_TYPE_SLIDER_FLOAT));

		SliderFloatWidget sampleDistributionPowerSlider;
		sampleDistributionPowerSlider.pData = &gGTAOConsts.sampleDistributionPower;
		sampleDistributionPowerSlider.mMin = 0.5f;
		sampleDistributionPowerSlider.mMax = 5.0f;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Sample Distribution Power", &sampleDistributionPowerSlider, WIDGET_TYPE_SLIDER_FLOAT));

		CheckboxWidget useTAACheck;
		useTAACheck.pData = &bEnableTemporalAA;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Use TAA", &useTAACheck, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget showDebugTargetsCheck;
		showDebugTargetsCheck.pData = &gShowDebugTargets;
		luaRegisterWidget(uiCreateDynamicWidgets(&GTAO_Widgets, "Show Debug Targets", &showDebugTargetsCheck, WIDGET_TYPE_CHECKBOX));

		uiShowDynamicWidgets(&GTAO_Widgets, pGui);
	}

	void SetupDebugTexturesWindow()
	{
		float scale = 0.15f;
		float2 screenSize = { (float)mSettings.mWidth, (float)mSettings.mHeight };
		float2 texSize = screenSize * scale;

		if (!pDebugTexturesWindow)
		{
			UIComponentDesc UIComponentDesc = {};
			UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
			UIComponentDesc.mStartPosition.setY(screenSize.getY() - texSize.getY() - 50.f);
			uiCreateComponent("DEBUG RTs", &UIComponentDesc, &pDebugTexturesWindow);

			DebugTexturesWidget widget;
			luaRegisterWidget(uiCreateComponentWidget(pDebugTexturesWindow, "Debug RTs", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
		}

		ASSERT(pDebugViewNormals);

		static const Texture* pVBRTs[3];
		pVBRTs[0] = pDebugViewspacePositions;
		pVBRTs[1] = pDebugViewNormals;
		pVBRTs[2] = pDebugViewNormalsNegated;

		if (pDebugTexturesWindow)
		{
			ASSERT(pDebugTexturesWindow->mWidgets[0]->mType == WIDGET_TYPE_DEBUG_TEXTURES);
			DebugTexturesWidget* pTexturesWidget = (DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget;
			pTexturesWidget->pTextures = pVBRTs;
			pTexturesWidget->mTexturesCount = TF_ARRAY_COUNT(pVBRTs);
			pTexturesWidget->mTextureDisplaySize = texSize;
		}
	}


	void UpdateGTAOConsts(const mat4& projMatrix)
	{
		static uint32_t sCurrentPresetQuality = gPresetQuality;
		if (sCurrentPresetQuality != gPresetQuality)
		{
			switch (gPresetQuality)
			{
				case PRESET_LOW:    gGTAOConsts.sliceCount = 1; gGTAOConsts.stepsPerSlice = 2; break;
				case PRESET_MEDIUM: gGTAOConsts.sliceCount = 2; gGTAOConsts.stepsPerSlice = 2; break;
				case PRESET_HIGH:   gGTAOConsts.sliceCount = 3; gGTAOConsts.stepsPerSlice = 3; break;
				case PRESET_ULTRA:  gGTAOConsts.sliceCount = 9; gGTAOConsts.stepsPerSlice = 3; break;
			}
			sCurrentPresetQuality = gPresetQuality;
		}

		float tanHalfFOVY = 1.0f / projMatrix[1][1]; // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
		float tanHalfFOVX = 1.0F / projMatrix[0][0]; // = tanHalfFOVY * drawContext.Camera.GetAspect( );
		vec2 cameraTanHalfFOV = { tanHalfFOVX, tanHalfFOVY };

		gGTAOConsts.ndcToViewMul = { cameraTanHalfFOV.getX() * 2.0f, cameraTanHalfFOV.getY() * -2.0f };
		gGTAOConsts.ndcToViewAdd = { cameraTanHalfFOV.getX() * -1.0f, cameraTanHalfFOV.getY() * 1.0f };

		gGTAOConsts.ndcToViewMul_x_PixelSize = { gGTAOConsts.ndcToViewMul.getX() * gGTAOConsts.viewportPixelSize.getX(), gGTAOConsts.ndcToViewMul.getY() * gGTAOConsts.viewportPixelSize.getY() };

		gGTAOConsts.noiseIndex = (bEnableTemporalAA) ? gFrameCounter % 64 : 0;

		BufferUpdateDesc GTAOConstsUpdateDesc = { gGTAOConstsBuffer };
		beginUpdateResource(&GTAOConstsUpdateDesc);
		*(GTAOConsts*)GTAOConstsUpdateDesc.pMappedData = gGTAOConsts;
		endUpdateResource(&GTAOConstsUpdateDesc, NULL);
	}

	void addShaders()
	{
		ShaderLoadDesc visibilityBufferShaderDesc = {};
		visibilityBufferShaderDesc.mStages[0] = { "visibilityBuffer_pass.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID | SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		visibilityBufferShaderDesc.mStages[1] = { "visibilityBuffer_pass.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID | SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
#if defined(ORBIS) || defined(PROSPERO)
		// No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
		// a passthrough gs
		visibilityBufferShaderDesc.mStages[2] = { "visibilityBuffer_pass.geom", NULL, 0 };
#endif
		addShader(pRenderer, &visibilityBufferShaderDesc, &pVisibilityBufferShader[0]);
		visibilityBufferShaderDesc.mStages[0] = { "visibilityBuffer_pass_alpha.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID | SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		visibilityBufferShaderDesc.mStages[1] = { "visibilityBuffer_pass_alpha.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID | SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
#if defined(ORBIS) || defined(PROSPERO)
		// No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
		// a passthrough gs
		visibilityBufferShaderDesc.mStages[2] = { "visibilityBuffer_pass_alpha.geom", NULL, 0 };
#endif
		addShader(pRenderer, &visibilityBufferShaderDesc, &pVisibilityBufferShader[1]);

		ShaderLoadDesc fillFSQShaderDesc = {};
		fillFSQShaderDesc.mStages[0] = { "fillFSQ.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		fillFSQShaderDesc.mStages[1] = { "fillFSQ.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &fillFSQShaderDesc, &pFillFSQShader);

		ShaderLoadDesc computeShaderDesc = {};
		computeShaderDesc.mStages[0] = { "prefilterDepths.comp", NULL, 0 };
		addShader(pRenderer, &computeShaderDesc, &pPrefilterDepthsShader);
		computeShaderDesc.mStages[0] = { "GTAO.comp", NULL, 0 };
		addShader(pRenderer, &computeShaderDesc, &pGTAOShader);
		computeShaderDesc.mStages[0] = { "denoiseAO.comp", NULL, 0 };
		addShader(pRenderer, &computeShaderDesc, &pDenoiseShader);
		computeShaderDesc.mStages[0] = { "generateMips.comp", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &computeShaderDesc, &pGenerateMipsShader);

		ShaderLoadDesc TAAShaderDesc = {};
		TAAShaderDesc.mStages[0] = { "Triangular.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		TAAShaderDesc.mStages[1] = { "TemporalAA.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &TAAShaderDesc, &pTAAShader);
	}

	void addRootSignatures()
	{
		// Visibility Buffer
		const char* pVisibilityBufferSamplerNames[] = { "textureFilter" };
		Sampler*    pVisibilityBufferSamplers[] = { pPointClampSampler };

		Shader* visibilityBufferShaders[] = { pVisibilityBufferShader[0], pVisibilityBufferShader[1] };
		RootSignatureDesc visibilityBufferRootDesc = { visibilityBufferShaders, gNumGeometrySets };
		visibilityBufferRootDesc.mStaticSamplerCount = 1;
		visibilityBufferRootDesc.ppStaticSamplerNames = pVisibilityBufferSamplerNames;
		visibilityBufferRootDesc.ppStaticSamplers = pVisibilityBufferSamplers;
		addRootSignature(pRenderer, &visibilityBufferRootDesc, &pRootSigVisibilityBuffer);

		/************************************************************************/
		// Setup indirect command signatures
		/************************************************************************/
		uint32_t indirectArgCount = 0;
		IndirectArgumentDescriptor indirectArgs[2] = {};
		if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
		{
			indirectArgs[0].mType = INDIRECT_CONSTANT;
			indirectArgs[0].mIndex = getDescriptorIndexFromName(pRootSigVisibilityBuffer, "indirectRootConstant");
			indirectArgs[0].mByteSize = sizeof(uint32_t);
			++indirectArgCount;
		}
		indirectArgs[indirectArgCount++].mType = INDIRECT_DRAW_INDEX;

		CommandSignatureDesc vbPassDesc = { pRootSigVisibilityBuffer, indirectArgs, indirectArgCount };
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVisibilityBuffer);

		// FSQ
		const char* pStaticSamplerNames[] = { "defaultSampler" };
		Sampler*    pStaticSamplers[] = { pLinearClampSampler };

		RootSignatureDesc FSQRootDesc = { &pFillFSQShader, 1 };
		FSQRootDesc.mStaticSamplerCount = 1;
		FSQRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		FSQRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &FSQRootDesc, &pRootSigFSQ);

		
		const char* pDepthSamplerNames[] = { "depthSampler" };
		Sampler*    pDepthSamplers[] = { pDepthSampler };
		// Prefilter depths
		RootSignatureDesc computeRootDesc = { &pPrefilterDepthsShader, 1 };
		computeRootDesc.mStaticSamplerCount = 1;
		computeRootDesc.ppStaticSamplerNames = pDepthSamplerNames;
		computeRootDesc.ppStaticSamplers = pDepthSamplers;
		addRootSignature(pRenderer, &computeRootDesc, &pRootSigPrefilterDepths);
		gRootConstantIndexPrefilterDepths = getDescriptorIndexFromName(pRootSigPrefilterDepths, "RootConstant");

		// GTAO
		const char* pGTAOSamplerNames[] = { "depthSampler", "defaultSampler" };
		Sampler*    pGTAOSamplers[] = { pDepthSampler, pLinearClampSampler };
		computeRootDesc = { &pGTAOShader, 1 };
		computeRootDesc.mStaticSamplerCount = 2;
		computeRootDesc.ppStaticSamplerNames = pGTAOSamplerNames;
		computeRootDesc.ppStaticSamplers = pGTAOSamplers;
		addRootSignature(pRenderer, &computeRootDesc, &pRootSigGTAO);
		gUseMipLevelsRootConstantIndex = getDescriptorIndexFromName(pRootSigGTAO, "RootConstant");

		// Denoise
		computeRootDesc = { &pDenoiseShader, 1 };
		computeRootDesc.mStaticSamplerCount = 1;
		computeRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		computeRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &computeRootDesc, &pRootSigDenoise);

		// Generate mips
		computeRootDesc = { &pGenerateMipsShader, 1 };
		addRootSignature(pRenderer, &computeRootDesc, &pRootSigGenerateMips);
		gMipSizeRootConstantIndex = getDescriptorIndexFromName(pRootSigGenerateMips, "RootConstant");

		// TAA
		const char* pTAASamplerNames[] = { "clampMiplessLinearSampler", "clampMiplessPointSampler" };
		Sampler*    pTAASamplers[] = { pLinearClampSampler, pPointClampSampler };
		RootSignatureDesc TAARootDesc = { &pTAAShader, 1 };
		TAARootDesc.mStaticSamplerCount = 2;
		TAARootDesc.ppStaticSamplerNames = pTAASamplerNames;
		TAARootDesc.ppStaticSamplers = pTAASamplers;
		addRootSignature(pRenderer, &TAARootDesc, &pRootSigTAA);
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc setDesc;
		// Visibility Buffer
		setDesc = { pRootSigVisibilityBuffer, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 1 };            // cbObject
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVisibilityBuffer[0]);
		setDesc = { pRootSigVisibilityBuffer, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount }; // cbCamera
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVisibilityBuffer[1]);
		setDesc = { pRootSigVisibilityBuffer, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };                // indirect material buffer + diffuse maps
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVisibilityBuffer[2]);

		// FSQ
		setDesc = { pRootSigFSQ, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, 1 }; // AO term
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFSQ[0]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFSQ[1]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFSQ[2]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFSQ[3]);

		// Prefilter depths
		setDesc = { pRootSigPrefilterDepths, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };                // source depth + filtered depth
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPrefilterDepths[0]);
		setDesc = { pRootSigPrefilterDepths, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount }; // cbCamera
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPrefilterDepths[1]);

		// GTAO
		setDesc = { pRootSigGTAO, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };           // visibility buffer + indices + cbGTAOConsts + filtered depth + mips + AO term + edges
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGTAO[0]);
		setDesc = { pRootSigGTAO, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount }; // cbCamera
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGTAO[1]);

		// Denoise
		setDesc = { pRootSigDenoise, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDenoise);

		// Generate mips
		setDesc = { pRootSigGenerateMips, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 5 }; // 5 mip levels
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGenerateMips[0]);
		setDesc = { pRootSigGenerateMips, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };     // cbGTAOConsts
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGenerateMips[1]);

		// TAA
		setDesc = { pRootSigTAA, DESCRIPTOR_UPDATE_FREQ_NONE, gImageCount * 2 }; // scene + history + depth
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTAA[0]);
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTAA[1]);
	}

	void addPipelines()
	{
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

		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;

		// Create depth state and rasterizer state
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
		RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };
		RasterizerStateDesc rasterizerStateCullBackDesc = { CULL_MODE_BACK };

		// Visibility Buffer Pass Pipeline
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& visibilityBufferPipelineSettings = desc.mGraphicsDesc;
		visibilityBufferPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		visibilityBufferPipelineSettings.mRenderTargetCount = 1;
		visibilityBufferPipelineSettings.pDepthState = &depthStateDesc;

		visibilityBufferPipelineSettings.pColorFormats = &pRenderTargetVisibilityBuffer->mFormat;

		visibilityBufferPipelineSettings.mSampleCount = pRenderTargetVisibilityBuffer->mSampleCount;
		visibilityBufferPipelineSettings.mSampleQuality = pRenderTargetVisibilityBuffer->mSampleQuality;

		visibilityBufferPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		visibilityBufferPipelineSettings.pRootSignature = pRootSigVisibilityBuffer;
		visibilityBufferPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
		for (uint32_t i = 0; i < gNumGeometrySets; i++)
		{
			if (i == 0)
				visibilityBufferPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
			else
				visibilityBufferPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
			visibilityBufferPipelineSettings.pRasterizerState = i == 1 ? &rasterizerStateCullNoneDesc : &rasterizerStateCullFrontDesc;
			visibilityBufferPipelineSettings.pShaderProgram = pVisibilityBufferShader[i];
			addPipeline(pRenderer, &desc, &pPipelineVisibilityBuffer[i]);
		}

		// FSQ Pass Pipeline
		depthStateDesc.mDepthTest = false;
		depthStateDesc.mDepthWrite = false;
		depthStateDesc.mDepthFunc = CMP_NEVER;

		desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& fsqPassPipelineSettings = desc.mGraphicsDesc;
		fsqPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		fsqPassPipelineSettings.mRenderTargetCount = 1;
		fsqPassPipelineSettings.pDepthState = &depthStateDesc;

		TinyImageFormat fsqColorFormat = getRecommendedSwapchainFormat(true, false);
		fsqPassPipelineSettings.pColorFormats = &fsqColorFormat;

		fsqPassPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		fsqPassPipelineSettings.mSampleQuality = 0;

		fsqPassPipelineSettings.pRootSignature = pRootSigFSQ;
		fsqPassPipelineSettings.pShaderProgram = pFillFSQShader;
		fsqPassPipelineSettings.pVertexLayout = NULL;
		fsqPassPipelineSettings.pRasterizerState = &rasterizerStateCullFrontDesc;
		addPipeline(pRenderer, &desc, &pPipelineFSQ);

		// TAA Pipeline
		desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& TAAPipelineSettings = desc.mGraphicsDesc;
		TAAPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		TAAPipelineSettings.mRenderTargetCount = 1;
		TAAPipelineSettings.pDepthState = NULL;
		TAAPipelineSettings.pBlendState = NULL;
		TAAPipelineSettings.pColorFormats = &pTAAHistoryRT[0]->mFormat;
		TAAPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		TAAPipelineSettings.mSampleQuality = 0;
		TAAPipelineSettings.pRootSignature = pRootSigTAA;
		TAAPipelineSettings.pRasterizerState = &rasterizerStateCullBackDesc;
		TAAPipelineSettings.pShaderProgram = pTAAShader;
		addPipeline(pRenderer, &desc, &pPipelineTAA);

		PipelineDesc computeDesc = {};
		computeDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& cpipelineSettings = computeDesc.mComputeDesc;
		// Depths Prefilter Pipeline
		{
			cpipelineSettings.pShaderProgram = pPrefilterDepthsShader;
			cpipelineSettings.pRootSignature = pRootSigPrefilterDepths;
			addPipeline(pRenderer, &computeDesc, &pPipelinePrefilterDepths);
		}

		// GTAO Pipeline
		{
			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pGTAOShader;
			cpipelineSettings.pRootSignature = pRootSigGTAO;
			addPipeline(pRenderer, &computeDesc, &pPipelineGTAO);
		}

		// Denoise Pipeline
		{
			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pDenoiseShader;
			cpipelineSettings.pRootSignature = pRootSigDenoise;
			addPipeline(pRenderer, &computeDesc, &pPipelineDenoise);
		}

		// Generate mips Pipeline
		{
			cpipelineSettings = { 0 };
			cpipelineSettings.pShaderProgram = pGenerateMipsShader;
			cpipelineSettings.pRootSignature = pRootSigGenerateMips;
			addPipeline(pRenderer, &computeDesc, &pPipelineGenerateMips);
		}
	}

	void prepareDescriptorSets()
	{
		// FSQ
		{
			DescriptorData params[1] = {};

			params[0].pName = "g_aoTerm";
			params[0].ppTextures = &pTAAHistoryRT[gCurrentTemporalAARenderTarget]->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetFSQ[0], 1, params);
			params[0].ppTextures = &pTAAHistoryRT[!gCurrentTemporalAARenderTarget]->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetFSQ[1], 1, params);
			params[0].ppTextures = &pFinalAOTermTex;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetFSQ[2], 1, params);
			params[0].ppTextures = &pAOTermTex;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetFSQ[3], 1, params);
		}

		// Visibility Buffer
		{
			DescriptorData params[3] = {};

			params[0].pName = "cbObject";
			params[0].ppBuffers = &pSanMiguelUniformBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVisibilityBuffer[0], 1, params);
			params[0].pName = "diffuseMaps";
			params[0].mCount = gMaterialCount;
			params[0].ppTextures = gDiffuseMapsStorage;
			params[1].pName = "indirectMaterialBuffer";
			params[1].ppBuffers = &pIndirectMaterialBufferAll;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVisibilityBuffer[2], 2, params);
		}


		// TAA
		{
			DescriptorData params[4] = {};

			RenderTarget* historyInput = pTAAHistoryRT[gCurrentTemporalAARenderTarget];
			RenderTarget* historyOutput = pTAAHistoryRT[!gCurrentTemporalAARenderTarget];

			{
				params[2].pName = "depthTexture";
				params[2].ppTextures = &pDepthBuffer->pTexture;

				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					params[1].pName = "historyTexture";
					params[1].ppTextures = &historyInput->pTexture;

					params[3].pName = "TAAUniformBuffer";
					params[3].ppBuffers = &pTAAUniformBuffer[i];

					for (uint32_t j = 0; j < 2; j++)
					{
						params[0].pName = "sceneTexture";
						params[0].ppTextures = (j == 0)? &pFinalAOTermTex : &pAOTermTex;
						updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetTAA[0], 4, params);
					}

					params[1].pName = "historyTexture";
					params[1].ppTextures = &historyOutput->pTexture;

					params[3].pName = "TAAUniformBuffer";
					params[3].ppBuffers = &pTAAUniformBuffer[i];
					
					for (uint32_t j = 0; j < 2; j++)
					{
						params[0].pName = "sceneTexture";
						params[0].ppTextures = (j == 0) ? &pFinalAOTermTex : &pAOTermTex;
						updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetTAA[1], 4, params);
					}
				}
			}
		}

		// Compute shaders
		{
			{
				DescriptorData params[3] = {};

				// Prefilter depths
				params[0].pName = "g_outDepth";
				params[0].ppTextures = &pFilteredDepths;
				params[0].mUAVMipSlice = 0;
				params[1].pName = "g_sourceNDCDepth";
				params[1].ppTextures = &pDepthBuffer->pTexture;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetPrefilterDepths[0], 2, params);
			}

			{
				DescriptorData params[16] = {};

				// GTAO
				params[0].pName = "cbGTAOConsts";
				params[0].ppBuffers = &gGTAOConstsBuffer;
				params[1].pName = "g_viewspaceDepth";
				params[1].ppTextures = &pFilteredDepths;
				params[2].pName = "g_outWorkingAOTerm";
				params[2].ppTextures = &pAOTermTex;
				params[3].pName = "g_outWorkingEdges";
				params[3].ppTextures = &pEdgesTex;
				params[4].pName = "g_vbTex";
				params[4].ppTextures = &pRenderTargetVisibilityBuffer->pTexture;
				params[5].pName = "g_indices";
				params[5].ppBuffers = &pGeom->pIndexBuffer;
				params[6].pName = "g_vertexNormals";
				params[6].ppBuffers = &pGeom->pVertexBuffers[2];
				params[7].pName = "g_vertexPositions";
				params[7].ppBuffers = &pGeom->pVertexBuffers[0];
				params[8].pName = "g_drawArgs_opaque";
				params[8].ppBuffers = &pIndirectDrawArgumentsBuffer[0];
				params[9].pName = "g_drawArgs_alphatest";
				params[9].ppBuffers = &pIndirectDrawArgumentsBuffer[1];
				params[10].pName = "cbObject";
				params[10].ppBuffers = &pSanMiguelUniformBuffer;
				params[11].pName = "g_indirectMaterialBuffer";
				params[11].ppBuffers = &pIndirectMaterialBufferAll;
				params[12].pName = "g_isTwoSidedMaterialBuffer";
				params[12].ppBuffers = &pIsTwoSidedMaterialBuffer;
				params[13].pName = "g_outDebugViewspacePositions";
				params[13].ppTextures = &pDebugViewspacePositions;
				params[14].pName = "g_outDebugViewNormals";
				params[14].ppTextures = &pDebugViewNormals;
				params[15].pName = "g_outDebugViewNormalsNegated";
				params[15].ppTextures = &pDebugViewNormalsNegated;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetGTAO[0], 16, params);
			}

			{
				DescriptorData params[4] = {};

				// Denoise AO term
				params[0].pName = "cbGTAOConsts";
				params[0].ppBuffers = &gGTAOConstsBuffer;
				params[1].pName = "g_sourceAOTerm";
				params[1].ppTextures = &pAOTermTex;
				params[2].pName = "g_sourceEdges";
				params[2].ppTextures = &pEdgesTex;
				params[3].pName = "g_output";
				params[3].ppTextures = &pFinalAOTermTex;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetDenoise, 4, params);
				params[1].pName = "g_sourceAOTerm";
				params[1].ppTextures = &pFinalAOTermTex;
				params[3].pName = "g_output";
				params[3].ppTextures = &pAOTermTex;
				updateDescriptorSet(pRenderer, 1, pDescriptorSetDenoise, 4, params);
			}

			{
				// Generate mips
				for (uint32_t i = 1; i < pFilteredDepths->mMipLevels; ++i)
				{
					DescriptorData params[2] = {};
					params[0].pName = "Source";
					params[0].ppTextures = &pFilteredDepths;
					params[0].mUAVMipSlice = i - 1;
					params[1].pName = "Destination";
					params[1].ppTextures = &pFilteredDepths;
					params[1].mUAVMipSlice = i;
					updateDescriptorSet(pRenderer, i - 1, pDescriptorSetGenerateMips[0], 2, params);
				}

				DescriptorData gtaoConstsParam = {};
				gtaoConstsParam.pName = "cbGTAOConsts";
				gtaoConstsParam.ppBuffers = &gGTAOConstsBuffer;
				updateDescriptorSet(pRenderer, 0, pDescriptorSetGenerateMips[1], 1, &gtaoConstsParam);
			}
		}

		// Common camera descriptor set
		DescriptorData camParam = {};
		camParam.pName = "cbCamera";
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			camParam.ppBuffers = &pCameraUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetVisibilityBuffer[1], 1, &camParam);
			updateDescriptorSet(pRenderer, i, pDescriptorSetPrefilterDepths[1], 1, &camParam);
			updateDescriptorSet(pRenderer, i, pDescriptorSetGTAO[1], 1, &camParam);
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
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, false);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetGenerateMips[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetGenerateMips[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetDenoise);
		removeDescriptorSet(pRenderer, pDescriptorSetGTAO[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetGTAO[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetVisibilityBuffer[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetVisibilityBuffer[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetVisibilityBuffer[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetPrefilterDepths[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPrefilterDepths[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetFSQ[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetFSQ[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetFSQ[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetFSQ[3]);
		removeDescriptorSet(pRenderer, pDescriptorSetTAA[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetTAA[1]);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSigGenerateMips);
		removeRootSignature(pRenderer, pRootSigDenoise);
		removeRootSignature(pRenderer, pRootSigGTAO);
		removeRootSignature(pRenderer, pRootSigPrefilterDepths);
		removeRootSignature(pRenderer, pRootSigFSQ);
		removeRootSignature(pRenderer, pRootSigVisibilityBuffer);
		removeRootSignature(pRenderer, pRootSigTAA);
		removeIndirectCommandSignature(pRenderer, pCmdSignatureVisibilityBuffer);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pGenerateMipsShader);
		removeShader(pRenderer, pDenoiseShader);
		removeShader(pRenderer, pGTAOShader);
		removeShader(pRenderer, pPrefilterDepthsShader);
		removeShader(pRenderer, pFillFSQShader);
		removeShader(pRenderer, pVisibilityBufferShader[0]);
		removeShader(pRenderer, pVisibilityBufferShader[1]);
		removeShader(pRenderer, pTAAShader);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineGenerateMips);
		removePipeline(pRenderer, pPipelineDenoise);
		removePipeline(pRenderer, pPipelineGTAO);
		removePipeline(pRenderer, pPipelinePrefilterDepths);
		removePipeline(pRenderer, pPipelineFSQ);
		removePipeline(pRenderer, pPipelineVisibilityBuffer[0]);
		removePipeline(pRenderer, pPipelineVisibilityBuffer[1]);
		removePipeline(pRenderer, pPipelineTAA);
	}

	void addIndirectBuffers(Scene* pScene)
	{
		/************************************************************************/
		// Indirect draw arguments to draw all triangles
		/************************************************************************/
		const uint32_t numBatches = pGeom->mDrawArgCount;
		uint32_t materialIDPerDrawCall[MATERIAL_BUFFER_SIZE] = {};
		uint32_t indirectArgsNoAlphaDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};
		uint32_t indirectArgsDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};

		uint32_t iAlpha = 0, iNoAlpha = 0;
		const uint32_t argOffset = pRenderer->pActiveGpuSettings->mIndirectRootConstant ? 1 : 0;
		for (uint32_t i = 0; i < numBatches; ++i)
		{
			uint matID = i;
			Material* mat = &pScene->materials[matID];

			if (mat->alphaTested)
			{
				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsDwords[iAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + argOffset];
				*arg = pScene->geom->pDrawArgs[i];
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					indirectArgsDwords[iAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = iAlpha;
				}
				else
				{
					// No drawId or gl_DrawId but instance id works as expected so use that as the draw id
					arg->mStartInstance = iAlpha;
				}

				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(true, j) + iAlpha] = matID;
				iAlpha++;
				}
			else
			{
				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsNoAlphaDwords[iNoAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + argOffset];
				*arg = pScene->geom->pDrawArgs[i];
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					indirectArgsNoAlphaDwords[iNoAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = iNoAlpha;
				}
				else
				{
					// No drawId or gl_DrawId but instance id works as expected so use that as the draw id
					arg->mStartInstance = iNoAlpha;
				}

				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(false, j) + iNoAlpha] = matID;
				iNoAlpha++;
			}
		}
		indirectArgsDwords[DRAW_COUNTER_SLOT_POS] = iAlpha;
		indirectArgsNoAlphaDwords[DRAW_COUNTER_SLOT_POS] = iNoAlpha;

		gDrawCount[0] = iNoAlpha;
		gDrawCount[1] = iAlpha;

		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < gNumGeometrySets; ++i)
		{
			// Setup uniform data for draw batch data
			BufferLoadDesc indirectBufferDesc = {};
			indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS;
			indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
			indirectBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT;
			indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
			indirectBufferDesc.pData = i == 0 ? indirectArgsNoAlphaDwords : indirectArgsDwords;
			indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBuffer[i];
			indirectBufferDesc.mDesc.pName = "Indirect Buffer Desc";
			addResource(&indirectBufferDesc, NULL);
		}

		BufferLoadDesc indirectDesc = {};
		indirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
		indirectDesc.pData = materialIDPerDrawCall;
		indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
		indirectDesc.mDesc.pName = "Indirect Desc";
		addResource(&indirectDesc, NULL);

		uint32_t* isTwoSidedMaterialBuffer = (uint32_t*)tf_malloc(gMeshCount * sizeof(uint32_t));

		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			isTwoSidedMaterialBuffer[i] = pScene->materials[i].twoSided ? 1 : 0;
		}

		BufferLoadDesc meshConstantDesc = {};
		meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		meshConstantDesc.mDesc.mElementCount = gMeshCount;
		meshConstantDesc.mDesc.mStructStride = sizeof(uint32_t);
		meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
		meshConstantDesc.pData = isTwoSidedMaterialBuffer;
		meshConstantDesc.ppBuffer = &pIsTwoSidedMaterialBuffer;
		meshConstantDesc.mDesc.pName = "Mesh Constant Desc";

		addResource(&meshConstantDesc, NULL);

		tf_free(isTwoSidedMaterialBuffer);
	}

	bool addVisibilityBuffer()
	{
		ClearValue optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

		/************************************************************************/
		// Visibility buffer pass render target
		/************************************************************************/
		RenderTargetDesc visibilityRTDesc = {};
		visibilityRTDesc.mArraySize = 1;
		visibilityRTDesc.mClearValue = optimizedColorClearWhite;
		visibilityRTDesc.mDepth = 1;
		visibilityRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		visibilityRTDesc.mWidth = mSettings.mWidth;
		visibilityRTDesc.mHeight = mSettings.mHeight;
		visibilityRTDesc.mSampleCount = SAMPLE_COUNT_1;
		visibilityRTDesc.mSampleQuality = 0;
		visibilityRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		visibilityRTDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		visibilityRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		visibilityRTDesc.pName = "Visibility Buffer RT";

		addRenderTarget(pRenderer, &visibilityRTDesc, &pRenderTargetVisibilityBuffer);

		if (pRenderTargetVisibilityBuffer == NULL)
			return false;

		return true;
	}

	bool addTAAHistoryRTs()
	{
		ClearValue colorClearBlack = { { 0.f, 0.f, 0.f, 0.f } };

		RenderTargetDesc TAAHistoryRTDesc = {};
		TAAHistoryRTDesc.mArraySize = 1;
		TAAHistoryRTDesc.mDepth = 1;
		TAAHistoryRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		TAAHistoryRTDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		TAAHistoryRTDesc.mClearValue = colorClearBlack;
		TAAHistoryRTDesc.mWidth = mSettings.mWidth;
		TAAHistoryRTDesc.mHeight = mSettings.mHeight;
		TAAHistoryRTDesc.mSampleCount = SAMPLE_COUNT_1;
		TAAHistoryRTDesc.mSampleQuality = 0;

		TAAHistoryRTDesc.pName = "TAA History 0";
		TAAHistoryRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		addRenderTarget(pRenderer, &TAAHistoryRTDesc, &pTAAHistoryRT[0]);

		TAAHistoryRTDesc.pName = "TAA History 1";
		TAAHistoryRTDesc.mStartState = RESOURCE_STATE_RENDER_TARGET;
		addRenderTarget(pRenderer, &TAAHistoryRTDesc, &pTAAHistoryRT[1]);

		gCurrentTemporalAARenderTarget = 0;
		gCurrentTemporalAAJitterSample = 0;

		if (pTAAHistoryRT[0] == NULL || pTAAHistoryRT[1] == NULL)
			return false;

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

	bool addIntermediateResources()
	{
		// Add GTAO constants buffer
		BufferLoadDesc gtaoConstsBufferDesc = {};
		gtaoConstsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		gtaoConstsBufferDesc.mDesc.mElementCount = 1;
		gtaoConstsBufferDesc.mDesc.mStructStride = sizeof(GTAOConsts);
		gtaoConstsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		gtaoConstsBufferDesc.mDesc.mSize = gtaoConstsBufferDesc.mDesc.mStructStride;
		gtaoConstsBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		gtaoConstsBufferDesc.pData = NULL;
		gtaoConstsBufferDesc.ppBuffer = &gGTAOConstsBuffer;
		addResource(&gtaoConstsBufferDesc, NULL);

		if (gGTAOConstsBuffer == NULL)
			return false;

		// Add filtered depth texture + mips
		TextureDesc texDesc = {};
		texDesc.mArraySize = 1;
		texDesc.mDepth = 1;
		texDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		texDesc.mHeight = mSettings.mHeight;
		texDesc.mWidth = mSettings.mWidth;
		texDesc.mMipLevels = 5;
		texDesc.mSampleCount = SAMPLE_COUNT_1;
		texDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
		texDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		texDesc.pName = "GTAO_DepthHierarchy";

		TextureLoadDesc depthHierarchyLoadDesc = {};
		depthHierarchyLoadDesc.pDesc = &texDesc;
		depthHierarchyLoadDesc.ppTexture = &pFilteredDepths;
		addResource(&depthHierarchyLoadDesc, NULL);

		if (pFilteredDepths == NULL)
			return false;

		// Main pass output textures
		texDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		texDesc.mMipLevels = 1;
		texDesc.mStartState = bOddDenoisePassesLastFrame ? RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texDesc.pName = "GTAO_AOTerm";

		TextureLoadDesc aoTermLoadDesc = {};
		aoTermLoadDesc.pDesc = &texDesc;
		aoTermLoadDesc.ppTexture = &pAOTermTex;
		addResource(&aoTermLoadDesc, NULL);

		if (pAOTermTex == NULL)
			return false;
		
		texDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		texDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texDesc.pName = "GTAO_Edges";

		TextureLoadDesc edgesLoadDesc = {};
		edgesLoadDesc.pDesc = &texDesc;
		edgesLoadDesc.ppTexture = &pEdgesTex;
		addResource(&edgesLoadDesc, NULL);

		if (pEdgesTex == NULL)
			return false;

		texDesc.mFormat = TinyImageFormat_B8G8R8A8_UNORM;
		texDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		texDesc.pName = "GTAO_DebugViewNormals";

		TextureLoadDesc debugTextureLoadDesc = {};
		debugTextureLoadDesc.pDesc = &texDesc;
		debugTextureLoadDesc.ppTexture = &pDebugViewNormals;
		addResource(&debugTextureLoadDesc, NULL);

		debugTextureLoadDesc.ppTexture = &pDebugViewNormalsNegated;
		addResource(&debugTextureLoadDesc, NULL);

		debugTextureLoadDesc.ppTexture = &pDebugViewspacePositions;
		addResource(&debugTextureLoadDesc, NULL);

		if (pDebugViewNormals == NULL || pDebugViewNormalsNegated == NULL || pDebugViewspacePositions == NULL)
			return false;

		// Denoise output
		texDesc.mFormat = TinyImageFormat_R32_SFLOAT;
		texDesc.mMipLevels = 1;
		texDesc.mStartState = bOddDenoisePassesLastFrame ? RESOURCE_STATE_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		texDesc.pName = "GTAO_Final_AOTerm";

		TextureLoadDesc finalAOTermLoadDesc = {};
		finalAOTermLoadDesc.pDesc = &texDesc;
		finalAOTermLoadDesc.ppTexture = &pFinalAOTermTex;
		addResource(&finalAOTermLoadDesc, NULL);

		if (pFinalAOTermTex == NULL)
			return false;

		return true;
	}

	void removeIntermediateResources()
	{
		removeResource(pFilteredDepths);
		removeResource(pFinalAOTermTex);
		removeResource(pAOTermTex);
		removeResource(pEdgesTex);
		removeResource(pDebugViewspacePositions);
		removeResource(pDebugViewNormals);
		removeResource(pDebugViewNormalsNegated);
		removeResource(gGTAOConstsBuffer);
	}
};

DEFINE_APPLICATION_MAIN(AmbientOcclusion_GTAO)
