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

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"

#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/OS/Image/Image.h"

// for cpu usage query
#if defined(_WIN32)
#if defined(_DURANGO)
#else
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppw.lib")
#endif
#elif defined(__linux__)
#include <unistd.h>    // sysconf(), _SC_NPROCESSORS_ONLN
#else
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"

#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

// startdust hash function, use this to generate all the seed and update the position of all particles
#define RND_GEN(x) (x = x * 196314165 + 907633515)

struct ParticleData
{
	float    mPaletteFactor;
	uint32_t mData;
	uint32_t mTextureIndex;
};

struct ThreadData
{
	CmdPool*      pCmdPool;
	Cmd**         ppCmds;
	RenderTarget* pRenderTarget;
	GpuProfiler*  pGpuProfiler;
	int           mStartPoint;
	int           mDrawCount;
	uint32_t      mFrameIndex;
};

struct ObjectProperty
{
	float mRotX = 0, mRotY = 0;
} gObjSettings;

const uint32_t gSampleCount = 60;
const uint32_t gImageCount = 3;

struct CpuGraphData
{
	int   mSampleIdx;
	float mSample[gSampleCount];
	float mSampley[gSampleCount];
	float mScale;
	int   mEmptyFlag;
};

struct ViewPortState
{
	float mOffsetX;
	float mOffsetY;
	float mWidth;
	float mHeight;
};

struct GraphVertex
{
	vec2 mPosition;
	vec4 mColor;
};

struct CpuGraph
{
	Buffer*       mVertexBuffer[gImageCount];    // vetex buffer for cpu sample
	ViewPortState mViewPort;                     //view port for different core
	GraphVertex   mPoints[gSampleCount * 3];
};

const int gTotalParticleCount = 2000000;
uint32_t  gGraphWidth = 200;
uint32_t  gGraphHeight = 100;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;
CmdPool* pGraphCmdPool = NULL;
Cmd**    ppGraphCmds = NULL;

Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;

Shader*        pShader = NULL;
Shader*        pSkyBoxDrawShader = NULL;
Shader*        pGraphShader = NULL;
Buffer*        pParticleVertexBuffer = NULL;
Buffer*        pProjViewUniformBuffer[gImageCount] = { NULL };
Buffer*        pSkyboxUniformBuffer[gImageCount] = { NULL };
Buffer*        pSkyBoxVertexBuffer = NULL;
Buffer*        pBackGroundVertexBuffer[gImageCount] = { NULL };
Pipeline*      pPipeline = NULL;
Pipeline*      pSkyBoxDrawPipeline = NULL;
Pipeline*      pGraphLinePipeline = NULL;
Pipeline*      pGraphLineListPipeline = NULL;
Pipeline*      pGraphTrianglePipeline = NULL;
RootSignature* pRootSignature = NULL;
RootSignature* pGraphRootSignature = NULL;
Texture*       pTextures[5];
Texture*       pSkyBoxTextures[6];
#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif
Sampler* pSampler = NULL;
Sampler* pSamplerSkyBox = NULL;
uint32_t gFrameIndex = 0;

#if defined(_WIN32)
#if defined(_DURANGO)
#else
IWbemServices* pService;
IWbemLocator*  pLocator;
uint64_t*      pOldTimeStamp;
uint64_t*      pOldPprocUsage;
#endif
#elif (__linux__)
uint64_t* pOldTimeStamp;
uint64_t* pOldPprocUsage;
#else
NSLock*                CPUUsageLock;
processor_info_array_t prevCpuInfo;
mach_msg_type_number_t numPrevCpuInfo;
#endif

uint   gCoresCount;
float* pCoresLoadData;

BlendState*      gParticleBlend;
RasterizerState* gSkyboxRast;

const uint gThreadCount = 3;

ThreadData   gThreadData[gThreadCount];
mat4         gProjectView;
mat4         gSkyboxProjectView;
ParticleData gParticleData;
uint32_t     gSeed;
float        gPaletteFactor;
uint         gTextureIndex;

GpuProfiler*       pGpuProfilers[gThreadCount] = { NULL };
UIApp              gAppUI;
ICameraController* pCameraController = NULL;

FileSystem gFileSystem;
ThreadPool gThreadSystem;
LogManager gLogManager;

GraphVertex   gBackGroundPoints[gImageCount][gSampleCount];
CpuGraphData* pCpuData;
CpuGraph*     pCpuGraph;

const char* pImageFileNames[] = { "Palette_Fire.png", "Palette_Purple.png", "Palette_Muted.png", "Palette_Rainbow.png", "Palette_Sky.png" };
const char* pSkyBoxImageFileNames[] = { "Skybox_right1.png",  "Skybox_left2.png",  "Skybox_top3.png",
										"Skybox_bottom4.png", "Skybox_front5.png", "Skybox_back6.png" };

