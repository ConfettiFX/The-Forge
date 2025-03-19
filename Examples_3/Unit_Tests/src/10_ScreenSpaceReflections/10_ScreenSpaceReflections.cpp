/*
 *
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

// Unit Test for testing materials and pbr.

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Renderer/Interfaces/IVisibilityBuffer.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Utilities/Threading/Atomics.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

// Input
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "SamplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_2spp.cpp"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define NO_FSL_DEFINITIONS
#include "../../../../Common_3/Graphics/FSL/fsl_srt.h"
#include "Shaders/FSL/ShaderDefs.h.fsl"

// Geometry
#include "../../../Visibility_Buffer/src/SanMiguel.h"

// fsl
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "./Shaders/FSL/Global.srt.h"
#include "./Shaders/FSL/SSSR.srt.h"
#include "./Shaders/FSL/PPR.srt.h"
#include "./Shaders/FSL/PBR.srt.h"
#include "./Shaders/FSL/GenerateMips.srt.h"
#include "./Shaders/FSL/DepthDownsample.srt.h"
#include "./Shaders/FSL/CopyDepth.srt.h"
#include "./Shaders/FSL/TriangleFiltering.srt.h"

#define MAX_PLANES  4
#define SCENE_SCALE 10.0f

#define FOREACH_SETTING(X)       \
    X(AddGeometryPassThrough, 0) \
    X(BindlessSupported, 1)

#define GENERATE_ENUM(x, y)    x,
#define GENERATE_STRING(x, y)  #x,
#define GENERATE_STRUCT(x, y)  uint32_t m##x;
#define GENERATE_VALUE(x, y)   y,
#define INIT_STRUCT(s)         s = { FOREACH_SETTING(GENERATE_VALUE) }

#define GENERATE_MIPS_MAX_MIPS 16

typedef enum ESettings
{
    FOREACH_SETTING(GENERATE_ENUM) Count
} ESettings;

const char* gSettingNames[] = { FOREACH_SETTING(GENERATE_STRING) };

// Useful for using names directly instead of subscripting an array
struct AppSettings
{
    FOREACH_SETTING(GENERATE_STRUCT)
} gSettings;

// Define different geometry sets (opaque and alpha tested geometry)
static const uint32_t gNumGeomSets = NUM_GEOMETRY_SETS;

// Have a uniform for camera data
struct UniformDataSkybox
{
    mat4 mProjectView;
};

// Have a uniform for extended camera data
struct UniformExtendedCamData
{
    mat4 mViewMat;
    mat4 mInvViewMat;
    mat4 mProjMat;
    mat4 mViewProjMat;
    mat4 mInvViewProjMat;

    vec4 mCameraWorldPos;

    float2 mViewPortSize;
    float  mNear;
    float  mFar;

    float3 mEnvColor;
    float  _pad1;

    float mGroundRoughness;
    float mGroundMetallic;
    uint  mUseEnvMap;
};

// Have a uniform for PPR properties
struct UniformPPRProData
{
    uint  renderMode;
    float useHolePatching;
    float useExpensiveHolePatching;
    float useNormalMap;

    float intensity;
    float useFadeEffect;
    float debugNonProjected;
    float padding01;
};

struct MeshInfoUniformBlock
{
    mat4 mWorldViewProjMat;
    mat4 mPrevWorldViewProjMat;
};

struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3     mEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    uint32_t mDrawCount[gNumGeomSets] = { 0 };
};

struct Light
{
    vec4  mPos;
    vec4  mCol;
    float mRadius;
    float mIntensity;
    char  _pad[8];
};

struct UniformLightData
{
    // Used to tell our shaders how many lights are currently present
    Light mLights[16] = {}; // array of lights seem to be broken so just a single light for now
    int   mCurrAmountOfLights = 0;
};

struct DirectionalLight
{
    vec4 mCol; // alpha is the intesity
    vec4 mDir;
};

struct UniformDirectionalLightData
{
    // Used to tell our shaders how many lights are currently present
    DirectionalLight mLights[16]; // array of lights seem to be broken so just a single light for now
    int              mCurrAmountOfDLights = 0;
};

struct PlaneInfo
{
    mat4 rotMat;
    vec4 centerPoint;
    vec4 size;
};

struct UniformPlaneInfoData
{
    PlaneInfo planeInfo[MAX_PLANES];
    uint32_t  numPlanes;
    uint32_t  pad00;
    uint32_t  pad01;
    uint32_t  pad02;
};

struct UniformSSSRConstantsData
{
    mat4 g_inv_view_proj;
    mat4 g_proj;
    mat4 g_inv_proj;
    mat4 g_view;
    mat4 g_inv_view;
    mat4 g_prev_view_proj;

    uint32_t g_frame_index;
    bool     g_clear_targets;
    uint32_t g_max_traversal_intersections;
    uint32_t g_min_traversal_occupancy;
    uint32_t g_most_detailed_mip;
    float    g_temporal_stability_factor;
    float    g_depth_buffer_thickness;
    uint32_t g_samples_per_quad;
    uint32_t g_temporal_variance_guided_tracing_enabled;
    float    g_roughness_threshold;
    uint32_t g_skip_denoiser;
};

enum
{
    SCENE_ONLY = 0,
    REFLECTIONS_ONLY = 1,
    SCENE_WITH_REFLECTIONS = 2,
    SCENE_EXCLU_REFLECTIONS = 3,
};

enum
{
    PP_REFLECTION = 0,
    SSS_REFLECTION = 1,
};

static bool gUseHolePatching;
static bool gUseExpensiveHolePatching;

static bool gUseNormalMap;
static bool gUseFadeEffect;
static bool gDebugNonProjectedPixels;

static uint32_t gRenderMode;
static uint32_t gReflectionType;
static uint32_t gLastReflectionType;

static uint32_t gPlaneNumber;
static float    gPlaneSize;
static float    gRRP_Intensity;
static float    gPlaneRotationOffset;

static bool gUseSPD;

static uint32_t gSSSR_MaxTravelsalIntersections;
#if defined(AUTOMATED_TESTING)
static uint32_t gSSSR_MinTravelsalOccupancy;
#else
static uint32_t gSSSR_MinTravelsalOccupancy;
#endif
static uint32_t gSSSR_MostDetailedMip;
static float    pSSSR_TemporalStability;
static float    gSSSR_DepthThickness;
static int32_t  gSSSR_SamplesPerQuad;
static int32_t  gSSSR_EAWPassCount;
static bool     gSSSR_TemporalVarianceEnabled;
static float    gSSSR_RougnessThreshold;
static bool     gSSSR_SkipDenoiser;

static bool gSSSRSupported;

UniformExtendedCamData gUniformDataExtenedCamera;
bool                   gUseEnvMap;

// We are rendering the scene (geometry, skybox, ...) at this resolution, UI at window resolution (mSettings.mWidth, mSettings.mHeight)
// Render scene at gSceneRes
// presentImage -> The scene rendertarget composed into the swapchain/backbuffer
// Render UI into backbuffer
static Resolution gSceneRes;

static void InitAppSettings()
{
    gUseHolePatching = true;
    gUseExpensiveHolePatching = true;

    gUseNormalMap = false;
    gUseFadeEffect = true;
    gDebugNonProjectedPixels = false;

    gRenderMode = SCENE_WITH_REFLECTIONS;
    gReflectionType = SSS_REFLECTION;
    gLastReflectionType = gReflectionType;

    gPlaneNumber = 1;
    gPlaneSize = 260.0f;
    gRRP_Intensity = 0.2f;
    gPlaneRotationOffset = 0.0f;

    gUseSPD = true;

    gSSSR_MaxTravelsalIntersections = 128;
#if defined(AUTOMATED_TESTING)
    gSSSR_MinTravelsalOccupancy = 0;
#else
    gSSSR_MinTravelsalOccupancy = 4;
#endif
    gSSSR_MostDetailedMip = 1;
    pSSSR_TemporalStability = 0.99f;
    gSSSR_DepthThickness = 0.15f;
    gSSSR_SamplesPerQuad = 1;
    gSSSR_EAWPassCount = 1;
    gSSSR_TemporalVarianceEnabled = true;
    gSSSR_RougnessThreshold = 0.1f;
    gSSSR_SkipDenoiser = false;

    gSSSRSupported = false;

    gUseEnvMap = false;
    gUniformDataExtenedCamera.mGroundRoughness = 0.05f;
    gUniformDataExtenedCamera.mGroundMetallic = 1.0f;
    gUniformDataExtenedCamera.mEnvColor = float3(0.095f, 0.095f, 0.11f);
}

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
static const uint32_t gDataBufferCount = 2;

ProfileToken gPPRGpuProfileToken;
ProfileToken gSSSRGpuProfileToken;
ProfileToken gCurrentGpuProfileToken;

Renderer*  pRenderer = NULL;
Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};
SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

VisibilityBuffer* pVisibilityBuffer = NULL;

RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pSceneBuffer = NULL;
RenderTarget* pNormalRoughnessBuffers[gDataBufferCount] = { NULL };
RenderTarget* pReflectionBuffer = NULL;
RenderTarget* pDepthBuffer = NULL;

DescriptorSet* pDescriptorSetPersistent = NULL;
DescriptorSet* pDescriptorSetPerFrame = NULL;
DescriptorSet* pDescriptorSetDepthDownSamplePerBatch = NULL;
DescriptorSet* pDescriptorSetSSSR = NULL;
DescriptorSet* pDescriptorSetPPR = NULL;
DescriptorSet* pDescriptorSetCopyDepth = NULL;
DescriptorSet* pDescriptorSetTriangleFilteringPerBatch = NULL;

// Clear buffers pipeline
Shader*   pShaderClearBuffers = NULL;
Pipeline* pPipelineClearBuffers = NULL;

// Triangle filtering pipeline
Shader*   pShaderTriangleFiltering = NULL;
Pipeline* pPipelineTriangleFiltering = NULL;

// VB pass pipeline
Shader*   pShaderVBBufferPass[gNumGeomSets] = {};
Pipeline* pPipelineVBBufferPass[gNumGeomSets] = {};

// VB shade pipeline
Shader*   pShaderVBShade = NULL;
Pipeline* pPipelineVBShadeSrgb = NULL;

Buffer*   pSkyboxVertexBuffer = NULL;
Shader*   pSkyboxShader = NULL;
Pipeline* pSkyboxPipeline = NULL;

Shader*   pPPR_ProjectionShader = NULL;
Pipeline* pPPR_ProjectionPipeline = NULL;

Shader*   pPPR_ReflectionShader = NULL;
Pipeline* pPPR_ReflectionPipeline = NULL;

Shader*   pPPR_HolePatchingShader = NULL;
Pipeline* pPPR_HolePatchingPipeline = NULL;

Shader*   pCopyDepthShader = NULL;
Pipeline* pCopyDepthPipeline = NULL;

Shader*        pGenerateMipShader = NULL;
Pipeline*      pGenerateMipPipeline = NULL;
DescriptorSet* pDescriptorGenerateMip = NULL;
uint32_t       gMipSizeRootConstantIndex = 0;

Shader*   pSPDShader = NULL;
Pipeline* pSPDPipeline = NULL;

Shader*   pSSSR_ClassifyTilesShader = NULL;
Pipeline* pSSSR_ClassifyTilesPipeline = NULL;

Shader*   pSSSR_PrepareIndirectArgsShader = NULL;
Pipeline* pSSSR_PrepareIndirectArgsPipeline = NULL;

Shader*   pSSSR_IntersectShader = NULL;
Pipeline* pSSSR_IntersectPipeline = NULL;

Shader*   pSSSR_ResolveSpatialShader = NULL;
Pipeline* pSSSR_ResolveSpatialPipeline = NULL;

Shader*   pSSSR_ResolveTemporalShader = NULL;
Pipeline* pSSSR_ResolveTemporalPipeline = NULL;

Shader*   pSSSR_ResolveEAWShader = NULL;
Pipeline* pSSSR_ResolveEAWPipeline = NULL;

Shader*   pSSSR_ResolveEAWStride2Shader = NULL;
Pipeline* pSSSR_ResolveEAWStride2Pipeline = NULL;

Shader*   pSSSR_ResolveEAWStride4Shader = NULL;
Pipeline* pSSSR_ResolveEAWStride4Pipeline = NULL;

Buffer*                  pSSSR_ConstantsBuffer[gDataBufferCount] = { NULL };
UniformSSSRConstantsData gUniformSSSRConstantsData;

Buffer* pSPD_AtomicCounterBuffer = NULL;

Buffer*       pSSSR_RayListBuffer = NULL;
Buffer*       pSSSR_TileListBuffer = NULL;
Buffer*       pSSSR_RayCounterBuffer = NULL;
Buffer*       pSSSR_TileCounterBuffer = NULL;
Buffer*       pSSSR_IntersectArgsBuffer = NULL;
Buffer*       pSSSR_DenoiserArgsBuffer = NULL;
Buffer*       pSSSR_SobolBuffer = NULL;
Buffer*       pSSSR_RankingTileBuffer = NULL;
Buffer*       pSSSR_ScramblingTileBuffer = NULL;
RenderTarget* pSSSR_TemporalResults[gDataBufferCount] = { NULL };
Texture*      pSSSR_TemporalVariance = NULL;
RenderTarget* pSSSR_RayLength = NULL;
Texture*      pSSSR_DepthHierarchy = NULL;

Buffer* pScreenQuadVertexBuffer = NULL;

Buffer*  pSSSR_GenMipsBuffers[GENERATE_MIPS_MAX_MIPS] = { NULL };
Texture* pSkybox = NULL;
Texture* pBRDFIntegrationMap = NULL;
Texture* pIrradianceMap = NULL;
Texture* pSpecularMap = NULL;

Buffer*   pIntermediateBuffer = NULL;
// For clearing Intermediate Buffer
uint32_t* gInitializeVal = NULL;

Buffer*              pBufferMeshTransforms[gDataBufferCount] = { NULL };
MeshInfoUniformBlock gMeshInfoUniformData[gDataBufferCount];

Buffer* pBufferMeshConstants = NULL;

UniformDataSkybox gUniformDataSky;

Buffer* pBufferUniformExtendedCamera[gDataBufferCount] = { NULL };

Buffer* pBufferUniformCameraSky[gDataBufferCount] = { NULL };

Buffer*           pBufferUniformPPRPro[gDataBufferCount] = { NULL };
UniformPPRProData gUniformPPRProData;

Buffer*          pBufferUniformLights = NULL;
UniformLightData gUniformDataLights;

Buffer*                     pBufferUniformDirectionalLights = NULL;
UniformDirectionalLightData gUniformDataDirectionalLights;

Buffer*              pBufferUniformPlaneInfo[gDataBufferCount] = { NULL };
UniformPlaneInfoData gUniformDataPlaneInfo;

Buffer*                 pBufferVBConstants[gDataBufferCount] = { NULL };
PerFrameVBConstantsData gVBConstants[gDataBufferCount] = {};

PerFrameData gPerFrameData[gDataBufferCount] = {};

Shader*   pShaderPostProc = NULL;
Pipeline* pPipelinePostProc = NULL;

Sampler* pDefaultSampler = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerNearestClampToEdge = NULL;

uint32_t gFrameIndex = 0;
uint32_t gFrameCount = 0;

ICameraController* pCameraController = NULL;

FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

UIComponent*     pGui = NULL;
DynamicUIWidgets PPR_Widgets;
DynamicUIWidgets SSSR_Widgets;

SyncToken gResourceSyncStartToken = {};
SyncToken gResourceSyncToken = {};

Texture** gDiffuseMapsStorage = NULL;
Texture** gNormalMapsStorage = NULL;
Texture** gSpecularMapsStorage = NULL;

VBMeshInstance*  pVBMeshInstances = NULL;
VBPreFilterStats gVBPreFilterStats[gDataBufferCount] = {};

Scene*    pScene = NULL;
Geometry* pSanMiguelModel;
uint32_t  gMeshCount = 0;
uint32_t  gMaterialCount = 0;
mat4      gSanMiguelModelMat;

const char* gTestScripts[] = { "Test_RenderScene.lua", "Test_RenderReflections.lua", "Test_RenderSceneReflections.lua",
                               "Test_RenderSceneExReflections.lua" };
uint32_t    gCurrentScriptIndex = 0;

void RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

class ScreenSpaceReflections: public IApp
{
public:
    ScreenSpaceReflections() //-V832
    {
#ifdef TARGET_IOS
        mSettings.mContentScaleFactor = 1.f;
#endif
    }

    bool Init()
    {
        InitAppSettings();

        INIT_STRUCT(gSettings);

        ExtendedSettings extendedSettings = {};
        extendedSettings.mNumSettings = ESettings::Count;
        extendedSettings.pSettings = (uint32_t*)&gSettings;
        extendedSettings.ppSettingNames = gSettingNames;

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.pExtendedSettings = &extendedSettings;
        settings.mShaderTarget = SHADER_TARGET_6_0;
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

// Andorid: Some devices might have support for all wave ops we use in this UT but tha results the WaveReadLaneAt gives us might be
// incorrect.
#if defined(ANDROID)
        gSSSRSupported = false;
#else
        gSSSRSupported = (pRenderer->pGpu->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_BASIC_BIT) &&
                         (pRenderer->pGpu->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_SHUFFLE_BIT) &&
                         (pRenderer->pGpu->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_BALLOT_BIT) &&
                         (pRenderer->pGpu->mWaveOpsSupportFlags & WAVE_OPS_SUPPORT_FLAG_VOTE_BIT);
#endif
        gLastReflectionType = gReflectionType = gSSSRSupported ? SSS_REFLECTION : PP_REFLECTION;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

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

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        initProfiler(&profiler);

        gPPRGpuProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gSSSRGpuProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gCurrentGpuProfileToken = gSSSRGpuProfileToken;

        ComputePBRMaps();

        // Some texture format are not well covered on android devices (R32G32_SFLOAT, R32G32B32A32_SFLOAT)
        // albedo texture use TinyImageFormat_DXBC1_RGBA_UNORM, might need an other sampler
        bool supportLinearFiltering = (pRenderer->pGpu->mFormatCaps[TinyImageFormat_R32G32B32A32_SFLOAT] & FORMAT_CAP_LINEAR_FILTER) &&
                                      (pRenderer->pGpu->mFormatCaps[TinyImageFormat_R32G32_SFLOAT] & FORMAT_CAP_LINEAR_FILTER);

        SamplerDesc samplerDesc = {};
        samplerDesc.mMinFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMagFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMipMapMode = supportLinearFiltering ? MIPMAP_MODE_LINEAR : MIPMAP_MODE_NEAREST;
        samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
        addSampler(pRenderer, &samplerDesc, &pSamplerBilinear);

        supportLinearFiltering = pRenderer->pGpu->mFormatCaps[TinyImageFormat_DXBC1_RGBA_UNORM] & FORMAT_CAP_LINEAR_FILTER;

        samplerDesc = {};
        samplerDesc.mMinFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMagFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMipMapMode = supportLinearFiltering ? MIPMAP_MODE_LINEAR : MIPMAP_MODE_NEAREST;
        samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
        samplerDesc.mMipLodBias = 0.0f;
        samplerDesc.mSetLodRange = false;
        samplerDesc.mMinLod = 0.0f;
        samplerDesc.mMaxLod = 0.0f;
        samplerDesc.mMaxAnisotropy = 8.0f;
        addSampler(pRenderer, &samplerDesc, &pDefaultSampler);

        samplerDesc = {};
        samplerDesc.mMinFilter = FILTER_NEAREST;
        samplerDesc.mMagFilter = FILTER_NEAREST;
        samplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
        samplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
        addSampler(pRenderer, &samplerDesc, &pSamplerNearestClampToEdge);

        // Generate sky box vertex buffer
        float skyBoxPoints[] = {
            0.5f,  -0.5f, -0.5f, 1.0f, // -z
            -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
            -0.5f, 1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

            -0.5f, -0.5f, 0.5f,  1.0f, //-x
            -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
            -0.5f, 1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

            0.5f,  -0.5f, -0.5f, 1.0f, //+x
            0.5f,  -0.5f, 0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
            0.5f,  1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

            -0.5f, -0.5f, 0.5f,  1.0f, // +z
            -0.5f, 0.5f,  0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
            0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

            -0.5f, 0.5f,  -0.5f, 1.0f, //+y
            0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
            0.5f,  1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,

            0.5f,  -0.5f, 0.5f,  1.0f, //-y
            0.5f,  -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, -0.5f,
            -0.5f, 1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,
        };

        SyncToken token = {};

        uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.pData = skyBoxPoints;
        skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
        addResource(&skyboxVbDesc, &token);

        float screenQuadPoints[] = {
            -1.0f, 3.0f, 0.5f, 0.0f, -1.0f, -1.0f, -1.0f, 0.5f, 0.0f, 1.0f, 3.0f, -1.0f, 0.5f, 2.0f, 1.0f,
        };

        uint64_t       screenQuadDataSize = 5 * 3 * sizeof(float);
        BufferLoadDesc screenQuadVbDesc = {};
        screenQuadVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        screenQuadVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        screenQuadVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        screenQuadVbDesc.mDesc.mSize = screenQuadDataSize;
        screenQuadVbDesc.pData = screenQuadPoints;
        screenQuadVbDesc.ppBuffer = &pScreenQuadVertexBuffer;
        addResource(&screenQuadVbDesc, &token);

        // Mesh transform constant buffer
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(MeshInfoUniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pBufferMeshTransforms[i];
            addResource(&ubDesc, NULL);
        }

        // Vis buffer per-frame constant buffer
        {
            BufferLoadDesc vbConstantUBDesc = {};
            vbConstantUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            vbConstantUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            vbConstantUBDesc.mDesc.mSize = sizeof(PerFrameVBConstantsData);
            vbConstantUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            vbConstantUBDesc.mDesc.pName = "gVBConstantsPerFrame Buffer Desc";
            vbConstantUBDesc.pData = NULL;

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                vbConstantUBDesc.ppBuffer = &pBufferVBConstants[i];
                addResource(&vbConstantUBDesc, NULL);
            }
        }

        // Uniform buffer for camera data
        BufferLoadDesc ubCamDesc = {};
        ubCamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubCamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubCamDesc.mDesc.mSize = sizeof(UniformDataSkybox);
        ubCamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubCamDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubCamDesc.ppBuffer = &pBufferUniformCameraSky[i];
            addResource(&ubCamDesc, NULL);
        }

        // Uniform buffer for extended camera data
        BufferLoadDesc ubECamDesc = {};
        ubECamDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubECamDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubECamDesc.mDesc.mSize = sizeof(UniformExtendedCamData);
        ubECamDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubECamDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubECamDesc.ppBuffer = &pBufferUniformExtendedCamera[i];
            addResource(&ubECamDesc, NULL);
        }

        // Uniform buffer for PPR's properties
        BufferLoadDesc ubPPR_ProDesc = {};
        ubPPR_ProDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubPPR_ProDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubPPR_ProDesc.mDesc.mSize = sizeof(UniformPPRProData);
        ubPPR_ProDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubPPR_ProDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubPPR_ProDesc.ppBuffer = &pBufferUniformPPRPro[i];
            addResource(&ubPPR_ProDesc, NULL);
        }

        uint32_t zero = 0;

        BufferLoadDesc SPD_AtomicCounterDesc = {};
        SPD_AtomicCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        SPD_AtomicCounterDesc.mDesc.mElementCount = 1;
        SPD_AtomicCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
        SPD_AtomicCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SPD_AtomicCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        SPD_AtomicCounterDesc.mDesc.mSize = SPD_AtomicCounterDesc.mDesc.mStructStride * SPD_AtomicCounterDesc.mDesc.mElementCount;
        SPD_AtomicCounterDesc.mDesc.pName = "SPD_AtomicCounterBuffer";
        SPD_AtomicCounterDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        SPD_AtomicCounterDesc.pData = &zero;
        SPD_AtomicCounterDesc.ppBuffer = &pSPD_AtomicCounterBuffer;
        addResource(&SPD_AtomicCounterDesc, &token);

        // SSSR
        BufferLoadDesc ubSSSR_ConstDesc = {};
        ubSSSR_ConstDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubSSSR_ConstDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubSSSR_ConstDesc.mDesc.mSize = sizeof(UniformSSSRConstantsData);
        ubSSSR_ConstDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubSSSR_ConstDesc.mDesc.pName = "pSSSR_ConstantsBuffer";
        ubSSSR_ConstDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubSSSR_ConstDesc.ppBuffer = &pSSSR_ConstantsBuffer[i];
            addResource(&ubSSSR_ConstDesc, NULL);
        }

        BufferLoadDesc SSSR_RayCounterDesc = {};
        SSSR_RayCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        SSSR_RayCounterDesc.mDesc.mElementCount = 1;
        SSSR_RayCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_RayCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_RayCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_RayCounterDesc.mDesc.mSize = SSSR_RayCounterDesc.mDesc.mStructStride * SSSR_RayCounterDesc.mDesc.mElementCount;
        SSSR_RayCounterDesc.mDesc.pName = "SSSR_RayCounterBuffer";
        SSSR_RayCounterDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        SSSR_RayCounterDesc.pData = &zero;
        SSSR_RayCounterDesc.ppBuffer = &pSSSR_RayCounterBuffer;
        addResource(&SSSR_RayCounterDesc, &token);

        BufferLoadDesc SSSR_TileCounterDesc = {};
        SSSR_TileCounterDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        SSSR_TileCounterDesc.mDesc.mElementCount = 1;
        SSSR_TileCounterDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_TileCounterDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_TileCounterDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_TileCounterDesc.mDesc.mSize = SSSR_TileCounterDesc.mDesc.mStructStride * SSSR_TileCounterDesc.mDesc.mElementCount;
        SSSR_TileCounterDesc.mDesc.pName = "SSSR_TileCounterBuffer";
        SSSR_TileCounterDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        SSSR_TileCounterDesc.pData = &zero;
        SSSR_TileCounterDesc.ppBuffer = &pSSSR_TileCounterBuffer;
        addResource(&SSSR_TileCounterDesc, &token);

        BufferLoadDesc SSSR_IntersectArgsDesc = {};
        SSSR_IntersectArgsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
        SSSR_IntersectArgsDesc.mDesc.mElementCount = 3;
        SSSR_IntersectArgsDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_IntersectArgsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_IntersectArgsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_IntersectArgsDesc.mDesc.mStartState = RESOURCE_STATE_INDIRECT_ARGUMENT;
        SSSR_IntersectArgsDesc.mDesc.mSize = SSSR_IntersectArgsDesc.mDesc.mStructStride * SSSR_IntersectArgsDesc.mDesc.mElementCount;
        SSSR_IntersectArgsDesc.mDesc.pName = "SSSR_IntersectArgsBuffer";
        SSSR_IntersectArgsDesc.pData = NULL;
        SSSR_IntersectArgsDesc.ppBuffer = &pSSSR_IntersectArgsBuffer;
        addResource(&SSSR_IntersectArgsDesc, NULL);

        BufferLoadDesc SSSR_DenoiserArgsDesc = {};
        SSSR_DenoiserArgsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_INDIRECT_BUFFER;
        SSSR_DenoiserArgsDesc.mDesc.mElementCount = 3;
        SSSR_DenoiserArgsDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_DenoiserArgsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_DenoiserArgsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_DenoiserArgsDesc.mDesc.mStartState = RESOURCE_STATE_INDIRECT_ARGUMENT;
        SSSR_DenoiserArgsDesc.mDesc.mSize = SSSR_DenoiserArgsDesc.mDesc.mStructStride * SSSR_DenoiserArgsDesc.mDesc.mElementCount;
        SSSR_DenoiserArgsDesc.mDesc.pName = "SSSR_DenoiserArgsBuffer";
        SSSR_DenoiserArgsDesc.pData = NULL;
        SSSR_DenoiserArgsDesc.ppBuffer = &pSSSR_DenoiserArgsBuffer;
        addResource(&SSSR_DenoiserArgsDesc, NULL);

        BufferLoadDesc sobolDesc = {};
        sobolDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        sobolDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        sobolDesc.mDesc.mStructStride = sizeof(sobol_256spp_256d[0]);
        sobolDesc.mDesc.mSize = sizeof(sobol_256spp_256d);
        sobolDesc.mDesc.mElementCount = (uint32_t)(sobolDesc.mDesc.mSize / sobolDesc.mDesc.mStructStride);
        sobolDesc.mDesc.pName = "SSSR_SobolBuffer";
        sobolDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        sobolDesc.pData = sobol_256spp_256d;
        sobolDesc.ppBuffer = &pSSSR_SobolBuffer;
        addResource(&sobolDesc, &token);

        BufferLoadDesc rankingTileDesc = {};
        rankingTileDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        rankingTileDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        rankingTileDesc.mDesc.mStructStride = sizeof(rankingTile[0]);
        rankingTileDesc.mDesc.mSize = sizeof(rankingTile);
        rankingTileDesc.mDesc.mElementCount = (uint32_t)(rankingTileDesc.mDesc.mSize / rankingTileDesc.mDesc.mStructStride);
        rankingTileDesc.mDesc.pName = "SSSR_RankingTileBuffer";
        rankingTileDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        rankingTileDesc.pData = rankingTile;
        rankingTileDesc.ppBuffer = &pSSSR_RankingTileBuffer;
        addResource(&rankingTileDesc, &token);

        BufferLoadDesc scramblingTileDesc = {};
        scramblingTileDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        scramblingTileDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        scramblingTileDesc.mDesc.mStructStride = sizeof(scramblingTile[0]);
        scramblingTileDesc.mDesc.mSize = sizeof(scramblingTile);
        scramblingTileDesc.mDesc.mElementCount = (uint32_t)(scramblingTileDesc.mDesc.mSize / scramblingTileDesc.mDesc.mStructStride);
        scramblingTileDesc.mDesc.pName = "SSSR_ScramblingTileBuffer";
        scramblingTileDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        scramblingTileDesc.pData = scramblingTile;
        scramblingTileDesc.ppBuffer = &pSSSR_ScramblingTileBuffer;
        addResource(&scramblingTileDesc, &token);

        // Uniform buffer for light data
        BufferLoadDesc ubLightsDesc = {};
        ubLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubLightsDesc.mDesc.mSize = sizeof(UniformLightData);
        ubLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        ubLightsDesc.pData = NULL;
        ubLightsDesc.ppBuffer = &pBufferUniformLights;
        addResource(&ubLightsDesc, NULL);

        // Uniform buffer for DirectionalLight data
        BufferLoadDesc ubDLightsDesc = {};
        ubDLightsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDLightsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDLightsDesc.mDesc.mSize = sizeof(UniformDirectionalLightData);
        ubDLightsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        ubDLightsDesc.pData = NULL;
        ubDLightsDesc.ppBuffer = &pBufferUniformDirectionalLights;
        addResource(&ubDLightsDesc, NULL);

        // Uniform buffer for extended camera data
        BufferLoadDesc ubPlaneInfoDesc = {};
        ubPlaneInfoDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubPlaneInfoDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubPlaneInfoDesc.mDesc.mSize = sizeof(UniformPlaneInfoData);
        ubPlaneInfoDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubPlaneInfoDesc.pData = NULL;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubPlaneInfoDesc.ppBuffer = &pBufferUniformPlaneInfo[i];
            addResource(&ubPlaneInfoDesc, NULL);
        }

        // Add light to scene
        // Point light
        Light light = {};
        light.mCol = vec4(1.0f, 0.5f, 0.1f, 0.0f);
        light.mPos = vec4(15.0f, 40.0f, 4.7f, 0.0f);
        light.mRadius = 30.0f;
        light.mIntensity = 1.0f;
        gUniformDataLights.mLights[0] = light;

        light.mCol = vec4(1.0f, 0.1f, 0.5f, 0.0f);
        light.mPos = vec4(-25.0f, 40.0f, -3.7f, 0.0f);
        light.mRadius = 30.0f;
        light.mIntensity = 1.0f;
        gUniformDataLights.mLights[1] = light;

        light.mCol = vec4(0.5f, 1.0f, 0.1f, 0.0f);
        light.mPos = vec4(40.0f, 40.0f, 4.7f, 0.0f);
        light.mRadius = 30.0f;
        light.mIntensity = 1.0f;
        gUniformDataLights.mLights[2] = light;

        light.mCol = vec4(0.5f, 0.1f, 1.0f, 0.0f);
        light.mPos = vec4(-60.0f, 40.0f, -3.7f, 0.0f);
        light.mRadius = 30.0f;
        light.mIntensity = 1.0f;
        gUniformDataLights.mLights[3] = light;

        gUniformDataLights.mCurrAmountOfLights = 4;
        BufferUpdateDesc lightBuffUpdateDesc = { pBufferUniformLights };
        beginUpdateResource(&lightBuffUpdateDesc);
        memcpy(lightBuffUpdateDesc.pMappedData, &gUniformDataLights, sizeof(gUniformDataLights));
        endUpdateResource(&lightBuffUpdateDesc);

        // Directional light
        DirectionalLight dLight;
        dLight.mCol = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        dLight.mDir = vec4(-1.0f, -1.5f, 1.0f, 0.0f);

        gUniformDataDirectionalLights.mLights[0] = dLight;
        gUniformDataDirectionalLights.mCurrAmountOfDLights = 1;
        BufferUpdateDesc directionalLightBuffUpdateDesc = { pBufferUniformDirectionalLights };
        beginUpdateResource(&directionalLightBuffUpdateDesc);
        memcpy(directionalLightBuffUpdateDesc.pMappedData, &gUniformDataDirectionalLights, sizeof(gUniformDataDirectionalLights));
        endUpdateResource(&directionalLightBuffUpdateDesc);

        // We need to allocate enough indices for the entire scene
        uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};

        GeometryLoadDesc sceneLoadDesc = {};
        pScene = initSanMiguel(&sceneLoadDesc, gResourceSyncToken, false);

        gSanMiguelModelMat = mat4::scale(vec3(SCENE_SCALE));
        gMeshCount = pScene->geom->mDrawArgCount;
        gMaterialCount = pScene->geom->mDrawArgCount;
        pSanMiguelModel = pScene->geom;
        pVBMeshInstances = (VBMeshInstance*)tf_calloc(gMeshCount, sizeof(VBMeshInstance));

        gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gNormalMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gSpecularMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);

        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            TextureLoadDesc desc = {};
            desc.pFileName = pScene->textures[i];
            desc.ppTexture = &gDiffuseMapsStorage[i];
            // Textures representing color should be stored in SRGB or HDR format
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;

            addResource(&desc, &token);

            TextureLoadDesc descNormal = {};
            descNormal.pFileName = pScene->normalMaps[i];
            descNormal.ppTexture = &gNormalMapsStorage[i];
            addResource(&descNormal, &token);

            TextureLoadDesc descSpec = {};
            descSpec.pFileName = pScene->specularMaps[i];
            descSpec.ppTexture = &gSpecularMapsStorage[i];
            addResource(&descSpec, &token);
        }

        MeshConstants* meshConstants = (MeshConstants*)tf_malloc(gMeshCount * sizeof(MeshConstants));

        // Calculate mesh constants and filter containers
        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            MaterialFlags materialFlag = pScene->materialFlags[i];
            uint32_t      geomSet = materialFlag & MATERIAL_FLAG_ALPHA_TESTED ? GEOMSET_ALPHA_CUTOUT : GEOMSET_OPAQUE;
            visibilityBufferFilteredIndexCount[geomSet] += (pScene->geom->pDrawArgs + i)->mIndexCount;
            pVBMeshInstances[i].mGeometrySet = geomSet;
            pVBMeshInstances[i].mMeshIndex = i;
            pVBMeshInstances[i].mTriangleCount = (pScene->geom->pDrawArgs + i)->mIndexCount / 3;
            pVBMeshInstances[i].mInstanceIndex = INSTANCE_INDEX_NONE;

            meshConstants[i].indexOffset = pSanMiguelModel->pDrawArgs[i].mStartIndex;
            meshConstants[i].vertexOffset = pSanMiguelModel->pDrawArgs[i].mVertexOffset;
            meshConstants[i].materialID = i;
            meshConstants[i].twoSided = (pScene->materialFlags[i] & MATERIAL_FLAG_TWO_SIDED) ? 1 : 0;
        }

        BufferLoadDesc meshConstantDesc = {};
        meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        meshConstantDesc.mDesc.mElementCount = gMeshCount;
        meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
        meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
        meshConstantDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        meshConstantDesc.ppBuffer = &pBufferMeshConstants;
        meshConstantDesc.pData = meshConstants;
        meshConstantDesc.mDesc.pName = "Mesh Constant desc";
        addResource(&meshConstantDesc, &token);

        VisibilityBufferDesc vbDesc = {};
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = 1; // We don't use Async Compute for triangle filtering, 1 buffer is enough
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
        updateVBMeshFilterGroupsDesc.mNumMeshInstance = gMeshCount;
        updateVBMeshFilterGroupsDesc.pVBMeshInstances = pVBMeshInstances;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            updateVBMeshFilterGroupsDesc.mFrameIndex = i;
            gVBPreFilterStats[i] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
        }

        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            gPerFrameData[frameIdx].mDrawCount[GEOMSET_OPAQUE] = gVBPreFilterStats[frameIdx].mGeomsetMaxDrawCounts[GEOMSET_OPAQUE];
            gPerFrameData[frameIdx].mDrawCount[GEOMSET_ALPHA_CUTOUT] =
                gVBPreFilterStats[frameIdx].mGeomsetMaxDrawCounts[GEOMSET_ALPHA_CUTOUT];
        }

        /************************************************************************/
        ////////////////////////////////////////////////

        CameraMotionParameters camParameters{ 200.0f, 150.0f, 300.0f };
        vec3                   camPos{ 95.5f, 47.2f, 70.75f };
        vec3                   lookAt{ -1.67f, 9.58f, 23.75f };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(camParameters);

        AddCustomInputBindings();

        waitForToken(&token);

        tf_free(meshConstants);
        initScreenshotCapturer(pRenderer, pGraphicsQueue, GetName());
        return true;
    }

    void Exit()
    {
        exitScreenshotCapturer();
        exitCameraController(pCameraController);
        tf_free(gInitializeVal);
        gInitializeVal = NULL;

        gFrameIndex = 0;
        gFrameCount = 0;

        exitGpuProfiler(gSSSRGpuProfileToken);
        exitGpuProfiler(gPPRGpuProfileToken);

        exitProfiler();

        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            removeResource(gDiffuseMapsStorage[i]);
            removeResource(gNormalMapsStorage[i]);
            removeResource(gSpecularMapsStorage[i]);
        }

        tf_free(gDiffuseMapsStorage);
        tf_free(gNormalMapsStorage);
        tf_free(gSpecularMapsStorage);
        removeResource(pScene->geomData);
        pScene->geomData = NULL;
        exitSanMiguel(pScene);

        removeResource(pSpecularMap);
        removeResource(pIrradianceMap);
        removeResource(pSkybox);
        removeResource(pBRDFIntegrationMap);

        removeResource(pSPD_AtomicCounterBuffer);
        removeResource(pSSSR_RayCounterBuffer);
        removeResource(pSSSR_TileCounterBuffer);
        removeResource(pSSSR_IntersectArgsBuffer);
        removeResource(pSSSR_DenoiserArgsBuffer);
        removeResource(pSSSR_SobolBuffer);
        removeResource(pSSSR_RankingTileBuffer);
        removeResource(pSSSR_ScramblingTileBuffer);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pBufferMeshTransforms[i]);
            removeResource(pBufferVBConstants[i]);
            removeResource(pBufferUniformPlaneInfo[i]);
            removeResource(pBufferUniformPPRPro[i]);
            removeResource(pBufferUniformExtendedCamera[i]);
            removeResource(pBufferUniformCameraSky[i]);
            removeResource(pSSSR_ConstantsBuffer[i]);
        }

        tf_free(pVBMeshInstances);

        removeResource(pBufferMeshConstants);

        removeResource(pBufferUniformLights);
        removeResource(pBufferUniformDirectionalLights);
        removeResource(pSkyboxVertexBuffer);
        removeResource(pScreenQuadVertexBuffer);

        removeResource(pSanMiguelModel);

        exitUserInterface();

        exitFontSystem();

        removeSampler(pRenderer, pSamplerBilinear);
        removeSampler(pRenderer, pDefaultSampler);
        removeSampler(pRenderer, pSamplerNearestClampToEdge);

        exitVisibilityBuffer(pVisibilityBuffer);

        // Remove commands and command pool
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitQueue(pRenderer, pGraphicsQueue);
        exitRootSignature(pRenderer);
        // Remove resource loader and renderer
        exitResourceLoaderInterface(pRenderer);
        exitRenderer(pRenderer);
        exitGPUConfiguration();

        pRenderer = NULL;
    }

    void ComputePBRMaps()
    {
        Shader*        pBRDFIntegrationShader = NULL;
        Pipeline*      pBRDFIntegrationPipeline = NULL;
        Shader*        pIrradianceShader = NULL;
        Pipeline*      pIrradiancePipeline = NULL;
        Shader*        pSpecularShader = NULL;
        Pipeline*      pSpecularPipeline = NULL;
        Sampler*       pSkyboxSampler = NULL;
        DescriptorSet* pDescriptorSetPBRPersistent = { NULL };
        DescriptorSet* pDescriptorSetPBRPerFrame = { NULL };
        DescriptorSet* pDescriptorSetPBRPerBatch = { NULL };

        static const int skyboxIndex = 0;
        const char*      skyboxNames[] = {
            "LA_Helipad3D.tex",
        };
        // PBR Texture values (these values are mirrored on the shaders).
        static const uint32_t gBRDFIntegrationSize = 512;
        static const uint32_t gSkyboxMips = 11;
        static const uint32_t gIrradianceSize = 32;
        static const uint32_t gSpecularSize = 128;
        static const uint32_t gSpecularMips = (uint)log2(gSpecularSize) + 1;

        // Some texture format are not well covered on android devices (R32G32_SFLOAT, D32_SFLOAT, R32G32B32A32_SFLOAT)
        bool supportLinearFiltering = pRenderer->pGpu->mFormatCaps[TinyImageFormat_R32G32B32A32_SFLOAT] & FORMAT_CAP_LINEAR_FILTER;

        SamplerDesc samplerDesc = {};
        samplerDesc.mMinFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMagFilter = supportLinearFiltering ? FILTER_LINEAR : FILTER_NEAREST;
        samplerDesc.mMipMapMode = supportLinearFiltering ? MIPMAP_MODE_LINEAR : MIPMAP_MODE_NEAREST;
        samplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
        samplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
        samplerDesc.mMipLodBias = 0.0f;
        samplerDesc.mSetLodRange = false;
        samplerDesc.mMinLod = 0.0f;
        samplerDesc.mMaxLod = 0.0f;
        samplerDesc.mMaxAnisotropy = 16.0f;

        addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

        // Load the skybox panorama texture.
        SyncToken       token = {};
        TextureLoadDesc skyboxDesc = {};
        skyboxDesc.pFileName = skyboxNames[skyboxIndex];
        skyboxDesc.ppTexture = &pSkybox;
        addResource(&skyboxDesc, &token);

        TextureDesc irrImgDesc = {};
        irrImgDesc.mArraySize = 6;
        irrImgDesc.mDepth = 1;
        irrImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        irrImgDesc.mHeight = gIrradianceSize;
        irrImgDesc.mWidth = gIrradianceSize;
        irrImgDesc.mMipLevels = 1;
        irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
        irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
        irrImgDesc.pName = "irrImgBuff";

        TextureLoadDesc irrLoadDesc = {};
        irrLoadDesc.pDesc = &irrImgDesc;
        irrLoadDesc.ppTexture = &pIrradianceMap;
        addResource(&irrLoadDesc, &token);

        TextureDesc specImgDesc = {};
        specImgDesc.mArraySize = 6;
        specImgDesc.mDepth = 1;
        specImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        specImgDesc.mHeight = gSpecularSize;
        specImgDesc.mWidth = gSpecularSize;
        specImgDesc.mMipLevels = gSpecularMips;
        specImgDesc.mSampleCount = SAMPLE_COUNT_1;
        specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
        specImgDesc.pName = "specImgBuff";

        TextureLoadDesc specImgLoadDesc = {};
        specImgLoadDesc.pDesc = &specImgDesc;
        specImgLoadDesc.ppTexture = &pSpecularMap;
        addResource(&specImgLoadDesc, &token);

        // Create empty texture for BRDF integration map.
        TextureLoadDesc brdfIntegrationLoadDesc = {};
        TextureDesc     brdfIntegrationDesc = {};
        brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
        brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
        brdfIntegrationDesc.mDepth = 1;
        brdfIntegrationDesc.mArraySize = 1;
        brdfIntegrationDesc.mMipLevels = 1;
        brdfIntegrationDesc.mFormat = TinyImageFormat_R32G32_SFLOAT;
        brdfIntegrationDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
        brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
        brdfIntegrationLoadDesc.ppTexture = &pBRDFIntegrationMap;
        addResource(&brdfIntegrationLoadDesc, &token);

        GPUPresetLevel presetLevel = pRenderer->pGpu->mGpuVendorPreset.mPresetLevel;

        const char* brdfIntegrationShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
            "BRDFIntegration_SAMPLES_0.comp",   // GPU_PRESET_NONE
            "BRDFIntegration_SAMPLES_0.comp",   // GPU_PRESET_OFFICE
            "BRDFIntegration_SAMPLES_32.comp",  // GPU_PRESET_VERYLOW
            "BRDFIntegration_SAMPLES_64.comp",  // GPU_PRESET_LOW
            "BRDFIntegration_SAMPLES_128.comp", // GPU_PRESET_MEDIUM
            "BRDFIntegration_SAMPLES_256.comp", // GPU_PRESET_HIGH
            "BRDFIntegration_SAMPLES_1024.comp" // GPU_PRESET_ULTRA
        };

        const char* irradianceShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
            "computeIrradianceMap_SAMPLE_DELTA_05.comp",   // GPU_PRESET_NONE
            "computeIrradianceMap_SAMPLE_DELTA_05.comp",   // GPU_PRESET_OFFICE
            "computeIrradianceMap_SAMPLE_DELTA_05.comp",   // GPU_PRESET_VERYLOW
            "computeIrradianceMap_SAMPLE_DELTA_025.comp",  // GPU_PRESET_LOW
            "computeIrradianceMap_SAMPLE_DELTA_0125.comp", // GPU_PRESET_MEDIUM
            "computeIrradianceMap_SAMPLE_DELTA_005.comp",  // GPU_PRESET_HIGH
            "computeIrradianceMap_SAMPLE_DELTA_0025.comp"  // GPU_PRESET_ULTRA
        };

        const char* specularShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
            "computeSpecularMap_SAMPLES_0.comp",   // GPU_PRESET_NONE
            "computeSpecularMap_SAMPLES_0.comp",   // GPU_PRESET_OFFICE
            "computeSpecularMap_SAMPLES_32.comp",  // GPU_PRESET_VERYLOW
            "computeSpecularMap_SAMPLES_64.comp",  // GPU_PRESET_LOW
            "computeSpecularMap_SAMPLES_128.comp", // GPU_PRESET_MEDIUM
            "computeSpecularMap_SAMPLES_256.comp", // GPU_PRESET_HIGH
            "computeSpecularMap_SAMPLES_1024.comp" // GPU_PRESET_ULTRA
        };

        ShaderLoadDesc brdfIntegrationShaderDesc = {};
        brdfIntegrationShaderDesc.mComp.pFileName = brdfIntegrationShaders[presetLevel];

        ShaderLoadDesc irradianceShaderDesc = {};
        irradianceShaderDesc.mComp.pFileName = irradianceShaders[presetLevel];

        ShaderLoadDesc specularShaderDesc = {};
        specularShaderDesc.mComp.pFileName = specularShaders[presetLevel];

        addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
        addShader(pRenderer, &specularShaderDesc, &pSpecularShader);
        addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);

        DescriptorSetDesc setDesc = SRT_SET_DESC(ComputeSpecularSrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPBRPersistent);
        setDesc = SRT_SET_DESC(ComputeSpecularSrtData, PerBatch, gSkyboxMips, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPBRPerBatch);

        setDesc = SRT_SET_DESC(ComputeSpecularSrtData, PerFrame, gSkyboxMips, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPBRPerFrame);

        PipelineDesc desc = {};
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(ComputeSpecularSrtData, Persistent), SRT_LAYOUT_DESC(ComputeSpecularSrtData, PerFrame),
                             SRT_LAYOUT_DESC(ComputeSpecularSrtData, PerBatch), NULL);
        desc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
        pipelineSettings.pShaderProgram = pIrradianceShader;
        addPipeline(pRenderer, &desc, &pIrradiancePipeline);
        pipelineSettings.pShaderProgram = pSpecularShader;

        addPipeline(pRenderer, &desc, &pSpecularPipeline);
        pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
        addPipeline(pRenderer, &desc, &pBRDFIntegrationPipeline);

        waitForToken(&token);

        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        Cmd*              pCmd = elem.pCmds[0];

        DescriptorData params[2] = {};
        params[0].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, Persistent, gSrcTexture);
        params[0].ppTextures = &pSkybox;
        params[1].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, Persistent, gSkyboxSampler);
        params[1].ppSamplers = &pSkyboxSampler;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPBRPersistent, 2, params);

        params[0].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, PerBatch, gDstTextureRW);
        params[0].ppTextures = &pBRDFIntegrationMap;
        params[1].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, PerBatch, gDstTextureArrayRW);
        params[1].ppTextures = &pIrradianceMap;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPBRPerBatch, 2, params);
        struct PrecomputeSkySpecularData
        {
            uint  mipSize;
            float roughness;
        };
        PrecomputeSkySpecularData data[8] = {};
        Buffer*                   pUniformBufferSpecularConfig[8] = { NULL };
        for (uint32_t i = 0; i < gSpecularMips; i++)
        {
            data[i].roughness = (float)i / (float)(gSpecularMips - 1);
            data[i].mipSize = gSpecularSize >> i;
            BufferLoadDesc specularDataBufferDesc = {};
            specularDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            specularDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            specularDataBufferDesc.mDesc.mSize = sizeof(PrecomputeSkySpecularData);
            specularDataBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            specularDataBufferDesc.pData = &data[i];
            specularDataBufferDesc.ppBuffer = &pUniformBufferSpecularConfig[i];
            addResource(&specularDataBufferDesc, NULL);
        }

        for (uint32_t i = 0; i < gSpecularMips; i++)
        {
            params[0].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, PerFrame, gComputeSpecularParams);
            params[0].ppBuffers = &pUniformBufferSpecularConfig[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetPBRPerFrame, 1, params);
            params[0].mIndex = SRT_RES_IDX(ComputeSpecularSrtData, PerBatch, gDstTexturePerDraw);
            params[0].ppTextures = &pSpecularMap;
            params[0].mUAVMipSlice = (uint16_t)i;
            updateDescriptorSet(pRenderer, i, pDescriptorSetPBRPerBatch, 1, params);
        }

        // Compute the BRDF Integration map.
        resetCmdPool(pRenderer, elem.pCmdPool);
        beginCmd(pCmd);

        cmdBindPipeline(pCmd, pBRDFIntegrationPipeline);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPBRPersistent);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPBRPerBatch);
        const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mNumThreadsPerGroup;
        cmdDispatch(pCmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

        TextureBarrier srvBarrier[1] = { { pBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };

        cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarrier, 0, NULL);

        /************************************************************************/
        // Compute sky irradiance
        /************************************************************************/
        params[0] = {};
        params[1] = {};
        cmdBindPipeline(pCmd, pIrradiancePipeline);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPBRPersistent);
        pThreadGroupSize = pIrradianceShader->mNumThreadsPerGroup;
        cmdDispatch(pCmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);

        /************************************************************************/
        // Compute specular sky
        /************************************************************************/
        cmdBindPipeline(pCmd, pSpecularPipeline);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPBRPersistent);
        for (uint32_t i = 0; i < gSpecularMips; i++)
        {
            cmdBindDescriptorSet(pCmd, i, pDescriptorSetPBRPerBatch);
            cmdBindDescriptorSet(pCmd, i, pDescriptorSetPBRPerFrame);
            pThreadGroupSize = pIrradianceShader->mNumThreadsPerGroup;
            cmdDispatch(pCmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
        }
        /************************************************************************/
        /************************************************************************/
        TextureBarrier srvBarriers2[2] = { { pIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
                                           { pSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
        cmdResourceBarrier(pCmd, 0, NULL, 2, srvBarriers2, 0, NULL);

        endCmd(pCmd);

        FlushResourceUpdateDesc flushDesc = {};
        flushResourceUpdates(&flushDesc);
        waitForFences(pRenderer, 1, &flushDesc.pOutFence);

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.ppCmds = &pCmd;
        submitDesc.pSignalFence = elem.pFence;
        submitDesc.mSubmitDone = true;
        queueSubmit(pGraphicsQueue, &submitDesc);
        waitForFences(pRenderer, 1, &elem.pFence);

        removeDescriptorSet(pRenderer, pDescriptorSetPBRPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPBRPersistent);
        removeDescriptorSet(pRenderer, pDescriptorSetPBRPerBatch);
        removePipeline(pRenderer, pSpecularPipeline);
        removeShader(pRenderer, pSpecularShader);
        removePipeline(pRenderer, pIrradiancePipeline);
        removeShader(pRenderer, pIrradianceShader);

        for (uint32_t i = 0; i < gSpecularMips; i++)
        {
            removeResource(pUniformBufferSpecularConfig[i]);
        }

        removePipeline(pRenderer, pBRDFIntegrationPipeline);
        removeShader(pRenderer, pBRDFIntegrationShader);
        removeSampler(pRenderer, pSkyboxSampler);
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        gSceneRes = getGPUCfgSceneResolution(mSettings.mWidth, mSettings.mHeight);

        gResourceSyncStartToken = getLastTokenCompleted();

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent("Screen Space Reflections", &guiDesc, &pGui);

            static const char* enumRenderModeNames[] = { "Render Scene Only", "Render Reflections Only", "Render Scene with Reflections",
                                                         "Render Scene with exclusive Reflections" };

            static const char* enumReflectionTypeNames[] = { "Pixel Projected Reflections", "Stochastic Screen Space Reflections" };

            DropdownWidget ddRenderMode;
            ddRenderMode.pData = &gRenderMode;
            ddRenderMode.pNames = enumRenderModeNames;
            ddRenderMode.mCount = sizeof(enumRenderModeNames) / sizeof(enumRenderModeNames[0]);
            luaRegisterWidget(uiAddComponentWidget(pGui, "Render Mode", &ddRenderMode, WIDGET_TYPE_DROPDOWN));

            DropdownWidget ddReflType;
            ddReflType.pData = &gReflectionType;
            ddReflType.pNames = enumReflectionTypeNames;
            ddReflType.mCount = sizeof(enumReflectionTypeNames) / sizeof(enumReflectionTypeNames[0]);
            luaRegisterWidget(uiAddComponentWidget(pGui, "Reflection Type", &ddReflType, WIDGET_TYPE_DROPDOWN));

            SliderFloatWidget ground;
            ground.pData = &gUniformDataExtenedCamera.mGroundRoughness;
            ground.mMax = 1.0f;
            ground.mMin = 0.0f;
            luaRegisterWidget(uiAddComponentWidget(pGui, "Ground roughness", &ground, WIDGET_TYPE_SLIDER_FLOAT));
            ground.pData = &gUniformDataExtenedCamera.mGroundMetallic;
            luaRegisterWidget(uiAddComponentWidget(pGui, "Ground metallic", &ground, WIDGET_TYPE_SLIDER_FLOAT));

            CheckboxWidget useEnvMap;
            useEnvMap.pData = &gUseEnvMap;
            luaRegisterWidget(uiAddComponentWidget(pGui, "Use Env map", &useEnvMap, WIDGET_TYPE_CHECKBOX));

            Color3PickerWidget envColor;
            envColor.pData = &gUniformDataExtenedCamera.mEnvColor;
            luaRegisterWidget(uiAddComponentWidget(pGui, "Env color", &envColor, WIDGET_TYPE_COLOR3_PICKER));

            CheckboxWidget holePatchCheck;
            holePatchCheck.pData = &gUseHolePatching;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Use Holepatching", &holePatchCheck, WIDGET_TYPE_CHECKBOX));

            CheckboxWidget holePatchExpCheck;
            holePatchExpCheck.pData = &gUseExpensiveHolePatching;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Use Expensive Holepatching", &holePatchExpCheck, WIDGET_TYPE_CHECKBOX));

            // pGui->AddWidget(CheckboxWidget("Use Normalmap", &gUseNormalMap));

            CheckboxWidget fadeCheck;
            fadeCheck.pData = &gUseFadeEffect;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Use Fade Effect", &fadeCheck, WIDGET_TYPE_CHECKBOX));

            SliderFloatWidget pprIntensitySlider;
            pprIntensitySlider.pData = &gRRP_Intensity;
            pprIntensitySlider.mMin = 0.0f;
            pprIntensitySlider.mMax = 1.0f;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Intensity of PPR", &pprIntensitySlider, WIDGET_TYPE_SLIDER_FLOAT));

            SliderUintWidget numPlanesSlider;
            numPlanesSlider.pData = &gPlaneNumber;
            numPlanesSlider.mMin = 1;
            numPlanesSlider.mMax = 4;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Number of Planes", &numPlanesSlider, WIDGET_TYPE_SLIDER_UINT));

            SliderFloatWidget mainPlaneSizeSlider;
            mainPlaneSizeSlider.pData = &gPlaneSize;
            mainPlaneSizeSlider.mMin = 5.0f;
            mainPlaneSizeSlider.mMax = 500.0;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Size of Main Plane", &mainPlaneSizeSlider, WIDGET_TYPE_SLIDER_FLOAT));

            SliderFloatWidget nonMainPlaneRotSlider;
            nonMainPlaneRotSlider.pData = &gPlaneRotationOffset;
            nonMainPlaneRotSlider.mMin = -180.0f;
            nonMainPlaneRotSlider.mMax = 180.0f;
            luaRegisterWidget(
                uiAddDynamicWidgets(&PPR_Widgets, "Rotation of Non-Main Planes", &nonMainPlaneRotSlider, WIDGET_TYPE_SLIDER_FLOAT));

            CheckboxWidget debugNonProjected;
            debugNonProjected.pData = &gDebugNonProjectedPixels;
            luaRegisterWidget(uiAddDynamicWidgets(&PPR_Widgets, "Debug Non Projected Pixels", &debugNonProjected, WIDGET_TYPE_CHECKBOX));

            if (gSSSRSupported)
            {
                OneLineCheckboxWidget olCheckbox;
                olCheckbox.pData = &gUseSPD;
                olCheckbox.mColor = float4(1.f);
                luaRegisterWidget(
                    uiAddDynamicWidgets(&SSSR_Widgets, "Use Singlepass Downsampler", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

                olCheckbox.pData = &gSSSR_SkipDenoiser;
                olCheckbox.mColor = float4(1.f);
                luaRegisterWidget(
                    uiAddDynamicWidgets(&SSSR_Widgets, "Show Intersection Results", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

                SliderUintWidget uintSlider;
                uintSlider.pData = &gSSSR_MaxTravelsalIntersections;
                uintSlider.mMin = 0;
                uintSlider.mMax = 256;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Max Traversal Iterations", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

                uintSlider.pData = &gSSSR_MinTravelsalOccupancy;
                uintSlider.mMin = 0;
                uintSlider.mMax = 32;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Min Traversal Occupancy", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

                uintSlider.pData = &gSSSR_MostDetailedMip;
                uintSlider.mMin = 0;
                uintSlider.mMax = 5;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Most Detailed Level", &uintSlider, WIDGET_TYPE_SLIDER_UINT));

                SliderFloatWidget floatSlider;
                floatSlider.pData = &gSSSR_DepthThickness;
                floatSlider.mMin = 0.0f;
                floatSlider.mMax = 0.3f;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Depth Buffer Thickness", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

                floatSlider.pData = &gSSSR_RougnessThreshold;
                floatSlider.mMin = 0.0f;
                floatSlider.mMax = 1.0f;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Roughness Threshold", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

                floatSlider.pData = &pSSSR_TemporalStability;
                floatSlider.mMin = 0.0f;
                floatSlider.mMax = 1.0f;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Temporal Stability", &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

                olCheckbox.pData = &gSSSR_TemporalVarianceEnabled;
                olCheckbox.mColor = float4(1.f);
                luaRegisterWidget(
                    uiAddDynamicWidgets(&SSSR_Widgets, "Enable Variance Guided Tracing", &olCheckbox, WIDGET_TYPE_ONE_LINE_CHECKBOX));

                RadioButtonWidget radiobutton;
                radiobutton.pData = &gSSSR_SamplesPerQuad;
                radiobutton.mRadioId = 1;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "1 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

                radiobutton.pData = &gSSSR_SamplesPerQuad;
                radiobutton.mRadioId = 2;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "2 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

                radiobutton.pData = &gSSSR_SamplesPerQuad;
                radiobutton.mRadioId = 4;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "4 Sample Per Quad", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

                radiobutton.pData = &gSSSR_EAWPassCount;
                radiobutton.mRadioId = 1;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "1 EAW Pass", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));

                radiobutton.pData = &gSSSR_EAWPassCount;
                radiobutton.mRadioId = 3;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "3 EAW Pass", &radiobutton, WIDGET_TYPE_RADIO_BUTTON));
            }
            else
            {
                LabelWidget notSupportedLabel;
                luaRegisterWidget(uiAddDynamicWidgets(&SSSR_Widgets, "Not supported by your GPU", &notSupportedLabel, WIDGET_TYPE_LABEL));
            }

            DropdownWidget ddTestScripts;
            ddTestScripts.pData = &gCurrentScriptIndex;
            ddTestScripts.pNames = gTestScripts;
            ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
            luaRegisterWidget(uiAddComponentWidget(pGui, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGui, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);

            if (gReflectionType == PP_REFLECTION)
            {
                uiShowDynamicWidgets(&PPR_Widgets, pGui);
            }
            else if (gReflectionType == SSS_REFLECTION)
            {
                uiShowDynamicWidgets(&SSSR_Widgets, pGui);
            }

            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            addRenderTargets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        waitForAllResourceLoads();
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

        gFrameIndex = 0;
        gFrameCount = 0;

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

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            waitForToken(&gResourceSyncToken);
            waitForAllResourceLoads();

            gResourceSyncToken = 0;

            removeDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            removeRenderTargets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveComponent(pGui);
            uiRemoveDynamicWidgets(&PPR_Widgets);
            uiRemoveDynamicWidgets(&SSSR_Widgets);

            unloadProfilerUI();
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeShaders();
        }
    }

    void Update(float deltaTime)
    {
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

        // Update camera
        CameraMatrix viewMat = pCameraController->getViewMatrix();
        const float  aspectInverse = (float)gSceneRes.mHeight / (float)gSceneRes.mWidth;
        const float  horizontalFov = PI / 2.0f;
        const float  nearPlane = 0.1f;
        const float  farPlane = 1000.f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontalFov, aspectInverse, nearPlane, farPlane);
        CameraMatrix ViewProjMat = projMat * viewMat;
        CameraMatrix mvp = ViewProjMat * gSanMiguelModelMat;

        gMeshInfoUniformData[gFrameIndex].mPrevWorldViewProjMat = gMeshInfoUniformData[gFrameIndex].mWorldViewProjMat;
        gMeshInfoUniformData[gFrameIndex].mWorldViewProjMat = mvp.mCamera;

        gVBConstants[gFrameIndex].transform[VIEW_CAMERA].mvp = mvp;
        gVBConstants[gFrameIndex].cullingViewports[VIEW_CAMERA].windowSize = { (float)gSceneRes.mWidth, (float)gSceneRes.mHeight };
        gVBConstants[gFrameIndex].cullingViewports[VIEW_CAMERA].sampleCount = 1;

        // data uniforms
        gUniformDataExtenedCamera.mCameraWorldPos = vec4(pCameraController->getViewPosition(), 1.0);
        gUniformDataExtenedCamera.mViewMat = viewMat.mCamera;
        gUniformDataExtenedCamera.mInvViewMat = inverse(gUniformDataExtenedCamera.mViewMat);
        gUniformDataExtenedCamera.mProjMat = projMat.mCamera;
        gUniformDataExtenedCamera.mViewProjMat = ViewProjMat.mCamera;
        gUniformDataExtenedCamera.mInvViewProjMat = inverse(ViewProjMat.mCamera);
        gUniformDataExtenedCamera.mViewPortSize = { (float)gSceneRes.mWidth, (float)gSceneRes.mHeight };
        gUniformDataExtenedCamera.mNear = nearPlane;
        gUniformDataExtenedCamera.mFar = farPlane;
        gUniformDataExtenedCamera.mUseEnvMap = gUseEnvMap ? 1 : 0;

        // projection uniforms
        gUniformPPRProData.renderMode = gRenderMode;
        gUniformPPRProData.useHolePatching =
            ((gReflectionType == PP_REFLECTION || !gSSSRSupported) && gUseHolePatching == true) ? 1.0f : 0.0f;
        gUniformPPRProData.useExpensiveHolePatching = gUseExpensiveHolePatching == true ? 1.0f : 0.0f;
        gUniformPPRProData.useNormalMap = gUseNormalMap == true ? 1.0f : 0.0f;
        gUniformPPRProData.useFadeEffect = gUseFadeEffect == true ? 1.0f : 0.0f;
        gUniformPPRProData.debugNonProjected = gDebugNonProjectedPixels == true ? 1.0f : 0.0f;
        gUniformPPRProData.intensity = (gReflectionType == PP_REFLECTION || !gSSSRSupported) ? gRRP_Intensity : 1.0f;

        // Planes
        gUniformDataPlaneInfo.numPlanes = gPlaneNumber;
        gUniformDataPlaneInfo.planeInfo[0].centerPoint = vec4(0.0, 24.0f, 0.0f, 0.0);
        gUniformDataPlaneInfo.planeInfo[0].size = vec4(gPlaneSize);

        gUniformDataPlaneInfo.planeInfo[1].centerPoint = vec4(10.0, 40.0f, 20.0f, 0.0);
        gUniformDataPlaneInfo.planeInfo[1].size = vec4(90.0f, 20.0f, 0.0f, 0.0f);

        gUniformDataPlaneInfo.planeInfo[2].centerPoint = vec4(10.0, 40.0f, 70.f, 0.0);
        gUniformDataPlaneInfo.planeInfo[2].size = vec4(90.0f, 20.0f, 0.0f, 0.0f);

        gUniformDataPlaneInfo.planeInfo[3].centerPoint = vec4(10.0, 50.0f, 0.9f, 0.0);
        gUniformDataPlaneInfo.planeInfo[3].size = vec4(100.f);

        static const float oneDegreeInRad = 0.01745329251994329576923690768489f;

        mat4 basicMat;
        basicMat[0] = vec4(1.0, 0.0, 0.0, 0.0);  // tan
        basicMat[1] = vec4(0.0, 0.0, -1.0, 0.0); // bitan
        basicMat[2] = vec4(0.0, 1.0, 0.0, 0.0);  // normal
        basicMat[3] = vec4(0.0, 0.0, 0.0, 1.0);

        gUniformDataPlaneInfo.planeInfo[0].rotMat = basicMat;
        gUniformDataPlaneInfo.planeInfo[1].rotMat = basicMat.rotationX(oneDegreeInRad * (-80.0f + gPlaneRotationOffset));
        gUniformDataPlaneInfo.planeInfo[2].rotMat = basicMat.rotationX(oneDegreeInRad * (-100.0f + gPlaneRotationOffset));
        gUniformDataPlaneInfo.planeInfo[3].rotMat = basicMat.rotationX(oneDegreeInRad * (90.0f + gPlaneRotationOffset));

        gUniformSSSRConstantsData.g_prev_view_proj =
            transpose(transpose(gUniformSSSRConstantsData.g_proj) * transpose(gUniformSSSRConstantsData.g_view));
        gUniformSSSRConstantsData.g_inv_view_proj = transpose(gUniformDataExtenedCamera.mInvViewProjMat);
        gUniformSSSRConstantsData.g_proj = transpose(projMat.mCamera);
        gUniformSSSRConstantsData.g_inv_proj = transpose(inverse(projMat.mCamera));
        gUniformSSSRConstantsData.g_view = transpose(gUniformDataExtenedCamera.mViewMat);
        gUniformSSSRConstantsData.g_inv_view = transpose(inverse(gUniformDataExtenedCamera.mViewMat));

        gUniformSSSRConstantsData.g_frame_index = gFrameCount;
        gUniformSSSRConstantsData.g_clear_targets = gFrameCount < gDataBufferCount; // clear both temporal denoise buffers
        gUniformSSSRConstantsData.g_max_traversal_intersections = gSSSR_MaxTravelsalIntersections;
        gUniformSSSRConstantsData.g_min_traversal_occupancy = gSSSR_MinTravelsalOccupancy;
        gUniformSSSRConstantsData.g_most_detailed_mip = gSSSR_MostDetailedMip;
        gUniformSSSRConstantsData.g_temporal_stability_factor = pSSSR_TemporalStability;
        gUniformSSSRConstantsData.g_depth_buffer_thickness = gSSSR_DepthThickness;
        gUniformSSSRConstantsData.g_samples_per_quad = gSSSR_SamplesPerQuad;
        gUniformSSSRConstantsData.g_temporal_variance_guided_tracing_enabled = gSSSR_TemporalVarianceEnabled;
        gUniformSSSRConstantsData.g_roughness_threshold = gSSSR_RougnessThreshold;
        gUniformSSSRConstantsData.g_skip_denoiser = gSSSR_SkipDenoiser;

        viewMat.setTranslation(vec3(0));
        gUniformDataSky.mProjectView = projMat.mCamera * viewMat.mCamera;

        if (gReflectionType != gLastReflectionType)
        {
            if (gReflectionType == PP_REFLECTION)
            {
                uiShowDynamicWidgets(&PPR_Widgets, pGui);
                uiHideDynamicWidgets(&SSSR_Widgets, pGui);
            }
            else if (gReflectionType == SSS_REFLECTION)
            {
                uiShowDynamicWidgets(&SSSR_Widgets, pGui);
                uiHideDynamicWidgets(&PPR_Widgets, pGui);
            }
            gLastReflectionType = gReflectionType;
        }
    }

    void drawVisibilityBufferPass(Cmd* cmd)
    {
        RenderTargetBarrier barriers[] = {
            { pRenderTargetVBPass, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barriers), barriers);

        const char* profileNames[gNumGeomSets] = { "VB pass Opaque", "VB pass Alpha" };
        cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "VB pass");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetVBPass, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, profileNames[i]);
            cmdBindPipeline(cmd, pPipelineVBBufferPass[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectDrawBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[0];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectDrawBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
        }

        cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
        cmdBindRenderTargets(cmd, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel.This render step uses the render target generated by
    // DrawVisibilityBufferPass
    //  to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index
    //  buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
    {
        RenderTarget* pNormalRoughnessBuffer = pNormalRoughnessBuffers[gFrameIndex];

        RenderTargetBarrier rtBarriers[] = { { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                                             { pSceneBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                                             { pNormalRoughnessBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(rtBarriers), rtBarriers);

        cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "VB Shade Pass");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 2;
        bindRenderTargets.mRenderTargets[0] = { pSceneBuffer, LOAD_ACTION_CLEAR };
        bindRenderTargets.mRenderTargets[1] = { pNormalRoughnessBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mWidth, (float)pSceneBuffer->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pSceneBuffer->mWidth, pSceneBuffer->mHeight);

        cmdBindPipeline(cmd, pPipelineVBShadeSrgb);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);

        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

        {
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Draw Skybox");

            bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pSceneBuffer, LOAD_ACTION_LOAD };
            bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_LOAD };
            cmdBindRenderTargets(cmd, &bindRenderTargets);
            // Android Mali G-77 NaN precision workaround
            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSceneBuffer->mWidth, (float)pSceneBuffer->mHeight, 1.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, pSceneBuffer->mWidth, pSceneBuffer->mHeight);
            // Draw the skybox
            const uint32_t skyboxStride = sizeof(float) * 4;
            cmdBindPipeline(cmd, pSkyboxPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
            cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &skyboxStride, NULL);
            cmdDraw(cmd, 36, 0);

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        // This will acquire the next swapchain image
        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        gPerFrameData[gFrameIndex].mEyeObjectSpace[VIEW_CAMERA] =
            (gUniformDataExtenedCamera.mInvViewMat * vec4(0.f, 0.f, 0.f, 1.f)).getXYZ();
        gPerFrameData[gFrameIndex].mEyeObjectSpace[VIEW_SHADOW] = gPerFrameData[gFrameIndex].mEyeObjectSpace[VIEW_CAMERA];

        gVBConstants[gFrameIndex].transform[VIEW_SHADOW].mvp = gVBConstants[gFrameIndex].transform[VIEW_CAMERA].mvp;
        gVBConstants[gFrameIndex].cullingViewports[VIEW_SHADOW] = gVBConstants[gFrameIndex].cullingViewports[VIEW_CAMERA];

        BufferUpdateDesc meshUniformBufferUpdateDesc = { pBufferMeshTransforms[gFrameIndex] };
        beginUpdateResource(&meshUniformBufferUpdateDesc);
        memcpy(meshUniformBufferUpdateDesc.pMappedData, &gMeshInfoUniformData[gFrameIndex], sizeof(MeshInfoUniformBlock));
        endUpdateResource(&meshUniformBufferUpdateDesc);

        BufferUpdateDesc skyboxViewProjCbv = { pBufferUniformCameraSky[gFrameIndex] };
        beginUpdateResource(&skyboxViewProjCbv);
        memcpy(skyboxViewProjCbv.pMappedData, &gUniformDataSky, sizeof(gUniformDataSky));
        endUpdateResource(&skyboxViewProjCbv);

        BufferUpdateDesc updateVisibilityBufferConstantDesc = { pBufferVBConstants[gFrameIndex] };
        beginUpdateResource(&updateVisibilityBufferConstantDesc);
        memcpy(updateVisibilityBufferConstantDesc.pMappedData, &gVBConstants[gFrameIndex], sizeof(PerFrameVBConstantsData));
        endUpdateResource(&updateVisibilityBufferConstantDesc);

        BufferUpdateDesc CbvExtendedCamera = { pBufferUniformExtendedCamera[gFrameIndex] };
        beginUpdateResource(&CbvExtendedCamera);
        memcpy(CbvExtendedCamera.pMappedData, &gUniformDataExtenedCamera, sizeof(gUniformDataExtenedCamera));
        endUpdateResource(&CbvExtendedCamera);

        BufferUpdateDesc CbPPR_Prop = { pBufferUniformPPRPro[gFrameIndex] };
        beginUpdateResource(&CbPPR_Prop);
        memcpy(CbPPR_Prop.pMappedData, &gUniformPPRProData, sizeof(gUniformPPRProData));
        endUpdateResource(&CbPPR_Prop);

        BufferUpdateDesc planeInfoBuffUpdateDesc = { pBufferUniformPlaneInfo[gFrameIndex] };
        beginUpdateResource(&planeInfoBuffUpdateDesc);
        memcpy(planeInfoBuffUpdateDesc.pMappedData, &gUniformDataPlaneInfo, sizeof(gUniformDataPlaneInfo));
        endUpdateResource(&planeInfoBuffUpdateDesc);

        BufferUpdateDesc SSSR_ConstantsBuffUpdateDesc = { pSSSR_ConstantsBuffer[gFrameIndex] };
        beginUpdateResource(&SSSR_ConstantsBuffUpdateDesc);
        memcpy(SSSR_ConstantsBuffUpdateDesc.pMappedData, &gUniformSSSRConstantsData, sizeof(gUniformSSSRConstantsData));
        endUpdateResource(&SSSR_ConstantsBuffUpdateDesc);

        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        gCurrentGpuProfileToken = gReflectionType == PP_REFLECTION ? gPPRGpuProfileToken : gSSSRGpuProfileToken;

        cmdBeginGpuFrameProfile(cmd, gCurrentGpuProfileToken);

        TriangleFilteringPassDesc triangleFilteringDesc = {};
        triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
        triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;
        triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetPersistent;
        triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetPersistent;
        triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetPerFrame;
        triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorSetTriangleFilteringPerBatch;

        triangleFilteringDesc.mFrameIndex = gFrameIndex;
        triangleFilteringDesc.mBuffersIndex = 0; // We don't use Async Compute for triangle filtering, we just have 1 buffer
        triangleFilteringDesc.mGpuProfileToken = gCurrentGpuProfileToken;
        triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[gFrameIndex];
        cmdVBTriangleFilteringPass(pVisibilityBuffer, cmd, &triangleFilteringDesc);

        {
            const uint32_t numBarriers = NUM_CULLING_VIEWPORTS + 2;
            BufferBarrier  barriers2[numBarriers] = {};
            uint32_t       barrierCount = 0;
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[0], RESOURCE_STATE_UNORDERED_ACCESS,
                                          RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[gFrameIndex], RESOURCE_STATE_UNORDERED_ACCESS,
                                          RESOURCE_STATE_SHADER_RESOURCE };
            for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i], RESOURCE_STATE_UNORDERED_ACCESS,
                                              RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
            }
            cmdResourceBarrier(cmd, barrierCount, barriers2, 0, NULL, 0, NULL);
        }

        RenderTarget* pNormalRoughnessBuffer = pNormalRoughnessBuffers[gFrameIndex];

        drawVisibilityBufferPass(cmd);
        drawVisibilityBufferShade(cmd, gFrameIndex);

        RenderTargetBarrier rtBarriers[4] = {};
        rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);

        cmdBindRenderTargets(cmd, NULL);

        const uint32_t quadStride = sizeof(float) * 5;

        if (gReflectionType == PP_REFLECTION || !gSSSRSupported)
        {
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Pixel-Projected Reflections");
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "ProjectionPass");

            cmdBindPipeline(cmd, pPPR_ProjectionPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
            const uint32_t* pThreadGroupSize = pPPR_ProjectionShader->mNumThreadsPerGroup;
            cmdDispatch(cmd, (gSceneRes.mWidth * gSceneRes.mHeight / pThreadGroupSize[0]) + 1, 1, 1);

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            rtBarriers[0] = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            rtBarriers[1] = { pNormalRoughnessBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            rtBarriers[2] = { pReflectionBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, rtBarriers);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "ReflectionPass");

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pReflectionBuffer, LOAD_ACTION_CLEAR };
            cmdBindRenderTargets(cmd, &bindRenderTargets);
            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pReflectionBuffer->mWidth, (float)pReflectionBuffer->mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, pReflectionBuffer->mWidth, pReflectionBuffer->mHeight);

            cmdBindPipeline(cmd, pPPR_ReflectionPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
            cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
            cmdDraw(cmd, 3, 0);

            cmdBindRenderTargets(cmd, NULL);

            // End ReflectionPass
            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "HolePatching");
            rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
        }
        else if (gReflectionType == SSS_REFLECTION)
        {
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Stochastic Screen Space Reflections");

            uint32_t dim_x = (pDepthBuffer->mWidth + 7) / 8;
            uint32_t dim_y = (pDepthBuffer->mHeight + 7) / 8;

            TextureBarrier textureBarriers[4] = {};
            textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
            cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

            cmdBindPipeline(cmd, pCopyDepthPipeline);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Depth mips generation");

            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetCopyDepth);
            cmdDispatch(cmd, dim_x, dim_y, 1);

            if (gUseSPD)
            {
                textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

                cmdBindPipeline(cmd, pSPDPipeline);
                cmdBindDescriptorSet(cmd, 0, pDescriptorSetDepthDownSamplePerBatch);
                cmdDispatch(cmd, (pDepthBuffer->mWidth + 63) / 64, (pDepthBuffer->mHeight + 63) / 64, pSceneBuffer->mArraySize);
            }
            else
            {
                uint32_t mipSizeX = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mWidth));
                uint32_t mipSizeY = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mHeight));
                cmdBindPipeline(cmd, pGenerateMipPipeline);
                for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
                {
                    mipSizeX >>= 1;
                    mipSizeY >>= 1;
                    cmdBindDescriptorSet(cmd, i - 1, pDescriptorGenerateMip);
                    textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);

                    uint32_t groupCountX = mipSizeX / 16;
                    uint32_t groupCountY = mipSizeY / 16;
                    if (groupCountX == 0)
                        groupCountX = 1;
                    if (groupCountY == 0)
                        groupCountY = 1;
                    cmdDispatch(cmd, groupCountX, groupCountY, pSceneBuffer->mArraySize);
                }
            }

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Classify");

            BufferBarrier bufferBarriers[4] = {};
            bufferBarriers[0] = { pSSSR_RayCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[1] = { pSSSR_TileCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[2] = { pSSSR_RayListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[3] = { pSSSR_TileListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
            rtBarriers[1] = { pNormalRoughnessBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            textureBarriers[0] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            textureBarriers[1] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                   RESOURCE_STATE_UNORDERED_ACCESS };
            textureBarriers[2] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            cmdResourceBarrier(cmd, 4, bufferBarriers, 3, textureBarriers, 2, rtBarriers);

            cmdBindPipeline(cmd, pSSSR_ClassifyTilesPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
            cmdDispatch(cmd, dim_x, dim_y, pSceneBuffer->mArraySize);

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Prepare Indirect");

            bufferBarriers[0] = { pSSSR_RayCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[1] = { pSSSR_TileCounterBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[2] = { pSSSR_IntersectArgsBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS };
            bufferBarriers[3] = { pSSSR_DenoiserArgsBuffer, RESOURCE_STATE_INDIRECT_ARGUMENT, RESOURCE_STATE_UNORDERED_ACCESS };

            cmdResourceBarrier(cmd, 4, bufferBarriers, 0, NULL, 0, NULL);

            cmdBindPipeline(cmd, pSSSR_PrepareIndirectArgsPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);
            cmdDispatch(cmd, 1, 1, 1);

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Intersect");

            bufferBarriers[0] = { pSSSR_IntersectArgsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT };
            bufferBarriers[1] = { pSSSR_DenoiserArgsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT };
            bufferBarriers[2] = { pSSSR_RayListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            textureBarriers[0] = { pSSSR_DepthHierarchy, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
            textureBarriers[1] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            textureBarriers[2] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                   RESOURCE_STATE_UNORDERED_ACCESS };
            rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
            rtBarriers[1] = { pSceneBuffer, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            cmdResourceBarrier(cmd, 3, bufferBarriers, 3, textureBarriers, 2, rtBarriers);

            cmdBindPipeline(cmd, pSSSR_IntersectPipeline);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);
            cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

            cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_IntersectArgsBuffer, 0, NULL, 0);

            cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

            if (!gSSSR_SkipDenoiser)
            {
                cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Spatial Denoise");

                bufferBarriers[0] = { pSSSR_TileListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                textureBarriers[0] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                textureBarriers[1] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                       RESOURCE_STATE_UNORDERED_ACCESS };
                textureBarriers[2] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                cmdResourceBarrier(cmd, 1, bufferBarriers, 3, textureBarriers, 1, rtBarriers);

                cmdBindPipeline(cmd, pSSSR_ResolveSpatialPipeline);
                cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
                cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);
                cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

                cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

                cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

                if (gFrameCount > 0) // do not apply temporal denoise as long as we do not have the historical data from previous frame
                {
                    cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR Temporal Denoise");

                    textureBarriers[0] = { pSSSR_TemporalResults[0]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                           RESOURCE_STATE_UNORDERED_ACCESS };
                    textureBarriers[1] = { pSSSR_TemporalResults[1]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                           RESOURCE_STATE_UNORDERED_ACCESS };
                    textureBarriers[2] = { pSSSR_TemporalVariance, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    textureBarriers[3] = { pSSSR_RayLength->pTexture, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(cmd, 0, NULL, 4, textureBarriers, 1, rtBarriers);

                    cmdBindPipeline(cmd, pSSSR_ResolveTemporalPipeline);
                    cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
                    cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);
                    cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

                    cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

                    cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
                }

                cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 1");

                textureBarriers[0] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                       RESOURCE_STATE_UNORDERED_ACCESS };
                rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

                cmdBindPipeline(cmd, pSSSR_ResolveEAWPipeline);
                cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
                cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSSSR);

                cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

                cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

                if (gSSSR_EAWPassCount == 3)
                {
                    cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 2");

                    textureBarriers[0] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                           RESOURCE_STATE_UNORDERED_ACCESS };
                    rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

                    cmdBindPipeline(cmd, pSSSR_ResolveEAWStride2Pipeline);

                    cmdBindDescriptorSet(cmd, 0, pDescriptorSetSSSR);
                    cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

                    cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

                    cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

                    cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "SSSR EAW Denoise Pass 3");

                    textureBarriers[0] = { pSSSR_TemporalResults[gFrameIndex]->pTexture, RESOURCE_STATE_UNORDERED_ACCESS,
                                           RESOURCE_STATE_UNORDERED_ACCESS };
                    rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 1, rtBarriers);

                    cmdBindPipeline(cmd, pSSSR_ResolveEAWStride4Pipeline);
                    cmdBindDescriptorSet(cmd, 0, pDescriptorSetSSSR);
                    cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);

                    cmdExecuteIndirect(cmd, INDIRECT_DISPATCH, 1, pSSSR_DenoiserArgsBuffer, 0, NULL, 0);

                    cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
                }
            }
            cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Apply Reflections");
            rtBarriers[0] = { pReflectionBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
        }

        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        rtBarriers[1] = { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPPR_HolePatchingPipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPPR);
        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetPerFrame);
        cmdBindVertexBuffer(cmd, 1, &pScreenQuadVertexBuffer, &quadStride, NULL);
        cmdDraw(cmd, 3, 0);

        cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);
        // End Reflections
        cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

        cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.f, txtSize.y + 75.f), gCurrentGpuProfileToken, &gFrameTimeDraw);

        cmdDrawUserInterface(cmd);
        cmdEndDebugMarker(cmd);

        cmdBindRenderTargets(cmd, NULL);

        {
            const uint32_t      numBarriers = NUM_CULLING_VIEWPORTS + 2;
            RenderTargetBarrier barrierPresent = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
            BufferBarrier       barriers2[numBarriers] = {};
            uint32_t            barrierCount = 0;

            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[0],
                                          RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE,
                                          RESOURCE_STATE_UNORDERED_ACCESS };
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[gFrameIndex], RESOURCE_STATE_SHADER_RESOURCE,
                                          RESOURCE_STATE_UNORDERED_ACCESS };
            for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i],
                                              RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE,
                                              RESOURCE_STATE_UNORDERED_ACCESS };
            }

            cmdResourceBarrier(cmd, numBarriers, barriers2, 0, NULL, 1, &barrierPresent);
        }

        cmdEndGpuFrameProfile(cmd, gCurrentGpuProfileToken);
        endCmd(cmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
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
        ++gFrameCount;
    }

    const char* GetName() { return "10_ScreenSpaceReflections"; }

    void prepareDescriptorSets()
    {
        DescriptorData persistentSetParams[32] = {};

        persistentSetParams[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBPassTexture);
        persistentSetParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
        persistentSetParams[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gDiffuseMaps);
        persistentSetParams[1].mCount = gMaterialCount;
        persistentSetParams[1].ppTextures = gDiffuseMapsStorage;
        persistentSetParams[2].mIndex = SRT_RES_IDX(SrtData, Persistent, gNormalMaps);
        persistentSetParams[2].mCount = gMaterialCount;
        persistentSetParams[2].ppTextures = gNormalMapsStorage;
        persistentSetParams[3].mIndex = SRT_RES_IDX(SrtData, Persistent, gSpecularMaps);
        persistentSetParams[3].mCount = gMaterialCount;
        persistentSetParams[3].ppTextures = gSpecularMapsStorage;
        persistentSetParams[4].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexPositionBuffer);
        persistentSetParams[4].ppBuffers = &pSanMiguelModel->pVertexBuffers[0];
        persistentSetParams[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexTexCoordBuffer);
        persistentSetParams[5].ppBuffers = &pSanMiguelModel->pVertexBuffers[1];
        persistentSetParams[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexNormalBuffer);
        persistentSetParams[6].ppBuffers = &pSanMiguelModel->pVertexBuffers[2];
        persistentSetParams[7].mIndex = SRT_RES_IDX(SrtData, Persistent, gMeshConstantsBuffer);
        persistentSetParams[7].ppBuffers = &pBufferMeshConstants;
        persistentSetParams[8].mIndex = SRT_RES_IDX(SrtData, Persistent, gCBLights);
        persistentSetParams[8].ppBuffers = &pBufferUniformLights;
        persistentSetParams[9].mIndex = SRT_RES_IDX(SrtData, Persistent, gCBDLights);
        persistentSetParams[9].ppBuffers = &pBufferUniformDirectionalLights;
        persistentSetParams[10].mIndex = SRT_RES_IDX(SrtData, Persistent, gBRDFIntegrationMap);
        persistentSetParams[10].ppTextures = &pBRDFIntegrationMap;
        persistentSetParams[11].mIndex = SRT_RES_IDX(SrtData, Persistent, gIrradianceMap);
        persistentSetParams[11].ppTextures = &pIrradianceMap;
        persistentSetParams[12].mIndex = SRT_RES_IDX(SrtData, Persistent, gSpecularMap);
        persistentSetParams[12].ppTextures = &pSpecularMap;
        persistentSetParams[13].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBConstantBuffer);
        persistentSetParams[13].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        persistentSetParams[14].mIndex = SRT_RES_IDX(SrtData, Persistent, gDefaultSampler);
        persistentSetParams[14].ppSamplers = &pDefaultSampler;
        persistentSetParams[15].mIndex = SRT_RES_IDX(SrtData, Persistent, gEnvSampler);
        persistentSetParams[15].ppSamplers = &pSamplerBilinear;
        persistentSetParams[16].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthSampler);
        persistentSetParams[16].ppSamplers = &pSamplerBilinear;
        persistentSetParams[17].mIndex = SRT_RES_IDX(SrtData, Persistent, gSkyboxSampler);
        persistentSetParams[17].ppSamplers = &pSamplerBilinear;
        persistentSetParams[18].mIndex = SRT_RES_IDX(SrtData, Persistent, gBilinearSampler);
        persistentSetParams[18].ppSamplers = &pSamplerBilinear;
        persistentSetParams[19].mIndex = SRT_RES_IDX(SrtData, Persistent, gSkyboxTex);
        persistentSetParams[19].ppTextures = &pSkybox;
        persistentSetParams[20].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexDataBuffer);
        persistentSetParams[20].ppBuffers = &pSanMiguelModel->pIndexBuffer;
        persistentSetParams[21].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthTexture);
        persistentSetParams[21].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[22].mIndex = SRT_RES_IDX(SrtData, Persistent, gSceneTexture);
        persistentSetParams[22].ppTextures = &pSceneBuffer->pTexture;
        persistentSetParams[23].mIndex = SRT_RES_IDX(SrtData, Persistent, gSSRTexture);
        persistentSetParams[23].ppTextures = &pReflectionBuffer->pTexture;
        persistentSetParams[24].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthBuffer);
        persistentSetParams[24].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[25].mIndex = SRT_RES_IDX(SrtData, Persistent, gSourceDepth);
        persistentSetParams[25].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[25].mUAVMipSlice = 0;
        persistentSetParams[26].mIndex = SRT_RES_IDX(SrtData, Persistent, gLitScene);
        persistentSetParams[26].ppTextures = &pSceneBuffer->pTexture;
        persistentSetParams[27].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthBufferHierarchy);
        persistentSetParams[27].ppTextures = &pSSSR_DepthHierarchy;
        persistentSetParams[28].mIndex = SRT_RES_IDX(SrtData, Persistent, gSobolBuffer);
        persistentSetParams[28].ppBuffers = &pSSSR_SobolBuffer;
        persistentSetParams[29].mIndex = SRT_RES_IDX(SrtData, Persistent, gRankingTileBuffer);
        persistentSetParams[29].ppBuffers = &pSSSR_RankingTileBuffer;
        persistentSetParams[30].mIndex = SRT_RES_IDX(SrtData, Persistent, gScramblingTileBuffer);
        persistentSetParams[30].ppBuffers = &pSSSR_ScramblingTileBuffer;

        updateDescriptorSet(pRenderer, 0, pDescriptorSetPersistent, 31, persistentSetParams);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            uint8_t        prevIndex = (i + gDataBufferCount - 1) % gDataBufferCount;
            DescriptorData SSSRParams[14] = {};
            SSSRParams[0].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gTileList);
            SSSRParams[0].ppBuffers = &pSSSR_TileListBuffer;
            SSSRParams[1].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gRayList);
            SSSRParams[1].ppBuffers = &pSSSR_RayListBuffer;
            SSSRParams[2].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gRayCounter);
            SSSRParams[2].ppBuffers = &pSSSR_RayCounterBuffer;
            SSSRParams[3].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gTileCounter);
            SSSRParams[3].ppBuffers = &pSSSR_TileCounterBuffer;
            SSSRParams[4].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gIntersectArgs);
            SSSRParams[4].ppBuffers = &pSSSR_IntersectArgsBuffer;
            SSSRParams[5].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gDenoiserArgs);
            SSSRParams[5].ppBuffers = &pSSSR_DenoiserArgsBuffer;
            SSSRParams[6].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gRayLengths);
            SSSRParams[6].ppTextures = &pSSSR_RayLength->pTexture;
            SSSRParams[7].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gHasRay);
            SSSRParams[7].ppTextures = &pSSSR_TemporalVariance;
            SSSRParams[8].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gTemporalVariance);
            SSSRParams[8].ppTextures = &pSSSR_TemporalVariance;
            SSSRParams[9].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gDenoisedReflections);
            SSSRParams[9].ppTextures = &pReflectionBuffer->pTexture;
            SSSRParams[10].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gTemporallyDenoisedReflections);
            SSSRParams[10].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
            SSSRParams[11].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gIntersectionResult);
            SSSRParams[11].ppTextures = &pSSSR_TemporalResults[i]->pTexture;
            SSSRParams[12].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gSpatiallyDenoisedReflections);
            SSSRParams[12].ppTextures = &pReflectionBuffer->pTexture;
            SSSRParams[13].mIndex = SRT_RES_IDX(SSSRSrtData, PerBatch, gTemporallyDenoisedReflectionsHistory);
            SSSRParams[13].ppTextures = &pSSSR_TemporalResults[prevIndex]->pTexture;
            updateDescriptorSet(pRenderer, i, pDescriptorSetSSSR, 14, SSSRParams);
        }

        DescriptorData PPRParams[2] = {};
        PPRParams[0].mIndex = SRT_RES_IDX(PPRSrtData, PerBatch, gIntermediateBuffer);
        PPRParams[0].ppBuffers = &pIntermediateBuffer;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPPR, 1, PPRParams);

        DescriptorData copyDepthParams[2] = {};
        copyDepthParams[0].mIndex = SRT_RES_IDX(CopyDepthSrtData, PerBatch, gDestinationDepth);
        copyDepthParams[0].ppTextures = &pSSSR_DepthHierarchy;
        copyDepthParams[0].mUAVMipSlice = 0;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetCopyDepth, 1, copyDepthParams);

        DescriptorData depthDownsampleParams[2] = {};
        depthDownsampleParams[0].mIndex = SRT_RES_IDX(DepthDownSampleSrtData, PerBatch, gGlobalAtomic);
        depthDownsampleParams[0].ppBuffers = &pSPD_AtomicCounterBuffer;
        depthDownsampleParams[1].mIndex = SRT_RES_IDX(DepthDownSampleSrtData, PerBatch, gDownsampledDepthBuffer);
        depthDownsampleParams[1].ppTextures = &pSSSR_DepthHierarchy;
        depthDownsampleParams[1].mBindMipChain = true;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetDepthDownSamplePerBatch, 2, depthDownsampleParams);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData indirectDrawClearParams[4] = {};
            indirectDrawClearParams[0].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDrawClearArgsRW);
            indirectDrawClearParams[0].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[0];
            indirectDrawClearParams[1].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDataBufferRW);
            indirectDrawClearParams[1].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            indirectDrawClearParams[2].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gFilteredIndicesBufferRW);
            indirectDrawClearParams[2].mCount = NUM_CULLING_VIEWPORTS;
            indirectDrawClearParams[2].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[0];
            updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFilteringPerBatch, 3, indirectDrawClearParams);
        }

        DescriptorData perFrameSetParams[12] = {};
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            uint8_t prevIndex = (i + gDataBufferCount - 1) % gDataBufferCount;
            perFrameSetParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gUniformBlockPerFrame);
            perFrameSetParams[0].ppBuffers = &pBufferUniformCameraSky[i];
            perFrameSetParams[1].mIndex = SRT_RES_IDX(SrtData, PerFrame, gVBConstantsPerFrame);
            perFrameSetParams[1].ppBuffers = &pBufferVBConstants[i];
            perFrameSetParams[2].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilterDispatchGroupDataBuffer);
            perFrameSetParams[2].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
            perFrameSetParams[3].mIndex = SRT_RES_IDX(SrtData, PerFrame, gObjectUniformBlockPerFrame);
            perFrameSetParams[3].ppBuffers = &pBufferMeshTransforms[i];
            perFrameSetParams[4].mIndex = SRT_RES_IDX(SrtData, PerFrame, gIndirectDataBuffer);
            perFrameSetParams[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            perFrameSetParams[5].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilteredIndexBuffer);
            perFrameSetParams[5].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
            perFrameSetParams[6].mIndex = SRT_RES_IDX(SrtData, PerFrame, gCBExtendCamera);
            perFrameSetParams[6].ppBuffers = &pBufferUniformExtendedCamera[i];
            perFrameSetParams[7].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPlaneInfoBuffer);
            perFrameSetParams[7].ppBuffers = &pBufferUniformPlaneInfo[i];
            perFrameSetParams[8].mIndex = SRT_RES_IDX(SrtData, PerFrame, gCBProperties);
            perFrameSetParams[8].ppBuffers = &pBufferUniformPPRPro[i];
            perFrameSetParams[9].mIndex = SRT_RES_IDX(SrtData, PerFrame, gNormalRoughness);
            perFrameSetParams[9].ppTextures = &pNormalRoughnessBuffers[i]->pTexture;
            perFrameSetParams[10].mIndex = SRT_RES_IDX(SrtData, PerFrame, gConstants);
            perFrameSetParams[10].ppBuffers = &pSSSR_ConstantsBuffer[i];
            perFrameSetParams[11].mIndex = SRT_RES_IDX(SrtData, PerFrame, gNormalRoughnessHistory);
            perFrameSetParams[11].ppTextures = &pNormalRoughnessBuffers[prevIndex]->pTexture;
            updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, 12, perFrameSetParams);
        }
        if (gSSSRSupported)
        {
            for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
            {
                perFrameSetParams[0].mIndex = SRT_RES_IDX(GenerateMipsSrtData, PerDraw, gSourceTexture);
                perFrameSetParams[0].ppTextures = &pSSSR_DepthHierarchy;
                perFrameSetParams[0].mUAVMipSlice = (uint16_t)(i - 1);
                perFrameSetParams[1].mIndex = SRT_RES_IDX(GenerateMipsSrtData, PerDraw, gDestinationTexture);
                perFrameSetParams[1].ppTextures = &pSSSR_DepthHierarchy;
                perFrameSetParams[1].mUAVMipSlice = (uint16_t)i;
                perFrameSetParams[2].mIndex = SRT_RES_IDX(GenerateMipsSrtData, PerDraw, gGenMipsConstants);
                perFrameSetParams[2].ppBuffers = &pSSSR_GenMipsBuffers[i];
                updateDescriptorSet(pRenderer, i - 1, pDescriptorGenerateMip, 3, perFrameSetParams);
            }
        }
    }

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
        // none set
        DescriptorSetDesc setDesc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPersistent);

        // per frame set
        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);

        if (gSSSRSupported)
        {
            // DepthDownsample
            setDesc = SRT_SET_DESC(GenerateMipsSrtData, PerDraw, 13, 0);
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorGenerateMip);
        }

        // Depth Downsample
        setDesc = SRT_SET_DESC_LARGE_RW(DepthDownSampleSrtData, PerBatch, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDepthDownSamplePerBatch);

        // SSSR
        setDesc = SRT_SET_DESC(SSSRSrtData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSSSR);

        // PPR
        setDesc = SRT_SET_DESC(PPRSrtData, PerBatch, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPPR);

        // copy depth
        setDesc = SRT_SET_DESC(CopyDepthSrtData, PerBatch, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCopyDepth);

        // indirect draw clear
        setDesc = SRT_SET_DESC(TriangleFilteringSrtData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFilteringPerBatch);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFilteringPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetCopyDepth);
        removeDescriptorSet(pRenderer, pDescriptorSetPPR);
        removeDescriptorSet(pRenderer, pDescriptorSetSSSR);
        removeDescriptorSet(pRenderer, pDescriptorSetDepthDownSamplePerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPersistent);

        if (gSSSRSupported)
        {
            removeDescriptorSet(pRenderer, pDescriptorGenerateMip);
        }
    }

    void addShaders()
    {
        // Load shaders for Vis Buffer
        ShaderLoadDesc clearBuffersShaderDesc = {};
        clearBuffersShaderDesc.mComp.pFileName = "clearVisibilityBuffers.comp";
        addShader(pRenderer, &clearBuffersShaderDesc, &pShaderClearBuffers);

        ShaderLoadDesc triangleFilteringShaderDesc = {};
        triangleFilteringShaderDesc.mComp.pFileName = "triangleFiltering.comp";
        addShader(pRenderer, &triangleFilteringShaderDesc, &pShaderTriangleFiltering);

        ShaderLoadDesc shaderVBrepass = {};
        shaderVBrepass.mVert.pFileName = "visibilityBufferPass.vert";
        shaderVBrepass.mFrag.pFileName = "visibilityBufferPass.frag";

        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gSettings.mAddGeometryPassThrough;
        if (addGeometryPassThrough)
        {
            // a passthrough gs
            shaderVBrepass.mGeom.pFileName = "visibilityBufferPass.geom";
        }

        addShader(pRenderer, &shaderVBrepass, &pShaderVBBufferPass[GEOMSET_OPAQUE]);

        ShaderLoadDesc visibilityBufferPassAlphaShaderDesc = {};
        visibilityBufferPassAlphaShaderDesc.mVert.pFileName = "visibilityBufferPassAlpha.vert";
        visibilityBufferPassAlphaShaderDesc.mFrag.pFileName = "visibilityBufferPassAlpha.frag";

        if (addGeometryPassThrough)
        {
            // a passthrough gs
            visibilityBufferPassAlphaShaderDesc.mGeom.pFileName = "visibilityBufferPassAlpha.geom";
        }

        addShader(pRenderer, &visibilityBufferPassAlphaShaderDesc, &pShaderVBBufferPass[GEOMSET_ALPHA_CUTOUT]);

        ShaderLoadDesc visibilityBufferShadeShaderDesc = {};
        visibilityBufferShadeShaderDesc.mVert.pFileName = "visibilityBufferShade.vert";
        visibilityBufferShadeShaderDesc.mFrag.pFileName = "visibilityBufferShade.frag";
        addShader(pRenderer, &visibilityBufferShadeShaderDesc, &pShaderVBShade);

        ShaderLoadDesc skyboxShaderDesc = {};
        skyboxShaderDesc.mVert.pFileName = "skybox.vert";
        skyboxShaderDesc.mFrag.pFileName = "skybox.frag";
        addShader(pRenderer, &skyboxShaderDesc, &pSkyboxShader);

        // PPR_Projection
        ShaderLoadDesc PPR_ProjectionShaderDesc = {};
        PPR_ProjectionShaderDesc.mComp.pFileName = "PPR_Projection.comp";
        addShader(pRenderer, &PPR_ProjectionShaderDesc, &pPPR_ProjectionShader);

        // PPR_Reflection
        ShaderLoadDesc PPR_ReflectionShaderDesc = {};
        PPR_ReflectionShaderDesc.mVert.pFileName = "PPR_Reflection.vert";
        PPR_ReflectionShaderDesc.mFrag.pFileName = "PPR_Reflection.frag";
        addShader(pRenderer, &PPR_ReflectionShaderDesc, &pPPR_ReflectionShader);

        // PPR_HolePatching
        ShaderLoadDesc PPR_HolePatchingShaderDesc = {};
        PPR_HolePatchingShaderDesc.mVert.pFileName = "PPR_Holepatching.vert";
        PPR_HolePatchingShaderDesc.mFrag.pFileName = "PPR_Holepatching.frag";
        addShader(pRenderer, &PPR_HolePatchingShaderDesc, &pPPR_HolePatchingShader);

        if (gSSSRSupported)
        {
            ShaderLoadDesc SPDDesc = {};
            SPDDesc.mComp.pFileName = "DepthDownsample.comp";
            addShader(pRenderer, &SPDDesc, &pSPDShader);

            ShaderLoadDesc CopyDepthShaderDesc = {};
            CopyDepthShaderDesc.mComp.pFileName = "copyDepth.comp";
            addShader(pRenderer, &CopyDepthShaderDesc, &pCopyDepthShader);

            ShaderLoadDesc GenerateMipShaderDesc = {};
            GenerateMipShaderDesc.mComp.pFileName = "generateMips.comp";
            addShader(pRenderer, &GenerateMipShaderDesc, &pGenerateMipShader);

            // SSSR
            ShaderLoadDesc SSSR_ClassifyTilesShaderDesc = {};
            SSSR_ClassifyTilesShaderDesc.mComp.pFileName = "SSSR_ClassifyTiles.comp";
            addShader(pRenderer, &SSSR_ClassifyTilesShaderDesc, &pSSSR_ClassifyTilesShader);

            ShaderLoadDesc SSSR_PrepareIndirectArgsShaderDesc = {};
            SSSR_PrepareIndirectArgsShaderDesc.mComp.pFileName = "SSSR_PrepareIndirectArgs.comp";
            addShader(pRenderer, &SSSR_PrepareIndirectArgsShaderDesc, &pSSSR_PrepareIndirectArgsShader);

            ShaderLoadDesc SSSR_IntersectShaderDesc = {};
            SSSR_IntersectShaderDesc.mComp.pFileName = "SSSR_Intersect.comp";
            addShader(pRenderer, &SSSR_IntersectShaderDesc, &pSSSR_IntersectShader);

            ShaderLoadDesc SSSR_ResolveSpatialShaderDesc = {};
            SSSR_ResolveSpatialShaderDesc.mComp.pFileName = "SSSR_ResolveSpatial.comp";
            addShader(pRenderer, &SSSR_ResolveSpatialShaderDesc, &pSSSR_ResolveSpatialShader);

            ShaderLoadDesc SSSR_ResolveTemporalShaderDesc = {};
            SSSR_ResolveTemporalShaderDesc.mComp.pFileName = "SSSR_ResolveTemporal.comp";
            addShader(pRenderer, &SSSR_ResolveTemporalShaderDesc, &pSSSR_ResolveTemporalShader);

            ShaderLoadDesc SSSR_ResolveEAWShaderDesc = {};
            SSSR_ResolveEAWShaderDesc.mComp.pFileName = "SSSR_ResolveEaw.comp";
            addShader(pRenderer, &SSSR_ResolveEAWShaderDesc, &pSSSR_ResolveEAWShader);

            ShaderLoadDesc SSSR_ResolveEAWStride2ShaderDesc = {};
            SSSR_ResolveEAWStride2ShaderDesc.mComp.pFileName = "SSSR_ResolveEawStride_2.comp";
            addShader(pRenderer, &SSSR_ResolveEAWStride2ShaderDesc, &pSSSR_ResolveEAWStride2Shader);

            ShaderLoadDesc SSSR_ResolveEAWStride4ShaderDesc = {};
            SSSR_ResolveEAWStride4ShaderDesc.mComp.pFileName = "SSSR_ResolveEawStride_4.comp";
            addShader(pRenderer, &SSSR_ResolveEAWStride4ShaderDesc, &pSSSR_ResolveEAWStride4Shader);
        }
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderVBBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVBBufferPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVBShade);

        if (gSSSRSupported)
        {
            removeShader(pRenderer, pSSSR_ResolveEAWStride4Shader);
            removeShader(pRenderer, pSSSR_ResolveEAWStride2Shader);
            removeShader(pRenderer, pSSSR_ResolveEAWShader);
            removeShader(pRenderer, pSSSR_ResolveTemporalShader);
            removeShader(pRenderer, pSSSR_ResolveSpatialShader);
            removeShader(pRenderer, pSSSR_IntersectShader);
            removeShader(pRenderer, pSSSR_PrepareIndirectArgsShader);
            removeShader(pRenderer, pSSSR_ClassifyTilesShader);
            removeShader(pRenderer, pSPDShader);
            removeShader(pRenderer, pGenerateMipShader);
            removeShader(pRenderer, pCopyDepthShader);
        }

        removeShader(pRenderer, pPPR_HolePatchingShader);
        removeShader(pRenderer, pPPR_ReflectionShader);
        removeShader(pRenderer, pPPR_ProjectionShader);
        removeShader(pRenderer, pSkyboxShader);
    }

    void addPipelines()
    {
        DepthStateDesc depthStateDisabledDesc = {};
        DepthStateDesc depthStateTestAndWriteDesc = {};
        depthStateTestAndWriteDesc.mDepthTest = true;
        depthStateTestAndWriteDesc.mDepthWrite = true;
        depthStateTestAndWriteDesc.mDepthFunc = CMP_GEQUAL;

        // Create rasteriser state objects
        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        {
            /************************************************************************/
            // Setup the resources needed for the Visibility Buffer Pipeline
            /******************************/

            TinyImageFormat formats[2] = {};
            formats[0] = pRenderTargetVBPass->mFormat;

            PipelineDesc desc = {};
            PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& vbPassPipelineSettings = desc.mGraphicsDesc;
            vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            vbPassPipelineSettings.mRenderTargetCount = 1;
            vbPassPipelineSettings.pDepthState = &depthStateTestAndWriteDesc;
            vbPassPipelineSettings.pColorFormats = formats;
            vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mSampleCount;
            vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
            vbPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
            vbPassPipelineSettings.pVertexLayout = NULL;
            vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;

            for (uint32_t i = 0; i < gNumGeomSets; ++i)
            {
                vbPassPipelineSettings.pShaderProgram = pShaderVBBufferPass[i];

#if defined(GFX_EXTENDED_PSO_OPTIONS)
                ExtendedGraphicsPipelineDesc edescs[2] = {};
                edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
                initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
                edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

                edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_PIXEL_SHADER_OPTIONS;
                edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
                edescs[1].pixelShaderOptions.depthBeforeShader =
                    !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

                desc.mExtensionCount = 2;
                desc.pPipelineExtensions = edescs;
#endif
                addPipeline(pRenderer, &desc, &pPipelineVBBufferPass[i]);

                desc.mExtensionCount = 0;
            }

            formats[0] = pSceneBuffer->mFormat;
            formats[1] = pNormalRoughnessBuffers[0]->mFormat;

            desc.mGraphicsDesc = {};
            GraphicsPipelineDesc& vbShadePipelineSettings = desc.mGraphicsDesc;
            vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            vbShadePipelineSettings.mRenderTargetCount = 2;
            vbShadePipelineSettings.pDepthState = &depthStateDisabledDesc;
            vbShadePipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
            vbShadePipelineSettings.pShaderProgram = pShaderVBShade;
            vbShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            vbShadePipelineSettings.pColorFormats = formats;
            vbShadePipelineSettings.mSampleQuality = pSceneBuffer->mSampleQuality;
#if defined(GFX_EXTENDED_PSO_OPTIONS)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            // edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_PIXEL_SHADER_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;

            desc.mExtensionCount = 2;
            desc.pPipelineExtensions = edescs;
#endif
            PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
            addPipeline(pRenderer, &desc, &pPipelineVBShadeSrgb);

            PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(TriangleFilteringSrtData, Persistent),
                                 SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerFrame), SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerBatch),
                                 NULL);
            desc.mExtensionCount = 0;
            desc.mType = PIPELINE_TYPE_COMPUTE;
            desc.mComputeDesc = {};
            ComputePipelineDesc& clearBufferPipelineSettings = desc.mComputeDesc;
            clearBufferPipelineSettings.pShaderProgram = pShaderClearBuffers;
            addPipeline(pRenderer, &desc, &pPipelineClearBuffers);

            PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(TriangleFilteringSrtData, Persistent),
                                 SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerFrame), SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerBatch),
                                 NULL);
            desc.mComputeDesc = {};
            ComputePipelineDesc& triangleFilteringPipelineSettings = desc.mComputeDesc;
            triangleFilteringPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
            addPipeline(pRenderer, &desc, &pPipelineTriangleFiltering);
        }

        // layout and pipeline for skybox draw
        BlendStateDesc blendStateSkyBoxDesc = {};
        blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
        blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
        blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;
        blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
        blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStateSkyBoxDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;

        VertexLayout vertexLayoutSkybox = {};
        vertexLayoutSkybox.mBindingCount = 1;
        vertexLayoutSkybox.mAttribCount = 1;
        vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayoutSkybox.mAttribs[0].mBinding = 0;
        vertexLayoutSkybox.mAttribs[0].mLocation = 0;
        vertexLayoutSkybox.mAttribs[0].mOffset = 0;

        PipelineDesc desc = {};
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& skyboxPipelineDesc = desc.mGraphicsDesc;
        skyboxPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        skyboxPipelineDesc.pDepthState = NULL;
        skyboxPipelineDesc.pBlendState = &blendStateSkyBoxDesc;
        skyboxPipelineDesc.mRenderTargetCount = 1;
        skyboxPipelineDesc.pColorFormats = &pSceneBuffer->mFormat;
        skyboxPipelineDesc.mSampleCount = pSceneBuffer->mSampleCount;
        skyboxPipelineDesc.mSampleQuality = pSceneBuffer->mSampleQuality;
        skyboxPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
        skyboxPipelineDesc.pShaderProgram = pSkyboxShader;
        skyboxPipelineDesc.pVertexLayout = &vertexLayoutSkybox;
        skyboxPipelineDesc.pRasterizerState = &rasterizerStateCullNoneDesc;
        addPipeline(pRenderer, &desc, &pSkyboxPipeline);

        // Position
        VertexLayout vertexLayoutScreenQuad = {};
        vertexLayoutScreenQuad.mBindingCount = 1;
        vertexLayoutScreenQuad.mAttribCount = 2;

        vertexLayoutScreenQuad.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutScreenQuad.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayoutScreenQuad.mAttribs[0].mBinding = 0;
        vertexLayoutScreenQuad.mAttribs[0].mLocation = 0;
        vertexLayoutScreenQuad.mAttribs[0].mOffset = 0;

        // Uv
        vertexLayoutScreenQuad.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayoutScreenQuad.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayoutScreenQuad.mAttribs[1].mLocation = 1;
        vertexLayoutScreenQuad.mAttribs[1].mBinding = 0;
        vertexLayoutScreenQuad.mAttribs[1].mOffset = 3 * sizeof(float); // first attribute contains 3 floats

        // PPR_Reflection
        PIPELINE_LAYOUT_DESC(desc, SRT_LAYOUT_DESC(PPRSrtData, Persistent), SRT_LAYOUT_DESC(PPRSrtData, PerFrame),
                             SRT_LAYOUT_DESC(PPRSrtData, PerBatch), NULL);
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = NULL;

        pipelineSettings.pColorFormats = &pReflectionBuffer->mFormat;
        pipelineSettings.mSampleCount = pReflectionBuffer->mSampleCount;
        pipelineSettings.mSampleQuality = pReflectionBuffer->mSampleQuality;

        pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        pipelineSettings.pShaderProgram = pPPR_ReflectionShader;
        pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
        pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        addPipeline(pRenderer, &desc, &pPPR_ReflectionPipeline);

        // PPR_HolePatching -> Present
        pipelineSettings = { 0 };
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = NULL;

        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

        pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        pipelineSettings.pShaderProgram = pPPR_HolePatchingShader;
        pipelineSettings.pVertexLayout = &vertexLayoutScreenQuad;
        pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        addPipeline(pRenderer, &desc, &pPPR_HolePatchingPipeline);

        // PPR_Projection
        PipelineDesc computeDesc = {};
        PIPELINE_LAYOUT_DESC(computeDesc, SRT_LAYOUT_DESC(PPRSrtData, Persistent), SRT_LAYOUT_DESC(PPRSrtData, PerFrame),
                             SRT_LAYOUT_DESC(PPRSrtData, PerBatch), NULL);
        computeDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& cpipelineSettings = computeDesc.mComputeDesc;
        cpipelineSettings.pShaderProgram = pPPR_ProjectionShader;
        addPipeline(pRenderer, &computeDesc, &pPPR_ProjectionPipeline);

        if (gSSSRSupported)
        {
            PIPELINE_LAYOUT_DESC(computeDesc, SRT_LAYOUT_DESC(DepthDownSampleSrtData, Persistent),
                                 SRT_LAYOUT_DESC(DepthDownSampleSrtData, PerFrame), SRT_LAYOUT_DESC(DepthDownSampleSrtData, PerBatch),
                                 NULL);
            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSPDShader;
            addPipeline(pRenderer, &computeDesc, &pSPDPipeline);

            PIPELINE_LAYOUT_DESC(computeDesc, SRT_LAYOUT_DESC(CopyDepthSrtData, Persistent), SRT_LAYOUT_DESC(CopyDepthSrtData, PerFrame),
                                 SRT_LAYOUT_DESC(CopyDepthSrtData, PerBatch), NULL);
            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pCopyDepthShader;
            addPipeline(pRenderer, &computeDesc, &pCopyDepthPipeline);

            PIPELINE_LAYOUT_DESC(computeDesc, SRT_LAYOUT_DESC(GenerateMipsSrtData, Persistent),
                                 SRT_LAYOUT_DESC(GenerateMipsSrtData, PerFrame), NULL, SRT_LAYOUT_DESC(GenerateMipsSrtData, PerDraw));
            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pGenerateMipShader;
            addPipeline(pRenderer, &computeDesc, &pGenerateMipPipeline);

            // SSSR
            PIPELINE_LAYOUT_DESC(computeDesc, SRT_LAYOUT_DESC(SSSRSrtData, Persistent), SRT_LAYOUT_DESC(SSSRSrtData, PerFrame),
                                 SRT_LAYOUT_DESC(SSSRSrtData, PerBatch), NULL);
            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ClassifyTilesShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ClassifyTilesPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_PrepareIndirectArgsShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_PrepareIndirectArgsPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_IntersectShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_IntersectPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ResolveSpatialShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveSpatialPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ResolveTemporalShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveTemporalPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWShader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWPipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWStride2Shader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWStride2Pipeline);

            cpipelineSettings = { 0 };
            cpipelineSettings.pShaderProgram = pSSSR_ResolveEAWStride4Shader;
            addPipeline(pRenderer, &computeDesc, &pSSSR_ResolveEAWStride4Pipeline);
        }
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineClearBuffers);
        removePipeline(pRenderer, pPipelineTriangleFiltering);

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            removePipeline(pRenderer, pPipelineVBBufferPass[i]);
        }
        removePipeline(pRenderer, pPipelineVBShadeSrgb);

        removePipeline(pRenderer, pSkyboxPipeline);
        removePipeline(pRenderer, pPPR_ProjectionPipeline);
        removePipeline(pRenderer, pPPR_ReflectionPipeline);
        removePipeline(pRenderer, pPPR_HolePatchingPipeline);
        if (gSSSRSupported)
        {
            removePipeline(pRenderer, pSPDPipeline);
            removePipeline(pRenderer, pCopyDepthPipeline);
            removePipeline(pRenderer, pGenerateMipPipeline);
            removePipeline(pRenderer, pSSSR_ClassifyTilesPipeline);
            removePipeline(pRenderer, pSSSR_PrepareIndirectArgsPipeline);
            removePipeline(pRenderer, pSSSR_IntersectPipeline);
            removePipeline(pRenderer, pSSSR_ResolveSpatialPipeline);
            removePipeline(pRenderer, pSSSR_ResolveTemporalPipeline);
            removePipeline(pRenderer, pSSSR_ResolveEAWPipeline);
            removePipeline(pRenderer, pSSSR_ResolveEAWStride2Pipeline);
            removePipeline(pRenderer, pSSSR_ResolveEAWStride4Pipeline);
        }
    }

    void addRenderTargets()
    {
        // 1920x1080 VB is 8MB
        ESRAM_BEGIN_ALLOC(pRenderer, "layout 0", 0);
        VERIFY(addVisibilityBuffer());
        ESRAM_END_ALLOC(pRenderer);

        // layout 1 will overlap layout 0 in ESRAM.
        // The Reflection Buffer at 1920x1080 is 16MB and will overlap and exceed VB.
        ESRAM_BEGIN_ALLOC(pRenderer, "layout 1", 0);
        VERIFY(addReflectionBuffer());
        VERIFY(addDepthBuffer());
        VERIFY(addSceneBuffer());
        VERIFY(addNormalRoughnessBuffer());
        VERIFY(addIntermeditateBuffer());
        ESRAM_END_ALLOC(pRenderer);
    }

    void removeRenderTargets()
    {
        for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
        {
            removeResource(pSSSR_GenMipsBuffers[i]);
        }
        removeRenderTarget(pRenderer, pRenderTargetVBPass);
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pSceneBuffer);

        for (uint8_t i = 0; i < gDataBufferCount; ++i)
        {
            removeRenderTarget(pRenderer, pNormalRoughnessBuffers[i]);
            removeRenderTarget(pRenderer, pSSSR_TemporalResults[i]);
        }

        removeRenderTarget(pRenderer, pReflectionBuffer);
        removeResource(pIntermediateBuffer);
        removeResource(pSSSR_DepthHierarchy);
        removeResource(pSSSR_TemporalVariance);
        removeRenderTarget(pRenderer, pSSSR_RayLength);
        removeResource(pSSSR_RayListBuffer);
        removeResource(pSSSR_TileListBuffer);
    }

    bool addVisibilityBuffer()
    {
        RenderTargetDesc vbRTDesc = {};
        vbRTDesc.mArraySize = 1;
        vbRTDesc.mClearValue = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        vbRTDesc.mDepth = 1;
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        vbRTDesc.mWidth = gSceneRes.mWidth;
        vbRTDesc.mHeight = gSceneRes.mHeight;
        vbRTDesc.mSampleCount = SAMPLE_COUNT_1;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);

        return pRenderTargetVBPass != NULL;
    }

    bool addSceneBuffer()
    {
        RenderTargetDesc sceneRT = {};
        sceneRT.mArraySize = 1;
        sceneRT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        sceneRT.mDepth = 1;
        sceneRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        sceneRT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        sceneRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        sceneRT.mHeight = gSceneRes.mHeight;
        sceneRT.mWidth = gSceneRes.mWidth;
        sceneRT.mSampleCount = SAMPLE_COUNT_1;
        sceneRT.mSampleQuality = 0;
        sceneRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        sceneRT.pName = "Scene Buffer";

        addRenderTarget(pRenderer, &sceneRT, &pSceneBuffer);

        return pSceneBuffer != NULL;
    }

    bool addNormalRoughnessBuffer()
    {
        RenderTargetDesc desc = {};
        desc.mArraySize = 1;
        desc.mClearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        desc.mDepth = 1;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        desc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        desc.mHeight = gSceneRes.mHeight;
        desc.mWidth = gSceneRes.mWidth;
        desc.mSampleCount = SAMPLE_COUNT_1;
        desc.mSampleQuality = 0;
        desc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        desc.pName = "Normal Roughness Buffer";

        for (uint8_t i = 0; i < gDataBufferCount; ++i)
        {
            addRenderTarget(pRenderer, &desc, &pNormalRoughnessBuffers[i]);

            if (pNormalRoughnessBuffers[i] == NULL)
                return false;
        }

        return true;
    }

    bool addReflectionBuffer()
    {
        RenderTargetDesc RT = {};
        RT.mArraySize = 1;
        RT.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        RT.mDepth = 1;
        RT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        RT.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        RT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        RT.mHeight = gSceneRes.mHeight;
        RT.mWidth = gSceneRes.mWidth;
        RT.mSampleCount = SAMPLE_COUNT_1;
        RT.mSampleQuality = 0;
        RT.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        RT.pName = "Reflection Buffer";

        addRenderTarget(pRenderer, &RT, &pReflectionBuffer);

        return pReflectionBuffer != NULL;
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = { { 0.0f, 0 } };
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        depthRT.mHeight = gSceneRes.mHeight;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = gSceneRes.mWidth;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        depthRT.pName = "Depth Buffer";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }

    bool addIntermeditateBuffer()
    {
        int bufferSize = gSceneRes.mWidth * gSceneRes.mHeight * pSwapChain->ppRenderTargets[0]->mArraySize;

        // Add Intermediate buffer
        BufferLoadDesc IntermediateBufferDesc = {};
        IntermediateBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        IntermediateBufferDesc.mDesc.mElementCount = bufferSize;
        IntermediateBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
        IntermediateBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        IntermediateBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        IntermediateBufferDesc.mDesc.mSize = IntermediateBufferDesc.mDesc.mStructStride * bufferSize;
        IntermediateBufferDesc.mDesc.pName = "PPR Intermediate buffer";

        gInitializeVal = (uint32_t*)tf_realloc(gInitializeVal, IntermediateBufferDesc.mDesc.mSize);
        memset(gInitializeVal, 255, IntermediateBufferDesc.mDesc.mSize);

        SyncToken token = {};
        IntermediateBufferDesc.pData = gInitializeVal;
        IntermediateBufferDesc.ppBuffer = &pIntermediateBuffer;
        addResource(&IntermediateBufferDesc, &token);

        if (pIntermediateBuffer == NULL)
            return false;
        waitForToken(&token);

        TextureDesc depthHierarchyDesc = {};
        depthHierarchyDesc.mArraySize = 1;
        depthHierarchyDesc.mDepth = 1;
        depthHierarchyDesc.mFormat = TinyImageFormat_R32_SFLOAT;
        depthHierarchyDesc.mHeight = gSceneRes.mHeight;
        depthHierarchyDesc.mWidth = gSceneRes.mWidth;
        depthHierarchyDesc.mMipLevels =
            static_cast<uint32_t>(log2(gSceneRes.mWidth > gSceneRes.mHeight ? gSceneRes.mWidth : gSceneRes.mHeight)) + 1;
        depthHierarchyDesc.mSampleCount = SAMPLE_COUNT_1;
        depthHierarchyDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        depthHierarchyDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        depthHierarchyDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        depthHierarchyDesc.pName = "SSSR_DepthHierarchy";

        TextureLoadDesc depthHierarchyLoadDesc = {};
        depthHierarchyLoadDesc.pDesc = &depthHierarchyDesc;
        depthHierarchyLoadDesc.ppTexture = &pSSSR_DepthHierarchy;
        addResource(&depthHierarchyLoadDesc, NULL);

        if (pSSSR_DepthHierarchy == NULL)
            return false;

        uint32_t mipSizeX = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mWidth));
        uint32_t mipSizeY = 1 << (uint32_t)ceil(log2((float)pDepthBuffer->mHeight));
        uint     mipSizes[GENERATE_MIPS_MAX_MIPS][2] = {};
        for (uint32_t i = 1; i < pSSSR_DepthHierarchy->mMipLevels; ++i)
        {
            mipSizeX >>= 1;
            mipSizeY >>= 1;
            mipSizes[i][0] = mipSizeX;
            mipSizes[i][1] = mipSizeY;
            BufferLoadDesc bufferDesc = {};
            bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            bufferDesc.mDesc.mSize = sizeof(uint32_t) * 2;
            bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            bufferDesc.mDesc.pName = "Mip generations uniforms";
            bufferDesc.pData = mipSizes[i];
            bufferDesc.ppBuffer = &pSSSR_GenMipsBuffers[i];
            addResource(&bufferDesc, NULL);
        }

        RenderTargetDesc intersectResultsDesc = {};
        intersectResultsDesc.mArraySize = 1;
        intersectResultsDesc.mDepth = 1;
        intersectResultsDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        intersectResultsDesc.mHeight = gSceneRes.mHeight;
        intersectResultsDesc.mWidth = gSceneRes.mWidth;
        intersectResultsDesc.mMipLevels = 1;
        intersectResultsDesc.mSampleCount = SAMPLE_COUNT_1;
        intersectResultsDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        intersectResultsDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        intersectResultsDesc.pName = "pSSSR_TemporalResults";
        intersectResultsDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        intersectResultsDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;

        for (uint8_t i = 0; i < gDataBufferCount; ++i)
        {
            addRenderTarget(pRenderer, &intersectResultsDesc, &pSSSR_TemporalResults[i]);

            if (pSSSR_TemporalResults[i] == NULL)
                return false;
        }

        const uint32_t capMask = FORMAT_CAP_READ | FORMAT_CAP_WRITE;
        const bool     isR16SFSupported = (pRenderer->pGpu->mFormatCaps[TinyImageFormat_R16_SFLOAT] & capMask) == capMask;

        TextureDesc temporalVarianceDesc = {};
        temporalVarianceDesc.mArraySize = 1;
        temporalVarianceDesc.mDepth = 1;
        temporalVarianceDesc.mFormat = isR16SFSupported ? TinyImageFormat_R16_SFLOAT : TinyImageFormat_R32_SFLOAT;
        temporalVarianceDesc.mHeight = gSceneRes.mHeight;
        temporalVarianceDesc.mWidth = gSceneRes.mWidth;
        temporalVarianceDesc.mMipLevels = 1;
        temporalVarianceDesc.mSampleCount = SAMPLE_COUNT_1;
        temporalVarianceDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        temporalVarianceDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        temporalVarianceDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        temporalVarianceDesc.pName = "SSSR_TemporalVariance";

        TextureLoadDesc temporalVarianceLoadDesc = {};
        temporalVarianceLoadDesc.pDesc = &temporalVarianceDesc;
        temporalVarianceLoadDesc.ppTexture = &pSSSR_TemporalVariance;
        addResource(&temporalVarianceLoadDesc, NULL);

        if (pSSSR_TemporalVariance == NULL)
            return false;

        RenderTargetDesc rayLengthDesc = {};
        rayLengthDesc.mArraySize = 1;
        rayLengthDesc.mDepth = 1;
        rayLengthDesc.mFormat = isR16SFSupported ? TinyImageFormat_R16_SFLOAT : TinyImageFormat_R32_SFLOAT;
        rayLengthDesc.mHeight = gSceneRes.mHeight;
        rayLengthDesc.mWidth = gSceneRes.mWidth;
        rayLengthDesc.mMipLevels = 1;
        rayLengthDesc.mSampleCount = SAMPLE_COUNT_1;
        rayLengthDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        rayLengthDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        rayLengthDesc.pName = "SSSR_RayLength";
        rayLengthDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        rayLengthDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        addRenderTarget(pRenderer, &rayLengthDesc, &pSSSR_RayLength);

        if (pSSSR_RayLength == NULL)
            return false;

        BufferLoadDesc SSSR_RayListDesc = {};
        SSSR_RayListDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        SSSR_RayListDesc.mDesc.mElementCount = bufferSize;
        SSSR_RayListDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_RayListDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_RayListDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_RayListDesc.mDesc.mSize = SSSR_RayListDesc.mDesc.mStructStride * bufferSize;
        SSSR_RayListDesc.mDesc.pName = "SSSR_RayListBuffer";
        SSSR_RayListDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        SSSR_RayListDesc.pData = NULL;
        SSSR_RayListDesc.ppBuffer = &pSSSR_RayListBuffer;
        addResource(&SSSR_RayListDesc, NULL);

        if (pSSSR_RayListBuffer == NULL)
            return false;

        BufferLoadDesc SSSR_TileListDesc = {};
        SSSR_TileListDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        SSSR_TileListDesc.mDesc.mElementCount =
            ((gSceneRes.mWidth * gSceneRes.mHeight + 63) / 64) * pSwapChain->ppRenderTargets[0]->mArraySize;
        SSSR_TileListDesc.mDesc.mStructStride = sizeof(uint32_t);
        SSSR_TileListDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        SSSR_TileListDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
        SSSR_TileListDesc.mDesc.mSize = SSSR_TileListDesc.mDesc.mStructStride * SSSR_TileListDesc.mDesc.mElementCount;
        SSSR_TileListDesc.mDesc.pName = "SSSR_TileListBuffer";
        SSSR_TileListDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        SSSR_TileListDesc.pData = NULL;
        SSSR_TileListDesc.ppBuffer = &pSSSR_TileListBuffer;
        addResource(&SSSR_TileListDesc, NULL);

        return pSSSR_TileListBuffer != NULL;
    }
};

DEFINE_APPLICATION_MAIN(ScreenSpaceReflections)
