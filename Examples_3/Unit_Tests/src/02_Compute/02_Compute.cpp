/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../Common_3/Renderer/IRenderer.h"
#include "../../Common_3/OS/Interfaces/IApp.h"
#include "../../Common_3/Renderer/GpuProfiler.h"
#include "../../Common_3/Renderer/ResourceLoader.h"

//Math
#include "../../Common_3/OS/Math/Noise.h"
#include "../../Common_3/OS/Math/MathTypes.h"

//ui
#include "../../Common_3/OS/Interfaces/IUIManager.h"

#include "../../Common_3/OS/Interfaces/IMemoryManager.h"

/// Camera Controller
#define GUI_CAMERACONTROLLER 1
#define FPS_CAMERACONTROLLER 2

#define USE_CAMERACONTROLLER FPS_CAMERACONTROLLER

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
	mat4 mProjectView;
	vec4 mDiffuseColor;
	vec4 mMu;
	float mEpsilon;
	float mZoom;

	int mWidth;
	int mHeight;
	int mRenderSoftShadows;
};

FileSystem gFileSystem;
LogManager gLogManager;
Timer gAccumTimer;
HiresTimer gTimer;

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#elif defined(_DURANGO)
#define RESOURCE_DIR "PCDX12"
#else
#error PLATFORM NOT SUPPORTED
#endif

#ifdef _DURANGO
// Durango load assets from 'Layout\Image\Loose'
const char* pszRoots[] =
{
	"Shaders/Binary/",									// FSR_BinShaders
	"Shaders/",											// FSR_SrcShaders
	"Shaders/Binary/",									// FSR_BinShaders_Common
	"Shaders/",											// FSR_SrcShaders_Common
	"Textures/",										// FSR_Textures
	"Meshes/",											// FSR_Meshes
	"Fonts/",											// FSR_Builtin_Fonts
	"",													// FSR_OtherFiles
};
#else
//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../../src/02_Compute/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/02_Compute/" RESOURCE_DIR "/",		// FSR_SrcShaders
	"",													// FSR_BinShaders_Common
	"",													// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",				// FSR_Textures
	"../../../UnitTestResources/Meshes/",				// FSR_Meshes
	"../../../UnitTestResources/Fonts/",				// FSR_Builtin_Fonts
	"",													// FSR_OtherFiles
};
#endif

const uint32_t		gImageCount = 3;

Renderer*			pRenderer = nullptr;
Buffer*				pUniformBuffer = nullptr;

Queue*				pGraphicsQueue = nullptr;
CmdPool*			pCmdPool = nullptr;
Cmd**				ppCmds = nullptr;
Sampler*			pSampler = nullptr;
RasterizerState*	pRast = nullptr;

Fence*				pRenderCompleteFences[gImageCount] = { nullptr };
Semaphore*			pImageAcquiredSemaphore = nullptr;
Semaphore*			pRenderCompleteSemaphores[gImageCount] = { nullptr };

SwapChain*			pSwapChain = nullptr;

Shader*				pShader = nullptr;
Pipeline*			pPipeline = nullptr;
RootSignature*		pRootSignature = nullptr;

Shader*				pComputeShader = nullptr;
Pipeline*			pComputePipeline = nullptr;
RootSignature*		pComputeRootSignature = nullptr;
Texture*			pTextureComputeOutput = nullptr;

#ifdef TARGET_IOS
Texture*            pVirtualJoystickTex = nullptr;
#endif

uint32_t			gFrameIndex = 0;
UniformBlock		gUniformData;

UIManager*			pUIManager = nullptr;
GpuProfiler*		pGpuProfiler = nullptr;
ICameraController*	pCameraController = nullptr;

struct ObjectProperty
{
  float rotX = 0, rotY = 0;
} gObjSettings;

class Compute : public IApp
{
public:
	bool Init()
	{
		initNoise();

		// window and renderer setup
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);

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
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		if (!Load())
			return false;
        
#ifdef TARGET_IOS
        // Add virtual joystick texture.
        TextureLoadDesc textureDesc = {};
        textureDesc.mRoot = FSRoot::FSR_Absolute;
        textureDesc.mUseMipmaps = false;
        textureDesc.pFilename = "circlepad.png";
        textureDesc.ppTexture = &pVirtualJoystickTex;
        addResource(&textureDesc, true);
#endif

