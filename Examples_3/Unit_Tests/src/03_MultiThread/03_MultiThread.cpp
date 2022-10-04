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

#define _USE_MATH_DEFINES

//tiny stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

#include "../../../../Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

// for cpu usage query
#if defined(_WINDOWS)
#include <Windows.h>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "comsuppw.lib")
#elif defined(__linux__)
#include <unistd.h>    // sysconf(), _SC_NPROCESSORS_ONLN
#elif defined(NX64)
//todo
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

// startdust hash function, use this to generate all the seed and update the position of all particles
#define RND_GEN(x) ((x) = (x)*196314165 + 907633515)

#define MAX_CORES 64
#define MAX_GPU_PROFILE_NAME_LENGTH 256

struct ParticleData
{
	float    mPaletteFactor;
	uint32_t mData;
	uint32_t mTextureIndex;
};

struct ThreadData
{
	CmdPool*      pCmdPool;
	Cmd*          pCmd;
	RenderTarget* pRenderTarget;
	int           mStartPoint;
	int           mDrawCount;
	int           mThreadIndex;
	ThreadID      mThreadID;
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
};

int      gTotalParticleCount = 2000000;
uint32_t gGraphWidth = 200;
uint32_t gGraphHeight = 100;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool[gImageCount] = { NULL };
Cmd*     ppCmds[gImageCount] = { NULL };
Cmd*     ppGraphCmds[gImageCount] = { NULL };

Cmd**     ppThreadCmds[gImageCount] = { NULL };
CmdPool** pThreadCmdPools[gImageCount] = { NULL };

Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

SwapChain* pSwapChain = NULL;

Shader*            pShader = NULL;
Shader*            pSkyBoxDrawShader = NULL;
Shader*            pGraphShader = NULL;
Buffer*            pParticleVertexBuffer = NULL;
Buffer*            pProjViewUniformBuffer[gImageCount] = { NULL };
Buffer*            pSkyboxUniformBuffer[gImageCount] = { NULL };
Buffer*            pSkyBoxVertexBuffer = NULL;
Buffer*            pBackGroundVertexBuffer[gImageCount] = { NULL };
Pipeline*          pPipeline = NULL;
Pipeline*          pSkyBoxDrawPipeline = NULL;
Pipeline*          pGraphLinePipeline = NULL;
Pipeline*          pGraphLineListPipeline = NULL;
Pipeline*          pGraphTrianglePipeline = NULL;
RootSignature*     pRootSignature = NULL;
RootSignature*     pGraphRootSignature = NULL;
DescriptorSet*     pDescriptorSet = NULL;
DescriptorSet*     pDescriptorSetUniforms = NULL;
Texture*           pTextures[5];
Texture*           pSkyBoxTextures[6];
Sampler*           pSampler = NULL;
Sampler*           pSamplerSkyBox = NULL;
uint32_t           gFrameIndex = 0;
bool               bShowThreadsPlot = true;

#if defined(_WINDOWS)
IWbemServices* pService;
IWbemLocator*  pLocator;
uint64_t*      pOldTimeStamp;
uint64_t*      pOldPprocUsage;
#elif (__linux__)
uint64_t* pOldTimeStamp;
uint64_t* pOldPprocUsage;
#elif defined(NX64)
//todo
#elif defined(__APPLE__)
NSLock*                CPUUsageLock;
processor_info_array_t prevCpuInfo;
mach_msg_type_number_t numPrevCpuInfo;
#endif

uint   gCoresCount;
float* pCoresLoadData;

uint32_t     gThreadCount = 0;
ThreadData*  pThreadData;
CameraMatrix gProjectView;
CameraMatrix gSkyboxProjectView;
ParticleData gParticleData;
uint32_t     gParticleRootConstantIndex;
uint32_t     gSeed;
float        gPaletteFactor;
uint         gTextureIndex;

char gMainThreadTxt[64] = { 0 };
char gParticleThreadText[64] = { 0 };

UIComponent*      pGuiWindow;
ICameraController* pCameraController = NULL;

ThreadSystem* pThreadSystem;

ProfileToken* pGpuProfiletokens;

CpuGraphData* pCpuData;
CpuGraph*     pCpuGraph;

const char* pImageFileNames[] = { "Palette_Fire", "Palette_Purple", "Palette_Muted", "Palette_Rainbow", "Palette_Sky" };
const char* pSkyBoxImageFileNames[] = { "Skybox_right1", "Skybox_left2", "Skybox_top3", "Skybox_bottom4", "Skybox_front5", "Skybox_back6" };

FontDrawDesc gFrameTimeDraw;

uint32_t* gSeedArray = NULL;
uint64_t  gParDataSize = 0;

