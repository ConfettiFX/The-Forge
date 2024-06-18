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

#include "../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../Common_3/Application/Interfaces/IInput.h"
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

#include "Shaders/FSL/shader_defs.h.fsl"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)  \
    X(BindlessSupported, 1) \
    X(DisableAO, 0)         \
    X(DisableGodRays, 0)    \
    X(MSAASampleCount, 2)   \
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

typedef enum LightingMode
{
    LIGHTING_PHONG = 0,
    LIGHTING_PBR = 1,

    LIGHTINGMODE_COUNT = 2
} LightingMode;

struct UniformShadingData
{
    float4 lightColor;
    uint   lightingMode;
    uint   outputMode;
    float4 CameraPlane; // x : near, y : far
};

struct SCurveInfo
{
    float C1;
    float C2;
    float C3;
    float UseSCurve;

    float ScurveSlope;
    float ScurveScale;
    float linearScale;
    float pad0;

    uint outputMode;
};

SCurveInfo gSCurveInfomation;

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

typedef enum CurveConversionMode
{
    CurveConversion_LinearScale = 0,
    CurveConversion_SCurve = 1
} CurveConversionMode;

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
float3       gCameraPathData[29084];

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

    LightingMode mLightingMode = LIGHTING_PBR;

    // Set this variable to true to bypass triangle filtering calculations, holding and representing the last filtered data.
    // This is useful for inspecting filtered geometry for debugging purposes.
    bool mHoldFilteredResults = false;

    bool mAsyncCompute = DEFAULT_ASYNC_COMPUTE;
    // toggle rendering of local point lights
    bool mRenderLocalLights = false;

    bool mDrawDebugTargets = false;

    float nearPlane = 10.0f;
    float farPlane = 3000.0f;

    // adjust directional sunlight angle
    float2 mSunControl = { -2.1f, 0.164f };

    float mSunSize = 300.0f;

    float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

    DynamicUIWidgets mDynamicUIWidgetsGR;
    bool             mEnableGodray = true;
    uint32_t         mFilterRadius = 3;

    float mEsmControl = 80.0f;

    float mRetinaScaling = 1.5f;

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

    SampleCount mMsaaLevel = SAMPLE_COUNT_2;
    uint32_t    mMsaaIndex = (uint32_t)log2((uint32_t)mMsaaLevel);
    uint32_t    mMsaaIndexRequested = mMsaaIndex;

    CurveConversionMode mCurveConversionMode = CurveConversion_LinearScale;

    // Camera Walking
    bool  cameraWalking = false;
    float cameraWalkingSpeed = 1.0f;

} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/
const char* gSceneName = "SanMiguel.bin";

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Constants
const uint32_t gShadowMapSize = 1024;
const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