		ShaderLoadDesc displayShader = {};
		displayShader.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		displayShader.mStages[1] = { "display.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc computeShader = {};
		computeShader.mStages[0] = { "compute.comp", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &displayShader, &pShader);
		addShader(pRenderer, &computeShader, &pComputeShader);

		addRasterizerState(&pRast, CULL_MODE_NONE);
		addSampler(pRenderer, &pSampler, FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST,
			ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE);

		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplers["uSampler0"] = pSampler;
		addRootSignature(pRenderer, 1, &pShader, &pRootSignature, &rootDesc);

		addRootSignature(pRenderer, 1, &pComputeShader, &pComputeRootSignature);

		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 0;
		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.pRasterizerState = pRast;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
		pipelineSettings.pVertexLayout = &vertexLayout;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShader;
		addPipeline(pRenderer, &pipelineSettings, &pPipeline);

		ComputePipelineDesc computePipelineDesc = { 0 };
		computePipelineDesc.pRootSignature = pComputeRootSignature;
		computePipelineDesc.pShaderProgram = pComputeShader;
		addComputePipeline(pRenderer, &computePipelineDesc, &pComputePipeline);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		ubDesc.ppBuffer = &pUniformBuffer;
		addResource(&ubDesc);

		// Width and height needs to be same as Texture's
		gUniformData.mHeight = mSettings.mHeight;
		gUniformData.mWidth = mSettings.mWidth;

#if !USE_CAMERACONTROLLER
		// initial camera properties
		gCameraProp.mCameraPitch = -0.785398163f;
		gCameraProp.mCamearYaw = 1.5f*0.785398163f;
		gCameraProp.mCameraPosition = Point3(48.0f, 48.0f, 20.0f);
		gCameraProp.mCameraForward = vec3(0.0f, 0.0f, -1.0f);
		gCameraProp.mCameraUp = vec3(0.0f, 1.0f, 0.0f);

		vec3 camRot(gCameraProp.mCameraPitch, gCameraProp.mCamearYaw, 0.0f);
		mat3 trans = mat3::rotationZYX(camRot);
		gCameraProp.mCameraDirection = trans* gCameraProp.mCameraForward;
		gCameraProp.mCameraRight = cross(gCameraProp.mCameraDirection, gCameraProp.mCameraUp);
		normalize(gCameraProp.mCameraRight);
#endif

		UISettings uiSettings = {};
		uiSettings.pDefaultFontName = "TitilliumText/TitilliumText-Bold.ttf";
		addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

#if USE_CAMERACONTROLLER
		CameraMotionParameters cmp{ 100.0f, 150.0f, 300.0f };
		vec3 camPos{ 48.0f, 48.0f, 20.0f };
		vec3 lookAt{ 0 };

#if USE_CAMERACONTROLLER == FPS_CAMERACONTROLLER
		pCameraController = createFpsCameraController(camPos, lookAt);
		requestMouseCapture(true);
#elif USE_CAMERACONTROLLER == GUI_CAMERACONTROLLER
		pCameraController = createGuiCameraController(camPos, lookAt);
#endif

		pCameraController->setMotionParameters(cmp);

#ifndef _DURANGO
		registerRawMouseMoveEvent(cameraMouseMove);
		registerMouseButtonEvent(cameraMouseButton);
		registerMouseWheelEvent(cameraMouseWheel);
#endif
        
#ifdef TARGET_IOS
        registerTouchEvent(onTouch);
        registerTouchMoveEvent(onTouchMove);
#endif

		registerKeyboardButtonEvent(onKeyPress);
		registerMouseWheelEvent(onMouseWheel);
		registerJoystickButtonEvent(onJoystick);
#endif

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount]);

#if USE_CAMERACONTROLLER
		destroyCameraController(pCameraController);
#endif

		removeUIManagerInterface(pRenderer, pUIManager);

		Unload();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        
#ifdef TARGET_IOS
        removeResource(pVirtualJoystickTex);
#endif

		removeResource(pUniformBuffer);
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeSampler(pRenderer, pSampler);
		removeRasterizerState(pRast);

		removeShader(pRenderer, pShader);
		removeShader(pRenderer, pComputeShader);
		removePipeline(pRenderer, pPipeline);
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
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.pQueue = pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mEnableVsync = false;
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		// Create empty texture for output of compute shader
		TextureLoadDesc textureDesc = {};
		TextureDesc desc = {};
		desc.mType = TEXTURE_TYPE_2D;
		desc.mWidth = mSettings.mWidth;
		desc.mHeight = mSettings.mHeight;
		desc.mDepth = 1;
		desc.mArraySize = 1;
		desc.mMipLevels = 1;
		desc.mFormat = ImageFormat::RGBA8;
		desc.mUsage = (TextureUsage)(TEXTURE_USAGE_SAMPLED_IMAGE | TEXTURE_USAGE_UNORDERED_ACCESS);
		desc.mSampleCount = SAMPLE_COUNT_1;
		desc.mHostVisible = false;
		textureDesc.pDesc = &desc;
		textureDesc.ppTexture = &pTextureComputeOutput;
		addResource(&textureDesc);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount]);
		removeResource(pTextureComputeOutput);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		const float autoModeTimeoutReset = 3.0f;
		static float autoModeTimeout = 0.0f;

