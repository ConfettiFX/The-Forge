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
#include "../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Renderer/Interfaces/IVisibilityBuffer.h"
#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../Common_3/Utilities/RingBuffer.h"
#include "../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "SanMiguel.h"

// fsl
#include "../../../Common_3/Graphics/FSL/defaults.h"
#include "Shaders/FSL/ShaderDefs.h.fsl"
#include "Shaders/FSL/GLobal.srt.h"
#include "Shaders/FSL/GodrayBlur.srt.h"
#include "Shaders/FSL/LightClusters.srt.h"
#include "Shaders/FSL/TriangleFiltering.srt.h"
#include "Shaders/FSL/ProgMSAAResolve.srt.h"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)       \
    X(BindlessSupported, 1)      \
    X(DisableAO, 0)              \
    X(DisableGodRays, 0)         \
    X(MSAASampleCount, 4)        \
    X(AddGeometryPassThrough, 0) \
    X(MaxMSAALevel, 4)           \
    X(DisableAsyncCompute, 0)

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

#define SCENE_SCALE 10.0f

typedef enum OutputMode
{
    OUTPUT_MODE_SDR = 0,
    OUTPUT_MODE_P2020,
    OUTPUT_MODE_COUNT
} OutputMode;

struct UniformShadingData
{
    float4 lightColor;
    uint   lightingMode;
    uint   outputMode;
    float4 CameraPlane; // x : near, y : far
};

struct GodRayConstant
{
    float mScatterFactor;
};

GodRayConstant gGodRayConstant{ 0.5f };

struct GodRayBlurConstant
{
    uint32_t mBlurPassType; // Horizontal or Vertical pass
    uint32_t mFilterRadius;
};

#define MAX_BLUR_KERNEL_SIZE 8

struct gBlurWeights
{
    float mBlurWeights[MAX_BLUR_KERNEL_SIZE];
};

GodRayBlurConstant gGodRayBlurConstant;
gBlurWeights       gBlurWeightsUniform;
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

// static const DisplayChromacities DisplayChromacityList[] =
//{
//	{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Display Gamut Rec709
//	{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // Display Gamut Rec2020
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Display Gamut P3D65
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31400f, 0.35100f }, // Display Gamut P3DCI(Theater)
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.32168f, 0.33767f }, // Display Gamut P3D60(ACES Cinema)
// };

// Camera Walking
static float gCameraWalkingTime = 0.0f;
float3*      gCameraPathData;

uint  gCameraPoints;
float gTotalElpasedTime;
/************************************************************************/
// GUI CONTROLS
/************************************************************************/
#define MSAA_LEVELS_COUNT 3U
#if defined(ANDROID)
#define DEFAULT_ASYNC_COMPUTE false
#else
#define DEFAULT_ASYNC_COMPUTE true
#endif

typedef struct AppSettings
{
    OutputMode mOutputMode = OUTPUT_MODE_SDR;

    // Set this variable to true to bypass triangle filtering calculations, holding and representing the last filtered data.
    // This is useful for inspecting filtered geometry for debugging purposes.
    bool mHoldFilteredResults = false;

    bool mAsyncCompute = DEFAULT_ASYNC_COMPUTE;
    // toggle rendering of local point lights
    bool mRenderLocalLights = false;

    bool mDrawDebugTargets = false;

    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // adjust directional sunlight angle
    float2 mSunControl = { -2.1f, 0.164f };

    float mSunSize = 300.0f;

    float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };
    float  mGodrayAttenuation = 0.3f;

    DynamicUIWidgets mDynamicUIWidgetsGR;
    bool             mEnableGodray = true;
    uint32_t         mFilterRadius = 3;

    float mEsmControl = 200.0f;

    // AO data
    DynamicUIWidgets mDynamicUIWidgetsAO;
    bool             mEnableAO = true;
    bool             mVisualizeAO = false;
    float            mAOIntensity = 3.0f;
    int              mAOQuality = 2;

    DynamicUIWidgets mSCurve;
    float            SCurveScaleFactor = 2.6f;
    float            SCurveSMin = 0.00f;
    float            SCurveSMid = 12.0f;
    float            SCurveSMax = 99.0f;
    float            SCurveTMin = 0.0f;
    float            SCurveTMid = 11.0f;
    float            SCurveTMax = 400.0f;
    float            SCurveSlopeFactor = 1.475f;

    DynamicUIWidgets mLinearScale;
    float            LinearScale = 260.0f;

    // HDR10
    DynamicUIWidgets mDisplaySetting;

    DisplayColorSpace  mCurrentSwapChainColorSpace = ColorSpace_Rec2020;
    DisplayColorRange  mDisplayColorRange = ColorRange_RGB;
    DisplaySignalRange mDisplaySignalRange = Display_SIGNAL_RANGE_FULL;

    SampleCount mMsaaLevel = SAMPLE_COUNT_1;
    SampleCount mMaxMsaaLevel = SAMPLE_COUNT_4;
    uint32_t    mMsaaIndex = (uint32_t)log2((uint32_t)mMsaaLevel);
    uint32_t    mMsaaIndexRequested = mMsaaIndex;

    // Camera Walking
    bool  cameraWalking = false;
    float cameraWalkingSpeed = 1.0f;

    // VR 2D layer transform (positioned at -1 along the Z axis, default rotation, default scale)
    VR2DLayerDesc mVR2DLayer{ { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, 1.0f };
} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/
const char* gSceneName = "SanMiguel_3/SanMiguel.bin";

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Constants
const uint32_t gShadowMapSize = 1024;
const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = NUM_GEOMETRY_SETS;

// The number of render targets to be resolved
FORGE_CONSTEXPR uint32_t gResolveTargetCount = 2;
FORGE_CONSTEXPR uint32_t gResolveGodRayPassIndex = 0;
FORGE_CONSTEXPR uint32_t gResolveFinalPassIndex = 1;

FORGE_CONSTEXPR TinyImageFormat gDebugRTFormat = TinyImageFormat_R8G8B8A8_UNORM;

/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3                    gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    PerFrameConstantsData   gPerFrameUniformData = {};
    PerFrameVBConstantsData gPerFrameVBUniformData = {};
    UniformCameraSkyData    gUniformDataSky;
};

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
VBPreFilterStats  gVBPreFilterStats[gDataBufferCount];
/************************************************************************/
// Queues and Command buffers
/************************************************************************/
Queue*            pGraphicsQueue = NULL;
GpuCmdRing        gGraphicsCmdRing = {};