const char* pszBases[FSR_Count] = {
	"../../../src/03_MultiThread/",         // FSR_BinShaders
	"../../../src/03_MultiThread/",         // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/03_MultiThread/",         // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

TextDrawDesc gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

class MultiThread: public IApp
{
	public:
	bool Init()
	{
		InitCpuUsage();

		gGraphWidth = mSettings.mWidth / 6;    //200;
		gGraphHeight = gCoresCount ? (mSettings.mHeight - 30 - gCoresCount * 10) / gCoresCount : 0;

		RendererDesc settings = { 0 };
		// settings.pLogFn = RendererLog;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pGraphCmdPool);
		addCmd_n(pGraphCmdPool, false, gImageCount, &ppGraphCmds);

		// initial needed datat for each thread
		for (int i = 0; i < gThreadCount; ++i)
		{
			// create cmd pools and and cmdbuffers for all thread
			addCmdPool(pRenderer, pGraphicsQueue, false, &gThreadData[i].pCmdPool);
			addCmd_n(gThreadData[i].pCmdPool, false, gImageCount, &gThreadData[i].ppCmds);

			// fill up the data for drawing point
			gThreadData[i].mStartPoint = i * (gTotalParticleCount / gThreadCount);
			gThreadData[i].mDrawCount = (gTotalParticleCount / gThreadCount);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		HiresTimer timer;
		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		// load all image to GPU
		for (int i = 0; i < 5; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pImageFileNames[i];
			textureDesc.ppTexture = &pTextures[i];
			addResource(&textureDesc, true);
		}

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSkyBoxImageFileNames[i];
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			addResource(&textureDesc, true);
		}

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		ShaderLoadDesc graphShader = {};
		graphShader.mStages[0] = { "Graph.vert", NULL, 0, FSR_SrcShaders };
		graphShader.mStages[1] = { "Graph.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc particleShader = {};
		particleShader.mStages[0] = { "Particle.vert", NULL, 0, FSR_SrcShaders };
		particleShader.mStages[1] = { "Particle.frag", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "Skybox.vert", NULL, 0, FSR_SrcShaders };
		skyShader.mStages[1] = { "Skybox.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &particleShader, &pShader);
		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &graphShader, &pGraphShader);

		SamplerDesc samplerDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
		SamplerDesc skyBoxSamplerDesc = { FILTER_LINEAR,
										  FILTER_LINEAR,
										  MIPMAP_MODE_NEAREST,
										  ADDRESS_MODE_CLAMP_TO_EDGE,
										  ADDRESS_MODE_CLAMP_TO_EDGE,
										  ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &samplerDesc, &pSampler);
		addSampler(pRenderer, &skyBoxSamplerDesc, &pSamplerSkyBox);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateDesc.mSrcFactors[0] = BC_ONE;
		blendStateDesc.mDstFactors[0] = BC_ONE;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &gParticleBlend);

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &gSkyboxRast);

		const char*       pStaticSamplerNames[] = { "uSampler0", "uSkyboxSampler" };
		Sampler*          pSamplers[] = { pSampler, pSamplerSkyBox };
		Shader*           shaders[] = { pShader, pSkyBoxDrawShader };
		RootSignatureDesc skyBoxRootDesc = {};
		skyBoxRootDesc.mStaticSamplerCount = 2;
		skyBoxRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		skyBoxRootDesc.ppStaticSamplers = pSamplers;
		skyBoxRootDesc.mShaderCount = 2;
		skyBoxRootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &skyBoxRootDesc, &pRootSignature);

		RootSignatureDesc graphRootDesc = {};
		graphRootDesc.mShaderCount = 1;
		graphRootDesc.ppShaders = &pGraphShader;
		addRootSignature(pRenderer, &graphRootDesc, &pGraphRootSignature);

		gTextureIndex = 0;

		//#ifdef _WIN32
		//	  SYSTEM_INFO sysinfo;
		//	  GetSystemInfo(&sysinfo);
		//	  gCPUCoreCount = sysinfo.dwNumberOfProcessors;
		//#elif defined(__APPLE__)
		//	  gCPUCoreCount = (unsigned int)[[NSProcessInfo processInfo] processorCount];
		//#endif

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
			10.0f,  -10.0f, -10.0f, 6.0f,    // -z
			-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
			-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

			-10.0f, -10.0f, 10.0f,  2.0f,    //-x
			-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
			-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

			10.0f,  -10.0f, -10.0f, 1.0f,    //+x
			10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
			10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

			-10.0f, -10.0f, 10.0f,  5.0f,    // +z
			-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
			10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

			-10.0f, 10.0f,  -10.0f, 3.0f,    //+y
			10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
			10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

			10.0f,  -10.0f, 10.0f,  4.0f,    //-y
			10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
			-10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
		};

		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
		addResource(&skyboxVbDesc);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(mat4);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc);
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc);
		}

		finishResourceLoading();
		LOGINFOF("Load Time %lld", timer.GetUSec(false) / 1000);

		// generate partcile data
		unsigned int particleSeed = 23232323;    //we have gseed as global declaration, pick a name that is not gseed
		for (int i = 0; i < 6 * 9; ++i)
		{
			RND_GEN(particleSeed);
		}
		uint32_t* seedArray = NULL;
		seedArray = (uint32_t*)conf_malloc(gTotalParticleCount * sizeof(uint32_t));
		for (int i = 0; i < gTotalParticleCount; ++i)
		{
			RND_GEN(particleSeed);
			seedArray[i] = particleSeed;
		}
		uint64_t parDataSize = sizeof(uint32_t) * (uint64_t)gTotalParticleCount;
		uint32_t parDataStride = sizeof(uint32_t);

		BufferLoadDesc particleVbDesc = {};
		particleVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		particleVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		particleVbDesc.mDesc.mSize = parDataSize;
		particleVbDesc.mDesc.mVertexStride = parDataStride;
		particleVbDesc.pData = seedArray;
		particleVbDesc.ppBuffer = &pParticleVertexBuffer;
		addResource(&particleVbDesc);

		conf_free(seedArray);

		uint32_t graphDataStride = sizeof(GraphVertex);                     // vec2(position) + vec4(color)
		uint32_t graphDataSize = sizeof(GraphVertex) * gSampleCount * 3;    // 2 vertex for tri, 1 vertex for line strip

		//generate vertex buffer for all cores to draw cpu graph and setting up view port for each graph
		pCpuGraph = (CpuGraph*)conf_malloc(sizeof(CpuGraph) * gCoresCount);
		for (uint i = 0; i < gCoresCount; ++i)
		{
			pCpuGraph[i].mViewPort.mOffsetX = mSettings.mWidth - 10.0f - gGraphWidth;
			pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
			pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
			pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;
			// create vertex buffer for each swapchain
			for (uint j = 0; j < gImageCount; ++j)
			{
				BufferLoadDesc vbDesc = {};
				vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
				vbDesc.mDesc.mSize = graphDataSize;
				vbDesc.mDesc.mVertexStride = graphDataStride;
				vbDesc.pData = NULL;
				vbDesc.ppBuffer = &pCpuGraph[i].mVertexBuffer[j];
				addResource(&vbDesc);
			}
		}
		graphDataSize = sizeof(GraphVertex) * gSampleCount;
		for (uint i = 0; i < gImageCount; ++i)
		{
			BufferLoadDesc vbDesc = {};
			vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
			vbDesc.mDesc.mSize = graphDataSize;
			vbDesc.mDesc.mVertexStride = graphDataStride;
			vbDesc.pData = NULL;
			vbDesc.ppBuffer = &pBackGroundVertexBuffer[i];
			addResource(&vbDesc);
		}

		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		gThreadSystem.CreateThreads(Thread::GetNumCPUCores() - 1);

		CameraMotionParameters cmp{ 100.0f, 800.0f, 1000.0f };
		vec3                   camPos{ 24.0f, 24.0f, 10.0f };
		vec3                   lookAt{ 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);
		InputSystem::RegisterInputEvent(cameraInputEvent);

		for (uint32_t i = 0; i < gThreadCount; ++i)
			addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfilers[i]);

		return true;
	}

	void Exit()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

		destroyCameraController(pCameraController);

		for (uint32_t i = 0; i < gThreadCount; ++i)
			removeGpuProfiler(pRenderer, pGpuProfilers[i]);

		removeDebugRendererInterface();

		gAppUI.Exit();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
			removeResource(pSkyboxUniformBuffer[i]);
		}
		removeResource(pParticleVertexBuffer);
		removeResource(pSkyBoxVertexBuffer);

		for (uint i = 0; i < gImageCount; ++i)
			removeResource(pBackGroundVertexBuffer[i]);

		for (uint i = 0; i < gCoresCount; ++i)
		{
			// remove all vertex buffer belongs to graph
			for (uint j = 0; j < gImageCount; ++j)
				removeResource(pCpuGraph[i].mVertexBuffer[j]);
		}

		conf_free(pCpuGraph);

		for (uint i = 0; i < 5; ++i)
			removeResource(pTextures[i]);
		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif

		removeSampler(pRenderer, pSampler);
		removeSampler(pRenderer, pSamplerSkyBox);

		removeShader(pRenderer, pShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeShader(pRenderer, pGraphShader);
		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pGraphRootSignature);

		removeBlendState(gParticleBlend);
		removeRasterizerState(gSkyboxRast);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeCmd_n(pGraphCmdPool, gImageCount, ppGraphCmds);
		removeCmdPool(pRenderer, pGraphCmdPool);

		for (int i = 0; i < gThreadCount; ++i)
		{
			removeCmd_n(gThreadData[i].pCmdPool, gImageCount, gThreadData[i].ppCmds);
			removeCmdPool(pRenderer, gThreadData[i].pCmdPool);
		}

		removeQueue(pGraphicsQueue);

		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);

		RemoveCpuUsage();
	}

	bool Load()
	{
		if (!addSwapChain())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif

		//vertexlayout and pipeline for particles
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::R32UI;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_POINT_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pBlendState = gParticleBlend;
		pipelineSettings.pRasterizerState = gSkyboxRast;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &pipelineSettings, &pPipeline);

		//layout and pipeline for skybox draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pRasterizerState = gSkyboxRast;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &pipelineSettings, &pSkyBoxDrawPipeline);

		/********** layout and pipeline for graph draw*****************/
		vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat =
			(sizeof(GraphVertex) > 24
				 ? ImageFormat::RGBA32F
				 : ImageFormat::RG32F);    // Handle the case when padding is added to the struct (yielding 32 bytes instead of 24) on macOS
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RGBA32F;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = calculateImageFormatStride(vertexLayout.mAttribs[0].mFormat);

		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_STRIP;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.pRootSignature = pGraphRootSignature;
		pipelineSettings.pShaderProgram = pGraphShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &pipelineSettings, &pGraphLinePipeline);

		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
		addPipeline(pRenderer, &pipelineSettings, &pGraphTrianglePipeline);

		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
		addPipeline(pRenderer, &pipelineSettings, &pGraphLineListPipeline);
		/********************************************************************/

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex % gImageCount], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipeline);
		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pGraphLineListPipeline);
		removePipeline(pRenderer, pGraphLinePipeline);
		removePipeline(pRenderer, pGraphTrianglePipeline);

		removeSwapChain(pRenderer, pSwapChain);
	}

	void Update(float deltaTime)
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		const float  autoModeTimeoutReset = 3.0f;
		static float autoModeTimeout = 0.0f;

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
		/************************************************************************/
		// Compute matrices
		/************************************************************************/
		// update camera with time
		mat4 modelMat = mat4::rotationX(gObjSettings.mRotX) * mat4::rotationY(gObjSettings.mRotY);
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 100.0f);
		gProjectView = projMat * viewMat * modelMat;
		// update particle position matrix

		viewMat.setTranslation(vec3(0));
		gSkyboxProjectView = projMat * viewMat;

		gPaletteFactor += deltaTime * 0.25f;
		if (gPaletteFactor > 1.0f)
		{
			for (int i = 0; i < 9; ++i)
			{
				RND_GEN(gSeed);
			}
			gPaletteFactor = 0.0f;

			gTextureIndex = (gTextureIndex + 1) % 5;

			//   gPaletteFactor = 1.0;
		}
		gParticleData.mPaletteFactor = gPaletteFactor * gPaletteFactor * (3.0f - 2.0f * gPaletteFactor);
		gParticleData.mData = gSeed;
		gParticleData.mTextureIndex = gTextureIndex;

		static float currentTime = 0.0f;
		currentTime += deltaTime;

		// update cpu data graph
		if (currentTime * 1000.0f > 500)
		{
			CalCpuUsage();
			for (uint i = 0; i < gCoresCount; ++i)
			{
				pCpuData[i].mSampley[pCpuData[i].mSampleIdx] = 0.0f;
				pCpuData[i].mSample[pCpuData[i].mSampleIdx] = pCoresLoadData[i] / 100.0f;
				pCpuData[i].mSampleIdx = (pCpuData[i].mSampleIdx + 1) % gSampleCount;
			}

			currentTime = 0.0f;
		}
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

		uint32_t frameIdx = gFrameIndex;
		/*******record command for drawing particles***************/
		WorkItem pWorkGroups[gThreadCount];

		for (int i = 0; i < gThreadCount; ++i)
		{
			gThreadData[i].pRenderTarget = pRenderTarget;
			gThreadData[i].mFrameIndex = frameIdx;
			gThreadData[i].pGpuProfiler = pGpuProfilers[i];
			pWorkGroups[i].pData = &gThreadData[i];
			pWorkGroups[i].pFunc = ParticleThreadDraw;
			gThreadSystem.AddWorkItem(&pWorkGroups[i]);
		}
		// simply record the screen cleaning command

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;

		Cmd* cmd = ppCmds[frameIdx];
		beginCmd(cmd);

		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex], &gProjectView, 0, 0, sizeof(gProjectView) };
		updateResource(&viewProjCbv);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex], &gSkyboxProjectView, 0, 0, sizeof(gSkyboxProjectView) };
		updateResource(&skyboxViewProjCbv);

		for (uint32_t i = 0; i < gCoresCount; ++i)
		{
			CpuGraphcmdUpdateBuffer(&pCpuData[i], &pCpuGraph[i], cmd, frameIdx);    // update vertex buffer for each cpugraph
			CpuGraphBackGroundUpdate(cmd, frameIdx);
		}

		flushResourceUpdates();

		TextureBarrier barrier = { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
		//// draw skybox

		DescriptorData params[7] = {};
		params[0].pName = "RightText";
		params[0].ppTextures = &pSkyBoxTextures[0];
		params[1].pName = "LeftText";
		params[1].ppTextures = &pSkyBoxTextures[1];
		params[2].pName = "TopText";
		params[2].ppTextures = &pSkyBoxTextures[2];
		params[3].pName = "BotText";
		params[3].ppTextures = &pSkyBoxTextures[3];
		params[4].pName = "FrontText";
		params[4].ppTextures = &pSkyBoxTextures[4];
		params[5].pName = "BackText";
		params[5].ppTextures = &pSkyBoxTextures[5];
		params[6].pName = "uniformBlock";
		params[6].ppBuffers = &pSkyboxUniformBuffer[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignature, 7, params);
		cmdBindPipeline(cmd, pSkyBoxDrawPipeline);

		cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, NULL);
		cmdDraw(cmd, 36, 0);

		static HiresTimer timer;
		timer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", timer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#if !defined(METAL)
		drawDebugText(cmd, 8, 65, "Particle CPU Times", NULL);
		for (uint32_t i = 0; i < gThreadCount; ++i)
		{
			drawDebugText(
				cmd, 8, 90.0f + i * 25.0f,
				tinystl::string::format("- Thread %u  %f ms", i, (float)pGpuProfilers[i]->mCumulativeCpuTime * 1000.0f), &gFrameTimeDraw);
		}

		drawDebugText(cmd, 8, 105 + gThreadCount * 25.0f, "Particle GPU Times", NULL);
		for (uint32_t i = 0; i < gThreadCount; ++i)
		{
			drawDebugText(
				cmd, 8, (130 + gThreadCount * 25.0f) + i * 25.0f,
				tinystl::string::format("- Thread %u  %f ms", i, (float)pGpuProfilers[i]->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		}
#endif

		gAppUI.Draw(cmd);
		endCmd(cmd);

		beginCmd(ppGraphCmds[frameIdx]);
		for (uint i = 0; i < gCoresCount; ++i)
		{
			gGraphWidth = pRenderTarget->mDesc.mWidth / 6;
			gGraphHeight = (pRenderTarget->mDesc.mHeight - 30 - gCoresCount * 10) / gCoresCount;
			pCpuGraph[i].mViewPort.mOffsetX = pRenderTarget->mDesc.mWidth - 10.0f - gGraphWidth;
			pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
			pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
			pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;

			cmdBindRenderTargets(ppGraphCmds[frameIdx], 1, &pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
			cmdSetViewport(
				ppGraphCmds[frameIdx], pCpuGraph[i].mViewPort.mOffsetX, pCpuGraph[i].mViewPort.mOffsetY, pCpuGraph[i].mViewPort.mWidth,
				pCpuGraph[i].mViewPort.mHeight, 0.0f, 1.0f);
			cmdSetScissor(ppGraphCmds[frameIdx], 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindDescriptors(ppGraphCmds[frameIdx], pGraphRootSignature, 0, NULL);

			cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
			cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx], NULL);
			cmdDraw(ppGraphCmds[frameIdx], 4, 0);

			cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLineListPipeline);
			cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx], NULL);
			cmdDraw(ppGraphCmds[frameIdx], 38, 4);

			cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
			cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &(pCpuGraph[i].mVertexBuffer[frameIdx]), NULL);
			cmdDraw(ppGraphCmds[frameIdx], 2 * gSampleCount, 0);

			cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLinePipeline);
			cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pCpuGraph[i].mVertexBuffer[frameIdx], NULL);
			cmdDraw(ppGraphCmds[frameIdx], gSampleCount, 2 * gSampleCount);
		}

		cmdBindRenderTargets(ppGraphCmds[frameIdx], 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		barrier = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(ppGraphCmds[frameIdx], 0, NULL, 1, &barrier, true);
		endCmd(ppGraphCmds[frameIdx]);
		// wait all particle threads done
		gThreadSystem.Complete(0);

		/***************draw cpu graph*****************************/
		/***************draw cpu graph*****************************/
		// gather all command buffer, it is important to keep the screen clean command at the beginning
		Cmd* allCmds[gThreadCount + 2];
		allCmds[0] = cmd;

		for (int i = 0; i < gThreadCount; ++i)
		{
			allCmds[i + 1] = gThreadData[i].ppCmds[frameIdx];
		}
		allCmds[gThreadCount + 1] = ppGraphCmds[frameIdx];
		// submit all command buffer

		queueSubmit(
			pGraphicsQueue, gThreadCount + 2, allCmds, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "03_MultiThread"; }

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
#if defined(__linux__)
	enum CPUStates{ S_USER = 0,    S_NICE, S_SYSTEM, S_IDLE, S_IOWAIT, S_IRQ, S_SOFTIRQ, S_STEAL, S_GUEST, S_GUEST_NICE,

					NUM_CPU_STATES };
	typedef struct CPUData
	{
		tinystl::string cpu;
		size_t          times[NUM_CPU_STATES];
	} CPUData;

	size_t GetIdleTime(const CPUData& e) { return e.times[S_IDLE] + e.times[S_IOWAIT]; }

	size_t GetActiveTime(const CPUData& e)
	{
		return e.times[S_USER] + e.times[S_NICE] + e.times[S_SYSTEM] + e.times[S_IRQ] + e.times[S_SOFTIRQ] + e.times[S_STEAL] +
			   e.times[S_GUEST] + e.times[S_GUEST_NICE];
	}
#endif

	void CalCpuUsage()
	{
#ifdef _WIN32
#if defined(_DURANGO)
#else
        HRESULT hr = NULL;
        ULONG   retVal;
        UINT    i;

        IWbemClassObject*     pclassObj;
        IEnumWbemClassObject* pEnumerator;

        hr = pService->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT TimeStamp_Sys100NS, PercentProcessorTime, Frequency_PerfTime FROM Win32_PerfRawData_PerfOS_Processor"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
        for (i = 0; i < gCoresCount; i++)
        {
            //Waiting for inifinite blocks resources and app.
            //Waiting for 15 ms (arbitrary) instead works much better
            hr = pEnumerator->Next(15, 1, &pclassObj, &retVal);
            if (!retVal)
            {
                break;
            }

            VARIANT vtPropTime;
            VARIANT vtPropClock;
            VariantInit(&vtPropTime);
            VariantInit(&vtPropClock);

            hr = pclassObj->Get(L"TimeStamp_Sys100NS", 0, &vtPropTime, 0, 0);
            UINT64 newTimeStamp = _wtoi64(vtPropTime.bstrVal);

            hr = pclassObj->Get(L"PercentProcessorTime", 0, &vtPropClock, 0, 0);
            UINT64 newPProcUsage = _wtoi64(vtPropClock.bstrVal);

            pCoresLoadData[i] =
                (float)(1.0 - (((double)newPProcUsage - (double)pOldPprocUsage[i]) / ((double)newTimeStamp - (double)pOldTimeStamp[i]))) *
                100.0f;

            if (pCoresLoadData[i] < 0)
                pCoresLoadData[i] = 0.0;
            else if (pCoresLoadData[i] > 100.0)
                pCoresLoadData[i] = 100.0;

            pOldPprocUsage[i] = newPProcUsage;
            pOldTimeStamp[i] = newTimeStamp;

            VariantClear(&vtPropTime);
            VariantClear(&vtPropClock);

            pclassObj->Release();
        }

        pEnumerator->Release();
#endif    //#if defined(_DURANGO)
#elif (__linux__)
		tinystl::vector<CPUData> entries;
		entries.reserve(gCoresCount);
		// Open cpu stat file
		File fileStat = {};
		fileStat.Open("/proc/stat", FM_ReadBinary, FSR_OtherFiles);

		FILE* statHandle = (FILE*)fileStat.GetHandle();
		if (statHandle)
		{
			// While eof not detected, keep parsing the stat file
			while (!feof(statHandle))
			{
				entries.emplace_back(CPUData());
				CPUData& entry = entries.back();
				char     dummyCpuName[256];    // dummy cpu name, not used.
				fscanf(
					statHandle, "%s %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu", &dummyCpuName[0], &entry.times[0], &entry.times[1],
					&entry.times[2], &entry.times[3], &entry.times[4], &entry.times[5], &entry.times[6], &entry.times[7], &entry.times[8],
					&entry.times[9]);
			}
			// Close the cpu stat file
			fileStat.Close();
		}

		for (uint32_t i = 0; i < gCoresCount; i++)
		{
			float ACTIVE_TIME = static_cast<float>(GetActiveTime(entries[i]));
			float IDLE_TIME = static_cast<float>(GetIdleTime(entries[i]));

			pCoresLoadData[i] = (ACTIVE_TIME - pOldPprocUsage[i]) / ((float)(IDLE_TIME + ACTIVE_TIME) - pOldTimeStamp[i]) * 100.0f;

			pOldPprocUsage[i] = ACTIVE_TIME;
			pOldTimeStamp[i] = IDLE_TIME + ACTIVE_TIME;
		}

#else
		processor_info_array_t cpuInfo;
		mach_msg_type_number_t numCpuInfo;

		natural_t     numCPUsU = 0U;
		kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);

		if (err == KERN_SUCCESS)
		{
			[CPUUsageLock lock];

			for (uint32_t i = 0; i < gCoresCount; i++)
			{
				float inUse, total;

				if (prevCpuInfo)
				{
					inUse =
						((cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER]) +
						 (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM]) +
						 (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE]));
					total = inUse + (cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE] - prevCpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE]);
				}
				else
				{
					inUse = cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_USER] + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_SYSTEM] +
							cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_NICE];
					total = inUse + cpuInfo[(CPU_STATE_MAX * i) + CPU_STATE_IDLE];
				}

				pCoresLoadData[i] = (float(inUse) / float(total)) * 100;

				if (pCoresLoadData[i] < 0)
					pCoresLoadData[i] = 0.0;
				else if (pCoresLoadData[i] > 100.0)
					pCoresLoadData[i] = 100.0;
			}

			[CPUUsageLock unlock];

			if (prevCpuInfo)
			{
				size_t prevCpuInfoSize = sizeof(integer_t) * numPrevCpuInfo;
				vm_deallocate(mach_task_self(), (vm_address_t)prevCpuInfo, prevCpuInfoSize);
			}

			prevCpuInfo = cpuInfo;
			numPrevCpuInfo = numCpuInfo;
		}

