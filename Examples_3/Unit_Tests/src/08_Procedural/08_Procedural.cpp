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

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const char* pszBases[FSR_Count] = {
	"../../../src/08_Procedural/",          // FSR_BinShaders
	"../../../src/08_Procedural/",          // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/08_Procedural/",          // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

LogManager gLogManager;

// Have a uniform for camera data
struct UniformCamData
{
	mat4  mProjectView;
	vec3  mCamPos;
	float pad0;
};

// Have a uniform for object data
struct UniformObjData
{
	mat4 mWorldMat;
	mat4 mInvWorldMat;

	vec4 mOceanColor;
	vec4 mShorelineColor;
	vec4 mFoliageColor;
	vec4 mMountainsColor;
	vec4 mSnowColor;
	vec4 mPolarCapsColor;
	vec4 mAtmosphereColor;
	vec4 mHeightsInfo;    // x : Ocean, y : Shore, z : Snow, w : Polar
	vec4 mTimeInfo;
};

struct ScreenSize
{
	vec4 mScreenSize;
};

struct Light
{
	vec4  mPos;
	vec4  mCol;
	float mRadius;
	float mIntensity;
};

struct UniformLightData
{
	// Used to tell our shaders how many lights are currently present
	int   mCurrAmountOfLights = 0;
	int   pad0;
	int   pad1;
	int   pad2;
	Light mLights[16];    // array of lights seem to be broken so just a single light for now
};

const uint32_t gImageCount = 3;
bool           gToggleVSync = false;
Texture*       pEnvTex = NULL;
Sampler*       pSamplerEnv = NULL;

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

Renderer* pRenderer = NULL;

UIApp         gAppUI;
GuiComponent* pGui;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

CmdPool* pUICmdPool = NULL;
Cmd**    ppUICmds = NULL;

SwapChain* pSwapChain = NULL;

RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader*        pShaderBRDF = NULL;
Pipeline*      pPipelineBRDF = NULL;
RootSignature* pRootSigBRDF = NULL;

Shader*        pShaderBG = NULL;
Pipeline*      pPipelineBG = NULL;
RootSignature* pRootSigBG = NULL;

UniformObjData gUniformDataMVP;
ScreenSize     gScreenSizeData;

Buffer*        pBufferUniformCamera[gImageCount];
UniformCamData gUniformDataCamera;

Buffer*          pBufferUniformLights[gImageCount];
UniformLightData gUniformDataLights;

Shader*   pShaderPostProc = NULL;
Pipeline* pPipelinePostProc = NULL;

DepthState*      pDepth = NULL;
RasterizerState* pRasterstateDefault = NULL;

// Vertex buffers
Buffer* pSphereVertexBuffer = NULL;
Buffer* pBGVertexBuffer = NULL;

uint32_t gFrameIndex = 0;

GpuProfiler*       pGpuProfiler = NULL;
ICameraController* pCameraController = NULL;

#ifndef TARGET_IOS
const int gSphereResolution = 1024;    // Increase for higher resolution spheres
#else
const int gSphereResolution = 512;    // Halve the resolution of the planet on iOS.
#endif
const char* pEnvImageFileNames[] = { "environment_sky.png" };

int gNumOfSpherePoints;

static float gEplasedTime = 0.0f;

static float3 gSunDir = float3(-1.0f, 1.0f, 1.0f);

static float gOceanHeight = 1.0f;
static float gShoreHeight = 0.02f;
static float gSnowHeight = 1.1f;
static float gPolarCapsAttitude = 1.1f;
static float gTerrainExp = 0.35f;
static float gTerrainSeed = 0.0f;

float gBgVertex[256];

tinystl::vector<Buffer*> gSphereBuffers[gImageCount];
Buffer*                  pScreenSizeBuffer;

float gCameraYRotateScale;    // decide how fast camera rotate

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class Procedural: public IApp
{
	public:
	bool Init()
	{
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

		addCmdPool(pRenderer, pGraphicsQueue, false, &pUICmdPool);
		addCmd_n(pUICmdPool, false, gImageCount, &ppUICmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		TextureLoadDesc textureDesc = {};
		textureDesc.mRoot = FSR_Textures;
		textureDesc.mUseMipmaps = true;
		textureDesc.pFilename = pEnvImageFileNames[0];
		textureDesc.ppTexture = &pEnvTex;
		addResource(&textureDesc);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		ShaderLoadDesc proceduralPlanet = {};
		proceduralPlanet.mStages[0] = { "proceduralPlanet.vert", NULL, 0, FSR_SrcShaders };
		proceduralPlanet.mStages[1] = { "proceduralPlanet.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc bgStars = {};
		bgStars.mStages[0] = { "backGround.vert", NULL, 0, FSR_SrcShaders };
		bgStars.mStages[1] = { "backGround.frag", NULL, 0, FSR_SrcShaders };

		SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &samplerDesc, &pSamplerEnv);

		addShader(pRenderer, &bgStars, &pShaderBG);
		addShader(pRenderer, &proceduralPlanet, &pShaderBRDF);

		const char*       pStaticSamplerName[] = { "uSampler0" };
		RootSignatureDesc bgRootDesc = { &pShaderBG, 1 };
		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerName;
		brdfRootDesc.ppStaticSamplers = &pSamplerEnv;

		addRootSignature(pRenderer, &bgRootDesc, &pRootSigBG);
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSigBRDF);

		// Create depth state and rasterizer state
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterstateDefault);

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepth);

		float* pSPherePoints;
		generateSpherePoints(&pSPherePoints, &gNumOfSpherePoints, gSphereResolution);

		uint64_t sphereDataSize = gNumOfSpherePoints * sizeof(float);

		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mSize = sphereDataSize;
		sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;    // 3 for vertex, 3 for normal
		sphereVbDesc.pData = pSPherePoints;
		sphereVbDesc.ppBuffer = &pSphereVertexBuffer;
		addResource(&sphereVbDesc);

		conf_free(pSPherePoints);

		uint64_t bgDataSize = 6 * sizeof(float) * 6;

		BufferLoadDesc bgVbDesc = {};
		bgVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		bgVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		bgVbDesc.mDesc.mSize = bgDataSize;
		bgVbDesc.mDesc.mVertexStride = sizeof(float) * 6;    // 3 for vertex, 3 for normal
		bgVbDesc.pData = gBgVertex;
		bgVbDesc.ppBuffer = &pBGVertexBuffer;
		addResource(&bgVbDesc);

		// Create a screenSize uniform buffer

		Buffer* screeSizeBuffer = NULL;

		BufferLoadDesc buffDesc = {};
		buffDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		buffDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		buffDesc.mDesc.mSize = sizeof(ScreenSize);
		buffDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		buffDesc.pData = NULL;
		buffDesc.ppBuffer = &screeSizeBuffer;
		addResource(&buffDesc);

		pScreenSizeBuffer = screeSizeBuffer;

		// Create a uniform buffer per obj
		BufferLoadDesc buffObjDesc = {};
		buffObjDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		buffObjDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		buffObjDesc.mDesc.mSize = sizeof(UniformObjData);
		buffObjDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		buffObjDesc.pData = NULL;

		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			Buffer* tBuffer = NULL;
			buffObjDesc.ppBuffer = &tBuffer;
			addResource(&buffObjDesc);
			gSphereBuffers[frameIdx].push_back(tBuffer);
		}

		// Uniform buffer for camera data
		BufferLoadDesc ubCamDesc = {};
		ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubCamDesc.mDesc.mSize = sizeof(UniformCamData);
		ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubCamDesc.pData = NULL;
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			ubCamDesc.ppBuffer = &pBufferUniformCamera[frameIdx];
			addResource(&ubCamDesc);
		}

		// Uniform buffer for light data
		BufferLoadDesc ubLightsDesc = {};
		ubLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
		ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubLightsDesc.pData = NULL;
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			ubLightsDesc.ppBuffer = &pBufferUniformLights[frameIdx];
			addResource(&ubLightsDesc);
		}

		finishResourceLoading();

		// prepare resources

		gUniformDataMVP.mAtmosphereColor = vec4(0.251f, 0.3451f, 0.6745f, 0.0f);
		gUniformDataMVP.mFoliageColor = vec4(0.043f, 0.560784f, 0.043f, 0.0f);
		gUniformDataMVP.mMountainsColor = vec4(0.243f, 0.13725f, 0.0118f, 0.0f);
		gUniformDataMVP.mOceanColor = vec4(0.15f, 0.596f, 0.9098f, 0.0f);
		gUniformDataMVP.mPolarCapsColor = vec4(0.60784f, 0.83921f, 0.92549f, 0.0f);
		gUniformDataMVP.mShorelineColor = vec4(0.91372f, 0.784313f, 0.56078f, 0.0f);
		gUniformDataMVP.mSnowColor = vec4(1.0f, 1.0f, 1.0f, 0.0f);

		// Update the uniform buffer for the objects
		mat4 modelmat = mat4::translation(vec3(0.0f, 0.0f, 0.0f));
		gUniformDataMVP.mWorldMat = modelmat;

		// Add light to scene
		Light light;
		light.mCol = vec4(1.0f, 1.0f, 1.0f, 0.0f);
		light.mPos = vec4(normalize(vec3(gSunDir.x, gSunDir.y, gSunDir.z)) * 1000.0f, 0.0f);
		light.mRadius = 10.0f;
		light.mIntensity = 40.0f;

		gUniformDataLights.mLights[0] = light;

		gUniformDataLights.mCurrAmountOfLights = 1;

		// Create UI
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartSize = vec2(200.0f, 360.0f) / dpiScale;
		guiDesc.mStartPosition = vec2(5.f, 50.f) / dpiScale;
		pGui = gAppUI.AddGuiComponent(GetName(), &guiDesc);

