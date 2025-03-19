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

#include "../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Renderer/Interfaces/IVisibilityBuffer2.h"
#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../Common_3/Utilities/RingBuffer.h"
#include "../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "SanMiguel.h"

#include "../../../Common_3/Graphics/FSL/fsl_srt.h"
#include "Shaders/FSL/ShaderDefs.h.fsl"
#include "Shaders/FSL/TriangleBinning.h.fsl"

// fsl
#include "../../../Common_3/Graphics/FSL/defaults.h"
#include "Shaders/FSL/Structs.h"
#include "Shaders/FSL/Global.srt.h"
#include "Shaders/FSL/TriangleFiltering.srt.h"
#include "Shaders/FSL/Display.srt.h"
#include "Shaders/FSL/GodrayBlur.srt.h"
#include "Shaders/FSL/LightClusters.srt.h"

#if defined(XBOX)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#include "../../../Xbox/Common_3/Graphics/IESRAMManager.h"
#define BEGINALLOCATION(X, O) esramBeginAllocation(pRenderer->mDx.pESRAMManager, X, O)
#define ALLOCATIONOFFSET()    esramGetCurrentOffset(pRenderer->mDx.pESRAMManager)
#define ENDALLOCATION(X)      esramEndAllocation(pRenderer->mDx.pESRAMManager)
#else
#define BEGINALLOCATION(X, O)
#define ALLOCATIONOFFSET() 0u
#define ENDALLOCATION(X)
#endif

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)  \
    X(BindlessSupported, 1) \
    X(DisableAO, 0)         \
    X(DisableGodRays, 0)    \
    X(AddGeometryPassThrough, 0)

#define GENERATE_ENUM(x, y)   x,
#define GENERATE_STRING(x, y) #x,
#define GENERATE_STRUCT(x, y) uint32_t m##x = y;
#define GENERATE_VALUE(x, y)  y,
#define INIT_STRUCT(s)        s = { FOREACH_SETTING(GENERATE_VALUE) }

typedef enum ESettings
{
    FOREACH_SETTING(GENERATE_ENUM) Count
} ESettings;

const char* gSettingNames[] = { FOREACH_SETTING(GENERATE_STRING) };

// Useful for using names directly instead of subscripting an array
struct ConfigSettings
{
    FOREACH_SETTING(GENERATE_STRUCT)
} gGpuSettings;

static ThreadSystem gThreadSystem;

#define SCENE_SCALE 50.0f

typedef enum OutputMode
{
    OUTPUT_MODE_SDR = 0,
    OUTPUT_MODE_P2020,
    OUTPUT_MODE_COUNT
} OutputMode;

struct GodRayConstant
{
    float mScatterFactor;
};

GodRayConstant   gGodRayConstant{ 0.5f };
RenderTargetInfo gShadowRenderTargetInfo;
RenderTargetInfo gDepthRenderTargetInfo;
RenderTargetInfo gClearRenderTargetInfo;

struct GodRayBlurConstant
{
    uint32_t mBlurPassType; // Horizontal or Vertical pass
    uint32_t mFilterRadius;
};

#define MAX_BLUR_KERNEL_SIZE 8

struct BlurWeights
{
    float mBlurWeights[MAX_BLUR_KERNEL_SIZE];
};

GodRayBlurConstant gGodRayBlurConstant;
BlurWeights        gBlurWeightsUniform;
float              gGaussianBlurSigma[2] = { 1.0f, 1.0f };

static float gaussian(float x, float sigma)
{
    x = abs(x) / sigma;
    x *= x;
    return exp(-0.5f * x);
}

enum DisplayColorRange
{
    ColorRange_RGB = 0,
    ColorRange_YCbCr422 = 1,
    ColorRange_YCbCr444 = 2
};

enum DisplaySignalRange
{
    Display_SIGNAL_RANGE_FULL = 0,
    Display_SIGNAL_RANGE_NARROW = 1
};

enum DisplayColorSpace
{
    ColorSpace_Rec709 = 0,
    ColorSpace_Rec2020 = 1,
    ColorSpace_P3D65 = 2
};

struct DisplayChromacities
{
    float RedX;
    float RedY;
    float GreenX;
    float GreenY;
    float BlueX;
    float BlueY;
    float WhiteX;
    float WhiteY;
};

enum BlurPassType
{
    BLUR_PASS_TYPE_HORIZONTAL,
    BLUR_PASS_TYPE_VERTICAL,
    BLUR_PASS_TYPE_COUNT
};

// Camera Walking
static float gCameraWalkingTime = 0.0f;
float3*      gCameraPathData;

uint  gCameraPoints;
float gTotalElpasedTime;
/************************************************************************/
// GUI CONTROLS
/************************************************************************/
#if defined(ANDROID)
#define DEFAULT_ASYNC_COMPUTE false
#else
#define DEFAULT_ASYNC_COMPUTE true
#endif

typedef struct AppSettings
{
    OutputMode mOutputMode = OUTPUT_MODE_SDR;

    bool mSmallScaleRaster = true;

    bool mAsyncCompute = DEFAULT_ASYNC_COMPUTE;
    // toggle rendering of local point lights
    bool mRenderLocalLights = false;

    bool mDrawDebugTargets = false;

    bool mLargeBinRasterGroups = true;

    float nearPlane = 10.0f;
    float farPlane = 3000.0f;

    float lightNearPlane = -0.1f;
    float lightFarPlane = 1500.0f;

    // adjust directional sunlight angle
    float2 mSunControl = { -2.1f, 0.164f };

    float mSunSize = 300.0f;

    float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

    DynamicUIWidgets mDynamicUIWidgetsGR;
    bool             mEnableGodray = true;
    uint32_t         mFilterRadius = 3;

    float mEsmControl = 200.0f;

    bool mVisualizeGeometry = false;
    bool mVisualizeBinTriangleCount = false;
    bool mVisualizeBinOccupancy = false;

    // AO data
    DynamicUIWidgets mDynamicUIWidgetsAO;
    bool             mEnableAO = true;
    bool             mVisualizeAO = false;
    float            mAOIntensity = 3.0f;
    int              mAOQuality = 2;

    DynamicUIWidgets mLinearScale;
    float            LinearScale = 260.0f;

    // HDR10
    DynamicUIWidgets mDisplaySetting;

    DisplayColorSpace  mCurrentSwapChainColorSpace = ColorSpace_Rec2020;
    DisplayColorRange  mDisplayColorRange = ColorRange_RGB;
    DisplaySignalRange mDisplaySignalRange = Display_SIGNAL_RANGE_FULL;

    // Camera Walking
    bool  cameraWalking = false;
    float cameraWalkingSpeed = 1.0f;

} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Constants
const uint32_t gShadowMapSize = 1024;

struct UniformDataSkybox
{
    mat4 mProjectView;
    vec3 mCamPos;
};

int gGodrayScale = 2;

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = NUM_GEOMETRY_SETS;

/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3                  gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    PerFrameConstantsData gPerFrameUniformData = {};
    VBViewConstantsData   gVBViewUniformData = {};
    UniformDataSkybox     gUniformDataSky;

    // These are just used for statistical information
    uint32_t gTotalClusters = 0;
    uint32_t gCulledClusters = 0;
    uint32_t gDrawCount[gNumGeomSets] = {};
    uint32_t gTotalDrawCount = {};
};

uint32_t gIndexCount = 0;

/************************************************************************/
// Scene
/************************************************************************/
Scene*            gScene = NULL;
/************************************************************************/
// Settings
/************************************************************************/
AppSettings       gAppSettings;
/************************************************************************/
// Profiling
/************************************************************************/
ProfileToken      gGraphicsProfileToken;
ProfileToken      gComputeProfileToken;
/************************************************************************/
// Rendering data
/************************************************************************/
Renderer*         pRenderer = NULL;
VisibilityBuffer* pVisibilityBuffer = NULL;
/************************************************************************/
// Queues and Command buffers
/************************************************************************/
Queue*            pGraphicsQueue = NULL;
GpuCmdRing        gGraphicsCmdRing = {};

Queue*     pComputeQueue = NULL;
GpuCmdRing gComputeCmdRing = {};

DescriptorSet* pDescriptorSetPersistent = NULL;
DescriptorSet* pDescriptorSetPerFrame = NULL;
DescriptorSet* pDescriptorSetDisplayPerDraw = NULL;
DescriptorSet* pDescriptorSetRenderTargetPerBatch = NULL;
DescriptorSet* pDescriptorSetClusterLights = NULL;
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain*     pSwapChain = NULL;
Semaphore*     pImageAcquiredSemaphore = NULL;
Semaphore*     pPresentSemaphore = NULL;
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*        pShaderClearBuffers = nullptr;
Pipeline*      pPipelineClearBuffers = nullptr;
/************************************************************************/
// Clear VisibilityBuffer pipeline
/************************************************************************/
Shader*        pShaderClearRenderTarget = NULL;
Pipeline*      pPipelineClearRenderTarget = NULL;
uint32_t       gClearRenderTargetRootConstantIndex;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*        pShaderTriangleFiltering = nullptr;
Pipeline*      pPipelineTriangleFiltering = nullptr;
DescriptorSet* pDescriptorSetTriangleFilteringPerBatch = NULL;
DescriptorSet* pDescriptorSetTriangleFilteringPerDraw = NULL;
DescriptorSet* pDescriptorSetTriangleFilteringPerDrawCompute = NULL; // Used in async compute shaders
/************************************************************************/
// Clear light clusters pipeline
/************************************************************************/
Shader*        pShaderClearLightClusters = nullptr;
Pipeline*      pPipelineClearLightClusters = nullptr;
/************************************************************************/
// Compute light clusters pipeline
/************************************************************************/
Shader*        pShaderClusterLights = nullptr;
Pipeline*      pPipelineClusterLights = nullptr;
/************************************************************************/
// Shadow pass pipeline
/************************************************************************/

/************************************************************************/
// GPU Driven Visibility Buffer Filling
/************************************************************************/
// Below are the GPU resources created for compute based visibility buffer filling.
// Unfortunately, to be able to stay cross platform, we need Root Signatures per each compute shader.
// This is due to Prospero requiring exact same bindings in each shader that shares the same root signature.
// Shaders below do not share the exact bindings with each other.
/************************************************************************/
// GPU Driven Visibility Buffer Filling - Depth/Shadow
/************************************************************************/
// Small triangles
uint32_t  gVisibilityBufferDepthRasterRootConstantIndex = 0;
Shader*   pShaderVisibilityBufferDepthRaster = nullptr;
Pipeline* pPipelineVisibilityBufferDepthRaster = nullptr;
/************************************************************************/
// VB Blit Depth
/************************************************************************/
Shader*   pShaderBlitDepth = nullptr;
Pipeline* pPipelineBlitDepth = nullptr;
uint32_t  gBlitDepthRootConstantIndex = 0;
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*   pShaderVisibilityBufferShade[2] = { nullptr };
Pipeline* pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
/************************************************************************/
// Skybox pipeline
/************************************************************************/
Shader*   pShaderSkybox = nullptr;
Pipeline* pSkyboxPipeline = nullptr;
Buffer*   pSkyboxVertexBuffer = NULL;
Texture*  pSkybox = NULL;
/************************************************************************/
// Godray pipeline
/************************************************************************/
Shader*   pGodRayPass = nullptr;
Pipeline* pPipelineGodRayPass = nullptr;
Buffer*   pBufferGodRayConstant = nullptr;
uint32_t  gGodRayConstantIndex = 0;

Shader*        pShaderGodRayBlurPass = nullptr;
Pipeline*      pPipelineGodRayBlurPass = nullptr;
DescriptorSet* pDescriptorSetGodRayBlurPassPerDraw = nullptr;
Buffer*        pBufferBlurWeights = nullptr;

Buffer*   pGodRayBlurBuffer[2][gDataBufferCount] = { { NULL } };
uint32_t  gGodRayBlurConstantIndex = 0;
/************************************************************************/
// Curve Conversion pipeline
/************************************************************************/
Shader*   pShaderCurveConversion = nullptr;
Pipeline* pPipelineCurveConversionPass = nullptr;

OutputMode         gWasOutputMode = gAppSettings.mOutputMode;
DisplayColorSpace  gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
DisplayColorRange  gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
DisplaySignalRange gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;

/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*       pShaderPresentPass = nullptr;
Pipeline*     pPipelinePresentPass = nullptr;
uint32_t      gSCurveRootConstantIndex = 0;
/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pDepthBuffer = NULL;
Buffer*       pVBDepthBuffer[2] = { NULL };
RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pRenderTargetShadow = NULL;
RenderTarget* pIntermediateRenderTarget = NULL;
RenderTarget* pRenderTargetGodRay[2] = { NULL };
RenderTarget* pCurveConversionRenderTarget = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler*      pSamplerTrilinearAniso = NULL;
Sampler*      pSamplerBilinear = NULL;
Sampler*      pSamplerPointClamp = NULL;
Sampler*      pSamplerBilinearClamp = NULL;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture**     gDiffuseMapsStorage = NULL;
Texture**     gNormalMapsStorage = NULL;
Texture**     gSpecularMapsStorage = NULL;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
Geometry*     pGeom = NULL;
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer*       pMaterialPropertyBuffer = NULL;
Buffer*       pPerFrameUniformBuffers[gDataBufferCount] = { NULL };
// used in bin_rasterizer, clear_render_target and visibilityBuffer_blitDepth
Buffer*       pRenderTargetInfoConstantsBuffers[gDataBufferCount][3] = { { NULL } };
enum
{
    VB_UB_COMPUTE = 0,
    VB_UB_GRAPHICS,
    VB_UB_COUNT
};
Buffer*  pPerFrameVBUniformBuffers[VB_UB_COUNT][gDataBufferCount] = {};
// Buffers containing all indirect draw commands per geometry set (no culling)
uint32_t gDrawCountAll[gNumGeomSets] = {};
Buffer*  pMeshConstantsBuffer = NULL;

