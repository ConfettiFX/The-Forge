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

// Unit Test for distributing heavy gpu workload such as Split Frame Rendering to Multiple GPUs (not necessarily identical)
// GPU 0 Renders Left Eye and finally does a composition pass to present Left and Right eye textures to screen
// GPU 1 Renders Right Eye

#define MAX_PLANETS                  20 // Does not affect test, just for allocating space in uniform block. Must match with shader.
#define MAX_GPU_PROFILER_NAME_LENGTH 128

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Resources/ResourceLoader/TextureContainers.h"
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Middleware_3/PaniniProjection/Panini.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Demo structures
struct PlanetInfoStruct
{
    mat4  mTranslationMat;
    mat4  mScaleMat;
    mat4  mSharedMat; // Matrix to pass down to children
    vec4  mColor;
    uint  mParentIndex;
    float mYOrbitSpeed; // Rotation speed around parent
    float mZOrbitSpeed;
    float mRotationSpeed; // Rotation speed around self
};

struct UniformBlock
{
    mat4 mProjectView;
    mat4 mToWorldMat[MAX_PLANETS];
    vec4 mColor[MAX_PLANETS];

    // Point Light Information
    vec3 mLightPosition;
    vec3 mLightColor;
};

const uint32_t gDataBufferCount = 3;

const uint32_t gViewCount = 2;

// Number of frames to use between rendering and merging/presentation. Must be < gDataBufferCount
uint32_t gFrameLatency = 2;
uint32_t gBufferedFrames = 0;

// Simulate heavy gpu workload by rendering high resolution spheres
const int   gSphereResolution = 1024; // Increase for higher resolution spheres
const float gSphereDiameter = 0.5f;
const uint  gNumPlanets = 11;     // Sun, Mercury -> Neptune, Pluto, Moon
const uint  gTimeOffset = 600000; // For visually better starting locations
const float gRotSelfScale = 0.0004f;
const float gRotOrbitYScale = 0.001f;
const float gRotOrbitZScale = 0.00001f;

RendererContext* pContext = NULL;
Renderer*        pRenderer[gViewCount] = { NULL };

Queue*        pGraphicsQueue[gViewCount] = { NULL };
GpuCmdRing    gGraphicsCmdRing[gViewCount] = {};
Buffer*       pSphereVertexBuffer[gViewCount] = { NULL };
Buffer*       pSkyBoxVertexBuffer[gViewCount] = { NULL };
Texture*      pSkyBoxTextures[gViewCount][6];
RenderTarget* pRenderTargets[gDataBufferCount][gViewCount] = { { NULL } };
RenderTarget* pDepthBuffers[gViewCount] = { NULL };

Buffer*   pTransferBuffer[gDataBufferCount][gViewCount] = { { NULL } };
Texture*  pRenderResult[gDataBufferCount][gViewCount - 1] = { { NULL } };
SyncToken gReadbackSyncTokens[gDataBufferCount][gViewCount - 1] = { { NULL } };

Semaphore* pImageAcquiredSemaphore = NULL;
SwapChain* pSwapChain = NULL;

Shader*   pSphereShader[gViewCount] = { NULL };
Pipeline* pSpherePipeline[gViewCount] = { NULL };

Shader*        pSkyBoxDrawShader[gViewCount] = { NULL };
Pipeline*      pSkyBoxDrawPipeline[gViewCount] = { NULL };
RootSignature* pRootSignature[gViewCount] = { NULL };
Sampler*       pSamplerSkyBox[gViewCount] = { NULL };
DescriptorSet* pDescriptorSetTexture[gViewCount] = { NULL };
DescriptorSet* pDescriptorSetUniforms[gViewCount] = { NULL };

Buffer* pProjViewUniformBuffer[gDataBufferCount][gViewCount] = { { NULL } };
Buffer* pSkyboxUniformBuffer[gDataBufferCount][gViewCount] = { { NULL } };

uint32_t gFrameIndex = 0;

int              gNumberOfSpherePoints;
UniformBlock     gUniformData;
UniformBlock     gUniformDataSky;
PlanetInfoStruct gPlanetInfoData[gNumPlanets];

ICameraController* pCameraController = NULL;

/// UI
UIComponent* pGui;

const char* pSkyBoxImageFileNames[] = {
    "Skybox_right1.tex", "Skybox_left2.tex", "Skybox_top3.tex", "Skybox_bottom4.tex", "Skybox_front5.tex", "Skybox_back6.tex",
};
char         gGpuProfilerNames[gViewCount + 1][MAX_GPU_PROFILER_NAME_LENGTH]{};
ProfileToken gGpuProfilerTokens[gViewCount + 1];
char         gGpuNames[MAX_MULTIPLE_GPUS][MAX_GPU_PROFILER_NAME_LENGTH]{};
const char*  gGpuNamePtrs[] = {
    gGpuNames[0],
    gGpuNames[1],
    gGpuNames[2],
    gGpuNames[3],
};
COMPILE_ASSERT(sizeof(gGpuNamePtrs) / sizeof(gGpuNamePtrs[0]) == MAX_MULTIPLE_GPUS);

ProfileToken gReadBackCpuToken;
ProfileToken gUploadCpuToken;
ProfileToken gGpuExecCpuToken;

FontDrawDesc     gFrameTimeDraw;
uint32_t         gFontID = 0;
ClearValue       gClearColor; // initialization in Init
ClearValue       gClearDepth;
Panini           gPanini = {};
PaniniParameters gPaniniParams = {};
bool             gMultiGPU = true;
bool             gMultiGPURestart = false;
bool             gMultiGPUCurrent = true;
uint32_t         gSelectedGpuIndices[gViewCount] = { 0, 1 };
float*           pSpherePoints;

uint32_t    gCurrentScriptIndex = 0;
const char* gTestScripts[] = { "Test0.lua" };
#ifdef AUTOMATED_TESTING
// For this UT the test ordering is done manually..
// It is done based of the number of times we reset the APP..
const char* gSwapRendererTestScriptFileName = "Test_SwapRenderers_Swap.lua";
const char* gSwapRendererScreenshotTestScriptFileName = "Test_SwapRenderers_Screenshot.lua";
int32_t     gNumResetsFromTestScripts = 0; // number of times we have reset the app..
#endif

void RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

void SwitchGpuMode(void* pUserData)
{
    UNREF_PARAM(pUserData);
    gMultiGPURestart = true;
    ResetDesc resetDescriptor;
    resetDescriptor.mType = RESET_TYPE_GPU_MODE_SWITCH;
    requestReset(&resetDescriptor);
}

class UnlinkedMultiGPU: public IApp
{
public:
    bool Init()
    {
        gPaniniParams = {}; // always reset panini parameters to default at start

        gMultiGPUCurrent = gMultiGPU;

        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        gClearColor.r = 0.0f;
        gClearColor.g = 0.0f;
        gClearColor.b = 0.0f;
        gClearColor.a = 0.0f;

        gClearDepth.depth = 0.0f;
        gClearDepth.stencil = 0;

        // Generate sphere vertex buffer
        if (!pSpherePoints)
            generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereDiameter);