struct UniformDataSkybox
{
    CameraMatrix mProjectView;
    vec3         mCamPos;
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
    vec3                gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    PerFrameConstants   gPerFrameUniformData = {};
    PerFrameVBConstants gPerFrameVBUniformData = {};
    UniformDataSkybox   gUniformDataSky;
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

Queue*            pComputeQueue = NULL;
GpuCmdRing        gComputeCmdRing = {};
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain*        pSwapChain = NULL;
Semaphore*        pImageAcquiredSemaphore = NULL;
Semaphore*        pPresentSemaphore = NULL;
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*           pShaderClearBuffers = nullptr;
Pipeline*         pPipelineClearBuffers = nullptr;
RootSignature*    pRootSignatureClearBuffers = NULL;
DescriptorSet*    pDescriptorSetClearBuffers = NULL;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*           pShaderTriangleFiltering = nullptr;
Pipeline*         pPipelineTriangleFiltering = nullptr;
RootSignature*    pRootSignatureTriangleFiltering = nullptr;
DescriptorSet*    pDescriptorSetTriangleFiltering[2] = { NULL };
/************************************************************************/
// Clear light clusters pipeline
/************************************************************************/
Shader*           pShaderClearLightClusters = nullptr;
Pipeline*         pPipelineClearLightClusters = nullptr;
RootSignature*    pRootSignatureLightClusters = nullptr;
DescriptorSet*    pDescriptorSetLightClusters[2] = { NULL };
/************************************************************************/
// Compute light clusters pipeline
/************************************************************************/
Shader*           pShaderClusterLights = nullptr;
Pipeline*         pPipelineClusterLights = nullptr;
/************************************************************************/
// Shadow pass pipeline
/************************************************************************/
Shader*           pShaderShadowPass[gNumGeomSets] = { NULL };
Pipeline*         pPipelineShadowPass[gNumGeomSets] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferPass[gNumGeomSets] = {};
Pipeline*         pPipelineVisibilityBufferPass[gNumGeomSets] = {};
RootSignature*    pRootSignatureVBPass = nullptr;
CommandSignature* pCmdSignatureVBPass = nullptr;
DescriptorSet*    pDescriptorSetVBPass[2] = { NULL };
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferShade[2 * MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
RootSignature*    pRootSignatureVBShade = nullptr;
DescriptorSet*    pDescriptorSetVBShade[2] = { NULL };
/************************************************************************/
// Resolve pipeline
/************************************************************************/
Shader*           pShaderResolve[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineResolve = nullptr;
Pipeline*         pPipelineResolvePost = nullptr;
RootSignature*    pRootSignatureResolve = nullptr;
DescriptorSet*    pDescriptorSetResolve = { NULL };
/************************************************************************/
// Skybox pipeline
/************************************************************************/
Shader*           pShaderSkybox = nullptr;
Pipeline*         pSkyboxPipeline = nullptr;
RootSignature*    pRootSingatureSkybox = nullptr;

Buffer*        pSkyboxVertexBuffer = NULL;
Texture*       pSkybox = NULL;
DescriptorSet* pDescriptorSetSkybox[2] = { NULL };
/************************************************************************/
// Godray pipeline
/************************************************************************/
Shader*        pGodRayPass[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*      pPipelineGodRayPass = nullptr;
RootSignature* pRootSigGodRayPass = nullptr;
DescriptorSet* pDescriptorSetGodRayPass = NULL;
DescriptorSet* pDescriptorSetGodRayPassPerFrame = NULL;
Buffer*        pBufferGodRayConstant = nullptr;
uint32_t       gGodRayConstantIndex = 0;

Shader*        pShaderGodRayBlurPass = nullptr;
Pipeline*      pPipelineGodRayBlurPass = nullptr;
RootSignature* pRootSignatureGodRayBlurPass = nullptr;
DescriptorSet* pDescriptorSetGodRayBlurPass = nullptr;
Buffer*        pBufferBlurWeights = nullptr;
Buffer*        pBufferGodRayBlurConstant = nullptr;
uint32_t       gGodRayBlurConstantIndex = 0;
/************************************************************************/
// Curve Conversion pipeline
/************************************************************************/
Shader*        pShaderCurveConversion = nullptr;
Pipeline*      pPipelineCurveConversionPass = nullptr;
RootSignature* pRootSigCurveConversionPass = nullptr;
DescriptorSet* pDescriptorSetCurveConversionPass = NULL;

OutputMode         gWasOutputMode = gAppSettings.mOutputMode;
DisplayColorSpace  gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
DisplayColorRange  gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
DisplaySignalRange gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;

/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*        pShaderPresentPass = nullptr;
Pipeline*      pPipelinePresentPass = nullptr;
RootSignature* pRootSigPresentPass = nullptr;
DescriptorSet* pDescriptorSetPresentPass = { NULL };
uint32_t       gSCurveRootConstantIndex = 0;
/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget*  pDepthBuffer = NULL;
RenderTarget*  pRenderTargetVBPass = NULL;
RenderTarget*  pRenderTargetMSAA = NULL;
RenderTarget*  pRenderTargetShadow = NULL;
RenderTarget*  pIntermediateRenderTarget = NULL;
RenderTarget*  pRenderTargetGodRay[2] = { NULL };
RenderTarget*  pCurveConversionRenderTarget = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler*       pSamplerTrilinearAniso = NULL;
Sampler*       pSamplerBilinear = NULL;
Sampler*       pSamplerPointClamp = NULL;
Sampler*       pSamplerBilinearClamp = NULL;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture**      gDiffuseMapsStorage = NULL;
Texture**      gNormalMapsStorage = NULL;
Texture**      gSpecularMapsStorage = NULL;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
Geometry*      pGeom = NULL;
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer*        pPerFrameUniformBuffers[gDataBufferCount] = { NULL };
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
        uiCreateComponent("DEBUG RTs", &UIComponentDesc, &pDebugTexturesWindow);

        DebugTexturesWidget widget;
        luaRegisterWidget(uiCreateComponentWidget(pDebugTexturesWindow, "Debug RTs", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
    }

    static const Texture* pVBRTs[6];
    uint32_t              textureCount = 0;

    pVBRTs[textureCount++] = pRenderTargetShadow->pTexture;

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

        UIWidget* pControl = uiCreateComponentWidget(pUIManager, "Screen Resolution", &control, WIDGET_TYPE_DROPDOWN);
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
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_PIPELINE_CACHE, "PipelineCaches");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        threadSystemInit(&gThreadSystem, &gThreadSystemInitDescDefault);

        // Camera Walking
        FileStream fh = {};
        if (fsOpenStreamFromPath(RD_OTHER_FILES, "cameraPath.bin", FM_READ, &fh))
        {
            fsReadFromStream(&fh, gCameraPathData, sizeof(float3) * 29084);
            fsCloseStream(&fh);
        }

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
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        if (!gGpuSettings.mBindlessSupported)
        {
            ShowUnsupportedMessage("Visibility Buffer does not run on this device. GPU does not support enough bindless texture entries");
            return false;
        }

        if (!pRenderer->pGpu->mSettings.mPrimitiveIdSupported)
        {
            ShowUnsupportedMessage("Visibility Buffer does not run on this device. PrimitiveID is not supported");
            return false;
        }

        // turn off by default depending on gpu config rules
        gAppSettings.mEnableGodray &= !gGpuSettings.mDisableGodRays;
        gAppSettings.mEnableAO &= !gGpuSettings.mDisableAO;
        gAppSettings.mMsaaLevel = (SampleCount)min(1u, gGpuSettings.mMSAASampleCount);
        gAppSettings.mMsaaIndex = (uint32_t)log2((uint32_t)gAppSettings.mMsaaLevel);
        gAppSettings.mMsaaIndexRequested = gAppSettings.mMsaaIndex;

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        QueueDesc computeQueueDesc = {};
        computeQueueDesc.mType = QUEUE_TYPE_COMPUTE;
        computeQueueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        addQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);
        addSemaphore(pRenderer, &pPresentSemaphore);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        cmdRingDesc = {};
        cmdRingDesc.pQueue = pComputeQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gComputeCmdRing);
        /************************************************************************/
        // Initialize helper interfaces (resource loader, profiler)
        /************************************************************************/
        initResourceLoaderInterface(pRenderer);

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

        UIComponentDesc UIComponentDesc = {};
        UIComponentDesc.mStartPosition = vec2(225.0f, 100.0f);
        uiCreateComponent(GetName(), &UIComponentDesc, &pGuiWindow);
        uiSetComponentFlags(pGuiWindow, GUI_COMPONENT_FLAGS_NO_RESIZE);

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = "circlepad.tex";
        if (!initInputSystem(&inputDesc))
            return false;
        /************************************************************************/
        /************************************************************************/
        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        gGraphicsProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gComputeProfileToken = addGpuProfiler(pRenderer, pComputeQueue, "Compute");
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
        godrayBlurConstantBufferDesc.pData = &gGodRayBlurConstant;
        godrayBlurConstantBufferDesc.ppBuffer = &pBufferGodRayBlurConstant;
        addResource(&godrayBlurConstantBufferDesc, NULL);

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
        skyboxTriDesc.pFileName = "daytime_cube.tex";
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

        const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

        DropdownWidget ddTestScripts;
        ddTestScripts.pData = &gCurrentScriptIndex;
        ddTestScripts.pNames = gTestScripts;
        ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

        ButtonWidget bRunScript;
        UIWidget*    pRunScript = uiCreateComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
        luaRegisterWidget(pRunScript);

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

        gAppSettings.mHoldFilteredResults = false;

        CheckboxWidget checkbox;
        checkbox.pData = &gAppSettings.mHoldFilteredResults;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Hold filtered results", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.mAsyncCompute;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Async Compute", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.mDrawDebugTargets;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw Debug Targets", &checkbox, WIDGET_TYPE_CHECKBOX));
        /************************************************************************/
        /************************************************************************/
        if (pRenderer->pGpu->mSettings.mHDRSupported)
        {
            LabelWidget labelWidget = {};
            pOutputSupportsHDRWidget = uiCreateComponentWidget(pGuiWindow, "Output Supports HDR", &labelWidget, WIDGET_TYPE_LABEL);
            REGISTER_LUA_WIDGET(pOutputSupportsHDRWidget);

            static const char* outputModeNames[] = { "SDR", "HDR10" };

            DropdownWidget outputMode;
            outputMode.pData = (uint32_t*)&gAppSettings.mOutputMode;
            outputMode.pNames = outputModeNames;
            outputMode.mCount = sizeof(outputModeNames) / sizeof(outputModeNames[0]);
            luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Output Mode", &outputMode, WIDGET_TYPE_DROPDOWN));
        }

        static const char* lightingModeNames[] = { "Phong", "Physically Based Rendering" };

        DropdownWidget lightingMode;
        lightingMode.pData = (uint32_t*)&gAppSettings.mLightingMode;
        lightingMode.pNames = lightingModeNames;
        lightingMode.mCount = sizeof(lightingModeNames) / sizeof(lightingModeNames[0]);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Lighting Mode", &lightingMode, WIDGET_TYPE_DROPDOWN));

        checkbox.pData = &gAppSettings.cameraWalking;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking", &checkbox, WIDGET_TYPE_CHECKBOX));

        SliderFloatWidget cameraSpeedProp;
        cameraSpeedProp.pData = &gAppSettings.cameraWalkingSpeed;
        cameraSpeedProp.mMin = 0.0f;
        cameraSpeedProp.mMax = 3.0f;
        luaRegisterWidget(
            uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraSpeedProp, WIDGET_TYPE_SLIDER_FLOAT));

        // Light Settings
        //---------------------------------------------------------------------------------
        // offset max angle for sun control so the light won't bleed with
        // small glancing angles, i.e., when lightDir is almost parallel to the plane

        SliderFloat2Widget sunX;
        sunX.pData = &gAppSettings.mSunControl;
        sunX.mMin = float2(-PI);
        sunX.mMax = float2(PI);
        sunX.mStep = float2(0.001f);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Sun Control", &sunX, WIDGET_TYPE_SLIDER_FLOAT2));

        SliderFloat4Widget lightColorUI;
        lightColorUI.pData = &gAppSettings.mLightColor;
        lightColorUI.mMin = float4(0.0f);
        lightColorUI.mMax = float4(30.0f);
        lightColorUI.mStep = float4(0.01f);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Color & Intensity", &lightColorUI, WIDGET_TYPE_SLIDER_FLOAT4));

        checkbox.pData = &gAppSettings.mEnableGodray;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Godray", &checkbox, WIDGET_TYPE_CHECKBOX));