/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*          pLightsBuffer = NULL;
Buffer**         gPerBatchUniformBuffers = NULL;
Buffer*          pVertexBufferCube = NULL;
Buffer*          pIndexBufferCube = NULL;
Buffer*          pLightClustersCount[gDataBufferCount] = { NULL };
Buffer*          pLightClusters[gDataBufferCount] = { NULL };
Buffer*          pUniformBufferSky[gDataBufferCount] = { NULL };
uint64_t         gFrameCount = 0;
FilterContainer* pFilterContainers = NULL;
uint32_t         gMeshCount = 0;
uint32_t         gMaterialCount = 0;
UIComponent*     pGuiWindow = NULL;
UIComponent*     pDebugTexturesWindow = NULL;
UIWidget*        pOutputSupportsHDRWidget = NULL;
FontDrawDesc     gFrameTimeDraw;
uint32_t         gFontID = 0;

/************************************************************************/
ICameraController* pCameraController = NULL;
/************************************************************************/
// CPU staging data
/************************************************************************/
// CPU buffers for light data
LightData          gLightData[LIGHT_COUNT] = {};

PerFrameData  gPerFrame[gDataBufferCount] = {};
RenderTarget* pScreenRenderTarget = NULL;

// We are rendering the scene (geometry, skybox, ...) at this resolution, UI at window resolution (mSettings.mWidth, mSettings.mHeight)
// Render scene at gSceneRes
// presentImage -> The scene rendertarget composed into the swapchain/backbuffer
// Render UI into backbuffer
static Resolution gSceneRes;
/************************************************************************/
// Screen resolution UI data
/************************************************************************/
#if defined(_WINDOWS)
struct ResolutionData
{
    // Buffer for all res name strings
    char*        mResNameContainer;
    // Array of const char*
    const char** mResNamePointers;
};

static ResolutionData gGuiResolution = { NULL, NULL };
#endif

const char*    pPipelineCacheName = "PipelineCache.cache";
PipelineCache* pPipelineCache = NULL;

/************************************************************************/
// App implementation
/************************************************************************/
void SetupDebugTexturesWindow()
{
    float  scale = 0.15f;
    float2 screenSize = { (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight };
    float2 texSize = screenSize * scale;

    if (!pDebugTexturesWindow)
    {
        UIComponentDesc UIComponentDesc = {};
        UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
        UIComponentDesc.mStartPosition.setY(screenSize.getY() - texSize.getY() - 50.f);
        uiAddComponent("DEBUG RTs", &UIComponentDesc, &pDebugTexturesWindow);

        DebugTexturesWidget widget;
        luaRegisterWidget(uiAddComponentWidget(pDebugTexturesWindow, "Debug RTs", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
    }

    static const Texture* pVBRTs[6];
    uint32_t              textureCount = 0;

    pVBRTs[textureCount++] = pRenderTargetVBPass->pTexture;
    pVBRTs[textureCount++] = pRenderTargetShadow->pTexture;
    pVBRTs[textureCount++] = pDepthBuffer->pTexture;

    if (pDebugTexturesWindow)
    {
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->pTextures = pVBRTs;
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->mTexturesCount = textureCount;
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->mTextureDisplaySize = texSize;
    }
}

class Visibility_Buffer: public IApp
{
public:
    Visibility_Buffer() { SetContentScaleFactor(2.0f); }
    bool Init()
    {
        threadSystemInit(&gThreadSystem, &gThreadSystemInitDescDefault);

        // Camera Walking
        loadCameraPath("cameraPath.txt", gCameraPoints, &gCameraPathData);
        gCameraPoints = (uint)29084 / 2;
        gTotalElpasedTime = (float)gCameraPoints * 0.00833f;

        /************************************************************************/
        // Initialize the Forge renderer with the appropriate parameters.
        /************************************************************************/
        INIT_STRUCT(gGpuSettings);
        ExtendedSettings extendedSettings = {};
        extendedSettings.mNumSettings = ESettings::Count;
        extendedSettings.pSettings = (uint32_t*)&gGpuSettings;
        extendedSettings.ppSettingNames = gSettingNames;

        RendererDesc settings = {};
        settings.pExtendedSettings = &extendedSettings;
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        // turn off by default depending on gpu config rules
        gAppSettings.mEnableGodray &= !gGpuSettings.mDisableGodRays;
        gAppSettings.mEnableAO &= !gGpuSettings.mDisableAO;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        QueueDesc computeQueueDesc = {};
        computeQueueDesc.mType = QUEUE_TYPE_COMPUTE;
        computeQueueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);
        initSemaphore(pRenderer, &pPresentSemaphore);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        cmdRingDesc = {};
        cmdRingDesc.pQueue = pComputeQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gComputeCmdRing);
        /************************************************************************/
        // Initialize helper interfaces (resource loader, profiler)
        /************************************************************************/
        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

        PipelineCacheLoadDesc cacheDesc = {};
        cacheDesc.pFileName = pPipelineCacheName;
        loadPipelineCache(pRenderer, &cacheDesc, &pPipelineCache);

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        /************************************************************************/
        // Setup the UI components for text rendering, UI controls...
        /************************************************************************/

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        /************************************************************************/
        /************************************************************************/
        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        initProfiler(&profiler);

        gGraphicsProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gComputeProfileToken = initGpuProfiler(pRenderer, pComputeQueue, "Compute");
        /************************************************************************/
        // Start timing the scene load
        /************************************************************************/
        HiresTimer totalTimer;
        initHiresTimer(&totalTimer);
        /************************************************************************/
        // Setup sampler states
        /************************************************************************/
        // Create sampler for VB render target
        SamplerDesc trilinearDesc = { FILTER_LINEAR,
                                      FILTER_LINEAR,
                                      MIPMAP_MODE_LINEAR,
                                      ADDRESS_MODE_REPEAT,
                                      ADDRESS_MODE_REPEAT,
                                      ADDRESS_MODE_REPEAT,
                                      0.0f,
                                      false,
                                      0.0f,
                                      0.0f,
                                      8.0f };
        SamplerDesc bilinearDesc = { FILTER_LINEAR,       FILTER_LINEAR,       MIPMAP_MODE_LINEAR,
                                     ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT };
        SamplerDesc pointDesc = { FILTER_NEAREST,
                                  FILTER_NEAREST,
                                  MIPMAP_MODE_NEAREST,
                                  ADDRESS_MODE_CLAMP_TO_EDGE,
                                  ADDRESS_MODE_CLAMP_TO_EDGE,
                                  ADDRESS_MODE_CLAMP_TO_EDGE };

        SamplerDesc bilinearClampDesc = { FILTER_LINEAR,
                                          FILTER_LINEAR,
                                          MIPMAP_MODE_LINEAR,
                                          ADDRESS_MODE_CLAMP_TO_EDGE,
                                          ADDRESS_MODE_CLAMP_TO_EDGE,
                                          ADDRESS_MODE_CLAMP_TO_EDGE };

        addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
        addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
        addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
        addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);

        BufferLoadDesc godrayConstantBufferDesc = {};
        godrayConstantBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        godrayConstantBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        godrayConstantBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        godrayConstantBufferDesc.mDesc.mSize = sizeof(GodRayConstant);
        godrayConstantBufferDesc.pData = &gGodRayConstant;
        godrayConstantBufferDesc.ppBuffer = &pBufferGodRayConstant;
        addResource(&godrayConstantBufferDesc, NULL);

        BufferLoadDesc blitDepthConstantBufferDesc = {};
        blitDepthConstantBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        blitDepthConstantBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        blitDepthConstantBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;

        for (uint32_t i = 0; i < gDataBufferCount; i++)
        {
            gShadowRenderTargetInfo = { 0, (int)gShadowMapSize, (int)gShadowMapSize };
            blitDepthConstantBufferDesc.mDesc.mSize = sizeof(RenderTargetInfo);
            blitDepthConstantBufferDesc.pData = &gShadowRenderTargetInfo;
            blitDepthConstantBufferDesc.ppBuffer = &pRenderTargetInfoConstantsBuffers[i][0];
            addResource(&blitDepthConstantBufferDesc, NULL);

            gDepthRenderTargetInfo = { 1, (int)gSceneRes.mWidth, (int)gSceneRes.mHeight };
            blitDepthConstantBufferDesc.mDesc.mSize = sizeof(RenderTargetInfo);
            blitDepthConstantBufferDesc.pData = &gDepthRenderTargetInfo;
            blitDepthConstantBufferDesc.ppBuffer = &pRenderTargetInfoConstantsBuffers[i][1];
            addResource(&blitDepthConstantBufferDesc, NULL);

            gClearRenderTargetInfo = { 0, (int)gSceneRes.mWidth * (int)gSceneRes.mHeight, 0 };
            blitDepthConstantBufferDesc.mDesc.mSize = sizeof(RenderTargetInfo);
            blitDepthConstantBufferDesc.pData = &gClearRenderTargetInfo;
            blitDepthConstantBufferDesc.ppBuffer = &pRenderTargetInfoConstantsBuffers[i][2];
            addResource(&blitDepthConstantBufferDesc, NULL);
        }

        BufferLoadDesc godrayBlurConstantBufferDesc = {};
        godrayBlurConstantBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        godrayBlurConstantBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        godrayBlurConstantBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        godrayBlurConstantBufferDesc.mDesc.mSize = sizeof(GodRayBlurConstant);
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            godrayBlurConstantBufferDesc.pData = &gGodRayBlurConstant;
            godrayBlurConstantBufferDesc.ppBuffer = &pGodRayBlurBuffer[0][frameIdx];
            addResource(&godrayBlurConstantBufferDesc, NULL);
            godrayBlurConstantBufferDesc.pData = &gGodRayBlurConstant;
            godrayBlurConstantBufferDesc.ppBuffer = &pGodRayBlurBuffer[1][frameIdx];
            addResource(&godrayBlurConstantBufferDesc, NULL);
        }

        for (int i = 0; i < MAX_BLUR_KERNEL_SIZE; i++)
        {
            gBlurWeightsUniform.mBlurWeights[i] = gaussian((float)i, gGaussianBlurSigma[0]);
        }

        BufferLoadDesc blurWeightsBufferDesc = {};
        blurWeightsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        blurWeightsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        blurWeightsBufferDesc.mDesc.mSize = sizeof(BlurWeights);
        blurWeightsBufferDesc.ppBuffer = &pBufferBlurWeights;
        blurWeightsBufferDesc.pData = &gBlurWeightsUniform;
        blurWeightsBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        addResource(&blurWeightsBufferDesc, NULL);

        /************************************************************************/
        // Load resources for skybox
        /************************************************************************/
        TextureLoadDesc skyboxTriDesc = {};
        skyboxTriDesc.pFileName = "SanMiguel_3/daytime_cube.tex";
        skyboxTriDesc.ppTexture = &pSkybox;
        addResource(&skyboxTriDesc, NULL);
        /************************************************************************/
        // Load the scene using the SceneLoader class
        /************************************************************************/
        HiresTimer sceneLoadTimer;
        initHiresTimer(&sceneLoadTimer);

        GeometryLoadDesc sceneLoadDesc = {};
        sceneLoadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED;
        SyncToken token = {};
        gScene = initSanMiguel(&sceneLoadDesc, token, false);

        if (!gScene)
            return false;
        LOGF(LogLevel::eINFO, "Load scene : %f ms", getHiresTimerUSec(&sceneLoadTimer, true) / 1000.0f);

        gMeshCount = gScene->geom->mDrawArgCount;
        gMaterialCount = gScene->geom->mDrawArgCount;
        pFilterContainers = (FilterContainer*)tf_calloc(gMeshCount, sizeof(FilterContainer));
        pGeom = gScene->geom;
        /************************************************************************/
        // Texture loading
        /************************************************************************/
        gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gNormalMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gSpecularMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);

        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            TextureLoadDesc desc = {};
            desc.pFileName = gScene->textures[i];
            desc.ppTexture = &gDiffuseMapsStorage[i];
            // Textures representing color should be stored in SRGB or HDR format
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = gScene->normalMaps[i];
            desc.ppTexture = &gNormalMapsStorage[i];
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = gScene->specularMaps[i];
            desc.ppTexture = &gSpecularMapsStorage[i];
            addResource(&desc, NULL);
        }

        // Filtered index buffer will be organized by batches. Each batch can have up to 256 triangles.
        // Here we calculate the number of maximum indices we need for the filtered index buffer.
        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            gIndexCount +=
                (uint32_t)ceil((gScene->geom->pDrawArgs + i)->mIndexCount / 3.f / RASTERIZE_BATCH_SIZE) * RASTERIZE_BATCH_SIZE * 3;
        }

        gIndexCount = MAX_BATCHES * RASTERIZE_BATCH_SIZE * 3;

        // Init visibility buffer
        VisibilityBufferDesc vbDesc = {};
        vbDesc.mFilterBatchCount = FILTER_BATCH_COUNT;
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = gDataBufferCount;
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mIndexCount = gIndexCount;
        vbDesc.mFilterBatchSize = FILTER_BATCH_SIZE;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        /************************************************************************/
        // Cluster creation
        /************************************************************************/
        HiresTimer clusterTimer;
        initHiresTimer(&clusterTimer);

        // Calculate clusters
        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            MaterialFlags material = gScene->materialFlags[i];

            FilterContainerDescriptor desc = {};
            // desc.mType = FILTER_CONTAINER_TYPE_CLUSTER;
            desc.mBaseIndex = (gScene->geom->pDrawArgs + i)->mStartIndex;
            desc.mInstanceIndex = INSTANCE_INDEX_NONE;

            desc.mGeometrySet = GEOMSET_OPAQUE;
            if (material & MATERIAL_FLAG_ALPHA_TESTED)
                desc.mGeometrySet = GEOMSET_ALPHA_CUTOUT;

            desc.mIndexCount = (gScene->geom->pDrawArgs + i)->mIndexCount;
            desc.mMeshIndex = i;
            addVBFilterContainer(&desc, &pFilterContainers[i]);
        }

        LOGF(LogLevel::eINFO, "Load clusters : %f ms", getHiresTimerUSec(&clusterTimer, true) / 1000.0f);

        // Create geometry for light rendering
        createCubeBuffers(pRenderer, &pVertexBufferCube, &pIndexBufferCube);

        // Generate sky box vertex buffer
        const float skyBoxPoints[] = {
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

        uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.pData = skyBoxPoints;
        skyboxVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
        addResource(&skyboxVbDesc, NULL);

        /************************************************************************/
        // Most important options
        /************************************************************************/
        // Default NX settings for better performance.
#if NX64
        // Async compute is not optimal on the NX platform. Turning this off to make use of default graphics queue for triangle visibility.
        gAppSettings.mAsyncCompute = false;
        // High fill rate features are also disabled by default for performance.
        gAppSettings.mEnableGodray = false;
        gAppSettings.mEnableAO = false;
#endif

        /************************************************************************/
        // Finish the resource loading process since the next code depends on the loaded resources
        /************************************************************************/
        waitForAllResourceLoads();

        HiresTimer setupBuffersTimer;
        initHiresTimer(&setupBuffersTimer);
        addTriangleFilteringBuffers(gScene);

        LOGF(LogLevel::eINFO, "Setup buffers : %f ms", getHiresTimerUSec(&setupBuffersTimer, true) / 1000.0f);

        LOGF(LogLevel::eINFO, "Total Load Time : %f ms", getHiresTimerUSec(&totalTimer, true) / 1000.0f);

        /************************************************************************/
        // Setup the fps camera for navigating through the scene
        /************************************************************************/
        vec3                   startPosition(620.0f, 490.0f, 70.0f);
        vec3                   startLookAt = startPosition + vec3(-1.0f - 0.0f, 0.1f, 0.0f);
        CameraMotionParameters camParams;
        camParams.acceleration = 1300 * 2.5f;
        camParams.braking = 1300 * 2.5f;
        camParams.maxSpeed = 200 * 2.5f;
        pCameraController = initFpsCameraController(startPosition, startLookAt);
        pCameraController->setMotionParameters(camParams);

        // App Actions
        AddCustomInputBindings();
        initScreenshotCapturer(pRenderer, pGraphicsQueue, GetName());
        return true;
    }

    void Exit()
    {
        exitScreenshotCapturer();

        tf_free(gCameraPathData);

        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);

        exitCameraController(pCameraController);
        for (uint32_t i = 0; i < gDataBufferCount; i++)
        {
            removeResource(pRenderTargetInfoConstantsBuffers[i][2]);
            removeResource(pRenderTargetInfoConstantsBuffers[i][1]);
            removeResource(pRenderTargetInfoConstantsBuffers[i][0]);
        }
        removeResource(pBufferGodRayConstant);
        removeResource(pGodRayBlurBuffer[0][0]);
        removeResource(pGodRayBlurBuffer[0][1]);
        removeResource(pGodRayBlurBuffer[1][0]);
        removeResource(pGodRayBlurBuffer[1][1]);
        removeResource(pBufferBlurWeights);

        removeResource(pSkybox);
        removeTriangleFilteringBuffers();

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        // Destroy geometry for light rendering
        removeResource(pVertexBufferCube);
        removeResource(pIndexBufferCube);

        /************************************************************************/
        // Remove loaded scene
        /************************************************************************/
        exitSanMiguel(gScene);

        // Destroy scene buffers
        removeResource(pGeom);

        removeResource(pSkyboxVertexBuffer);

        // Remove Textures
        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            removeResource(gDiffuseMapsStorage[i]);
            removeResource(gNormalMapsStorage[i]);
            removeResource(gSpecularMapsStorage[i]);
        }

        tf_free(gDiffuseMapsStorage);
        tf_free(gNormalMapsStorage);
        tf_free(gSpecularMapsStorage);
        tf_free(pFilterContainers);