        // Setup planets (Rotation speeds are relative to Earth's, some values randomly given)
        // Sun
        gPlanetInfoData[0].mParentIndex = 0;
        gPlanetInfoData[0].mYOrbitSpeed = 0; // Earth years for one orbit
        gPlanetInfoData[0].mZOrbitSpeed = 0;
        gPlanetInfoData[0].mRotationSpeed = 24.0f; // Earth days for one rotation
        gPlanetInfoData[0].mTranslationMat = mat4::identity();
        gPlanetInfoData[0].mScaleMat = mat4::scale(vec3(10.0f));
        gPlanetInfoData[0].mColor = vec4(0.97f, 0.38f, 0.09f, 0.0f);

        // Mercury
        gPlanetInfoData[1].mParentIndex = 0;
        gPlanetInfoData[1].mYOrbitSpeed = 0.5f;
        gPlanetInfoData[1].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[1].mRotationSpeed = 58.7f;
        gPlanetInfoData[1].mTranslationMat = mat4::translation(vec3(10.0f, 0, 0));
        gPlanetInfoData[1].mScaleMat = mat4::scale(vec3(1.0f));
        gPlanetInfoData[1].mColor = vec4(0.45f, 0.07f, 0.006f, 1.0f);

        // Venus
        gPlanetInfoData[2].mParentIndex = 0;
        gPlanetInfoData[2].mYOrbitSpeed = 0.8f;
        gPlanetInfoData[2].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[2].mRotationSpeed = 243.0f;
        gPlanetInfoData[2].mTranslationMat = mat4::translation(vec3(20.0f, 0, 5));
        gPlanetInfoData[2].mScaleMat = mat4::scale(vec3(2));
        gPlanetInfoData[2].mColor = vec4(0.6f, 0.32f, 0.006f, 1.0f);

        // Earth
        gPlanetInfoData[3].mParentIndex = 0;
        gPlanetInfoData[3].mYOrbitSpeed = 1.0f;
        gPlanetInfoData[3].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[3].mRotationSpeed = 1.0f;
        gPlanetInfoData[3].mTranslationMat = mat4::translation(vec3(30.0f, 0, 0));
        gPlanetInfoData[3].mScaleMat = mat4::scale(vec3(4));
        gPlanetInfoData[3].mColor = vec4(0.07f, 0.028f, 0.61f, 1.0f);

        // Mars
        gPlanetInfoData[4].mParentIndex = 0;
        gPlanetInfoData[4].mYOrbitSpeed = 2.0f;
        gPlanetInfoData[4].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[4].mRotationSpeed = 1.1f;
        gPlanetInfoData[4].mTranslationMat = mat4::translation(vec3(40.0f, 0, 0));
        gPlanetInfoData[4].mScaleMat = mat4::scale(vec3(3));
        gPlanetInfoData[4].mColor = vec4(0.79f, 0.07f, 0.006f, 1.0f);

        // Jupiter
        gPlanetInfoData[5].mParentIndex = 0;
        gPlanetInfoData[5].mYOrbitSpeed = 11.0f;
        gPlanetInfoData[5].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[5].mRotationSpeed = 0.4f;
        gPlanetInfoData[5].mTranslationMat = mat4::translation(vec3(50.0f, 0, 0));
        gPlanetInfoData[5].mScaleMat = mat4::scale(vec3(8));
        gPlanetInfoData[5].mColor = vec4(0.32f, 0.13f, 0.13f, 1);

        // Saturn
        gPlanetInfoData[6].mParentIndex = 0;
        gPlanetInfoData[6].mYOrbitSpeed = 29.4f;
        gPlanetInfoData[6].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[6].mRotationSpeed = 0.5f;
        gPlanetInfoData[6].mTranslationMat = mat4::translation(vec3(60.0f, 0, 0));
        gPlanetInfoData[6].mScaleMat = mat4::scale(vec3(6));
        gPlanetInfoData[6].mColor = vec4(0.45f, 0.45f, 0.21f, 1.0f);

        // Uranus
        gPlanetInfoData[7].mParentIndex = 0;
        gPlanetInfoData[7].mYOrbitSpeed = 84.07f;
        gPlanetInfoData[7].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[7].mRotationSpeed = 0.8f;
        gPlanetInfoData[7].mTranslationMat = mat4::translation(vec3(70.0f, 0, 0));
        gPlanetInfoData[7].mScaleMat = mat4::scale(vec3(7));
        gPlanetInfoData[7].mColor = vec4(0.13f, 0.13f, 0.32f, 1.0f);

        // Neptune
        gPlanetInfoData[8].mParentIndex = 0;
        gPlanetInfoData[8].mYOrbitSpeed = 164.81f;
        gPlanetInfoData[8].mZOrbitSpeed = 0.0f;
        gPlanetInfoData[8].mRotationSpeed = 0.9f;
        gPlanetInfoData[8].mTranslationMat = mat4::translation(vec3(80.0f, 0, 0));
        gPlanetInfoData[8].mScaleMat = mat4::scale(vec3(8));
        gPlanetInfoData[8].mColor = vec4(0.21f, 0.028f, 0.79f, 1.0f);

        // Pluto - Not a planet XDD
        gPlanetInfoData[9].mParentIndex = 0;
        gPlanetInfoData[9].mYOrbitSpeed = 247.7f;
        gPlanetInfoData[9].mZOrbitSpeed = 1.0f;
        gPlanetInfoData[9].mRotationSpeed = 7.0f;
        gPlanetInfoData[9].mTranslationMat = mat4::translation(vec3(90.0f, 0, 0));
        gPlanetInfoData[9].mScaleMat = mat4::scale(vec3(1.0f));
        gPlanetInfoData[9].mColor = vec4(0.45f, 0.21f, 0.21f, 1.0f);

        // Moon
        gPlanetInfoData[10].mParentIndex = 3;
        gPlanetInfoData[10].mYOrbitSpeed = 1.0f;
        gPlanetInfoData[10].mZOrbitSpeed = 200.0f;
        gPlanetInfoData[10].mRotationSpeed = 27.0f;
        gPlanetInfoData[10].mTranslationMat = mat4::translation(vec3(5.0f, 0, 0));
        gPlanetInfoData[10].mScaleMat = mat4::scale(vec3(1));
        gPlanetInfoData[10].mColor = vec4(0.07f, 0.07f, 0.13f, 1.0f);

