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

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

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

Shader*           pShaderWave = NULL;
Pipeline*         pPipelineWave = NULL;
Shader*           pShaderMagnify = NULL;
Pipeline*         pPipelineMagnify = NULL;
RootSignature*    pRootSignature = NULL;

DescriptorSet*    pDescriptorSetUniforms = NULL;
DescriptorSet*    pDescriptorSetTexture = NULL;

Sampler* pSamplerPointWrap = NULL;

DepthState*      pDepthNone = NULL;
RasterizerState* pRasterizerCullNone = NULL;

Buffer* pUniformBuffer[gImageCount] = { NULL };
Buffer* pVertexBufferTriangle = NULL;
Buffer* pVertexBufferQuad = NULL;

uint32_t gFrameIndex = 0;

SceneConstantBuffer gSceneData;
float2* pMovePosition = NULL;
float2 gMoveDelta = {};

/// UI
UIApp         gAppUI;
GuiComponent* pGui = NULL;

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

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

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
        // FILE PATHS
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/14_WaveIntrinsics");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,        "../../UnitTestResources/Textures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,          "../../UnitTestResources/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,    "../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,      "../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,  "../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,    "../../../../Middleware_3/UI");
        }
        
		// window and renderer setup
		RendererDesc settings = { 0 };

#if defined(_DURANGO)
		settings.mShaderTarget = shader_target_5_1;
#else
		settings.mShaderTarget = shader_target_6_0;
#endif

		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

#ifdef METAL
		//Instead of setting the gpu to Office and disabling the Unit test on more platforms, we do it here as the issue is only specific to Metal.
		if (stricmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mVendorId, "0x1002") == 0 &&
			stricmp(pRenderer->pActiveGpuSettings->mGpuVendorPreset.mModelId, "0x67df") == 0)
		{
			LOGF(LogLevel::eERROR, "This GPU model causes Internal Shader compiler errors on Metal when compiling the wave instrinsics.");
			removeRenderer(pRenderer);
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

		initResourceLoaderInterface(pRenderer);

		ShaderLoadDesc waveShader = {};
		waveShader.mStages[0] = { "wave.vert", NULL, 0, RD_SHADER_SOURCES };
		waveShader.mStages[1] = { "wave.frag", NULL, 0, RD_SHADER_SOURCES };

#if defined(_DURANGO)
		waveShader.mTarget = shader_target_5_1;
#else
		waveShader.mTarget = shader_target_6_0;
#endif

#ifdef TARGET_IOS
		ShaderMacro iosMacro;
		iosMacro.definition = "TARGET_IOS";
		iosMacro.value = "1";
		waveShader.mStages[1].mMacroCount = 1;
		waveShader.mStages[1].pMacros = &iosMacro;
#endif

		ShaderLoadDesc magnifyShader = {};
		magnifyShader.mStages[0] = { "magnify.vert", NULL, 0, RD_SHADER_SOURCES };
		magnifyShader.mStages[1] = { "magnify.frag", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &waveShader, &pShaderWave);
		addShader(pRenderer, &magnifyShader, &pShaderMagnify);

		SamplerDesc samplerDesc = { FILTER_NEAREST,      FILTER_NEAREST,      MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_CLAMP_TO_BORDER };
		addSampler(pRenderer, &samplerDesc, &pSamplerPointWrap);

		Shader* pShaders[] = { pShaderMagnify, pShaderWave };
		const char*       pStaticSamplers[] = { "g_sampler" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSamplerPointWrap;
		rootDesc.mShaderCount = 2;
		rootDesc.ppShaders = pShaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);

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

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);
		GuiDesc guiDesc = {};
		pGui = gAppUI.AddGuiComponent("Render Modes", &guiDesc);

		const char* m_labels[RenderModeCount] = {};
		m_labels[RenderMode1] = "1. Normal render.\n";
		m_labels[RenderMode2] = "2. Color pixels by lane indices.\n";
