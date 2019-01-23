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
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

// Julia4D Parameters
#define SIZE_X 412
#define SIZE_Y 256
#define SIZE_Z 1024

#if defined(TARGET_IOS) || defined(__ANDROID__)
#define MOBILE_PLATFORM
#endif

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

FileSystem gFileSystem;
LogManager gLogManager;
Timer      gAccumTimer;
HiresTimer gTimer;

const char* pszBases[FSR_Count] = {
	"../../../src/02_Compute/",             // FSR_BinShaders
	"../../../src/02_Compute/",             // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/02_Compute/",             // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

const uint32_t gImageCount = 3;

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

Shader*        pShader = NULL;
Pipeline*      pPipeline = NULL;
RootSignature* pRootSignature = NULL;

Shader*        pComputeShader = NULL;
Pipeline*      pComputePipeline = NULL;
RootSignature* pComputeRootSignature = NULL;
Texture*       pTextureComputeOutput = NULL;

#if defined(MOBILE_PLATFORM)
VirtualJoystickUI gVirtualJoystick;
#endif

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

class Compute: public IApp
{
	public:
	bool Init()
	{
		initNoise();

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

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

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

#if defined(MOBILE_PLATFORM)
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		ShaderLoadDesc displayShader = {};
		displayShader.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		displayShader.mStages[1] = { "display.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0, FSR_SrcShaders };

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

		ComputePipelineDesc computePipelineDesc = { 0 };
		computePipelineDesc.pRootSignature = pComputeRootSignature;
		computePipelineDesc.pShaderProgram = pComputeShader;
		addComputePipeline(pRenderer, &computePipelineDesc, &pComputePipeline);

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

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ 48.0f, 48.0f, 20.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);
		InputSystem::RegisterInputEvent(cameraInputEvent);

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

		destroyCameraController(pCameraController);

		// Destroy debug renderer interface
		removeDebugRendererInterface();

#if defined(MOBILE_PLATFORM)
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

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

#if defined(MOBILE_PLATFORM)
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], 0))
			return false;
#endif

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 0;
		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pRasterizerState = pRast;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShader;
		addPipeline(pRenderer, &pipelineSettings, &pPipeline);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

#if defined(MOBILE_PLATFORM)
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipeline);

		removeResource(pTextureComputeOutput);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(85.0f);
		}
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
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

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

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Compute Pass");

		// Compute Julia 4D
		cmdBindPipeline(cmd, pComputePipeline);

		DescriptorData params[2] = {};
		params[0].pName = "uniformBlock";
		params[0].ppBuffers = &pUniformBuffer[gFrameIndex];
		params[1].pName = "outputTexture";
		params[1].ppTextures = &pTextureComputeOutput;
		cmdBindDescriptors(cmd, pComputeRootSignature, 2, params);

		uint32_t groupCountX = (pTextureComputeOutput->mDesc.mWidth + pThreadGroupSize[0] - 1) / pThreadGroupSize[0];
		uint32_t groupCountY = (pTextureComputeOutput->mDesc.mHeight + pThreadGroupSize[1] - 1) / pThreadGroupSize[1];
		cmdDispatch(cmd, groupCountX, groupCountY, pThreadGroupSize[2]);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pTextureComputeOutput, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Pass");
		// Draw computed results
		cmdBindPipeline(cmd, pPipeline);
		params[0].pName = "uTex0";
		params[0].ppTextures = &pTextureComputeOutput;
		cmdBindDescriptors(cmd, pRootSignature, 1, params);

		cmdDraw(cmd, 3, 0);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		gTimer.GetUSec(true);

#if defined(MOBILE_PLATFORM)
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#if !defined(METAL) && !defined(__ANDROID__)    // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugGpuProfile(cmd, 8, 65, pGpuProfiler, NULL);
#endif

		gAppUI.Draw(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		barriers[1] = { pTextureComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "02_Compute"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
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
		desc.mFormat = ImageFormat::RGBA8;
		desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mHostVisible = false;
		textureDesc.pDesc = &desc;
		textureDesc.ppTexture = &pTextureComputeOutput;
		addResource(&textureDesc);

		return pTextureComputeOutput != NULL;
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

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(Compute)