#if !defined(TARGET_IOS) && !defined(_DURANGO)
		pGui->AddWidget(CheckboxWidget("Toggle VSync", &gToggleVSync));
#endif

		pGui->AddWidget(SliderFloat3Widget("Sunlight", &gSunDir, float3(-1.0f), float3(1.0f)));

		pGui->AddWidget(SliderFloatWidget("Ocean Level : ", &gOceanHeight, 0.0f, 1.5f, 0.01f));
		pGui->AddWidget(SliderFloatWidget("Shoreline Height : ", &gShoreHeight, 0.0f, 0.04f, 0.01f));
		pGui->AddWidget(SliderFloatWidget("Snowy Land Height : ", &gSnowHeight, 0.0f, 2.00f, 0.01f));
		pGui->AddWidget(SliderFloatWidget("PolarCaps Attitude : ", &gPolarCapsAttitude, 0.0f, 3.0f, 0.01f));
		pGui->AddWidget(SliderFloatWidget("Terrain Exp : ", &gTerrainExp, 0.0f, 1.0f, 0.01f));
		pGui->AddWidget(SliderFloatWidget("Terrain Seed : ", &gTerrainSeed, 0.0f, 100.0f, 1.0f));

		CameraMotionParameters camParameters{ 10.0f, 600.0f, 200.0f };
		vec3                   camPos{ 0.0f, 0.0f, 10.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);

		pCameraController->setMotionParameters(camParameters);
		InputSystem::RegisterInputEvent(cameraInputEvent);

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);
		destroyCameraController(pCameraController);

		removeDebugRendererInterface();
		removeResource(pSphereVertexBuffer);
		removeResource(pBGVertexBuffer);
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			removeResource(pBufferUniformCamera[frameIdx]);
			removeResource(pBufferUniformLights[frameIdx]);

			for (uint32_t i = 0; i < (uint32_t)gSphereBuffers[frameIdx].size(); ++i)
			{
				removeResource(gSphereBuffers[frameIdx][i]);
			}
		}

		removeResource(pScreenSizeBuffer);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		// Remove commands and command pool&
		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
		removeCmdPool(pRenderer, pUICmdPool);

		removeDepthState(pDepth);
		removeResource(pEnvTex);

		removeRasterizerState(pRasterstateDefault);

		removeSampler(pRenderer, pSamplerEnv);

		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pShaderBG);

		removeRootSignature(pRenderer, pRootSigBRDF);
		removeRootSignature(pRenderer, pRootSigBG);

