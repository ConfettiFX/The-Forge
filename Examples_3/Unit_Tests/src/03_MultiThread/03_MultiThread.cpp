/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

// for cpu usage query
#if defined(NX64)
#endif

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

// fsl
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "./Shaders/FSL/Global.srt.h"

// startdust hash function, use this to generate all the seed and update the position of all particles
#define RND_GEN(x)                  ((x) = (x)*196314165 + 907633515)

#define MAX_CORES                   64
#define MAX_GPU_PROFILE_NAME_LENGTH 256

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

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

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
    Buffer*       mVertexBuffer[gDataBufferCount]; // vetex buffer for cpu sample
    ViewPortState mViewPort;                       // view port for different core
};

struct UniformBlock
{
    CameraMatrix mProjectView;
    CameraMatrix mSkyProjectView;
};

int      gTotalParticleCount = 2000000;
uint32_t gGraphWidth = 200;
uint32_t gGraphHeight = 100;

// VR 2D layer transform (positioned at -1 along the Z axis, default rotation, default scale)
VR2DLayerDesc gVR2DLayer{ { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, 1.0f };

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

static FORGE_CONSTEXPR const uint32_t gMaxThreadCount = MAX_GPU_CMD_POOLS_PER_RING / gDataBufferCount;
GpuCmdRing                            gThreadCmdRing = {};

Semaphore* pImageAcquiredSemaphore = NULL;
SwapChain* pSwapChain = NULL;

Shader*        pShader = NULL;
Shader*        pSkyBoxDrawShader = NULL;
Shader*        pGraphShader = NULL;
Buffer*        pParticleVertexBuffer = NULL;
Buffer*        pUniformBuffer[gDataBufferCount] = { NULL };
Buffer*        pSkyBoxVertexBuffer = NULL;
Buffer*        pBackGroundVertexBuffer[gDataBufferCount] = { NULL };
Buffer*        pPerDrawBuffers[gDataBufferCount][gMaxThreadCount];
Pipeline*      pPipeline = NULL;
Pipeline*      pSkyBoxDrawPipeline = NULL;
Pipeline*      pGraphLinePipeline = NULL;
Pipeline*      pGraphLineListPipeline = NULL;
Pipeline*      pGraphTrianglePipeline = NULL;
DescriptorSet* pDescriptorSetPersistent = NULL;
DescriptorSet* pDescriptorSetPerFrame = NULL;
DescriptorSet* pDescriptorSetPerDraw = NULL;
Texture*       pTextures[5];
Texture*       pSkyBoxTextures[6];
Sampler*       pSampler = NULL;
uint32_t       gFrameIndex = 0;
bool           bShowThreadsPlot = true;

uint   gCoresCount;
float* pCoresLoadData;

uint32_t     gThreadCount = 0;
ThreadData   gThreadData[gMaxThreadCount] = {};
CameraMatrix gProjectView;
CameraMatrix gSkyboxProjectView;
ParticleData gParticleData;
uint32_t     gParticleRootConstantIndex;
uint32_t     gSeed;
float        gPaletteFactor;
uint         gTextureIndex;

char gMainThreadTxt[64] = { 0 };
char gParticleThreadText[64] = { 0 };

UIComponent*       pGuiWindow;
ICameraController* pCameraController = NULL;

static ThreadSystem gThreadSystem;

ProfileToken gGpuProfiletokens[gMaxThreadCount + 1] = {};

CpuGraphData* pCpuData;
CpuGraph*     pCpuGraph;
bool          gPerformanceStatsInited = false;

const char* pImageFileNames[] = {
    "Palette_Fire.tex", "Palette_Purple.tex", "Palette_Muted.tex", "Palette_Rainbow.tex", "Palette_Sky.tex",
};
const char* pSkyBoxImageFileNames[] = {
    "Skybox_right1.tex", "Skybox_left2.tex", "Skybox_top3.tex", "Skybox_bottom4.tex", "Skybox_front5.tex", "Skybox_back6.tex",
};

FontDrawDesc gFrameTimeDraw;

uint32_t* gSeedArray = NULL;
uint64_t  gParDataSize = 0;

ThreadID initialThread;

uint32_t gFontID = 0;

const char* gTestScripts[] = { "Test_TurnOffPlots.lua" };
uint32_t    gCurrentScriptIndex = 0;

void RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

class MultiThread: public IApp
{
public:
    MultiThread() //-V832
    {
#ifdef TARGET_IOS
        mSettings.mContentScaleFactor = 1.f;
#endif
#ifdef ANDROID
        // We reduce particles quantity for Android in order to keep 30fps
        gTotalParticleCount = 750000;
        bShowThreadsPlot = false;
#endif // ANDROID
    }

    bool Init() override
    {
        if (InitCpuUsage())
            gPerformanceStatsInited = true;

        // gThreadCount is the amount of secondary threads: the amount of physical cores except the main thread
        gThreadCount = min(gMaxThreadCount, max(4u, gCoresCount) - 2);

        initialThread = getCurrentThreadID();

        // initial needed data for each thread
        for (uint32_t i = 0; i < gThreadCount; ++i)
        {
            // fill up the data for drawing point
            gThreadData[i].mStartPoint = i * (gTotalParticleCount / gThreadCount);
            gThreadData[i].mDrawCount = (gTotalParticleCount / gThreadCount);
            gThreadData[i].mThreadIndex = i;
            gThreadData[i].mThreadID = initialThread;
        }

        bool threadSystemInitialized = threadSystemInit(&gThreadSystem, &gThreadSystemInitDescDefault);
        ASSERT(threadSystemInitialized);

        // generate partcile data
        unsigned int particleSeed = 23232323; // we have gseed as global declaration, pick a name that is not gseed
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

        const char* constGpuProfileNames[gMaxThreadCount + 1] = {};
        Queue*      queues[gMaxThreadCount + 1] = {};

        // DirectX 11 not supported on this unit test
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        // initial Gpu profilers for each core
        for (uint32_t i = 0; i < gThreadCount + 1; ++i)
        {
            if (i == 0)
                snprintf(gpuProfileNames[i], 64, "Graphics");
            else
                snprintf(gpuProfileNames[i], 64, "Gpu Particle cmd %u", i - 1);

            constGpuProfileNames[i] = gpuProfileNames[i]; //-V507
            queues[i] = pGraphicsQueue;
        }

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 2;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        GpuCmdRingDesc threadCmdRingDesc = {};
        threadCmdRingDesc.pQueue = pGraphicsQueue;
        threadCmdRingDesc.mPoolCount = gThreadCount * gDataBufferCount;
        threadCmdRingDesc.mCmdPerPoolCount = 1;
        threadCmdRingDesc.mAddSyncPrimitives = false;
        initGpuCmdRing(pRenderer, &threadCmdRingDesc, &gThreadCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

        HiresTimer timer;
        initHiresTimer(&timer);
        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

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
        addSampler(pRenderer, &samplerDesc, &pSampler);
        gTextureIndex = 0;

        // #ifdef _WINDOWS
        //	  SYSTEM_INFO sysinfo;
        //	  GetSystemInfo(&sysinfo);
        //	  gCPUCoreCount = sysinfo.dwNumberOfProcessors;
        // #elif defined(__APPLE__)
        //	  gCPUCoreCount = (unsigned int)[[NSProcessInfo processInfo] processorCount];
        // #endif

        // Generate sky box vertex buffer
        float skyBoxPoints[] = {
            10.0f,  -10.0f, -10.0f, 6.0f, // -z
            -10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
            -10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

            -10.0f, -10.0f, 10.0f,  2.0f, //-x
            -10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
            -10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

            10.0f,  -10.0f, -10.0f, 1.0f, //+x
            10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
            10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

            -10.0f, -10.0f, 10.0f,  5.0f, // +z
            -10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
            10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

            -10.0f, 10.0f,  -10.0f, 3.0f, //+y
            10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
            10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

            10.0f,  -10.0f, 10.0f,  4.0f, //-y
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
        ubDesc.mDesc.mSize = sizeof(UniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pUniformBuffer[i];
            addResource(&ubDesc, NULL);
        }

        ubDesc.mDesc.mSize = sizeof(ParticleData);
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            for (uint32_t j = 0; j < gMaxThreadCount; ++j)
            {
                ubDesc.ppBuffer = &pPerDrawBuffers[i][j];
                addResource(&ubDesc, NULL);
            }
        }

        BufferLoadDesc particleVbDesc = {};
        particleVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        particleVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        particleVbDesc.mDesc.mSize = gParDataSize;
        particleVbDesc.pData = gSeedArray;
        particleVbDesc.ppBuffer = &pParticleVertexBuffer;
        addResource(&particleVbDesc, NULL);

        uint32_t graphDataSize = sizeof(GraphVertex) * gSampleCount * 3; // 2 vertex for tri, 1 vertex for line strip

        // generate vertex buffer for all cores to draw cpu graph and setting up view port for each graph
        pCpuGraph = (CpuGraph*)tf_malloc(sizeof(CpuGraph) * gCoresCount);
        for (uint i = 0; i < gCoresCount; ++i)
        {
            pCpuGraph[i].mViewPort.mOffsetX = mSettings.mWidth - 10.0f - gGraphWidth;
            pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
            pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
            pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;
            // create vertex buffer for each swapchain
            for (uint j = 0; j < gDataBufferCount; ++j)
            {
                BufferLoadDesc vbDesc = {};
                vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
                vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
                vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
                vbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
                vbDesc.mDesc.mSize = graphDataSize;
                vbDesc.pData = NULL;
                vbDesc.ppBuffer = &pCpuGraph[i].mVertexBuffer[j];
                addResource(&vbDesc, NULL);
            }
        }
        graphDataSize = sizeof(GraphVertex) * gSampleCount;
        for (uint i = 0; i < gDataBufferCount; ++i)
        {
            BufferLoadDesc vbDesc = {};
            vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
            vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
            vbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
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

        const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

        // Initialize profiler
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.ppQueues = queues;
        profiler.ppProfilerNames = constGpuProfileNames;
        profiler.pProfileTokens = gGpuProfiletokens;
        profiler.mGpuProfilerCount = gThreadCount + 1;
        initProfiler(&profiler);

        waitForAllResourceLoads();
        LOGF(LogLevel::eINFO, "Load Time %lld", getHiresTimerUSec(&timer, false) / 1000);

        CameraMotionParameters cmp{ 100.0f, 800.0f, 1000.0f };
        vec3                   camPos{ 24.0f, 24.0f, 10.0f };
        vec3                   lookAt{ 0 };

        pCameraController = initFpsCameraController(camPos, lookAt);

        pCameraController->setMotionParameters(cmp);

        AddCustomInputBindings();
        initScreenshotCapturer(pRenderer, pGraphicsQueue, GetName());
        gFrameIndex = 0;

        return true;
    }

    void Exit() override
    {
        exitScreenshotCapturer();
        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);
        exitCameraController(pCameraController);

        ExitCpuUsage();

        tf_free(gSeedArray);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pUniformBuffer[i]);
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            for (uint32_t j = 0; j < gMaxThreadCount; ++j)
            {
                removeResource(pPerDrawBuffers[i][j]);
            }
        }
        removeResource(pParticleVertexBuffer);
        removeResource(pSkyBoxVertexBuffer);

        for (uint i = 0; i < gDataBufferCount; ++i)
            removeResource(pBackGroundVertexBuffer[i]);

        for (uint i = 0; i < gCoresCount; ++i)
        {
            // remove all vertex buffer belongs to graph
            for (uint j = 0; j < gDataBufferCount; ++j)
                removeResource(pCpuGraph[i].mVertexBuffer[j]);
        }

        tf_free(pCpuGraph);

        for (uint i = 0; i < 5; ++i)
            removeResource(pTextures[i]);
        for (uint i = 0; i < 6; ++i)
            removeResource(pSkyBoxTextures[i]);

        removeSampler(pRenderer, pSampler);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitGpuCmdRing(pRenderer, &gThreadCmdRing);

        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitQueue(pRenderer, pGraphicsQueue);
        exitRootSignature(pRenderer);
        exitResourceLoaderInterface(pRenderer);
        exitRenderer(pRenderer);
        exitGPUConfiguration();

        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc) override
    {
        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            gGraphWidth = mSettings.mWidth / 6; // 200;
            gGraphHeight = gCoresCount ? (mSettings.mHeight - 30 - gCoresCount * 10) / gCoresCount : 0;
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent(GetName(), &guiDesc, &pGuiWindow);

#if !defined(TARGET_IOS) && !defined(DURANGO) && !defined(ANDROID)

            CheckboxWidget threadPlotsBox;
            threadPlotsBox.pData = &bShowThreadsPlot;
            UIWidget* pThreadPlotsBox = uiAddComponentWidget(pGuiWindow, "Show threads plot", &threadPlotsBox, WIDGET_TYPE_CHECKBOX);
            luaRegisterWidget(pThreadPlotsBox);

#endif
            DropdownWidget ddTestScripts;
            ddTestScripts.pData = &gCurrentScriptIndex;
            ddTestScripts.pNames = gTestScripts;
            ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);

            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);

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
        uiLoad.mVR2DLayer.mPosition = float3(gVR2DLayer.m2DLayerPosition.x, gVR2DLayer.m2DLayerPosition.y, gVR2DLayer.m2DLayerPosition.z);
        uiLoad.mVR2DLayer.mScale = gVR2DLayer.m2DLayerScale;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc) override
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
            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeShaders();
        }
    }

    void Update(float deltaTime) override
    {
        /************************************************************************/
        // Input
        /************************************************************************/
        if (!uiIsFocused())
        {
            pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
            pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
            pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
            if (inputGetValue(0, CUSTOM_RESET_VIEW))
            {
                pCameraController->resetView();
            }
            if (inputGetValue(0, CUSTOM_TOGGLE_FULLSCREEN))
            {
                toggleFullscreen(pWindow);
            }
            if (inputGetValue(0, CUSTOM_TOGGLE_UI))
            {
                uiToggleActive();
            }
            if (inputGetValue(0, CUSTOM_DUMP_PROFILE))
            {
                dumpProfileData(GetName());
            }
            if (inputGetValue(0, CUSTOM_EXIT))
            {
                requestShutdown();
            }
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
        mat4         modelMat = mat4::rotationX(gObjSettings.mRotX) * mat4::rotationY(gObjSettings.mRotY);
        CameraMatrix viewMat = pCameraController->getViewMatrix();

        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 100.0f);
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

    void Draw() override
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 2);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        uint32_t frameIdx = gFrameIndex;

        resetCmdPool(pRenderer, elem.pCmdPool);

        for (uint32_t i = 0; i < gCoresCount; ++i)
        {
            CpuGraphcmdUpdateBuffer(frameIdx, &pCpuData[i], &pCpuGraph[i]); // update vertex buffer for each cpugraph
        }

        // update vertex buffer for background of the graph (grid)
        CpuGraphBackGroundUpdate(frameIdx);
        /*******record command for drawing particles***************/
        for (uint32_t i = 0; i < gThreadCount; ++i)
        {
            GpuCmdRingElement threadElem = getNextGpuCmdRingElement(&gThreadCmdRing, true, 1);

            gThreadData[i].pRenderTarget = pRenderTarget;
            gThreadData[i].mFrameIndex = frameIdx;
            gThreadData[i].pCmdPool = threadElem.pCmdPool;
            gThreadData[i].pCmd = threadElem.pCmds[0];
        }
        threadSystemAddTaskGroup(gThreadSystem, ParticleThreadDraw, gThreadCount, gThreadData);
        // simply record the screen cleaning command

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
        cmdBeginGpuFrameProfile(cmd, gGpuProfiletokens[0]); // pGpuProfiletokens[0] is reserved for main thread

        BufferUpdateDesc viewProjCbv = { pUniformBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        UniformBlock ub = {};
        ub.mProjectView = gProjectView;
        ub.mSkyProjectView = gSkyboxProjectView;
        memcpy(viewProjCbv.pMappedData, &ub, sizeof(UniformBlock));
        endUpdateResource(&viewProjCbv);

        RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        //// draw skybox
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
        cmdBindPipeline(cmd, pSkyBoxDrawPipeline);
        const uint32_t skyboxStride = sizeof(float) * 4;
        cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer, &skyboxStride, NULL);
        cmdDraw(cmd, 36, 0);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);

        cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
        Cmd*          graphCmd = elem.pCmds[1];
        RenderTarget* pUIRenderTarget = cmdBeginDrawingUserInterface(cmd, pSwapChain, pRenderTarget);
        {
            const float yTxtOffset = 12.f;
            const float xTxtOffset = 8.f;
            float       yTxtOrig = yTxtOffset;

            gFrameTimeDraw.mFontColor = 0xff00ffff;
            gFrameTimeDraw.mFontID = gFontID;

            float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
            yTxtOrig += txtSizePx.y + 7 * yTxtOffset;

            txtSizePx = cmdDrawGpuProfile(cmd, float2(xTxtOffset, yTxtOrig), gGpuProfiletokens[0], &gFrameTimeDraw);
            yTxtOrig += txtSizePx.y + yTxtOffset;

            txtSizePx.y = 15.0f;

            // Disable UI rendering when taking screenshots
            if (getIsProfilerDrawing())
            {
                for (uint32_t i = 0; i < gThreadCount + 1; ++i)
                {
                    if (i == 0)
                    {
                        snprintf(gParticleThreadText, 64, "GPU Main Thread - %f ms", getGpuProfileAvgTime(gGpuProfiletokens[i]));
                    }
                    else
                    {
                        snprintf(gParticleThreadText, 64, "GPU Particle Thread %u - %f ms", i - 1,
                                 getGpuProfileAvgTime(gGpuProfiletokens[i]));
                    }
                    gFrameTimeDraw.pText = gParticleThreadText;
                    cmdDrawTextWithFont(cmd, float2(xTxtOffset, yTxtOrig), &gFrameTimeDraw);
                    yTxtOrig += txtSizePx.y + yTxtOffset;
                }
            }

            cmdDrawUserInterface(cmd);
            cmdEndDebugMarker(cmd);

            cmdEndGpuFrameProfile(cmd, gGpuProfiletokens[0]); // pGpuProfiletokens[0] is reserved for main thread
            endCmd(cmd);

            beginCmd(graphCmd);

            if (getIsProfilerDrawing() && bShowThreadsPlot)
            {
                cmdBeginDebugMarker(graphCmd, 0, 1, 0, "Draw Graph");

                BindRenderTargetsDesc bindDesc = {};
                bindDesc.mRenderTargetCount = 1;
                bindDesc.mRenderTargets[0] = { pUIRenderTarget, LOAD_ACTION_LOAD };
                cmdBindRenderTargets(graphCmd, &bindDesc);

                gGraphWidth = pUIRenderTarget->mWidth / 6;
                gGraphHeight = (pUIRenderTarget->mHeight - 30 - gCoresCount * 10) / gCoresCount;

                for (uint i = 0; i < gCoresCount; ++i)
                {
                    pCpuGraph[i].mViewPort.mOffsetX = pUIRenderTarget->mWidth - 10.0f - gGraphWidth;
                    pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
                    pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
                    pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;

                    cmdSetViewport(graphCmd, pCpuGraph[i].mViewPort.mOffsetX, pCpuGraph[i].mViewPort.mOffsetY,
                                   pCpuGraph[i].mViewPort.mWidth, pCpuGraph[i].mViewPort.mHeight, 0.0f, 1.0f);
                    cmdSetScissor(graphCmd, 0, 0, pUIRenderTarget->mWidth, pUIRenderTarget->mHeight);

                    const uint32_t graphDataStride = sizeof(GraphVertex); // vec2(position) + vec4(color)

                    cmdBindPipeline(graphCmd, pGraphTrianglePipeline);
                    cmdBindVertexBuffer(graphCmd, 1, &pBackGroundVertexBuffer[frameIdx], &graphDataStride, NULL);
                    cmdDraw(graphCmd, 4, 0);

                    cmdBindPipeline(graphCmd, pGraphLineListPipeline);
                    cmdBindVertexBuffer(graphCmd, 1, &pBackGroundVertexBuffer[frameIdx], &graphDataStride, NULL);
                    cmdDraw(graphCmd, 38, 4);

                    cmdBindPipeline(graphCmd, pGraphTrianglePipeline);
                    cmdBindVertexBuffer(graphCmd, 1, &(pCpuGraph[i].mVertexBuffer[frameIdx]), &graphDataStride, NULL);
                    cmdDraw(graphCmd, 2 * gSampleCount, 0);

                    cmdBindPipeline(graphCmd, pGraphLinePipeline);
                    cmdBindVertexBuffer(graphCmd, 1, &pCpuGraph[i].mVertexBuffer[frameIdx], &graphDataStride, NULL);
                    cmdDraw(graphCmd, gSampleCount, 2 * gSampleCount);
                }
                cmdBindRenderTargets(graphCmd, NULL);

                cmdEndDebugMarker(graphCmd);
            }
        }
        cmdEndDrawingUserInterface(graphCmd, pSwapChain);

        barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(graphCmd, 0, NULL, 0, NULL, 1, &barrier);
        endCmd(graphCmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        // wait all particle threads done
        threadSystemWaitIdle(gThreadSystem);
        /***************draw cpu graph*****************************/
        /***************draw cpu graph*****************************/
        // gather all command buffer, it is important to keep the screen clean command at the beginning
        uint32_t cmdCount = gThreadCount + 2;
        Cmd*     allCmds[gMaxThreadCount + 2] = {};
        allCmds[0] = cmd;

        for (uint32_t i = 0; i < gThreadCount; ++i)
        {
            allCmds[i + 1] = gThreadData[i].pCmd;
        }
        allCmds[gThreadCount + 1] = graphCmd;

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = cmdCount;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = allCmds;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() override { return "03_MultiThread"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_2D_VR_LAYER;
        swapChainDesc.mVR.m2DLayer = gVR2DLayer;

        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc setDesc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPersistent);
        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);
        setDesc = SRT_SET_DESC(SrtData, PerDraw, gDataBufferCount * gMaxThreadCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerDraw);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPersistent);
    }

    void addShaders()
    {
        ShaderLoadDesc graphShader = {};
        graphShader.mVert.pFileName = "Graph.vert";
        graphShader.mFrag.pFileName = "Graph.frag";

        ShaderLoadDesc particleShader = {};
        particleShader.mVert.pFileName = "Particle.vert";
        particleShader.mFrag.pFileName = "Particle.frag";

        ShaderLoadDesc skyShader = {};
        skyShader.mVert.pFileName = "Skybox.vert";
        skyShader.mFrag.pFileName = "Skybox.frag";

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
        // vertexlayout and pipeline for particles
        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
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
        blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateDesc.mIndependentBlend = false;

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        PipelineDesc graphicsPipelineDesc = {};
        PIPELINE_LAYOUT_DESC(graphicsPipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL,
                             SRT_LAYOUT_DESC(SrtData, PerDraw));
        graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_POINT_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pBlendState = &blendStateDesc;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pShaderProgram = pShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pPipeline);

        // layout and pipeline for skybox draw
        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
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
        pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pSkyBoxDrawPipeline);

        /********** layout and pipeline for graph draw*****************/
        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_COLOR;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 4 * sizeof(float);

        pipelineSettings = { 0 };
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_LINE_STRIP;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
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
        DescriptorData params[8] = {};
        params[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gRightText);
        params[0].ppTextures = &pSkyBoxTextures[0];
        params[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gLeftText);
        params[1].ppTextures = &pSkyBoxTextures[1];
        params[2].mIndex = SRT_RES_IDX(SrtData, Persistent, gTopText);
        params[2].ppTextures = &pSkyBoxTextures[2];
        params[3].mIndex = SRT_RES_IDX(SrtData, Persistent, gBotText);
        params[3].ppTextures = &pSkyBoxTextures[3];
        params[4].mIndex = SRT_RES_IDX(SrtData, Persistent, gFrontText);
        params[4].ppTextures = &pSkyBoxTextures[4];
        params[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gBackText);
        params[5].ppTextures = &pSkyBoxTextures[5];
        params[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gSampler);
        params[6].ppSamplers = &pSampler;
        params[7].mIndex = SRT_RES_IDX(SrtData, Persistent, gTexture);
        params[7].mCount = sizeof(pImageFileNames) / sizeof(pImageFileNames[0]);
        params[7].ppTextures = pTextures;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPersistent, 8, params);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0] = {};
            params[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gUniformBlock);
            params[0].ppBuffers = &pUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, 1, params);
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0] = {};
            params[0].mIndex = SRT_RES_IDX(SrtData, PerDraw, gParticleConstants);

            for (uint32_t j = 0; j < gMaxThreadCount; j++)
            {
                params[0].ppBuffers = &pPerDrawBuffers[i][j];
                updateDescriptorSet(pRenderer, i * gMaxThreadCount + j, pDescriptorSetPerDraw, 1, params);
            }
        }
    }

    void CalCpuUsage()
    {
        if (!gPerformanceStatsInited)
            return;

        PerformanceStats cpuStats = getPerformanceStats();
        for (uint32_t i = 0; i < gCoresCount; i++)
        {
            if (cpuStats.mCoreUsagePercentage[i] >= 0.0f && cpuStats.mCoreUsagePercentage[i] <= 100.0f)
                pCoresLoadData[i] = cpuStats.mCoreUsagePercentage[i];
        }
    }

    int InitCpuUsage()
    {
        gCoresCount = getNumCPUCores();

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

        if (initPerformanceStats(PERFORMANCE_STATS_FLAG_CORE_USAGE_PERCENTAGE) < 0)
            return 0;

        CalCpuUsage();
        return 1;
    }

    void ExitCpuUsage()
    {
        tf_free(pCpuData);
        tf_free(pCoresLoadData);

        if (gPerformanceStatsInited)
            exitPerformanceStats();
    }

    void CpuGraphBackGroundUpdate(uint32_t frameIdx)
    {
        BufferUpdateDesc backgroundVbUpdate = { pBackGroundVertexBuffer[frameIdx] };
        backgroundVbUpdate.mCurrentState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        beginUpdateResource(&backgroundVbUpdate);
        GraphVertex* backGroundPoints = (GraphVertex*)backgroundVbUpdate.pMappedData;
        memset(backgroundVbUpdate.pMappedData, 0, pBackGroundVertexBuffer[frameIdx]->mSize);

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
        // start from 42

        backGroundPoints[42].mPosition = vec2(-1.0f, -1.0f);
        backGroundPoints[42].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
        backGroundPoints[43].mPosition = vec2(1.0f, -1.0f);
        backGroundPoints[43].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
        backGroundPoints[44].mPosition = vec2(-1.0f, 1.0f);
        backGroundPoints[44].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);
        backGroundPoints[45].mPosition = vec2(1.0f, 1.0f);
        backGroundPoints[45].mColor = vec4(0.85f, 0.0f, 0.0f, 0.25f);

        endUpdateResource(&backgroundVbUpdate);
    }

    void CpuGraphcmdUpdateBuffer(uint32_t frameIdx, CpuGraphData* graphData, CpuGraph* graph)
    {
        BufferUpdateDesc vbUpdate = { graph->mVertexBuffer[frameIdx] };
        vbUpdate.mCurrentState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        beginUpdateResource(&vbUpdate);
        GraphVertex* points = (GraphVertex*)vbUpdate.pMappedData;
        memset(vbUpdate.pMappedData, 0, graph->mVertexBuffer[frameIdx]->mSize);

        int index = graphData->mSampleIdx;
        // fill up tri vertex
        for (uint32_t i = 0; i < gSampleCount; ++i)
        {
            if (--index < 0)
                index = gSampleCount - 1;
            points[i * 2].mPosition = vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f, -0.97f);
            points[i * 2].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
            points[i * 2 + 1].mPosition =
                vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
                     (2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
            points[i * 2 + 1].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
        }

        // line vertex
        index = graphData->mSampleIdx;
        for (uint32_t i = 0; i < gSampleCount; ++i)
        {
            if (--index < 0)
                index = gSampleCount - 1;
            points[i + 2 * gSampleCount].mPosition =
                vec2((1.0f - i * (2.0f / gSampleCount)) * 0.999f - 0.02f,
                     (2.0f * ((graphData->mSample[index] + graphData->mSampley[index]) * graphData->mScale - 0.5f)) * 0.97f);
            points[i + 2 * gSampleCount].mColor = vec4(0.0f, 0.85f, 0.0f, 1.0f);
        }

        endUpdateResource(&vbUpdate);
    }

    // thread for recording particle draw
    static void ParticleThreadDraw(void* pData, uint64_t)
    {
        ThreadData& data = *(ThreadData*)pData;
        if (data.mThreadID == initialThread)
            data.mThreadID = getCurrentThreadID();

        BufferUpdateDesc update = { pPerDrawBuffers[data.mFrameIndex][data.mThreadIndex] };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, &gParticleData, sizeof(gParticleData));
        endUpdateResource(&update);
        // PROFILER_SET_CPU_SCOPE("Threads", "Cpu draw", 0xffffff);
        Cmd* cmd = data.pCmd;
        resetCmdPool(pRenderer, data.pCmdPool);
        beginCmd(cmd);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, data.mFrameIndex, pDescriptorSetPerFrame);
        cmdBeginGpuFrameProfile(cmd, gGpuProfiletokens[data.mThreadIndex + 1], false); // pGpuProfiletokens[0] is reserved for main thread
        char buffer[32] = {};
        snprintf(buffer, TF_ARRAY_COUNT(buffer), "Particle Thread Cmd %d", data.mThreadIndex);
        cmdBeginDebugMarker(cmd, 0.6f, 0.7f, 0.8f, buffer);
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { data.pRenderTarget, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)data.pRenderTarget->mWidth, (float)data.pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, data.pRenderTarget->mWidth, data.pRenderTarget->mHeight);
        const uint32_t parDataStride = sizeof(uint32_t);
        cmdBindPipeline(cmd, pPipeline);
        cmdBindDescriptorSet(cmd, data.mFrameIndex * gMaxThreadCount + data.mThreadIndex, pDescriptorSetPerDraw);
        cmdBindVertexBuffer(cmd, 1, &pParticleVertexBuffer, &parDataStride, NULL);
        cmdDrawInstanced(cmd, data.mDrawCount, data.mStartPoint, 1, 0);
        cmdEndDebugMarker(cmd);
        cmdEndGpuFrameProfile(cmd, gGpuProfiletokens[data.mThreadIndex + 1]); // pGpuProfiletokens[0] is reserved for main thread
        endCmd(cmd);
        updatePerformanceStats();
    }
};

DEFINE_APPLICATION_MAIN(MultiThread)
