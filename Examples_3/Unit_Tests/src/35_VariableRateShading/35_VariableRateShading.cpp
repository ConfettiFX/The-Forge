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

/* 
 * Unit Test for Variable rate shading
*/

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Demo structures
#define CUBES_COUNT 2 // Must match with the shader

int                 gCuboidVertexCount                        = 36;
Buffer*             pCuboidVertexBuffer                       = NULL;

struct CubeInfoStruct
{
    vec4 mColor;
    mat4 mLocalMat;
} gCubesInfo[CUBES_COUNT];

struct UniformBlock
{
    mat4 mProjectView;
    mat4 mToWorldMat[CUBES_COUNT];
    vec4 mColor[CUBES_COUNT];
};

struct ShadingRateCB
{
    float2 mFocusCenter;
    float  mRadius;
};

const uint32_t      gImageCount                               = 3;
Renderer*           pRenderer                                 = NULL;
                     
Queue*              pGraphicsQueue                            = NULL;
CmdPool*            pCmdPools[gImageCount]                    = {NULL};
Cmd*                pCmds[gImageCount]                        = {NULL};
                     
SwapChain*          pSwapChain                                = NULL;
RenderTarget*       pDepthBuffer                              = NULL;
Fence*              pRenderCompleteFences[gImageCount]        = {NULL};
Semaphore*          pImageAcquiredSemaphore                   = NULL;
Semaphore*          pRenderCompleteSemaphores[gImageCount]    = {NULL};

Shader*             pShadingRateShader                        = NULL;
Pipeline*           pShadingRatePipeline                      = NULL;
RootSignature*      pShadingRateRootSignature                 = NULL;
DescriptorSet*      pShadingRateDescriptorSet_PerFrame        = {};
ShadingRateCB       gShadingRateCB;
uint32_t            gShadingRateRootConstantIndex             = 0;

Shader*             pResolveShader                            = NULL;
Shader*             pPlaneShader                              = NULL;
Shader*             pCubeShader                               = NULL;
Pipeline*           pCubePipeline                             = NULL;
Pipeline*           pPlanePipeline                            = NULL;
Pipeline*           pResolvePipeline                          = NULL;
RootSignature*      pRootSignature                            = NULL;
RenderTarget*       pColorRenderTarget                        = NULL;
DescriptorSet*      pDescriptorSet_NonFreq                    = {};
DescriptorSet*      pDescriptorSet_PerFrame                   = {};
Buffer*             pUniformBuffer[gImageCount]               = {};
Texture*            pPaletteTexture                           = NULL;
Texture*            pShadingRateTexture                       = NULL;
Texture*            pTestTexture                              = NULL;
UniformBlock        gUniformData                              = {};
DescriptorSet*      pResolveDescriptorSet_NonFreq             = {};
uint32_t            gDebugViewRootConstantIndex               = 0;

Sampler*            pStaticSampler                            = {NULL};
const char*         pStaticSamplerName                        = "uSampler";

uint32_t            gFrameIndex                               = 0;
ProfileToken        gGpuProfileToken                          = PROFILE_INVALID_TOKEN;
                                                             
ICameraController*  pCameraController                         = NULL;

UIComponent*       pGuiWindow;
bool                bToggleVRS;
bool                bToggleDebugView;

uint32_t            gCubesShadingRateIndex                    = 0;
ShadingRate*        pShadingRates                             = NULL;

/// UI
FontDrawDesc gFrameTimeDraw; 
uint32_t gFontID = 0; 

bool gTakeScreenshot = false;
void takeScreenshot(void* pUserData)
{
	if (!gTakeScreenshot)
		gTakeScreenshot = true;
}