#if USE_CAMERACONTROLLER != FPS_CAMERACONTROLLER && !defined(TARGET_IOS)
		if (getKeyDown(KEY_W) || getKeyDown(KEY_UP))
		{
			gObjSettings.rotX += gCameraYRotateScale;
			autoModeTimeout = autoModeTimeoutReset;
		}
		if (getKeyDown(KEY_A) || getKeyDown(KEY_LEFT))
		{
			gObjSettings.rotY -= gCameraYRotateScale;
			autoModeTimeout = autoModeTimeoutReset;
		}
		if (getKeyDown(KEY_S) || getKeyDown(KEY_DOWN))
		{
			gObjSettings.rotX -= gCameraYRotateScale;
			autoModeTimeout = autoModeTimeoutReset;
		}
		if (getKeyDown(KEY_D) || getKeyDown(KEY_RIGHT))
		{
			gObjSettings.rotY += gCameraYRotateScale;
			autoModeTimeout = autoModeTimeoutReset;
		}
#endif // USE_CAMERACONTROLLER != FPS_CAMERACONTROLLER

#ifndef TARGET_IOS
#ifdef _DURANGO
		if (getJoystickButtonDown(BUTTON_A))
#else
		if (getKeyDown(KEY_F))
#endif
		{
			RecenterCameraView(85.0f);
		}
#endif
		pCameraController->update(deltaTime);

		const float k_wrapAround = (float)(M_PI * 2.0);
		if (gObjSettings.rotX > k_wrapAround)
			gObjSettings.rotX -= k_wrapAround;
		if (gObjSettings.rotX < -k_wrapAround)
			gObjSettings.rotX += k_wrapAround;
		if (gObjSettings.rotY > k_wrapAround)
			gObjSettings.rotY -= k_wrapAround;
		if (gObjSettings.rotY < -k_wrapAround)
			gObjSettings.rotY += k_wrapAround;

		UpdateMu(deltaTime, &gMuT, gMuA, gMuB);
		Interpolate(gMuC, gMuT, gMuA, gMuB);
		UpdateColor(deltaTime, &gColorT, gColorA, gColorB);
		Interpolate(gColorC, gColorT, gColorA, gColorB);
		/************************************************************************/
		// Compute matrices
		/************************************************************************/
#if USE_CAMERACONTROLLER
		mat4 rotMat = mat4::rotationX(gObjSettings.rotX) * mat4::rotationY(gObjSettings.rotY);
		mat4 viewMat = pCameraController->getViewMatrix();
#else
		mat4 rotMat = mat4::rotationX(gObjSettings.rotX) * mat4::rotationY(gObjSettings.rotY);
		mat4 viewMat = mat4::lookAt(gCameraProp.mCameraPosition, Point3(gCameraProp.mCameraPosition + gCameraProp.mCameraDirection), gCameraProp.mCameraUp);
#endif
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
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
		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 0.0f, 0.0f, 0.0f, 0.0f };

		Cmd* cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		BufferUpdateDesc cbvUpdate = { pUniformBuffer, &gUniformData };
		updateResource(&cbvUpdate);

		const uint32_t* pThreadGroupSize = pComputeShader->mNumThreadsPerGroup;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Compute Pass");

		// Compute Julia 4D
		cmdBindPipeline(cmd, pComputePipeline);

		DescriptorData params[2] = {};
		params[0].pName = "uniformBlock";
		params[0].ppBuffers = &pUniformBuffer;
		params[1].pName = "outputTexture";
		params[1].ppTextures = &pTextureComputeOutput;
		cmdBindDescriptors(cmd, pComputeRootSignature, 2, params);

		cmdDispatch(cmd, pTextureComputeOutput->mDesc.mWidth / pThreadGroupSize[0], pTextureComputeOutput->mDesc.mHeight / pThreadGroupSize[1], pThreadGroupSize[2]);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pTextureComputeOutput, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		cmdBeginRender(cmd, 1, &pRenderTarget, NULL, &loadActions);
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

		static HiresTimer timer;
		cmdUIBeginRender(cmd, pUIManager, 1, &pRenderTarget, NULL);
        