Queue*         pComputeQueue = NULL;
GpuCmdRing     gComputeCmdRing = {};
DescriptorSet* pDescriptorSetPersistent = NULL;
DescriptorSet* pDescriptorSetPerFrame = NULL;
DescriptorSet* pDescriptorSetClusterLights = NULL;
DescriptorSet* pDescriptorSetTriangleFiltering = NULL;
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
// Triangle filtering pipeline
/************************************************************************/
Shader*        pShaderTriangleFiltering = nullptr;
Pipeline*      pPipelineTriangleFiltering = nullptr;
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
Shader*        pShaderShadowPass[gNumGeomSets] = { NULL };
Pipeline*      pPipelineShadowPass[gNumGeomSets] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*        pShaderVisibilityBufferPass[gNumGeomSets] = {};
Pipeline*      pPipelineVisibilityBufferPass[gNumGeomSets] = {};
/************************************************************************/
// VB shade pipeline
/************************************************************************/
// MSAA + AO + Godray variants
const uint32_t kNumVisBufShaderVariants = 4 * MSAA_LEVELS_COUNT;
Shader*        pShaderVisibilityBufferShade[kNumVisBufShaderVariants] = { nullptr };
Pipeline*      pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
Texture*       pSkybox = NULL;
/************************************************************************/
// MSAA Edge Detection pipeline
/************************************************************************/
Shader*        pShaderDrawMSAAEdges[MSAA_LEVELS_COUNT - 1] = { nullptr };
Shader*        pShaderDownscaleMSAAEdges[MSAA_LEVELS_COUNT - 1] = { nullptr };
Pipeline*      pPipelineDrawMSAAEdges = nullptr;
Pipeline*      pPipelineDownscaleMSAAEdges = nullptr;
/************************************************************************/
// Resolve pipeline
/************************************************************************/
Shader*        pShaderResolve[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*      pPipelineResolve = nullptr;
Pipeline*      pPipelineResolveGodRay = nullptr;
DescriptorSet* pDescriptorSetResolve = nullptr;
/************************************************************************/
// Godray pipeline
/************************************************************************/
Shader*        pGodRayPass[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*      pPipelineGodRayPass = nullptr;
Buffer*        pBufferGodRayConstant = nullptr;
uint32_t       gGodRayConstantIndex = 0;

Shader*   pShaderGodRayBlurPass = nullptr;
Pipeline* pPipelineGodRayBlurPass = nullptr;
Buffer*   pBufferBlurWeights = nullptr;

DescriptorSet* pDescriptorSetGodRayBlurPassPerDraw = nullptr;
Buffer*        pGodRayBlurBuffer[2][gDataBufferCount] = { { NULL } };
uint32_t       gGodRayBlurConstantIndex = 0;

OutputMode         gWasOutputMode = gAppSettings.mOutputMode;
DisplayColorSpace  gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
DisplayColorRange  gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
DisplaySignalRange gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;

/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*   pShaderPresentPass = nullptr;
Pipeline* pPipelinePresentPass = nullptr;
uint32_t  gSCurveRootConstantIndex = 0;

/************************************************************************/
// Debug View pipeline
/************************************************************************/
Shader*   pShaderDebugMSAA[MSAA_LEVELS_COUNT - 1] = { nullptr };
Pipeline* pPipelineDebugMSAA = nullptr;

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pDepthBuffer = NULL;
RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pRenderTargetMSAA = NULL;
RenderTarget* pRenderTargetDebugMSAA = NULL;
RenderTarget* pRenderTargetShadow = NULL;
RenderTarget* pIntermediateRenderTarget = NULL;
RenderTarget* pRenderTargetGodRay[2] = { NULL };
RenderTarget* pRenderTargetGodRayMS = NULL;
RenderTarget* pRenderTargetDebugGodRayMSAA = NULL;
RenderTarget* pMSAAEdgesStencilBuffer = NULL;
RenderTarget* pDownscaledMSAAEdgesStencilBuffer = NULL;
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
Buffer*       pPerFrameUniformBuffers[gDataBufferCount] = { NULL };
enum
{
    VB_UB_COMPUTE = 0,
    VB_UB_GRAPHICS,
    VB_UB_COUNT
};
Buffer* pPerFrameVBUniformBuffers[VB_UB_COUNT][gDataBufferCount] = {};
Buffer* pMeshConstantsBuffer = NULL;

/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*         pLightsBuffer = NULL;
Buffer**        gPerBatchUniformBuffers = NULL;
Buffer*         pVertexBufferCube = NULL;
Buffer*         pIndexBufferCube = NULL;
Buffer*         pLightClustersCount[gDataBufferCount] = { NULL };
Buffer*         pLightClusters[gDataBufferCount] = { NULL };
Buffer*         pUniformBufferSky[gDataBufferCount] = { NULL };
uint64_t        gFrameCount = 0;
VBMeshInstance* pVBMeshInstances = NULL;
uint32_t        gMeshCount = 0;
uint32_t        gMaterialCount = 0;
UIComponent*    pGuiWindow = NULL;
UIComponent*    pDebugTexturesWindow = NULL;
UIWidget*       pOutputSupportsHDRWidget = NULL;
FontDrawDesc    gFrameTimeDraw;
uint32_t        gFontID = 0;

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

    pVBRTs[textureCount++] = pRenderTargetShadow->pTexture;
    if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
    {
        if (gAppSettings.mEnableGodray)
        {
            pVBRTs[textureCount++] = pRenderTargetDebugGodRayMSAA->pTexture;
        }
        pVBRTs[textureCount++] = pRenderTargetDebugMSAA->pTexture;
    }

    if (pDebugTexturesWindow)
    {
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->pTextures = pVBRTs;
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->mTexturesCount = textureCount;
        ((DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget)->mTextureDisplaySize = texSize;
    }
}

UIWidget* addResolutionProperty(UIComponent* pUIManager, uint32_t& resolutionIndex, uint32_t resCount, Resolution* pResolutions,
                                WidgetCallback onResolutionChanged)
{
#if defined(_WINDOWS)
    if (pUIManager)
    {
        ResolutionData& data = gGuiResolution;

        static const uint32_t maxResNameLength = 16;
        arrsetlen(data.mResNameContainer, 0);
        arrsetcap(data.mResNameContainer, maxResNameLength * resCount);
        arrsetlen(data.mResNamePointers, resCount);

        char* pBuf = data.mResNameContainer;
        int   remainingLen = (int)arrcap(data.mResNameContainer);

        for (uint32_t i = 0; i < resCount; ++i)
        {
            int res = snprintf(pBuf, remainingLen, "%ux%u", pResolutions[i].mWidth, pResolutions[i].mHeight);
            ASSERT(res >= 0 && res < remainingLen);

            data.mResNamePointers[i] = pBuf;

            pBuf += res + 1;
            remainingLen -= res + 1;
        }

        DropdownWidget control;
        control.pData = &resolutionIndex;
        control.pNames = data.mResNamePointers;
        control.mCount = resCount;

        UIWidget* pControl = uiAddComponentWidget(pUIManager, "Screen Resolution", &control, WIDGET_TYPE_DROPDOWN);
        pControl->pOnEdited = onResolutionChanged;
        return pControl;
    }
#else
    UNREF_PARAM(pUIManager);
    UNREF_PARAM(resolutionIndex);
    UNREF_PARAM(resCount);
    UNREF_PARAM(pResolutions);
    UNREF_PARAM(onResolutionChanged);
#endif

    return NULL;
}

const char* gTestScripts[] = { "Test_MSAA_0.lua", "Test_MSAA_2.lua", "Test_MSAA_4.lua" };

uint32_t gCurrentScriptIndex = 0;
void     RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

class Visibility_Buffer: public IApp
{
public:
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

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
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
        gAppSettings.mMaxMsaaLevel = (SampleCount)gGpuSettings.mMaxMSAALevel;
        gAppSettings.mMsaaLevel =
            (SampleCount)clamp(gGpuSettings.mMSAASampleCount, (uint32_t)SAMPLE_COUNT_1, (uint32_t)gAppSettings.mMaxMsaaLevel);
        gAppSettings.mMsaaIndex = (uint32_t)log2((uint32_t)gAppSettings.mMsaaLevel);
        gAppSettings.mMsaaIndexRequested = gAppSettings.mMsaaIndex;
        gAppSettings.mAsyncCompute &= !gGpuSettings.mDisableAsyncCompute;

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
        blurWeightsBufferDesc.mDesc.mSize = sizeof(gBlurWeights);
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
        pVBMeshInstances = (VBMeshInstance*)tf_calloc(gMeshCount, sizeof(VBMeshInstance));
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

        /************************************************************************/
        // Init visibility buffer
        /************************************************************************/
        uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};

        HiresTimer vbSetupTimer;
        initHiresTimer(&vbSetupTimer);

        // Calculate clusters
        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            MaterialFlags material = gScene->materialFlags[i];
            uint32_t      geomSet = material & MATERIAL_FLAG_ALPHA_TESTED ? GEOMSET_ALPHA_CUTOUT : GEOMSET_OPAQUE;
            visibilityBufferFilteredIndexCount[geomSet] += (gScene->geom->pDrawArgs + i)->mIndexCount;

            pVBMeshInstances[i].mGeometrySet = geomSet;
            pVBMeshInstances[i].mMeshIndex = i;
            pVBMeshInstances[i].mTriangleCount = (gScene->geom->pDrawArgs + i)->mIndexCount / 3;
            pVBMeshInstances[i].mInstanceIndex = INSTANCE_INDEX_NONE;
        }

        VisibilityBufferDesc vbDesc = {};
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = gDataBufferCount;
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        removeResource(gScene->geomData);
        gScene->geomData = nullptr;
        LOGF(LogLevel::eINFO, "Setup vb : %f ms", getHiresTimerUSec(&vbSetupTimer, true) / 1000.0f);

        UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
        updateVBMeshFilterGroupsDesc.mNumMeshInstance = gMeshCount;
        updateVBMeshFilterGroupsDesc.pVBMeshInstances = pVBMeshInstances;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            updateVBMeshFilterGroupsDesc.mFrameIndex = i;
            gVBPreFilterStats[i] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
        }

        // Create geometry for light rendering
        createCubeBuffers(pRenderer, &pVertexBufferCube, &pIndexBufferCube);

        const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

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
        vec3                   startPosition(124.0f, 98.0f, 14.0f);
        vec3                   startLookAt = startPosition + vec3(-0.2f, 0.02f, 0.0f);
        CameraMotionParameters camParams;
        camParams.acceleration = 1300 * 2.5f;
        camParams.braking = 1300 * 2.5f;
        camParams.maxSpeed = 200 * .25f;
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
        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);

        tf_free(gCameraPathData);

        exitCameraController(pCameraController);
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            removeResource(pGodRayBlurBuffer[0][frameIdx]);
            removeResource(pGodRayBlurBuffer[1][frameIdx]);
        }
        removeResource(pBufferGodRayConstant);
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
        tf_free(pVBMeshInstances);

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

            DropdownWidget ddTestScripts;
            ddTestScripts.pData = &gCurrentScriptIndex;
            ddTestScripts.pNames = gTestScripts;
            ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);

            /************************************************************************/
            // Most important options
            /************************************************************************/

            gAppSettings.mHoldFilteredResults = false;

            CheckboxWidget checkbox;
            checkbox.pData = &gAppSettings.mHoldFilteredResults;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Hold filtered results", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mAsyncCompute;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Async Compute", &checkbox, WIDGET_TYPE_CHECKBOX));

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

            // Light Settings
            //---------------------------------------------------------------------------------
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

            SliderFloatWidget esmControl;
            esmControl.pData = &gAppSettings.mEsmControl;
            esmControl.mMin = 0.0f;
            esmControl.mMax = 400.0f;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "ESM Control", &esmControl, WIDGET_TYPE_SLIDER_FLOAT));

            checkbox.pData = &gAppSettings.mEnableGodray;
            UIWidget* pGRSelector = uiAddComponentWidget(pGuiWindow, "Enable Godray", &checkbox, WIDGET_TYPE_CHECKBOX);
            uiSetWidgetOnEditedCallback(pGRSelector, nullptr,
                                        [](void* pUserData)
                                        {
                                            UNREF_PARAM(pUserData);
                                            ReloadDesc reloadDescriptor;
                                            reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                                            requestReload(&reloadDescriptor);
                                        });
            luaRegisterWidget(pGRSelector);

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

            checkbox.pData = &gAppSettings.mRenderLocalLights;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable Random Point Lights", &checkbox, WIDGET_TYPE_CHECKBOX));

            /************************************************************************/
            // Rendering Settings
            /************************************************************************/
            /************************************************************************/
            // MSAA Settings
            /************************************************************************/
            static const char* msaaSampleNames[] = { "Off", "2 Samples", "4 Samples" };

            DropdownWidget ddMSAA;
            ddMSAA.pData = &gAppSettings.mMsaaIndexRequested;
            ddMSAA.pNames = msaaSampleNames;
            uint32_t msaaOptionsCount = 1 + (((uint32_t)gAppSettings.mMaxMsaaLevel) >> 1);
            ddMSAA.mCount = msaaOptionsCount;

            UIWidget* msaaWidget = uiAddComponentWidget(pGuiWindow, "MSAA", &ddMSAA, WIDGET_TYPE_DROPDOWN);
            uiSetWidgetOnEditedCallback(msaaWidget, nullptr,
                                        [](void* pUserData)
                                        {
                                            UNREF_PARAM(pUserData);
                                            ReloadDesc reloadDescriptor;
                                            reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                                            requestReload(&reloadDescriptor);
                                        });
            luaRegisterWidget(msaaWidget);

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

            if (pReloadDesc->mType & RELOAD_TYPE_RENDERTARGET)
            {
                if (gAppSettings.mMsaaIndex != gAppSettings.mMsaaIndexRequested)
                {
                    gAppSettings.mMsaaIndex = gAppSettings.mMsaaIndexRequested;
                    gAppSettings.mMsaaLevel = (SampleCount)(1 << gAppSettings.mMsaaIndex);
                    while (gAppSettings.mMsaaIndex > 0)
                    {
                        bool isValidLevel = (pRenderer->pGpu->mFrameBufferSamplesCount & gAppSettings.mMsaaLevel) != 0;
                        isValidLevel &= gAppSettings.mMsaaLevel <= gAppSettings.mMaxMsaaLevel;
                        if (!isValidLevel)
                        {
                            gAppSettings.mMsaaIndex--;
                            gAppSettings.mMsaaLevel = (SampleCount)(gAppSettings.mMsaaLevel / 2);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
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
        VR2DLayerDesc vr2DLayer = gAppSettings.mVR2DLayer;
        uiLoad.mVR2DLayer.mPosition = float3(vr2DLayer.m2DLayerPosition.x, vr2DLayer.m2DLayerPosition.y, vr2DLayer.m2DLayerPosition.z);
        uiLoad.mVR2DLayer.mScale = vr2DLayer.m2DLayerScale;
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

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
            uiRemoveDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO);
            uiRemoveDynamicWidgets(&gAppSettings.mLinearScale);
            uiRemoveDynamicWidgets(&gAppSettings.mSCurve);
            uiRemoveDynamicWidgets(&gAppSettings.mDisplaySetting);
            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET | RELOAD_TYPE_SCENE_RESOLUTION))
        {
            removeRenderTargets();

            if (pDebugTexturesWindow)
            {
                uiRemoveComponent(pDebugTexturesWindow);
                pDebugTexturesWindow = NULL;
            }

            ESRAM_RESET_ALLOCS(pRenderer);
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
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
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
        bool              useDedicatedComputeQueue = gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredResults;
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
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
            endUpdateResource(&update);
            /************************************************************************/
            // Triangle filtering async compute pass
            /************************************************************************/
            Cmd* computeCmd = computeElem.pCmds[0];

            resetCmdPool(pRenderer, computeElem.pCmdPool);
            beginCmd(computeCmd);
            cmdBindDescriptorSet(computeCmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(computeCmd, frameIdx, pDescriptorSetPerFrame);
            cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);

            TriangleFilteringPassDesc triangleFilteringDesc = {};
            triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
            triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorSetTriangleFiltering;

            triangleFilteringDesc.mFrameIndex = frameIdx;
            triangleFilteringDesc.mBuffersIndex = frameIdx;
            triangleFilteringDesc.mGpuProfileToken = gComputeProfileToken;
            triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIdx];
            cmdVBTriangleFilteringPass(pVisibilityBuffer, computeCmd, &triangleFilteringDesc);

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

            // Get the current render target for this frame
            acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);

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
                memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
                endUpdateResource(&update);
            }

            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
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
            cmdBindDescriptorSet(graphicsCmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(graphicsCmd, frameIdx, pDescriptorSetPerFrame);

            cmdBeginGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);

            if (!gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredResults)
            {
                TriangleFilteringPassDesc triangleFilteringDesc = {};
                triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
                triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorSetTriangleFiltering;

                triangleFilteringDesc.mFrameIndex = frameIdx;
                triangleFilteringDesc.mBuffersIndex = frameIdx;
                triangleFilteringDesc.mGpuProfileToken = gGraphicsProfileToken;
                triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIdx];
                cmdVBTriangleFilteringPass(pVisibilityBuffer, graphicsCmd, &triangleFilteringDesc);
            }

            if (!gAppSettings.mAsyncCompute)
            {
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Clear Light Clusters");
                clearLightClusters(graphicsCmd, frameIdx);
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
            }

            if (!gAppSettings.mAsyncCompute && gAppSettings.mRenderLocalLights)
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
            uint32_t            rtBarriersCount = gAppSettings.mMsaaLevel > 1 ? 3 : 2;
            RenderTargetBarrier rtBarriers[] = {
                { pScreenRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
                { pRenderTargetMSAA, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
            };

            const uint32_t maxNumBarriers = NUM_CULLING_VIEWPORTS + 4;
            uint32_t       barrierCount = 0;
            BufferBarrier  barriers2[maxNumBarriers] = {};
            barriers2[barrierCount++] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
            barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                              RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };

                barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                              RESOURCE_STATE_SHADER_RESOURCE };

                for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
                {
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + i],
                                                  RESOURCE_STATE_UNORDERED_ACCESS,
                                                  RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
                }
            }

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, rtBarriersCount, rtBarriers);

            drawScene(graphicsCmd, frameIdx);

            if (gAppSettings.mDrawDebugTargets && gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
            {
                if (gAppSettings.mEnableGodray)
                {
                    drawMSAADebugRenderTargets(graphicsCmd, frameIdx, gResolveGodRayPassIndex, "GodRay", pRenderTargetDebugGodRayMSAA);
                }
                drawMSAADebugRenderTargets(graphicsCmd, frameIdx, gResolveFinalPassIndex, "VB Shade", pRenderTargetDebugMSAA);
            }

            barrierCount = 0;
            barriers2[barrierCount++] = { pLightClusters[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
            barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };

            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx],
                                              RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE,
                                              RESOURCE_STATE_UNORDERED_ACCESS };

                barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE,
                                              RESOURCE_STATE_UNORDERED_ACCESS };

                for (uint32_t i = 0; i < NUM_CULLING_VIEWPORTS; ++i)
                {
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + i],
                                                  RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE,
                                                  RESOURCE_STATE_UNORDERED_ACCESS };
                }
            }

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 0, NULL);

            presentImage(graphicsCmd, pScreenRenderTarget, pSwapChain->ppRenderTargets[presentIndex]);

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

    const char* GetName() { return "Visibility_Buffer"; }

    bool addDescriptorSets()
    {
        // Clear Buffers
        DescriptorSetDesc setDesc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPersistent);
        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);

        setDesc = SRT_SET_DESC(SrtClusterLightsData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClusterLights);

        setDesc = SRT_SET_DESC(TriangleFilteringSrtData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering);

        // God Ray Blur
        setDesc = SRT_SET_DESC(SrtGodrayBlurComp, PerDraw, 4, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayBlurPassPerDraw);

        // Resolve
        // Currently, we do two resolve passes (godrays for blur and final resolve).
        const uint32_t resolveMaxSets = gDataBufferCount * gResolveTargetCount;
        setDesc = SRT_SET_DESC(SrtResolve, PerDraw, resolveMaxSets, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetResolve);
        return true;
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetResolve);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayBlurPassPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering);
        removeDescriptorSet(pRenderer, pDescriptorSetClusterLights);
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPersistent);
    }

    void prepareDescriptorSets()
    {
        constexpr uint32_t PERSISTENT_SET_COUNT = 20;
        DescriptorData     persistentSetParams[PERSISTENT_SET_COUNT] = {};
        Texture*           godrayTextures[] = { pRenderTargetGodRay[0]->pTexture, pRenderTargetGodRay[1]->pTexture };

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
        persistentSetParams[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gTextureFilter);
        persistentSetParams[5].ppSamplers = &pSamplerPointClamp;
        persistentSetParams[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthSampler);
        persistentSetParams[6].ppSamplers = &pSamplerBilinearClamp;
        persistentSetParams[7].mIndex = SRT_RES_IDX(SrtData, Persistent, gTextureSampler);
        persistentSetParams[7].ppSamplers = &pSamplerTrilinearAniso;
        persistentSetParams[8].mIndex = SRT_RES_IDX(SrtData, Persistent, gLights);
        persistentSetParams[8].ppBuffers = &pLightsBuffer;
        persistentSetParams[9].mIndex = SRT_RES_IDX(SrtData, Persistent, gDiffuseMaps);
        persistentSetParams[9].mCount = gMaterialCount;
        persistentSetParams[9].ppTextures = gDiffuseMapsStorage;
        persistentSetParams[10].mIndex = SRT_RES_IDX(SrtData, Persistent, gNormalMaps);
        persistentSetParams[10].mCount = gMaterialCount;
        persistentSetParams[10].ppTextures = gNormalMapsStorage;
        persistentSetParams[11].mIndex = SRT_RES_IDX(SrtData, Persistent, gSpecularMaps);
        persistentSetParams[11].mCount = gMaterialCount;
        persistentSetParams[11].ppTextures = gSpecularMapsStorage;
        persistentSetParams[12].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBTex);
        persistentSetParams[12].ppTextures = &pRenderTargetVBPass->pTexture;
        persistentSetParams[13].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthTex);
        persistentSetParams[13].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[14].mIndex = SRT_RES_IDX(SrtData, Persistent, gShadowMap);
        persistentSetParams[14].ppTextures = &pRenderTargetShadow->pTexture;
        persistentSetParams[15].mIndex = SRT_RES_IDX(SrtData, Persistent, gGodRayTexture);
        persistentSetParams[15].ppTextures = &pRenderTargetGodRay[0]->pTexture;
        persistentSetParams[16].mIndex = SRT_RES_IDX(SrtData, Persistent, gBlurWeights);
        persistentSetParams[16].ppBuffers = &pBufferBlurWeights;
        persistentSetParams[17].mIndex = SRT_RES_IDX(SrtData, Persistent, gDisplayTexture);
        persistentSetParams[17].ppTextures = &pIntermediateRenderTarget->pTexture;
        persistentSetParams[18].mIndex = SRT_RES_IDX(SrtData, Persistent, gSkyboxTex);
        persistentSetParams[18].ppTextures = &pSkybox;
        persistentSetParams[19].mIndex = SRT_RES_IDX(SrtData, Persistent, gMSAAStencil);
        persistentSetParams[19].ppTextures = &pMSAAEdgesStencilBuffer->pTexture;
        persistentSetParams[19].mBindStencilResource = true;

        updateDescriptorSet(pRenderer, 0, pDescriptorSetPersistent, PERSISTENT_SET_COUNT, persistentSetParams);
        // per frame set
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData perFrameParams[10] = {};
                perFrameParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameVBConstants);
                perFrameParams[0].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                perFrameParams[1].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameVBConstantsComp);
                perFrameParams[1].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
                perFrameParams[2].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilterDispatchGroupDataBuffer);
                perFrameParams[2].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
                perFrameParams[3].mIndex = SRT_RES_IDX(SrtData, PerFrame, gVBConstantBuffer);
                perFrameParams[3].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
                perFrameParams[4].mIndex = SRT_RES_IDX(SrtData, PerFrame, gIndirectDataBuffer);
                perFrameParams[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                perFrameParams[5].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClustersCount);
                perFrameParams[5].ppBuffers = &pLightClustersCount[i];
                perFrameParams[6].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClusters);
                perFrameParams[6].ppBuffers = &pLightClusters[i];
                perFrameParams[7].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameConstants);
                perFrameParams[7].ppBuffers = &pPerFrameUniformBuffers[i];
                perFrameParams[8].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilteredIndexBuffer);
                perFrameParams[8].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
                perFrameParams[9].mIndex = SRT_RES_IDX(SrtData, PerFrame, gUniformCameraSky);
                perFrameParams[9].ppBuffers = &pUniformBufferSky[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, 10, perFrameParams);
            }
        }

        // cluster lights
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData clusterLightsParams[2] = {};
            clusterLightsParams[0].mIndex = SRT_RES_IDX(SrtClusterLightsData, PerBatch, gLightClustersCountRW);
            clusterLightsParams[0].ppBuffers = &pLightClustersCount[i];
            clusterLightsParams[1].mIndex = SRT_RES_IDX(SrtClusterLightsData, PerBatch, gLightClustersRW);
            clusterLightsParams[1].ppBuffers = &pLightClusters[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetClusterLights, 2, clusterLightsParams);
        }

        // triangle filtering
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData triangleFilteringParams[4] = {};
            triangleFilteringParams[0].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gFilteredIndicesBufferRW);
            triangleFilteringParams[0].mCount = gNumViews;
            triangleFilteringParams[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS];
            triangleFilteringParams[1].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDrawFilteringArgsRW);
            triangleFilteringParams[1].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
            triangleFilteringParams[2].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDataBufferRW);
            triangleFilteringParams[2].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            triangleFilteringParams[3].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDrawClearArgsRW);
            triangleFilteringParams[3].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFiltering, 4, triangleFilteringParams);
        }

        // God Ray Blur
        {
            DescriptorData params[2] = {};
            params[0].mIndex = SRT_RES_IDX(SrtGodrayBlurComp, PerDraw, gGodRayTexturesRW);
            params[0].ppTextures = godrayTextures;
            params[0].mCount = 2;
            params[1].mIndex = SRT_RES_IDX(SrtGodrayBlurComp, PerDraw, gBlurParams);
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

        // Resolve
        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            Texture* resolveSources[] = { pRenderTargetGodRayMS->pTexture, pRenderTargetMSAA->pTexture };
            for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; frameIdx++)
            {
                for (uint32_t resolvePassIdx = 0; resolvePassIdx < gResolveTargetCount; ++resolvePassIdx)
                {
                    DescriptorData params[1] = {};
                    params[0].mIndex = SRT_RES_IDX(SrtResolve, PerDraw, gResolveSource);
                    params[0].ppTextures = &resolveSources[resolvePassIdx];
                    updateDescriptorSet(pRenderer, frameIdx * gDataBufferCount + resolvePassIdx, pDescriptorSetResolve, 1, params);
                }
            }
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
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_2D_VR_LAYER | SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
        swapChainDesc.mVR.m2DLayer = gAppSettings.mVR2DLayer;

        swapChainDesc.mVR.mFoveationLevel = FOVEATION_LEVEL_HIGH;

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

        ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        ClearValue optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

        // Disable foveation when rendering with MSAA (stencil + foveation)
        TextureCreationFlags foveationFlags =
            gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 ? TEXTURE_CREATION_FLAG_NONE : TEXTURE_CREATION_FLAG_VR_FOVEATED_RENDERING;

        /************************************************************************/
        // Main depth buffer
        /************************************************************************/
        // Add depth buffer
        ESRAM_BEGIN_ALLOC(pRenderer, "Depth Buffer", 0u);
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = depthStencilClear;
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        depthRT.mHeight = height;
        depthRT.mSampleCount = gAppSettings.mMsaaLevel;
        depthRT.mSampleQuality = 0;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW | foveationFlags |
                         (gAppSettings.mMsaaLevel > SAMPLE_COUNT_2 ? TEXTURE_CREATION_FLAG_NONE : TEXTURE_CREATION_FLAG_ESRAM);
        depthRT.mWidth = width;
        depthRT.pName = "Depth Buffer RT";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        ESRAM_CURRENT_OFFSET(pRenderer, depthAllocationOffset);

        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        // Shadow pass render target
        /************************************************************************/
        RenderTargetDesc shadowRTDesc = {};
        shadowRTDesc.mArraySize = 1;
        shadowRTDesc.mClearValue = depthStencilClear;
        shadowRTDesc.mDepth = 1;
        shadowRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        shadowRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
        shadowRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
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
        ESRAM_BEGIN_ALLOC(pRenderer, "VB RT", depthAllocationOffset);
        RenderTargetDesc vbRTDesc = {};
        vbRTDesc.mArraySize = 1;
        vbRTDesc.mClearValue = optimizedColorClearWhite;
        vbRTDesc.mDepth = 1;
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        vbRTDesc.mHeight = height;
        vbRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW | foveationFlags;
        vbRTDesc.mWidth = width;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);

        ESRAM_CURRENT_OFFSET(pRenderer, vbPassRTAllocationOffset);

        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        // MSAA render target
        /************************************************************************/
        RenderTargetDesc msaaRTDesc = {};
        msaaRTDesc.mArraySize = 1;
        msaaRTDesc.mClearValue = optimizedColorClearBlack;
        msaaRTDesc.mDepth = 1;
        msaaRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        msaaRTDesc.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        msaaRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        msaaRTDesc.mHeight = height;
        msaaRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
        msaaRTDesc.mSampleQuality = 0;
        msaaRTDesc.mWidth = width;
        msaaRTDesc.pName = "MSAA RT";
        msaaRTDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW | foveationFlags;
        // Disabling compression data will avoid decompression phase before resolve pass.
        // However, the shading pass will require more memory bandwidth.
        // We measured with and without compression and without compression is faster in our case.