#endif
	}

	int InitCpuUsage()
	{
		gCoresCount = 0;
#ifdef _WIN32
#if defined(_DURANGO)
#else
        IWbemClassObject*     pclassObj;
        IEnumWbemClassObject* pEnumerator;
        HRESULT               hr;
        ULONG                 retVal;

        pService = NULL;
        pLocator = NULL;
        pOldTimeStamp = NULL;
        pOldPprocUsage = NULL;
        pCoresLoadData = NULL;

        CoInitializeEx(0, COINIT_MULTITHREADED);
        CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

        hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLocator);
        if (FAILED(hr))
        {
            return 0;
        }
        hr = pLocator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pService);
        if (FAILED(hr))
        {
            return 0;
        }

        CoSetProxyBlanket(
            pService, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

        hr = pService->ExecQuery(
            bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
            &pEnumerator);
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclassObj, &retVal);
        if (retVal)
        {
            VARIANT vtProp;
            VariantInit(&vtProp);
            hr = pclassObj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0);
            gCoresCount = vtProp.uintVal;
            VariantClear(&vtProp);
        }

        pclassObj->Release();
        pEnumerator->Release();

        if (gCoresCount)
        {
            pOldTimeStamp = (uint64_t*)conf_malloc(sizeof(uint64_t) * gCoresCount);
            pOldPprocUsage = (uint64_t*)conf_malloc(sizeof(uint64_t) * gCoresCount);
        }