#ifdef TARGET_IOS
		m_labels[RenderMode9] = "3. Color pixels by their quad id.\n";

#elif _DURANGO
		m_labels[RenderMode3] = "3. Show first lane (white dot) in each wave.\n";
		m_labels[RenderMode4] = "4. Show first(white dot) and last(red dot) lanes in each wave.\n";
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
			if(i == RenderMode1 || i == RenderMode2 || i == RenderMode9)
				pGui->AddWidget(RadioButtonWidget(m_labels[i], &gRenderModeToggles, i));
#elif _DURANGO
			if (i == RenderMode1 || i == RenderMode2 || i == RenderMode3 || i == RenderMode4)
				pGui->AddWidget(RadioButtonWidget(m_labels[i], &gRenderModeToggles, i));
#else
			pGui->AddWidget(RadioButtonWidget(m_labels[i], &gRenderModeToggles, i));
#endif
		}

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
				if (gAppUI.OnButton(ctx->mBinding, ctx->mBool, ctx->pPosition) && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase)
					pMovePosition = ctx->pPosition;
				else
					pMovePosition = NULL;
				return true;
			}, this
		};
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { pMovePosition = NULL; gMoveDelta = ctx->mFloat2; return true; } };
		addInputAction(&actionDesc);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "SceneConstantBuffer";
			params[0].ppBuffers = &pUniformBuffer[i];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
		}

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}
		removeResource(pVertexBufferQuad);
		removeResource(pVertexBufferTriangle);

		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		removeDescriptorSet(pRenderer, pDescriptorSetTexture);

		removeSampler(pRenderer, pSamplerPointWrap);
		removeShader(pRenderer, pShaderMagnify);
		removeShader(pRenderer, pShaderWave);
		removeRootSignature(pRenderer, pRootSignature);

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

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthNone;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShaderWave;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerCullNone;
		addPipeline(pRenderer, &desc, &pPipelineWave);

		//layout and pipeline for skybox draw
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		pipelineSettings.pShaderProgram = pShaderMagnify;
		addPipeline(pRenderer, &desc, &pPipelineMagnify);

		gSceneData.mousePosition.x = mSettings.mWidth * 0.5f;
		gSceneData.mousePosition.y = mSettings.mHeight * 0.5f;

		DescriptorData magnifyParams[1] = {};
		magnifyParams[0].pName = "g_texture";
		magnifyParams[0].ppTextures = &pRenderTargetIntermediate->pTexture;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 1, magnifyParams);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineMagnify);
		removePipeline(pRenderer, pPipelineWave);

		removeRenderTarget(pRenderer, pRenderTargetIntermediate);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

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

		if (pMovePosition)
			gSceneData.mousePosition = *pMovePosition;

		gSceneData.mousePosition += gMoveDelta;
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
		/************************************************************************/
		/************************************************************************/
		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		BufferUpdateDesc viewProjCbv = { pUniformBuffer[gFrameIndex], &gSceneData };
		updateResource(&viewProjCbv);

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
		cmdResourceBarrier(cmd, 0, NULL, 2, rtBarrier);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		// wave debug
		cmdBeginDebugMarker(cmd, 0, 0, 1, "Wave Shader");
		cmdBindPipeline(cmd, pPipelineWave);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferTriangle, NULL);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		// magnify
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Magnify");
		TextureBarrier srvBarrier[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarrier);

		loadActions.mClearColorValues[0] = pScreenRenderTarget->mDesc.mClearValue;
		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		cmdBindPipeline(cmd, pPipelineMagnify);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferQuad, NULL);
		cmdDrawInstanced(cmd, 6, 0, 2, 0);

		cmdEndDebugMarker(cmd);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		TextureBarrier presentBarrier = { pScreenRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &presentBarrier);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
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
};

DEFINE_APPLICATION_MAIN(WaveIntrinsics)