#if defined(_WINDOWS)
        arrfree(gGuiResolution.mResNameContainer);
        arrfree(gGuiResolution.mResNamePointers);
#endif
        /************************************************************************/
        /************************************************************************/
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitSemaphore(pRenderer, pPresentSemaphore);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitGpuCmdRing(pRenderer, &gComputeCmdRing);

        exitQueue(pRenderer, pGraphicsQueue);
        exitQueue(pRenderer, pComputeQueue);

        removeSampler(pRenderer, pSamplerTrilinearAniso);
        removeSampler(pRenderer, pSamplerBilinear);
        removeSampler(pRenderer, pSamplerPointClamp);
        removeSampler(pRenderer, pSamplerBilinearClamp);

        PipelineCacheSaveDesc saveDesc = {};
        saveDesc.pFileName = pPipelineCacheName;
        savePipelineCache(pRenderer, pPipelineCache, &saveDesc);
        removePipelineCache(pRenderer, pPipelineCache);

        exitVisibilityBuffer(pVisibilityBuffer);
        exitRootSignature(pRenderer);
        exitResourceLoaderInterface(pRenderer);

        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = NULL;
    }

    // Setup the render targets used in this demo.
    // The only render target that is being currently used stores the results of the Visibility Buffer pass.
    // As described earlier, this render target uses 32 bit per pixel to store draw / triangle IDs that will be
    // loaded later by the shade step to reconstruct interpolated triangle data per pixel.
    bool Load(ReloadDesc* pReloadDesc)
    {
        gSceneRes = getGPUCfgSceneResolution(mSettings.mWidth, mSettings.mHeight);
        gFrameCount = 0;

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc UIComponentDesc = {};
            UIComponentDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent(GetName(), &UIComponentDesc, &pGuiWindow);
            uiSetComponentFlags(pGuiWindow, GUI_COMPONENT_FLAGS_NO_RESIZE);

            /************************************************************************/
            // Most important options
            /************************************************************************/
            CheckboxWidget checkbox;
            checkbox.pData = &gAppSettings.mSmallScaleRaster;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable Small Scale Rasterization", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mAsyncCompute;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Async Compute", &checkbox, WIDGET_TYPE_CHECKBOX));

#if defined(FORGE_DEBUG)
            checkbox.pData = &gAppSettings.mVisualizeGeometry;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Visualize Geometry", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mVisualizeBinTriangleCount;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Visualize Bin Triangle Count", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mVisualizeBinOccupancy;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Visualize Bin Occupancy", &checkbox, WIDGET_TYPE_CHECKBOX));
