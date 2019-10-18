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
#include "../../../../Common_3/Renderer/ResourceLoader.h"

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

Timer      gAccumTimer;
HiresTimer gTimer;

const uint32_t gImageCount = 3;
bool           gMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;
Buffer*   pUniformBuffer[gImageCount] = { NULL };

Queue*           pGraphicsQueue = NULL;
CmdPool*         pCmdPool = NULL;
Cmd**            ppCmds = NULL;
Sampler*         pSampler = NULL;
RasterizerState* pRast = NULL;

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
UniformBlock gUniformData;

UIApp              gAppUI;
GpuProfiler*       pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

struct ObjectProperty
{
	float mRotX = 0, mRotY = 0;
} gObjSettings;

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
GuiComponent* pGui = NULL;

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
        PathHandle programDirectory = fsCopyProgramDirectoryPath();
        if (!fsPlatformUsesBundledResources())
        {
            PathHandle resourceDirRoot = fsAppendPathComponent(programDirectory, "../../../src/02_Compute");
            fsSetResourceDirectoryRootPath(resourceDirRoot);
            
            fsSetRelativePathForResourceDirectory(RD_TEXTURES,        "../../UnitTestResources/Textures");
            fsSetRelativePathForResourceDirectory(RD_MESHES,             "../../UnitTestResources/Meshes");
            fsSetRelativePathForResourceDirectory(RD_BUILTIN_FONTS,     "../../UnitTestResources/Fonts");
            fsSetRelativePathForResourceDirectory(RD_ANIMATIONS,         "../../UnitTestResources/Animation");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_TEXT,     "../../../../Middleware_3/Text");
            fsSetRelativePathForResourceDirectory(RD_MIDDLEWARE_UI,     "../../../../Middleware_3/UI");
        }
        
		initNoise();

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
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

		if (!gVirtualJoystick.Init(pRenderer, "circlepad", RD_TEXTURES))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}

		ShaderLoadDesc displayShader = {};
		displayShader.mStages[0] = { "display.vert", NULL, 0, RD_SHADER_SOURCES };
		displayShader.mStages[1] = { "display.frag", NULL, 0, RD_SHADER_SOURCES };
		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0, RD_SHADER_SOURCES };

		addShader(pRenderer, &displayShader, &pShader);
		addShader(pRenderer, &computeShader, &pComputeShader);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRast);

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
			addResource(&ubDesc);
		}

		// Width and height needs to be same as Texture's
		gUniformData.mHeight = mSettings.mHeight;
		gUniformData.mWidth = mSettings.mWidth;

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", RD_BUILTIN_FONTS);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(140.0f, 320.0f);
		guiDesc.mStartPosition = vec2(mSettings.mWidth / dpiScale - guiDesc.mStartSize.getX() * 1.1f, guiDesc.mStartSize.getY() * 0.5f);

		pGui = gAppUI.AddGuiComponent("Micro profiler", &guiDesc);

		pGui->AddWidget(CheckboxWidget("Toggle Micro Profiler", &gMicroProfiler));

		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		if (!initInputSystem(pWindow))
			return false;

    // Initialize profile
    initProfiler();

    addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

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
			if (!gMicroProfiler && !gAppUI.IsFocused() && *ctx->pCaptured)
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

		// Exit profile
		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
			removeResource(pUniformBuffer[i]);
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeSampler(pRenderer, pSampler);
		removeRasterizerState(pRast);

		removeShader(pRenderer, pShader);
		removeShader(pRenderer, pComputeShader);
		removePipeline(pRenderer, pComputePipeline);
		removeDescriptorSet(pRenderer, pDescriptorSetTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetComputeTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pComputeRootSignature);

		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addJuliaFractalUAV())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;

		loadProfiler(&gAppUI, mSettings.mWidth, mSettings.mHeight);

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 0;
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pRasterizerState = pRast;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pVertexLayout = &vertexLayout;
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

		unloadProfiler();

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

    if (gMicroProfiler != bPrevToggleMicroProfiler)
    {
      toggleProfiler();
      bPrevToggleMicroProfiler = gMicroProfiler;
    }

    /************************************************************************/
    // Update GUI
    /************************************************************************/
    gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);
		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		BufferUpdateDesc cbvUpdate = { pUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&cbvUpdate);

		const uint32_t* pThreadGroupSize = pComputeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Compute Pass", true);

		// Compute Julia 4D
		cmdBindPipeline(cmd, pComputePipeline);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetUniforms);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetComputeTexture);

		uint32_t groupCountX = (pTextureComputeOutput->mDesc.mWidth + pThreadGroupSize[0] - 1) / pThreadGroupSize[0];
		uint32_t groupCountY = (pTextureComputeOutput->mDesc.mHeight + pThreadGroupSize[1] - 1) / pThreadGroupSize[1];
		cmdDispatch(cmd, groupCountX, groupCountY, pThreadGroupSize[2]);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pTextureComputeOutput, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Pass", true);
		// Draw computed results
		cmdBindPipeline(cmd, pPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture);

		cmdDraw(cmd, 3, 0);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		gTimer.GetUSec(true);

		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });

		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)
		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);
		gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

    gAppUI.Gui(pGui);

    cmdDrawProfiler();
		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		barriers[1] = { pTextureComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
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
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
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
		addResource(&textureDesc);

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