class VariableRateShading: public IApp
{
public:
	VariableRateShading()
	{
		bToggleVRS = true;
		bToggleDebugView = false;
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,	   "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,		   "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,		   "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,		   "Scripts");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mShaderTarget = shader_target_6_4;
		initRenderer(GetName(), &settings, &pRenderer);
		// check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);

			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		// Load textures
		TextureLoadDesc loadDesc = {};
		loadDesc.pFileName = "colorPalette";
		loadDesc.ppTexture = &pPaletteTexture;
		// Textures representing color should be stored in SRGB or HDR format
		loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		addResource(&loadDesc, NULL);

		loadDesc = {};
		loadDesc.pFileName = "Lion_Albedo";
		loadDesc.ppTexture = &pTestTexture;
		// Textures representing color should be stored in SRGB or HDR format
		loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		addResource(&loadDesc, NULL);

		// Sampler
		SamplerDesc samplerDesc =
		{
			FILTER_LINEAR,
			FILTER_LINEAR,
			MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_REPEAT,
			ADDRESS_MODE_REPEAT,
			ADDRESS_MODE_REPEAT,
		};
		addSampler(pRenderer, &samplerDesc, &pStaticSampler);

		// Color palette
		{
			BufferLoadDesc ubDesc = {};
			ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
			ubDesc.mDesc.mSize = sizeof(UniformBlock);
			ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
			ubDesc.pData = NULL;
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				ubDesc.ppBuffer = &pUniformBuffer[i];
				addResource(&ubDesc, NULL);
			}
		}

