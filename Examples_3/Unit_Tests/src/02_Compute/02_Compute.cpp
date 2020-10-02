/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#define _USE_MATH_DEFINES

// Unit Test for testing Compute Shaders
// using Julia 4D demonstration
//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/IResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"

// Julia4D Parameters
#define SIZE_X 412
#define SIZE_Y 256
#define SIZE_Z 1024

const float gTimeScale = 0.2f;

float gZoom = 1;
int   gRenderSoftShadows = 1;
float gEpsilon = 0.003f;
float gColorT = 0.0f;
float gColorA[4] = { 0.25f, 0.45f, 1.0f, 1.0f };
float gColorB[4] = { 0.25f, 0.45f, 1.0f, 1.0f };
float gColorC[4] = { 0.25f, 0.45f, 1.0f, 1.0f };
float gMuT = 0.0f;
float gMuA[4] = { -.278f, -.479f, 0.0f, 0.0f };
float gMuB[4] = { 0.278f, 0.479f, 0.0f, 0.0f };
float gMuC[4] = { -.278f, -.479f, -.231f, .235f };

struct UniformBlock
{
	mat4  mProjectView;
	vec4  mDiffuseColor;
	vec4  mMu;
	float mEpsilon;
	float mZoom;

	int mWidth;
	int mHeight;
	int mRenderSoftShadows;
};


const uint32_t gImageCount = 3;

Renderer* pRenderer = NULL;
Buffer*   pUniformBuffer[gImageCount] = { NULL };

Queue*           pGraphicsQueue = NULL;
CmdPool*         pCmdPools[gImageCount];
Cmd*             pCmds[gImageCount];
Sampler*         pSampler = NULL;

Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;

Shader*            pShader = NULL;
Pipeline*          pPipeline = NULL;
RootSignature*     pRootSignature = NULL;

Shader*           pComputeShader = NULL;
Pipeline*         pComputePipeline = NULL;
RootSignature*    pComputeRootSignature = NULL;
Texture*          pTextureComputeOutput = NULL;

DescriptorSet*    pDescriptorSetUniforms = NULL;
DescriptorSet*    pDescriptorSetComputeTexture = NULL;
DescriptorSet*    pDescriptorSetTexture = NULL;

VirtualJoystickUI gVirtualJoystick;

uint32_t     gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;
UniformBlock gUniformData;

UIApp              gAppUI;
ICameraController* pCameraController = NULL;