#ifdef TARGET_IOS
        // Draw the camera controller's virtual joysticks.
        float extSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickExternalRadius();
        float intSide = min(mSettings.mHeight, mSettings.mWidth) * pCameraController->getVirtualJoystickInternalRadius();
        
        vec2 joystickSize = vec2(extSide);
        vec2 leftJoystickCenter = pCameraController->getVirtualLeftJoystickCenter();
        vec2 leftJoystickPos = vec2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
        cmdUIDrawTexturedQuad(cmd, pUIManager, leftJoystickPos, joystickSize, pVirtualJoystickTex);
        vec2 rightJoystickCenter = pCameraController->getVirtualRightJoystickCenter();
        vec2 rightJoystickPos = vec2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
        cmdUIDrawTexturedQuad(cmd, pUIManager, rightJoystickPos, joystickSize, pVirtualJoystickTex);
        
        joystickSize = vec2(intSide);
        leftJoystickCenter = pCameraController->getVirtualLeftJoystickPos();
        leftJoystickPos = vec2(leftJoystickCenter.getX() * mSettings.mWidth, leftJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
        cmdUIDrawTexturedQuad(cmd, pUIManager, leftJoystickPos, joystickSize, pVirtualJoystickTex);
        rightJoystickCenter = pCameraController->getVirtualRightJoystickPos();
        rightJoystickPos = vec2(rightJoystickCenter.getX() * mSettings.mWidth, rightJoystickCenter.getY() * mSettings.mHeight) - 0.5f * joystickSize;
        cmdUIDrawTexturedQuad(cmd, pUIManager, rightJoystickPos, joystickSize, pVirtualJoystickTex);
#endif

		cmdUIDrawFrameTime(cmd, pUIManager, { 8, 15 }, "CPU ", timer.GetUSec(true) / 1000.0f);

#ifndef METAL // Metal doesn't support GPU profilers
		cmdUIDrawFrameTime(cmd, pUIManager, { 8, 40 }, "GPU ", (float)pGpuProfiler->mCumulativeTime * 1000.0f);
#endif

		cmdUIDrawGpuProfileData(cmd, pUIManager, { 8, 65 }, pGpuProfiler);
		cmdUIEndRender(cmd, pUIManager);

		cmdEndRender(cmd, 1, &pRenderTarget, NULL);

		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		barriers[1] = { pTextureComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, true);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);

		// Stall if CPU is running "Swap Chain Buffer Count - 1" frames ahead of GPU
		Fence* pNextFence = pRenderCompleteFences[(gFrameIndex + 1) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence);
	}

	String GetName()
	{
		return "02_Compute";
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
		} while (v[0] < 0 && v[1] < 0 && v[2] < 0); // prevent black colors
		v[3] = 1.0f;
	}

	void UpdateColor(float deltaTime, float t[4], float a[4], float b[4])
	{
		*t += gTimeScale *deltaTime;

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

	/// Camera controller functionality
#ifndef _DURANGO
	static bool cameraMouseMove(const RawMouseMoveEventData* data)
	{
		pCameraController->onMouseMove(data);
		return true;
	}

	static bool cameraMouseButton(const MouseButtonEventData* data)
	{
		pCameraController->onMouseButton(data);
		return true;
	}

	static bool cameraMouseWheel(const MouseWheelEventData* data)
	{
		pCameraController->onMouseWheel(data);
		return true;
	}
#endif

	static bool onKeyPress(const KeyboardButtonEventData* pData)
	{
#if !defined(_DURANGO) && !defined(TARGET_IOS)
		if (pData->pressed)
		{
			if (pData->key == KEY_SPACE)
				gRenderSoftShadows = !gRenderSoftShadows;
		}
#endif
		return true;
	}

	static bool onMouseWheel(const MouseWheelEventData* pData)
	{
		if (pData->scroll < 0)
			gZoom *= 1.1f;
		else
			gZoom /= 1.1f;

		return true;
	}

	static bool onJoystick(const JoystickButtonEventData* pData)
	{
		if (pData->pressed)
		{
			if (pData->button == BUTTON_B)
				gRenderSoftShadows = !gRenderSoftShadows;
		}
		return true;
	}
    
#ifdef TARGET_IOS
    static bool onTouch(const TouchEventData* data)
    {
        pCameraController->onTouch(data);
        return true;
    }
    
    static bool onTouchMove(const TouchEventData* data)
    {
        pCameraController->onTouchMove(data);
        return true;
    }
#endif
};

DEFINE_APPLICATION_MAIN(Compute)