        SliderFloatWidget sliderFloat;
        sliderFloat.pData = &gAppSettings.mSunSize;
        sliderFloat.mMin = 1.0f;
        sliderFloat.mMax = 1000.0f;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Sun Size", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gGodRayConstant.mScatterFactor;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 1.0f;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray: Scatter Factor", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        SliderUintWidget sliderUint;
        sliderUint.pData = &gAppSettings.mFilterRadius;
        sliderUint.mMin = 1u;
        sliderUint.mMax = 8u;
        sliderUint.mStep = 1u;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Gaussian Blur Kernel Size", &sliderUint,
                               WIDGET_TYPE_SLIDER_UINT);

        sliderFloat.pData = &gGaussianBlurSigma[0];
        sliderFloat.mMin = 0.1f;
        sliderFloat.mMax = 5.0f;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Gaussian Blur Sigma", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        if (gAppSettings.mEnableGodray)
            uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, pGuiWindow);

        // SliderFloatWidget esm("Shadow Control", &gAppSettings.mEsmControl, 0, 200.0f);
        // pGuiWindow->AddWidget(esm);

        checkbox.pData = &gAppSettings.mRenderLocalLights;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Random Point Lights", &checkbox, WIDGET_TYPE_CHECKBOX));

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
        ddMSAA.mCount = sizeof(msaaSampleNames) / sizeof(msaaSampleNames[0]);

        UIWidget* msaaWidget = uiCreateComponentWidget(pGuiWindow, "MSAA", &ddMSAA, WIDGET_TYPE_DROPDOWN);
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
        uiCreateDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Color Range", &ddColor, WIDGET_TYPE_DROPDOWN);

        DropdownWidget ddRange;
        ddRange.pData = (uint32_t*)&gAppSettings.mDisplaySignalRange;
        ddRange.pNames = displaySignalRangeNames;
        ddRange.mCount = sizeof(displaySignalRangeNames) / sizeof(displaySignalRangeNames[0]);
        uiCreateDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Signal Range", &ddRange, WIDGET_TYPE_DROPDOWN);

        DropdownWidget ddSpace;
        ddSpace.pData = (uint32_t*)&gAppSettings.mCurrentSwapChainColorSpace;
        ddSpace.pNames = displayColorSpaceNames;
        ddSpace.mCount = sizeof(displayColorSpaceNames) / sizeof(displayColorSpaceNames[0]);
        uiCreateDynamicWidgets(&gAppSettings.mDisplaySetting, "Display Color Space", &ddSpace, WIDGET_TYPE_DROPDOWN);

        /************************************************************************/
        // AO Settings
        /************************************************************************/
        checkbox.pData = &gAppSettings.mEnableAO;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable AO", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.mVisualizeAO;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "Visualize AO", &checkbox, WIDGET_TYPE_CHECKBOX);

        sliderFloat.pData = &gAppSettings.mAOIntensity;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 10.0f;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "AO Intensity", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        SliderIntWidget sliderInt;
        sliderInt.pData = &gAppSettings.mAOQuality;
        sliderInt.mMin = 1;
        sliderInt.mMax = 4;
        uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, "AO Quality", &sliderInt, WIDGET_TYPE_SLIDER_INT);

        if (gAppSettings.mEnableAO)
            uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO, pGuiWindow);

        static const char* curveConversionModeNames[] = { "Linear Scale", "Scurve" };

        DropdownWidget curveConversionMode;
        curveConversionMode.pData = (uint32_t*)&gAppSettings.mCurveConversionMode;
        curveConversionMode.pNames = curveConversionModeNames;
        curveConversionMode.mCount = sizeof(curveConversionModeNames) / sizeof(curveConversionModeNames[0]);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Curve Conversion", &curveConversionMode, WIDGET_TYPE_DROPDOWN));

        sliderFloat.pData = &gAppSettings.LinearScale;
        sliderFloat.mMin = 80.0f;
        sliderFloat.mMax = 400.0f;
        uiCreateDynamicWidgets(&gAppSettings.mLinearScale, "Linear Scale", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        if (gAppSettings.mCurveConversionMode == CurveConversion_LinearScale)
        {
            uiShowDynamicWidgets(&gAppSettings.mLinearScale, pGuiWindow);
        }

        sliderFloat.pData = &gAppSettings.SCurveScaleFactor;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 10.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: Scale Factor", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveSMin;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 2.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: SMin", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveSMid;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 20.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: SMid", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveSMax;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 100.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: SMax", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveTMin;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 10.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: TMin", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveTMid;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 300.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: TMid", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveTMax;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 4000.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: TMax", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        sliderFloat.pData = &gAppSettings.SCurveSlopeFactor;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 3.0f;
        uiCreateDynamicWidgets(&gAppSettings.mSCurve, "SCurve: Slope Factor", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

        if (gAppSettings.mOutputMode != OutputMode::OUTPUT_MODE_SDR && gAppSettings.mCurveConversionMode == CurveConversion_SCurve)
        {
            uiShowDynamicWidgets(&gAppSettings.mSCurve, pGuiWindow);
            gSCurveInfomation.UseSCurve = 1.0f;
        }
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
        vec3                   startPosition(600.0f, 490.0f, 70.0f);
        vec3                   startLookAt = startPosition + vec3(-1.0f - 0.0f, 0.1f, 0.0f);
        CameraMotionParameters camParams;
        camParams.acceleration = 1300 * 2.5f;
        camParams.braking = 1300 * 2.5f;
        camParams.maxSpeed = 200 * 2.5f;
        pCameraController = initFpsCameraController(startPosition, startLookAt);
        pCameraController->setMotionParameters(camParams);

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

        return true;
    }

    void Exit()
    {
        exitInputSystem();
        threadSystemExit(&gThreadSystem, &gThreadSystemExitDescDefault);

        exitCameraController(pCameraController);

        removeResource(pBufferGodRayConstant);

        removeResource(pBufferGodRayBlurConstant);
        removeResource(pBufferBlurWeights);

        removeResource(pSkybox);
        removeTriangleFilteringBuffers();
        uiDestroyDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
        uiDestroyDynamicWidgets(&gAppSettings.mDynamicUIWidgetsAO);
        uiDestroyDynamicWidgets(&gAppSettings.mLinearScale);
        uiDestroyDynamicWidgets(&gAppSettings.mSCurve);
        uiDestroyDynamicWidgets(&gAppSettings.mDisplaySetting);

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
        tf_free(pVBMeshInstances);

#if defined(_WINDOWS)
        arrfree(gGuiResolution.mResNameContainer);
        arrfree(gGuiResolution.mResNamePointers);
#endif
        /************************************************************************/
        /************************************************************************/
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeSemaphore(pRenderer, pPresentSemaphore);

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeGpuCmdRing(pRenderer, &gComputeCmdRing);

        removeQueue(pRenderer, pGraphicsQueue);
        removeQueue(pRenderer, pComputeQueue);

        removeSampler(pRenderer, pSamplerTrilinearAniso);
        removeSampler(pRenderer, pSamplerBilinear);
        removeSampler(pRenderer, pSamplerPointClamp);
        removeSampler(pRenderer, pSamplerBilinearClamp);

        PipelineCacheSaveDesc saveDesc = {};
        saveDesc.pFileName = pPipelineCacheName;
        savePipelineCache(pRenderer, pPipelineCache, &saveDesc);
        removePipelineCache(pRenderer, pPipelineCache);

        exitVisibilityBuffer(pVisibilityBuffer);

        exitResourceLoaderInterface(pRenderer);

        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    // Setup the render targets used in this demo.
    // The only render target that is being currently used stores the results of the Visibility Buffer pass.
    // As described earlier, this render target uses 32 bit per pixel to store draw / triangle IDs that will be
    // loaded later by the shade step to reconstruct interpolated triangle data per pixel.
    bool Load(ReloadDesc* pReloadDesc)
    {
        gFrameCount = 0;

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (pReloadDesc->mType & RELOAD_TYPE_RENDERTARGET)
            {
                if (gAppSettings.mMsaaIndex != gAppSettings.mMsaaIndexRequested)
                {
                    gAppSettings.mMsaaIndex = gAppSettings.mMsaaIndexRequested;
                    gAppSettings.mMsaaLevel = (SampleCount)(1 << gAppSettings.mMsaaIndex);
                }
            }

            if (!addSwapChain())
                return false;
            addRenderTargets();

            SetupDebugTexturesWindow();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
        uiLoad.mHeight = mSettings.mHeight;
        uiLoad.mWidth = mSettings.mWidth;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

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
            removeRenderTargets();

            if (pDebugTexturesWindow)
            {
                uiDestroyComponent(pDebugTexturesWindow);
                pDebugTexturesWindow = NULL;
            }

            ESRAM_RESET_ALLOCS(pRenderer);
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
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

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
            pCameraController->moveTo(f3Tov3(newPos));

            float3 newLookat = v3ToF3(lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]),
                                           f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
            pCameraController->lookAt(f3Tov3(newLookat));
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
            cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);

            TriangleFilteringPassDesc triangleFilteringDesc = {};
            triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
            triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

            triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetClearBuffers;
            triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetTriangleFiltering[0];
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetTriangleFiltering[1];

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

            cmdBeginGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);

            if (!gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredResults)
            {
                TriangleFilteringPassDesc triangleFilteringDesc = {};
                triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
                triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

                triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetClearBuffers;
                triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetTriangleFiltering[0];
                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetTriangleFiltering[1];

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
                { pRenderTargetMSAA, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
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
            drawSkybox(graphicsCmd, frameIdx);

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

            if (gAppSettings.mEnableGodray)
            {
                drawGodray(graphicsCmd, frameIdx);
                blurGodRay(graphicsCmd, frameIdx);
                drawColorconversion(graphicsCmd);
            }

#if !defined(QUEST_VR)
            cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "UI Pass");
            drawGUI(graphicsCmd, frameIdx);
            cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
#endif

            // Get the current render target for this frame
            acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
            presentImage(graphicsCmd, pScreenRenderTarget, pSwapChain->ppRenderTargets[presentIndex]);

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
        DescriptorSetDesc setDesc = { pRootSignatureClearBuffers, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClearBuffers);
        // Triangle Filtering
        setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);
        setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);
        // Light Clustering
        setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[0]);
        setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[1]);
        // VB, Shadow
        setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[0]);
        setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[1]);
        // VB Shade
        setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[0]);
        setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[1]);
        // Resolve
        setDesc = { pRootSignatureResolve, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetResolve);
        // God Ray
        setDesc = { pRootSigGodRayPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayPass);
        setDesc = { pRootSigGodRayPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayPassPerFrame);
        // God Ray Blur
        setDesc = { pRootSignatureGodRayBlurPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayBlurPass);
        // Curve Conversion
        setDesc = { pRootSigCurveConversionPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCurveConversionPass);
        // Sky
        setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
        setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
        // Present
        setDesc = { pRootSigPresentPass, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPresentPass);

        return true;
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetPresentPass);
        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetResolve);
        removeDescriptorSet(pRenderer, pDescriptorSetCurveConversionPass);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayBlurPass);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayPass);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayPassPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetVBShade[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBShade[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBPass[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBPass[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetLightClusters[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetLightClusters[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetClearBuffers);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[1]);
    }

    void prepareDescriptorSets()
    {
        // Clear Buffers
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                uint32_t       clearParamsCount = 0;
                DescriptorData clearParams[2] = {};
                clearParams[clearParamsCount].pName = "indirectDrawArgs";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                clearParams[clearParamsCount].pName = "VBConstantBuffer";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
                updateDescriptorSet(pRenderer, i, pDescriptorSetClearBuffers, clearParamsCount, clearParams);
            }
        }
        // Triangle Filtering
        {
            const uint32_t paramsCount = 4;
            DescriptorData filterParams[paramsCount] = {};
            filterParams[0].pName = "vertexPositionBuffer";
            filterParams[0].ppBuffers = &pGeom->pVertexBuffers[0];
            filterParams[1].pName = "indexDataBuffer";
            filterParams[1].ppBuffers = &pGeom->pIndexBuffer;
            filterParams[2].pName = "meshConstantsBuffer";
            filterParams[2].ppBuffers = &pMeshConstantsBuffer;
            filterParams[3].pName = "VBConstantBuffer";
            filterParams[3].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], paramsCount, filterParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData filterParamsPerFrame[5] = {};
                filterParamsPerFrame[0].pName = "filteredIndicesBuffer";
                filterParamsPerFrame[0].mCount = gNumViews;
                filterParamsPerFrame[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS];
                filterParamsPerFrame[1].pName = "indirectDrawArgs";
                filterParamsPerFrame[1].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                filterParamsPerFrame[2].pName = "PerFrameVBConstants";
                filterParamsPerFrame[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
                filterParamsPerFrame[3].pName = "filterDispatchGroupDataBuffer";
                filterParamsPerFrame[3].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
                filterParamsPerFrame[4].pName = "indirectDataBuffer";
                filterParamsPerFrame[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFiltering[1], 5, filterParamsPerFrame);
            }
        }
        // Light Clustering
        {
            DescriptorData params[3] = {};
            params[0].pName = "lights";
            params[0].ppBuffers = &pLightsBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetLightClusters[0], 1, params);
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                params[0].pName = "lightClustersCount";
                params[0].ppBuffers = &pLightClustersCount[i];
                params[1].pName = "lightClusters";
                params[1].ppBuffers = &pLightClusters[i];
                params[2].pName = "PerFrameVBConstants";
                params[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetLightClusters[1], 3, params);
            }
        }
        // VB, Shadow
        {
            DescriptorData params[3] = {};
            params[0].pName = "diffuseMaps";
            params[0].mCount = gMaterialCount;
            params[0].ppTextures = gDiffuseMapsStorage;
            params[1].pName = "vertexPositionBuffer";
            params[1].ppBuffers = &pGeom->pVertexBuffers[0];
            params[2].pName = "vertexTexCoordBuffer";
            params[2].ppBuffers = &pGeom->pVertexBuffers[1];
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 3, params);
            params[0] = {};
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                params[0].pName = "indirectDataBuffer";
                params[0].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                params[1].pName = "PerFrameVBConstants";
                params[1].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetVBPass[1], 2, params);
            }
        }
        // VB Shade
        {
            DescriptorData vbShadeParams[12] = {};
            DescriptorData vbShadeParamsPerFrame[6] = {};
            vbShadeParams[0].pName = "vbTex";
            vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
            vbShadeParams[1].pName = "diffuseMaps";
            vbShadeParams[1].mCount = gMaterialCount;
            vbShadeParams[1].ppTextures = gDiffuseMapsStorage;
            vbShadeParams[2].pName = "normalMaps";
            vbShadeParams[2].mCount = gMaterialCount;
            vbShadeParams[2].ppTextures = gNormalMapsStorage;
            vbShadeParams[3].pName = "specularMaps";
            vbShadeParams[3].mCount = gMaterialCount;
            vbShadeParams[3].ppTextures = gSpecularMapsStorage;
            vbShadeParams[4].pName = "vertexPos";
            vbShadeParams[4].ppBuffers = &pGeom->pVertexBuffers[0];
            vbShadeParams[5].pName = "vertexTexCoord";
            vbShadeParams[5].ppBuffers = &pGeom->pVertexBuffers[1];
            vbShadeParams[6].pName = "vertexNormal";
            vbShadeParams[6].ppBuffers = &pGeom->pVertexBuffers[2];
            vbShadeParams[7].pName = "lights";
            vbShadeParams[7].ppBuffers = &pLightsBuffer;
            vbShadeParams[8].pName = "depthTex";
            vbShadeParams[8].ppTextures = &pDepthBuffer->pTexture;
            vbShadeParams[9].pName = "shadowMap";
            vbShadeParams[9].ppTextures = &pRenderTargetShadow->pTexture;
            vbShadeParams[10].pName = "meshConstantsBuffer";
            vbShadeParams[10].ppBuffers = &pMeshConstantsBuffer;
            vbShadeParams[11].pName = "VBConstantBuffer";
            vbShadeParams[11].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 12, vbShadeParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                vbShadeParamsPerFrame[0].pName = "lightClustersCount";
                vbShadeParamsPerFrame[0].ppBuffers = &pLightClustersCount[i];
                vbShadeParamsPerFrame[1].pName = "lightClusters";
                vbShadeParamsPerFrame[1].ppBuffers = &pLightClusters[i];
                vbShadeParamsPerFrame[2].pName = "PerFrameConstants";
                vbShadeParamsPerFrame[2].ppBuffers = &pPerFrameUniformBuffers[i];
                vbShadeParamsPerFrame[3].pName = "PerFrameVBConstants";
                vbShadeParamsPerFrame[3].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                vbShadeParamsPerFrame[4].pName = "indirectDataBuffer";
                vbShadeParamsPerFrame[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                vbShadeParamsPerFrame[5].pName = "filteredIndexBuffer";
                vbShadeParamsPerFrame[5].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
                updateDescriptorSet(pRenderer, i, pDescriptorSetVBShade[1], 6, vbShadeParamsPerFrame);
            }
        }
        // Resolve
        {
            DescriptorData params[2] = {};
            params[0].pName = "msaaSource";
            params[0].ppTextures = &pRenderTargetMSAA->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetResolve, 1, params);
        }
        // God Ray
        {
            DescriptorData params[2] = {};
            params[0].pName = "depthTexture";
            params[0].ppTextures = &pDepthBuffer->pTexture;
            params[1].pName = "shadowMap";
            params[1].ppTextures = &pRenderTargetShadow->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetGodRayPass, 2, params);

            for (uint32_t i = 0; i < gDataBufferCount; i++)
            {
                DescriptorData GodrayParams[2] = {};
                GodrayParams[0].pName = "PerFrameConstants";
                GodrayParams[0].ppBuffers = &pPerFrameUniformBuffers[i];
                GodrayParams[1].pName = "PerFrameVBConstants";
                GodrayParams[1].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetGodRayPassPerFrame, 2, GodrayParams);
            }
        }
        // God Ray Blur
        {
            Texture*       textures[] = { pRenderTargetGodRay[0]->pTexture, pRenderTargetGodRay[1]->pTexture };
            DescriptorData params[2] = {};
            params[0].pName = "godrayTextures";
            params[0].ppTextures = textures;
            params[0].mCount = 2;
            params[1].pName = "BlurWeights";
            params[1].ppBuffers = &pBufferBlurWeights;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetGodRayBlurPass, 2, params);
        }
        // Curve conversion
        {
            DescriptorData CurveConversionParams[2] = {};
            CurveConversionParams[0].pName = "SceneTex";
            CurveConversionParams[0].ppTextures = &pIntermediateRenderTarget->pTexture;
            CurveConversionParams[1].pName = "GodRayTex";
            CurveConversionParams[1].ppTextures = &pRenderTargetGodRay[0]->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetCurveConversionPass, 2, CurveConversionParams);
        }
        // Sky
        {
            DescriptorData skyParams[1] = {};
            skyParams[0].pName = "skyboxTex";
            skyParams[0].ppTextures = &pSkybox;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                skyParams[0].pName = "UniformCameraSky";
                skyParams[0].ppBuffers = &pUniformBufferSky[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyParams);
            }
        }
        // Present
        {
            DescriptorData params[3] = {};
            params[0].pName = "uTex0";
            params[0].ppTextures = &pIntermediateRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetPresentPass, 1, params);
            params[0].ppTextures = &pCurveConversionRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, 1, pDescriptorSetPresentPass, 1, params);
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
        if (pRenderer->pGpu->mSettings.mHDRSupported)
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
        const uint32_t   width = mSettings.mWidth;
        const uint32_t   height = mSettings.mHeight;
        /************************************************************************/
        /************************************************************************/
        const ClearValue depthStencilClear = { { 0.0f, 0 } };
        // Used for ESM render target shadow
        const ClearValue lessEqualDepthStencilClear = { { 1.f, 0 } };

        ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        ClearValue optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

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
        depthRT.mFlags = gAppSettings.mMsaaLevel > SAMPLE_COUNT_2 ? TEXTURE_CREATION_FLAG_VR_MULTIVIEW
                                                                  : TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        depthRT.mWidth = width;
        depthRT.pName = "Depth Buffer RT";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        const uint32_t depthAllocationOffset = ESRAM_CURRENT_OFFSET(pRenderer);
        UNREF_PARAM(depthAllocationOffset);

        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        // Shadow pass render target
        /************************************************************************/
        RenderTargetDesc shadowRTDesc = {};
        shadowRTDesc.mArraySize = 1;
        shadowRTDesc.mClearValue = lessEqualDepthStencilClear;
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
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        vbRTDesc.mWidth = width;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
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
        // Disabling compression data will avoid decompression phase before resolve pass.
        // However, the shading pass will require more memory bandwidth.
        // We measured with and without compression and without compression is faster in our case.
