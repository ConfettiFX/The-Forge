/*
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

// Unit Test for testing wave intrinsic operations

#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"
//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

/// Demo structures
struct SceneConstantBuffer
{
	mat4   orthProjMatrix;
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
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

RenderTarget* pRenderTargetIntermediate = NULL;

Shader*        pShaderWave = NULL;
Pipeline*      pPipelineWave = NULL;
RootSignature* pRootSignatureWave = NULL;
Shader*        pShaderMagnify = NULL;
Pipeline*      pPipelineMagnify = NULL;
RootSignature* pRootSignatureMagnify = NULL;

Sampler* pSamplerPointWrap = NULL;
#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif
DepthState*      pDepthNone = NULL;
RasterizerState* pRasterizerCullNone = NULL;

Buffer* pUniformBuffer[gImageCount] = { NULL };
Buffer* pVertexBufferTriangle = NULL;
Buffer* pVertexBufferQuad = NULL;

uint32_t gFrameIndex = 0;

SceneConstantBuffer gSceneData;

/// UI
UIApp         gAppUI;
GuiComponent* pGui = NULL;

FileSystem gFileSystem;
LogManager gLogManager;

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

const char* pszBases[FSR_Count] = {
	"../../../src/14_WaveIntrinsics/",      // FSR_BinShaders
	"../../../src/14_WaveIntrinsics/",      // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/14_WaveIntrinsics/",      // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class WaveIntrinsics: public IApp
{
	public:
	WaveIntrinsics()
	{
		mSettings.mWidth = 1920;
		mSettings.mHeight = 1080;
	}

	bool Init()
	{
		// window and renderer setup
		RendererDesc settings = { 0 };
		settings.mShaderTarget = shader_target_6_0;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

#ifdef METAL
		//Instead of setting the gpu to Office and disabling the Unit test on more platforms, we do it here as the issue is only specific to Metal.
		if (stricmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId, "0x1002") == 0 &&
			stricmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId, "0x67df") == 0)
		{
			LOGERROR("This GPU model causes Internal Shader compiler errors on Metal when compiling the wave instrinsics.");
			//exit instead of returning not to trigger failure in Jenkins
			exit(0);
		}
#endif

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Absolute))
			return false;
#endif

		ShaderLoadDesc waveShader = {};
		waveShader.mStages[0] = { "wave.vert", NULL, 0, FSR_SrcShaders };
		waveShader.mStages[1] = { "wave.frag", NULL, 0, FSR_SrcShaders };
		waveShader.mTarget = shader_target_6_0;
		ShaderLoadDesc magnifyShader = {};
		magnifyShader.mStages[0] = { "magnify.vert", NULL, 0, FSR_SrcShaders };
		magnifyShader.mStages[1] = { "magnify.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &waveShader, &pShaderWave);
		addShader(pRenderer, &magnifyShader, &pShaderMagnify);

		SamplerDesc samplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_CLAMP_TO_BORDER };
		addSampler(pRenderer, &samplerDesc, &pSamplerPointWrap);

		const char*       pStaticSamplers[] = { "g_sampler" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerPointWrap;
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pShaderWave;
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureWave);

		rootDesc.ppShaders = &pShaderMagnify;
		addRootSignature(pRenderer, &rootDesc, &pRootSignatureMagnify);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerCullNone);

		DepthStateDesc depthStateDesc = {};
		addDepthState(pRenderer, &depthStateDesc, &pDepthNone);

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

		// Define the geometry for a triangle.
		Vertex triangleVertices[] = { { { 0.0f, 0.5f, 0.0f }, { 0.8f, 0.8f, 0.0f, 1.0f } },
									  { { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.8f, 0.8f, 1.0f } },
									  { { -0.5f, -0.5f, 0.0f }, { 0.8f, 0.0f, 0.8f, 1.0f } } };

		BufferLoadDesc triangleColorDesc = {};
		triangleColorDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		triangleColorDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		triangleColorDesc.mDesc.mVertexStride = sizeof(Vertex);
		triangleColorDesc.mDesc.mSize = sizeof(triangleVertices);
		triangleColorDesc.pData = triangleVertices;
		triangleColorDesc.ppBuffer = &pVertexBufferTriangle;
		addResource(&triangleColorDesc);

		// Define the geometry for a rectangle.
		Vertex2 quadVertices[] = { { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
								   { { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },

								   { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }, { { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
								   { { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } } };

		BufferLoadDesc quadUVDesc = {};
		quadUVDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadUVDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadUVDesc.mDesc.mVertexStride = sizeof(Vertex2);
		quadUVDesc.mDesc.mSize = sizeof(quadVertices);
		quadUVDesc.pData = quadVertices;
		quadUVDesc.ppBuffer = &pVertexBufferQuad;
		addResource(&quadUVDesc);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(SceneConstantBuffer);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pUniformBuffer[i];
			addResource(&ubDesc);
		}

		finishResourceLoading();

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);
		GuiDesc guiDesc = {};
		pGui = gAppUI.AddGuiComponent("Render Modes", &guiDesc);

		const char* m_labels[RenderModeCount] = {};
		m_labels[RenderMode1] = "1. Normal render.\n";
		m_labels[RenderMode2] = "2. Color pixels by lane indices.\n";
		m_labels[RenderMode3] = "3. Show first lane (white dot) in each wave.\n";
		m_labels[RenderMode4] = "4. Show first(white dot) and last(red dot) lanes in each wave.\n";
		m_labels[RenderMode5] = "5. Color pixels by active lane ratio (white = 100%; black = 0%).\n";
		m_labels[RenderMode6] = "6. Broadcast the color of the first active lane to the wave.\n";
		m_labels[RenderMode7] = "7. Average the color in a wave.\n";
		m_labels[RenderMode8] = "8. Color pixels by prefix sum of distance between current and first lane.\n";
		m_labels[RenderMode9] = "9. Color pixels by their quad id.\n";

		// Radio Buttons
		for (uint32_t i = 0; i < RenderModeCount; ++i)
		{
			pGui->AddWidget(RadioButtonWidget(m_labels[i], &gRenderModeToggles, i));
		}

		requestMouseCapture(true);
		InputSystem::RegisterInputEvent(onInput);

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		removeDebugRendererInterface();

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}
		removeResource(pVertexBufferQuad);
		removeResource(pVertexBufferTriangle);

		removeSampler(pRenderer, pSamplerPointWrap);
		removeShader(pRenderer, pShaderMagnify);
		removeRootSignature(pRenderer, pRootSignatureMagnify);
		removeShader(pRenderer, pShaderWave);
		removeRootSignature(pRenderer, pRootSignatureWave);

		removeDepthState(pDepthNone);
		removeRasterizerState(pRasterizerCullNone);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addIntermediateRenderTarget())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], pDepthBuffer->mDesc.mFormat))
			return false;
#endif

		//layout and pipeline for sphere draw
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthNone;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignatureWave;
		pipelineSettings.pShaderProgram = pShaderWave;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineWave);

		//layout and pipeline for skybox draw
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		pipelineSettings.pRootSignature = pRootSignatureMagnify;
		pipelineSettings.pShaderProgram = pShaderMagnify;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineMagnify);

		gSceneData.mousePosition.x = mSettings.mWidth * 0.5f;
		gSceneData.mousePosition.y = mSettings.mHeight * 0.5f;

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pPipelineMagnify);
		removePipeline(pRenderer, pPipelineWave);

		removeRenderTarget(pRenderer, pRenderTargetIntermediate);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		gAppUI.Update(deltaTime);
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
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		BufferUpdateDesc viewProjCbv = { pUniformBuffer[gFrameIndex], &gSceneData };
		updateResource(&viewProjCbv);
		/************************************************************************/
		/************************************************************************/
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);

		RenderTarget* pRenderTarget = pRenderTargetIntermediate;
		RenderTarget* pScreenRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTarget->mDesc.mClearValue;

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		TextureBarrier rtBarrier[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pScreenRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, rtBarrier, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// wave debug
		cmdBeginDebugMarker(cmd, 0, 0, 1, "Wave Shader");
		cmdBindPipeline(cmd, pPipelineWave);
		DescriptorData params[1] = {};
		params[0].pName = "SceneConstantBuffer";
		params[0].ppBuffers = &pUniformBuffer[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignatureWave, 1, params);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferTriangle, NULL);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// magnify
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Magnify");
		TextureBarrier srvBarrier[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarrier, false);

		loadActions.mClearColorValues[0] = pScreenRenderTarget->mDesc.mClearValue;
		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		DescriptorData magnifyParams[2] = {};
		magnifyParams[0].pName = "SceneConstantBuffer";
		magnifyParams[0].ppBuffers = &pUniformBuffer[gFrameIndex];
		magnifyParams[1].pName = "g_texture";
		magnifyParams[1].ppTextures = &pRenderTarget->pTexture;
		cmdBindDescriptors(cmd, pRootSignatureMagnify, 2, magnifyParams);
		cmdBindPipeline(cmd, pPipelineMagnify);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferQuad, NULL);
		cmdDrawInstanced(cmd, 6, 0, 2, 0);

		cmdEndDebugMarker(cmd);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		TextureBarrier presentBarrier = { pScreenRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &presentBarrier, true);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "14_WaveIntrinsics"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
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

	bool addIntermediateRenderTarget()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		depthRT.mDepth = 1;
		depthRT.mFormat = getRecommendedSwapchainFormat(true);
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetIntermediate);

		return pRenderTargetIntermediate != NULL;
	}

	static bool onInput(const ButtonData* pData)
	{
		if (InputSystem::IsMouseCaptured())
		{
			if (pData->mUserId == KEY_UI_MOVE)
			{
				if (InputSystem::IsButtonPressed(KEY_CONFIRM))
				{
					gSceneData.mousePosition.x = pData->mValue[0];
					gSceneData.mousePosition.y = pData->mValue[1];

					return true;
				}
			}
		}
		return false;
	}
};

DEFINE_APPLICATION_MAIN(WaveIntrinsics)