        RendererContextDesc contextSettings;
        memset(&contextSettings, 0, sizeof(contextSettings));
        initRendererContext(GetName(), &contextSettings, &pContext);
        if (!pContext || pContext->mGpuCount < 1)
        {
            LOGF(LogLevel::eWARNING, "Unlinked multi GPU is not supported with the selected API Falling back to single GPU mode.");

            RendererDesc settings;
            memset(&settings, 0, sizeof(settings));
            settings.mGpuMode = GPU_MODE_SINGLE;

            initRenderer(GetName(), &settings, &pRenderer[0]);
            // check for init success
            if (!pRenderer[0])
                return false;

            for (uint32_t i = 1; i < gViewCount; ++i)
                pRenderer[i] = pRenderer[0];

            gMultiGPUCurrent = false;
        }
        else
        {
            if (pContext->mGpuCount < 2 && gMultiGPUCurrent)
            {
                LOGF(LogLevel::eWARNING, "The system has only one GPU, Renderers will be created on the same GPU");
            }
            for (uint32_t i = 0; i < gViewCount; ++i)
            {
                if (gSelectedGpuIndices[i] >= pContext->mGpuCount)
                    gSelectedGpuIndices[i] = 0;
            }

            RendererDesc settings;
            memset(&settings, 0, sizeof(settings));
            settings.mGpuMode = gMultiGPUCurrent ? GPU_MODE_UNLINKED : GPU_MODE_SINGLE;
            settings.pContext = pContext;
            for (uint32_t i = 0; i < gViewCount; ++i)
            {
                if (!gMultiGPUCurrent && i > 0)
                {
                    pRenderer[i] = pRenderer[0];
                }
                else
                {
                    settings.mGpuIndex = gSelectedGpuIndices[i];
                    initRenderer(GetName(), &settings, &pRenderer[i]);
                    // check for init success
                    if (!pRenderer[i])
                        return false;
                }
            }
        }

        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;

        initResourceLoaderInterface(pRenderer, rendererCount);

        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            QueueDesc queueDesc = {};
            queueDesc.mType = QUEUE_TYPE_GRAPHICS;
            queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
            queueDesc.mNodeIndex = i;

            if (gMultiGPUCurrent || i == 0)
                addQueue(pRenderer[i], &queueDesc, &pGraphicsQueue[i]);
            else
                pGraphicsQueue[i] = pGraphicsQueue[0];

            uint32_t rendererIndex = gMultiGPUCurrent ? i : 0;
            snprintf(gGpuProfilerNames[i], sizeof(gGpuProfilerNames[gViewCount]), "Graphics %d: GPU %u (%s)", i,
                     gSelectedGpuIndices[rendererIndex], pRenderer[i]->pGpu->mSettings.mGpuVendorPreset.mGpuName);
        }

        snprintf(gGpuProfilerNames[gViewCount], sizeof(gGpuProfilerNames[gViewCount]), "Merge (GPU %u)", gSelectedGpuIndices[0]);

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer[0];
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer[0];
        uiRenderDesc.mFrameCount = gDataBufferCount;
        initUserInterface(&uiRenderDesc);

#ifdef AUTOMATED_TESTING
        const uint32_t numScripts = (sizeof(gTestScripts) / sizeof(gTestScripts[0])) + 1;
#else
        const uint32_t numScripts = (sizeof(gTestScripts) / sizeof(gTestScripts[0]));
#endif
        uint32_t      numScriptsToDefine = numScripts;
        LuaScriptDesc scriptDescs[numScripts] = {};

#ifdef AUTOMATED_TESTING
        if (gNumResetsFromTestScripts == 0)
        {
            // add all non swap renderscripts
            for (uint32_t i = 0; i < numScripts - 1; ++i)
                scriptDescs[i].pScriptFileName = gTestScripts[i];

            // last slot is for render swapping..
            scriptDescs[numScripts - 1].pScriptFileName = gSwapRendererTestScriptFileName;
        }
        else if (gNumResetsFromTestScripts == 1) // we only want to take a screenshot for swapped renderer
        {
            scriptDescs[0].pScriptFileName = gSwapRendererScreenshotTestScriptFileName;
            scriptDescs[1].pScriptFileName = gSwapRendererTestScriptFileName;
        }
        else if (gNumResetsFromTestScripts == 2) // just take a default screenshot again (since first one gets overriden)..
        {
            numScriptsToDefine = 0;
            gNumResetsFromTestScripts = -1;
        }