ThreadID initialThread;

uint32_t gFontID = 0; 

class MultiThread: public IApp
{
	public:
	MultiThread()  //-V832
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
#ifdef ANDROID
		//We reduce particles quantity for Android in order to keep 30fps
		gTotalParticleCount = 750000;
		bShowThreadsPlot = false;
#endif    // ANDROID
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		InitCpuUsage();

		// gThreadCount is the amount of secondary threads: the amount of physical cores except the main thread
		gThreadCount = gCoresCount - 1;
		pThreadData = (ThreadData*)tf_calloc(gThreadCount, sizeof(ThreadData));

		initialThread = getCurrentThreadID();

		// initial needed data for each thread
		for (uint32_t i = 0; i < gThreadCount; ++i)
		{
			// fill up the data for drawing point
			pThreadData[i].mStartPoint = i * (gTotalParticleCount / gThreadCount);
			pThreadData[i].mDrawCount = (gTotalParticleCount / gThreadCount);
			pThreadData[i].mThreadIndex = i;
			pThreadData[i].mThreadID = initialThread;
		}

		// This information is per core
		pGpuProfiletokens = (ProfileToken*)tf_calloc(gCoresCount, sizeof(ProfileToken));

		initThreadSystem(&pThreadSystem);

		// generate partcile data
		unsigned int particleSeed = 23232323;    //we have gseed as global declaration, pick a name that is not gseed
		for (int i = 0; i < 6 * 9; ++i)
		{
			RND_GEN(particleSeed);
		}

		gSeedArray = (uint32_t*)tf_malloc(gTotalParticleCount * sizeof(uint32_t));
		for (int i = 0; i < gTotalParticleCount; ++i)
		{
			RND_GEN(particleSeed);
			gSeedArray[i] = particleSeed;
		}

		gParDataSize = sizeof(uint32_t) * (uint64_t)gTotalParticleCount;

		char gpuProfileNames[MAX_CORES][MAX_GPU_PROFILE_NAME_LENGTH];

		const char** ppConstGpuProfileNames = (const char**)tf_calloc(gCoresCount, sizeof(const char*));
		Queue**      ppQueues = (Queue**)tf_calloc(gCoresCount, sizeof(Queue*));

		gGraphWidth = mSettings.mWidth / 6;    //200;
		gGraphHeight = gCoresCount ? (mSettings.mHeight - 30 - gCoresCount * 10) / gCoresCount : 0;

		// DirectX 11 not supported on this unit test
		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// initial Gpu profilers for each core
		for (uint32_t i = 0; i < gCoresCount; ++i)
		{
			if (i == 0)
				snprintf(gpuProfileNames[i], 64, "Graphics");
			else
				snprintf(gpuProfileNames[i], 64, "Gpu Particle thread %u", i - 1);

			ppConstGpuProfileNames[i] = gpuProfileNames[i];    //-V507
			ppQueues[i] = pGraphicsQueue;
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPool[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPool[i];
			addCmd(pRenderer, &cmdDesc, &ppCmds[i]);

			cmdDesc.pPool = pCmdPool[i];
			addCmd(pRenderer, &cmdDesc, &ppGraphCmds[i]);

			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);

			ppThreadCmds[i] = (Cmd**)tf_malloc(gThreadCount * sizeof(Cmd*));
			pThreadCmdPools[i] = (CmdPool**)tf_malloc(gThreadCount * sizeof(CmdPool*));

			for (uint32_t t = 0; t < gThreadCount; ++t)
			{
				// create cmd pools and and cmdbuffers for all thread
				addCmdPool(pRenderer, &cmdPoolDesc, &pThreadCmdPools[i][t]);
				cmdDesc.pPool = pThreadCmdPools[i][t];
				addCmd(pRenderer, &cmdDesc, &ppThreadCmds[i][t]);
			}
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		HiresTimer timer;
		initHiresTimer(&timer);
		initResourceLoaderInterface(pRenderer);

		// load all image to GPU
		for (int i = 0; i < 5; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.pFileName = pImageFileNames[i];
			textureDesc.ppTexture = &pTextures[i];
			// Textures representing color should be stored in SRGB or HDR format
			textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
			addResource(&textureDesc, NULL);
		}

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.pFileName = pSkyBoxImageFileNames[i];
			textureDesc.ppTexture = &pSkyBoxTextures[i];
			// Textures representing color should be stored in SRGB or HDR format
			textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
			addResource(&textureDesc, NULL);
		}

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

		gTextureIndex = 0;

