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

#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"

#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITime.h"
#include "../../../../Common_3/OS/Interfaces/IProfiler.h"
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemory.h"    // Must be last include in cpp file

struct UniformBlock
{
	vec4 res;
	mat4 invView;
};

const uint32_t gImageCount = 3;
bool           bToggleMicroProfiler = false;
bool           bPrevToggleMicroProfiler = false;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*           pRTDemoShader = NULL;
Pipeline*         pRTDemoPipeline = NULL;
RootSignature*    pRootSignature = NULL;
DescriptorBinder* pDescriptorBinder = NULL;
#if defined(TARGET_IOS) || defined(__ANDROID__)
VirtualJoystickUI gVirtualJoystick;
#endif
RasterizerState* pRast = NULL;

Buffer* pUniformBuffer[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;

UniformBlock gUniformData;

ICameraController* pCameraController = NULL;

GuiComponent*			pGuiWindow;

GpuProfiler* pGpuProfiler = NULL;

/// UI
UIApp gAppUI;

const char* pszBases[FSR_Count] = {
	"../../../src/16a_SphereTracing/",       // FSR_BinShaders
	"../../../src/16a_SphereTracing/",       // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/16a_SphereTracing/",       // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_Audio
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff0080ff, 18);

class SphereTracing: public IApp
{
	public:
	SphereTracing()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init()
	{
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

		initResourceLoaderInterface(pRenderer);

		initProfiler(pRenderer);
		profileRegisterInput();

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler, "GpuProfiler");

#if defined(__ANDROID__) || defined(TARGET_IOS)
		if (!gVirtualJoystick.Init(pRenderer, "circlepad", FSR_Textures))
		{
			LOGF(LogLevel::eERROR, "Could not initialize Virtual Joystick.");
			return false;
		}
#endif

		ShaderLoadDesc rtDemoShader = {};
		rtDemoShader.mStages[0] = { "fstri.vert", NULL, 0, FSR_SrcShaders };
		rtDemoShader.mStages[1] = { "rt.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &rtDemoShader, &pRTDemoShader);

		Shader*           shaders[] = { pRTDemoShader };
		RootSignatureDesc rootDesc = {};
		rootDesc.mShaderCount = 1;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);

		DescriptorBinderDesc descriptorBinderDesc = { pRootSignature };
		addDescriptorBinder(pRenderer, 0, 1, &descriptorBinderDesc, &pDescriptorBinder);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRast);

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

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

    /************************************************************************/
    // GUI
    /************************************************************************/
    GuiDesc guiDesc = {};
    guiDesc.mStartSize = vec2(300.0f, 250.0f);
    guiDesc.mStartPosition = vec2(0.0f, guiDesc.mStartSize.getY());
    pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);

    pGuiWindow->AddWidget(CheckboxWidget("Toggle Micro Profiler", &bToggleMicroProfiler));

		CameraMotionParameters cmp{ 1.6f, 6.0f, 2.0f };
		vec3                   camPos{ 3.5, 1.0, 0.5 };
		vec3                   lookAt{ -0.5f, -0.4f, 0.5f };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif

		pCameraController->setMotionParameters(cmp);

		InputSystem::RegisterInputEvent(cameraInputEvent);
		return true;
	}

	void Exit()
	{
		waitQueueIdle(pGraphicsQueue);

		exitProfiler();

		destroyCameraController(pCameraController);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBuffer[i]);
		}

		removeShader(pRenderer, pRTDemoShader);
		removeRootSignature(pRenderer, pRootSignature);
		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeRasterizerState(pRast);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeGpuProfiler(pRenderer, pGpuProfiler);
		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#if defined(TARGET_IOS) || defined(__ANDROID__)
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0]))
			return false;
#endif
		loadProfiler(pSwapChain->ppSwapchainRenderTargets[0]);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pVertexLayout = NULL;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pRasterizerState = pRast;
		pipelineSettings.pShaderProgram = pRTDemoShader;
		addPipeline(pRenderer, &desc, &pRTDemoPipeline);

		return true;
	}

	void Unload()
	{
		waitQueueIdle(pGraphicsQueue);

		gAppUI.Unload();

		unloadProfiler();
#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Unload();
#endif

		removePipeline(pRenderer, pRTDemoPipeline);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
		{
			RecenterCameraView(5.0f, vec3(-0.5f, -0.4f, 0.5f));
		}

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		gUniformData.res = vec4((float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 0.0f);
		mat4 viewMat = pCameraController->getViewMatrix();
		gUniformData.invView = inverse(viewMat);

		/************************************************************************/
		/************************************************************************/

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

    gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence*      pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pNextFence);

		// Update uniform buffers
		BufferUpdateDesc shaderCbv = { pUniformBuffer[gFrameIndex], &gUniformData };
		updateResource(&shaderCbv);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 1.0f;
		loadActions.mClearColorValues[0].g = 1.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		//// draw skybox
		cmdBeginDebugMarker(cmd, 0, 0, 1, "Draw");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "RT Shader");
		cmdBindPipeline(cmd, pRTDemoPipeline);

		DescriptorData params[1] = {};
		params[0].pName = "u_input";
		params[0].ppBuffers = &pUniformBuffer[gFrameIndex];
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignature, 1, params);
		cmdDraw(cmd, 3, 0);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.DrawText(
			cmd, float2(8, 15), eastl::string().sprintf("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f).c_str(), &gFrameTimeDraw);

#if !defined(__ANDROID__)    // Metal doesn't support GPU profilers
		gAppUI.DrawText(
			cmd, float2(8, 40), eastl::string().sprintf("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f).c_str(),
			&gFrameTimeDraw);
		gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#endif

		cmdDrawProfiler(cmd);

    gAppUI.Gui(pGuiWindow);
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndDebugMarker(cmd);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
		flipProfiler();
	}

	const char* GetName() { return "16a_SphereTracing"; }

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

DEFINE_APPLICATION_MAIN(SphereTracing)