#ifndef PROSPERO
        msaaRTDesc.mFlags = TEXTURE_CREATION_FLAG_NO_COMPRESSION;
#endif
        addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);

        /************************************************************************/
        // Intermediate render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "Intermediate", depthAllocationOffset);
        RenderTargetDesc postProcRTDesc = {};
        postProcRTDesc.mArraySize = 1;
        postProcRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        postProcRTDesc.mDepth = 1;
        postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        postProcRTDesc.mFormat = pSwapChain->mFormat;
        postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        postProcRTDesc.mHeight = mSettings.mHeight;
        postProcRTDesc.mWidth = mSettings.mWidth;
        postProcRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        postProcRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        postProcRTDesc.pName = "pIntermediateRenderTarget";
        addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

        /************************************************************************/
        // GodRay render target
        /************************************************************************/
        RenderTargetDesc GRRTDesc = {};
        GRRTDesc.mArraySize = 1;
        GRRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        GRRTDesc.mDepth = 1;
        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mHeight = mSettings.mHeight / gGodrayScale;
        GRRTDesc.mWidth = mSettings.mWidth / gGodrayScale;
        GRRTDesc.mFormat = TinyImageFormat_B10G11R11_UFLOAT;
        GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        GRRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        GRRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        GRRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;

        GRRTDesc.pName = "GodRay RT A";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[0]);
        GRRTDesc.pName = "GodRay RT B";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[1]);
        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        // Color Conversion render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "CurveConversion", 0u);
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
        postCurveConversionRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        postCurveConversionRTDesc.pName = "pCurveConversionRenderTarget";
        addRenderTarget(pRenderer, &postCurveConversionRTDesc, &pCurveConversionRenderTarget);
        ESRAM_END_ALLOC(pRenderer);
        /************************************************************************/
        /************************************************************************/
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pCurveConversionRenderTarget);

        removeRenderTarget(pRenderer, pRenderTargetGodRay[0]);
        removeRenderTarget(pRenderer, pRenderTargetGodRay[1]);

        removeRenderTarget(pRenderer, pIntermediateRenderTarget);
        removeRenderTarget(pRenderer, pRenderTargetMSAA);
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pRenderTargetVBPass);
        removeRenderTarget(pRenderer, pRenderTargetShadow);
    }
    /************************************************************************/
    // Load all the shaders needed for the demo
    /************************************************************************/
    void addRootSignatures()
    {
        // Graphics root signatures
        const char* pTextureSamplerName = "textureFilter";
        const char* pShadingSamplerNames[] = { "depthSampler", "textureSampler" };
        Sampler*    pShadingSamplers[] = { pSamplerBilinearClamp, pSamplerTrilinearAniso };

        Shader* pShaders[gNumGeomSets * 2] = {};
        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            pShaders[i * 2] = pShaderVisibilityBufferPass[i];
            pShaders[i * 2 + 1] = pShaderShadowPass[i];
        }
        RootSignatureDesc vbRootDesc = { pShaders, gNumGeomSets * 2 };
        vbRootDesc.mMaxBindlessTextures = gMaterialCount;
        vbRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
        vbRootDesc.ppStaticSamplers = &pSamplerPointClamp;
        vbRootDesc.mStaticSamplerCount = 1;
        addRootSignature(pRenderer, &vbRootDesc, &pRootSignatureVBPass);

        RootSignatureDesc shadeRootDesc = { pShaderVisibilityBufferShade, 2 * MSAA_LEVELS_COUNT };
        // Set max number of bindless textures in the root signature
        shadeRootDesc.mMaxBindlessTextures = gMaterialCount;
        shadeRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
        shadeRootDesc.ppStaticSamplers = pShadingSamplers;
        shadeRootDesc.mStaticSamplerCount = 2;
        addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureVBShade);

        RootSignatureDesc resolveRootDesc = { pShaderResolve, MSAA_LEVELS_COUNT };
        addRootSignature(pRenderer, &resolveRootDesc, &pRootSignatureResolve);

        // Clear buffers root signature
        RootSignatureDesc clearBuffersRootDesc = { &pShaderClearBuffers, 1 };
        addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);

        // Triangle filtering root signatures
        RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
        addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);

        Shader*           pClusterShaders[] = { pShaderClearLightClusters, pShaderClusterLights };
        RootSignatureDesc clearLightRootDesc = { pClusterShaders, 2 };
        addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureLightClusters);

        const char*       pColorConvertStaticSamplerNames[] = { "uSampler0" };
        RootSignatureDesc CurveConversionRootSigDesc = { &pShaderCurveConversion, 1 };
        CurveConversionRootSigDesc.mStaticSamplerCount = 1;
        CurveConversionRootSigDesc.ppStaticSamplerNames = pColorConvertStaticSamplerNames;
        CurveConversionRootSigDesc.ppStaticSamplers = &pSamplerBilinearClamp;
        addRootSignature(pRenderer, &CurveConversionRootSigDesc, &pRootSigCurveConversionPass);

        const char*       pGodRayStaticSamplerNames[] = { "uSampler0" };
        RootSignatureDesc godrayPassShaderRootSigDesc = { pGodRayPass, MSAA_LEVELS_COUNT };
        godrayPassShaderRootSigDesc.mStaticSamplerCount = 1;
        godrayPassShaderRootSigDesc.ppStaticSamplerNames = pGodRayStaticSamplerNames;
        godrayPassShaderRootSigDesc.ppStaticSamplers = &pSamplerPointClamp;
        addRootSignature(pRenderer, &godrayPassShaderRootSigDesc, &pRootSigGodRayPass);
        gGodRayConstantIndex = getDescriptorIndexFromName(pRootSigGodRayPass, "GodRayRootConstant");

        RootSignatureDesc godrayBlurPassRootSigDesc = { &pShaderGodRayBlurPass, 1 };
        addRootSignature(pRenderer, &godrayBlurPassRootSigDesc, &pRootSignatureGodRayBlurPass);
        gGodRayBlurConstantIndex = getDescriptorIndexFromName(pRootSignatureGodRayBlurPass, "BlurRootConstant");

        const char*       pPresentStaticSamplerNames[] = { "uSampler0" };
        RootSignatureDesc finalShaderRootSigDesc = { &pShaderPresentPass, 1 };
        finalShaderRootSigDesc.mStaticSamplerCount = 1;
        finalShaderRootSigDesc.ppStaticSamplerNames = pPresentStaticSamplerNames;
        finalShaderRootSigDesc.ppStaticSamplers = &pSamplerBilinear;
        addRootSignature(pRenderer, &finalShaderRootSigDesc, &pRootSigPresentPass);
        gSCurveRootConstantIndex = getDescriptorIndexFromName(pRootSigPresentPass, "RootConstantSCurveInfo");

        const char*       pSkyboxSamplerName = "skyboxSampler";
        RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
        skyboxRootDesc.mStaticSamplerCount = 1;
        skyboxRootDesc.ppStaticSamplerNames = &pSkyboxSamplerName;
        skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
        addRootSignature(pRenderer, &skyboxRootDesc, &pRootSingatureSkybox);

        // Setup indirect command signature
        IndirectArgumentDescriptor indirectArg = {};
        indirectArg.mType = INDIRECT_DRAW_INDEX;

        CommandSignatureDesc vbPassDesc = { pRootSignatureVBPass, &indirectArg, 1 };
        addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
    }

    void removeRootSignatures()
    {
        removeRootSignature(pRenderer, pRootSignatureResolve);

        removeRootSignature(pRenderer, pRootSingatureSkybox);

        removeRootSignature(pRenderer, pRootSigGodRayPass);
        removeRootSignature(pRenderer, pRootSignatureGodRayBlurPass);
        removeRootSignature(pRenderer, pRootSigCurveConversionPass);

        removeRootSignature(pRenderer, pRootSigPresentPass);

        removeRootSignature(pRenderer, pRootSignatureLightClusters);
        removeRootSignature(pRenderer, pRootSignatureClearBuffers);
        removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
        removeRootSignature(pRenderer, pRootSignatureVBShade);
        removeRootSignature(pRenderer, pRootSignatureVBPass);

        // Remove indirect command signatures
        removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
    }

    void addShaders()
    {
        ShaderLoadDesc shadowPass = {};
        ShaderLoadDesc shadowPassAlpha = {};
        ShaderLoadDesc vbPass = {};
        ShaderLoadDesc vbPassAlpha = {};
        ShaderLoadDesc vbShade[2 * MSAA_LEVELS_COUNT] = {};
        ShaderLoadDesc resolvePass[MSAA_LEVELS_COUNT] = {};
        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};

        shadowPass.mStages[0].pFileName = "shadow_pass.vert";
        shadowPassAlpha.mStages[0].pFileName = "shadow_pass_alpha.vert";
        shadowPassAlpha.mStages[1].pFileName = "shadow_pass_alpha.frag";

        vbPass.mStages[0].pFileName = "visibilityBuffer_pass.vert";
        vbPass.mStages[1].pFileName = "visibilityBuffer_pass.frag";
        vbPassAlpha.mStages[0].pFileName = "visibilityBuffer_pass_alpha.vert";
        vbPassAlpha.mStages[1].pFileName = "visibilityBuffer_pass_alpha.frag";