		//#ifdef _WINDOWS
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
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer;
		addResource(&skyboxVbDesc, NULL);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(CameraMatrix);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc, NULL);
			ubDesc.ppBuffer = &pSkyboxUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		BufferLoadDesc particleVbDesc = {};
		particleVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		particleVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		particleVbDesc.mDesc.mSize = gParDataSize;
		particleVbDesc.pData = gSeedArray;
		particleVbDesc.ppBuffer = &pParticleVertexBuffer;
		addResource(&particleVbDesc, NULL);

		uint32_t graphDataSize = sizeof(GraphVertex) * gSampleCount * 3;    // 2 vertex for tri, 1 vertex for line strip

		//generate vertex buffer for all cores to draw cpu graph and setting up view port for each graph
		pCpuGraph = (CpuGraph*)tf_malloc(sizeof(CpuGraph) * gCoresCount);
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
				vbDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
				vbDesc.mDesc.mSize = graphDataSize;
				vbDesc.pData = NULL;
				vbDesc.ppBuffer = &pCpuGraph[i].mVertexBuffer[j];
				addResource(&vbDesc, NULL);
			}
		}
		graphDataSize = sizeof(GraphVertex) * gSampleCount;
		for (uint i = 0; i < gImageCount; ++i)
		{
			BufferLoadDesc vbDesc = {};
			vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
			vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
			vbDesc.mDesc.mStartState = RESOURCE_STATE_COMMON;
			vbDesc.mDesc.mSize = graphDataSize;
			vbDesc.pData = NULL;
			vbDesc.ppBuffer = &pBackGroundVertexBuffer[i];
			addResource(&vbDesc, NULL);
		}

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

		// Initialize profiler
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.ppQueues = ppQueues;
		profiler.ppProfilerNames = ppConstGpuProfileNames; 
		profiler.pProfileTokens = pGpuProfiletokens; 
		profiler.mGpuProfilerCount = gCoresCount; 
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		tf_free(ppQueues);
		tf_free(ppConstGpuProfileNames);

		/************************************************************************/
		// GUI
		/************************************************************************/
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.15f, mSettings.mHeight * 0.01f);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

#if !defined(TARGET_IOS) && !defined(DURANGO) && !defined(ANDROID)

		CheckboxWidget threadPlotsBox;
		threadPlotsBox.pData = &bShowThreadsPlot;
		UIWidget* pThreadPlotsBox = uiCreateComponentWidget(pGuiWindow, "Show threads plot", &threadPlotsBox, WIDGET_TYPE_CHECKBOX);
		luaRegisterWidget(pThreadPlotsBox);