#endif
        luaDefineScripts(scriptDescs, numScriptsToDefine);

        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);
        uiCreateComponent(GetName(), &guiDesc, &pGui);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer[0];
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            gGpuProfilerTokens[i] = addGpuProfiler(pRenderer[i], pGraphicsQueue[i], gGpuProfilerNames[i]);
        }
        gGpuProfilerTokens[gViewCount] = addGpuProfiler(pRenderer[0], pGraphicsQueue[0], gGpuProfilerNames[gViewCount]);
        gReadBackCpuToken = getCpuProfileToken("Stalls", "Reading results", 0xff00ffff);
        gUploadCpuToken = getCpuProfileToken("Stalls", "Upload results", 0xff00ffff);
        gGpuExecCpuToken = getCpuProfileToken("Stalls", "Gpu execution", 0xff00ffff);

        for (uint32_t j = 0; j < gViewCount; ++j)
        {
            GpuCmdRingDesc cmdRingDesc = {};
            cmdRingDesc.pQueue = pGraphicsQueue[j];
            cmdRingDesc.mPoolCount = gDataBufferCount;
            cmdRingDesc.mCmdPerPoolCount = j == 0 ? 2 : 1;
            cmdRingDesc.mAddSyncPrimitives = true;
            addGpuCmdRing(pRenderer[j], &cmdRingDesc, &gGraphicsCmdRing[j]);
        }

        addSemaphore(pRenderer[0], &pImageAcquiredSemaphore);

        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (!gMultiGPUCurrent && i > 0)
            {
                pSamplerSkyBox[i] = pSamplerSkyBox[0];
            }
            else
            {
                SamplerDesc samplerDesc = { FILTER_LINEAR,
                                            FILTER_LINEAR,
                                            MIPMAP_MODE_NEAREST,
                                            ADDRESS_MODE_CLAMP_TO_EDGE,
                                            ADDRESS_MODE_CLAMP_TO_EDGE,
                                            ADDRESS_MODE_CLAMP_TO_EDGE };
                addSampler(pRenderer[i], &samplerDesc, &pSamplerSkyBox[i]);
            }
        }

        uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
        BufferLoadDesc sphereVbDesc = {};
        sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        sphereVbDesc.mDesc.mSize = sphereDataSize;
        sphereVbDesc.pData = pSpherePoints;

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

        TextureLoadDesc textureDesc = {};

        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            textureDesc.mNodeIndex = view;

            for (int i = 0; i < 6; ++i)
            {
                textureDesc.pFileName = pSkyBoxImageFileNames[i];
                textureDesc.ppTexture = &pSkyBoxTextures[view][i];
                // Textures representing color should be stored in SRGB or HDR format
                textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;

                addResource(&textureDesc, NULL);
            }

            sphereVbDesc.mDesc.mNodeIndex = view;
            sphereVbDesc.ppBuffer = &pSphereVertexBuffer[view];

            skyboxVbDesc.mDesc.mNodeIndex = view;
            skyboxVbDesc.ppBuffer = &pSkyBoxVertexBuffer[view];

            addResource(&sphereVbDesc, NULL);
            addResource(&skyboxVbDesc, NULL);
        }

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(UniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            for (uint32_t j = 0; j < rendererCount; ++j)
            {
                ubDesc.mDesc.mNodeIndex = j;
                ubDesc.ppBuffer = &pProjViewUniformBuffer[i][j];
                addResource(&ubDesc, NULL);
                ubDesc.ppBuffer = &pSkyboxUniformBuffer[i][j];
                addResource(&ubDesc, NULL);
            }
        }

        waitForAllResourceLoads();

        if (!gMultiGPUCurrent)
        {
            for (uint32_t view = 1; view < gViewCount; ++view)
            {
                for (int i = 0; i < 6; ++i)
                    pSkyBoxTextures[view][i] = pSkyBoxTextures[0][i];
                pSphereVertexBuffer[view] = pSphereVertexBuffer[0];
                pSkyBoxVertexBuffer[view] = pSkyBoxVertexBuffer[0];
                for (uint32_t i = 0; i < gDataBufferCount; ++i)
                {
                    pProjViewUniformBuffer[i][view] = pProjViewUniformBuffer[i][0];
                    pSkyboxUniformBuffer[i][view] = pSkyboxUniformBuffer[i][0];
                }
            }
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            for (uint32_t j = 0; j < gViewCount - 1; ++j)
                gReadbackSyncTokens[i][j] = 0;
        }

        // Multi gpu controls
        if (pContext)
        {
            CheckboxWidget multiGpuCheckbox;
            multiGpuCheckbox.pData = &gMultiGPU;
            UIWidget* pMultiRendererToggle =
                uiCreateComponentWidget(pGui, "Enable Multi Renderer", &multiGpuCheckbox, WIDGET_TYPE_CHECKBOX);
            uiSetWidgetOnEditedCallback(pMultiRendererToggle, nullptr, SwitchGpuMode);
            luaRegisterWidget(pMultiRendererToggle);

            for (uint32_t i = 0; i < pContext->mGpuCount; ++i)
            {
                snprintf(gGpuNames[i], sizeof(gGpuNames), "GPU %u : %s", i, pContext->mGpus[i].mSettings.mGpuVendorPreset.mGpuName); //-V512
            }

            char renderer[20] = {};
            for (uint32_t i = 0; i < rendererCount; ++i)
            {
                DropdownWidget ddGpuSelect;
                ddGpuSelect.pData = &gSelectedGpuIndices[i];
                ddGpuSelect.pNames = gGpuNamePtrs;
                ddGpuSelect.mCount = pContext->mGpuCount;

                snprintf(renderer, 20, "Renderer %d", i);
                UIWidget* pGpuSelect = uiCreateComponentWidget(pGui, renderer, &ddGpuSelect, WIDGET_TYPE_DROPDOWN);
                uiSetWidgetOnEditedCallback(pGpuSelect, nullptr, SwitchGpuMode);
                luaRegisterWidget(pGpuSelect);
            }

#ifdef AUTOMATED_TESTING
            ButtonWidget btnSwapRenderers;
            UIWidget*    pSwapRenderers = uiCreateComponentWidget(pGui, "Swap Renderers", &btnSwapRenderers, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pSwapRenderers, this,
                                        [](void* pUserData)
                                        {
                                            UNREF_PARAM(pUserData);
                                            uint32_t temporary = gSelectedGpuIndices[0];
                                            gSelectedGpuIndices[0] = gSelectedGpuIndices[1];
                                            gSelectedGpuIndices[1] = temporary;

                                            ResetDesc resetDescriptor;
                                            resetDescriptor.mType = RESET_TYPE_GRAPHIC_CARD_SWITCH;
                                            requestReset(&resetDescriptor);
                                        });
            luaRegisterWidget(pSwapRenderers);
#endif
        }
        SliderUintWidget frameLatencyWidget;
        frameLatencyWidget.mMin = 0;
        frameLatencyWidget.mMax = gDataBufferCount - 1;
        frameLatencyWidget.pData = &gFrameLatency;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Frame Latency", &frameLatencyWidget, WIDGET_TYPE_SLIDER_UINT));

        SliderFloatWidget camHorFovSlider;
        camHorFovSlider.pData = &gPaniniParams.FoVH;
        camHorFovSlider.mMin = 30.0f;
        camHorFovSlider.mMax = 179.0f;
        camHorFovSlider.mStep = 1.0f;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Camera Horizontal FoV", &camHorFovSlider, WIDGET_TYPE_SLIDER_FLOAT));

        SliderFloatWidget paniniSliderD;
        paniniSliderD.pData = &gPaniniParams.D;
        paniniSliderD.mMin = 0.0f;
        paniniSliderD.mMax = 1.0f;
        paniniSliderD.mStep = 0.001f;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Panini D Parameter", &paniniSliderD, WIDGET_TYPE_SLIDER_FLOAT));

        SliderFloatWidget paniniSliderS;
        paniniSliderS.pData = &gPaniniParams.S;
        paniniSliderS.mMin = 0.0f;
        paniniSliderS.mMax = 1.0f;
        paniniSliderS.mStep = 0.001f;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Panini S Parameter", &paniniSliderS, WIDGET_TYPE_SLIDER_FLOAT));

        SliderFloatWidget screenScaleSlider;
        screenScaleSlider.pData = &gPaniniParams.scale;
        screenScaleSlider.mMin = 1.0f;
        screenScaleSlider.mMax = 10.0f;
        screenScaleSlider.mStep = 0.01f;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Screen Scale", &screenScaleSlider, WIDGET_TYPE_SLIDER_FLOAT));

        DropdownWidget ddTestScripts;
        ddTestScripts.pData = &gCurrentScriptIndex;
        ddTestScripts.pNames = gTestScripts;
        ddTestScripts.mCount = numScriptsToDefine;
        luaRegisterWidget(uiCreateComponentWidget(pGui, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

        ButtonWidget bRunScript;
        UIWidget*    pRunScript = uiCreateComponentWidget(pGui, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
        luaRegisterWidget(pRunScript);

        if (!gPanini.Init(pRenderer[0]))
            return false;

        gPanini.SetMaxDraws(gDataBufferCount * 2);

        CameraMotionParameters cmp{ 160.0f, 600.0f, 600.0f };
        vec3                   camPos{ 48.0f, 48.0f, 20.0f };
        vec3                   lookAt{ 0 };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(cmp);

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer[0];
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
        gMultiGPURestart = false;

        return true;
    }

    void Exit()
    {
#ifdef AUTOMATED_TESTING
        // Exit is only called whenever SwapRenderer.lua script gets executed or API gets switched
        // In case of API switch we can ignore (it will already be -1)
        // In case if SwapRenderer, this decides what scripts get executed...
        gNumResetsFromTestScripts++;
#endif
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;

        exitInputSystem();

        exitCameraController(pCameraController);

        exitProfiler();

        gPanini.Exit();

        exitUserInterface();

        exitFontSystem();

        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                removeResource(pProjViewUniformBuffer[i][view]);
                removeResource(pSkyboxUniformBuffer[i][view]);
            }

            removeResource(pSphereVertexBuffer[view]);
            removeResource(pSkyBoxVertexBuffer[view]);

            for (uint i = 0; i < 6; ++i)
                removeResource(pSkyBoxTextures[view][i]);

            removeSampler(pRenderer[view], pSamplerSkyBox[view]);
        }

        for (uint32_t j = 0; j < gViewCount; ++j)
        {
            removeGpuCmdRing(pRenderer[j], &gGraphicsCmdRing[j]);
        }

        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            removeQueue(pRenderer[view], pGraphicsQueue[view]);
        }

        removeSemaphore(pRenderer[0], pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer, rendererCount);

        for (uint32_t view = 0; view < gViewCount; ++view)
        {
            if (view < rendererCount)
                exitRenderer(pRenderer[view]);
            pRenderer[view] = NULL;
        }

        if (pContext)
        {
            exitRendererContext(pContext);
            pContext = NULL;
        }

        gBufferedFrames = 0;

        if (!gMultiGPURestart)
        {
            // Need to free memory;
            tf_free(pSpherePoints);
            pSpherePoints = 0;
        }
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        gFrameIndex = 0;

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

            if (!addDepthBuffer())
                return false;

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                for (uint32_t view = 0; view < gViewCount; ++view)
                {
                    if (view == 0 || !gMultiGPUCurrent)
                        gPanini.SetSourceTexture(pRenderTargets[i][view]->pTexture, i * gViewCount + view);
                    else
                        gPanini.SetSourceTexture(pRenderResult[i][view - 1], i * gViewCount + view);
                }
            }
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

        initScreenshotInterface(pRenderer[0], pGraphicsQueue[0]);

        memset(gReadbackSyncTokens, 0, sizeof(gReadbackSyncTokens));
        gFrameIndex = 0;

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;

        for (uint32_t i = 0; i < rendererCount; ++i)
            waitQueueIdle(pGraphicsQueue[i]);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        // Must flush resource loader queues as well.
        waitForAllResourceLoads();
        waitCopyQueueIdle();

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer[0], pSwapChain);

            for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
                for (uint32_t i = 0; i < gViewCount; ++i)
                {
                    removeRenderTarget(pRenderer[i], pRenderTargets[frameIdx][i]);
                    if (gMultiGPUCurrent && i > 0)
                    {
                        removeResource(pRenderResult[frameIdx][i - 1]);
                        removeResource(pTransferBuffer[frameIdx][i]);
                    }
                }

            for (uint32_t i = 0; i < gViewCount; ++i)
                removeRenderTarget(pRenderer[i], pDepthBuffers[i]);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mHeight, mSettings.mHeight);
        /************************************************************************/
        // Update GUI
        /************************************************************************/
        static uint32_t frameLatencyCurrent = gFrameLatency;
        if (gFrameLatency != frameLatencyCurrent)
        {
            ResetFrameLatency();
            frameLatencyCurrent = gFrameLatency;
        }

        pCameraController->update(deltaTime);
        /************************************************************************/
        // Scene Update
        /************************************************************************/
        static float currentTime = 0.0f;
        currentTime += deltaTime * 1000.0f;

        // update camera with time
        mat4        viewMat = pCameraController->getViewMatrix();
        const float aspectInverse = (float)mSettings.mHeight / ((float)mSettings.mWidth * 0.5f);
        const float horizontal_fov = gPaniniParams.FoVH * PI / 180.0f;
        mat4        projMat = mat4::perspectiveLH_ReverseZ(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
        gUniformData.mProjectView = projMat * viewMat;

        // point light parameters
        gUniformData.mLightPosition = vec3(0, 0, 0);
        gUniformData.mLightColor = vec3(0.9f, 0.9f, 0.7f); // Pale Yellow

        // update planet transformations
        for (int i = 0; i < gNumPlanets; i++)
        {
            mat4 rotSelf, rotOrbitY, rotOrbitZ, trans, scale, parentMat;
            rotSelf = rotOrbitY = rotOrbitZ = parentMat = mat4::identity();
            if (gPlanetInfoData[i].mRotationSpeed > 0.0f)
                rotSelf = mat4::rotationY(gRotSelfScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mRotationSpeed);
            if (gPlanetInfoData[i].mYOrbitSpeed > 0.0f)
                rotOrbitY = mat4::rotationY(gRotOrbitYScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mYOrbitSpeed);
            if (gPlanetInfoData[i].mZOrbitSpeed > 0.0f)
                rotOrbitZ = mat4::rotationZ(gRotOrbitZScale * (currentTime + gTimeOffset) / gPlanetInfoData[i].mZOrbitSpeed);
            if (gPlanetInfoData[i].mParentIndex > 0)
                parentMat = gPlanetInfoData[gPlanetInfoData[i].mParentIndex].mSharedMat;

            trans = gPlanetInfoData[i].mTranslationMat;
            scale = gPlanetInfoData[i].mScaleMat;

            gPlanetInfoData[i].mSharedMat = parentMat * rotOrbitY * trans;
            gUniformData.mToWorldMat[i] = parentMat * rotOrbitY * rotOrbitZ * trans * rotSelf * scale;
            gUniformData.mColor[i] = gPlanetInfoData[i].mColor;
        }

        gUniformDataSky = gUniformData;
        viewMat.setTranslation(vec3(0));
        gUniformDataSky.mProjectView = projMat * viewMat;

        gPanini.SetParams(gPaniniParams);
        gPanini.Update(deltaTime);
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue[0]);
            ::toggleVSync(pRenderer[0], &pSwapChain);
        }

        // Update uniform buffers
        for (int i = gViewCount - 1; i >= 0; --i)
        {
            if (!gMultiGPUCurrent && i > 0)
                continue;

            BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex][i] };
            beginUpdateResource(&viewProjCbv);
            memcpy(viewProjCbv.pMappedData, &gUniformData, sizeof(gUniformData));
            endUpdateResource(&viewProjCbv);

            BufferUpdateDesc skyboxViewProjCbv = { pSkyboxUniformBuffer[gFrameIndex][i] };
            beginUpdateResource(&skyboxViewProjCbv);
            memcpy(skyboxViewProjCbv.pMappedData, &gUniformDataSky, sizeof(gUniformDataSky));
            endUpdateResource(&skyboxViewProjCbv);
        }

        for (int i = gViewCount - 1; i >= 0; --i)
        {
            const uint32_t nextFrameIndex = (gFrameIndex + gFrameLatency) % gDataBufferCount;
            RenderTarget*  pRenderTarget = pRenderTargets[nextFrameIndex][i];
            RenderTarget*  pDepthBuffer = pDepthBuffers[i];
            Semaphore*     pRenderCompleteSemaphore = gGraphicsCmdRing[i].pSemaphores[gFrameIndex][0];
            Fence*         pRenderCompleteFence = gGraphicsCmdRing[i].pFences[gFrameIndex][0];
            Cmd*           cmd = gGraphicsCmdRing[i].pCmds[gFrameIndex][0];

            // simply record the screen cleaning command
            beginCmd(cmd);
            cmdBeginGpuFrameProfile(cmd, gGpuProfilerTokens[i]);

            if (i == 0 || !gMultiGPUCurrent)
            {
                RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
            }
            else
            {
                RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, RESOURCE_STATE_COPY_SOURCE };
                // Barrier - Acquire from Transfer queue to be used on Graphics queue as RT
                // Conditional barrier as if the transfer queue never acquired the RT, no need to reacquire
                if (gReadbackSyncTokens[nextFrameIndex][i - 1] && isTokenCompleted(&gReadbackSyncTokens[nextFrameIndex][i - 1]))
                {
                    barrier.mAcquire = true;
                    barrier.mQueueType = QUEUE_TYPE_TRANSFER;
                    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
                }
                barrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, RESOURCE_STATE_RENDER_TARGET };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
            }

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
            bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
            cmdBindRenderTargets(cmd, &bindRenderTargets);

            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

            //// draw skybox
            const uint32_t skyboxStride = sizeof(float) * 4;
            cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Draw skybox");
            cmdBindPipeline(cmd, pSkyBoxDrawPipeline[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetTexture[i]);
            cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 0, pDescriptorSetUniforms[i]);
            cmdBindVertexBuffer(cmd, 1, &pSkyBoxVertexBuffer[i], &skyboxStride, NULL);
            cmdDraw(cmd, 36, 0);
            cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);

            ////// draw planets
            const uint32_t sphereStride = sizeof(float) * 6;
            cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[i], "Draw Planets");
            cmdBindPipeline(cmd, pSpherePipeline[i]);
            cmdBindDescriptorSet(cmd, gFrameIndex * 2 + 1, pDescriptorSetUniforms[i]);
            cmdBindVertexBuffer(cmd, 1, &pSphereVertexBuffer[i], &sphereStride, NULL);
            cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, gNumPlanets, 0);
            cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[i]);

            cmdBindRenderTargets(cmd, NULL);

            if (i == 0 || !gMultiGPUCurrent)
            {
                RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
            }
            else
            {
                RenderTargetBarrier barrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_COPY_SOURCE };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

                // Barrier - Release from Graphics queue to be used on Transfer queue as CopySource
                barrier = { pRenderTarget, RESOURCE_STATE_COPY_SOURCE, RESOURCE_STATE_COPY_SOURCE };
                barrier.mRelease = true;
                barrier.mQueueType = QUEUE_TYPE_TRANSFER;
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);
            }

            cmdEndGpuFrameProfile(cmd, gGpuProfilerTokens[i]);
            endCmd(cmd);

            // submit graphics workload
            if (i == 0)
            {
                QueueSubmitDesc submitDesc = {};
                submitDesc.mCmdCount = 1;
                submitDesc.ppCmds = &cmd;
                queueSubmit(pGraphicsQueue[i], &submitDesc);
            }
            else
            {
                QueueSubmitDesc submitDesc = {};
                submitDesc.mCmdCount = 1;
                submitDesc.ppCmds = &cmd;
                submitDesc.pSignalFence = pRenderCompleteFence;
                submitDesc.mSignalSemaphoreCount = 1;
                submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
                queueSubmit(pGraphicsQueue[i], &submitDesc);
            }
        }

        // Copy outputs from secondary GPUs to CPU
        for (int i = 1; i < gViewCount && gMultiGPUCurrent; ++i)
        {
            const uint32_t nextFrameIndex = (gFrameIndex + gFrameLatency) % gDataBufferCount;
            RenderTarget*  pRenderTarget = pRenderTargets[nextFrameIndex][i];
            Semaphore*     pRenderCompleteSemaphore = gGraphicsCmdRing[i].pSemaphores[gFrameIndex][0];

            TextureCopyDesc copyDesc = {};
            copyDesc.pTexture = pRenderTarget->pTexture;
            copyDesc.pBuffer = pTransferBuffer[nextFrameIndex][i];
            copyDesc.pWaitSemaphore = pRenderCompleteSemaphore;
            copyDesc.mTextureState = RESOURCE_STATE_COPY_SOURCE;
            // Barrier - Info to copy engine that the resource was last acquired by Graphics queue
            // Copy engine will use this as the queue type for acquire and release barrier on Transfer queue
            copyDesc.mQueueType = QUEUE_TYPE_GRAPHICS;
            copyResource(&copyDesc, &gReadbackSyncTokens[nextFrameIndex][i - 1]);
        }

        // Upload results from secondary GPUs to primary GPU
        if (gMultiGPUCurrent)
        {
            if (gBufferedFrames == gFrameLatency)
            {
                for (uint32_t i = 1; i < gViewCount; ++i)
                {
                    uint64_t tick = cpuProfileEnter(gReadBackCpuToken);
                    waitForToken(&gReadbackSyncTokens[gFrameIndex][i - 1]);
                    cpuProfileLeave(gReadBackCpuToken, tick);

                    TextureUpdateDesc updateDesc = { pRenderResult[gFrameIndex][i - 1] };
                    updateDesc.mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    Buffer* stagingBuffer = pTransferBuffer[gFrameIndex][i];
                    beginUpdateResource(&updateDesc);
                    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
                    memcpy(subresource.pMappedData, stagingBuffer->pCpuMappedAddress, stagingBuffer->mSize);
                    endUpdateResource(&updateDesc);
                }
            }
            else
            {
                for (uint32_t i = 1; i < gViewCount; ++i)
                {
                    // clear the result texture, to have consistent behavior between the first frames and the normal frames
                    TextureUpdateDesc updateDesc = { pRenderResult[gFrameIndex][i - 1] };
                    updateDesc.mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    Buffer* stagingBuffer = pTransferBuffer[gFrameIndex][i];
                    beginUpdateResource(&updateDesc);
                    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
                    memset(subresource.pMappedData, 0, stagingBuffer->mSize);
                    endUpdateResource(&updateDesc);
                }
                ++gBufferedFrames;
            }
        }

        // Merge results from previous frame and present
        {
            uint32_t swapchainImageIndex;
            acquireNextImage(pRenderer[0], pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

            Cmd*          cmd = gGraphicsCmdRing[0].pCmds[gFrameIndex][1];
            RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

            beginCmd(cmd);
            cmdBeginGpuFrameProfile(cmd, gGpuProfilerTokens[gViewCount]);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[gViewCount], "Draw Results");
            RenderTargetBarrier barriers[] = { { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET } };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
            cmdBindRenderTargets(cmd, &bindRenderTargets);

            cmdBeginGpuTimestampQuery(cmd, gGpuProfilerTokens[gViewCount], "Panini Projection");

            cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
            gPanini.Draw(cmd);

            cmdSetViewport(cmd, (float)mSettings.mWidth * 0.5f, 0.0f, (float)mSettings.mWidth * 0.5f, (float)mSettings.mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, mSettings.mWidth, mSettings.mHeight);
            gPanini.Draw(cmd);

            cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[gViewCount]);

            cmdSetViewport(cmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);

            const float txtIndentY = 15.f;
            float       txtOrigY = txtIndentY;
            float2      screenCoords = float2(8.0f, txtOrigY);

            gFrameTimeDraw.mFontColor = 0xff00ffff;
            gFrameTimeDraw.mFontSize = 18.0f;
            gFrameTimeDraw.mFontID = gFontID;
            float2 txtSize = cmdDrawCpuProfile(cmd, screenCoords, &gFrameTimeDraw);

            txtOrigY += txtSize.y + 4 * txtIndentY;
            for (uint32_t j = 0; j < gViewCount; ++j)
            {
                screenCoords = float2(8.f, txtOrigY);
                txtSize = cmdDrawGpuProfile(cmd, screenCoords, gGpuProfilerTokens[j], &gFrameTimeDraw);
                txtOrigY += txtSize.y + txtIndentY;
            }
            screenCoords = float2(8.f, txtOrigY);
            txtSize = cmdDrawGpuProfile(cmd, screenCoords, gGpuProfilerTokens[gViewCount], &gFrameTimeDraw);

            cmdDrawUserInterface(cmd);

            cmdBindRenderTargets(cmd, NULL);

            barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
            cmdEndGpuTimestampQuery(cmd, gGpuProfilerTokens[gViewCount]);
            cmdEndGpuFrameProfile(cmd, gGpuProfilerTokens[gViewCount]);
            endCmd(cmd);

            // Avoid submitting graphics work before the resource loader submits the copy.
            FlushResourceUpdateDesc flushUpdateDesc[gViewCount] = {};
            for (uint32_t i = 0; i < gViewCount; ++i)
            {
                if (i && !gMultiGPUCurrent)
                {
                    flushUpdateDesc[i] = flushUpdateDesc[0];
                    continue;
                }
                flushUpdateDesc[i].mNodeIndex = i;
                flushResourceUpdates(&flushUpdateDesc[i]);
            }

            const uint32_t previousFrameIndex = (gFrameIndex + gDataBufferCount - gFrameLatency) % gDataBufferCount;
            Semaphore*     uploadCompleteSemaphore =
                gMultiGPUCurrent ? getLastSemaphoreSubmitted(0) : gGraphicsCmdRing[1].pSemaphores[previousFrameIndex][0];
            Semaphore* pRenderCompleteSemaphore = gGraphicsCmdRing[0].pSemaphores[gFrameIndex][0];
            Semaphore* ppWaitSemaphores[] = { flushUpdateDesc[0].pOutSubmittedSemaphore, pImageAcquiredSemaphore, uploadCompleteSemaphore };
            Fence*     pRenderCompleteFence = gGraphicsCmdRing[0].pFences[gFrameIndex][0];

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.ppCmds = &cmd;
            submitDesc.pSignalFence = pRenderCompleteFence;
            submitDesc.ppWaitSemaphores = ppWaitSemaphores;
            submitDesc.mWaitSemaphoreCount = uploadCompleteSemaphore ? 3 : 2;
            submitDesc.mSignalSemaphoreCount = 1;
            submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
            queueSubmit(pGraphicsQueue[0], &submitDesc);

            QueuePresentDesc presentDesc = {};
            presentDesc.mIndex = (uint8_t)swapchainImageIndex;
            presentDesc.mWaitSemaphoreCount = 1;
            presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
            presentDesc.pSwapChain = pSwapChain;
            presentDesc.mSubmitDone = true;
            queuePresent(pGraphicsQueue[0], &presentDesc);
        }

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            Fence*      pNextFence = gGraphicsCmdRing[i].pFences[(gFrameIndex + 1) % gDataBufferCount][0];
            FenceStatus fenceStatus;
            getFenceStatus(pRenderer[i], pNextFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            {
                uint64_t tick = cpuProfileEnter(gGpuExecCpuToken);
                waitForFences(pRenderer[i], 1, &pNextFence);
                cpuProfileLeave(gGpuExecCpuToken, tick);
            }

            resetCmdPool(pRenderer[i], gGraphicsCmdRing[i].pCmdPools[(gFrameIndex + 1) % gDataBufferCount]);
        }
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "11a_UnlinkedMultiGPU"; }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue[0];
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer[0], &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer[0], &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer[0], &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (!gMultiGPUCurrent && i > 0)
            {
                pDescriptorSetTexture[i] = pDescriptorSetTexture[0];
                pDescriptorSetUniforms[i] = pDescriptorSetUniforms[0];
            }
            else
            {
                DescriptorSetDesc setDesc = { pRootSignature[i], DESCRIPTOR_UPDATE_FREQ_NONE, 1, i };
                addDescriptorSet(pRenderer[i], &setDesc, &pDescriptorSetTexture[i]);
                setDesc = { pRootSignature[i], DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2, i };
                addDescriptorSet(pRenderer[i], &setDesc, &pDescriptorSetUniforms[i]);
            }
        }
    }

    void removeDescriptorSets()
    {
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;

        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            removeDescriptorSet(pRenderer[view], pDescriptorSetTexture[view]);
            removeDescriptorSet(pRenderer[view], pDescriptorSetUniforms[view]);
        }
    }

    void addRootSignatures()
    {
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (!gMultiGPUCurrent && i > 0)
            {
                pRootSignature[i] = pRootSignature[0];
            }
            else
            {
                Shader*           shaders[] = { pSphereShader[i], pSkyBoxDrawShader[i] };
                const char*       pStaticSamplers[] = { "uSampler0" };
                RootSignatureDesc rootDesc = {};
                rootDesc.mStaticSamplerCount = 1;
                rootDesc.ppStaticSamplerNames = pStaticSamplers;
                rootDesc.ppStaticSamplers = &pSamplerSkyBox[i];
                rootDesc.mShaderCount = 2;
                rootDesc.ppShaders = shaders;
                addRootSignature(pRenderer[i], &rootDesc, &pRootSignature[i]);
            }
        }
    }

    void removeRootSignatures()
    {
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;
        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            removeRootSignature(pRenderer[view], pRootSignature[view]);
        }
    }

    void addShaders()
    {
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (!gMultiGPUCurrent && i > 0)
            {
                pSkyBoxDrawShader[i] = pSkyBoxDrawShader[0];
                pSphereShader[i] = pSphereShader[0];
            }
            else
            {
                ShaderLoadDesc skyShader = {};
                skyShader.mStages[0].pFileName = "skybox.vert";
                skyShader.mStages[1].pFileName = "skybox.frag";
                ShaderLoadDesc basicShader = {};
                basicShader.mStages[0].pFileName = "basic.vert";
                basicShader.mStages[1].pFileName = "basic.frag";

                addShader(pRenderer[i], &skyShader, &pSkyBoxDrawShader[i]);
                addShader(pRenderer[i], &basicShader, &pSphereShader[i]);
            }
        }
    }

    void removeShaders()
    {
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;

        for (uint32_t view = 0; view < rendererCount; ++view)
        {
            removeShader(pRenderer[view], pSphereShader[view]);
            removeShader(pRenderer[view], pSkyBoxDrawShader[view]);
        }
    }

    void addPipelines()
    {
        gPanini.Load(pSwapChain->ppRenderTargets);

        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (!gMultiGPUCurrent && i > 0)
            {
                pSpherePipeline[i] = pSpherePipeline[0];
                pSkyBoxDrawPipeline[i] = pSkyBoxDrawPipeline[0];
                continue;
            }
            // layout and pipeline for sphere draw
            VertexLayout vertexLayout = {};
            vertexLayout.mBindingCount = 1;
            vertexLayout.mAttribCount = 2;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;
            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 0;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

            RasterizerStateDesc rasterizerStateDesc = {};
            rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = true;
            depthStateDesc.mDepthFunc = CMP_GEQUAL;

            PipelineDesc desc = {};
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.mRenderTargetCount = 1;
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
            pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
            pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
            pipelineSettings.mDepthStencilFormat = pDepthBuffers[0]->mFormat;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;

            pipelineSettings.pRootSignature = pRootSignature[i];
            pipelineSettings.pShaderProgram = pSphereShader[i];
            addPipeline(pRenderer[i], &desc, &pSpherePipeline[i]);

            // layout and pipeline for skybox draw
            vertexLayout = {};
            vertexLayout.mBindingCount = 1;
            vertexLayout.mAttribCount = 1;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            pipelineSettings.pDepthState = NULL;
            pipelineSettings.pRasterizerState = &rasterizerStateDesc;
            pipelineSettings.pShaderProgram = pSkyBoxDrawShader[i];
            addPipeline(pRenderer[i], &desc, &pSkyBoxDrawPipeline[i]);
        }
    }

    void removePipelines()
    {
        gPanini.Unload();

        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;
        for (uint32_t i = 0; i < rendererCount; ++i)
        {
            removePipeline(pRenderer[i], pSkyBoxDrawPipeline[i]);
            removePipeline(pRenderer[i], pSpherePipeline[i]);
        }
    }

    void prepareDescriptorSets()
    {
        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (i > 0 && pDescriptorSetTexture[i] == pDescriptorSetTexture[0])
                continue;

            DescriptorData params[6] = {};
            params[0].pName = "RightText";
            params[0].ppTextures = &pSkyBoxTextures[i][0];
            params[1].pName = "LeftText";
            params[1].ppTextures = &pSkyBoxTextures[i][1];
            params[2].pName = "TopText";
            params[2].ppTextures = &pSkyBoxTextures[i][2];
            params[3].pName = "BotText";
            params[3].ppTextures = &pSkyBoxTextures[i][3];
            params[4].pName = "FrontText";
            params[4].ppTextures = &pSkyBoxTextures[i][4];
            params[5].pName = "BackText";
            params[5].ppTextures = &pSkyBoxTextures[i][5];
            updateDescriptorSet(pRenderer[i], 0, pDescriptorSetTexture[i], 6, params);

            for (uint32_t f = 0; f < gDataBufferCount; ++f)
            {
                DescriptorData uParams[1] = {};
                uParams[0].pName = "uniformBlock";
                uParams[0].ppBuffers = &pSkyboxUniformBuffer[f][i];
                updateDescriptorSet(pRenderer[i], f * 2 + 0, pDescriptorSetUniforms[i], 1, uParams);
                uParams[0].ppBuffers = &pProjViewUniformBuffer[f][i];
                updateDescriptorSet(pRenderer[i], f * 2 + 1, pDescriptorSetUniforms[i], 1, uParams);
            }
        }
    }

    bool addDepthBuffer()
    {
        // Add color buffer
        RenderTargetDesc colorRT = {};
        colorRT.mArraySize = 1;
        colorRT.mClearValue = gClearColor;
        colorRT.mDepth = 1;
        colorRT.mFormat = pSwapChain->mFormat;
        colorRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        colorRT.mHeight = mSettings.mHeight;
        colorRT.mSampleCount = SAMPLE_COUNT_1;
        colorRT.mSampleQuality = 0;
        colorRT.mWidth = mSettings.mWidth / 2;
        colorRT.pName = "Color RT";

        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = gClearDepth;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth / 2;

        TextureDesc colorResult = {};
        colorResult.mArraySize = colorRT.mArraySize;
        colorResult.mMipLevels = 1;
        colorResult.mClearValue = colorRT.mClearValue;
        colorResult.mDepth = colorRT.mDepth;
        colorResult.mFormat = colorRT.mFormat;
        colorResult.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        colorResult.mHeight = colorRT.mHeight;
        colorResult.mSampleCount = colorRT.mSampleCount;
        colorResult.mSampleQuality = colorRT.mSampleQuality;
        colorResult.mWidth = colorRT.mWidth;
        colorResult.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        colorResult.pName = "Color result";

        for (uint32_t i = 0; i < gViewCount; ++i)
        {
            if (gMultiGPUCurrent)
            {
                colorRT.mNodeIndex = i;
                depthRT.mNodeIndex = i;
            }

            addRenderTarget(pRenderer[i], &depthRT, &pDepthBuffers[i]);

            for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
            {
                if (gMultiGPUCurrent && i > 0)
                {
                    colorRT.mStartState = RESOURCE_STATE_COPY_SOURCE;
                    addRenderTarget(pRenderer[i], &colorRT, &pRenderTargets[frameIdx][i]);

                    TextureLoadDesc colorTexture = { &pRenderResult[frameIdx][i - 1], &colorResult };
                    addResource(&colorTexture, NULL);

                    const uint32_t rowAlignment = max(1u, pRenderer[i]->pGpu->mSettings.mUploadBufferTextureRowAlignment);
                    const uint32_t blockSize = max(1u, TinyImageFormat_BitSizeOfBlock(colorResult.mFormat));
                    const uint32_t sliceAlignment =
                        round_up(round_up(pRenderer[i]->pGpu->mSettings.mUploadBufferTextureAlignment, blockSize), rowAlignment);
                    BufferLoadDesc bufferDesc = {};
                    bufferDesc.mDesc.mSize = util_get_surface_size(colorResult.mFormat, colorResult.mWidth, colorResult.mHeight, 1,
                                                                   rowAlignment, sliceAlignment, 0, 1, 0, 1);
                    bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
                    bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
                    bufferDesc.mDesc.mQueueType = QUEUE_TYPE_TRANSFER;
                    bufferDesc.mDesc.mNodeIndex = i;
                    bufferDesc.ppBuffer = &pTransferBuffer[frameIdx][i];

                    addResource(&bufferDesc, NULL);
                }
                else
                {
                    colorRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                    addRenderTarget(pRenderer[i], &colorRT, &pRenderTargets[frameIdx][i]);
                }
            }
        }

        return pDepthBuffers[0] != NULL;
    }

    void ResetFrameLatency()
    {
        const uint32_t rendererCount = gMultiGPUCurrent ? gViewCount : 1;
        for (uint32_t i = 0; i < rendererCount; ++i)
            waitQueueIdle(pGraphicsQueue[i]);

        waitForAllResourceLoads();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            for (uint32_t j = 0; j < gViewCount; ++j)
            {
                resetCmdPool(pRenderer[j], gGraphicsCmdRing[j].pCmdPools[i]);

                // Recycle fence and semaphores to reset signaled state.
                removeFence(pRenderer[j], gGraphicsCmdRing[j].pFences[i][0]);
                removeSemaphore(pRenderer[j], gGraphicsCmdRing[j].pSemaphores[i][0]);

                if (j > 0)
                    gReadbackSyncTokens[i][j - 1] = {};

                addFence(pRenderer[j], &gGraphicsCmdRing[j].pFences[i][0]);
                addSemaphore(pRenderer[j], &gGraphicsCmdRing[j].pSemaphores[i][0]);
            }
        }
        gBufferedFrames = 0;
    }
};

DEFINE_APPLICATION_MAIN(UnlinkedMultiGPU)
