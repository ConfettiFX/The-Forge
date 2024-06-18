/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

#include "../../../../Common_3/Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_query.h"

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
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

// startdust hash function, use this to generate all the seed and update the position of all particles
#define RND_GEN(x)                  ((x) = (x)*196314165 + 907633515)

#define MAX_CORES                   64
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

int      gTotalParticleCount = 2000000;
uint32_t gGraphWidth = 200;
uint32_t gGraphHeight = 100;

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
Buffer*        pProjViewUniformBuffer[gDataBufferCount] = { NULL };
Buffer*        pSkyboxUniformBuffer[gDataBufferCount] = { NULL };
Buffer*        pSkyBoxVertexBuffer = NULL;
Buffer*        pBackGroundVertexBuffer[gDataBufferCount] = { NULL };
Pipeline*      pPipeline = NULL;
Pipeline*      pSkyBoxDrawPipeline = NULL;
Pipeline*      pGraphLinePipeline = NULL;
Pipeline*      pGraphLineListPipeline = NULL;
Pipeline*      pGraphTrianglePipeline = NULL;
RootSignature* pRootSignature = NULL;
RootSignature* pGraphRootSignature = NULL;
DescriptorSet* pDescriptorSet = NULL;
DescriptorSet* pDescriptorSetUniforms = NULL;
Texture*       pTextures[5];
Texture*       pSkyBoxTextures[6];
Sampler*       pSampler = NULL;
Sampler*       pSamplerSkyBox = NULL;
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
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

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

        gGraphWidth = mSettings.mWidth / 6; // 200;
        gGraphHeight = gCoresCount ? (mSettings.mHeight - 30 - gCoresCount * 10) / gCoresCount : 0;

        // DirectX 11 not supported on this unit test
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

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
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        GpuCmdRingDesc threadCmdRingDesc = {};
        threadCmdRingDesc.pQueue = pGraphicsQueue;
        threadCmdRingDesc.mPoolCount = gThreadCount * gDataBufferCount;
        threadCmdRingDesc.mCmdPerPoolCount = 1;
        threadCmdRingDesc.mAddSyncPrimitives = false;
        addGpuCmdRing(pRenderer, &threadCmdRingDesc, &gThreadCmdRing);

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
        ubDesc.mDesc.mSize = sizeof(CameraMatrix);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
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
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);
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
        DropdownWidget ddTestScripts;
        ddTestScripts.pData = &gCurrentScriptIndex;
        ddTestScripts.pNames = gTestScripts;
        ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);

        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

        ButtonWidget bRunScript;
        UIWidget*    pRunScript = uiCreateComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
        luaRegisterWidget(pRunScript);

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
        inputDesc.pJoystickTexture = "circlepad.tex";
        if (!initInputSystem(&inputDesc))
            return false;

        // App Actions
        InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       this };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           requestShutdown();
                           return true;
                       } };
        addInputAction(&actionDesc);
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }
            return true;
        };

        typedef bool (*CameraInputHandler)(InputActionContext * ctx, DefaultInputActions::DefaultInputAction action);
        static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
        {
            if (*(ctx->pCaptured))
            {
                float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
                switch (action)
                {
                case DefaultInputActions::ROTATE_CAMERA:
                    pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    pCameraController->onMoveY(delta[0]);
                    break;
                default:
                    break;
                }
            }
            return true;
        };
        actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                       [](InputActionContext* ctx)
                       {
                           setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                           return true;
                       },
                       NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           if (!uiWantTextInput())
                               pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;

        return true;
    }

    void Exit() override
    {
        exitInputSystem();
        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);
        exitCameraController(pCameraController);

        ExitCpuUsage();

        tf_free(gSeedArray);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pProjViewUniformBuffer[i]);
            removeResource(pSkyboxUniformBuffer[i]);
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
        removeSampler(pRenderer, pSamplerSkyBox);

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeGpuCmdRing(pRenderer, &gThreadCmdRing);

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeQueue(pRenderer, pGraphicsQueue);
        exitResourceLoaderInterface(pRenderer);
        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc) override
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

        initScreenshotInterface(pRenderer, pGraphicsQueue);

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
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime) override
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
        cmdBeginGpuFrameProfile(cmd, gGpuProfiletokens[0]); // pGpuProfiletokens[0] is reserved for main thread

        BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
        beginUpdateResource(&viewProjCbv);
        memcpy(viewProjCbv.pMappedData, &gProjectView, sizeof(gProjectView));
        endUpdateResource(&viewProjCbv);

        BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex] };
        beginUpdateResource(&skyboxViewProjCbv);
        memcpy(skyboxViewProjCbv.pMappedData, &gSkyboxProjectView, sizeof(gSkyboxProjectView));
        endUpdateResource(&skyboxViewProjCbv);

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

        txtSizePx = cmdDrawGpuProfile(cmd, float2(xTxtOffset, yTxtOrig), gGpuProfiletokens[0], &gFrameTimeDraw);
        yTxtOrig += txtSizePx.y + yTxtOffset;

        txtSizePx.y = 15.0f;

        // Disable UI rendering when taking screenshots
        if (uiIsRenderingEnabled())
        {
            for (uint32_t i = 0; i < gThreadCount + 1; ++i)
            {
                if (i == 0)
                {
                    snprintf(gParticleThreadText, 64, "GPU Main Thread - %f ms", getGpuProfileAvgTime(gGpuProfiletokens[i]));
                }
                else
                {
                    snprintf(gParticleThreadText, 64, "GPU Particle Thread %u - %f ms", i - 1, getGpuProfileAvgTime(gGpuProfiletokens[i]));
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

        Cmd* graphCmd = elem.pCmds[1];
        beginCmd(graphCmd);
        if (uiIsRenderingEnabled() && bShowThreadsPlot)
        {
            cmdBeginDebugMarker(graphCmd, 0, 1, 0, "Draw Graph");

            for (uint i = 0; i < gCoresCount; ++i)
            {
                gGraphWidth = pRenderTarget->mWidth / 6;
                gGraphHeight = (pRenderTarget->mHeight - 30 - gCoresCount * 10) / gCoresCount;
                pCpuGraph[i].mViewPort.mOffsetX = pRenderTarget->mWidth - 10.0f - gGraphWidth;
                pCpuGraph[i].mViewPort.mWidth = (float)gGraphWidth;
                pCpuGraph[i].mViewPort.mOffsetY = 36 + i * (gGraphHeight + 4.0f);
                pCpuGraph[i].mViewPort.mHeight = (float)gGraphHeight;

                BindRenderTargetsDesc bindDesc = {};
                bindDesc.mRenderTargetCount = 1;
                bindDesc.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
                cmdBindRenderTargets(graphCmd, &bindDesc);
                cmdSetViewport(graphCmd, pCpuGraph[i].mViewPort.mOffsetX, pCpuGraph[i].mViewPort.mOffsetY, pCpuGraph[i].mViewPort.mWidth,
                               pCpuGraph[i].mViewPort.mHeight, 0.0f, 1.0f);
                cmdSetScissor(graphCmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

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

            cmdEndDebugMarker(graphCmd);
        }

        cmdBindRenderTargets(graphCmd, NULL);

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
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSet);
        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2 };
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
        graphShader.mStages[0].pFileName = "Graph.vert";
        graphShader.mStages[1].pFileName = "Graph.frag";

        ShaderLoadDesc particleShader = {};
        particleShader.mStages[0].pFileName = "Particle.vert";
        particleShader.mStages[1].pFileName = "Particle.frag";

        ShaderLoadDesc skyShader = {};
        skyShader.mStages[0].pFileName = "Skybox.vert";
        skyShader.mStages[1].pFileName = "Skybox.frag";

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
        pipelineSettings.pRootSignature = pRootSignature;
        pipelineSettings.pShaderProgram = pSkyBoxDrawShader;
        pipelineSettings.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pSkyBoxDrawPipeline);

        /********** layout and pipeline for graph draw*****************/
        vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 2;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat =
            (sizeof(GraphVertex) > 24 ? TinyImageFormat_R32G32B32A32_SFLOAT
                                      : TinyImageFormat_R32G32_SFLOAT); // Handle the case when padding is added to the struct (yielding 32
                                                                        // bytes instead of 24) on macOS
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

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0] = {};
            params[0].pName = "uniformBlock";
            params[0].ppBuffers = &pSkyboxUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 0, pDescriptorSetUniforms, 1, params);
            params[0].ppBuffers = &pProjViewUniformBuffer[i];
            updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSetUniforms, 1, params);
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
        // PROFILER_SET_CPU_SCOPE("Threads", "Cpu draw", 0xffffff);
        Cmd* cmd = data.pCmd;
        resetCmdPool(pRenderer, data.pCmdPool);
        beginCmd(cmd);
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
        cmdBindDescriptorSet(cmd, 1, pDescriptorSet);
        cmdBindDescriptorSet(cmd, data.mFrameIndex * 2 + 1, pDescriptorSetUniforms);
        cmdBindPushConstants(cmd, pRootSignature, gParticleRootConstantIndex, &gParticleData);
        cmdBindVertexBuffer(cmd, 1, &pParticleVertexBuffer, &parDataStride, NULL);

        cmdDrawInstanced(cmd, data.mDrawCount, data.mStartPoint, 1, 0);

        cmdEndDebugMarker(cmd);
        cmdEndGpuFrameProfile(cmd, gGpuProfiletokens[data.mThreadIndex + 1]); // pGpuProfiletokens[0] is reserved for main thread
        endCmd(cmd);

        updatePerformanceStats();
    }
};

DEFINE_APPLICATION_MAIN(MultiThread)