#ifndef PROSPERO
        msaaRTDesc.mFlags |= TEXTURE_CREATION_FLAG_NO_COMPRESSION;
#endif
        addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);

        // Debug MSAA Render Target. It will only be used if drawing debug targets is enabled.
        msaaRTDesc.mSampleCount = SAMPLE_COUNT_1;
        msaaRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        msaaRTDesc.mFormat = gDebugRTFormat;
        msaaRTDesc.pName = "MSAA Debug RT";
        addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetDebugMSAA);

        /************************************************************************/
        // Intermediate render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "Intermediate", vbPassRTAllocationOffset);
        RenderTargetDesc postProcRTDesc = {};
        postProcRTDesc.mArraySize = 1;
        postProcRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        postProcRTDesc.mDepth = 1;
        postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        postProcRTDesc.mFormat = pSwapChain->mFormat;
        postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        postProcRTDesc.mHeight = height;
        postProcRTDesc.mWidth = width;
        postProcRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        postProcRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW | foveationFlags;
        postProcRTDesc.pName = "pIntermediateRenderTarget";
        addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

        /************************************************************************/
        // GodRay render target
        /************************************************************************/
        TinyImageFormat GRRTFormat = (pRenderer->pGpu->mFormatCaps[TinyImageFormat_R10G10B10A2_UNORM] & (FORMAT_CAP_READ_WRITE))
                                         ? TinyImageFormat_R10G10B10A2_UNORM
                                         : TinyImageFormat_R8G8B8A8_UNORM;

        RenderTargetDesc GRRTDesc = {};
        GRRTDesc.mArraySize = 1;
        GRRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        GRRTDesc.mDepth = 1;
        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mHeight = height / GODRAY_SCALE;
        GRRTDesc.mWidth = width / GODRAY_SCALE;
        GRRTDesc.mFormat = GRRTFormat;
        GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        GRRTDesc.mSampleCount = SAMPLE_COUNT_1;
        GRRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        GRRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW | foveationFlags;

        GRRTDesc.pName = "GodRay RT A";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[0]);
        GRRTDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        GRRTDesc.mSampleCount = SAMPLE_COUNT_1;
        GRRTDesc.pName = "GodRay RT B";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[1]);

        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            GRRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
            GRRTDesc.pName = "GodRay RT - MSAA";
            addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRayMS);

            GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            GRRTDesc.mSampleCount = SAMPLE_COUNT_1;
            GRRTDesc.mFormat = gDebugRTFormat;
            GRRTDesc.pName = "GodRay RT - Debug MSAA View";
            addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetDebugGodRayMSAA);
        }

        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        /************************************************************************/

        {
            RenderTargetDesc msaaStencilRT = {};
            msaaStencilRT.mArraySize = 1;
            msaaStencilRT.mClearValue = depthStencilClear;
            msaaStencilRT.mDepth = 1;
            TinyImageFormat       stencilImageFormat = TinyImageFormat_S8_UINT;
            const TinyImageFormat candidateStencilFormats[] = { TinyImageFormat_S8_UINT, TinyImageFormat_D24_UNORM_S8_UINT,
                                                                TinyImageFormat_D16_UNORM_S8_UINT, TinyImageFormat_D32_SFLOAT_S8_UINT };
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(candidateStencilFormats); ++i)
            {
                TinyImageFormat  candidateFormat = candidateStencilFormats[i];
                FormatCapability capMask = FORMAT_CAP_DEPTH_STENCIL | FORMAT_CAP_READ;
                if ((pRenderer->pGpu->mFormatCaps[candidateFormat] & capMask) == capMask)
                {
                    stencilImageFormat = candidateFormat;
                    break;
                }
            }
            msaaStencilRT.mFormat = stencilImageFormat;
            msaaStencilRT.mStartState = RESOURCE_STATE_DEPTH_READ;
            msaaStencilRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            msaaStencilRT.mHeight = height;
            msaaStencilRT.mWidth = width;
            msaaStencilRT.mSampleCount = gAppSettings.mMsaaLevel;
            msaaStencilRT.mSampleQuality = 0;
            msaaStencilRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            msaaStencilRT.pName = "Stencil Buffer MSAA";
            addRenderTarget(pRenderer, &msaaStencilRT, &pMSAAEdgesStencilBuffer);

            msaaStencilRT.pName = "Downscaled Stencil Buffer MSAA";
            msaaStencilRT.mHeight = height / GODRAY_SCALE;
            msaaStencilRT.mWidth = width / GODRAY_SCALE;
            addRenderTarget(pRenderer, &msaaStencilRT, &pDownscaledMSAAEdgesStencilBuffer);
        }
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pDownscaledMSAAEdgesStencilBuffer);
        removeRenderTarget(pRenderer, pMSAAEdgesStencilBuffer);
        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            removeRenderTarget(pRenderer, pRenderTargetDebugGodRayMSAA);
            removeRenderTarget(pRenderer, pRenderTargetGodRayMS);
        }
        removeRenderTarget(pRenderer, pRenderTargetGodRay[0]);
        removeRenderTarget(pRenderer, pRenderTargetGodRay[1]);
        removeRenderTarget(pRenderer, pIntermediateRenderTarget);
        removeRenderTarget(pRenderer, pRenderTargetDebugMSAA);
        removeRenderTarget(pRenderer, pRenderTargetMSAA);
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pRenderTargetVBPass);
        removeRenderTarget(pRenderer, pRenderTargetShadow);
    }
    /************************************************************************/
    // Load all the shaders needed for the demo
    /************************************************************************/
    void addShaders()
    {
        ShaderLoadDesc shadowPass = {};
        ShaderLoadDesc shadowPassAlpha = {};
        ShaderLoadDesc vbPass = {};
        ShaderLoadDesc vbPassAlpha = {};
        ShaderLoadDesc vbShade[kNumVisBufShaderVariants] = {};
        ShaderLoadDesc resolvePass[MSAA_LEVELS_COUNT] = {};
        ShaderLoadDesc msaaEdgesShader[MSAA_LEVELS_COUNT - 1] = {};
        ShaderLoadDesc downscaleMSAAEdgesShader[MSAA_LEVELS_COUNT - 1] = {};
        ShaderLoadDesc msaaDebugShader[MSAA_LEVELS_COUNT - 1] = {};
        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};

        shadowPass.mVert.pFileName = "shadow_pass.vert";
        shadowPassAlpha.mVert.pFileName = "shadow_pass_alpha.vert";
        shadowPassAlpha.mFrag.pFileName = "shadow_pass_alpha.frag";

        vbPass.mVert.pFileName = "visibilityBuffer_pass.vert";
        vbPass.mFrag.pFileName = "visibilityBuffer_pass.frag";
        vbPassAlpha.mVert.pFileName = "visibilityBuffer_pass_alpha.vert";
        vbPassAlpha.mFrag.pFileName = "visibilityBuffer_pass_alpha.frag";

        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough;
        if (addGeometryPassThrough)
        {
            vbPass.mGeom.pFileName = "visibilityBuffer_pass.geom";
            vbPassAlpha.mGeom.pFileName = "visibilityBuffer_pass_alpha.geom";
        }

        const char* visibilityBufferShadeShaders[kNumVisBufShaderVariants] = {
            "visibilityBuffer_shade_SAMPLE_1.frag",
            "visibilityBuffer_shade_SAMPLE_1_AO.frag",
            "visibilityBuffer_shade_SAMPLE_2.frag",
            "visibilityBuffer_shade_SAMPLE_2_AO.frag",
            "visibilityBuffer_shade_SAMPLE_4.frag",
            "visibilityBuffer_shade_SAMPLE_4_AO.frag",
            // Godray variants
            "visibilityBuffer_shade_SAMPLE_1_GRAY.frag",
            "visibilityBuffer_shade_SAMPLE_1_AO_GRAY.frag",
            "visibilityBuffer_shade_SAMPLE_2_GRAY.frag",
            "visibilityBuffer_shade_SAMPLE_2_AO_GRAY.frag",
            "visibilityBuffer_shade_SAMPLE_4_GRAY.frag",
            "visibilityBuffer_shade_SAMPLE_4_AO_GRAY.frag",
        };

        const char* resolveShaders[] = {
            "progMSAAResolve_SAMPLE_1.frag",
            "progMSAAResolve_SAMPLE_2.frag",
            "progMSAAResolve_SAMPLE_4.frag",
        };

        for (uint32_t i = 0; i < kNumVisBufShaderVariants; ++i)
        {
            vbShade[i].mVert.pFileName = "visibilityBuffer_shade.vert";
            vbShade[i].mFrag.pFileName = visibilityBufferShadeShaders[i];
        }

        const char* edgeDetectShaders[] = { "msaa_edge_detect_SAMPLE_2.frag", "msaa_edge_detect_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            msaaEdgesShader[i].mVert.pFileName = "display.vert";
            msaaEdgesShader[i].mFrag.pFileName = edgeDetectShaders[i];
        }

        const char* edgeDetectDownscaleShaders[] = { "msaa_stencil_downscale_SAMPLE_2.frag", "msaa_stencil_downscale_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            downscaleMSAAEdgesShader[i].mVert.pFileName = "display.vert";
            downscaleMSAAEdgesShader[i].mFrag.pFileName = edgeDetectDownscaleShaders[i];
        }

        const char* debugMSAAFrag[] = { "msaa_debug_SAMPLE_2.frag", "msaa_debug_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            msaaDebugShader[i].mVert.pFileName = "display.vert";
            msaaDebugShader[i].mFrag.pFileName = debugMSAAFrag[i];
        }

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            // Resolve shader
            resolvePass[i].mVert.pFileName = "progMSAAResolve.vert";
            resolvePass[i].mFrag.pFileName = resolveShaders[i];
        }

        // Triangle culling compute shader
        triangleCulling.mComp.pFileName = "triangle_filtering.comp";
        // Clear buffers compute shader
        clearBuffer.mComp.pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mComp.pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mComp.pFileName = "cluster_lights.comp";

        const char* godrayShaderFileName[] = { "godray_SAMPLE_COUNT_1.frag", "godray_SAMPLE_COUNT_2.frag", "godray_SAMPLE_COUNT_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            ShaderLoadDesc godrayShaderDesc = {};
            godrayShaderDesc.mVert.pFileName = "display.vert";
            godrayShaderDesc.mFrag.pFileName = godrayShaderFileName[i];
            addShader(pRenderer, &godrayShaderDesc, &pGodRayPass[i]);
        }

        ShaderLoadDesc godrayBlurShaderDesc = {};
        godrayBlurShaderDesc.mComp.pFileName = "godray_blur.comp";
        addShader(pRenderer, &godrayBlurShaderDesc, &pShaderGodRayBlurPass);

        ShaderLoadDesc presentShaderDesc = {};
        presentShaderDesc.mVert.pFileName = "display.vert";
        presentShaderDesc.mFrag.pFileName = "display.frag";

        addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);
        addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < kNumVisBufShaderVariants; ++i)
            addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            addShader(pRenderer, &resolvePass[i], &pShaderResolve[i]);
        }
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            addShader(pRenderer, &msaaEdgesShader[i], &pShaderDrawMSAAEdges[i]);
            addShader(pRenderer, &downscaleMSAAEdgesShader[i], &pShaderDownscaleMSAAEdges[i]);
            addShader(pRenderer, &msaaDebugShader[i], &pShaderDebugMSAA[i]);
        }
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < kNumVisBufShaderVariants; ++i)
            removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            removeShader(pRenderer, pShaderDebugMSAA[i]);
            removeShader(pRenderer, pShaderDownscaleMSAAEdges[i]);
            removeShader(pRenderer, pShaderDrawMSAAEdges[i]);
        }
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            removeShader(pRenderer, pShaderResolve[i]);
        }
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
            removeShader(pRenderer, pGodRayPass[i]);
        removeShader(pRenderer, pShaderGodRayBlurPass);

        removeShader(pRenderer, pShaderPresentPass);
    }

    void addPipelines()
    {
        /************************************************************************/
        // Setup compute pipelines for triangle filtering
        /************************************************************************/
        PipelineDesc pipelineDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(TriangleFilteringSrtData, Persistent),
                             SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerFrame), SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerBatch),
                             NULL);
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
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtClusterLightsData, Persistent),
                             SRT_LAYOUT_DESC(SrtClusterLightsData, PerFrame), SRT_LAYOUT_DESC(SrtClusterLightsData, PerBatch), NULL);
        // Setup the clearing light clusters pipeline
        pipelineDesc.pName = "Clear Light Clusters";
        compPipelineSettings.pShaderProgram = pShaderClearLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // Setup the compute the light clusters pipeline
        pipelineDesc.pName = "Cluster Lights";
        compPipelineSettings.pShaderProgram = pShaderClusterLights;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);

        // God Ray Blur Pass
        pipelineDesc.pName = "God Ray Blur";
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtGodrayBlurComp, Persistent), SRT_LAYOUT_DESC(SrtGodrayBlurComp, PerFrame),
                             NULL, SRT_LAYOUT_DESC(SrtGodrayBlurComp, PerDraw));
        compPipelineSettings.pShaderProgram = pShaderGodRayBlurPass;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayBlurPass);

        /************************************************************************/
        /************************************************************************/
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;
        DepthStateDesc depthStateDisableDesc = {};

        DepthStateDesc depthStateOnlyReadStencilDesc = {};
        depthStateOnlyReadStencilDesc.mStencilWriteMask = 0x00;
        depthStateOnlyReadStencilDesc.mStencilReadMask = 0xFF;
        depthStateOnlyReadStencilDesc.mStencilTest = true;
        depthStateOnlyReadStencilDesc.mStencilFrontFunc = CMP_EQUAL;
        depthStateOnlyReadStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilDesc.mStencilFrontPass = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilDesc.mStencilBackFunc = CMP_EQUAL;
        depthStateOnlyReadStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilDesc.mStencilBackPass = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilDesc.mDepthBackFail = STENCIL_OP_KEEP;

        DepthStateDesc depthStateOnlyWriteStencilDesc = {};
        depthStateOnlyWriteStencilDesc.mStencilWriteMask = 0xFF;
        depthStateOnlyWriteStencilDesc.mStencilReadMask = 0xFF;
        depthStateOnlyWriteStencilDesc.mStencilTest = true;
        depthStateOnlyWriteStencilDesc.mStencilFrontFunc = CMP_GEQUAL;
        depthStateOnlyWriteStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyWriteStencilDesc.mStencilFrontPass = STENCIL_OP_REPLACE;
        depthStateOnlyWriteStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyWriteStencilDesc.mStencilBackFunc = CMP_GEQUAL;
        depthStateOnlyWriteStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateOnlyWriteStencilDesc.mStencilBackPass = STENCIL_OP_REPLACE;
        depthStateOnlyWriteStencilDesc.mDepthBackFail = STENCIL_OP_KEEP;

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        RasterizerStateDesc rasterizerStateCullNoneMsDesc = { CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID };
        rasterizerStateCullNoneMsDesc.mMultiSample = true;

        const bool isMSAAEnabled = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1;
        const bool isFoveationEnabled = !isMSAAEnabled;

        /************************************************************************/
        // Setup the Shadow Pass Pipeline
        /************************************************************************/
        // Setup pipeline settings
        pipelineDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
        shadowPipelineSettings = { 0 };
        shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        shadowPipelineSettings.pDepthState = &depthStateDesc;
        shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
        shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
        shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
        shadowPipelineSettings.pVertexLayout = NULL;
        shadowPipelineSettings.pRasterizerState =
            gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[0];
        pipelineDesc.pName = "Shadow Opaque";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[0]);

        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[1];
        pipelineDesc.pName = "Shadow AlphaTested";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[1]);

        /************************************************************************/
        // Setup the Visibility Buffer Pass Pipeline
        /************************************************************************/
        // Setup pipeline settings
        GraphicsPipelineDesc& vbPassPipelineSettings = pipelineDesc.mGraphicsDesc;
        vbPassPipelineSettings = { 0 };
        vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbPassPipelineSettings.mRenderTargetCount = 1;
        vbPassPipelineSettings.pDepthState = &depthStateDesc;
        vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mFormat;
        vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mSampleCount;
        vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
        vbPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        vbPassPipelineSettings.pVertexLayout = NULL;
        vbPassPipelineSettings.pRasterizerState =
            (gAppSettings.mMsaaLevel > 1) ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
        vbPassPipelineSettings.mVRFoveatedRendering = isFoveationEnabled;

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