#endif

		waitForAllResourceLoads();
		LOGF(LogLevel::eINFO, "Load Time %lld", getHiresTimerUSec(&timer, false) / 1000);

		CameraMotionParameters cmp{ 100.0f, 800.0f, 1000.0f };
		vec3                   camPos{ 24.0f, 24.0f, 10.0f };
		vec3                   lookAt{ 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);

		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
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

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0;

		return true;
	}

	void Exit()
	{
		waitThreadSystemIdle(pThreadSystem);

		exitInputSystem();
		exitThreadSystem(pThreadSystem);
		exitCameraController(pCameraController);

		ExitCpuUsage();

		tf_free(gSeedArray);
		tf_free(pThreadData);
		tf_free(pGpuProfiletokens);

		exitProfiler();

		exitUserInterface();

		exitFontSystem();

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

		tf_free(pCpuGraph);

		for (uint i = 0; i < 5; ++i)
			removeResource(pTextures[i]);
		for (uint i = 0; i < 6; ++i)
			removeResource(pSkyBoxTextures[i]);

		removeSampler(pRenderer, pSampler);
		removeSampler(pRenderer, pSamplerSkyBox);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

			removeCmd(pRenderer, ppCmds[i]);
			removeCmd(pRenderer, ppGraphCmds[i]);
			removeCmdPool(pRenderer, pCmdPool[i]);

			for (uint32_t t = 0; t < gThreadCount; ++t)
			{
				removeCmd(pRenderer, ppThreadCmds[i][t]);
				removeCmdPool(pRenderer, pThreadCmdPools[i][t]);
			}

			tf_free(ppThreadCmds[i]);
			tf_free(pThreadCmdPools[i]);
		}

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);
		removeQueue(pRenderer, pGraphicsQueue);
		exitResourceLoaderInterface(pRenderer);
		exitRenderer(pRenderer);
		pRenderer = NULL;
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
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
		// Input
		/************************************************************************/
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
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 100.0f);
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
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

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

		uint32_t frameIdx = gFrameIndex;

		resetCmdPool(pRenderer, pCmdPool[frameIdx]);

		SyncToken graphUpdateToken = {};

		for (uint32_t i = 0; i < gCoresCount; ++i)
			CpuGraphcmdUpdateBuffer(frameIdx, &pCpuData[i], &pCpuGraph[i], &graphUpdateToken);    // update vertex buffer for each cpugraph

		// update vertex buffer for background of the graph (grid)
		CpuGraphBackGroundUpdate(frameIdx, &graphUpdateToken);
		/*******record command for drawing particles***************/
		for (uint32_t i = 0; i < gThreadCount; ++i)
		{
			pThreadData[i].pRenderTarget = pRenderTarget;
			pThreadData[i].mFrameIndex = frameIdx;
			pThreadData[i].pCmdPool = pThreadCmdPools[gFrameIndex][i];
			pThreadData[i].pCmd = ppThreadCmds[gFrameIndex][i];
		}
		addThreadSystemRangeTask(pThreadSystem, &MultiThread::ParticleThreadDraw, pThreadData, gThreadCount);
		// simply record the screen cleaning command

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0].r = 0.0f;
		loadActions.mClearColorValues[0].g = 0.0f;
		loadActions.mClearColorValues[0].b = 0.0f;
		loadActions.mClearColorValues[0].a = 0.0f;

		Cmd* cmd = ppCmds[frameIdx];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiletokens[0]);    // pGpuProfiletokens[0] is reserved for main thread

		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(CameraMatrix*)viewProjCbv.pMappedData = gProjectView;
		endUpdateResource(&viewProjCbv, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(CameraMatrix*)skyboxViewProjCbv.pMappedData = gSkyboxProjectView;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		//// draw skybox
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
		cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSet);
		cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSetUniforms);
		const uint32_t skyboxStride = sizeof(float) * 4;
		cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, &skyboxStride, NULL);
		cmdDraw(cmd, 36, 0);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);

		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
				
		const float yTxtOffset = 12.f;
		const float xTxtOffset = 8.f;
		float       yTxtOrig = yTxtOffset;

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontID = gFontID;

		float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
		yTxtOrig += txtSizePx.y + 7 * yTxtOffset;

		txtSizePx = cmdDrawGpuProfile(cmd, float2(xTxtOffset, yTxtOrig), pGpuProfiletokens[0], &gFrameTimeDraw);
		yTxtOrig += txtSizePx.y + yTxtOffset;

		txtSizePx.y = 15.0f;
		for (uint32_t i = 0; i < gCoresCount; ++i)
		{
			snprintf(gParticleThreadText, 64, "GPU Particle Thread %u - %f ms", i, getGpuProfileAvgTime(pGpuProfiletokens[i]));
			gFrameTimeDraw.pText = gParticleThreadText;
			cmdDrawTextWithFont(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
			yTxtOrig += txtSizePx.y + yTxtOffset;
		}

		cmdDrawUserInterface(cmd);
		cmdEndDebugMarker(cmd);

		cmdEndGpuFrameProfile(cmd, pGpuProfiletokens[0]);    // pGpuProfiletokens[0] is reserved for main thread
		endCmd(cmd);

		beginCmd(ppGraphCmds[frameIdx]);
		if (bShowThreadsPlot)
		{
			for (uint i = 0; i < gCoresCount; ++i)
			{
				gGraphWidth = pRenderTarget->mWidth / 6;
				gGraphHeight = (pRenderTarget->mHeight - 30 - gCoresCount * 10) / gCoresCount;
				pCpuGraph[i].mViewPort.mOffsetX = pRenderTarget->mWidth - 10.0f - gGraphWidth;
				pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
				pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
				pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;

				loadActions = {};
				loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
				cmdBindRenderTargets(ppGraphCmds[frameIdx], 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
				
				cmdSetViewport(
					ppGraphCmds[frameIdx], pCpuGraph[i].mViewPort.mOffsetX, pCpuGraph[i].mViewPort.mOffsetY, pCpuGraph[i].mViewPort.mWidth,
					pCpuGraph[i].mViewPort.mHeight, 0.0f, 1.0f);
				cmdSetScissor(ppGraphCmds[frameIdx], 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

				const uint32_t graphDataStride = sizeof(GraphVertex);    // vec2(position) + vec4(color)

				cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
				cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx], &graphDataStride, NULL);
				cmdDraw(ppGraphCmds[frameIdx], 4, 0);

				cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLineListPipeline);
				cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pBackGroundVertexBuffer[frameIdx], &graphDataStride, NULL);
				cmdDraw(ppGraphCmds[frameIdx], 38, 4);

				cmdBindPipeline(ppGraphCmds[frameIdx], pGraphTrianglePipeline);
				cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &(pCpuGraph[i].mVertexBuffer[frameIdx]), &graphDataStride, NULL);
				cmdDraw(ppGraphCmds[frameIdx], 2 * gSampleCount, 0);

				cmdBindPipeline(ppGraphCmds[frameIdx], pGraphLinePipeline);
				cmdBindVertexBuffer(ppGraphCmds[frameIdx], 1, &pCpuGraph[i].mVertexBuffer[frameIdx], &graphDataStride, NULL);
				cmdDraw(ppGraphCmds[frameIdx], gSampleCount, 2 * gSampleCount);
			}
		}
		cmdSetViewport(
			ppGraphCmds[frameIdx], 0.0f, 0.0f, static_cast<float>(mSettings.mWidth), static_cast<float>(mSettings.mHeight), 0.0f, 1.0f);
		cmdSetScissor(ppGraphCmds[frameIdx], 0, 0, mSettings.mWidth, mSettings.mHeight);

		cmdBindRenderTargets(ppGraphCmds[frameIdx], 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(ppGraphCmds[frameIdx], 0, NULL, 0, NULL, 1, &barrier);
		endCmd(ppGraphCmds[frameIdx]);

		// wait all particle threads done
		waitThreadSystemIdle(pThreadSystem);
		// Wait till graph buffers have been uploaded to the gpu
		waitForToken(&graphUpdateToken);
		/***************draw cpu graph*****************************/
		/***************draw cpu graph*****************************/
		// gather all command buffer, it is important to keep the screen clean command at the beginning
		uint32_t cmdCount = gThreadCount + 2;
		Cmd**    allCmds = (Cmd**)alloca(cmdCount * sizeof(Cmd*));
		allCmds[0] = cmd;

		for (uint32_t i = 0; i < gThreadCount; ++i)
		{
			allCmds[i + 1] = pThreadData[i].pCmd;
		}
		allCmds[gThreadCount + 1] = ppGraphCmds[frameIdx];

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = cmdCount;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = allCmds;
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
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "03_MultiThread"; }

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
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addDescriptorSets()
	{
		DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
		setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSet);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
	}

	void addRootSignatures()
	{
		const char*       pStaticSamplerNames[] = { "uSampler0", "uSkyboxSampler" };
		Sampler*          pSamplers[] = { pSampler, pSamplerSkyBox };
		Shader*           shaders[] = { pShader, pSkyBoxDrawShader };
		RootSignatureDesc skyBoxRootDesc = {};
		skyBoxRootDesc.mStaticSamplerCount = 2;
		skyBoxRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		skyBoxRootDesc.ppStaticSamplers = pSamplers;
		skyBoxRootDesc.mShaderCount = 2;
		skyBoxRootDesc.ppShaders = shaders;
		skyBoxRootDesc.mMaxBindlessTextures = 5;
		addRootSignature(pRenderer, &skyBoxRootDesc, &pRootSignature);
		gParticleRootConstantIndex = getDescriptorIndexFromName(pRootSignature, "particleRootConstant");

		RootSignatureDesc graphRootDesc = {};
		graphRootDesc.mShaderCount = 1;
		graphRootDesc.ppShaders = &pGraphShader;
		addRootSignature(pRenderer, &graphRootDesc, &pGraphRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
		removeRootSignature(pRenderer, pGraphRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc graphShader = {};
		graphShader.mStages[0] = { "Graph.vert", NULL, 0 };
		graphShader.mStages[1] = { "Graph.frag", NULL, 0 };

		ShaderLoadDesc particleShader = {};
		particleShader.mStages[0] = { "Particle.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		particleShader.mStages[1] = { "Particle.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "Skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyShader.mStages[1] = { "Skybox.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };

		addShader(pRenderer, &particleShader, &pShader);
		addShader(pRenderer, &skyShader, &pSkyBoxDrawShader);
		addShader(pRenderer, &graphShader, &pGraphShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShader);
		removeShader(pRenderer, pSkyBoxDrawShader);
		removeShader(pRenderer, pGraphShader);
	}

	void addPipelines()
	{
		//vertexlayout and pipeline for particles
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32_UINT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateDesc.mSrcFactors[0] = BC_ONE;
		blendStateDesc.mDstFactors[0] = BC_ONE;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		PipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_POINT_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pBlendState = &blendStateDesc;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipeline);

		//layout and pipeline for skybox draw
		vertexLayout = {};
		vertexLayout.mAttribCount = 1;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;

		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pRootSignature;
		pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pSkyBoxDrawPipeline);

		/********** layout and pipeline for graph draw*****************/
		vertexLayout = {};
		vertexLayout.mAttribCount = 2;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat =
			(sizeof(GraphVertex) > 24
				? TinyImageFormat_R32G32B32A32_SFLOAT
				: TinyImageFormat_R32G32_SFLOAT);    // Handle the case when padding is added to the struct (yielding 32 bytes instead of 24) on macOS
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[0].mOffset = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayout.mAttribs[1].mBinding = 0;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(vertexLayout.mAttribs[0].mFormat) / 8;

		pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_STRIP;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.pRootSignature = pGraphRootSignature;
		pipelineSettings.pShaderProgram = pGraphShader;
		pipelineSettings.pVertexLayout = &vertexLayout;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pGraphLinePipeline);

		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_STRIP;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pGraphTrianglePipeline);

		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_LIST;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pGraphLineListPipeline);
		/********************************************************************/
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipeline);
		removePipeline(pRenderer, pSkyBoxDrawPipeline);
		removePipeline(pRenderer, pGraphLineListPipeline);
		removePipeline(pRenderer, pGraphLinePipeline);
		removePipeline(pRenderer, pGraphTrianglePipeline);
	}

	void prepareDescriptorSets()
	{
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
		updateDescriptorSet(pRenderer, 0, pDescriptorSet, 6, params);

		params[0].pName = "uTex0";
		params[0].mCount = sizeof(pImageFileNames) / sizeof(pImageFileNames[0]);
		params[0].ppTextures = pTextures;
		updateDescriptorSet(pRenderer, 1, pDescriptorSet, 1, params);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			params[0] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pSkyboxUniformBuffer[i];
			updateDescriptorSet(pRenderer, i * 2 + 0, pDescriptorSetUniforms, 1, params);
			params[0].ppBuffers = &pProjViewUniformBuffer[i];
			updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSetUniforms, 1, params);
		}
	}