#endif
#elif defined(__linux__)
		int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
		gCoresCount = numCPU;
		if (gCoresCount)
		{
			pOldTimeStamp = (uint64_t*)conf_malloc(sizeof(uint64_t) * gCoresCount);
			pOldPprocUsage = (uint64_t*)conf_malloc(sizeof(uint64_t) * gCoresCount);
		}
#elif defined(__APPLE__)
		processor_info_array_t cpuInfo;
		mach_msg_type_number_t numCpuInfo;

		natural_t     numCPUsU = 0U;
		kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);

		assert(err == KERN_SUCCESS);

		gCoresCount = numCPUsU;

		CPUUsageLock = [[NSLock alloc] init];
#endif

		pCpuData = (CpuGraphData*)conf_malloc(sizeof(CpuGraphData) * gCoresCount);
		for (uint i = 0; i < gCoresCount; ++i)
		{
			pCpuData[i].mSampleIdx = 0;
			pCpuData[i].mScale = 1.0f;
			for (uint j = 0; j < gSampleCount; ++j)
			{
				pCpuData[i].mSample[j] = 0.0f;
				pCpuData[i].mSampley[j] = 0.0f;
			}
		}

		if (gCoresCount)
		{
			pCoresLoadData = (float*)conf_malloc(sizeof(float) * gCoresCount);
			float zeroFloat = 0.0;
			memset(pCoresLoadData, *(int*)&zeroFloat, sizeof(float) * gCoresCount);
		}

		CalCpuUsage();
		return 1;
	}

	void RemoveCpuUsage()
	{
		conf_free(pCpuData);
#ifdef _WIN32
#if defined(_DURANGO)
#else
        conf_free(pOldTimeStamp);
        conf_free(pOldPprocUsage);
#endif
#endif
		conf_free(pCoresLoadData);
	}

	void CpuGraphBackGroundUpdate(Cmd* cmd, uint32_t frameIdx)
	{
		// background data
		gBackGroundPoints[frameIdx][0].mPosition = vec2(-1.0f, -1.0f);
		gBackGroundPoints[frameIdx][0].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		gBackGroundPoints[frameIdx][1].mPosition = vec2(1.0f, -1.0f);
		gBackGroundPoints[frameIdx][1].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		gBackGroundPoints[frameIdx][2].mPosition = vec2(-1.0f, 1.0f);
		gBackGroundPoints[frameIdx][2].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		gBackGroundPoints[frameIdx][3].mPosition = vec2(1.0f, 1.0f);
		gBackGroundPoints[frameIdx][3].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);

		const float woff = 2.0f / gGraphWidth;
		const float hoff = 2.0f / gGraphHeight;

		gBackGroundPoints[frameIdx][4].mPosition = vec2(-1.0f + woff, -1.0f + hoff);
		gBackGroundPoints[frameIdx][4].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][5].mPosition = vec2(1.0f - woff, -1.0f + hoff);
		gBackGroundPoints[frameIdx][5].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][6].mPosition = vec2(1.0f - woff, -1.0f + hoff);
		gBackGroundPoints[frameIdx][6].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][7].mPosition = vec2(1.0f - woff, 1.0f - hoff);
		gBackGroundPoints[frameIdx][7].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][8].mPosition = vec2(1.0f - woff, 1.0f - hoff);
		gBackGroundPoints[frameIdx][8].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][9].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
		gBackGroundPoints[frameIdx][9].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][10].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
		gBackGroundPoints[frameIdx][10].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		gBackGroundPoints[frameIdx][11].mPosition = vec2(-1.0f + woff, -1.0f + hoff);
		gBackGroundPoints[frameIdx][11].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);

		for (int i = 1; i <= 6; ++i)
		{
			gBackGroundPoints[frameIdx][12 + i * 2].mPosition =
				vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount), -1.0f);
			gBackGroundPoints[frameIdx][12 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
			gBackGroundPoints[frameIdx][13 + i * 2].mPosition =
				vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount), 1.0f);
			gBackGroundPoints[frameIdx][13 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
		}
		// start from 24

		for (int i = 1; i <= 9; ++i)
		{
			gBackGroundPoints[frameIdx][24 + i * 2].mPosition = vec2(-1.0f, -1.0f + i * (2.0f / 10.0f));
			gBackGroundPoints[frameIdx][24 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
			gBackGroundPoints[frameIdx][25 + i * 2].mPosition = vec2(1.0f, -1.0f + i * (2.0f / 10.0f));
			gBackGroundPoints[frameIdx][25 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
		}
		//start from 42

		gBackGroundPoints[frameIdx][42].mPosition = vec2(-1.0f, -1.0f);
		gBackGroundPoints[frameIdx][42].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][43].mPosition = vec2(1.0f, -1.0f);
		gBackGroundPoints[frameIdx][43].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][44].mPosition = vec2(-1.0f, 1.0f);
		gBackGroundPoints[frameIdx][44].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][45].mPosition = vec2(1.0f, 1.0f);
		gBackGroundPoints[frameIdx][45].mColor = vec4(0.85f, 0.9f, 0.0f, 0.25f);

		gBackGroundPoints[frameIdx][42].mPosition = vec2(-1.0f, -1.0f);
		gBackGroundPoints[frameIdx][42].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][43].mPosition = vec2(1.0f, -1.0f);
		gBackGroundPoints[frameIdx][43].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][44].mPosition = vec2(-1.0f, 1.0f);
		gBackGroundPoints[frameIdx][44].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		gBackGroundPoints[frameIdx][45].mPosition = vec2(1.0f, 1.0f);
		gBackGroundPoints[frameIdx][45].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);

		BufferUpdateDesc backgroundVbUpdate = { pBackGroundVertexBuffer[frameIdx], gBackGroundPoints[frameIdx] };
		updateResource(&backgroundVbUpdate, true);
	}

	void CpuGraphcmdUpdateBuffer(CpuGraphData* graphData, CpuGraph* graph, Cmd* cmd, uint32_t frameIdx)
	{
		int index = graphData->mSampleIdx;
		// fill up tri vertex
		for (int i = 0; i < gSampleCount; ++i)
		{
			if (--index < 0)
				index = gSampleCount - 1;
			graph->mPoints[i * 2].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f, -0.97f);
			graph->mPoints[i * 2].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
			graph->mPoints[i * 2 + 1].mPosition = vec2(
				(1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
				(2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
			graph->mPoints[i * 2 + 1].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
		}

		//line vertex
		index = graphData->mSampleIdx;
		for (int i = 0; i < gSampleCount; ++i)
		{
			if (--index < 0)
				index = gSampleCount - 1;
			graph->mPoints[i + 2 * gSampleCount].mPosition = vec2(
				(1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
				(2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
			graph->mPoints[i + 2 * gSampleCount].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
		}

		BufferUpdateDesc vbUpdate = { graph->mVertexBuffer[frameIdx], graph->mPoints };
		updateResource(&vbUpdate, true);
	}

	// thread for recording particle draw
	static void ParticleThreadDraw(void* pData)
	{
		ThreadData* data = (ThreadData*)pData;
		Cmd*        cmd = data->ppCmds[data->mFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, data->pGpuProfiler);
		cmdBindRenderTargets(cmd, 1, &data->pRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)data->pRenderTarget->mDesc.mWidth, (float)data->pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, data->pRenderTarget->mDesc.mWidth, data->pRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipeline);
		DescriptorData params[3] = {};
		params[0].pName = "uTex0";
		params[0].mCount = sizeof(pImageFileNames) / sizeof(pImageFileNames[0]);
		params[0].ppTextures = pTextures;
		params[1].pName = "uniformBlock";
		params[1].ppBuffers = &pProjViewUniformBuffer[gFrameIndex];
		params[2].pName = "particleRootConstant";
		params[2].pRootConstant = &gParticleData;
		cmdBindDescriptors(cmd, pRootSignature, 3, params);

		cmdBindVertexBuffer(cmd, 1, &pParticleVertexBuffer, NULL);

		cmdDrawInstanced(cmd, data->mDrawCount, data->mStartPoint, 1, 0);

		cmdEndGpuFrameProfile(cmd, data->pGpuProfiler);
		endCmd(cmd);
	}

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(MultiThread)