#ifndef METAL
		removeGpuProfiler(pRenderer, pGpuProfiler);
#endif

		// Remove resource loader and renderer
		removeResourceLoaderInterface(pRenderer);

		removeQueue(pGraphicsQueue);

		removeRenderer(pRenderer);
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!addDepthBuffer())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif

		// Create vertex layout
		VertexLayout vertexLayoutSphere = {};
		vertexLayoutSphere.mAttribCount = 2;

		vertexLayoutSphere.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSphere.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[0].mBinding = 0;
		vertexLayoutSphere.mAttribs[0].mLocation = 0;
		vertexLayoutSphere.mAttribs[0].mOffset = 0;

		vertexLayoutSphere.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutSphere.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutSphere.mAttribs[1].mBinding = 0;
		vertexLayoutSphere.mAttribs[1].mLocation = 1;
		vertexLayoutSphere.mAttribs[1].mOffset = 3 * sizeof(float);    // first attribute contains 3 floats

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepth;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSigBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &vertexLayoutSphere;
		pipelineSettings.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);

		// Create vertex layout
		VertexLayout vertexLayoutBG = {};
		vertexLayoutBG.mAttribCount = 2;

		vertexLayoutBG.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutBG.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutBG.mAttribs[0].mBinding = 0;
		vertexLayoutBG.mAttribs[0].mLocation = 0;
		vertexLayoutBG.mAttribs[0].mOffset = 0;

		vertexLayoutBG.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutBG.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutBG.mAttribs[1].mBinding = 0;
		vertexLayoutBG.mAttribs[1].mLocation = 1;
		vertexLayoutBG.mAttribs[1].mOffset = 3 * sizeof(float);    // first attribute contains 3 floats

		GraphicsPipelineDesc pipelineSettingsBG = { 0 };
		pipelineSettingsBG.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsBG.mRenderTargetCount = 1;
		pipelineSettingsBG.pDepthState = pDepth;
		pipelineSettingsBG.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettingsBG.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettingsBG.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsBG.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsBG.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettingsBG.pRootSignature = pRootSigBG;
		pipelineSettingsBG.pShaderProgram = pShaderBG;
		pipelineSettingsBG.pVertexLayout = &vertexLayoutBG;
		pipelineSettingsBG.pRasterizerState = pRasterstateDefault;
		addPipeline(pRenderer, &pipelineSettingsBG, &pPipelineBG);