		// Cubes
		{
			// scene data
			gCubesInfo[0].mColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
			gCubesInfo[1].mColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
			gUniformData.mColor[0] = gCubesInfo[0].mColor;
			gUniformData.mColor[1] = gCubesInfo[1].mColor;

			// Vertex buffer
			addCube(0.4f, 0.4f, 0.4f);
		}

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			if (!addShadingRateTexture())
				return false;
		}

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

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		// Gpu profiler can only be added after initProfile.
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		/************************************************************************/
		// GUI
		/************************************************************************/
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

		ButtonWidget screenshot;
		UIWidget* pScreenshot = uiCreateComponentWidget(pGuiWindow, "Screenshot", &screenshot, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pScreenshot, nullptr, takeScreenshot);
		luaRegisterWidget(pScreenshot);

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps)
		{
			CheckboxWidget toggleVRSCheckbox;
			toggleVRSCheckbox.pData = &bToggleVRS;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Toggle VRS\t\t\t\t\t", &toggleVRSCheckbox, WIDGET_TYPE_CHECKBOX));

			CheckboxWidget toggleDebugCheckbox;
			toggleDebugCheckbox.pData = &bToggleDebugView;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Toggle Debug View\t\t\t\t\t", &toggleDebugCheckbox, WIDGET_TYPE_CHECKBOX));

			SliderFloat2Widget focusCenterSlider;
			focusCenterSlider.pData = &gShadingRateCB.mFocusCenter;
			focusCenterSlider.mMin = { 0.0f, 0.0f };
			focusCenterSlider.mMax = { mSettings.mWidth, mSettings.mHeight };
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Focus center", &focusCenterSlider, WIDGET_TYPE_SLIDER_FLOAT2));

			// Shading Rates dropdown
			{
#if defined(DIRECT3D12)
				if (pRenderer->pActiveGpuSettings->mShadingRates & SHADING_RATE_2X4) // does it have additional rates support
				{
					const uint32_t count = 7;
					static const char* names[count] = {
						"SHADING_RATE_FULL", "SHADING_RATE_HALF", "SHADING_RATE_1X2", "SHADING_RATE_2X1",
						"SHADING_RATE_2X4", "SHADING_RATE_4X2", "SHADING_RATE_QUARTER"
					};

					pShadingRates = (ShadingRate*)tf_calloc(count, sizeof(ShadingRate));
					ASSERT(pShadingRates);

					pShadingRates[0] = SHADING_RATE_FULL;
					pShadingRates[1] = SHADING_RATE_HALF;
					pShadingRates[2] = SHADING_RATE_1X2;
					pShadingRates[3] = SHADING_RATE_2X1;
					pShadingRates[4] = SHADING_RATE_2X4;
					pShadingRates[5] = SHADING_RATE_4X2;
					pShadingRates[6] = SHADING_RATE_QUARTER;

					DropdownWidget ddCubeSR;
					ddCubeSR.pData = &gCubesShadingRateIndex;
					ddCubeSR.pNames = names;
					ddCubeSR.mCount = count;
					luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cubes shading rate: ", &ddCubeSR, WIDGET_TYPE_DROPDOWN));
				}
				else // tier-1 support
				{
					const uint32_t count = 4;
					static const char* names[count] = { "SHADING_RATE_FULL", "SHADING_RATE_HALF", "SHADING_RATE_1X2", "SHADING_RATE_2X1" };

					pShadingRates = (ShadingRate*)tf_calloc(count, sizeof(ShadingRate));
					ASSERT(pShadingRates);

					pShadingRates[0] = SHADING_RATE_FULL;
					pShadingRates[1] = SHADING_RATE_HALF;
					pShadingRates[2] = SHADING_RATE_1X2;
					pShadingRates[3] = SHADING_RATE_2X1;

					DropdownWidget ddCubeSR;
					ddCubeSR.pData = &gCubesShadingRateIndex;
					ddCubeSR.pNames = names;
					ddCubeSR.mCount = count;
					luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cubes shading rate: ", &ddCubeSR, WIDGET_TYPE_DROPDOWN));
				}
#endif
			}
		}
		else
		{
			LabelWidget notSupportedLabel;
			luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Variable Rate Shading is not supported by this GPU.", &notSupportedLabel, WIDGET_TYPE_LABEL));
		}

		waitForAllResourceLoads();

		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ vec3(0) };
		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

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

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		exitCameraController(pCameraController);

		exitUserInterface();

		exitFontSystem(); 

		exitProfiler();

		tf_free(pShadingRates);
        pShadingRates = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			removeResource(pShadingRateTexture);
		}

		removeResource(pPaletteTexture);
		removeResource(pTestTexture);
		removeResource(pCuboidVertexBuffer);

		removeSampler(pRenderer, pStaticSampler);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
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

			if (!addDepthBuffer())
				return false;

			if (!addColorRenderTarget())
				return false;
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
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
			removeRenderTarget(pRenderer, pColorRenderTarget);
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

		// Update Scene
		{
			// camera
			mat4 viewMat = pCameraController->getViewMatrix();

			const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
			const float horizontal_fov = PI / 2.0f;
			mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 1000.0f, 0.1f);
			gUniformData.mProjectView = projMat * viewMat;

			// cubes
			static float rot = 0.0f;
			rot += deltaTime * 4.0f;
 
			gCubesInfo[0].mLocalMat = mat4::translation(vec3(10.0f, 0, 30))  * mat4::rotationY(rot) * mat4::scale(vec3(25.0f)) * mat4::identity();
			gUniformData.mToWorldMat[0] = gCubesInfo[0].mLocalMat;
			gCubesInfo[1].mLocalMat = mat4::translation(vec3(10.0f, 0, -30)) * mat4::rotationZ(rot) * mat4::scale(vec3(25.0f)) * mat4::identity();
			gUniformData.mToWorldMat[1] = gCubesInfo[1].mLocalMat;		
		}
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*	  pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*		  pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps)
		{
			// Update uniform buffers
			BufferUpdateDesc uniformBuffer = { pUniformBuffer[gFrameIndex] };
			beginUpdateResource(&uniformBuffer);
			*(UniformBlock*)uniformBuffer.pMappedData = gUniformData;
			endUpdateResource(&uniformBuffer, NULL);
		}

		// Reset cmd pool for this frame
		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] =
		{
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps)
		{
			// Generate the shading rate image
			if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
			{
				cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Generate the shading rate image");
	
				TextureBarrier textureBarriers[] =
				{
					{ pShadingRateTexture, RESOURCE_STATE_SHADING_RATE_SOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
				};
				cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

				cmdBindPipeline(cmd, pShadingRatePipeline);
				cmdBindDescriptorSet(cmd, gFrameIndex, pShadingRateDescriptorSet_PerFrame);
				cmdBindPushConstants(cmd, pShadingRateRootSignature, gShadingRateRootConstantIndex, &gShadingRateCB);

				uint32_t* threadGroupSize = pShadingRateShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
				uint32_t groupCountX = (pShadingRateTexture->mWidth  + threadGroupSize[0] - 1) / threadGroupSize[0];
				uint32_t groupCountY = (pShadingRateTexture->mHeight + threadGroupSize[1] - 1) / threadGroupSize[1];
				cmdDispatch(cmd, groupCountX, groupCountY, threadGroupSize[2]);

				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}

			// Draw
			{
				if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
				{
					TextureBarrier textureBarriers[] =
					{
						{ pShadingRateTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADING_RATE_SOURCE },
					};
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);
				}

				barriers[0] = { pColorRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

				LoadActionsDesc loadActions = {};
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
				loadActions.mClearColorValues[0] = pColorRenderTarget->mClearValue;
				loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
				loadActions.mClearDepth.depth = 1.0f;
				loadActions.mClearDepth.stencil = 0;
				cmdBindRenderTargets(cmd, 1, &pColorRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pColorRenderTarget->mWidth, (float)pColorRenderTarget->mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pColorRenderTarget->mWidth, pColorRenderTarget->mHeight);
				
				float toggleDebugView = bToggleDebugView ? 1.0f : 0.0f;

				// draw the background
				{
					cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw the color palette");
					cmdBindPipeline(cmd, pPlanePipeline);
					cmdBindDescriptorSet(cmd, 0, pDescriptorSet_NonFreq);

					cmdBindPushConstants(cmd, pRootSignature, gDebugViewRootConstantIndex, &toggleDebugView);

					if (bToggleVRS && (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE))
					{
						// Binding shading rate texture and using a override combiner to make sure our texture is affecting the final shaidng rate
						cmdSetShadingRate(cmd, SHADING_RATE_FULL, pShadingRateTexture, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);
					}
				
					// draw the background
					cmdDraw(cmd, 3, 0);
					cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
				}

				// draw the cubes with higher shading rate and override screen values
				{
					cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw the cubes");
					cmdBindPipeline(cmd, pCubePipeline);
					cmdBindDescriptorSet(cmd, 0, pDescriptorSet_NonFreq);
					cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSet_PerFrame);

					cmdBindPushConstants(cmd, pRootSignature, gDebugViewRootConstantIndex, &toggleDebugView);

					static uint32_t stride = sizeof(float) * 8;
					cmdBindVertexBuffer(cmd, 1, &pCuboidVertexBuffer, &stride, NULL);

					if (bToggleVRS && (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_DRAW))
						cmdSetShadingRate(cmd, pShadingRates[gCubesShadingRateIndex], NULL, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH);

					cmdDrawInstanced(cmd, gCuboidVertexCount, 0, CUBES_COUNT, 0);

					if (bToggleVRS && pRenderer->pActiveGpuSettings->mShadingRateCaps)
						cmdSetShadingRate(cmd, SHADING_RATE_FULL, NULL, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH);

					cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
				}
			}

			// Draw Screen
			{	
				cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw screen");

				barriers[0] = { pColorRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

				LoadActionsDesc loadActions = {};
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
				cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

				cmdBindPipeline(cmd, pResolvePipeline);
				cmdBindDescriptorSet(cmd, 0, pResolveDescriptorSet_NonFreq);

				cmdDraw(cmd, 3, 0);
				cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			}
		}

		// UI and Other Overlays
		{
			LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = pRenderer->pActiveGpuSettings->mShadingRateCaps ? LOAD_ACTION_LOAD : LOAD_ACTION_CLEAR;
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

			const float txtIndent = 8.f;

			gFrameTimeDraw.mFontColor = 0xff00ffff;
			gFrameTimeDraw.mFontSize = 18.0f;
			gFrameTimeDraw.mFontID = gFontID;
			float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

			cmdDrawUserInterface(cmd);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
			barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
			cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
			endCmd(cmd);
		}

		// Sumbit and Present
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
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.mSubmitDone = true;

		// Capture screenshot before presentation.
		if (gTakeScreenshot)
		{
			// Metal platforms need one renderpass to prepare the swapchain textures for copy.
			if(prepareScreenshot(pSwapChain))
			{
				captureScreenshot(pSwapChain, swapchainImageIndex, RESOURCE_STATE_PRESENT, GetName());
				gTakeScreenshot = false;
			}
		}

		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "35_VariableRateShading"; }

	bool addColorRenderTarget()
	{
		RenderTargetDesc desc = {};
		desc.mWidth = mSettings.mWidth;
		desc.mHeight = mSettings.mHeight;
		desc.mDepth = 1;
		desc.mArraySize = 1;
		desc.mMipLevels = 1;
		desc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mFormat = getRecommendedSwapchainFormat(true, true);
		desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		desc.mClearValue.r = 0.0f;
		desc.mClearValue.g = 0.0f;
		desc.mClearValue.b = 0.0f;
		desc.mClearValue.a = 0.0f;
		desc.mSampleQuality = 0;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		desc.pName = "Color RT";
		addRenderTarget(pRenderer, &desc, &pColorRenderTarget);

		return NULL != pColorRenderTarget;
	}

	bool addShadingRateTexture()
	{
		TextureDesc texture = {};
		texture.mArraySize = 1;
		texture.mMipLevels = 1;
		texture.mDepth = 1;
		texture.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		texture.mWidth	= uint32_t(ceil(float(mSettings.mWidth)  / float(max(1, int32_t(pRenderer->pActiveGpuSettings->mShadingRateTexelWidth)))));
		texture.mHeight	= uint32_t(ceil(float(mSettings.mHeight) / float(max(1, int32_t(pRenderer->pActiveGpuSettings->mShadingRateTexelHeight)))));
		texture.mSampleCount = SAMPLE_COUNT_1;
		texture.mFormat = TinyImageFormat_R8_UINT;
		texture.mStartState = RESOURCE_STATE_SHADING_RATE_SOURCE;
		texture.pName = "Shading Rate Texture";

		TextureLoadDesc textureDesc = {};
		textureDesc.pDesc = &texture;
		textureDesc.ppTexture = &pShadingRateTexture;
		addResource(&textureDesc, NULL);

		// Set defaults
		{
			gShadingRateCB.mFocusCenter.x = float(texture.mWidth) / 2.0f;
			gShadingRateCB.mFocusCenter.y = float(texture.mHeight) / 2.0f;
			gShadingRateCB.mRadius = sqrt(sqrf(float(texture.mWidth)) + sqrf(float(texture.mHeight))) / 2.0f;
		}

		return NULL != pShadingRateTexture;
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
		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			DescriptorSetDesc desc = { pShadingRateRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
			addDescriptorSet(pRenderer, &desc, &pShadingRateDescriptorSet_PerFrame);
		}

		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSet_NonFreq);
		addDescriptorSet(pRenderer, &desc, &pResolveDescriptorSet_NonFreq);

		desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSet_PerFrame);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSet_NonFreq);
		removeDescriptorSet(pRenderer, pDescriptorSet_PerFrame);
		removeDescriptorSet(pRenderer, pResolveDescriptorSet_NonFreq);

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			removeDescriptorSet(pRenderer, pShadingRateDescriptorSet_PerFrame);
		}
	}

	void addRootSignatures()
	{
		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			Shader* shaders[] = { pShadingRateShader };
			RootSignatureDesc rootDesc = {};
			rootDesc.mStaticSamplerCount = 0;
			rootDesc.ppStaticSamplerNames = 0;
			rootDesc.ppStaticSamplers = 0;
			rootDesc.mShaderCount = 1;
			rootDesc.ppShaders = shaders;
			addRootSignature(pRenderer, &rootDesc, &pShadingRateRootSignature);
			gShadingRateRootConstantIndex = getDescriptorIndexFromName(pShadingRateRootSignature, "cbRootConstant");
		}

		Shader* shaders[] = { pPlaneShader, pCubeShader, pResolveShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 0;
		rootDesc.ppStaticSamplerNames = &pStaticSamplerName;
		rootDesc.ppStaticSamplers = &pStaticSampler;
		rootDesc.mShaderCount = 3;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);
		gDebugViewRootConstantIndex = getDescriptorIndexFromName(pRootSignature, "cbRootConstant");
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			removeRootSignature(pRenderer, pShadingRateRootSignature);
		}
	}

	void addShaders()
	{
		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			ShaderLoadDesc shader = {};
			if (pRenderer->pActiveGpuSettings->mShadingRates & SHADING_RATE_2X4) // does it have additional rates support
			{
				shader.mStages[0] = { "compShadingRate_ADDITIONAL_SUPPORT.comp", NULL, 0 };
			}
			else
			{
				shader.mStages[0] = { "compShadingRate.comp", NULL, 0 };
			}
			addShader(pRenderer, &shader, &pShadingRateShader);
		}

		ShaderLoadDesc shader = {};
		shader.mStages[0] = { "basic.vert", NULL, 0 };
		shader.mStages[1] = { "basic.frag", NULL, 0 };
		shader.mTarget = shader_target_6_4; // for shading rate debug view
		addShader(pRenderer, &shader, &pPlaneShader);

		shader.mStages[0] = { "basic.vert", NULL, 0 };
		shader.mStages[1] = { "basic.frag", NULL, 0 };
		addShader(pRenderer, &shader, &pResolveShader);

		shader.mStages[0] = { "cube.vert", NULL, 0 };
		shader.mStages[1] = { "cube.frag", NULL, 0 };
		addShader(pRenderer, &shader, &pCubeShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pResolveShader);
		removeShader(pRenderer, pPlaneShader);
		removeShader(pRenderer, pCubeShader);


		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			removeShader(pRenderer, pShadingRateShader);
		}
	}

	void addPipelines()
	{
		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			PipelineDesc pipelineDesc = {};
			pipelineDesc.pName = "Shading Rate Pipeline";
			pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;

			ComputePipelineDesc & computePipelineDesc = pipelineDesc.mComputeDesc;
			computePipelineDesc.pRootSignature = pShadingRateRootSignature;
			computePipelineDesc.pShaderProgram = pShadingRateShader;
			addPipeline(pRenderer, &pipelineDesc, &pShadingRatePipeline);
		}

		// Color palette
		PipelineDesc pipelineDesc = {};
		pipelineDesc.pName = "Color Palette Pipeline";
		pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		GraphicsPipelineDesc & graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
		graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		graphicsPipelineDesc.mRenderTargetCount = 1;
		graphicsPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		graphicsPipelineDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		graphicsPipelineDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		graphicsPipelineDesc.pVertexLayout = NULL;
		graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;

		graphicsPipelineDesc.pShaderProgram = pPlaneShader;
		graphicsPipelineDesc.pRootSignature = pRootSignature;
		addPipeline(pRenderer, &pipelineDesc, &pPlanePipeline);

		graphicsPipelineDesc.pShaderProgram = pResolveShader;
		addPipeline(pRenderer, &pipelineDesc, &pResolvePipeline);

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 3;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[2].mBinding = 0;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		graphicsPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
		graphicsPipelineDesc.pDepthState = &depthStateDesc;
		graphicsPipelineDesc.pShaderProgram = pCubeShader;
		graphicsPipelineDesc.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &pipelineDesc, &pCubePipeline);
	}

	void removePipelines()
	{
		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			removePipeline(pRenderer, pShadingRatePipeline);
		}

		removePipeline(pRenderer, pResolvePipeline);
		removePipeline(pRenderer, pPlanePipeline);
		removePipeline(pRenderer, pCubePipeline);
	}

	void prepareDescriptorSets()
	{
		DescriptorData params[2] = {};

		if (pRenderer->pActiveGpuSettings->mShadingRateCaps & SHADING_RATE_CAPS_PER_TILE)
		{
			// Prepare descriptor sets
			params[0].pName = "uTexture";
			params[0].ppTextures = &pPaletteTexture;
			params[1].pName = "uTexture1";
			params[1].ppTextures = &pTestTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSet_NonFreq, 2, params);

			params[0].pName = "uniformBlock";
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].ppBuffers = &pUniformBuffer[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSet_PerFrame, 1, params);
			}

			params[0].pName = "outputTexture";
			params[0].ppTextures = &pShadingRateTexture;
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				updateDescriptorSet(pRenderer, i, pShadingRateDescriptorSet_PerFrame, 1, params);
			}
		}

		params[0].pName = "uTexture";
		params[0].ppTextures = &pColorRenderTarget->pTexture;
		updateDescriptorSet(pRenderer, 0, pResolveDescriptorSet_NonFreq, 1, params);
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 1.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void addCube(float width, float height, float depth)
	{
		float cubePoints[] =
		{
			//Position				     //Normals			    //TexCoords
			-width,  -height,  -depth  ,  0.0f,   0.0f,  -1.0f,   0.0f, 0.0f,
			 width,  -height,  -depth  ,  0.0f,   0.0f,  -1.0f,   1.0f, 0.0f,
			 width,   height,  -depth  ,  0.0f,   0.0f,  -1.0f,   1.0f, 1.0f,
			 width,   height,  -depth  ,  0.0f,   0.0f,  -1.0f,   1.0f, 1.0f,
			-width,   height,  -depth  ,  0.0f,   0.0f,  -1.0f,   0.0f, 1.0f,
			-width,  -height,  -depth  ,  0.0f,   0.0f,  -1.0f,   0.0f, 0.0f,
											   		  
			-width,  -height,  depth  ,	  0.0f,   0.0f,   1.0f,   0.0f, 0.0f,
			 width,  -height,  depth  ,	  0.0f,   0.0f,   1.0f,   1.0f, 0.0f,
			 width,   height,  depth  ,	  0.0f,   0.0f,   1.0f,   1.0f, 1.0f,
			 width,   height,  depth  ,	  0.0f,   0.0f,   1.0f,   1.0f, 1.0f,
			-width,   height,  depth  ,	  0.0f,   0.0f,   1.0f,   0.0f, 1.0f,
			-width,  -height,  depth  ,	  0.0f,   0.0f,   1.0f,   0.0f, 0.0f,
											   		  		   
			-width,   height,  depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 0.0f,
			-width,   height, -depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 1.0f,
			-width,  -height, -depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 1.0f,
			-width,  -height, -depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 1.0f,
			-width,  -height,  depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 0.0f,
			-width,   height,  depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 0.0f,
											   		  		   
			 width,   height,  depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 0.0f,
			 width,   height, -depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 1.0f,
			 width,  -height, -depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 1.0f,
			 width,  -height, -depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 1.0f,
			 width,  -height,  depth  ,   1.0f,   0.0f,   0.0f,   0.0f, 0.0f,
			 width,   height,  depth  ,   1.0f,   0.0f,   0.0f,   1.0f, 0.0f,
													  		   
			-width,  -height, -depth  ,	  0.0f,  -1.0f,   0.0f,   0.0f, 1.0f,
			 width,  -height, -depth  ,	  0.0f,  -1.0f,   0.0f,   1.0f, 1.0f,
			 width,  -height,  depth  ,	  0.0f,  -1.0f,   0.0f,   1.0f, 0.0f,
			 width,  -height,  depth  ,	  0.0f,  -1.0f,   0.0f,   1.0f, 0.0f,
			-width,  -height,  depth  ,	  0.0f,  -1.0f,   0.0f,   0.0f, 0.0f,
			-width,  -height, -depth  ,	  0.0f,  -1.0f,   0.0f,   0.0f, 1.0f,
										  		   		  		   
			-width,  height,  -depth  ,	  0.0f,   1.0f,   0.0f,   0.0f, 1.0f,
			 width,  height,  -depth  ,	  0.0f,   1.0f,   0.0f,   1.0f, 1.0f,
			 width,  height,   depth  ,	  0.0f,   1.0f,   0.0f,   1.0f, 0.0f,
			 width,  height,   depth  ,	  0.0f,   1.0f,   0.0f,   1.0f, 0.0f,
			-width,  height,   depth  ,	  0.0f,   1.0f,   0.0f,   0.0f, 0.0f,
			-width,  height,  -depth  ,	  0.0f,   1.0f,   0.0f,   0.0f, 1.0f
		};

		BufferLoadDesc cuboidVbDesc = {};
		cuboidVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		cuboidVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		cuboidVbDesc.mDesc.mSize = uint64_t(288 * sizeof(float));
		cuboidVbDesc.pData = cubePoints;
		cuboidVbDesc.ppBuffer = &pCuboidVertexBuffer;
		addResource(&cuboidVbDesc, NULL);
	}
};

DEFINE_APPLICATION_MAIN(VariableRateShading)