#if defined(VULKAN)
        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough && pRenderer->mRendererApi == RENDERER_API_VULKAN;
#else
        bool addGeometryPassThrough = false;
#endif
        // No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
#if defined(ORBIS) || defined(PROSPERO)
        addGeometryPassThrough = true;
#endif
        if (addGeometryPassThrough)
        {
            vbPass.mStages[2].pFileName = "visibilityBuffer_pass.geom";
            vbPassAlpha.mStages[2].pFileName = "visibilityBuffer_pass_alpha.geom";
        }

        const char* visibilityBufferShadeShaders[] = {
            "visibilityBuffer_shade_SAMPLE_1.frag",    "visibilityBuffer_shade_SAMPLE_1_AO.frag", "visibilityBuffer_shade_SAMPLE_2.frag",
            "visibilityBuffer_shade_SAMPLE_2_AO.frag", "visibilityBuffer_shade_SAMPLE_4.frag",    "visibilityBuffer_shade_SAMPLE_4_AO.frag",
        };

        const char* resolveShaders[] = {
            "resolve_SAMPLE_1.frag",
            "resolve_SAMPLE_2.frag",
            "resolve_SAMPLE_4.frag",
        };

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            for (uint32_t j = 0; j < 2; ++j)
            {
                uint32_t index = i * 2 + j;
                vbShade[index].mStages[0].pFileName = "visibilityBuffer_shade.vert";
                vbShade[index].mStages[1].pFileName = visibilityBufferShadeShaders[index];
            }
        }

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            // Resolve shader
            resolvePass[i].mStages[0].pFileName = "resolve.vert";
            resolvePass[i].mStages[1].pFileName = resolveShaders[i];
        }

        // Triangle culling compute shader
        triangleCulling.mStages[0].pFileName = "triangle_filtering.comp";
        // Clear buffers compute shader
        clearBuffer.mStages[0].pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mStages[0].pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mStages[0].pFileName = "cluster_lights.comp";

        const char* godrayShaderFileName[] = { "godray_SAMPLE_COUNT_1.frag", "godray_SAMPLE_COUNT_2.frag", "godray_SAMPLE_COUNT_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            ShaderLoadDesc godrayShaderDesc = {};
            godrayShaderDesc.mStages[0].pFileName = "display.vert";
            godrayShaderDesc.mStages[1].pFileName = godrayShaderFileName[i];
            addShader(pRenderer, &godrayShaderDesc, &pGodRayPass[i]);
        }

        ShaderLoadDesc godrayBlurShaderDesc = {};
        godrayBlurShaderDesc.mStages[0].pFileName = "godray_blur.comp";
        addShader(pRenderer, &godrayBlurShaderDesc, &pShaderGodRayBlurPass);

        ShaderLoadDesc CurveConversionShaderDesc = {};
        CurveConversionShaderDesc.mStages[0].pFileName = "display.vert";
        CurveConversionShaderDesc.mStages[1].pFileName = "CurveConversion.frag";
        addShader(pRenderer, &CurveConversionShaderDesc, &pShaderCurveConversion);

        ShaderLoadDesc presentShaderDesc = {};
        presentShaderDesc.mStages[0].pFileName = "display.vert";
        presentShaderDesc.mStages[1].pFileName = "display.frag";

        ShaderLoadDesc skyboxShaderDesc = {};
        skyboxShaderDesc.mStages[0].pFileName = "skybox.vert";
        skyboxShaderDesc.mStages[1].pFileName = "skybox.frag";

        addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);

        addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
            addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            addShader(pRenderer, &resolvePass[i], &pShaderResolve[i]);
        }
        addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
            removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            removeShader(pRenderer, pShaderResolve[i]);
        }

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
            removeShader(pRenderer, pGodRayPass[i]);
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
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& compPipelineSettings = pipelineDesc.mComputeDesc;
        compPipelineSettings.pShaderProgram = pShaderClearBuffers;
        compPipelineSettings.pRootSignature = pRootSignatureClearBuffers;
        pipelineDesc.pName = "Clear Filtering Buffers";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

        // Create the compute pipeline for GPU triangle filtering
        pipelineDesc.pName = "Triangle Filtering";
        compPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        compPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

        // Setup the clearing light clusters pipeline
        pipelineDesc.pName = "Clear Light Clusters";
        compPipelineSettings.pShaderProgram = pShaderClearLightClusters;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // God Ray Blur Pass
        pipelineDesc.pName = "God Ray Blur";
        compPipelineSettings.pShaderProgram = pShaderGodRayBlurPass;
        compPipelineSettings.pRootSignature = pRootSignatureGodRayBlurPass;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayBlurPass);

        // Setup the compute the light clusters pipeline
        pipelineDesc.pName = "Cluster Lights";
        compPipelineSettings.pShaderProgram = pShaderClusterLights;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);
        /************************************************************************/
        /************************************************************************/
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;
        DepthStateDesc depthStateDisableDesc = {};

        // Shadow mapping...
        DepthStateDesc depthStateLEQUALEnabledDesc = {};
        depthStateLEQUALEnabledDesc.mDepthFunc = CMP_LEQUAL;
        depthStateLEQUALEnabledDesc.mDepthWrite = true;
        depthStateLEQUALEnabledDesc.mDepthTest = true;

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        RasterizerStateDesc rasterizerStateCullNoneMsDesc = { CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID };
        rasterizerStateCullNoneMsDesc.mMultiSample = true;

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

        /************************************************************************/
        // Setup the Shadow Pass Pipeline
        /************************************************************************/
        // Setup pipeline settings
        pipelineDesc = {};
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
        shadowPipelineSettings = { 0 };
        shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        shadowPipelineSettings.pDepthState = &depthStateLEQUALEnabledDesc;
        shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
        shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
        shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
        shadowPipelineSettings.pRootSignature = pRootSignatureVBPass;
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
        vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
        vbPassPipelineSettings.pVertexLayout = NULL;
        vbPassPipelineSettings.pRasterizerState =
            (gAppSettings.mMsaaLevel > 1) ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