#if defined(VULKAN)
		transitionRenderTargets();
#endif

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pPipelineBG);

		removeRenderTarget(pRenderer, pDepthBuffer);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
#if !defined(TARGET_IOS) && !defined(_DURANGO)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#endif

		gEplasedTime += deltaTime;

		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		// Update camera
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformDataCamera.mProjectView = projMat * viewMat * mat4::identity();
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		// Update screen buffer
		gScreenSizeData.mScreenSize.setX((float)(mSettings.mWidth));
		gScreenSizeData.mScreenSize.setY((float)(mSettings.mHeight));

		// Update object buffer
		float rotSpeed = 0.01f;
		gUniformDataMVP.mWorldMat = mat4::rotationY(gEplasedTime * rotSpeed);
		gUniformDataMVP.mInvWorldMat = mat4::rotationY(-gEplasedTime * rotSpeed);
		gUniformDataMVP.mHeightsInfo = vec4(gOceanHeight, gShoreHeight, gSnowHeight, gPolarCapsAttitude);
		gUniformDataMVP.mTimeInfo = vec4(gEplasedTime * 60.0f, 0.0f, gTerrainExp, gTerrainSeed * 39.0f);

		// Update light buffer
		gUniformDataLights.mLights[0].mPos = vec4(normalize(vec3(gSunDir.x, gSunDir.y, gSunDir.z)) * 1000.0f, 0.0);

		gAppUI.Update(deltaTime);
	}

	void Draw()
	{
		// This will acquire the next swapchain image
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);
		/************************************************************************/
		// Upload uniform data to GPU
		/************************************************************************/
		BufferUpdateDesc camBuffUpdateDesc = { pBufferUniformCamera[gFrameIndex], &gUniformDataCamera };
		updateResource(&camBuffUpdateDesc);

		BufferUpdateDesc bgBuffUpdateDesc = { pScreenSizeBuffer, &gScreenSizeData };
		updateResource(&bgBuffUpdateDesc);

		BufferUpdateDesc objBuffUpdateDesc = { gSphereBuffers[gFrameIndex][0], &gUniformDataMVP };
		updateResource(&objBuffUpdateDesc);

		BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights[gFrameIndex], &gUniformDataLights };
		updateResource(&lightBuffUpdateDesc);
		/************************************************************************/
		// Rendering
		/************************************************************************/
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { 0.2109f, 0.6470f, 0.8470f, 1.0f };    // Light blue cclear
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = { 1.0f, 0.0f };

		/************************************************************************/
		// Record commmand buffers
		/************************************************************************/
		tinystl::vector<Cmd*> allCmds;
		Cmd*                  cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);
