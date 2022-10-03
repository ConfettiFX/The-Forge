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

// Unit Test for testing wave intrinsic operations

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Demo structures
struct SceneConstantBuffer
{
	mat4 orthProjMatrix;
	float2 mousePosition;
	float2 resolution;
	float  time;
	uint   renderMode;
	uint   laneSize;
	uint   padding;
};

const uint32_t gImageCount = 3;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

RenderTarget* pRenderTargetIntermediate = NULL;

Shader*           pShaderWave = NULL;
Pipeline*         pPipelineWave = NULL;
Shader*           pShaderMagnify = NULL;
Pipeline*         pPipelineMagnify = NULL;
RootSignature*    pRootSignature = NULL;

DescriptorSet*    pDescriptorSetUniforms = NULL;
DescriptorSet*    pDescriptorSetTexture = NULL;

Sampler* pSamplerPointWrap = NULL;

Buffer* pUniformBuffer[gImageCount] = { NULL };
Buffer* pVertexBufferTriangle = NULL;
Buffer* pVertexBufferQuad = NULL;

char gCpuFrametimeText[64] = { 0 };

uint32_t gFrameIndex = 0;

SceneConstantBuffer gSceneData;
float2* pMovePosition = NULL;
float2 gMoveDelta = {};

/// UI
UIComponent* pGui = NULL;

enum RenderMode
{
	RenderMode1,
	RenderMode2,
	RenderMode3,
	RenderMode4,
	RenderMode5,
	RenderMode6,
	RenderMode7,
	RenderMode8,
	RenderMode9,
	RenderModeCount,
};
int32_t gRenderModeToggles = 0;

FontDrawDesc gFrameTimeDraw; 
uint32_t     gFontID = 0; 

static bool gWaveOpsSupported = false;

static HiresTimer gTimer;

struct Vertex
{
	float3 position;
	float4 color;
};

struct Vertex2
{
	float3 position;
	float2 uv;
};

class WaveIntrinsics: public IApp
{
	public:
	WaveIntrinsics()
	{
#ifndef TARGET_IOS
		mSettings.mWidth = 1920;
		mSettings.mHeight = 1080;
#endif
	}

	bool Init()
	{
		initHiresTimer(&gTimer);

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mShaderTarget = shader_target_6_0;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		gWaveOpsSupported = (pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_BASIC_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_QUAD_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_ARITHMETIC_BIT) &&
			(pRenderer->pActiveGpuSettings->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_VOTE_BIT);

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
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		SamplerDesc samplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_CLAMP_TO_BORDER };
		addSampler(pRenderer, &samplerDesc, &pSamplerPointWrap);