#if defined(GFX_EXTENDED_PSO_OPTIONS)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_PIXEL_SHADER_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader =
                !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

            pipelineDesc.pPipelineExtensions = edescs;
            pipelineDesc.mExtensionCount = sizeof(edescs) / sizeof(edescs[0]);
#endif
            pipelineDesc.pName = GEOMSET_OPAQUE == i ? "VB Opaque" : "VB AlphaTested";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferPass[i]);

            pipelineDesc.mExtensionCount = 0;
        }
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
        vbShadePipelineSettings.pDepthState = isMSAAEnabled ? &depthStateOnlyReadStencilDesc : &depthStateDisableDesc;
        vbShadePipelineSettings.mDepthStencilFormat = isMSAAEnabled ? pMSAAEdgesStencilBuffer->mFormat : TinyImageFormat_UNDEFINED;
        vbShadePipelineSettings.pRasterizerState = isMSAAEnabled ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
        vbShadePipelineSettings.mVRFoveatedRendering = isFoveationEnabled;
        // Shader variants excluding godray
        const uint32_t kNumShaderVariantsExGodray = 2 * MSAA_LEVELS_COUNT;
        for (uint32_t i = 0; i < 2; ++i)
        {
            uint32_t shaderIndex = (gAppSettings.mMsaaIndex * 2 + i) + (kNumShaderVariantsExGodray * gAppSettings.mEnableGodray);
            vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[shaderIndex];
            vbShadePipelineSettings.mSampleCount = gAppSettings.mMsaaLevel;
            if (isMSAAEnabled)
            {
                vbShadePipelineSettings.pColorFormats = &pRenderTargetMSAA->mFormat;
                vbShadePipelineSettings.mSampleQuality = pRenderTargetMSAA->mSampleQuality;
            }
            else
            {
                // vbShadePipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
                vbShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
                vbShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
            }

#if defined(GFX_EXTENDED_PSO_OPTIONS)
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
        // Setup Godray pipeline
        /************************************************************************/
        GraphicsPipelineDesc& pipelineSettingsGodRay = pipelineDesc.mGraphicsDesc;
        pipelineSettingsGodRay = { 0 };
        pipelineSettingsGodRay.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsGodRay.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsGodRay.mVRFoveatedRendering = isFoveationEnabled;
        pipelineSettingsGodRay.mRenderTargetCount = 1;
        pipelineSettingsGodRay.pColorFormats = isMSAAEnabled ? &pRenderTargetGodRayMS->mFormat : &pRenderTargetGodRay[0]->mFormat;
        pipelineSettingsGodRay.mSampleCount = gAppSettings.mMsaaLevel;
        pipelineSettingsGodRay.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsGodRay.pShaderProgram = pGodRayPass[gAppSettings.mMsaaIndex];
        pipelineSettingsGodRay.pDepthState = isMSAAEnabled ? &depthStateOnlyReadStencilDesc : &depthStateDisableDesc;
        pipelineSettingsGodRay.mDepthStencilFormat = isMSAAEnabled ? pDownscaledMSAAEdgesStencilBuffer->mFormat : TinyImageFormat_UNDEFINED;
        pipelineDesc.pName = "God Ray";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayPass);

        /************************************************************************/
        // Setup Present pipeline
        /************************************************************************/

        GraphicsPipelineDesc& pipelineSettingsFinalPass = pipelineDesc.mGraphicsDesc;
        pipelineSettingsFinalPass = { 0 };
        pipelineSettingsFinalPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsFinalPass.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsFinalPass.mVRFoveatedRendering = true;
        pipelineSettingsFinalPass.mRenderTargetCount = 1;
        pipelineSettingsFinalPass.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;
        pipelineSettingsGodRay.pDepthState = NULL;
        pipelineSettingsGodRay.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        pipelineDesc.pName = "Composite";
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePresentPass);

        /************************************************************************/
        // Setup MSAA resolve pipeline
        /************************************************************************/
        depthStateDisableDesc = {};
        rasterizerStateCullNoneDesc = { CULL_MODE_NONE };

        pipelineDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineDesc, NULL, NULL, NULL, SRT_LAYOUT_DESC(SrtResolve, PerDraw));
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& resolvePipelineSettings = pipelineDesc.mGraphicsDesc;
        resolvePipelineSettings = { 0 };
        resolvePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        resolvePipelineSettings.mRenderTargetCount = 1;
        resolvePipelineSettings.pDepthState = &depthStateDisableDesc;
        resolvePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
        resolvePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
        resolvePipelineSettings.mSampleQuality = 0;
        resolvePipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        resolvePipelineSettings.pShaderProgram = pShaderResolve[gAppSettings.mMsaaIndex];
        pipelineDesc.pName = "MSAA Resolve - Final";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineResolve);

        pipelineDesc.pName = "MSAA Resolve - GodRay";
        resolvePipelineSettings.pColorFormats = &pRenderTargetGodRay[0]->mFormat;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineResolveGodRay);

        /************************************************************************/
        // Setup MSAA edge detect pipeline
        /************************************************************************/
        if (isMSAAEnabled)
        {
            pipelineDesc = {};
            PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
            pipelineDesc.pCache = pPipelineCache;
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& msaaEdgesPipeline = pipelineDesc.mGraphicsDesc;
            msaaEdgesPipeline = { 0 };
            msaaEdgesPipeline.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            msaaEdgesPipeline.mRenderTargetCount = 0;
            msaaEdgesPipeline.pDepthState = &depthStateOnlyWriteStencilDesc;
            msaaEdgesPipeline.mDepthStencilFormat = pMSAAEdgesStencilBuffer->mFormat;
            msaaEdgesPipeline.mSampleCount = gAppSettings.mMsaaLevel;
            msaaEdgesPipeline.mSampleQuality = 0;
            msaaEdgesPipeline.pRasterizerState = &rasterizerStateCullNoneMsDesc;
            msaaEdgesPipeline.pShaderProgram = pShaderDrawMSAAEdges[gAppSettings.mMsaaIndex - 1];
            pipelineDesc.pName = "Render Stencil MSAA edges";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineDrawMSAAEdges);

            // Downscale pipeline
            msaaEdgesPipeline.pShaderProgram = pShaderDownscaleMSAAEdges[gAppSettings.mMsaaIndex - 1];
            pipelineDesc.pName = "Downscale Stencil MSAA edges";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineDownscaleMSAAEdges);

            /************************************************************************/
            // Setup MSAA debug view pipeline
            /************************************************************************/
            pipelineDesc = {};
            PIPELINE_LAYOUT_DESC(pipelineDesc, NULL, NULL, NULL, SRT_LAYOUT_DESC(SrtResolve, PerDraw));
            pipelineDesc.pCache = pPipelineCache;
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& msaaDebugViewPipeline = pipelineDesc.mGraphicsDesc;
            msaaDebugViewPipeline = { 0 };
            msaaDebugViewPipeline.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            msaaDebugViewPipeline.mRenderTargetCount = 1;
            TinyImageFormat targetFormat = gDebugRTFormat;
            msaaDebugViewPipeline.pColorFormats = &targetFormat;
            msaaDebugViewPipeline.pDepthState = &depthStateDisableDesc;
            msaaDebugViewPipeline.mSampleCount = SAMPLE_COUNT_1;
            msaaDebugViewPipeline.mSampleQuality = 0;
            msaaDebugViewPipeline.pRasterizerState = &rasterizerStateCullNoneDesc;
            msaaDebugViewPipeline.pShaderProgram = pShaderDebugMSAA[gAppSettings.mMsaaIndex - 1];
            pipelineDesc.pName = "Render MSAA Debug Shader";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineDebugMSAA);
        }
    }

    void removePipelines()
    {
        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            removePipeline(pRenderer, pPipelineDebugMSAA);
            removePipeline(pRenderer, pPipelineDownscaleMSAAEdges);
            removePipeline(pRenderer, pPipelineDrawMSAAEdges);
        }
        removePipeline(pRenderer, pPipelineResolve);
        removePipeline(pRenderer, pPipelineResolveGodRay);

        removePipeline(pRenderer, pPipelineGodRayPass);
        removePipeline(pRenderer, pPipelineGodRayBlurPass);

        removePipeline(pRenderer, pPipelinePresentPass);

        for (uint32_t i = 0; i < 2; ++i)
        {
            removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
        }

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
            removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
            removePipeline(pRenderer, pPipelineShadowPass[i]);

        // Destroy triangle filtering pipelines
        removePipeline(pRenderer, pPipelineClusterLights);
        removePipeline(pRenderer, pPipelineClearLightClusters);
        removePipeline(pRenderer, pPipelineTriangleFiltering);
        removePipeline(pRenderer, pPipelineClearBuffers);
    }

    // This method sets the contents of the buffers to indicate the rendering pass that
    // the whole scene triangles must be rendered (no cluster / triangle filtering).
    // This is useful for testing purposes to compare visual / performance results.
    void addTriangleFilteringBuffers(Scene* pScene)
    {
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

        ubDesc.mDesc.mSize = sizeof(PerFrameVBConstantsData);
        ubDesc.mDesc.pName = "gPerFrameVBConstants Uniform Buffer Desc";
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
        lightClustersCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
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
        lightClustersDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
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
        skyDataBufferDesc.mDesc.mSize = sizeof(UniformCameraSkyData);
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
        /************************************************************************/
        /************************************************************************/
        waitForAllResourceLoads();

        tf_free(meshConstants);
    }

    void removeTriangleFilteringBuffers()
    {
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

        mat4         cameraModel = mat4::scale(vec3(SCENE_SCALE));
        CameraMatrix cameraView = pCameraController->getViewMatrix();

        CameraMatrix cameraProj =
            CameraMatrix::perspectiveReverseZ(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

        // Compute light matrices
        vec3 lightSourcePos(10.0f, 000.0f, 90.0f);

        // directional light rotation & translation
        mat4 rotation = mat4::rotationXY(gAppSettings.mSunControl.x, gAppSettings.mSunControl.y);
        vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));
        lightSourcePos += -160.0f * normalize(lightDir.getXYZ());
        mat4 translation = mat4::translation(-lightSourcePos);

        mat4         lightModel = mat4::scale(vec3(SCENE_SCALE));
        mat4         lightView = rotation * translation;
        CameraMatrix lightProj = CameraMatrix::orthographicReverseZ(-120.0f, 120.0f, -190.0f, 70.0f, 0.1f, 260.0f);

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
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp = lightProj * lightView;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp);
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].projection = lightProj;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].mvp =
            currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp * lightModel;

        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp = cameraProj * cameraView;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp);
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].projection = cameraProj;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].mvp =
            currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp * cameraModel;