#ifndef METAL
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);
#endif

		// Transfer our render target to a render target state
		TextureBarrier barriers[] = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, false);

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
		/************************************************************************/
		//Draw BG
		/************************************************************************/
#ifndef METAL
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw BG");
#endif

		cmdBindPipeline(cmd, pPipelineBG);

		DescriptorData paramsBG[2] = {};
		paramsBG[0].pName = "cbObject";
		paramsBG[0].ppBuffers = &gSphereBuffers[gFrameIndex][0];
		paramsBG[1].pName = "cbScreen";
		paramsBG[1].ppBuffers = &pScreenSizeBuffer;

		cmdBindDescriptors(cmd, pRootSigBG, 2, paramsBG);
		cmdBindVertexBuffer(cmd, 1, &pBGVertexBuffer, NULL);

		cmdDraw(cmd, 6, 0);
#ifndef METAL
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#endif
		/************************************************************************/
		//Draw Planet
		/************************************************************************/
#ifndef METAL
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Planet");
#endif

		cmdBindPipeline(cmd, pPipelineBRDF);

		// These params stays the same, we alternate our next param
		DescriptorData params[4] = {};
		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pBufferUniformCamera[gFrameIndex];

		params[1].pName = "cbObject";
		params[1].ppBuffers = &gSphereBuffers[gFrameIndex][0];

		params[2].pName = "cbLights";
		params[2].ppBuffers = &pBufferUniformLights[gFrameIndex];

		params[3].pName = "uEnvTex0";
		params[3].ppTextures = &pEnvTex;

		cmdBindDescriptors(cmd, pRootSigBRDF, 4, params);
		cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer, NULL);

		cmdDrawInstanced(cmd, gNumOfSpherePoints / 6, 0, 1, 0);
#ifndef METAL
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#endif
		/************************************************************************/
		/************************************************************************/
#ifndef METAL
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
#endif
		endCmd(cmd);
		allCmds.push_back(cmd);

		cmd = ppUICmds[gFrameIndex];
		beginCmd(cmd);

		// Prepare UI command buffers

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#ifndef METAL
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugGpuProfile(cmd, 8, 65, pGpuProfiler, NULL);
#endif

		gAppUI.Gui(pGui);
		gAppUI.Draw(cmd);

		// Transition our texture to present state
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);

		endCmd(cmd);
		allCmds.push_back(cmd);

		queueSubmit(
			pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
			&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "08_Procedural"; }

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

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = { 1.0f, 0 };
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

#if defined(VULKAN)
	void transitionRenderTargets()
	{
		// Transition render targets to desired state
		const uint32_t numBarriers = gImageCount + 1;
		TextureBarrier rtBarriers[numBarriers] = {};
		for (uint32_t i = 0; i < gImageCount; ++i)
			rtBarriers[i] = { pSwapChain->ppSwapchainRenderTargets[i]->pTexture, RESOURCE_STATE_RENDER_TARGET };
		rtBarriers[numBarriers - 1] = { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE };
		beginCmd(ppCmds[0]);
		cmdResourceBarrier(ppCmds[0], 0, 0, numBarriers, rtBarriers, false);
		endCmd(ppCmds[0]);
		queueSubmit(pGraphicsQueue, 1, &ppCmds[0], pRenderCompleteFences[0], 0, NULL, 0, NULL);
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0], false);
	}
#endif

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

	// Camera controller functionality
	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(Procedural)