struct ObjectProperty
{
	float mRotX = 0, mRotY = 0;
} gObjSettings;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class Compute: public IApp
{
	public:
	Compute()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
        // FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,	"Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_SHADER_BINARIES,	"CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,		"GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,			"Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,			"Fonts");
        
		initNoise();

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		CmdPoolDesc cmdPoolDesc = {};
		cmdPoolDesc.pQueue = pGraphicsQueue;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
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

		if (!gVirtualJoystick.Init(pRenderer, "circlepad"))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		ShaderLoadDesc displayShader = {};
		displayShader.mStages[0] = { "display.vert", NULL, 0 };
		displayShader.mStages[1] = { "display.frag", NULL, 0 };
		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0 };

		addShader(pRenderer, &displayShader, &pShader);
		addShader(pRenderer, &computeShader, &pComputeShader);

		SamplerDesc samplerDesc = { FILTER_NEAREST,
									FILTER_NEAREST,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);

		const char*       pStaticSamplers[] = { "uSampler0" };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 1;
		rootDesc.ppStaticSamplerNames = pStaticSamplers;
		rootDesc.ppStaticSamplers = &pSampler;
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = &pShader;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		RootSignatureDesc computeRootDesc = {};
		computeRootDesc.mShaderCount = 1;
		computeRootDesc.ppShaders = &pComputeShader;
		addRootSignature(pRenderer, &computeRootDesc, &pComputeRootSignature);

		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTexture);
		setDesc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetComputeTexture);
		setDesc = { pComputeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = desc.mComputeDesc;
		computePipelineDesc.pRootSignature = pComputeRootSignature;
		computePipelineDesc.pShaderProgram = pComputeShader;
		addPipeline(pRenderer, &desc, &pComputePipeline);

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

		// Width and height needs to be same as Texture's
		gUniformData.mHeight = mSettings.mHeight;
		gUniformData.mWidth = mSettings.mWidth;

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf");
				
		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

		// Initialize profile
		initProfiler();
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

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
			if (!gAppUI.IsFocused() && *ctx->pCaptured)
			{
				gVirtualJoystick.OnMove(index, ctx->mPhase != INPUT_ACTION_PHASE_CANCELED, ctx->pPosition);
				index ? pCameraController->onRotate(ctx->mFloat2) : pCameraController->onMove(ctx->mFloat2);
			}
			return true;
		};
		actionDesc = { InputBindings::FLOAT_RIGHTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL, 20.0f, 200.0f, 0.5f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::FLOAT_LEFTSTICK, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL, 20.0f, 200.0f, 1.0f };
		addInputAction(&actionDesc);
		actionDesc = { InputBindings::BUTTON_NORTH, [](InputActionContext* ctx) { pCameraController->resetView(); return true; } };
		addInputAction(&actionDesc);

		waitForAllResourceLoads();

		DescriptorData params[1] = {};
		params[0].pName = "uniformBlock";
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			params[0].ppBuffers = &pUniformBuffer[gFrameIndex];
			updateDescriptorSet(pRenderer, i, pDescriptorSetUniforms, 1, params);
		}

		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitInputSystem();

		destroyCameraController(pCameraController);

		gVirtualJoystick.Exit();

		gAppUI.Exit();

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pUniformBuffer[i]);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		removeSampler(pRenderer, pSampler);

		removeShader(pRenderer, pShader);
		removeShader(pRenderer, pComputeShader);
		removePipeline(pRenderer, pComputePipeline);
		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetComputeTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pComputeRootSignature);

		exitResourceLoaderInterface(pRenderer);
		removeQueue(pRenderer, pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addJuliaFractalUAV())
			return false;

		if (!gAppUI.Load(pSwapChain->ppRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppRenderTargets[0]))
			return false;

		loadProfilerUI(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		waitForAllResourceLoads();

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShader;
		addPipeline(pRenderer, &desc, &pPipeline);

		DescriptorData params[1] = {};
		params[0].pName = "uTex0";
		params[0].ppTextures = &pTextureComputeOutput;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetTexture, 1, params);

		params[0].pName = "outputTexture";
		params[0].ppTextures = &pTextureComputeOutput;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetComputeTexture, 1, params);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		unloadProfilerUI();

		gVirtualJoystick.Unload();

		gAppUI.Unload();

		removePipeline(pRenderer, pPipeline);

		removeResource(pTextureComputeOutput);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		updateInputSystem(mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);

		const float k_wrapAround = (float)(M_PI * 2.0);
		if (gObjSettings.mRotX > k_wrapAround)
			gObjSettings.mRotX -= k_wrapAround;
		if (gObjSettings.mRotX < -k_wrapAround)
			gObjSettings.mRotX += k_wrapAround;
		if (gObjSettings.mRotY > k_wrapAround)
			gObjSettings.mRotY -= k_wrapAround;
		if (gObjSettings.mRotY < -k_wrapAround)
			gObjSettings.mRotY += k_wrapAround;

		UpdateMu(deltaTime, &gMuT, gMuA, gMuB);
		Interpolate(gMuC, gMuT, gMuA, gMuB);
		UpdateColor(deltaTime, &gColorT, gColorA, gColorB);
		Interpolate(gColorC, gColorT, gColorA, gColorB);
		/************************************************************************/
		// Compute matrices
		/************************************************************************/
		mat4 rotMat = mat4::rotationX(gObjSettings.mRotX) * mat4::rotationY(gObjSettings.mRotY);
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformData.mProjectView = projMat * viewMat * rotMat;

		gUniformData.mDiffuseColor = vec4(gColorC[0], gColorC[1], gColorC[2], gColorC[3]);
		gUniformData.mRenderSoftShadows = gRenderSoftShadows;
		gUniformData.mEpsilon = gEpsilon;
		gUniformData.mZoom = gZoom;
		gUniformData.mMu = vec4(gMuC[0], gMuC[1], gMuC[2], gMuC[3]);
		gUniformData.mWidth = mSettings.mWidth;
		gUniformData.mHeight = mSettings.mHeight;

		/************************************************************************/
		// Update GUI
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);
		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		BufferUpdateDesc cbvUpdate = { pUniformBuffer[gFrameIndex] };
		beginUpdateResource(&cbvUpdate);
		*(UniformBlock*)cbvUpdate.pMappedData = gUniformData;
		endUpdateResource(&cbvUpdate, NULL);

		const uint32_t* pThreadGroupSize = pComputeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Compute Pass");

		// Compute Julia 4D
		cmdBindPipeline(cmd, pComputePipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetComputeTexture);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);

		uint32_t groupCountX = (mSettings.mWidth + pThreadGroupSize[0] - 1) / pThreadGroupSize[0];
		uint32_t groupCountY = (mSettings.mHeight + pThreadGroupSize[1] - 1) / pThreadGroupSize[1];
		cmdDispatch(cmd, groupCountX, groupCountY, pThreadGroupSize[2]);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		TextureBarrier barriers[] = {
			{ pTextureComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
		};
		RenderTargetBarrier rtBarriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, 1, rtBarriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Pass");
		// Draw computed results
		cmdBindPipeline(cmd, pPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);

		cmdDraw(cmd, 3, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		const float	txtIndent = 8.f;
		float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
		cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 30.f), gGpuProfileToken, &gFrameTimeDraw);

        cmdDrawProfilerUI();
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		rtBarriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		barriers[0] = { pTextureComputeOutput, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, 1, rtBarriers);

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
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "02_Compute"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = mSettings.mDefaultVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addJuliaFractalUAV()
	{
		// Create empty texture for output of compute shader
		TextureLoadDesc textureDesc = {};
		TextureDesc     desc = {};
		desc.mWidth = mSettings.mWidth;
		desc.mHeight = mSettings.mHeight;
		desc.mDepth = 1;
		desc.mArraySize = 1;
		desc.mMipLevels = 1;
		desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mHostVisible = false;
		desc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		textureDesc.pDesc = &desc;
		textureDesc.ppTexture = &pTextureComputeOutput;
		addResource(&textureDesc, NULL);

		return pTextureComputeOutput != NULL;
	}

	void Interpolate(float m[4], float t, float a[4], float b[4])
	{
		int i;
		for (i = 0; i < 4; i++)
			m[i] = (1.0f - t) * a[i] + t * b[i];
	}

	void UpdateMu(float deltaTime, float t[4], float a[4], float b[4])
	{
		*t += gTimeScale * deltaTime;

		if (*t >= 1.0f)
		{
			*t = 0.0f;

			a[0] = b[0];
			a[1] = b[1];
			a[2] = b[2];
			a[3] = b[3];

			b[0] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
			b[1] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
			b[2] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
			b[3] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
		}
	}

	void RandomColor(float v[4])
	{
		do
		{
			v[0] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
			v[1] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
			v[2] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
		} while (v[0] < 0 && v[1] < 0 && v[2] < 0);    // prevent black colors
		v[3] = 1.0f;
	}

	void UpdateColor(float deltaTime, float t[4], float a[4], float b[4])
	{
		*t += gTimeScale * deltaTime;

		if (*t >= 1.0f)
		{
			*t = 0.0f;

			a[0] = b[0];
			a[1] = b[1];
			a[2] = b[2];
			a[3] = b[3];

			RandomColor(b);
		}
	}
};

DEFINE_APPLICATION_MAIN(Compute)