#if defined(GFX_EXTENDED_PSO_OPTIONS)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
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
        vbShadePipelineSettings.pDepthState = &depthStateDisableDesc;
        vbShadePipelineSettings.pRasterizerState =
            gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
        vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;

        for (uint32_t i = 0; i < 2; ++i)
        {
            vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[gAppSettings.mMsaaIndex * 2 + i];
            vbShadePipelineSettings.mSampleCount = gAppSettings.mMsaaLevel;
            if (gAppSettings.mMsaaLevel > 1)
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

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
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
        pipelineSettings.pRootSignature = pRootSingatureSkybox;
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
        pipelineSettingsGodRay.pRootSignature = pRootSigGodRayPass;
        pipelineSettingsGodRay.pShaderProgram = pGodRayPass[gAppSettings.mMsaaIndex];
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
        pipelineSettingsCurveConversion.pRootSignature = pRootSigCurveConversionPass;
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
        pipelineSettingsFinalPass.pRootSignature = pRootSigPresentPass;
        pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;
        pipelineDesc.pName = "Composite";
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePresentPass);

        /************************************************************************/
        // Setup MSAA resolve pipeline
        /************************************************************************/
        depthStateDisableDesc = {};
        rasterizerStateCullNoneDesc = { CULL_MODE_NONE };

        pipelineDesc = {};
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
        resolvePipelineSettings.pRootSignature = pRootSignatureResolve;
        resolvePipelineSettings.pShaderProgram = pShaderResolve[gAppSettings.mMsaaIndex];
        pipelineDesc.pName = "MSAA Resolve";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineResolve);
        addPipeline(pRenderer, &pipelineDesc, &pPipelineResolvePost);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineResolve);
        removePipeline(pRenderer, pPipelineResolvePost);

        removePipeline(pRenderer, pPipelineGodRayPass);
        removePipeline(pRenderer, pPipelineGodRayBlurPass);

        removePipeline(pRenderer, pPipelineCurveConversionPass);
        removePipeline(pRenderer, pPipelinePresentPass);

        for (uint32_t i = 0; i < 2; ++i)
        {
            removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
        }

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
            removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
            removePipeline(pRenderer, pPipelineShadowPass[i]);

        removePipeline(pRenderer, pSkyboxPipeline);

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
        uint64_t       size = sizeof(PerFrameConstants);
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

        ubDesc.mDesc.mSize = sizeof(PerFrameVBConstants);
        ubDesc.mDesc.pName = "PerFrameVBConstants Uniform Buffer Desc";
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
        for (uint32_t i = 0; i < LIGHT_COUNT; i++)
        {
            gLightData[i].position.setX(randomFloat(-1000.f, 1000.0f));
            gLightData[i].position.setY(100);
            gLightData[i].position.setZ(randomFloat(-1000.f, 1000.0f));
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
        const uint32_t width = mSettings.mWidth;
        const uint32_t height = mSettings.mHeight;
        const float    aspectRatioInv = (float)height / width;
        const uint32_t frameIdx = currentFrameIdx;
        PerFrameData*  currentFrame = &gPerFrame[frameIdx];

        mat4 cameraModel = mat4::translation(vec3(-20, 0, 0)) * mat4::scale(vec3(SCENE_SCALE));
        mat4 cameraView = pCameraController->getViewMatrix();

        CameraMatrix cameraProj =
            CameraMatrix::perspectiveReverseZ(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

        // Compute light matrices
        vec3 lightSourcePos(50.0f, 000.0f, 450.0f);

        // directional light rotation & translation
        mat4 rotation = mat4::rotationXY(gAppSettings.mSunControl.x, gAppSettings.mSunControl.y);
        vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));
        lightSourcePos += -800.0f * normalize(lightDir.getXYZ());
        mat4 translation = mat4::translation(-lightSourcePos);

        mat4         lightModel = mat4::translation(vec3(-20, 0, 0)) * mat4::scale(vec3(SCENE_SCALE));
        mat4         lightView = rotation * translation;
        CameraMatrix lightProj = CameraMatrix::orthographic(-600, 600, -950, 350, -300, 1300);

        float2 twoOverRes;
        twoOverRes.setX(gAppSettings.mRetinaScaling / float(width));
        twoOverRes.setY(gAppSettings.mRetinaScaling / float(height));
        /************************************************************************/
        // Lighting data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.camPos = v4ToF4(vec4(pCameraController->getViewPosition()));
        currentFrame->gPerFrameUniformData.lightDir = v4ToF4(vec4(lightDir));
        currentFrame->gPerFrameUniformData.twoOverRes = twoOverRes;
        currentFrame->gPerFrameUniformData.esmControl = gAppSettings.mEsmControl;
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
            (inverse(cameraView * cameraModel) * vec4(0, 0, 0, 1)).getXYZ(); // vec4(0,0,0,1) is the camera position in eye space
        /************************************************************************/
        // Shading data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.lightColor = gAppSettings.mLightColor;
        currentFrame->gPerFrameUniformData.lightingMode = (uint)gAppSettings.mLightingMode;
        currentFrame->gPerFrameUniformData.outputMode = (uint)gAppSettings.mOutputMode;
        currentFrame->gPerFrameUniformData.CameraPlane = { gAppSettings.nearPlane, gAppSettings.farPlane };
        currentFrame->gPerFrameUniformData.aoQuality = gAppSettings.mEnableAO ? gAppSettings.mAOQuality : 0;
        currentFrame->gPerFrameUniformData.aoIntensity = gAppSettings.mAOIntensity;
        currentFrame->gPerFrameUniformData.frustumPlaneSizeNormalized = frustumPlaneSizeFovX(PI / 2.0f, (float)width / (float)height, 1.0f);
        currentFrame->gPerFrameUniformData.visualizeAo = gAppSettings.mVisualizeAO;
        currentFrame->gPerFrameUniformData.depthTexSize = { (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight };

        /************************************************************************/
        // Skybox
        /************************************************************************/
        cameraView.setTranslation(vec3(0));
        currentFrame->gUniformDataSky.mCamPos = pCameraController->getViewPosition();
        currentFrame->gUniformDataSky.mProjectView = cameraProj * cameraView;
        /************************************************************************/
        // S-Curve
        /************************************************************************/
        gSCurveInfomation.ScurveScale = gAppSettings.SCurveScaleFactor;
        gSCurveInfomation.ScurveSlope = gAppSettings.SCurveSlopeFactor;

        float x1 = pow(gAppSettings.SCurveSMin, gSCurveInfomation.ScurveSlope);
        float x2 = pow(gAppSettings.SCurveSMid, gSCurveInfomation.ScurveSlope);
        float x3 = pow(gAppSettings.SCurveSMax, gSCurveInfomation.ScurveSlope);
        float y1 = gAppSettings.SCurveTMin;
        float y2 = gAppSettings.SCurveTMid;
        float y3 = gAppSettings.SCurveTMax;

        float tmp = (x3 * y3 * (x1 - x2)) + (x2 * y2 * (x3 - x1)) + (x1 * y1 * (x2 - x3));
        gSCurveInfomation.C1 = ((x2 * x3 * (y2 - y3) * y1) - (x1 * x3 * (y1 - y3) * y2) + (x1 * x2 * (y1 - y2) * y3)) / tmp;
        gSCurveInfomation.C2 = (-(x2 * y2 - x3 * y3) * y1 + (x1 * y1 - x3 * y3) * y2 - (x1 * y1 - x2 * y2) * y3) / tmp;
        gSCurveInfomation.C3 = ((x3 - x2) * y1 - (x3 - x1) * y2 + (x2 - x1) * y3) / tmp;

        gSCurveInfomation.linearScale = gAppSettings.LinearScale / 10000.0f;
        gSCurveInfomation.outputMode = (uint)gAppSettings.mOutputMode;
    }
    /************************************************************************/
    // UI
    /************************************************************************/
    void updateDynamicUIElements()
    {
        static OutputMode          wasHDR10 = gAppSettings.mOutputMode;
        static CurveConversionMode wasLinear = gAppSettings.mCurveConversionMode;

        if (gAppSettings.mCurveConversionMode != wasLinear)
        {
            if (gAppSettings.mCurveConversionMode == CurveConversion_LinearScale)
            {
                gSCurveInfomation.UseSCurve = 0.0f;

                uiShowDynamicWidgets(&gAppSettings.mLinearScale, pGuiWindow);
                uiHideDynamicWidgets(&gAppSettings.mSCurve, pGuiWindow);
            }
            else
            {
                uiHideDynamicWidgets(&gAppSettings.mLinearScale, pGuiWindow);

                if (gAppSettings.mOutputMode != OUTPUT_MODE_SDR)
                {
                    uiShowDynamicWidgets(&gAppSettings.mSCurve, pGuiWindow);
                    gSCurveInfomation.UseSCurve = 1.0f;
                }
            }

            wasLinear = gAppSettings.mCurveConversionMode;
        }

        if (gAppSettings.mOutputMode != wasHDR10)
        {
            if (gAppSettings.mOutputMode == OUTPUT_MODE_SDR)
            {
                uiHideDynamicWidgets(&gAppSettings.mSCurve, pGuiWindow);
                gSCurveInfomation.UseSCurve = 0.0f;
            }
            else
            {
                if (wasHDR10 == OUTPUT_MODE_SDR && gAppSettings.mCurveConversionMode != CurveConversion_LinearScale)
                {
                    uiShowDynamicWidgets(&gAppSettings.mSCurve, pGuiWindow);
                    gSCurveInfomation.UseSCurve = 1.0f;
                }
            }

            wasHDR10 = gAppSettings.mOutputMode;
        }

        static OutputMode prevOutputMode;

        if (prevOutputMode != gAppSettings.mOutputMode)
        {
            if (OUTPUT_MODE_SDR < gAppSettings.mOutputMode)
                uiShowDynamicWidgets(&gAppSettings.mDisplaySetting, pGuiWindow);
            else
            {
                if (OUTPUT_MODE_SDR < prevOutputMode)
                    uiHideDynamicWidgets(&gAppSettings.mDisplaySetting, pGuiWindow);
            }
        }

        prevOutputMode = gAppSettings.mOutputMode;

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
                uiLoad.mColorFormat =
                    gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
                uiLoad.mHeight = mSettings.mHeight;
                uiLoad.mWidth = mSettings.mWidth;
                uiLoad.mLoadType = RELOAD_TYPE_RENDERTARGET;
                loadUserInterface(&uiLoad);

                FontSystemLoadDesc fontLoad = {};
                fontLoad.mColorFormat =
                    gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
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

            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
            cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_SHADOW, geomSet, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
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
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
            cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
    {
        RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pDestinationRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[gAppSettings.mEnableAO]);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBShade[1]);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    void resolveMSAA(Cmd* cmd, RenderTarget* msaaRT, RenderTarget* destRT)
    {
        // transition world render target to be used as input texture in post process pass
        RenderTargetBarrier barrier = { msaaRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { destRT, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdBindPipeline(cmd, pPipelineResolve);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetResolve);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    // Executes a compute shader to clear (reset) the the light clusters on the GPU
    void clearLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClearLightClusters);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetLightClusters[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetLightClusters[1]);
        cmdDispatch(cmd, 1, 1, 1);
    }

    // Executes a compute shader that computes the light clusters on the GPU
    void computeLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClusterLights);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetLightClusters[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetLightClusters[1]);
        cmdDispatch(cmd, LIGHT_COUNT, 1, 1);
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

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Shading Pass");

        drawVisibilityBufferShade(cmd, frameIdx);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        if (gAppSettings.mMsaaLevel > 1)
        {
            // Pixel Puzzle needs the unresolved MSAA texture
            cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Resolve Pass");
            resolveMSAA(cmd, pRenderTargetMSAA, pScreenRenderTarget);
            cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
        }
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
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetSkybox[1]);
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
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetGodRayPass);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetGodRayPassPerFrame);
        cmdBindPushConstants(cmd, pRootSigGodRayPass, gGodRayConstantIndex, &gGodRayConstant);
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
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetGodRayBlurPass);

        const uint32_t threadGroupSizeX = pRenderTargetGodRay[0]->mWidth / 16 + 1;
        const uint32_t threadGroupSizeY = pRenderTargetGodRay[0]->mHeight / 16 + 1;

        // Horizontal Pass
        gGodRayBlurConstant.mBlurPassType = BLUR_PASS_TYPE_HORIZONTAL;
        gGodRayBlurConstant.mFilterRadius = gAppSettings.mFilterRadius;
        cmdBindPushConstants(cmd, pRootSignatureGodRayBlurPass, gGodRayBlurConstantIndex, &gGodRayBlurConstant);
        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

        renderTargetBarrier[0] = { pRenderTargetGodRay[0], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        // Vertical Pass
        gGodRayBlurConstant.mBlurPassType = BLUR_PASS_TYPE_VERTICAL;
        gGodRayBlurConstant.mFilterRadius = gAppSettings.mFilterRadius;
        cmdBindPushConstants(cmd, pRootSignatureGodRayBlurPass, gGodRayBlurConstantIndex, &gGodRayBlurConstant);
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
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetCurveConversionPass);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        pScreenRenderTarget = pCurveConversionRenderTarget;

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
        cmdBindPushConstants(cmd, pRootSigPresentPass, gSCurveRootConstantIndex, &gSCurveInfomation);
        cmdBindDescriptorSet(cmd, gAppSettings.mEnableGodray ? 1 : 0, pDescriptorSetPresentPass);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        RenderTargetBarrier barrierPresent = { pDstCol, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrierPresent);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    // Draw GUI / 2D elements
    void drawGUI(Cmd* cmd, uint32_t frameIdx)
    {
        UNREF_PARAM(frameIdx);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pScreenRenderTarget, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

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

        cmdBindRenderTargets(cmd, NULL);
    }
};

DEFINE_APPLICATION_MAIN(Visibility_Buffer)