		// Define the geometry for a triangle.
		Vertex triangleVertices[] = { { { 0.0f, 0.5f, 0.0f }, { 0.8f, 0.8f, 0.0f, 1.0f } },
										{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.8f, 0.8f, 1.0f } },
										{ { -0.5f, -0.5f, 0.0f }, { 0.8f, 0.0f, 0.8f, 1.0f } } };

		BufferLoadDesc triangleColorDesc = {};
		triangleColorDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		triangleColorDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		triangleColorDesc.mDesc.mSize = sizeof(triangleVertices);
		triangleColorDesc.pData = triangleVertices;
		triangleColorDesc.ppBuffer = &pVertexBufferTriangle;
		addResource(&triangleColorDesc, NULL);

		// Define the geometry for a rectangle.
		Vertex2 quadVertices[] = { { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
									{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },

									{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, { { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
									{ { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } } };

		BufferLoadDesc quadUVDesc = {};
		quadUVDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadUVDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadUVDesc.mDesc.mSize = sizeof(quadVertices);
		quadUVDesc.pData = quadVertices;
		quadUVDesc.ppBuffer = &pVertexBufferQuad;
		addResource(&quadUVDesc, NULL);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(SceneConstantBuffer);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		waitForAllResourceLoads();

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

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.05f);
		uiCreateComponent("Render Modes", &guiDesc, &pGui);

		if (gWaveOpsSupported)
		{
			const char* m_labels[RenderModeCount] = {};
			m_labels[RenderMode1] = "1. Normal render.\n";
			m_labels[RenderMode2] = "2. Color pixels by lane indices.\n";
#ifdef TARGET_IOS
			m_labels[RenderMode9] = "3. Color pixels by their quad id.\n";
#else
			m_labels[RenderMode3] = "3. Show first lane (white dot) in each wave.\n";
			m_labels[RenderMode4] = "4. Show first(white dot) and last(red dot) lanes in each wave.\n";
			m_labels[RenderMode5] = "5. Color pixels by active lane ratio (white = 100%; black = 0%).\n";
			m_labels[RenderMode6] = "6. Broadcast the color of the first active lane to the wave.\n";
			m_labels[RenderMode7] = "7. Average the color in a wave.\n";
			m_labels[RenderMode8] = "8. Color pixels by prefix sum of distance between current and first lane.\n";
			m_labels[RenderMode9] = "9. Color pixels by their quad id.\n";
#endif

			// Radio Buttons
			for (uint32_t i = 0; i < RenderModeCount; ++i)
			{
#ifdef TARGET_IOS
				//Subset of supported render modes on iOS
				if (i == RenderMode1 || i == RenderMode2 || i == RenderMode9)
				{
					RadioButtonWidget iosRenderMode;
					iosRenderMode.pData = &gRenderModeToggles;
					iosRenderMode.mRadioId = i;
                    luaRegisterWidget(uiCreateComponentWidget(pGui, m_labels[i], &iosRenderMode, WIDGET_TYPE_RADIO_BUTTON));
				}
#else
				RadioButtonWidget modeToggle;
				modeToggle.pData = &gRenderModeToggles;
				modeToggle.mRadioId = i;
				luaRegisterWidget(uiCreateComponentWidget(pGui, m_labels[i], &modeToggle, WIDGET_TYPE_RADIO_BUTTON));
#endif
			}
		}
		else
		{
			LabelWidget notSupportedLabel;
			luaRegisterWidget(uiCreateComponentWidget(pGui, "Some of wave ops are not supported on this GPU", &notSupportedLabel, WIDGET_TYPE_LABEL));
		}

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
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
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { pMovePosition = NULL; gMoveDelta = ctx->mFloat2; return true; }};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) { 
			if (ctx->mBool && !uiIsFocused()) 
				pMovePosition = ctx->pPosition; 
			else 
				pMovePosition = NULL; 
			return true; 
		}};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0;

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		exitUserInterface();

		exitFontSystem(); 

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}
		removeResource(pVertexBufferQuad);
		removeResource(pVertexBufferTriangle);

		removeSampler(pRenderer, pSamplerPointWrap);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType == RELOAD_TYPE_ALL)
		{
			gSceneData.mousePosition.x = mSettings.mWidth * 0.5f;
			gSceneData.mousePosition.y = mSettings.mHeight * 0.5f;
		}

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

			if (!addIntermediateRenderTarget())
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
			removeRenderTarget(pRenderer, pRenderTargetIntermediate);
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

		/************************************************************************/
		// Uniforms
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		float aspectRatio = (float)mSettings.mWidth / mSettings.mHeight;
		gSceneData.orthProjMatrix = mat4::orthographic(-1.0f * aspectRatio, 1.0f * aspectRatio, -1.0f, 1.0f, 0.0f, 1.0f);
		gSceneData.laneSize = pRenderer->pActiveGpuSettings->mWaveLaneCount;
		gSceneData.time = currentTime;
		gSceneData.resolution.x = (float)(mSettings.mWidth);
		gSceneData.resolution.y = (float)(mSettings.mHeight);
		gSceneData.renderMode = (gRenderModeToggles + 1);

		if (pMovePosition)
			gSceneData.mousePosition = *pMovePosition;

		gSceneData.mousePosition += float2(gMoveDelta.x, -gMoveDelta.y);
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
		/************************************************************************/
		/************************************************************************/
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		BufferUpdateDesc viewProjCbv = { pUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(SceneConstantBuffer*)viewProjCbv.pMappedData = gSceneData;
		endUpdateResource(&viewProjCbv, NULL);

		RenderTarget* pRenderTarget = pRenderTargetIntermediate;
		RenderTarget* pScreenRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mClearValue;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		RenderTargetBarrier rtBarrier[] = {
			{ pRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			{ pScreenRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarrier);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// wave debug
		if (gWaveOpsSupported)
		{
			const uint32_t triangleStride = sizeof(Vertex);
			cmdBeginDebugMarker(cmd, 0, 0, 1, "Wave Shader");
			cmdBindPipeline(cmd, pPipelineWave);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
			cmdBindVertexBuffer(cmd, 1, &pVertexBufferTriangle, &triangleStride, NULL);
			cmdDraw(cmd, 3, 0);
			cmdEndDebugMarker(cmd);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		// magnify
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Magnify");
		RenderTargetBarrier srvBarrier[] = {
			{ pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, srvBarrier);

		loadActions.mClearColorValues[0] = pScreenRenderTarget->mClearValue;
		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		const uint32_t quadStride = sizeof(Vertex2);
		cmdBindPipeline(cmd, pPipelineMagnify);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferQuad, &quadStride, NULL);
		cmdDrawInstanced(cmd, 6, 0, 2, 0);

		cmdEndDebugMarker(cmd);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		getHiresTimerUSec(&gTimer, true);

		sprintf(gCpuFrametimeText, "CPU %f ms", getHiresTimerUSecAverage(&gTimer) / 1000.0f);

		gFrameTimeDraw.pText = gCpuFrametimeText; 
		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
		cmdDrawTextWithFont(cmd, float2(8, 15), &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		RenderTargetBarrier presentBarrier = { pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &presentBarrier);
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

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "14_WaveIntrinsics"; }

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
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
	}

	void addRootSignatures()
	{
		Shader* pShaders[] = { pShaderMagnify, pShaderWave };
		const char* pStaticSamplers[] = { "g_sampler" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerPointWrap;
		rootDesc.mShaderCount = gWaveOpsSupported ? 2 : 1;
		rootDesc.ppShaders = pShaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
	}

	void addShaders()
	{
		if (gWaveOpsSupported)
		{
			ShaderLoadDesc waveShader = {};
			waveShader.mStages[0] = { "wave.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			waveShader.mStages[1] = { "wave.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
			waveShader.mTarget = shader_target_6_0;
			addShader(pRenderer, &waveShader, &pShaderWave);
		}

		ShaderLoadDesc magnifyShader = {};
		magnifyShader.mStages[0] = { "magnify.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		magnifyShader.mStages[1] = { "magnify.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &magnifyShader, &pShaderMagnify);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShaderMagnify);
		if (gWaveOpsSupported)
		{
			removeShader(pRenderer, pShaderWave);
		}
	}

	void addPipelines()
	{
		//layout and pipeline for sphere draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc depthStateDesc = {};

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShaderWave;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		if (gWaveOpsSupported)
		{
			addPipeline(pRenderer, &desc, &pPipelineWave);
		}

		//layout and pipeline for skybox draw
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		pipelineSettings.pShaderProgram = pShaderMagnify;
		addPipeline(pRenderer, &desc, &pPipelineMagnify);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineMagnify);
		if (gWaveOpsSupported)
		{
			removePipeline(pRenderer, pPipelineWave);
		}
	}

	void prepareDescriptorSets()
	{
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "SceneConstantBuffer";
			params[0].ppBuffers = &pUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
		}

		DescriptorData magnifyParams[1] = {};
		magnifyParams[0].pName = "g_texture";
		magnifyParams[0].ppTextures = &pRenderTargetIntermediate->pTexture;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 1, magnifyParams);
	}

	bool addIntermediateRenderTarget()
	{
		// Add depth buffer
		RenderTargetDesc rtDesc = {};
		rtDesc.mArraySize = 1;
		rtDesc.mClearValue = { { 0.001f, 0.001f, 0.001f, 0.001f } }; // This is a temporary workaround for AMD cards on macOS. Setting this to (0,0,0,0) will introduce weird behavior.
		rtDesc.mDepth = 1;
		rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		rtDesc.mFormat = getRecommendedSwapchainFormat(true, true);
		rtDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rtDesc.mHeight = mSettings.mHeight;
		rtDesc.mSampleCount = SAMPLE_COUNT_1;
		rtDesc.mSampleQuality = 0;
		rtDesc.mWidth = mSettings.mWidth;
        rtDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &rtDesc, &pRenderTargetIntermediate);

		return pRenderTargetIntermediate != NULL;
	}
};

DEFINE_APPLICATION_MAIN(WaveIntrinsics)