#if defined(__linux__)
	enum CPUStates{ S_USER = 0,    S_NICE, S_SYSTEM, S_IDLE, S_IOWAIT, S_IRQ, S_SOFTIRQ, S_STEAL, S_GUEST, S_GUEST_NICE,

					NUM_CPU_STATES };
	typedef struct CPUData
	{
		char   cpu[64];
		size_t times[NUM_CPU_STATES];
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
#ifdef _WINDOWS
		ULONG retVal;
		UINT  i;

		IWbemClassObject*     pclassObj;
		IEnumWbemClassObject* pEnumerator;

		pService->ExecQuery(
			bstr_t("WQL"),
			bstr_t("SELECT TimeStamp_Sys100NS, PercentProcessorTime, Frequency_PerfTime FROM Win32_PerfRawData_PerfOS_Processor"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
		for (i = 0; i < gCoresCount; i++)
		{
			//Waiting for inifinite blocks resources and app.
			//Waiting for 15 ms (arbitrary) instead works much better
			pEnumerator->Next(15, 1, &pclassObj, &retVal);
			if (!retVal)
			{
				break;
			}

			VARIANT vtPropTime;
			VARIANT vtPropClock;
			VariantInit(&vtPropTime);
			VariantInit(&vtPropClock);

			pclassObj->Get(L"TimeStamp_Sys100NS", 0, &vtPropTime, 0, 0);
			UINT64 newTimeStamp = _wtoi64(vtPropTime.bstrVal);

			pclassObj->Get(L"PercentProcessorTime", 0, &vtPropClock, 0, 0);
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
#elif defined(__linux__) && !(__ANDROID__)
		eastl::vector<CPUData> entries;
		entries.reserve(gCoresCount);
		// Open cpu stat file

		FILE* fh = fopen("/proc/stat", "rb");

		if (fh)
		{
			// While eof not detected, keep parsing the stat file
			while (!feof(fh))
			{
				entries.emplace_back(CPUData());
				CPUData& entry = entries.back();
				char     dummyCpuName[256];    // dummy cpu name, not used.
				int _ret = fscanf(
					fh, "%s %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu", &dummyCpuName[0], &entry.times[0], &entry.times[1], &entry.times[2],
					&entry.times[3], &entry.times[4], &entry.times[5], &entry.times[6], &entry.times[7], &entry.times[8], &entry.times[9]);
                (void)_ret;
			}
			// Close the cpu stat file
			fclose(fh);
		}
		else
		{
			return;
		}

		for (uint32_t i = 0; i < gCoresCount; i++)
		{
			float ACTIVE_TIME = static_cast<float>(GetActiveTime(entries[i]));
			float IDLE_TIME = static_cast<float>(GetIdleTime(entries[i]));

			pCoresLoadData[i] = (ACTIVE_TIME - pOldPprocUsage[i]) / ((float)(IDLE_TIME + ACTIVE_TIME) - pOldTimeStamp[i]) * 100.0f;

			pOldPprocUsage[i] = ACTIVE_TIME;
			pOldTimeStamp[i] = IDLE_TIME + ACTIVE_TIME;
		}
#elif defined(NX64)
		//
#elif defined(__APPLE__)
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
#if defined(_WINDOWS)
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

		pService->ExecQuery(
			bstr_t("WQL"), bstr_t("SELECT * FROM Win32_Processor"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
			&pEnumerator);
		pEnumerator->Next(WBEM_INFINITE, 1, &pclassObj, &retVal);
		if (retVal)
		{
			VARIANT vtProp;
			VariantInit(&vtProp);
			pclassObj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0);
			gCoresCount = vtProp.uintVal;
			VariantClear(&vtProp);
		}

		pclassObj->Release();
		pEnumerator->Release();

		if (gCoresCount)
		{
			pOldTimeStamp = (uint64_t*)tf_malloc(sizeof(uint64_t) * gCoresCount);
			pOldPprocUsage = (uint64_t*)tf_malloc(sizeof(uint64_t) * gCoresCount);
		}
#elif defined(XBOX)
		gCoresCount = getNumCPUCores();
#elif defined(__linux__)
		int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
		gCoresCount = numCPU;
		if (gCoresCount)
		{
			pOldTimeStamp = (uint64_t*)tf_malloc(sizeof(uint64_t) * gCoresCount);
			pOldPprocUsage = (uint64_t*)tf_malloc(sizeof(uint64_t) * gCoresCount);
		}
#elif defined(__APPLE__)
		processor_info_array_t cpuInfo;
		mach_msg_type_number_t numCpuInfo;

		natural_t     numCPUsU = 0U;
		kern_return_t err = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numCPUsU, &cpuInfo, &numCpuInfo);

		ASSERT(err == KERN_SUCCESS);

		gCoresCount = numCPUsU;

		CPUUsageLock = [[NSLock alloc] init];
#elif defined(ORBIS) || defined(PROSPERO) || defined(NX64)
		gCoresCount = getNumCPUCores();
#endif

		pCpuData = (CpuGraphData*)tf_malloc(sizeof(CpuGraphData) * gCoresCount);
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
			pCoresLoadData = (float*)tf_malloc(sizeof(float) * gCoresCount);
			memset(pCoresLoadData, 0, sizeof(float) * gCoresCount);
		}

		CalCpuUsage();
		return 1;
	}

	void ExitCpuUsage()
	{
		tf_free(pCpuData);
#if defined(_WINDOWS) || defined(__linux__)
		tf_free(pOldTimeStamp);
		tf_free(pOldPprocUsage);
#endif
		tf_free(pCoresLoadData);
	}

	void CpuGraphBackGroundUpdate(uint32_t frameIdx, SyncToken* token)
	{
		BufferUpdateDesc backgroundVbUpdate = { pBackGroundVertexBuffer[frameIdx] };
		beginUpdateResource(&backgroundVbUpdate);
		GraphVertex* backGroundPoints = (GraphVertex*)backgroundVbUpdate.pMappedData;
		memset((void *)backGroundPoints, 0, pBackGroundVertexBuffer[frameIdx]->mSize);

		// background data
		backGroundPoints[0].mPosition = vec2(-1.0f, -1.0f);
		backGroundPoints[0].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		backGroundPoints[1].mPosition = vec2(1.0f, -1.0f);
		backGroundPoints[1].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		backGroundPoints[2].mPosition = vec2(-1.0f, 1.0f);
		backGroundPoints[2].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);
		backGroundPoints[3].mPosition = vec2(1.0f, 1.0f);
		backGroundPoints[3].mColor = vec4(0.0f, 0.0f, 0.0f, 0.3f);

		const float woff = 2.0f / gGraphWidth;
		const float hoff = 2.0f / gGraphHeight;

		backGroundPoints[4].mPosition = vec2(-1.0f + woff, -1.0f + hoff);
		backGroundPoints[4].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[5].mPosition = vec2(1.0f - woff, -1.0f + hoff);
		backGroundPoints[5].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[6].mPosition = vec2(1.0f - woff, -1.0f + hoff);
		backGroundPoints[6].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[7].mPosition = vec2(1.0f - woff, 1.0f - hoff);
		backGroundPoints[7].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[8].mPosition = vec2(1.0f - woff, 1.0f - hoff);
		backGroundPoints[8].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[9].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
		backGroundPoints[9].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[10].mPosition = vec2(-1.0f + woff, 1.0f - hoff);
		backGroundPoints[10].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);
		backGroundPoints[11].mPosition = vec2(-1.0f + woff, -1.0f + hoff);
		backGroundPoints[11].mColor = vec4(0.0f, 0.3f, 0.5f, 0.25f);

		for (int i = 1; i <= 6; ++i)
		{
			backGroundPoints[12 + i * 2].mPosition =
				vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount), -1.0f);
			backGroundPoints[12 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
			backGroundPoints[13 + i * 2].mPosition =
				vec2(-1.0f + i * (2.0f / 6.0f) - 2.0f * ((pCpuData[0].mSampleIdx % (gSampleCount / 6)) / (float)gSampleCount), 1.0f);
			backGroundPoints[13 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
		}
		// start from 24

		for (int i = 1; i <= 9; ++i)
		{
			backGroundPoints[24 + i * 2].mPosition = vec2(-1.0f, -1.0f + i * (2.0f / 10.0f));
			backGroundPoints[24 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
			backGroundPoints[25 + i * 2].mPosition = vec2(1.0f, -1.0f + i * (2.0f / 10.0f));
			backGroundPoints[25 + i * 2].mColor = vec4(0.0f, 0.1f, 0.2f, 0.25f);
		}
		//start from 42

		backGroundPoints[42].mPosition = vec2(-1.0f, -1.0f);
		backGroundPoints[42].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		backGroundPoints[43].mPosition = vec2(1.0f, -1.0f);
		backGroundPoints[43].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		backGroundPoints[44].mPosition = vec2(-1.0f, 1.0f);
		backGroundPoints[44].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
		backGroundPoints[45].mPosition = vec2(1.0f, 1.0f);
		backGroundPoints[45].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);

		endUpdateResource(&backgroundVbUpdate, token);
	}

	void CpuGraphcmdUpdateBuffer(uint32_t frameIdx, CpuGraphData* graphData, CpuGraph* graph, SyncToken* token)
	{
		BufferUpdateDesc vbUpdate = { graph->mVertexBuffer[frameIdx] };
		beginUpdateResource(&vbUpdate);
		GraphVertex* points = (GraphVertex*)vbUpdate.pMappedData;
		memset((void *)points, 0, graph->mVertexBuffer[frameIdx]->mSize);

		int index = graphData->mSampleIdx;
		// fill up tri vertex
		for (uint32_t i = 0; i < gSampleCount; ++i)
		{
			if (--index < 0)
				index = gSampleCount - 1;
			points[i * 2].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f, -0.97f);
			points[i * 2].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
			points[i * 2 + 1].mPosition = vec2(
				(1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
				(2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
			points[i * 2 + 1].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
		}

		//line vertex
		index = graphData->mSampleIdx;
		for (uint32_t i = 0; i < gSampleCount; ++i)
		{
			if (--index < 0)
				index = gSampleCount - 1;
			points[i + 2 * gSampleCount].mPosition = vec2(
				(1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
				(2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
			points[i + 2 * gSampleCount].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
		}

		endUpdateResource(&vbUpdate, token);
	}

	// thread for recording particle draw
	static void ParticleThreadDraw(void* pData, uintptr_t i)
	{
		ThreadData& data = ((ThreadData*)pData)[i];
		if (data.mThreadID == initialThread)
			data.mThreadID = getCurrentThreadID();
		//PROFILER_SET_CPU_SCOPE("Threads", "Cpu draw", 0xffffff);
		Cmd* cmd = data.pCmd;
		resetCmdPool(pRenderer, data.pCmdPool);
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiletokens[data.mThreadIndex + 1]);    // pGpuProfiletokens[0] is reserved for main thread

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

		cmdBindRenderTargets(cmd, 1, &data.pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)data.pRenderTarget->mWidth, (float)data.pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, data.pRenderTarget->mWidth, data.pRenderTarget->mHeight);

		const uint32_t parDataStride = sizeof(uint32_t);
		cmdBindPipeline(cmd, pPipeline);
		cmdBindDescriptorSet(cmd, 1, pDescriptorSet);
		cmdBindDescriptorSet(cmd, data.mFrameIndex * 2 + 1, pDescriptorSetUniforms);
		cmdBindPushConstants(cmd, pRootSignature, gParticleRootConstantIndex, &gParticleData);
		cmdBindVertexBuffer(cmd, 1, &pParticleVertexBuffer, &parDataStride, NULL);

		cmdDrawInstanced(cmd, data.mDrawCount, data.mStartPoint, 1, 0);

		cmdEndGpuFrameProfile(cmd, pGpuProfiletokens[data.mThreadIndex + 1]);    // pGpuProfiletokens[0] is reserved for main thread
		endCmd(cmd);
	}
};

DEFINE_APPLICATION_MAIN(MultiThread)