#endif

            checkbox.pData = &gAppSettings.mDrawDebugTargets;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Draw Debug Targets", &checkbox, WIDGET_TYPE_CHECKBOX));
            /************************************************************************/
            /************************************************************************/
            if (pRenderer->pGpu->mHDRSupported)
            {
                LabelWidget labelWidget = {};
                pOutputSupportsHDRWidget = uiAddComponentWidget(pGuiWindow, "Output Supports HDR", &labelWidget, WIDGET_TYPE_LABEL);
                REGISTER_LUA_WIDGET(pOutputSupportsHDRWidget);

                static const char* outputModeNames[] = { "SDR", "HDR10" };

                DropdownWidget outputMode;
                outputMode.pData = (uint32_t*)&gAppSettings.mOutputMode;
                outputMode.pNames = outputModeNames;
                outputMode.mCount = sizeof(outputModeNames) / sizeof(outputModeNames[0]);
                luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Output Mode", &outputMode, WIDGET_TYPE_DROPDOWN));
            }

            checkbox.pData = &gAppSettings.cameraWalking;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Cinematic Camera walking", &checkbox, WIDGET_TYPE_CHECKBOX));

            SliderFloatWidget cameraSpeedProp;
            cameraSpeedProp.pData = &gAppSettings.cameraWalkingSpeed;
            cameraSpeedProp.mMin = 0.0f;
            cameraSpeedProp.mMax = 3.0f;
            luaRegisterWidget(
                uiAddComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraSpeedProp, WIDGET_TYPE_SLIDER_FLOAT));

            /************************************************************************/
            // Light Settings
            /************************************************************************/
            // offset max angle for sun control so the light won't bleed with
            // small glancing angles, i.e., when lightDir is almost parallel to the plane

            SliderFloat2Widget sunX;
            sunX.pData = &gAppSettings.mSunControl;
            sunX.mMin = float2(-PI);
            sunX.mMax = float2(PI);
            sunX.mStep = float2(0.001f);
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Sun Control", &sunX, WIDGET_TYPE_SLIDER_FLOAT2));

            SliderFloat4Widget lightColorUI;
            lightColorUI.pData = &gAppSettings.mLightColor;
            lightColorUI.mMin = float4(0.0f);
            lightColorUI.mMax = float4(30.0f);
            lightColorUI.mStep = float4(0.01f);
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Light Color & Intensity", &lightColorUI, WIDGET_TYPE_SLIDER_FLOAT4));

            checkbox.pData = &gAppSettings.mEnableGodray;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable Godray", &checkbox, WIDGET_TYPE_CHECKBOX));

            SliderFloatWidget sliderFloat;
            sliderFloat.pData = &gAppSettings.mSunSize;
            sliderFloat.mMin = 1.0f;
            sliderFloat.mMax = 1000.0f;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Sun Size", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

            sliderFloat.pData = &gGodRayConstant.mScatterFactor;
            sliderFloat.mMin = 0.0f;
            sliderFloat.mMax = 1.0f;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray: Scatter Factor", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

            SliderUintWidget sliderUint;
            sliderUint.pData = &gAppSettings.mFilterRadius;
            sliderUint.mMin = 1u;
            sliderUint.mMax = 8u;
            sliderUint.mStep = 1u;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Gaussian Blur Kernel Size", &sliderUint,
                                WIDGET_TYPE_SLIDER_UINT);

            sliderFloat.pData = &gGaussianBlurSigma[0];
            sliderFloat.mMin = 0.1f;
            sliderFloat.mMax = 5.0f;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Gaussian Blur Sigma", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

            if (gAppSettings.mEnableGodray)
                uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, pGuiWindow);

            // SliderFloatWidget esm("Shadow Control", &gAppSettings.mEsmControl, 0, 200.0f);
            // pGuiWindow->AddWidget(esm);

            checkbox.pData = &gAppSettings.mRenderLocalLights;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable Random Point Lights", &checkbox, WIDGET_TYPE_CHECKBOX));
            /************************************************************************/
            // Display Settings
            /************************************************************************/
            static const char* displayColorRangeNames[] = { "RGB" };

            static const char* displaySignalRangeNames[] = { "Range Full", "Range Limited" };

            static const char* displayColorSpaceNames[] = { "ColorSpace Rec709", "ColorSpace Rec2020", "ColorSpace P3D65" };

            DropdownWidget ddColor;
            ddColor.pData = (uint32_t*)&gAppSettings.mDisplayColorRange;
            ddColor.pNames = displayColorRangeNames;
            ddColor.mCount = sizeof(displayColorRangeNames) / sizeof(displayColorRangeNames[0]);
            uiAddDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Color Range", &ddColor, WIDGET_TYPE_DROPDOWN);

            DropdownWidget ddRange;
            ddRange.pData = (uint32_t*)&gAppSettings.mDisplaySignalRange;
            ddRange.pNames = displaySignalRangeNames;
            ddRange.mCount = sizeof(displaySignalRangeNames) / sizeof(displaySignalRangeNames[0]);
            uiAddDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Signal Range", &ddRange, WIDGET_TYPE_DROPDOWN);

            DropdownWidget ddSpace;
            ddSpace.pData = (uint32_t*)&gAppSettings.mCurrentSwapChainColorSpace;
            ddSpace.pNames = displayColorSpaceNames;
            ddSpace.mCount = sizeof(displayColorSpaceNames) / sizeof(displayColorSpaceNames[0]);
            uiAddDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Color Space", &ddSpace, WIDGET_TYPE_DROPDOWN);

            /************************************************************************/
            // AO Settings
            /************************************************************************/
            checkbox.pData = &gAppSettings.mEnableAO;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable AO", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mVisualizeAO;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "Visualize AO", &checkbox, WIDGET_TYPE_CHECKBOX);

            sliderFloat.pData = &gAppSettings.mAOIntensity;
            sliderFloat.mMin = 0.0f;
            sliderFloat.mMax = 10.0f;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "AO Intensity", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

            SliderIntWidget sliderInt;
            sliderInt.pData = &gAppSettings.mAOQuality;
            sliderInt.mMin = 1;
            sliderInt.mMax = 4;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "AO Quality", &sliderInt, WIDGET_TYPE_SLIDER_INT);

            if (gAppSettings.mEnableAO)
                uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, pGuiWindow);

            sliderFloat.pData = &gAppSettings.LinearScale;
            sliderFloat.mMin = 80.0f;
            sliderFloat.mMax = 400.0f;
            uiAddDynamicWidgets(&gAppSettings.mLinearScale, "Linear Scale", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

            if (gAppSettings.mOutputMode != OutputMode::OUTPUT_MODE_SDR)
            {
                uiShowDynamicWidgets(&gAppSettings.mLinearScale, pGuiWindow);
            }

            if (!addSwapChain())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            addRenderTargets();

            SetupDebugTexturesWindow();
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
        waitQueueIdle(pComputeQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            removeRenderTargets();

            if (pDebugTexturesWindow)
            {
                uiRemoveComponent(pDebugTexturesWindow);
                pDebugTexturesWindow = NULL;
            }
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveComponent(pGuiWindow);
            uiRemoveDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
            uiRemoveDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO);
            uiRemoveDynamicWidgets(&gAppSettings.mLinearScale);
            uiRemoveDynamicWidgets(&gAppSettings.mDisplaySetting);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
#if defined(XBOX)
            esramResetAllocations(pRenderer->mDx.pESRAMManager);
#endif
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
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

        if (gWasColorSpace != gAppSettings.mCurrentSwapChainColorSpace || gWasDisplayColorRange != gAppSettings.mDisplayColorRange ||
            gWasDisplaySignalRange != gAppSettings.mDisplaySignalRange)
        {
            if (gWasColorSpace != gAppSettings.mCurrentSwapChainColorSpace && OUTPUT_MODE_SDR < gAppSettings.mOutputMode)
            {
                ReloadDesc reloadDescriptor;
                reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                requestReload(&reloadDescriptor);
            }

            gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
            gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
            gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;
        }

        // Change format
        if (gWasOutputMode != gAppSettings.mOutputMode)
        {
            ReloadDesc reloadDescriptor;
            reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
            requestReload(&reloadDescriptor);

            gWasOutputMode = gAppSettings.mOutputMode;
        }

        pCameraController->update(deltaTime);

        // Camera Walking Update

        if (gAppSettings.cameraWalking)
        {
            if (gTotalElpasedTime - (0.033333f * gAppSettings.cameraWalkingSpeed) <= gCameraWalkingTime)
            {
                gCameraWalkingTime = 0.0f;
            }

            gCameraWalkingTime += deltaTime * gAppSettings.cameraWalkingSpeed;

            uint  currentCameraFrame = (uint)(gCameraWalkingTime / 0.00833f);
            float remind = gCameraWalkingTime - (float)currentCameraFrame * 0.00833f;

            float3 newPos = v3ToF3(
                lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame]), f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1)]), remind));
            pCameraController->moveTo(f3Tov3(newPos) * SCENE_SCALE);

            float3 newLookat = v3ToF3(lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]),
                                           f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
            pCameraController->lookAt(f3Tov3(newLookat) * SCENE_SCALE);
        }

        updateDynamicUIElements();

        updateUniformData(gFrameCount % gDataBufferCount);

        if (gGaussianBlurSigma[1] != gGaussianBlurSigma[0])
        {
            gGaussianBlurSigma[1] = gGaussianBlurSigma[0];
            for (int i = 0; i < MAX_BLUR_KERNEL_SIZE; i++)
            {
                gBlurWeightsUniform.mBlurWeights[i] = gaussian((float)i, gGaussianBlurSigma[0]);
            }
        }
    }

    void Draw()
    {
        if (pSwapChain->mEnableVsync != (uint32_t)mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t          presentIndex = 0;
        uint32_t          frameIdx = gFrameCount % gDataBufferCount;
        static Semaphore* prevGraphicsSemaphore = NULL;
        static Semaphore* computeSemaphores[gDataBufferCount] = {};
        /************************************************************************/
        // Async compute pass
        /************************************************************************/
        bool              useDedicatedComputeQueue = gAppSettings.mAsyncCompute;
        if (useDedicatedComputeQueue)
        {
            GpuCmdRingElement computeElem = getNextGpuCmdRingElement(&gComputeCmdRing, true, 1);

            // check to see if we can use the cmd buffer
            FenceStatus fenceStatus;
            getFenceStatus(pRenderer, computeElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                waitForFences(pRenderer, 1, &computeElem.pFence);
            /************************************************************************/
            // Update uniform buffer to gpu
            /************************************************************************/
            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gVBViewUniformData, sizeof(gPerFrame[frameIdx].gVBViewUniformData));
            endUpdateResource(&update);

            updateRenderTargetInfo(frameIdx);
            /************************************************************************/
            // Triangle filtering async compute pass
            /************************************************************************/
            Cmd* computeCmd = computeElem.pCmds[0];

            resetCmdPool(pRenderer, computeElem.pCmdPool);
            beginCmd(computeCmd);
            cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);

            // we need to clear the VB here, since tiny tris get directly emitted from filtering
            clearVisibilityBuffer(computeCmd, gComputeProfileToken, frameIdx);

            TriangleFilteringPassDesc triangleFilteringDesc = {};
            triangleFilteringDesc.pFilterContainers = pFilterContainers;
            triangleFilteringDesc.mNumContainers = gMeshCount;

            triangleFilteringDesc.mNumCullingViewports = NUM_CULLING_VIEWPORTS;
            triangleFilteringDesc.pViewportObjectSpace = gPerFrame[frameIdx].gEyeObjectSpace;

            triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
            triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

            triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetPerFrame;
            triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetPersistent;
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetPerFrame;
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorSetTriangleFilteringPerBatch;
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerDraw = pDescriptorSetTriangleFilteringPerDrawCompute;
            triangleFilteringDesc.mFrameIndex = frameIdx;
            triangleFilteringDesc.mBuffersIndex = frameIdx;
            triangleFilteringDesc.mGpuProfileToken = gComputeProfileToken;
            triangleFilteringDesc.mClearThreadCount = CLEAR_THREAD_COUNT;
            triangleFilteringDesc.mTriangleFilteringBatchIndex =
                SRT_RES_IDX(TriangleFilteringCompSrtData, PerBatch, gTriangleFilteringBatchData);
            FilteringStats stats = cmdVisibilityBufferTriangleFilteringPass(pVisibilityBuffer, computeCmd, &triangleFilteringDesc);

            gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = stats.mGeomsetDrawCounts[GEOMSET_OPAQUE];
            gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHA_CUTOUT] = stats.mGeomsetDrawCounts[GEOMSET_ALPHA_CUTOUT];
            gPerFrame[frameIdx].gTotalDrawCount = stats.mTotalDrawCount;
            // triangleFilteringPass(computeCmd, gComputeProfileToken, frameIdx);

            cmdBeginGpuTimestampQuery(computeCmd, gComputeProfileToken, "Clear Light Clusters");
            clearLightClusters(computeCmd, frameIdx);
            cmdEndGpuTimestampQuery(computeCmd, gComputeProfileToken);

            if (gAppSettings.mRenderLocalLights)
            {
                /************************************************************************/
                // Synchronization
                /************************************************************************/
                // Update Light clusters on the GPU
                cmdBeginGpuTimestampQuery(computeCmd, gComputeProfileToken, "Compute Light Clusters");
                BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                               RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(computeCmd, 1, barriers, 0, NULL, 0, NULL);
                computeLightClusters(computeCmd, frameIdx);
                cmdEndGpuTimestampQuery(computeCmd, gComputeProfileToken);
            }

            cmdEndGpuFrameProfile(computeCmd, gComputeProfileToken);
            endCmd(computeCmd);

            FlushResourceUpdateDesc flushUpdateDesc = {};
            flushUpdateDesc.mNodeIndex = 0;
            flushResourceUpdates(&flushUpdateDesc);
            Semaphore* waitSemaphores[] = { flushUpdateDesc.pOutSubmittedSemaphore, gFrameCount > 1 ? prevGraphicsSemaphore : NULL };

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.mSignalSemaphoreCount = 1;
            submitDesc.mWaitSemaphoreCount = waitSemaphores[1] ? TF_ARRAY_COUNT(waitSemaphores) : 1;
            submitDesc.ppCmds = &computeCmd;
            submitDesc.ppSignalSemaphores = &computeElem.pSemaphore;
            submitDesc.ppWaitSemaphores = waitSemaphores;
            submitDesc.pSignalFence = computeElem.pFence;
            submitDesc.mSubmitDone = (gFrameCount < 1);
            queueSubmit(pComputeQueue, &submitDesc);

            computeSemaphores[frameIdx] = computeElem.pSemaphore;
            /************************************************************************/
            /************************************************************************/
        }

        /************************************************************************/
        // Draw Pass - Skip first frame since draw will always be one frame behind compute
        /************************************************************************/
        if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
        {
            frameIdx = gAppSettings.mAsyncCompute ? ((gFrameCount - 1) % gDataBufferCount) : frameIdx;

            GpuCmdRingElement graphicsElem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

            // check to see if we can use the cmd buffer
            FenceStatus fenceStatus;
            getFenceStatus(pRenderer, graphicsElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                waitForFences(pRenderer, 1, &graphicsElem.pFence);

            pScreenRenderTarget = pIntermediateRenderTarget;
            // pScreenRenderTarget = pSwapChain->ppRenderTargets[gPresentFrameIdx];
            /************************************************************************/
            // Update uniform buffer to gpu
            /************************************************************************/
            if (!useDedicatedComputeQueue)
            {
                BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
                beginUpdateResource(&update);
                memcpy(update.pMappedData, &gPerFrame[frameIdx].gVBViewUniformData, sizeof(gPerFrame[frameIdx].gVBViewUniformData));
                endUpdateResource(&update);

                updateRenderTargetInfo(frameIdx);
            }

            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gVBViewUniformData, sizeof(gPerFrame[frameIdx].gVBViewUniformData));
            endUpdateResource(&update);

            update = { pPerFrameUniformBuffers[frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameUniformData, sizeof(gPerFrame[frameIdx].gPerFrameUniformData));
            endUpdateResource(&update);

            // Update uniform buffers
            update = { pUniformBufferSky[frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gUniformDataSky, sizeof(gPerFrame[frameIdx].gUniformDataSky));
            endUpdateResource(&update);

            /************************************************************************/
            /************************************************************************/
            // Get command list to store rendering commands for this frame
            Cmd* graphicsCmd = graphicsElem.pCmds[0];
            // Submit all render commands for this frame
            resetCmdPool(pRenderer, graphicsElem.pCmdPool);
            beginCmd(graphicsCmd);

            cmdBeginGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);

            if (!gAppSettings.mAsyncCompute)
            {
                clearVisibilityBuffer(graphicsCmd, gGraphicsProfileToken, frameIdx);

                TriangleFilteringPassDesc triangleFilteringDesc = {};
                triangleFilteringDesc.pFilterContainers = pFilterContainers;
                triangleFilteringDesc.mNumContainers = gMeshCount;

                triangleFilteringDesc.mNumCullingViewports = NUM_CULLING_VIEWPORTS;
                triangleFilteringDesc.pViewportObjectSpace = gPerFrame[frameIdx].gEyeObjectSpace;

                triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
                triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

                triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetPerFrame;
                triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetPersistent;
                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetPerFrame;
                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorSetTriangleFilteringPerBatch;
                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerDraw = pDescriptorSetTriangleFilteringPerDraw;
                triangleFilteringDesc.mFrameIndex = frameIdx;
                triangleFilteringDesc.mBuffersIndex = frameIdx;
                triangleFilteringDesc.mGpuProfileToken = gGraphicsProfileToken;
                triangleFilteringDesc.mClearThreadCount = CLEAR_THREAD_COUNT;
                FilteringStats stats = cmdVisibilityBufferTriangleFilteringPass(pVisibilityBuffer, graphicsCmd, &triangleFilteringDesc);

                gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = stats.mGeomsetDrawCounts[GEOMSET_OPAQUE];
                gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHA_CUTOUT] = stats.mGeomsetDrawCounts[GEOMSET_ALPHA_CUTOUT];
                gPerFrame[frameIdx].gTotalDrawCount = stats.mTotalDrawCount;
            }

            if (!gAppSettings.mAsyncCompute)
            {
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Clear Light Clusters");
                clearLightClusters(graphicsCmd, frameIdx);
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
            }

            if ((!gAppSettings.mAsyncCompute) && gAppSettings.mRenderLocalLights)
            {
                // Update Light clusters on the GPU
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Compute Light Clusters");
                BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                               RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
                computeLightClusters(graphicsCmd, frameIdx);
                barriers[0] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
            }

            // Transition swapchain buffer to be used as a render target
            uint32_t            rtBarriersCount = 1;
            RenderTargetBarrier rtBarriers[] = {
                { pScreenRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            };

            const uint32_t maxNumBarriers = NUM_CULLING_VIEWPORTS * 2 + 4;
            uint32_t       barrierCount = 0;
            BufferBarrier  barriers2[maxNumBarriers] = {};
            barriers2[barrierCount++] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
            barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

            barriers2[barrierCount++] = { pVisibilityBuffer->ppBinBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                          RESOURCE_STATE_SHADER_RESOURCE };

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, rtBarriersCount, rtBarriers);

            drawScene(graphicsCmd, frameIdx);
            drawSkybox(graphicsCmd, frameIdx);

            barrierCount = 0;
            barriers2[barrierCount++] = { pLightClusters[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
            barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };

            barriers2[barrierCount++] = { pVisibilityBuffer->ppBinBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE,
                                          RESOURCE_STATE_UNORDERED_ACCESS };

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 0, NULL);

            if (gAppSettings.mEnableGodray)
            {
                drawGodray(graphicsCmd, frameIdx);
                blurGodRay(graphicsCmd, frameIdx);
                drawColorconversion(graphicsCmd);
            }

            // Get the current render target for this frame
            acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
            presentImage(graphicsCmd, frameIdx, pScreenRenderTarget, pSwapChain->ppRenderTargets[presentIndex]);

            cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "UI Pass");
            drawGUI(graphicsCmd, presentIndex);
            cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

            RenderTargetBarrier barrierPresent = { pSwapChain->ppRenderTargets[presentIndex], RESOURCE_STATE_RENDER_TARGET,
                                                   RESOURCE_STATE_PRESENT };
            cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 1, &barrierPresent);

            cmdEndGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);
            endCmd(graphicsCmd);

            // Submit all the work to the GPU and present
            FlushResourceUpdateDesc flushUpdateDesc = {};
            flushUpdateDesc.mNodeIndex = 0;
            flushResourceUpdates(&flushUpdateDesc);
            Semaphore* waitSemaphores[] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore, computeSemaphores[frameIdx] };
            Semaphore* signalSemaphores[] = { graphicsElem.pSemaphore, pPresentSemaphore };

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.mSignalSemaphoreCount = TF_ARRAY_COUNT(signalSemaphores);
            submitDesc.ppCmds = &graphicsCmd;
            submitDesc.ppSignalSemaphores = signalSemaphores;
            submitDesc.pSignalFence = graphicsElem.pFence;
            submitDesc.ppWaitSemaphores = waitSemaphores;
            submitDesc.mWaitSemaphoreCount = (useDedicatedComputeQueue && computeSemaphores[frameIdx]) ? TF_ARRAY_COUNT(waitSemaphores) : 2;
            queueSubmit(pGraphicsQueue, &submitDesc);

            prevGraphicsSemaphore = graphicsElem.pSemaphore;

            QueuePresentDesc presentDesc = {};
            presentDesc.mIndex = (uint8_t)presentIndex;
            presentDesc.mWaitSemaphoreCount = 1;
            presentDesc.ppWaitSemaphores = &pPresentSemaphore;
            presentDesc.pSwapChain = pSwapChain;
            presentDesc.mSubmitDone = true;
            queuePresent(pGraphicsQueue, &presentDesc);
            flipProfiler();
        }

        ++gFrameCount;
    }

    const char* GetName() { return "Visibility_Buffer2"; }

    bool addDescriptorSets()
    {
        // Persistent  set
        DescriptorSetDesc setDesc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPersistent);

        // per frame set
        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);

        // Triangle Filtering per batch
        setDesc = SRT_SET_DESC(TriangleFilteringCompSrtData, PerBatch, gDataBufferCount * FILTER_BATCH_COUNT, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFilteringPerBatch);

        // Triangle Filtering per draw
        setDesc = SRT_SET_DESC(TriangleFilteringCompSrtData, PerDraw, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFilteringPerDraw);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFilteringPerDrawCompute);

        // cluster lights set
        setDesc = SRT_SET_DESC(ClusterLightsSrtData, PerDraw, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClusterLights);

        // display per draw : one with godray, another without
        setDesc = SRT_SET_DESC(DisplaySrtData, PerDraw, 2, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDisplayPerDraw);

        // display per draw : blit Depth
        setDesc = SRT_SET_DESC(TriangleFilteringCompSrtData, PerBatch, 3 * gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetRenderTargetPerBatch);

        // God Ray Blur
        setDesc = SRT_SET_DESC(GodrayBlurCompSrtData, PerDraw, 2 * gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayBlurPassPerDraw);

        return true;
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFilteringPerDrawCompute);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFilteringPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetClusterLights);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayBlurPassPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetRenderTargetPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetDisplayPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFilteringPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPersistent);
    }

    void prepareDescriptorSets()
    {
        // persistent set
        Texture*       godRayTextures[] = { pRenderTargetGodRay[0]->pTexture, pRenderTargetGodRay[1]->pTexture };
        DescriptorData persistentSetParams[19] = {};
        persistentSetParams[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexPositionBuffer);
        persistentSetParams[0].ppBuffers = &pGeom->pVertexBuffers[0];
        persistentSetParams[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexTexCoordBuffer);
        persistentSetParams[1].ppBuffers = &pGeom->pVertexBuffers[1];
        persistentSetParams[2].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexNormalBuffer);
        persistentSetParams[2].ppBuffers = &pGeom->pVertexBuffers[2];
        persistentSetParams[3].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexDataBuffer);
        persistentSetParams[3].ppBuffers = &pGeom->pIndexBuffer;
        persistentSetParams[4].mIndex = SRT_RES_IDX(SrtData, Persistent, gMeshConstantsBuffer);
        persistentSetParams[4].ppBuffers = &pMeshConstantsBuffer;
        persistentSetParams[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gMaterialProps);
        persistentSetParams[5].ppBuffers = &pMaterialPropertyBuffer;
        persistentSetParams[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gdiffuseMaps);
        persistentSetParams[6].mCount = gMaterialCount;
        persistentSetParams[6].ppTextures = gDiffuseMapsStorage;
        persistentSetParams[7].mIndex = SRT_RES_IDX(SrtData, Persistent, gNormalMaps);
        persistentSetParams[7].mCount = gMaterialCount;
        persistentSetParams[7].ppTextures = gNormalMapsStorage;
        persistentSetParams[8].mIndex = SRT_RES_IDX(SrtData, Persistent, gSpecularMaps);
        persistentSetParams[8].mCount = gMaterialCount;
        persistentSetParams[8].ppTextures = gSpecularMapsStorage;
        persistentSetParams[9].mIndex = SRT_RES_IDX(SrtData, Persistent, gLights);
        persistentSetParams[9].ppBuffers = &pLightsBuffer;
        persistentSetParams[10].mIndex = SRT_RES_IDX(SrtData, Persistent, gBlurWeights);
        persistentSetParams[10].ppBuffers = &pBufferBlurWeights;
        persistentSetParams[11].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthTexture);
        persistentSetParams[11].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[12].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthTex);
        persistentSetParams[12].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[13].mIndex = SRT_RES_IDX(SrtData, Persistent, gShadowMap);
        persistentSetParams[13].ppTextures = &pRenderTargetShadow->pTexture;
        persistentSetParams[14].mIndex = SRT_RES_IDX(SrtData, Persistent, gMeshConstantsBuffer);
        persistentSetParams[14].ppBuffers = &pMeshConstantsBuffer;
        persistentSetParams[15].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexDataBuffer);
        persistentSetParams[15].ppBuffers = &pGeom->pIndexBuffer;
        persistentSetParams[16].mIndex = SRT_RES_IDX(SrtData, Persistent, gSkyboxTex);
        persistentSetParams[16].ppTextures = &pSkybox;
        persistentSetParams[17].mIndex = SRT_RES_IDX(SrtData, Persistent, gSceneTex);
        persistentSetParams[17].ppTextures = &pIntermediateRenderTarget->pTexture;
        persistentSetParams[18].mIndex = SRT_RES_IDX(SrtData, Persistent, gGodRayTex);
        persistentSetParams[18].ppTextures = &pRenderTargetGodRay[0]->pTexture;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPersistent, 19, persistentSetParams);

        // per frame set
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            const uint32_t perFrameParameterCount = 7;
            DescriptorData perFrameSetParams[perFrameParameterCount] = {};
            perFrameSetParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameConstants);
            perFrameSetParams[0].ppBuffers = &pPerFrameUniformBuffers[i];
            perFrameSetParams[1].mIndex = SRT_RES_IDX(SrtData, PerFrame, gVBViewConstants);
            perFrameSetParams[1].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
            perFrameSetParams[2].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClustersCount);
            perFrameSetParams[2].ppBuffers = &pLightClustersCount[i];
            perFrameSetParams[3].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClusters);
            perFrameSetParams[3].ppBuffers = &pLightClusters[i];
            perFrameSetParams[4].mIndex = SRT_RES_IDX(SrtData, PerFrame, gBinBuffer);
            perFrameSetParams[4].ppBuffers = &pVisibilityBuffer->ppBinBuffer[i];
            perFrameSetParams[5].mIndex = SRT_RES_IDX(SrtData, PerFrame, gVisibilityBuffer);
            perFrameSetParams[5].ppBuffers = &pVBDepthBuffer[i];
            perFrameSetParams[6].mIndex = SRT_RES_IDX(SrtData, PerFrame, gUniformCameraSky);
            perFrameSetParams[6].ppBuffers = &pUniformBufferSky[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, perFrameParameterCount, perFrameSetParams);
        }

        // triangle filtering per draw
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData triangleFilteringPerDrawParams[3] = {};
            triangleFilteringPerDrawParams[0].mIndex = SRT_RES_IDX(TriangleFilteringCompSrtData, PerDraw, gBinBufferRW);
            triangleFilteringPerDrawParams[0].ppBuffers = &pVisibilityBuffer->ppBinBuffer[i];
            triangleFilteringPerDrawParams[1].mIndex = SRT_RES_IDX(TriangleFilteringCompSrtData, PerDraw, gVisibilityBufferRW);
            triangleFilteringPerDrawParams[1].ppBuffers = &pVBDepthBuffer[i];
            triangleFilteringPerDrawParams[2].mIndex = SRT_RES_IDX(TriangleFilteringCompSrtData, PerDraw, gComputeVBViewConstants);
            triangleFilteringPerDrawParams[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFilteringPerDraw, 3, triangleFilteringPerDrawParams);

            triangleFilteringPerDrawParams[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFilteringPerDrawCompute, 3, triangleFilteringPerDrawParams);
        }

        // cluster lights set
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData clusterLightsParams[2] = {};
            clusterLightsParams[0].mIndex = SRT_RES_IDX(ClusterLightsSrtData, PerDraw, gLightClustersCountRW);
            clusterLightsParams[0].ppBuffers = &pLightClustersCount[i];
            clusterLightsParams[1].mIndex = SRT_RES_IDX(ClusterLightsSrtData, PerDraw, gLightClustersRW);
            clusterLightsParams[1].ppBuffers = &pLightClusters[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetClusterLights, 2, clusterLightsParams);
        }

        // blit depth
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData params[1] = {};
                params[0].mIndex = SRT_RES_IDX(TriangleFilteringCompSrtData, PerBatch, gRenderTargetInfo);
                params[0].ppBuffers = &pRenderTargetInfoConstantsBuffers[i][0];
                updateDescriptorSet(pRenderer, i * 3, pDescriptorSetRenderTargetPerBatch, 1, params);
                params[0].ppBuffers = &pRenderTargetInfoConstantsBuffers[i][1];
                updateDescriptorSet(pRenderer, i * 3 + 1, pDescriptorSetRenderTargetPerBatch, 1, params);
                params[0].ppBuffers = &pRenderTargetInfoConstantsBuffers[i][2];
                updateDescriptorSet(pRenderer, i * 3 + 2, pDescriptorSetRenderTargetPerBatch, 1, params);
            }
        }

        // God Ray Blur
        {
            DescriptorData params[2] = {};
            params[0].mIndex = SRT_RES_IDX(GodrayBlurCompSrtData, PerDraw, gGodrayTexturesRW);
            params[0].ppTextures = godRayTextures;
            params[0].mCount = 2;

            params[1].mIndex = SRT_RES_IDX(GodrayBlurCompSrtData, PerDraw, gBlurParams);
            for (uint32_t i = 0; i < gDataBufferCount; i++)
            {
                params[1].ppBuffers = &pGodRayBlurBuffer[i][0];
                params[1].mCount = 1;
                updateDescriptorSet(pRenderer, i * 2, pDescriptorSetGodRayBlurPassPerDraw, 2, params);
                params[1].ppBuffers = &pGodRayBlurBuffer[i][1];
                params[1].mCount = 1;
                updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSetGodRayBlurPassPerDraw, 2, params);
            }
        }
        // Present
        {
            DescriptorData params[1] = {};
            params[0].mIndex = SRT_RES_IDX(DisplaySrtData, PerDraw, gDisplaytexture);
            params[0].ppTextures = &pIntermediateRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetDisplayPerDraw, 1, params);
            params[0].ppTextures = &pCurveConversionRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, 1, pDescriptorSetDisplayPerDraw, 1, params);
        }
    }
    /************************************************************************/
    // Add render targets
    /************************************************************************/
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
        swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;

        TinyImageFormat hdrFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_P2020);
        const bool      wantsHDR = OUTPUT_MODE_P2020 == gAppSettings.mOutputMode;
        const bool      supportsHDR = TinyImageFormat_UNDEFINED != hdrFormat;
        if (pRenderer->pGpu->mHDRSupported)
        {
            strcpy(pOutputSupportsHDRWidget->mLabel, supportsHDR ? "Current Output Supports HDR" : "Current Output Does Not Support HDR");
        }

        if (wantsHDR)
        {
            if (supportsHDR)
            {
                swapChainDesc.mColorFormat = hdrFormat;
                swapChainDesc.mColorSpace = COLOR_SPACE_P2020;
            }
            else
            {
                errorMessagePopup("Error",
                                  "Could not create hdr swapchain, please use a HDR-capable display and enable HDR in OS settings.",
                                  &pWindow->handle, NULL);
            }
        }

        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
        return pSwapChain != NULL;
    }

    void addRenderTargets()
    {
        const uint32_t   width = gSceneRes.mWidth;
        const uint32_t   height = gSceneRes.mHeight;
        /************************************************************************/
        /************************************************************************/
        const ClearValue depthStencilClear = { { 0.0f, 0 } };
        // Used for ESM render target shadow
        const ClearValue lessEqualDepthStencilClear = { { 1.f, 0 } };
        ClearValue       optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

        /************************************************************************/
        // ESRAM layout
        /************************************************************************/
        // alloc0: Depth Buffer RT  |  Shared Depth Buffer  |  VB RT
        // alloc1:                  |  Intermediate  | ...

        /************************************************************************/
        // Main depth buffer
        /************************************************************************/
        // Add depth buffer
        BEGINALLOCATION("Depth/SharedDepth/VBRT", 0u);
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = depthStencilClear;
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        depthRT.mHeight = height;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        depthRT.mWidth = width;
        depthRT.pName = "Depth Buffer RT";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

#if defined(XBOX)
        const uint32_t depthAllocationOffset = ALLOCATIONOFFSET();
#endif

        // Here we create the depth buffer filled by the software rasterizer.
        // The depth buffer is filled both for shadow and visibility buffer.
        // We need a UINT depth buffer, as it's not possible to write to a depth texture (D32_SFLOAT or others) from a compute shader.
        // Metal 2 does not support AtomicMin2D or AtomicMax2D on image/texture resources. Thus, we fallback to a regular buffer for metal.
        // const uint32_t depthWidth = max(width, gShadowMapSize);
        // const uint32_t depthHeight = max(height, gShadowMapSize);
        uint32_t       vbSize = (gShadowMapSize * gShadowMapSize) + (width * height);
        BufferLoadDesc bufferDesc = {};
        bufferDesc.mDesc.mSize = vbSize * sizeof(uint64_t);
        bufferDesc.mDesc.mFirstElement = 0;
        bufferDesc.mDesc.mElementCount = vbSize;
        bufferDesc.mDesc.mStructStride = sizeof(uint64_t);
        bufferDesc.mDesc.pName = "Packed Depth Visibility Buffer - 0";
        bufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        bufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        bufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        bufferDesc.pData = NULL;
        bufferDesc.ppBuffer = &pVBDepthBuffer[0];
        addResource(&bufferDesc, NULL);
        bufferDesc.mDesc.pName = "Packed Depth Visibility Buffer - 1";
        bufferDesc.ppBuffer = &pVBDepthBuffer[1];
        addResource(&bufferDesc, NULL);

        /************************************************************************/
        // Shadow pass render target
        /************************************************************************/
        RenderTargetDesc shadowRTDesc = {};
        shadowRTDesc.mArraySize = 1;
        shadowRTDesc.mClearValue = lessEqualDepthStencilClear;
        shadowRTDesc.mDepth = 1;
        shadowRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        shadowRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
        shadowRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        shadowRTDesc.mWidth = gShadowMapSize;
        shadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
        shadowRTDesc.mSampleQuality = 0;
        // shadowRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        shadowRTDesc.mHeight = gShadowMapSize;
        shadowRTDesc.pName = "Shadow Map RT";
        addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadow);
        /************************************************************************/
        // Visibility buffer pass render target
        /************************************************************************/
        RenderTargetDesc vbRTDesc = {};
        vbRTDesc.mArraySize = 1;
        vbRTDesc.mClearValue = optimizedColorClearWhite;
        vbRTDesc.mDepth = 1;
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        vbRTDesc.mHeight = height;
        vbRTDesc.mSampleCount = SAMPLE_COUNT_1;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        vbRTDesc.mWidth = width;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
        ENDALLOCATION("Depth / SharedDepth / VBRT");
        /************************************************************************/
        // Intermediate render target
        /************************************************************************/
        BEGINALLOCATION("Intermediate", depthAllocationOffset);
        RenderTargetDesc postProcRTDesc = {};
        postProcRTDesc.mArraySize = 1;
        postProcRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        postProcRTDesc.mDepth = 1;
        postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        postProcRTDesc.mFormat = pSwapChain->mFormat;
        postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        postProcRTDesc.mHeight = gSceneRes.mHeight;
        postProcRTDesc.mWidth = gSceneRes.mWidth;
        postProcRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        postProcRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        postProcRTDesc.pName = "pIntermediateRenderTarget";
        addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

        /************************************************************************/
        // GodRay render target
        /************************************************************************/
        TinyImageFormat  GRRTFormat = pRenderer->pGpu->mFormatCaps[TinyImageFormat_B10G11R11_UFLOAT] & (FORMAT_CAP_READ_WRITE)
                                          ? TinyImageFormat_B10G11R11_UFLOAT
                                          : TinyImageFormat_R8G8B8A8_UNORM;
        RenderTargetDesc GRRTDesc = {};
        GRRTDesc.mArraySize = 1;
        GRRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        GRRTDesc.mDepth = 1;
        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mHeight = gSceneRes.mHeight / gGodrayScale;
        GRRTDesc.mWidth = gSceneRes.mWidth / gGodrayScale;
        GRRTDesc.mFormat = GRRTFormat;
        GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        GRRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        GRRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        GRRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;

        GRRTDesc.pName = "GodRay RT A";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[0]);
        GRRTDesc.pName = "GodRay RT B";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[1]);
        ENDALLOCATION("Intermediate");
        /************************************************************************/
        // Color Conversion render target
        /************************************************************************/
        BEGINALLOCATION("CurveConversion", 0u);
        RenderTargetDesc postCurveConversionRTDesc = {};
        postCurveConversionRTDesc.mArraySize = 1;
        postCurveConversionRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        postCurveConversionRTDesc.mDepth = 1;
        postCurveConversionRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        postCurveConversionRTDesc.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        postCurveConversionRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        postCurveConversionRTDesc.mHeight = height;
        postCurveConversionRTDesc.mWidth = width;
        postCurveConversionRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        postCurveConversionRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        postCurveConversionRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        postCurveConversionRTDesc.pName = "pCurveConversionRenderTarget";
        addRenderTarget(pRenderer, &postCurveConversionRTDesc, &pCurveConversionRenderTarget);
        ENDALLOCATION("CurveConversion");
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pCurveConversionRenderTarget);

        removeRenderTarget(pRenderer, pRenderTargetGodRay[0]);
        removeRenderTarget(pRenderer, pRenderTargetGodRay[1]);

        removeRenderTarget(pRenderer, pIntermediateRenderTarget);
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeResource(pVBDepthBuffer[0]);
        removeResource(pVBDepthBuffer[1]);
        removeRenderTarget(pRenderer, pRenderTargetVBPass);
        removeRenderTarget(pRenderer, pRenderTargetShadow);
    }
    /************************************************************************/
    // Load all the shaders needed for the demo
    /************************************************************************/
    void addShaders()
    {
        ShaderLoadDesc vbShade[2] = {};
        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};
        ShaderLoadDesc vbDepthRasterize = {};
        ShaderLoadDesc clearRenderTarget = {};
        ShaderLoadDesc blitDepth = {};

        const char* visibilityBufferShadeShaders[] = { "visibilityBuffer_shade_SAMPLE_1.frag", "visibilityBuffer_shade_SAMPLE_1_AO.frag" };

        for (uint32_t j = 0; j < 2; ++j)
        {
            uint32_t index = j;
            vbShade[index].mVert.pFileName = "visibilityBuffer_shade.vert";
            vbShade[index].mFrag.pFileName = visibilityBufferShadeShaders[index];
        }

        // Triangle culling compute shader
        triangleCulling.mComp.pFileName = "triangle_filtering.comp";
        if (gAppSettings.mLargeBinRasterGroups)
        {
            triangleCulling.mComp.pFileName = "triangle_filtering_LG.comp";
        }
        // Clear buffers compute shader
        clearBuffer.mComp.pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mComp.pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mComp.pFileName = "cluster_lights.comp";

        vbDepthRasterize.mComp.pFileName = "bin_rasterizer.comp";
        clearRenderTarget.mComp.pFileName = "clear_render_target.comp";

        blitDepth.mVert.pFileName = "visibilityBuffer_shade.vert";
        blitDepth.mFrag.pFileName = "visibilityBuffer_blitDepth.frag";

        ShaderLoadDesc godrayShaderDesc = {};
        godrayShaderDesc.mVert.pFileName = "display.vert";
        godrayShaderDesc.mFrag.pFileName = "godray.frag";
        addShader(pRenderer, &godrayShaderDesc, &pGodRayPass);

        ShaderLoadDesc godrayBlurShaderDesc = {};
        godrayBlurShaderDesc.mComp.pFileName = "godray_blur.comp";
        addShader(pRenderer, &godrayBlurShaderDesc, &pShaderGodRayBlurPass);

        ShaderLoadDesc CurveConversionShaderDesc = {};
        CurveConversionShaderDesc.mVert.pFileName = "display.vert";
        CurveConversionShaderDesc.mFrag.pFileName = "CurveConversion.frag";
        addShader(pRenderer, &CurveConversionShaderDesc, &pShaderCurveConversion);

        ShaderLoadDesc presentShaderDesc = {};
        presentShaderDesc.mVert.pFileName = "display.vert";
        presentShaderDesc.mFrag.pFileName = "display.frag";

        ShaderLoadDesc skyboxShaderDesc = {};
        skyboxShaderDesc.mVert.pFileName = "skybox.vert";
        skyboxShaderDesc.mFrag.pFileName = "skybox.frag";

        addShader(pRenderer, &clearRenderTarget, &pShaderClearRenderTarget);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &vbDepthRasterize, &pShaderVisibilityBufferDepthRaster);
        addShader(pRenderer, &blitDepth, &pShaderBlitDepth);

        for (uint32_t i = 0; i < 2; ++i)
            addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

        addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderClearRenderTarget);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderVisibilityBufferDepthRaster);
        removeShader(pRenderer, pShaderBlitDepth);
        for (uint32_t i = 0; i < 2; ++i)
            removeShader(pRenderer, pShaderVisibilityBufferShade[i]);

        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);

        removeShader(pRenderer, pGodRayPass);
        removeShader(pRenderer, pShaderGodRayBlurPass);

        removeShader(pRenderer, pShaderSkybox);
        removeShader(pRenderer, pShaderCurveConversion);
        removeShader(pRenderer, pShaderPresentPass);
    }

    void addPipelines()
    {
        /************************************************************************/
        // Setup compute pipelines for triangle filtering
        /************************************************************************/
        PipelineDesc pipelineDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, Persistent),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerFrame),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerBatch),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerDraw));
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& compPipelineSettings = pipelineDesc.mComputeDesc;
        compPipelineSettings.pShaderProgram = pShaderClearBuffers;
        pipelineDesc.pName = "Clear Filtering Buffers";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

        // Create the compute pipeline for GPU triangle filtering
        pipelineDesc.pName = "Triangle Filtering";
        compPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

        // Setup the visibility buffer depth software rasterizer
        pipelineDesc.pName = "Visibility Buffer - Fill Depth";
        compPipelineSettings.pShaderProgram = pShaderVisibilityBufferDepthRaster;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferDepthRaster);

        // Setup the clear big triangles buffer
        pipelineDesc.pName = "Clear Render Target";
        compPipelineSettings.pShaderProgram = pShaderClearRenderTarget;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearRenderTarget);

        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(ClusterLightsSrtData, Persistent),
                             SRT_LAYOUT_DESC(ClusterLightsSrtData, PerFrame), NULL, SRT_LAYOUT_DESC(ClusterLightsSrtData, PerDraw));
        // Setup the clearing light clusters pipeline
        pipelineDesc.pName = "Clear Light Clusters";
        compPipelineSettings.pShaderProgram = pShaderClearLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // Setup the compute the light clusters pipeline
        pipelineDesc.pName = "Cluster Lights";
        compPipelineSettings.pShaderProgram = pShaderClusterLights;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);

        // God Ray Blur Pass
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(GodrayBlurCompSrtData, Persistent),
                             SRT_LAYOUT_DESC(GodrayBlurCompSrtData, PerFrame), NULL, SRT_LAYOUT_DESC(GodrayBlurCompSrtData, PerDraw));

        pipelineDesc.pName = "God Ray Blur";
        compPipelineSettings.pShaderProgram = pShaderGodRayBlurPass;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayBlurPass);

        /************************************************************************/
        /************************************************************************/
        DepthStateDesc depthStateDisableDesc = {};

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };

        BlendStateDesc blendStateSkyBoxDesc = {};
        blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
        blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;

        blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
        blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;

        blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
        blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;

        blendStateSkyBoxDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateSkyBoxDesc.mIndependentBlend = false;

        // Setup pipeline settings
        pipelineDesc = {};
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        /************************************************************************/
        // Setup the resources needed for the Visibility Buffer Shade Pipeline
        /************************************************************************/
        // Create pipeline
        // Note: the vertex layout is set to null because the positions of the fullscreen triangle are being calculated automatically
        // in the vertex shader using each vertex_id.
        GraphicsPipelineDesc& vbShadePipelineSettings = pipelineDesc.mGraphicsDesc;
        vbShadePipelineSettings = { 0 };
        vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbShadePipelineSettings.mRenderTargetCount = 1;
        vbShadePipelineSettings.pDepthState = &depthStateDisableDesc;
        vbShadePipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        for (uint32_t i = 0; i < 2; ++i)
        {
            vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[i];
            vbShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            vbShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
            vbShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

#if defined(XBOX)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            // edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_PIXEL_SHADER_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader =
                !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

            pipelineDesc.pPipelineExtensions = edescs;
            pipelineDesc.mExtensionCount = sizeof(edescs) / sizeof(edescs[0]);
#endif
            pipelineDesc.pName = "VB Shade";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferShadeSrgb[i]);

            pipelineDesc.mExtensionCount = 0;
        }
        /************************************************************************/
        // Setup blit depth pipeline
        /************************************************************************/
        DepthStateDesc depthStateWriteOnlyDesc = {};
        depthStateWriteOnlyDesc.mDepthTest = true;
        depthStateWriteOnlyDesc.mDepthWrite = true;
        depthStateWriteOnlyDesc.mDepthFunc = CMP_ALWAYS;

        pipelineDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, Persistent),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerFrame),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerBatch),
                             SRT_LAYOUT_DESC(TriangleFilteringCompSrtData, PerDraw));
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& blitDepthPipelineSettings = pipelineDesc.mGraphicsDesc;
        blitDepthPipelineSettings = { 0 };
        blitDepthPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        blitDepthPipelineSettings.pDepthState = &depthStateWriteOnlyDesc;
        blitDepthPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        blitDepthPipelineSettings.mSampleCount = pDepthBuffer->mSampleCount;
        blitDepthPipelineSettings.mSampleQuality = pDepthBuffer->mSampleQuality;
        blitDepthPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        blitDepthPipelineSettings.pShaderProgram = pShaderBlitDepth;
        pipelineDesc.pName = "Blit Depth";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineBlitDepth);

        /************************************************************************/
        // Setup Skybox pipeline
        /************************************************************************/

        // layout and pipeline for skybox draw
        VertexLayout vertexLayoutSkybox = {};
        vertexLayoutSkybox.mBindingCount = 1;
        vertexLayoutSkybox.mAttribCount = 1;
        vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayoutSkybox.mAttribs[0].mBinding = 0;
        vertexLayoutSkybox.mAttribs[0].mLocation = 0;
        vertexLayoutSkybox.mAttribs[0].mOffset = 0;
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& pipelineSettings = pipelineDesc.mGraphicsDesc;
        pipelineSettings = { 0 };
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = NULL;

        pipelineSettings.pBlendState = &blendStateSkyBoxDesc;

        pipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        // pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelineSettings.pShaderProgram = pShaderSkybox;
        pipelineSettings.pVertexLayout = &vertexLayoutSkybox;
        pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineDesc.pName = "Skybox";
        addPipeline(pRenderer, &pipelineDesc, &pSkyboxPipeline);

        /************************************************************************/
        // Setup Godray pipeline
        /************************************************************************/

        GraphicsPipelineDesc& pipelineSettingsGodRay = pipelineDesc.mGraphicsDesc;
        pipelineSettingsGodRay = { 0 };
        pipelineSettingsGodRay.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsGodRay.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsGodRay.mRenderTargetCount = 1;
        pipelineSettingsGodRay.pColorFormats = &pRenderTargetGodRay[0]->mFormat;
        pipelineSettingsGodRay.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettingsGodRay.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsGodRay.pShaderProgram = pGodRayPass;
        pipelineDesc.pName = "God Ray";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayPass);

        /************************************************************************/
        // Setup Curve Conversion pipeline
        /************************************************************************/

        GraphicsPipelineDesc& pipelineSettingsCurveConversion = pipelineDesc.mGraphicsDesc;
        pipelineSettingsCurveConversion = { 0 };
        pipelineSettingsCurveConversion.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;

        pipelineSettingsCurveConversion.pRasterizerState = &rasterizerStateCullNoneDesc;

        pipelineSettingsCurveConversion.mRenderTargetCount = 1;
        pipelineSettingsCurveConversion.pColorFormats = &pCurveConversionRenderTarget->mFormat;
        pipelineSettingsCurveConversion.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettingsCurveConversion.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsCurveConversion.pShaderProgram = pShaderCurveConversion;
        pipelineDesc.pName = "Curve Conversion";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineCurveConversionPass);

        /************************************************************************/
        // Setup Present pipeline
        /************************************************************************/

        GraphicsPipelineDesc& pipelineSettingsFinalPass = pipelineDesc.mGraphicsDesc;
        pipelineSettingsFinalPass = { 0 };
        pipelineSettingsFinalPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsFinalPass.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsFinalPass.mRenderTargetCount = 1;
        pipelineSettingsFinalPass.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(DisplaySrtData, Persistent), SRT_LAYOUT_DESC(DisplaySrtData, PerFrame), NULL,
                             SRT_LAYOUT_DESC(DisplaySrtData, PerDraw));
        pipelineDesc.pName = "Composite";
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePresentPass);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineGodRayPass);
        removePipeline(pRenderer, pPipelineGodRayBlurPass);

        removePipeline(pRenderer, pPipelineCurveConversionPass);
        removePipeline(pRenderer, pPipelinePresentPass);

        for (uint32_t i = 0; i < 2; ++i)
        {
            removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
        }

        removePipeline(pRenderer, pSkyboxPipeline);

        // Destroy triangle filtering pipelines
        removePipeline(pRenderer, pPipelineClusterLights);
        removePipeline(pRenderer, pPipelineClearLightClusters);
        removePipeline(pRenderer, pPipelineTriangleFiltering);
        removePipeline(pRenderer, pPipelineClearBuffers);

        removePipeline(pRenderer, pPipelineVisibilityBufferDepthRaster);
        removePipeline(pRenderer, pPipelineBlitDepth);
        removePipeline(pRenderer, pPipelineClearRenderTarget);
    }

    // This method sets the contents of the buffers to indicate the rendering pass that
    // the whole scene triangles must be rendered (no cluster / triangle filtering).
    // This is useful for testing purposes to compare visual / performance results.
    void addTriangleFilteringBuffers(Scene* pScene)
    {
        /************************************************************************/
        // Material props
        /************************************************************************/
        uint32_t* alphaTestMaterials = (uint32_t*)tf_malloc(gMaterialCount * sizeof(uint32_t));
        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            alphaTestMaterials[i] = (pScene->materialFlags[i] & MATERIAL_FLAG_ALPHA_TESTED) ? 1 : 0;
        }

        BufferLoadDesc materialPropDesc = {};
        materialPropDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        materialPropDesc.mDesc.mElementCount = gMaterialCount;
        materialPropDesc.mDesc.mStructStride = sizeof(uint32_t);
        materialPropDesc.mDesc.mSize = materialPropDesc.mDesc.mElementCount * materialPropDesc.mDesc.mStructStride;
        materialPropDesc.pData = alphaTestMaterials;
        materialPropDesc.ppBuffer = &pMaterialPropertyBuffer;
        materialPropDesc.mDesc.pName = "Material Prop Desc";
        addResource(&materialPropDesc, NULL);

        removeResource(pScene->geomData);
        pScene->geomData = nullptr;

        /************************************************************************/
        // Mesh constants
        /************************************************************************/
        // create mesh constants buffer
        MeshConstants* meshConstants = (MeshConstants*)tf_malloc(gMeshCount * sizeof(MeshConstants));

        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            meshConstants[i].indexOffset = pScene->geom->pDrawArgs[i].mStartIndex;
            meshConstants[i].vertexOffset = pScene->geom->pDrawArgs[i].mVertexOffset;
            meshConstants[i].materialID = i;
            meshConstants[i].twoSided = (pScene->materialFlags[i] & MATERIAL_FLAG_TWO_SIDED) ? 1 : 0;
        }

        BufferLoadDesc meshConstantDesc = {};
        meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        meshConstantDesc.mDesc.mElementCount = gMeshCount;
        meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
        meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
        meshConstantDesc.pData = meshConstants;
        meshConstantDesc.ppBuffer = &pMeshConstantsBuffer;
        meshConstantDesc.mDesc.pName = "Mesh Constant Desc";

        addResource(&meshConstantDesc, NULL);
        /************************************************************************/
        // Per Frame Constant Buffers
        /************************************************************************/
        uint64_t       size = sizeof(PerFrameConstantsData);
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mSize = size;
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        ubDesc.mDesc.pName = "Uniform Buffer Desc";

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pPerFrameUniformBuffers[i];
            addResource(&ubDesc, NULL);
        }

        ubDesc.mDesc.mSize = sizeof(VBViewConstantsData);
        ubDesc.mDesc.pName = "gVBViewConstants Uniform Buffer Desc";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
            addResource(&ubDesc, NULL);
            ubDesc.ppBuffer = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
            addResource(&ubDesc, NULL);
        }
        /************************************************************************/
        // Lighting buffers
        /************************************************************************/
        // Setup lights uniform buffer
        // It should cover the courtyard area with tree...
        for (uint32_t i = 0; i < LIGHT_COUNT; i++)
        {
            gLightData[i].position.setX(randomFloat(-90.0f, 55.0f));
            gLightData[i].position.setY(40.0f);
            gLightData[i].position.setZ(randomFloat(-15.0f, 120.0f));
            gLightData[i].color.setX(randomFloat01());
            gLightData[i].color.setY(randomFloat01());
            gLightData[i].color.setZ(randomFloat01());
        }
        BufferLoadDesc batchUb = {};
        batchUb.mDesc.mSize = sizeof(gLightData);
        batchUb.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        batchUb.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        batchUb.mDesc.mFirstElement = 0;
        batchUb.mDesc.mElementCount = LIGHT_COUNT;
        batchUb.mDesc.mStructStride = sizeof(LightData);
        batchUb.pData = gLightData;
        batchUb.ppBuffer = &pLightsBuffer;
        batchUb.mDesc.pName = "Lights Desc";
        addResource(&batchUb, NULL);

        // Setup lights cluster data
        uint32_t       lightClustersInitData[LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT] = {};
        BufferLoadDesc lightClustersCountBufferDesc = {};
        lightClustersCountBufferDesc.mDesc.mSize = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
        lightClustersCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
        lightClustersCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        lightClustersCountBufferDesc.mDesc.mFirstElement = 0;
        lightClustersCountBufferDesc.mDesc.mElementCount = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
        lightClustersCountBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
        lightClustersCountBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        lightClustersCountBufferDesc.pData = lightClustersInitData;
        lightClustersCountBufferDesc.mDesc.pName = "Light Cluster Count Buffer Desc";
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            lightClustersCountBufferDesc.ppBuffer = &pLightClustersCount[frameIdx];
            addResource(&lightClustersCountBufferDesc, NULL);
        }

        BufferLoadDesc lightClustersDataBufferDesc = {};
        lightClustersDataBufferDesc.mDesc.mSize = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
        lightClustersDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
        lightClustersDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        lightClustersDataBufferDesc.mDesc.mFirstElement = 0;
        lightClustersDataBufferDesc.mDesc.mElementCount = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
        lightClustersDataBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
        lightClustersDataBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        lightClustersDataBufferDesc.pData = NULL;
        lightClustersDataBufferDesc.mDesc.pName = "Light Cluster Data Buffer Desc";
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            lightClustersDataBufferDesc.ppBuffer = &pLightClusters[frameIdx];
            addResource(&lightClustersDataBufferDesc, NULL);
        }

        BufferLoadDesc skyDataBufferDesc = {};
        skyDataBufferDesc.mDesc.mSize = sizeof(UniformDataSkybox);
        skyDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        skyDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        skyDataBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        skyDataBufferDesc.pData = NULL;
        skyDataBufferDesc.mDesc.pName = "Sky Uniforms";
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            skyDataBufferDesc.ppBuffer = &pUniformBufferSky[frameIdx];
            addResource(&skyDataBufferDesc, NULL);
        }
        waitForAllResourceLoads();

        tf_free(alphaTestMaterials);
        tf_free(meshConstants);
    }

    void removeTriangleFilteringBuffers()
    {
        /************************************************************************/
        // Material props
        /************************************************************************/
        removeResource(pMaterialPropertyBuffer);

        /************************************************************************/
        // Mesh constants
        /************************************************************************/
        removeResource(pMeshConstantsBuffer);

        /************************************************************************/
        // Per Frame Constant Buffers
        /************************************************************************/
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPerFrameUniformBuffers[i]);
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i]);
            removeResource(pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i]);
        }
        /************************************************************************/
        // Lighting buffers
        /************************************************************************/
        removeResource(pLightsBuffer);
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            removeResource(pLightClustersCount[frameIdx]);
            removeResource(pLightClusters[frameIdx]);
            removeResource(pUniformBufferSky[frameIdx]);
        }
        /************************************************************************/
        /************************************************************************/
    }

    /************************************************************************/
    // Scene update
    /************************************************************************/
    // Updates uniform data for the given frame index.
    // This includes transform matrices, render target resolution and global information about the scene.
    void updateUniformData(uint currentFrameIdx)
    {
        const uint32_t width = gSceneRes.mWidth;
        const uint32_t height = gSceneRes.mHeight;
        const float    aspectRatioInv = (float)height / width;
        const uint32_t frameIdx = currentFrameIdx;
        PerFrameData*  currentFrame = &gPerFrame[frameIdx];

        mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
        // mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
        mat4 cameraView = pCameraController->getViewMatrix().mCamera;
        mat4 cameraProj = mat4::perspectiveLH_ReverseZ(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

        // Compute light matrices
        vec3 lightSourcePos(50.0f, 000.0f, 450.0f);

        // directional light rotation & translation
        mat4 rotation = mat4::rotationXY(gAppSettings.mSunControl.x, gAppSettings.mSunControl.y);
        vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));
        lightSourcePos += -800.0f * normalize(lightDir.getXYZ());
        mat4 translation = mat4::translation(-lightSourcePos);

        mat4   lightModel = mat4::scale(vec3(SCENE_SCALE));
        mat4   lightView = rotation * translation;
        mat4   lightProj = mat4::orthographicLH_ReverseZ(-600, 600, -950, 350, gAppSettings.lightNearPlane, gAppSettings.lightFarPlane);
        float2 twoOverRes = { 2.0f / float(width), 2.0f / float(height) };

        /************************************************************************/
        // Lighting data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.camPos = v4ToF4(vec4(pCameraController->getViewPosition()));
        currentFrame->gPerFrameUniformData.lightDir = v4ToF4(vec4(lightDir));
        currentFrame->gPerFrameUniformData.twoOverRes = twoOverRes;
        currentFrame->gPerFrameUniformData.esmControl = gAppSettings.mEsmControl;
        currentFrame->gPerFrameUniformData.mGodRayScatterFactor = gGodRayConstant.mScatterFactor;
        /************************************************************************/
        // Matrix data
        /************************************************************************/
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].vp = lightProj * lightView;
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].view = lightView;
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].invVP = inverse(currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].vp);
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].projection = lightProj;
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].mvp =
            currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].vp * lightModel;
        currentFrame->gVBViewUniformData.transform[VIEW_SHADOW].cameraPlane = { gAppSettings.lightNearPlane, gAppSettings.lightFarPlane };

        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].vp = cameraProj * cameraView;
        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].view = cameraView;
        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].invVP = inverse(currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].vp);
        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].projection = cameraProj;
        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].mvp =
            currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].vp * cameraModel;
        currentFrame->gVBViewUniformData.transform[VIEW_CAMERA].cameraPlane = { gAppSettings.nearPlane, gAppSettings.farPlane };

        /************************************************************************/
        // Culling data
        /************************************************************************/
        currentFrame->gVBViewUniformData.cullingViewports[VIEW_SHADOW].sampleCount = 1;
        currentFrame->gVBViewUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)gShadowMapSize, (float)gShadowMapSize };

        currentFrame->gVBViewUniformData.cullingViewports[VIEW_CAMERA].sampleCount = SAMPLE_COUNT_1;
        currentFrame->gVBViewUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

        // Cache eye position in object space for cluster culling on the CPU
        currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
        currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
            (inverse(cameraView * cameraModel) * vec4(0, 0, 0, 1)).getXYZ(); // vec4(0,0,0,1) is the camera position in eye space
        /************************************************************************/
        // Shading data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.lightColor = gAppSettings.mLightColor;
        currentFrame->gPerFrameUniformData.outputMode = (uint)gAppSettings.mOutputMode;
        currentFrame->gPerFrameUniformData.CameraPlane = { gAppSettings.nearPlane, gAppSettings.farPlane };
        currentFrame->gPerFrameUniformData.aoQuality = gAppSettings.mEnableAO ? gAppSettings.mAOQuality : 0;
        currentFrame->gPerFrameUniformData.aoIntensity = gAppSettings.mAOIntensity;
        currentFrame->gPerFrameUniformData.frustumPlaneSizeNormalized = frustumPlaneSizeFovX(PI / 2.0f, (float)width / (float)height, 1.0f);
        currentFrame->gPerFrameUniformData.visualizeAo = gAppSettings.mVisualizeAO;
        currentFrame->gPerFrameUniformData.smallScaleRaster = gAppSettings.mSmallScaleRaster;
        currentFrame->gPerFrameUniformData.visualizeGeometry = gAppSettings.mVisualizeGeometry;
        currentFrame->gPerFrameUniformData.visualizeBinTriangleCount = gAppSettings.mVisualizeBinTriangleCount;
        currentFrame->gPerFrameUniformData.visualizeBinOccupancy = gAppSettings.mVisualizeBinOccupancy;
        currentFrame->gPerFrameUniformData.depthTexSize = { (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight };
        /************************************************************************/
        // Skybox
        /************************************************************************/
        cameraView.setTranslation(vec3(0));
        currentFrame->gUniformDataSky.mCamPos = pCameraController->getViewPosition();
        currentFrame->gUniformDataSky.mProjectView = cameraProj * cameraView;
        /************************************************************************/
        // Tonemap
        /************************************************************************/
        currentFrame->gPerFrameUniformData.mLinearScale = gAppSettings.LinearScale;
        currentFrame->gPerFrameUniformData.mOutputMode = gAppSettings.mOutputMode;

        /************************************************************************/
        // Render Target Info - Used in rasterization
        /************************************************************************/
        gShadowRenderTargetInfo.view = VIEW_SHADOW;
        gShadowRenderTargetInfo.width = gShadowMapSize;
        gShadowRenderTargetInfo.height = gShadowMapSize;

        gDepthRenderTargetInfo.view = VIEW_CAMERA;
        gDepthRenderTargetInfo.width = (int)pRenderTargetVBPass->mWidth;
        gDepthRenderTargetInfo.height = (int)pRenderTargetVBPass->mHeight;

        int size = gSceneRes.mWidth * gSceneRes.mHeight + gShadowMapSize * gShadowMapSize;
        gClearRenderTargetInfo = { 0U, size, 0 }; // dims
    }
    /************************************************************************/
    // UI
    /************************************************************************/
    void updateDynamicUIElements()
    {
        static bool wasAOEnabled = gAppSettings.mEnableAO;

        if (gAppSettings.mEnableAO != wasAOEnabled)
        {
            wasAOEnabled = gAppSettings.mEnableAO;
            if (wasAOEnabled)
            {
                uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, pGuiWindow);
            }
            else
            {
                uiHideDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, pGuiWindow);
            }
        }

        static bool wasGREnabled = gAppSettings.mEnableGodray;

        if (gAppSettings.mEnableGodray != wasGREnabled)
        {
            wasGREnabled = gAppSettings.mEnableGodray;
            if (wasGREnabled)
            {
                uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, pGuiWindow);
            }
            else
            {
                uiHideDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, pGuiWindow);
            }
        }

        // God Ray
        {
            static bool gPrevEnableGodRay = gAppSettings.mEnableGodray;
            if (gPrevEnableGodRay != gAppSettings.mEnableGodray)
            {
                gPrevEnableGodRay = gAppSettings.mEnableGodray;
                waitQueueIdle(pGraphicsQueue);
                waitQueueIdle(pComputeQueue);
                unloadFontSystem(RELOAD_TYPE_RENDERTARGET);
                unloadUserInterface(RELOAD_TYPE_RENDERTARGET);

                UserInterfaceLoadDesc uiLoad = {};
                uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
                uiLoad.mHeight = mSettings.mHeight;
                uiLoad.mWidth = mSettings.mWidth;
                uiLoad.mLoadType = RELOAD_TYPE_RENDERTARGET;
                loadUserInterface(&uiLoad);

                FontSystemLoadDesc fontLoad = {};
                fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
                fontLoad.mHeight = mSettings.mHeight;
                fontLoad.mWidth = mSettings.mWidth;
                fontLoad.mLoadType = RELOAD_TYPE_RENDERTARGET;
                loadFontSystem(&fontLoad);
            }
        }

        uiSetComponentActive(pDebugTexturesWindow, gAppSettings.mDrawDebugTargets);

        // Async compute
        {
            static bool gPrevAsyncCompute = gAppSettings.mAsyncCompute;
            if (gPrevAsyncCompute != gAppSettings.mAsyncCompute)
            {
                gPrevAsyncCompute = gAppSettings.mAsyncCompute;
                waitQueueIdle(pGraphicsQueue);
                waitQueueIdle(pComputeQueue);
                gFrameCount = 0;
            }
        }

        // AO
        gAppSettings.mVisualizeAO &= gAppSettings.mEnableAO;
    }
    /************************************************************************/
    // Rendering
    /************************************************************************/
    void clearVisibilityBuffer(Cmd* cmd, ProfileToken profileToken, uint32_t frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, profileToken, "Clear Visibility Buffer");
        cmdBindPipeline(cmd, pPipelineClearRenderTarget);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetTriangleFilteringPerDraw);
        cmdBindDescriptorSet(cmd, frameIdx * 3 + 2, pDescriptorSetRenderTargetPerBatch);
        cmdDispatch(cmd, gClearRenderTargetInfo.width / 256 + 1, 1, 1);