#ifdef QUEST_VR
        currentFrame->gPerFrameVBUniformData.cullingMVP[VIEW_SHADOW] =
            currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].mvp.mCamera;
        mat4 superFrustumView;
        mat4 superFrustumProject;
        CameraMatrix::superFrustumReverseZ(cameraView, gAppSettings.nearPlane, gAppSettings.farPlane, superFrustumView,
                                           superFrustumProject);
        currentFrame->gPerFrameVBUniformData.cullingMVP[VIEW_CAMERA] = superFrustumProject * superFrustumView * cameraModel;
#endif
        /************************************************************************/
        // Culling data
        /************************************************************************/
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_SHADOW].sampleCount = 1;
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)gShadowMapSize, (float)gShadowMapSize };

        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_CAMERA].sampleCount = gAppSettings.mMsaaLevel;
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

        // Cache eye position in object space for cluster culling on the CPU
        currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
        currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
            (inverse(cameraView.mCamera * cameraModel) * vec4(0, 0, 0, 1)).getXYZ(); // vec4(0,0,0,1) is the camera position in eye space
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
        currentFrame->gPerFrameUniformData.depthTexSize = { (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight };
        currentFrame->gPerFrameUniformData.godrayAttenuation = gAppSettings.mGodrayAttenuation;
        currentFrame->gPerFrameUniformData.nearPlane = gAppSettings.nearPlane;
        currentFrame->gPerFrameUniformData.farPlane = gAppSettings.farPlane;

        /************************************************************************/
        // Skybox
        /************************************************************************/
        cameraView.setTranslation(vec3(0));
        currentFrame->gUniformDataSky.invProjViewOrigin = CameraMatrix::inverse(cameraProj * cameraView);

        /************************************************************************/
        // Tonemap
        /************************************************************************/

        currentFrame->gPerFrameUniformData.mLinearScale = gAppSettings.LinearScale / 10000.0f;
        currentFrame->gPerFrameUniformData.mOutputMode = (uint)gAppSettings.mOutputMode;
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
    // Render the shadow mapping pass. This pass updates the shadow map texture
    void drawShadowMapPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        // Start render pass and apply load actions
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pRenderTargetShadow, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mWidth, (float)pRenderTargetShadow->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mWidth, pRenderTargetShadow->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + VIEW_SHADOW];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

        const char* profileNames[gNumGeomSets] = { "SM Opaque", "SM Alpha" };
        for (uint32_t geomSet = 0; geomSet < NUM_GEOMETRY_SETS; ++geomSet)
        {
            cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[geomSet]);
            cmdBindPipeline(cmd, pPipelineShadowPass[geomSet]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_SHADOW, geomSet, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    // Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
    // into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
    // to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
    // less memory bandwidth is used.
    void drawVisibilityBufferPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        // Start render pass and apply load actions
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetVBPass, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

        const char* profileNames[gNumGeomSets] = { "VB Opaque", "VB Alpha" };
        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

            cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd)
    {
        const bool    isDrawingToMSRT = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1;
        RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

        if (isDrawingToMSRT)
        {
            RenderTargetBarrier barriers[] = {
                { pMSAAEdgesStencilBuffer, gAppSettings.mEnableGodray ? RESOURCE_STATE_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_DEPTH_WRITE,
                  RESOURCE_STATE_DEPTH_READ }
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
        }

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pDestinationRenderTarget, LOAD_ACTION_CLEAR };
        if (isDrawingToMSRT)
        {
            bindRenderTargets.mDepthStencil = { pMSAAEdgesStencilBuffer, LOAD_ACTION_DONTCARE, LOAD_ACTION_LOAD, STORE_ACTION_DONTCARE,
                                                STORE_ACTION_NONE };
        }
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[gAppSettings.mEnableAO]);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using
        // vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    void resolveMSAA(Cmd* cmd, uint32_t frameIdx, RenderTarget* msaaRT, RenderTarget* destRT)
    {
        // transition world render target to be used as input texture in post process pass
        RenderTargetBarrier barrier = { msaaRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { destRT, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)destRT->mWidth, (float)destRT->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, destRT->mWidth, destRT->mHeight);
        cmdBindPipeline(cmd, pPipelineResolve);
        cmdBindDescriptorSet(cmd, frameIdx * gDataBufferCount + gResolveFinalPassIndex, pDescriptorSetResolve);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    // Executes a compute shader to clear (reset) the the light clusters on the GPU
    void clearLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClearLightClusters);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLights);
        cmdDispatch(cmd, 1, 1, 1);
    }

    // Executes a compute shader that computes the light clusters on the GPU
    void computeLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClusterLights);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLights);
        cmdDispatch(cmd, LIGHT_COUNT, 1, 1);
    }

    void drawMSAAEdgesStencil(Cmd* cmd)
    {
        // This depth-only pass will render to the stencil buffer. Samples that must be shaded in
        // future shading or post-processing passes will be set to 0x01.
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Edges Stencil Pass");

        RenderTargetBarrier barriers[] = {
            { pMSAAEdgesStencilBuffer, RESOURCE_STATE_DEPTH_READ, RESOURCE_STATE_DEPTH_WRITE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pMSAAEdgesStencilBuffer, LOAD_ACTION_DONTCARE, LOAD_ACTION_CLEAR, STORE_ACTION_DONTCARE,
                                            STORE_ACTION_STORE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pMSAAEdgesStencilBuffer->mWidth, (float)pMSAAEdgesStencilBuffer->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pMSAAEdgesStencilBuffer->mWidth, pMSAAEdgesStencilBuffer->mHeight);

        cmdBindPipeline(cmd, pPipelineDrawMSAAEdges);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void downscaleMSAAEdgesStencil(Cmd* cmd)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Edges Stencil Downscale Pass");

        RenderTargetBarrier barriers[] = { {
                                               pMSAAEdgesStencilBuffer,
                                               RESOURCE_STATE_DEPTH_WRITE,
                                               RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           },
                                           { pDownscaledMSAAEdgesStencilBuffer, RESOURCE_STATE_DEPTH_READ, RESOURCE_STATE_DEPTH_WRITE } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pDownscaledMSAAEdgesStencilBuffer, LOAD_ACTION_DONTCARE, LOAD_ACTION_CLEAR,
                                            STORE_ACTION_DONTCARE, STORE_ACTION_STORE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDownscaledMSAAEdgesStencilBuffer->mWidth, (float)pDownscaledMSAAEdgesStencilBuffer->mHeight,
                       0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDownscaledMSAAEdgesStencilBuffer->mWidth, pDownscaledMSAAEdgesStencilBuffer->mHeight);

        cmdBindPipeline(cmd, pPipelineDownscaleMSAAEdges);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    // This is the main scene rendering function. It shows the different steps / rendering passes.
    void drawScene(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Shadow Pass");

        RenderTargetBarrier rtBarriers[] = { { pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                                             { pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

        drawShadowMapPass(cmd, gGraphicsProfileToken, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Filling Pass");
        drawVisibilityBufferPass(cmd, gGraphicsProfileToken, frameIdx);
        RenderTargetBarrier barriers[] = {
            { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            { pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, barriers);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            drawMSAAEdgesStencil(cmd);
        }

        // Draw GodRay after Initial VB Pass
        // Accumulate in shade pass...
        if (gAppSettings.mEnableGodray)
        {
            if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
            {
                downscaleMSAAEdgesStencil(cmd);
            }
            drawGodray(cmd);
            if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
            {
                resolveGodRay(cmd, frameIdx);
            }
            blurGodRay(cmd, frameIdx);
        }

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Shading Pass");

        drawVisibilityBufferShade(cmd);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        if (gAppSettings.mMsaaLevel > 1)
        {
            // Pixel Puzzle needs the unresolved MSAA texture
            cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Resolve Pass");
            resolveMSAA(cmd, frameIdx, pRenderTargetMSAA, pScreenRenderTarget);
            cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
        }
    }

    void drawGodray(Cmd* cmd)
    {
        const bool isMSAAEnabled = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1;

        RenderTarget* pGodRayActiveRenderTarget = isMSAAEnabled ? pRenderTargetGodRayMS : pRenderTargetGodRay[0];

        RenderTargetBarrier barrier[] = { { pGodRayActiveRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                                          { pDownscaledMSAAEdgesStencilBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_DEPTH_READ } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, isMSAAEnabled ? 2 : 1, barrier);

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pGodRayActiveRenderTarget, LOAD_ACTION_CLEAR };
        if (isMSAAEnabled)
        {
            bindRenderTargets.mDepthStencil = { pDownscaledMSAAEdgesStencilBuffer, LOAD_ACTION_DONTCARE, LOAD_ACTION_LOAD,
                                                STORE_ACTION_DONTCARE, STORE_ACTION_NONE };
        }
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pGodRayActiveRenderTarget->mWidth, (float)pGodRayActiveRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pGodRayActiveRenderTarget->mWidth, pGodRayActiveRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineGodRayPass);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void resolveGodRay(Cmd* cmd, uint frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray Resolve");

        RenderTarget* pResolveTarget = pRenderTargetGodRay[0];

        RenderTargetBarrier renderTargetBarrier[2];
        renderTargetBarrier[0] = { pRenderTargetGodRayMS, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        renderTargetBarrier[1] = { pResolveTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pResolveTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pResolveTarget->mWidth, (float)pResolveTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pResolveTarget->mWidth, pResolveTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineResolveGodRay);
        cmdBindDescriptorSet(cmd, frameIdx * gDataBufferCount + gResolveGodRayPassIndex, pDescriptorSetResolve);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

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
        renderTargetBarrier[0] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        renderTargetBarrier[1] = { pRenderTargetGodRay[0], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        cmdBindPipeline(cmd, pPipelineGodRayBlurPass);

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
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void presentImage(Cmd* const cmd, RenderTarget* pSrc, RenderTarget* pDstCol)
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
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawMSAADebugRenderTargets(Cmd* const cmd, uint32_t frameIdx, uint32_t sourceRTIndex, const char* pPassName,
                                    RenderTarget* targetRT)
    {
        char pFullPassName[256];
        snprintf(pFullPassName, TF_ARRAY_COUNT(pFullPassName), "Draw MSAA Debug RTs - %s", pPassName);
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, pFullPassName);

        RenderTargetBarrier barrier[] = { { targetRT, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { targetRT, LOAD_ACTION_CLEAR };

        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)targetRT->mWidth, (float)targetRT->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, targetRT->mWidth, targetRT->mHeight);

        cmdBindPipeline(cmd, pPipelineDebugMSAA);
        // We can re-use the resolve descriptor set here for code simplicity
        cmdBindDescriptorSet(cmd, frameIdx * gDataBufferCount + sourceRTIndex, pDescriptorSetResolve);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        // Encapsulate all debugging behavior here, waiting for rendering to finish here isn't optimal
        // but results in cleaner code.
        barrier[0] = { targetRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    // Draw GUI / 2D elements
    void drawGUI(Cmd* cmd, uint32_t frameIdx)
    {
        UNREF_PARAM(frameIdx);

        RenderTarget* rt = pSwapChain->ppRenderTargets[frameIdx];
        cmdBeginDrawingUserInterface(cmd, pSwapChain, rt);
        {
            gFrameTimeDraw.mFontColor = gAppSettings.mVisualizeAO ? 0xff000000 : 0xff00ffff;
            gFrameTimeDraw.mFontSize = 18.0f;
            gFrameTimeDraw.mFontID = gFontID;
            cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

            if (gAppSettings.mAsyncCompute)
            {
                if (!gAppSettings.mHoldFilteredResults)
                {
                    cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gComputeProfileToken, &gFrameTimeDraw);
                    cmdDrawGpuProfile(cmd, float2(8.0f, 425.0f), gGraphicsProfileToken, &gFrameTimeDraw);
                }
                else
                {
                    cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGraphicsProfileToken, &gFrameTimeDraw);
                }
            }
            else
            {
                cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGraphicsProfileToken, &gFrameTimeDraw);
            }

            cmdDrawUserInterface(cmd);
        }
        cmdEndDrawingUserInterface(cmd, pSwapChain);
    }
};

DEFINE_APPLICATION_MAIN(Visibility_Buffer)
