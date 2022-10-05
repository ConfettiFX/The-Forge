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

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

const uint32_t gImageCount = 3;
Renderer* pRenderer = NULL;

Queue* pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount] = { NULL };
Cmd* pCmds[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence* pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

ICameraController* pCameraController = NULL;

UIComponent* pGuiWindow = NULL;

uint32_t gFontID = 0;

FontDrawDesc gFrameTimeDraw;

bool gTakeScreenshot = false;
void takeScreenshot(void* pUserData)
{
	if (!gTakeScreenshot)
		gTakeScreenshot = true;
}

struct Vertex
{
	float2 Position;
	float2 UV;
};

struct PerFrame
{
	CameraMatrix WorldViewProjection;
	CameraMatrix WorldView;
	vec4 LightDirection;
} gPerFrameData = {};

float gSunCurrentTime = 0.0f;
float gSunSpeed = 1.75f;
float2 gSunControl = { -2.1f, -2.479f };
bool gAutomaticSunMovement = true;

// float3 gSunDirection = { 0.0F, -1.0F, 0.0F };
// bool gAutomaticSunMovement = true;

Texture* gPrecomputedDLUT = NULL;
Sampler* gPrecomputedDLUTSampler = NULL;
Shader* gPrecomputedDLUTShader = NULL;
RootSignature* gPrecomputedDLUTRootSignature = NULL;
Pipeline* gPrecomputedDLUTPipeline = NULL;
Buffer* gPrecomputedDLUTPerFrameBuffer[gImageCount] = { NULL };
DescriptorSet* gPrecomputedDLUTPerFrameDescriptorSet = NULL;
DescriptorSet* gPrecomputedDLUTTextureDescriptorSet = NULL;
Buffer* gPrecomputedDLUTVertexBuffer = NULL;
Buffer* gPrecomputedDLUTIndexBuffer = NULL;

class PrecomputedVolumeDLUT : public IApp
{
public:
	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		// window and renderer setup
		RendererDesc settings = {};
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
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

		initScreenshotInterface(pRenderer, pGraphicsQueue);

		initResourceLoaderInterface(pRenderer);

		SamplerDesc samplerClampDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE
		};
		addSampler(pRenderer, &samplerClampDesc, &gPrecomputedDLUTSampler);

		addPrecomputedDLUTResources();

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

		// Take a screenshot with a button.
		ButtonWidget screenshot;
		UIWidget* pScreenshot = uiCreateComponentWidget(pGuiWindow, "Screenshot", &screenshot, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pScreenshot, nullptr, takeScreenshot);
		luaRegisterWidget(pScreenshot);

		SliderFloat2Widget sunX;
		sunX.pData = &gSunControl;
		sunX.mMin = float2(-PI);
		sunX.mMax = float2(PI);
		sunX.mStep = float2(0.00001f);
		UIWidget* pSunControlWidget = uiCreateComponentWidget(pGuiWindow, "Sun control", &sunX, WIDGET_TYPE_SLIDER_FLOAT2);
		luaRegisterWidget(pSunControlWidget);

		// SliderFloat3Widget sunWidget;
		// sunWidget.pData = &gSunDirection;
		// sunWidget.mMin = float3(-1.0F);
		// sunWidget.mMax = float3(1.0F);
		// UIWidget* pSunControlWidget = uiCreateComponentWidget(pGuiWindow, "Sun control", &sunWidget, WIDGET_TYPE_SLIDER_FLOAT3);
		// luaRegisterWidget(pSunControlWidget);

		CheckboxWidget checkbox;
		checkbox.pData = &gAutomaticSunMovement;
		UIWidget* pCheckbox = uiCreateComponentWidget(pGuiWindow, "Sun movement", &checkbox, WIDGET_TYPE_CHECKBOX);
		luaRegisterWidget(pCheckbox);

		waitForAllResourceLoads();

		CameraMotionParameters cmp{ 160.0f, 600.0f, 200.0f };
		vec3                   camPos{ 0.0f, 0.0f, 100.0f };
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
		removePrecomputedDLUTResources();

		removeSampler(pRenderer, gPrecomputedDLUTSampler);

		exitInputSystem();

		exitCameraController(pCameraController);

		exitUserInterface();

		exitFontSystem();

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);
		exitScreenshotInterface();

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
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
			gFrameIndex = 0;
		}

		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		if (gAutomaticSunMovement)
		{
			float delta = deltaTime * gSunSpeed;
			gSunControl.x += delta;
			gSunControl.y += delta * 0.25F;
			if (gSunControl.x >= PI)
			{
				gSunControl.x = -(PI);
			}
			if (gSunControl.y >= PI)
			{
				gSunControl.y = -(PI);
			}
			gSunCurrentTime += deltaTime * 1000.0f;
		}

		mat4 rotation = mat4::rotationXY(gSunControl.x, gSunControl.y);
		vec3 newLightDir = vec4((inverse(rotation) * vec4(0, 0, 1, 0))).getXYZ() * -1.f;
		mat4 nextRotation = mat4::rotationXY(gSunControl.x, gSunControl.y + (PI / 2.f));
		vec3 lightDirDest = -(inverse(nextRotation) * vec4(0, 0, 1, 0)).getXYZ();
		float f = float((static_cast<uint32_t>(gSunCurrentTime) >> 5) & 0xfff) / 8096.0f;
		vec3 sunLightDir = normalize(lerp(newLightDir, lightDirDest, fabsf(f * 2.f)));

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 1000.0f, 0.1f);

		gPerFrameData.WorldViewProjection = projMat * viewMat;
		gPerFrameData.WorldView = CameraMatrix::identity() * viewMat;
		gPerFrameData.LightDirection = vec4(sunLightDir, 1.0f);
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);


		// Reset cmd pool for this frame
		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		BufferUpdateDesc shaderCbv = { gPrecomputedDLUTPerFrameBuffer[gFrameIndex] };
		beginUpdateResource(&shaderCbv);
		*(PerFrame*)shaderCbv.pMappedData = gPerFrameData;
		endUpdateResource(&shaderCbv, NULL);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 0.0f;
		loadActions.mClearDepth.stencil = 0;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		{
			const uint32_t vbStride = sizeof(Vertex);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Precomputed DLUT");

			cmdBindPipeline(cmd, gPrecomputedDLUTPipeline);
			cmdBindDescriptorSet(cmd, gFrameIndex, gPrecomputedDLUTPerFrameDescriptorSet);
			cmdBindDescriptorSet(cmd, 0, gPrecomputedDLUTTextureDescriptorSet);
			cmdBindVertexBuffer(cmd, 1, &gPrecomputedDLUTVertexBuffer, &vbStride, NULL);
			cmdBindIndexBuffer(cmd, gPrecomputedDLUTIndexBuffer, INDEX_TYPE_UINT16, 0);
			cmdDrawIndexed(cmd, 6, 0, 0);

			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

		{
			loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
			cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

			cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

			gFrameTimeDraw.mFontColor = 0xff00ffff;
			gFrameTimeDraw.mFontSize = 18.0f;
			gFrameTimeDraw.mFontID = gFontID;
			float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(8.f, 15.f), &gFrameTimeDraw);
			cmdDrawGpuProfile(cmd, float2(8.f, txtSizePx.y + 75.0f), gGpuProfileToken, &gFrameTimeDraw);

			cmdDrawUserInterface(cmd);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
		}

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
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.mSubmitDone = true;

		// captureScreenshot() must be used before presentation.
		if (gTakeScreenshot)
		{
			// Metal platforms need one renderpass to prepare the swapchain textures for copy.
			if (prepareScreenshot(pSwapChain))
			{
				captureScreenshot(pSwapChain, swapchainImageIndex, RESOURCE_STATE_PRESENT, "37_PrecomputedVolumeDLUT.png");
				gTakeScreenshot = false;
			}
		}

		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "37_PrecomputedVolumeDLUT"; }

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
		swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc descriptorSetDescPerFrame = { gPrecomputedDLUTRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &descriptorSetDescPerFrame, &gPrecomputedDLUTPerFrameDescriptorSet);

		DescriptorSetDesc descriptorSetDescTexture = { gPrecomputedDLUTRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &descriptorSetDescTexture, &gPrecomputedDLUTTextureDescriptorSet);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, gPrecomputedDLUTPerFrameDescriptorSet);
		removeDescriptorSet(pRenderer, gPrecomputedDLUTTextureDescriptorSet);
	}

	void addRootSignatures()
	{
		const char* pStaticSamplerNames[] = { "gPrecomputedDLUTSampler" };
		Sampler* pStaticSamplers[] = { gPrecomputedDLUTSampler };

		RootSignatureDesc rootSignatureDesc = {};
		rootSignatureDesc.mShaderCount = 1;
		rootSignatureDesc.ppShaders = &gPrecomputedDLUTShader;
		rootSignatureDesc.mStaticSamplerCount = 1;
		rootSignatureDesc.ppStaticSamplerNames = pStaticSamplerNames;
		rootSignatureDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &rootSignatureDesc, &gPrecomputedDLUTRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, gPrecomputedDLUTRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc shaderLoadDesc = {};
		shaderLoadDesc.mStages[0] = { "PrecomputedDLUT.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		shaderLoadDesc.mStages[1] = { "PrecomputedDLUT.frag", NULL, 0 };

		addShader(pRenderer, &shaderLoadDesc, &gPrecomputedDLUTShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, gPrecomputedDLUTShader);
	}

	void addPipelines()
	{
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 2 * sizeof(float);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = false;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;

		BlendStateDesc blendStateAlphaDesc = {};
		blendStateAlphaDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateAlphaDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateAlphaDesc.mBlendModes[0] = BM_ADD;
		blendStateAlphaDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateAlphaDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateAlphaDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateAlphaDesc.mMasks[0] = ALL;
		blendStateAlphaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateAlphaDesc.mIndependentBlend = false;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pBlendState = &blendStateAlphaDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = gPrecomputedDLUTRootSignature;
		pipelineSettings.pShaderProgram = gPrecomputedDLUTShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.mVRFoveatedRendering = true;
		addPipeline(pRenderer, &desc, &gPrecomputedDLUTPipeline);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, gPrecomputedDLUTPipeline);
	}

	void prepareDescriptorSets()
	{
		for (uint32_t frame = 0; frame < gImageCount; ++frame)
		{
			DescriptorData params = {};
			params.pName = "PerFrame";
			params.ppBuffers = &gPrecomputedDLUTPerFrameBuffer[frame];

			updateDescriptorSet(pRenderer, frame, gPrecomputedDLUTPerFrameDescriptorSet, 1, &params);
		}

		DescriptorData params = {};
		params.pName = "gPrecomputedDLUT";
		params.ppTextures = &gPrecomputedDLUT;
		updateDescriptorSet(pRenderer, 0, gPrecomputedDLUTTextureDescriptorSet, 1, &params);
	}

	void addPrecomputedDLUTResources()
	{
		TextureLoadDesc textureLoadDesc = {};
		textureLoadDesc.mContainer = TEXTURE_CONTAINER_KTX;
		textureLoadDesc.ppTexture = &gPrecomputedDLUT;
		textureLoadDesc.pFileName = "dlut";

		addResource(&textureLoadDesc, NULL);

		BufferLoadDesc bufferLoadDesc = {};
		bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		bufferLoadDesc.mDesc.mSize = sizeof(PerFrame);
		bufferLoadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		bufferLoadDesc.pData = NULL;
		for (uint32_t frame = 0; frame < gImageCount; ++frame)
		{
			bufferLoadDesc.ppBuffer = &gPrecomputedDLUTPerFrameBuffer[frame];
			addResource(&bufferLoadDesc, NULL);
		}

		const Vertex vbData[4] = {
			{ float2(-50.0F, -50.0F), float2(0.0F, 0.0F) },
			{ float2(50.0F, -50.0F), float2(1.0F, 0.0F) },
			{ float2(-50.0F,  50.0F), float2(0.0F, 1.0F) },
			{ float2(50.0F,  50.0F), float2(1.0F, 1.0F) }
		};

		BufferLoadDesc vbLoadDesc = {};
		vbLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		vbLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbLoadDesc.mDesc.mSize = sizeof(Vertex) * 4;
		vbLoadDesc.pData = vbData;
		vbLoadDesc.ppBuffer = &gPrecomputedDLUTVertexBuffer;
		addResource(&vbLoadDesc, NULL);

		const uint16_t ibData[6] = { 0, 1, 2, 2, 1, 3 };

		BufferLoadDesc ibLoadDesc = {};
		ibLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		ibLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibLoadDesc.mDesc.mSize = sizeof(uint16_t) * 6;
		ibLoadDesc.pData = ibData;
		ibLoadDesc.ppBuffer = &gPrecomputedDLUTIndexBuffer;
		addResource(&ibLoadDesc, NULL);

		waitForAllResourceLoads();
	}

	void removePrecomputedDLUTResources()
	{
		removeResource(gPrecomputedDLUT);
		for (uint32_t frame = 0; frame < gImageCount; ++frame)
		{
			removeResource(gPrecomputedDLUTPerFrameBuffer[frame]);
		}
		removeResource(gPrecomputedDLUTVertexBuffer);
		removeResource(gPrecomputedDLUTIndexBuffer);
	}
};

DEFINE_APPLICATION_MAIN(PrecomputedVolumeDLUT)