#ifdef METAL // is this necessary?
        cmdResourceBarrier(cmd, 1, nullptr, 0, nullptr, 0, nullptr);
#endif
        cmdEndGpuTimestampQuery(cmd, profileToken);
    }

    // Render the shadow mapping pass. This pass updates the shadow map texture.
    void drawShadowMapPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        UNREF_PARAM(pGpuProfiler);
        cmdBindPipeline(cmd, pPipelineVisibilityBufferDepthRaster);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetTriangleFilteringPerDraw);
        cmdBindDescriptorSet(cmd, frameIdx * 3, pDescriptorSetRenderTargetPerBatch);
        const uint ts = 128;
        cmdDispatch(cmd, (gShadowMapSize + ts - 1) / ts, (gShadowMapSize + ts - 1) / ts, 8);
#ifdef METAL
        // force a barrier to anchor the small triangle dispatch to the frame // TODO: check if still necessary
        cmdResourceBarrier(cmd, 1, NULL, 0, NULL, 0, NULL);
#endif
    }

    // Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
    // into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
    // to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
    // less memory bandwidth is used.
    void drawVisibilityBufferPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        UNREF_PARAM(pGpuProfiler);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferDepthRaster);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindDescriptorSet(cmd, frameIdx * 3 + 1, pDescriptorSetRenderTargetPerBatch);
        const uint32_t binRasterThreadsZCount = 64;
        cmdDispatch(cmd, (gDepthRenderTargetInfo.width + BIN_SIZE - 1) / BIN_SIZE,
                    (gDepthRenderTargetInfo.height + BIN_SIZE - 1) / BIN_SIZE, binRasterThreadsZCount);
    }

    void blitVisibilityBufferDepthPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t view, uint32_t frameIdx, RenderTarget* depthTarget)
    {
        UNREF_PARAM(pGpuProfiler);

        // cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Blit Depth Buffer");
        {
            RenderTargetBarrier rtBarriers[] = {
                { depthTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);
        }

        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { depthTarget, LOAD_ACTION_CLEAR };

        // Start render pass and apply load actions
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)depthTarget->mWidth, (float)depthTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, depthTarget->mWidth, depthTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineBlitDepth);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindDescriptorSet(cmd, frameIdx * 3 + (int)view, pDescriptorSetRenderTargetPerBatch);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        {
            RenderTargetBarrier rtBarriers[] = {
                { depthTarget, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);
        }
        // cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
    {
        RenderTarget* pDestinationRenderTarget = pScreenRenderTarget;

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pDestinationRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[gAppSettings.mEnableAO]);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    // Executes a compute shader to clear (reset) the the light clusters on the GPU
    void clearLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClearLightClusters);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLights);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdDispatch(cmd, 1, 1, 1);
    }

    // Executes a compute shader that computes the light clusters on the GPU
    void computeLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClusterLights);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLights);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdDispatch(cmd, LIGHT_COUNT, 1, 1);
    }

    // This is the main scene rendering function. It shows the different steps / rendering passes.
    void drawScene(Cmd* cmd, uint32_t frameIdx)
    {
        BufferBarrier bufferBarrier = { pVBDepthBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 1, &bufferBarrier, 0, NULL, 0, NULL);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Shadow Pass");
        drawShadowMapPass(cmd, gGraphicsProfileToken, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Filling Pass");
        drawVisibilityBufferPass(cmd, gGraphicsProfileToken, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        cmdResourceBarrier(cmd, 1, &bufferBarrier, 0, NULL, 0, NULL);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Blits");
        blitVisibilityBufferDepthPass(cmd, gGraphicsProfileToken, 0, frameIdx, pRenderTargetShadow);
        blitVisibilityBufferDepthPass(cmd, gGraphicsProfileToken, 1, frameIdx, pDepthBuffer);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Shading Pass");
        drawVisibilityBufferShade(cmd, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawSkybox(Cmd* cmd, int frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Draw Skybox");

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pScreenRenderTarget, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

        // Draw the skybox
        const uint32_t stride = sizeof(float) * 4;
        cmdBindPipeline(cmd, pSkyboxPipeline);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &stride, NULL);

        cmdDraw(cmd, 36, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawGodray(Cmd* cmd, uint frameIdx)
    {
        RenderTargetBarrier barrier[2] = {
            { pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            { pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
        };

        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barrier);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetGodRay[0], LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetGodRay[0]->mWidth, (float)pRenderTargetGodRay[0]->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetGodRay[0]->mWidth, pRenderTargetGodRay[0]->mHeight);

        cmdBindPipeline(cmd, pPipelineGodRayPass);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        RenderTargetBarrier barrier2[] = { { pRenderTargetGodRay[0], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier2);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void blurGodRay(Cmd* cmd, uint frameIdx)
    {
        UNREF_PARAM(frameIdx);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray Blur");

        BufferUpdateDesc bufferUpdate = { pBufferBlurWeights };
        beginUpdateResource(&bufferUpdate);
        memcpy(bufferUpdate.pMappedData, &gBlurWeightsUniform, sizeof(gBlurWeightsUniform));
        endUpdateResource(&bufferUpdate);

        RenderTargetBarrier renderTargetBarrier[2];
        renderTargetBarrier[0] = { pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        cmdBindPipeline(cmd, pPipelineGodRayBlurPass);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);

        const uint32_t threadGroupSizeX = pRenderTargetGodRay[0]->mWidth / 16 + 1;
        const uint32_t threadGroupSizeY = pRenderTargetGodRay[0]->mHeight / 16 + 1;

        // Horizontal Pass
        gGodRayBlurConstant.mBlurPassType = BLUR_PASS_TYPE_HORIZONTAL;
        gGodRayBlurConstant.mFilterRadius = gAppSettings.mFilterRadius;
        BufferUpdateDesc update = { pGodRayBlurBuffer[frameIdx][0] };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, &gGodRayBlurConstant, sizeof(gGodRayBlurConstant));
        endUpdateResource(&update);
        cmdBindDescriptorSet(cmd, frameIdx * 2, pDescriptorSetGodRayBlurPassPerDraw);
        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);
        renderTargetBarrier[0] = { pRenderTargetGodRay[0], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);
        // Vertical Pass
        gGodRayBlurConstant.mBlurPassType = BLUR_PASS_TYPE_VERTICAL;
        gGodRayBlurConstant.mFilterRadius = gAppSettings.mFilterRadius;
        update = { pGodRayBlurBuffer[frameIdx][1] };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, &gGodRayBlurConstant, sizeof(gGodRayBlurConstant));
        endUpdateResource(&update);
        cmdBindDescriptorSet(cmd, frameIdx * 2 + 1, pDescriptorSetGodRayBlurPassPerDraw);
        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);
        renderTargetBarrier[0] = { pRenderTargetGodRay[0], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawColorconversion(Cmd* cmd)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Curve Conversion");

        // Transfer our render target to a render target state
        RenderTargetBarrier barrierCurveConversion[] = { { pCurveConversionRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                           RESOURCE_STATE_RENDER_TARGET } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrierCurveConversion);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pCurveConversionRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);

        // CurveConversion
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pCurveConversionRenderTarget->mWidth, (float)pCurveConversionRenderTarget->mHeight, 0.0f,
                       1.0f);
        cmdSetScissor(cmd, 0, 0, pCurveConversionRenderTarget->mWidth, pCurveConversionRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineCurveConversionPass);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        pScreenRenderTarget = pCurveConversionRenderTarget;

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void presentImage(Cmd* const cmd, uint32_t frameIdx, RenderTarget* pSrc, RenderTarget* pDstCol)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Present Image");

        RenderTargetBarrier barrier[] = { { pSrc, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
                                          { pDstCol, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET } };

        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barrier);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pDstCol, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDstCol->mWidth, (float)pDstCol->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDstCol->mWidth, pDstCol->mHeight);

        cmdBindPipeline(cmd, pPipelinePresentPass);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetPersistent);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetPerFrame);
        cmdBindDescriptorSet(cmd, gAppSettings.mEnableGodray ? 1 : 0, pDescriptorSetDisplayPerDraw);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void updateRenderTargetInfo(uint32_t frameIdx)
    {
        // Update clear vb render target info (combined view pass)
        BufferUpdateDesc bufferUpdate = { pRenderTargetInfoConstantsBuffers[frameIdx][2] };
        beginUpdateResource(&bufferUpdate);
        memcpy(bufferUpdate.pMappedData, &gClearRenderTargetInfo, sizeof(RenderTargetInfo));
        endUpdateResource(&bufferUpdate);

        // Update main view render target data
        bufferUpdate = { pRenderTargetInfoConstantsBuffers[frameIdx][VIEW_CAMERA] };
        beginUpdateResource(&bufferUpdate);
        memcpy(bufferUpdate.pMappedData, &gDepthRenderTargetInfo, sizeof(RenderTargetInfo));
        endUpdateResource(&bufferUpdate);

        // Update shadow
        bufferUpdate = { pRenderTargetInfoConstantsBuffers[frameIdx][VIEW_SHADOW] };
        beginUpdateResource(&bufferUpdate);
        memcpy(bufferUpdate.pMappedData, &gShadowRenderTargetInfo, sizeof(RenderTargetInfo));
        endUpdateResource(&bufferUpdate);
    }

    // Draw GUI / 2D elements
    void drawGUI(Cmd* cmd, uint32_t frameIdx)
    {
        UNREF_PARAM(frameIdx);

        RenderTarget*         rt = pSwapChain->ppRenderTargets[frameIdx];
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { rt, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)rt->mWidth, (float)rt->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, rt->mWidth, rt->mHeight);

        gFrameTimeDraw.mFontColor = gAppSettings.mVisualizeAO ? 0xff000000 : 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

        if (gAppSettings.mAsyncCompute)
        {
            cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gComputeProfileToken, &gFrameTimeDraw);
            cmdDrawGpuProfile(cmd, float2(8.0f, 425.0f), gGraphicsProfileToken, &gFrameTimeDraw);
        }
        else
        {
            cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGraphicsProfileToken, &gFrameTimeDraw);
        }

        cmdDrawUserInterface(cmd);

        cmdBindRenderTargets(cmd, NULL);
    }
};

DEFINE_APPLICATION_MAIN(Visibility_Buffer)
