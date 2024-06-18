/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 *
 * This file(code) is licensed under a
 * Creative Commons Attribution-NonCommercial 4.0 International License
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode)
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
 */

#include "../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../Common_3/Application/Interfaces/ICameraController.h"
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

#include "../../../../Custom-Middleware/Aura/LightPropagation/LightPropagationVolume.h"
#include "../../../Common_3/Utilities/RingBuffer.h"

#define NO_FSL_DEFINITIONS
#include "../../Visibility_Buffer/src/SanMiguel.h"

#include "Camera.hpp"
#include "Shadows.hpp"

#include "Shaders/FSL/shader_defs.h.fsl"

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)       \
    X(AddGeometryPassThrough, 0) \
    X(BindlessSupported, 1)

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

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/wind.h.fsl"

#define SCENE_SCALE 50.0f
#define CLOTH_SCALE 0.6f

#define TEST_RSM    0

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
    float4 CameraPlane; // x : near, y : far
};

// Camera Walking
static float gCameraWalkingTime = 0.0f;
float3       gCameraPathData[29084];

uint  gCameraPoints;
float gTotalElpasedTime;
/************************************************************************/
// GUI CONTROLS
/************************************************************************/
typedef struct AppSettings
{ //-V802
    LightingMode mLightingMode = LIGHTING_PBR;

    // Set this variable to true to bypass triangle filtering calculations, holding and representing the last filtered data.
    // This is useful for inspecting filtered geometry for debugging purposes.
    bool mHoldFilteredResults = false;

    bool mAsyncCompute = true;
    // toggle rendering of local point lights
    bool mRenderLocalLights = false;

    bool mDrawDebugTargets = false;
    bool mDrawShadowTargets = false;
#if TEST_RSM
    bool mDrawRSMTargets = false;
#endif

    float mEsmControl = 80.0f;

    float nearPlane = 10.0f;
    float farPlane = 3000.0f;

    float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

    DynamicUIWidgets mDynamicUIWidgetsGR;

    float mRetinaScaling = 1.5f;

    DynamicUIWidgets mLinearScale;
    float            LinearScale = 140.0f;

    // HDR10
    DynamicUIWidgets mDisplaySetting;

    // Camera Walking
    bool  cameraWalking = false;
    float cameraWalkingSpeed = 1.0f;

    // Aura settings
    bool  useCPUPropagation = false;
    bool  useAdvancedDirectionsCPU = false;
    bool  useAdvancedDirectionsGPU = true;
    bool  useCPUPropagationDecoupled = false;
    bool  useMultipleReflections = false;
    bool  alternateGPUUpdates = false;
    float propagationScale = 0.0f;
    float giStrength = 0.0f;
    uint  propagationValueIndex = 0;
    uint  specularQuality = 0;
    bool  useLPV = false;
    bool  enableSun = false;
    bool  useColorMaps = false;
    bool  useSpecular = false;

    float lightScale0 = 0.0f;
    float lightScale1 = 0.0f;
    float lightScale2 = 0.0f;
    float specPow = 0.0f;
    float specScale = 0.0f;
    float specFresnel = 0.0f;

} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/
const char* gSceneName = "SanMiguel.bin";

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// Constants
const uint32_t gRSMResolution = 256;
const uint32_t gShadowMapSize = 1024;

const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

struct UniformDataSkybox
{
    mat4 mProjectView;
    vec3 mCamPos;
};

struct UniformDataSunMatrices
{
    mat4 mProjectView;
    mat4 mModelMat;
    vec4 mLightColor;
};

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = NUM_GEOMETRY_SETS;

const char*    gProfileNames[gNumGeomSets] = { "Opaque", "Alpha" };
const uint32_t gRSMCascadeCount = NUM_RSM_CASCADES;

const float gLPVCascadeSpans[gRSMCascadeCount] = { 500.0f, 1000.0f, 2000.0f };

const char* const NSTEPS_STRINGS[] = { "0",
                                       "2",
                                       "4",
                                       "6",
                                       "8",
                                       "12 - GPU default",
                                       "24 - CPU default",
                                       /*Default*/ "28",
                                       "30",
                                       "32 - max reasonable",
                                       "36",
                                       "40",
                                       "48",
                                       "64 - max allowed" };

const uint NSTEPS_VALS[] = { 0, 2, 4, 6, 8, 12, 24, /*Default*/ 28, 30, 32, 36, 40, 48, 64 };

static float gWindRandomOffset = 4.96f;
static float gWindRandomFactor = 2000.0f;
static float gFlagAnchorHeight = 13.0f;
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
    mat4                gCameraView;
    mat4                gCameraModelView;

    CameraMatrix gRSMCascadeProjection[gRSMCascadeCount] = {};
    mat4         gRSMCascadeView[gRSMCascadeCount] = {};
    float        gRSMViewSize[gRSMCascadeCount] = {};
};

typedef struct DebugVisualizationSettings
{
    bool  isEnabled = false;
    int   cascadeIndex = 0;
    float probeRadius = 2.0f;
} DebugVisualizationSettings;

/************************************************************************/
// Scene
/************************************************************************/
Scene*                     gScene = NULL;
/************************************************************************/
// Settings
/************************************************************************/
AppSettings                gAppSettings;
DebugVisualizationSettings gDebugVisualizationSettings;

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
uint32_t          gVBPassRootConstantIndex = 0;
/************************************************************************/
// VB shade pipeline
/************************************************************************/
const int         gVbShadeConfigCount = 4;
Shader*           pShaderVisibilityBufferShade[gVbShadeConfigCount] = { nullptr };
Pipeline*         pPipelineVisibilityBufferShadeSrgb[gVbShadeConfigCount] = { nullptr };
RootSignature*    pRootSignatureVBShade = nullptr;
DescriptorSet*    pDescriptorSetVBShade[2] = { NULL };
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
// RSM pipeline
/************************************************************************/
Shader*   pShaderFillRSM[gNumGeomSets] = { nullptr };
Pipeline* pPipelineFillRSM[gNumGeomSets] = { nullptr };

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pDepthBuffer = NULL;
RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pRenderTargetShadow = NULL;

#if TEST_RSM
RenderTarget* pRenderTargetRSMAlbedo[gRSMCascadeCount] = { nullptr };
RenderTarget* pRenderTargetRSMNormal[gRSMCascadeCount] = { nullptr };
RenderTarget* pRenderTargetRSMDepth[gRSMCascadeCount] = { nullptr };
#else
RenderTarget* pRenderTargetRSMAlbedo = nullptr;
RenderTarget* pRenderTargetRSMNormal = nullptr;
RenderTarget* pRenderTargetRSMDepth = nullptr;
#endif
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerBilinearClamp = NULL;
Sampler* pSamplerLinearBorder = nullptr;

/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture** gDiffuseMapsStorage = NULL;
Texture** gNormalMapsStorage = NULL;
Texture** gSpecularMapsStorage = NULL;
/************************************************************************/
// Geometry for the scene
/************************************************************************/
Geometry* pGeom = NULL;
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer*   pPerFrameUniformBuffers[gDataBufferCount] = { NULL };
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
Buffer*         pLightClustersCount[gDataBufferCount] = { NULL };
Buffer*         pLightClusters[gDataBufferCount] = { NULL };
Buffer*         pUniformBufferSky[gDataBufferCount] = { NULL };
Buffer*         pUniformBufferAuraLightApply[gDataBufferCount] = { NULL };
uint64_t        gFrameCount = 0;
VBMeshInstance* pVBMeshInstances = NULL;
uint32_t        gMeshCount = 0;
uint32_t        gMaterialCount = 0;
FontDrawDesc    gFrameTimeDraw;
uint32_t        gFontID = 0;

UIComponent* pGuiWindow = NULL;
UIComponent* pAuraDebugTexturesWindow = nullptr;
UIComponent* pAuraDebugVisualizationGuiWindow = nullptr;
UIComponent* pShadowTexturesWindow = nullptr;
UIComponent* pAuraGuiWindow = nullptr;
#if TEST_RSM
UIComponent* pAuraRSMTexturesWindow = nullptr;
#endif
/************************************************************************/
// GPU Profilers
/************************************************************************/
ProfileToken gGpuProfileTokens[2];
/************************************************************************/
// CPU staging data
/************************************************************************/

// CPU buffers for light data
LightData gLightData[LIGHT_COUNT] = {};

Camera  gCamera;
Camera  gDebugCamera;
Camera* gActiveCamera = &gCamera;

typedef struct DirectionalLight
{
    float2 mRotationXY;
} DirectionalLight;

DirectionalLight gDirectionalLight;

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
/************************************************************************/
/************************************************************************/
aura::Aura*         pAura = nullptr;
aura::ITaskManager* pTaskManager = nullptr;

float gAccumTime = 0.0f;

const char*    pPipelineCacheName = "PipelineCache.cache";
PipelineCache* pPipelineCache = NULL;

class AuraApp* pAuraApp = NULL;

/************************************************************************/
// App implementation
/************************************************************************/
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

class AuraApp: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_PIPELINE_CACHE, "PipelineCaches");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        pAuraApp = this;

#if defined(AUTOMATED_TESTING) && defined(ENABLE_CPU_PROPAGATION)
        LuaScriptDesc scripts[] = { { "Test_CPU_Propagation_Switching.lua" }, { "Test_CPU_propagation.lua" } };
        luaDefineScripts(scripts, 2);
#endif

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
#ifdef VULKAN
        const char* rtIndexVSExtension = VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME;
        settings.mVk.mDeviceExtensionCount = 1;
        settings.mVk.ppDeviceExtensions = &rtIndexVSExtension;
#endif
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        if (!gGpuSettings.mBindlessSupported)
        {
            ShowUnsupportedMessage("Aura does not run on this device. GPU does not support enough bindless texture entries");
            return false;
        }

        if (!pRenderer->pGpu->mSettings.mPrimitiveIdSupported)
        {
            ShowUnsupportedMessage("Aura does not run on this device. PrimitiveID is not supported");
            return false;
        }

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

        Queue*      ppQueues[2] = { pGraphicsQueue, pComputeQueue };
        const char* pnGpuProfileTokens[2]{ "GraphicsGpuProfiler", "ComputeGpuProfiler" };

        // Initialize profiler
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.ppQueues = ppQueues;
        profiler.ppProfilerNames = pnGpuProfileTokens;
        profiler.pProfileTokens = gGpuProfileTokens;
        profiler.mGpuProfilerCount = 2;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        /************************************************************************/
        // Initialize Aura
        /************************************************************************/
        HiresTimer auraTimer;
        initHiresTimer(&auraTimer);

        aura::LightPropagationVolumeParams params;
        aura::CPUPropagationParams         cpuParams;
        fillAuraParams(&params, &cpuParams);

        aura::LightPropagationCascadeDesc cascades[gRSMCascadeCount] = {
            { gLPVCascadeSpans[0], 0.1f, 0 },
            { gLPVCascadeSpans[1], 1.0f, 0 },
            { gLPVCascadeSpans[2], 10.0f, 0 },
        };
        initAura(pRenderer, 1024, 1024, params, gDataBufferCount, gRSMCascadeCount, cascades, &pAura);

#if defined(ENABLE_CPU_PROPAGATION)
        initTaskManager(&pTaskManager);
#endif
        LOGF(LogLevel::eINFO, "Init Aura : %f ms", getHiresTimerUSec(&auraTimer, true) / 1000.0f);

        /************************************************************************/
        // Start timing the scene load
        /************************************************************************/
        HiresTimer totalTimer;
        initHiresTimer(&totalTimer);

        /************************************************************************/
        // Init Aura settings
        /************************************************************************/
        // LPV settings
        gAppSettings.useCPUPropagation = false;
        gAppSettings.useAdvancedDirectionsCPU = false;
        gAppSettings.useAdvancedDirectionsGPU = true;
        gAppSettings.useCPUPropagationDecoupled = false;
        gAppSettings.useMultipleReflections = false;
        gAppSettings.alternateGPUUpdates = false;
        gAppSettings.propagationScale = 1.0f;
        gAppSettings.giStrength = 12.0f;
        gAppSettings.propagationValueIndex = 5;
        gAppSettings.specularQuality = 2;
        gAppSettings.useLPV = true;
        gAppSettings.enableSun = true;
        gAppSettings.useSpecular = true;
        gAppSettings.useColorMaps = true;

        gAppSettings.lightScale0 = 1.0f;
        gAppSettings.lightScale1 = 1.0f;
        gAppSettings.lightScale2 = 1.0f;
        gAppSettings.specPow = 5.f;
        gAppSettings.specScale = 0.25f;
        gAppSettings.specFresnel = 1.f;
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

        SamplerDesc auraLinearBorderDesc = { FILTER_LINEAR,
                                             FILTER_LINEAR,
                                             MIPMAP_MODE_LINEAR,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER,
                                             ADDRESS_MODE_CLAMP_TO_BORDER };

        addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
        addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
        addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
        addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);
        addSampler(pRenderer, &auraLinearBorderDesc, &pSamplerLinearBorder);

        /************************************************************************/
        // Load resources for skybox
        /************************************************************************/
        TextureLoadDesc skyboxTriDesc = {};
        skyboxTriDesc.pFileName = "daytime_cube.tex";
        skyboxTriDesc.ppTexture = &pSkybox;
        addResource(&skyboxTriDesc, NULL);
        /************************************************************************/
        // Load the scene using the SceneLoader class, which uses Assimp
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
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_NONE;
            desc.pFileName = gScene->normalMaps[i];
            desc.ppTexture = &gNormalMapsStorage[i];
            addResource(&desc, NULL);
            desc.pFileName = gScene->specularMaps[i];
            desc.ppTexture = &gSpecularMapsStorage[i];
            addResource(&desc, NULL);
        }

        uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};

        /************************************************************************/
        // Filter batch creation
        /************************************************************************/
        HiresTimer clusterTimer;
        initHiresTimer(&clusterTimer);

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
        removeResource(gScene->geomData);

        // Init visibility buffer
        VisibilityBufferDesc vbDesc = {};
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = gDataBufferCount;
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
        updateVBMeshFilterGroupsDesc.mNumMeshInstance = gMeshCount;
        updateVBMeshFilterGroupsDesc.pVBMeshInstances = pVBMeshInstances;
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            updateVBMeshFilterGroupsDesc.mFrameIndex = frameIdx;
            gVBPreFilterStats[frameIdx] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
        }

        LOGF(LogLevel::eINFO, "Created Filter Batches : %f ms", getHiresTimerUSec(&clusterTimer, true) / 1000.0f);

        // Generate sky box vertex buffer
        static const float skyBoxPoints[] = {
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
        float2 screenSize = { (float)mSettings.mWidth, (float)mSettings.mHeight };
        SetupAppSettingsWindow(screenSize);
        SetupAuraSettingsWindow(screenSize);
        SetupDebugVisualizationWindow(screenSize);

        /************************************************************************/
        /************************************************************************/
        // Finish the resource loading process since the next code depends on the loaded resources
        waitForAllResourceLoads();

        HiresTimer setupBuffersTimer;
        initHiresTimer(&setupBuffersTimer);
        addTriangleFilteringBuffers(gScene);

        LOGF(LogLevel::eINFO, "Setup buffers : %f ms", getHiresTimerUSec(&setupBuffersTimer, true) / 1000.0f);

        BufferLoadDesc auraUbDesc = {};
        auraUbDesc.mDesc.mDescriptors = (::DescriptorType)(DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        auraUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        auraUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        auraUbDesc.mDesc.mSize = sizeof(LightApplyData);
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            auraUbDesc.ppBuffer = &pUniformBufferAuraLightApply[frameIdx];
            addResource(&auraUbDesc, NULL);
        }

        LOGF(LogLevel::eINFO, "Total Load Time : %f ms", getHiresTimerUSec(&totalTimer, true) / 1000.0f);

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
        // Setup the fps camera for navigating through the scene
        /************************************************************************/
        gDirectionalLight.mRotationXY = float2(-2.026f, 1.557f);

        vec3 startPosition(596.0f, 534.0f, -112.0f);
        vec3 startLookAt = startPosition + vec3(-2.5, -1.0f, 1.5f);

        PerspectiveProjection projection;
        projection.mFovY = PI / 2.0f;
        projection.mNear = 1.0f;
        projection.mFar = 2000.0f;
        projection.mAspectRatio = 1.0f;

        ICameraController* pCameraController = initFpsCameraController(startPosition, startLookAt);
        gCamera.mProjection = projection;
        gCamera.pCameraController = pCameraController;

        CameraMotionParameters camParams;
        camParams.acceleration = 2600 * 2.5f;
        camParams.braking = 2600 * 2.5f;
        camParams.maxSpeed = 500 * 2.5f;
        pCameraController->setMotionParameters(camParams);

        // Setup debug camera
        ICameraController* pDebugCameraController = initFpsCameraController(startPosition, startLookAt);
        pDebugCameraController->setMotionParameters(camParams);
        gDebugCamera.mProjection = projection;
        gDebugCamera.pCameraController = pDebugCameraController;

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
                    gActiveCamera->pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    gActiveCamera->pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    gActiveCamera->pCameraController->onMoveY(delta[0]);
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
                               gActiveCamera->pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameCount = 0;

        return true;
    }

    void Exit()
    {
        removeResource(pSkybox);

        removeTriangleFilteringBuffers();

        uiDestroyDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
        uiDestroyDynamicWidgets(&gAppSettings.mLinearScale);
        uiDestroyDynamicWidgets(&gAppSettings.mDisplaySetting);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        pAuraDebugTexturesWindow = nullptr;
        pAuraDebugVisualizationGuiWindow = nullptr;
        pShadowTexturesWindow = nullptr;
        pAuraGuiWindow = nullptr;
#if TEST_RSM
        pAuraRSMTexturesWindow = nullptr;
#endif
        /************************************************************************/
        // Remove loaded scene
        /************************************************************************/
        exitSanMiguel(gScene);

        // Destroy scene buffers
        removeResource(pGeom);

        removeResource(pSkyboxVertexBuffer);

        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            removeResource(pUniformBufferAuraLightApply[frameIdx]);
        }

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
        removeSampler(pRenderer, pSamplerLinearBorder);

        PipelineCacheSaveDesc saveDesc = {};
        saveDesc.pFileName = pPipelineCacheName;
        savePipelineCache(pRenderer, pPipelineCache, &saveDesc);
        removePipelineCache(pRenderer, pPipelineCache);

#if defined(ENABLE_CPU_PROPAGATION)
        removeTaskManager(pTaskManager);
#endif
        exitVisibilityBuffer(pVisibilityBuffer);

        exitAura(pRenderer, pTaskManager, pAura);

        exitResourceLoaderInterface(pRenderer);

        exitRenderer(pRenderer);
        pRenderer = NULL;

        exitInputSystem();

        exitCameraController(gCamera.pCameraController);
        exitCameraController(gDebugCamera.pCameraController);
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
            if (!addSwapChain())
                return false;

            addRenderTargets();

            if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
            {
                setCascadeCenter(pAura, 2, aura::vec3(0.0f, 0.0f, 0.0f));
                float2 screenSize = { (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight };
                SetupGuiWindows(screenSize);
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

            if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
            {
                if (pAuraDebugTexturesWindow)
                {
                    uiDestroyComponent(pAuraDebugTexturesWindow);
                    pAuraDebugTexturesWindow = NULL;
                }

                if (pShadowTexturesWindow)
                {
                    uiDestroyComponent(pShadowTexturesWindow);
                    pShadowTexturesWindow = NULL;
                }

#if TEST_RSM
                if (pAuraRSMTexturesWindow)
                {
                    uiDestroyComponent(pAuraRSMTexturesWindow);
                    pAuraRSMTexturesWindow = NULL;
                }
#endif

                ESRAM_RESET_ALLOCS(pRenderer);
            }
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
        gAccumTime += deltaTime;

        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

#if !defined(TARGET_IOS)
        gActiveCamera->pCameraController->update(deltaTime);
#endif

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
            gActiveCamera->pCameraController->moveTo(f3Tov3(newPos));

            float3 newLookat = v3ToF3(lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]),
                                           f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
            gActiveCamera->pCameraController->lookAt(f3Tov3(newLookat));
        }

        updateDynamicUIElements();
        updateLPVParams();

        updateUniformData(gFrameCount % gDataBufferCount);
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

            cmdBeginGpuFrameProfile(computeCmd, gGpuProfileTokens[1], true);

            TriangleFilteringPassDesc triangleFilteringDesc = {};
            triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
            triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;

            triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetClearBuffers;
            triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetTriangleFiltering[0];
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetTriangleFiltering[1];

            triangleFilteringDesc.mFrameIndex = frameIdx;
            triangleFilteringDesc.mBuffersIndex = frameIdx;
            triangleFilteringDesc.mGpuProfileToken = gGpuProfileTokens[1];
            triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIdx];
            cmdVBTriangleFilteringPass(pVisibilityBuffer, computeCmd, &triangleFilteringDesc);

            cmdBeginGpuTimestampQuery(computeCmd, gGpuProfileTokens[1], "Clear Light Clusters");

            clearLightClusters(computeCmd, frameIdx);

            cmdEndGpuTimestampQuery(computeCmd, gGpuProfileTokens[1]);

            if (gAppSettings.mRenderLocalLights)
            {
                /************************************************************************/
                // Synchronization
                /************************************************************************/
                // Update Light clusters on the GPU

                cmdBeginGpuTimestampQuery(computeCmd, gGpuProfileTokens[1], "Compute Light Clusters");
                BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                               RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(computeCmd, 1, barriers, 0, NULL, 0, NULL);
                computeLightClusters(computeCmd, frameIdx);
                cmdEndGpuTimestampQuery(computeCmd, gGpuProfileTokens[1]);
            }

            cmdEndGpuFrameProfile(computeCmd, gGpuProfileTokens[1]);

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
        // Draw Pass
        /************************************************************************/
        if (gAppSettings.useLPV || gDebugVisualizationSettings.isEnabled)
        {
            const float4&    baseCamPos = gPerFrame[gFrameCount % gDataBufferCount].gPerFrameUniformData.camPos;
            const aura::vec3 camPos(baseCamPos.getX(), baseCamPos.getY(), baseCamPos.getZ());
            const vec4&      baseCamDir = gPerFrame[gFrameCount % gDataBufferCount].gCameraModelView.getRow(2);
            const aura::vec3 camDir(baseCamDir.getX(), baseCamDir.getY(), baseCamDir.getZ());
            aura::beginFrame(pRenderer, pAura, camPos, camDir);
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

            acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
            pScreenRenderTarget = pSwapChain->ppRenderTargets[presentIndex];
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
            // Submit all render commands for this frame
            beginCmd(graphicsCmd);

            cmdBeginGpuFrameProfile(graphicsCmd, gGpuProfileTokens[0]);

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
                triangleFilteringDesc.mGpuProfileToken = gGpuProfileTokens[0];
                triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIdx];
                cmdVBTriangleFilteringPass(pVisibilityBuffer, graphicsCmd, &triangleFilteringDesc);
            }

            if (!gAppSettings.mAsyncCompute)
            {
                cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "Clear Light Clusters");
                clearLightClusters(graphicsCmd, frameIdx);
                cmdEndGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0]);
            }

            if (!gAppSettings.mAsyncCompute && gAppSettings.mRenderLocalLights)
            {
                // Update Light clusters on the GPU

                cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "Compute Light Clusters");
                BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                               RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
                computeLightClusters(graphicsCmd, frameIdx);
                barriers[0] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
                cmdEndGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0]);
            }

            // Transition swapchain buffer to be used as a render target
            RenderTargetBarrier rtBarriers[] = {
                { pScreenRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
                { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
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

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 2, rtBarriers);

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

            cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "UI Pass");
            drawGUI(graphicsCmd, frameIdx);
            cmdEndGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0]);

            rtBarriers[0] = { pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
            cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 1, rtBarriers);

            cmdEndGpuFrameProfile(graphicsCmd, gGpuProfileTokens[0]);
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

            if (gAppSettings.useLPV || gDebugVisualizationSettings.isEnabled)
            {
                aura::endFrame(pRenderer, pAura);
            }

            flipProfiler();
        }

        ++gFrameCount;
    }

    const char* GetName() { return "Aura"; }

    bool addDescriptorSets()
    {
        aura::addDescriptorSets();
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

        // Sky
        setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
        setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);

        return true;
    }

    void removeDescriptorSets()
    {
        aura::removeDescriptorSets();

        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
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
        aura::prepareDescriptorSets();

        // Clear Buffers
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                uint32_t       clearParamsCount = 0;
                DescriptorData clearParams[NUM_GEOMETRY_SETS + 1] = {};
                clearParams[clearParamsCount].pName = "indirectDrawArgs";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                clearParams[clearParamsCount].pName = "VBConstantBuffer";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
                updateDescriptorSet(pRenderer, i, pDescriptorSetClearBuffers, clearParamsCount, clearParams);
            }
        }
        // Triangle Filtering
        {
            DescriptorData filterParams[4] = {};
            filterParams[0].pName = "vertexPositionBuffer";
            filterParams[0].ppBuffers = &pGeom->pVertexBuffers[0];
            filterParams[1].pName = "indexDataBuffer";
            filterParams[1].ppBuffers = &pGeom->pIndexBuffer;
            filterParams[2].pName = "meshConstantsBuffer";
            filterParams[2].ppBuffers = &pMeshConstantsBuffer;
            filterParams[3].pName = "VBConstantBuffer";
            filterParams[3].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], 4, filterParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData filterParamsIdx[5] = {};
                filterParamsIdx[0].pName = "filteredIndicesBuffer";
                filterParamsIdx[0].mCount = gNumViews;
                filterParamsIdx[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS];
                filterParamsIdx[1].pName = "indirectDrawArgs";
                filterParamsIdx[1].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                filterParamsIdx[2].pName = "PerFrameVBConstants";
                filterParamsIdx[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
                filterParamsIdx[3].pName = "filterDispatchGroupDataBuffer";
                filterParamsIdx[3].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
                filterParamsIdx[4].pName = "indirectDataBuffer";
                filterParamsIdx[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFiltering[1], 5, filterParamsIdx);
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
            DescriptorData params[4] = {};
            params[0].pName = "diffuseMaps";
            params[0].mCount = gMaterialCount;
            params[0].ppTextures = gDiffuseMapsStorage;
            params[1].pName = "vertexPositionBuffer";
            params[1].ppBuffers = &pGeom->pVertexBuffers[0];
            params[2].pName = "vertexTexCoordBuffer";
            params[2].ppBuffers = &pGeom->pVertexBuffers[1];
            params[3].pName = "vertexNormalBuffer";
            params[3].ppBuffers = &pGeom->pVertexBuffers[2];
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 4, params);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData paramsIndirect[3] = {};
                paramsIndirect[0].pName = "indirectDataBuffer";
                paramsIndirect[0].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                paramsIndirect[1].pName = "PerFrameConstants";
                paramsIndirect[1].ppBuffers = &pPerFrameUniformBuffers[i];
                paramsIndirect[2].pName = "PerFrameVBConstants";
                paramsIndirect[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetVBPass[1], 3, paramsIndirect);
            }
        }
        // VB Shade
        {
            Texture** ppTextures = (Texture**)alloca(NUM_GRIDS_PER_CASCADE * pAura->mCascadeCount * sizeof(Texture*));
            for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
            {
                for (uint32_t j = 0; j < NUM_GRIDS_PER_CASCADE; ++j)
                {
                    ppTextures[i * NUM_GRIDS_PER_CASCADE + j] = pAura->pCascades[i]->pLightGrids[j]->pTexture;
                }
            }

            DescriptorData vbShadeParams[12] = {};
            DescriptorData vbShadeParamsPerFrame[9] = {};

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
            vbShadeParams[8].pName = "shadowMap";
            vbShadeParams[8].ppTextures = &pRenderTargetShadow->pTexture;
            vbShadeParams[9].pName = "LPVGridCascades";
            vbShadeParams[9].ppTextures = ppTextures;
            vbShadeParams[9].mCount = pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE;
            vbShadeParams[10].pName = "meshConstantsBuffer";
            vbShadeParams[10].ppBuffers = &pMeshConstantsBuffer;
            vbShadeParams[11].pName = "VBConstantBuffer";
            vbShadeParams[11].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 12, vbShadeParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                uint32_t count = 0;
                vbShadeParamsPerFrame[count].pName = "lightClustersCount";
                vbShadeParamsPerFrame[count++].ppBuffers = &pLightClustersCount[i];
                vbShadeParamsPerFrame[count].pName = "lightClusters";
                vbShadeParamsPerFrame[count++].ppBuffers = &pLightClusters[i];
                vbShadeParamsPerFrame[count].pName = "PerFrameConstants";
                vbShadeParamsPerFrame[count++].ppBuffers = &pPerFrameUniformBuffers[i];
                vbShadeParamsPerFrame[count].pName = "PerFrameVBConstants";
                vbShadeParamsPerFrame[count++].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                vbShadeParamsPerFrame[count].pName = "auraApplyLightData";
                vbShadeParamsPerFrame[count++].ppBuffers = &pUniformBufferAuraLightApply[i];
                vbShadeParamsPerFrame[count].pName = "indirectDataBuffer";
                vbShadeParamsPerFrame[count++].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                vbShadeParamsPerFrame[count].pName = "filteredIndexBuffer";
                vbShadeParamsPerFrame[count++].ppBuffers =
                    &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
                updateDescriptorSet(pRenderer, i, pDescriptorSetVBShade[1], count, vbShadeParamsPerFrame);
            }
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

        swapChainDesc.mColorClearValue = { { 0, 0, 0, 0 } };
        swapChainDesc.mEnableVsync = false;
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
        const ClearValue lessEqualDepthStencilClear = { { 1.f, 0 } }; // shadow mapping

        ClearValue       optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        ClearValue       optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        /************************************************************************/
        // Main depth buffer
        /************************************************************************/
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = depthStencilClear;
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        depthRT.mHeight = height;
        depthRT.mSampleCount = (SampleCount)1;
        depthRT.mSampleQuality = 0;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        depthRT.mWidth = width;
        depthRT.pName = "Depth Buffer RT";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
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
        shadowRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
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
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        vbRTDesc.mHeight = height;
        vbRTDesc.mSampleCount = (SampleCount)1;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        vbRTDesc.mWidth = width;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
        /***********************************************************************/
        // RSM render targets
        /************************************************************************/
        RenderTargetDesc rsmRTDesc = {};
        rsmRTDesc.mArraySize = 1;
        rsmRTDesc.mDepth = 1;
        rsmRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        rsmRTDesc.mHeight = gRSMResolution;
        rsmRTDesc.mSampleCount = SAMPLE_COUNT_1;
        rsmRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        rsmRTDesc.mWidth = gRSMResolution;
        rsmRTDesc.pName = "RSM RT";
#if TEST_RSM
        for (uint32_t i = 0; i < gRSMCascadeCount; ++i)
        {
            rsmRTDesc.mClearValue = optimizedColorClearBlack;
            rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
            rsmRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
            addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMAlbedo[i]);
            addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMNormal[i]);

            rsmRTDesc.mClearValue = optimizedDepthClear;
            rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            rsmRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
            addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMDepth[i]);
        }
#else
        rsmRTDesc.mClearValue = optimizedColorClearBlack;
        rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        rsmRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
        addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMAlbedo);
        addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMNormal);

        rsmRTDesc.mClearValue = lessEqualDepthStencilClear;
        rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        rsmRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
        addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMDepth);
#endif

        /************************************************************************/
        /************************************************************************/
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pRenderTargetVBPass);

        removeRenderTarget(pRenderer, pRenderTargetShadow);
#if TEST_RSM
        for (int i = 0; i < gRSMCascadeCount; i++)
        {
            removeRenderTarget(pRenderer, pRenderTargetRSMAlbedo[i]);
            removeRenderTarget(pRenderer, pRenderTargetRSMNormal[i]);
            removeRenderTarget(pRenderer, pRenderTargetRSMDepth[i]);
        }
#else
        removeRenderTarget(pRenderer, pRenderTargetRSMAlbedo);
        removeRenderTarget(pRenderer, pRenderTargetRSMNormal);
        removeRenderTarget(pRenderer, pRenderTargetRSMDepth);
#endif
    }

    void addRootSignatures()
    {
        aura::addRootSignatures();

        // Graphics root signatures
        const char* pTextureSamplerNames[] = { "textureFilter", "alphaFilterBilinear", "rsmFilter", "alphaFilterPoint" };
        Sampler*    pTextureSamplers[] = { pSamplerPointClamp, pSamplerPointClamp, pSamplerTrilinearAniso, pSamplerPointClamp };
        const char* pShadingSamplerNames[] = { "depthSampler", "textureSampler", "linearBorderSampler" };
        Sampler*    pShadingSamplers[] = { pSamplerBilinearClamp, pSamplerBilinear, pSamplerLinearBorder };

        const int shaderCount = 3;
        Shader*   pShaders[gNumGeomSets * shaderCount] = {};
        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            pShaders[i * shaderCount] = pShaderVisibilityBufferPass[i];
            pShaders[i * shaderCount + 1] = pShaderFillRSM[i];
            pShaders[i * shaderCount + 2] = pShaderShadowPass[i];
        }

        RootSignatureDesc vbRootDesc = { pShaders, gNumGeomSets * shaderCount };
        vbRootDesc.mMaxBindlessTextures = gMaterialCount;
        vbRootDesc.ppStaticSamplerNames = pTextureSamplerNames;
        vbRootDesc.ppStaticSamplers = pTextureSamplers;
        vbRootDesc.mStaticSamplerCount = 4;
        addRootSignature(pRenderer, &vbRootDesc, &pRootSignatureVBPass);
        gVBPassRootConstantIndex = getDescriptorIndexFromName(pRootSignatureVBPass, "materialRootConstant");

        RootSignatureDesc shadeRootDesc = { pShaderVisibilityBufferShade, gVbShadeConfigCount };
        // Set max number of bindless textures in the root signature
        shadeRootDesc.mMaxBindlessTextures = gMaterialCount;
        shadeRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
        shadeRootDesc.ppStaticSamplers = pShadingSamplers;
        shadeRootDesc.mStaticSamplerCount = 3;
        addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureVBShade);

        // Clear buffers root signature
        RootSignatureDesc clearBuffersRootDesc = { &pShaderClearBuffers, 1 };
        addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);
        // Triangle filtering root signature
        RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
        addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);

        Shader*           pClusterShaders[] = { pShaderClearLightClusters, pShaderClusterLights };
        RootSignatureDesc clearLightRootDesc = { pClusterShaders, 2 };
        addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureLightClusters);

        const char*       pSkyboxSamplerName = "skyboxSampler";
        RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
        skyboxRootDesc.mStaticSamplerCount = 1;
        skyboxRootDesc.ppStaticSamplerNames = &pSkyboxSamplerName;
        skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
        addRootSignature(pRenderer, &skyboxRootDesc, &pRootSingatureSkybox);

        /************************************************************************/
        // Setup indirect command signature
        /************************************************************************/
        IndirectArgumentDescriptor indirectArg = {};
        indirectArg.mType = INDIRECT_DRAW_INDEX;
        CommandSignatureDesc vbPassDesc = { pRootSignatureVBPass, &indirectArg, 1 };
        addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
    }

    void removeRootSignatures()
    {
        aura::removeRootSignatures();

        removeRootSignature(pRenderer, pRootSingatureSkybox);

        removeRootSignature(pRenderer, pRootSignatureLightClusters);
        removeRootSignature(pRenderer, pRootSignatureClearBuffers);
        removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
        removeRootSignature(pRenderer, pRootSignatureVBShade);
        removeRootSignature(pRenderer, pRootSignatureVBPass);

        // Remove indirect command signatures
        removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
    }

    /************************************************************************/
    // Load all the shaders needed for the demo
    /************************************************************************/
    void addShaders()
    {
        aura::addShaders();

        ShaderLoadDesc shadowPass = {};
        ShaderLoadDesc shadowPassAlpha = {};
        ShaderLoadDesc vbPass = {};
        ShaderLoadDesc vbPassAlpha = {};
        ShaderLoadDesc vbShade[gVbShadeConfigCount] = {};
        // ShaderLoadDesc resolvePass = {};
        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};
        ShaderLoadDesc fillRSM = {};
        ShaderLoadDesc fillRSMAlpha = {};

#if defined(VULKAN)
        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough && pRenderer->mRendererApi == RENDERER_API_VULKAN;
#else
        bool            addGeometryPassThrough = false;
#endif
        // No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
#if defined(ORBIS) || defined(PROSPERO)
        addGeometryPassThrough = true;
#endif

        shadowPass.mStages[0].pFileName = "shadow_pass.vert";
        shadowPassAlpha.mStages[0].pFileName = "shadow_pass_alpha.vert";
        shadowPassAlpha.mStages[1].pFileName = "shadow_pass_alpha.frag";

        vbPass.mStages[0].pFileName = "visibilityBuffer_pass.vert";
        vbPass.mStages[1].pFileName = "visibilityBuffer_pass.frag";
        vbPassAlpha.mStages[0].pFileName = "visibilityBuffer_pass_alpha.vert";
        vbPassAlpha.mStages[1].pFileName = "visibilityBuffer_pass_alpha.frag";

        if (addGeometryPassThrough)
        {
            vbPass.mStages[2].pFileName = "visibilityBuffer_pass.geom";
            vbPassAlpha.mStages[2].pFileName = "visibilityBuffer_pass_alpha.geom";
        }

        const char* vbShadeShaderNames[] = {
            "visibilityBuffer_shade.frag",
            "visibilityBuffer_shade_ENABLE_SUN.frag",
            "visibilityBuffer_shade_USE_LPV.frag",
            "visibilityBuffer_shade_USE_LPV_ENABLE_SUN.frag",
        };

        for (uint32_t i = 0; i < 2; ++i)
        {
            for (uint32_t j = 0; j < 2; ++j)
            {
                int configIndex = 2 * i + j;

                vbShade[configIndex].mStages[0].pFileName = "visibilityBuffer_shade.vert";
                vbShade[configIndex].mStages[1].pFileName = vbShadeShaderNames[configIndex];
            }
        }
        // Triangle culling compute shader
        triangleCulling.mStages[0].pFileName = "triangle_filtering.comp";
        // Clear buffers compute shader
        clearBuffer.mStages[0].pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mStages[0].pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mStages[0].pFileName = "cluster_lights.comp";

        ShaderLoadDesc skyboxShaderDesc = {};
        skyboxShaderDesc.mStages[0].pFileName = "skybox.vert";
        skyboxShaderDesc.mStages[1].pFileName = "skybox.frag";

        // Fill RSM shader
        fillRSM.mStages[0].pFileName = "fill_rsm.vert";
        fillRSM.mStages[1].pFileName = "fill_rsm.frag";
        fillRSMAlpha.mStages[0].pFileName = "fill_rsm_alpha.vert";
        fillRSMAlpha.mStages[1].pFileName = "fill_rsm_alpha.frag";

        addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < gVbShadeConfigCount; ++i)
            addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
        addShader(pRenderer, &fillRSM, &pShaderFillRSM[GEOMSET_OPAQUE]);
        addShader(pRenderer, &fillRSMAlpha, &pShaderFillRSM[GEOMSET_ALPHA_CUTOUT]);
    }

    void removeShaders()
    {
        aura::removeShaders();

        removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        for (uint32_t i = 0; i < gVbShadeConfigCount; ++i)
            removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);

        removeShader(pRenderer, pShaderSkybox);

        removeShader(pRenderer, pShaderFillRSM[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderFillRSM[GEOMSET_ALPHA_CUTOUT]);
    }

    void addPipelines()
    {
        aura::addPipelines(pPipelineCache, pSwapChain->ppRenderTargets[0]->mFormat, pDepthBuffer->mFormat,
                           pSwapChain->ppRenderTargets[0]->mSampleCount, pSwapChain->ppRenderTargets[0]->mSampleQuality);
        /************************************************************************/
        // Setup compute pipelines for triangle filtering
        /************************************************************************/
        PipelineDesc pipelineDesc = {};
        pipelineDesc.pCache = pPipelineCache;
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& computePipelineSettings = pipelineDesc.mComputeDesc;
        computePipelineSettings.pShaderProgram = pShaderClearBuffers;
        computePipelineSettings.pRootSignature = pRootSignatureClearBuffers;
        pipelineDesc.pName = "Clear Filtering Buffers";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

        // Create the compute pipeline for GPU triangle filtering
        pipelineDesc.pName = "Triangle Filtering";
        computePipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

        // Setup the clearing light clusters pipeline
        pipelineDesc.pName = "Clear Light Clusters";
        computePipelineSettings.pShaderProgram = pShaderClearLightClusters;
        computePipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // Setup the compute the light clusters pipeline
        pipelineDesc.pName = "Cluster Lights";
        computePipelineSettings.pShaderProgram = pShaderClusterLights;
        computePipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);

        /************************************************************************/
        // Setup the Shadow Pass Pipeline
        /************************************************************************/
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;
        DepthStateDesc depthStateDisableDesc = {};

        // Shadow mapping
        DepthStateDesc depthStateLEQUALEnabledDesc = {};
        depthStateLEQUALEnabledDesc.mDepthFunc = CMP_LEQUAL;
        depthStateLEQUALEnabledDesc.mDepthWrite = true;
        depthStateLEQUALEnabledDesc.mDepthTest = true;

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        RasterizerStateDesc rasterizerStateCullNoneDepthClampedDesc = rasterizerStateCullNoneDesc;
        rasterizerStateCullNoneDepthClampedDesc.mDepthClampEnable = true;

        BlendStateDesc blendStateSkyBoxDesc = {};
        blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
        blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;

        blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
        blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;

        blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
        blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;

        blendStateSkyBoxDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;

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
        shadowPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDepthClampedDesc;
        shadowPipelineSettings.pVertexLayout = NULL;
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
        vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;

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
        vbShadePipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;

        for (uint32_t i = 0; i < gVbShadeConfigCount; ++i)
        {
            vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[i];
            vbShadePipelineSettings.mSampleCount = (SampleCount)1;
            vbShadePipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
            vbShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

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

        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
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
        // Setup Fill RSM pipeline
        /************************************************************************/
#if TEST_RSM
        TinyImageFormat rsmColorFormats[] = {
            pRenderTargetRSMAlbedo[0]->mFormat,
            pRenderTargetRSMNormal[0]->mFormat,
        };
#else
        TinyImageFormat rsmColorFormats[] = {
            pRenderTargetRSMAlbedo->mFormat,
            pRenderTargetRSMNormal->mFormat,
        };
#endif
        PipelineDesc fillRSMPipelineSettings = {};
        fillRSMPipelineSettings.mType = PIPELINE_TYPE_GRAPHICS;
        fillRSMPipelineSettings.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        fillRSMPipelineSettings.mGraphicsDesc.mRenderTargetCount = 2;
        fillRSMPipelineSettings.mGraphicsDesc.pDepthState = &depthStateLEQUALEnabledDesc;
#if TEST_RSM
        fillRSMPipelineSettings.mGraphicsDesc.mDepthStencilFormat = pRenderTargetRSMDepth[0]->mFormat;
        fillRSMPipelineSettings.mGraphicsDesc.mSampleCount = pRenderTargetRSMAlbedo[0]->mSampleCount;
        fillRSMPipelineSettings.mGraphicsDesc.mSampleQuality = pRenderTargetRSMAlbedo[0]->mSampleQuality;
#else
        fillRSMPipelineSettings.mGraphicsDesc.mDepthStencilFormat = pRenderTargetRSMDepth->mFormat;
        fillRSMPipelineSettings.mGraphicsDesc.mSampleCount = pRenderTargetRSMAlbedo->mSampleCount;
        fillRSMPipelineSettings.mGraphicsDesc.mSampleQuality = pRenderTargetRSMAlbedo->mSampleQuality;
#endif

        fillRSMPipelineSettings.mGraphicsDesc.pColorFormats = rsmColorFormats;
        fillRSMPipelineSettings.mGraphicsDesc.pRootSignature = pRootSignatureVBPass;
        fillRSMPipelineSettings.mGraphicsDesc.pVertexLayout = NULL;
        fillRSMPipelineSettings.mGraphicsDesc.pRasterizerState = &rasterizerStateCullNoneDepthClampedDesc;

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            fillRSMPipelineSettings.mGraphicsDesc.pShaderProgram = pShaderFillRSM[i];
            pipelineDesc.pName = GEOMSET_OPAQUE == i ? "RSM Opaque" : "RSM AlphaTested";
            addPipeline(pRenderer, &fillRSMPipelineSettings, &pPipelineFillRSM[i]);
        }
    }

    void removePipelines()
    {
        aura::removePipelines();

        for (uint32_t i = 0; i < 4; ++i)
        {
            removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
        }

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            removePipeline(pRenderer, pPipelineFillRSM[i]);
            removePipeline(pRenderer, pPipelineShadowPass[i]);
            removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);
        }

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
        batchUb.mDesc.pName = "Batch UB Desc";
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

        mat4         cameraModel = mat4::scale(vec3(SCENE_SCALE));
        mat4         cameraView = currentFrame->gCameraView = gCamera.pCameraController->getViewMatrix();
        CameraMatrix cameraProj =
            CameraMatrix::perspectiveReverseZ(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

        currentFrame->gCameraModelView = cameraView * cameraModel;

        // directional light rotation & translation
        mat4 rotation = mat4::rotationXY(gDirectionalLight.mRotationXY.x, gDirectionalLight.mRotationXY.y);
        vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));

        mat4         lightModel = mat4::scale(vec3(SCENE_SCALE));
        mat4         lightView = rotation;
        CameraMatrix lightProj = CameraMatrix::orthographic(-600, 600, -950, 350, -1100, 500);

        float2 twoOverRes;
        twoOverRes.setX(gAppSettings.mRetinaScaling / float(width));
        twoOverRes.setY(gAppSettings.mRetinaScaling / float(height));
        /************************************************************************/
        // Lighting data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.camPos = v4ToF4(vec4(gActiveCamera->pCameraController->getViewPosition()));
        currentFrame->gPerFrameUniformData.lightDir = v4ToF4(vec4(lightDir));
        currentFrame->gPerFrameUniformData.twoOverRes = twoOverRes;
        currentFrame->gPerFrameUniformData.specularQuality = gAppSettings.useSpecular ? gAppSettings.specularQuality : 0;
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

        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_CAMERA].sampleCount = 1;
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
        currentFrame->gPerFrameUniformData.CameraPlane = { gAppSettings.nearPlane, gAppSettings.farPlane };
        /************************************************************************/
        // Skybox
        /************************************************************************/
        cameraView.setTranslation(vec3(0));
        currentFrame->gUniformDataSky.mCamPos = gActiveCamera->pCameraController->getViewPosition();
        currentFrame->gUniformDataSky.mProjectView = cameraProj.mCamera * cameraView;

        if (gAppSettings.useLPV)
        {
            calculateRSMCascadesForLPV(lightView, gRSMCascadeCount, &currentFrame->gRSMCascadeProjection[0],
                                       &currentFrame->gRSMCascadeView[0], &currentFrame->gRSMViewSize[0]);

            for (uint32_t i = 0; i < gRSMCascadeCount; i++)
            {
                uint32_t viewID = VIEW_RSM_CASCADE0 + i;

                /************************************************************************/
                // Matrix data
                /************************************************************************/
                CameraMatrix cascadeProjection = currentFrame->gRSMCascadeProjection[i];
                mat4         cascadeTransform = currentFrame->gRSMCascadeView[i];
                currentFrame->gPerFrameVBUniformData.transform[viewID].projection = cascadeProjection;
                currentFrame->gPerFrameVBUniformData.transform[viewID].vp = cascadeProjection * cascadeTransform;
                currentFrame->gPerFrameVBUniformData.transform[viewID].invVP =
                    CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[viewID].vp);
                currentFrame->gPerFrameVBUniformData.transform[viewID].mvp =
                    currentFrame->gPerFrameVBUniformData.transform[viewID].vp * cameraModel;
                /************************************************************************/
                // Culling data
                /************************************************************************/
                currentFrame->gPerFrameVBUniformData.cullingViewports[viewID].sampleCount = 1;
                currentFrame->gPerFrameVBUniformData.cullingViewports[viewID] = { float2((float)gRSMResolution, (float)gRSMResolution), 1 };

                // Cache eye position in object space for cluster culling on the CPU
                currentFrame->gEyeObjectSpace[viewID] = (inverse(cascadeTransform * cameraModel) * vec4(0, 0, 0, 1)).getXYZ();
            }
        }
        /************************************************************************/
        // Wind data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.windData.gFabricRandOffset = gWindRandomOffset;
        currentFrame->gPerFrameUniformData.windData.gFabricRandPeriodFactor = gWindRandomFactor;
        currentFrame->gPerFrameUniformData.windData.gFabricAnchorHeight = gFlagAnchorHeight;
        currentFrame->gPerFrameUniformData.windData.gFabricFreeHeight = 0.0f;
        currentFrame->gPerFrameUniformData.windData.gFabricAmplitude = float3(50.0f, 50.f, 0.0f) / SCENE_SCALE;
        currentFrame->gPerFrameUniformData.windData.g_Time = gAccumTime;
    }

    void updateLPVParams() { fillAuraParams(&pAura->mParams, &pAura->mCPUParams); }

    void fillAuraParams(aura::LightPropagationVolumeParams* params, aura::CPUPropagationParams* cpuParams)
    {
        params->bUseMultipleReflections = gAppSettings.useMultipleReflections;
        params->bUseCPUPropagation = gAppSettings.useCPUPropagation;
        params->bUseAdvancedPropagation = gAppSettings.useAdvancedDirectionsGPU;
        params->bAlternateGPUUpdates = gAppSettings.alternateGPUUpdates;
        params->fPropagationScale = gAppSettings.propagationScale;
        params->bDebugLight = false;
        params->bDebugOccluder = false;

        params->fGIStrength = gAppSettings.giStrength;
        params->iPropagationSteps = NSTEPS_VALS[gAppSettings.propagationValueIndex];
        params->iSpecularQuality = gAppSettings.useSpecular ? gAppSettings.specularQuality : 0;

        params->fLightScale[0] = gAppSettings.lightScale0;
        params->fLightScale[1] = gAppSettings.lightScale1;
        params->fLightScale[2] = gAppSettings.lightScale2;
        params->fSpecPow = gAppSettings.specPow;
        params->fFresnel = gAppSettings.specFresnel;
        params->fSpecScale = gAppSettings.specScale;
        params->userDebug = 0;

        cpuParams->eMTMode = aura::MT_ExtremeTasks;
        cpuParams->bDecoupled = gAppSettings.useCPUPropagationDecoupled;
        cpuParams->bAdvancedDirections = gAppSettings.useAdvancedDirectionsCPU;
    }

    /************************************************************************/
    // UI
    /************************************************************************/
    void updateDynamicUIElements()
    {
        uiSetComponentActive(pAuraDebugTexturesWindow, gAppSettings.mDrawDebugTargets);
        uiSetComponentActive(pShadowTexturesWindow, gAppSettings.mDrawShadowTargets);
#if TEST_RSM
        uiSetComponentActive(pAuraRSMTexturesWindow, gAppSettings.mDrawRSMTargets);
#endif

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
    }
    /************************************************************************/
    // Rendering
    /************************************************************************/
    void drawShadowMapPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        // Start render pass and apply load actions
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
    }

    void drawAura(Cmd* cmd, uint32_t frameIdx)
    {
        /************************************************************************/
        // Fill LPVs
        /************************************************************************/
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "AURA Fill LPVs");
        updateAuraCascades(cmd, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

        /************************************************************************/
        //// Light Propagation
        /************************************************************************/
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "AURA Propagate Light");
        propagateLight(cmd, pRenderer, pTaskManager, pAura);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

        cmdBindRenderTargets(cmd, NULL);
    }

    void updateAuraCascades(Cmd* cmd, uint32_t frameIdx)
    {
        uint32_t CascadesToUpdateMask = getCascadesToUpdateMask(pAura);

        for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
        {
            //  Igor: cascade is not updated so don't prepare and inject data.
            if (!(CascadesToUpdateMask & (0x0001 << i)))
                continue;

#if TEST_RSM
            RenderTargetBarrier renderTargetBarriers[] = {
                { pRenderTargetRSMAlbedo[i], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetRSMNormal[i], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetRSMDepth[i], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
            };
#else
            RenderTargetBarrier renderTargetBarriers[] = {
                { pRenderTargetRSMAlbedo, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetRSMNormal, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetRSMDepth, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
            };
#endif

            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, renderTargetBarriers);

            char label[32]; // Increased array size to 32 to avoid overflow warning (-Wformat-overflow)
            sprintf(label, "Fill RSM) Cascade #%u", i);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], label);
            fillRSM(cmd, frameIdx, gGpuProfileTokens[0], i);
            cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

#if TEST_RSM
            RenderTargetBarrier srvBarriers[] = {
                { pRenderTargetRSMAlbedo[i], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                { pRenderTargetRSMNormal[i], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                { pRenderTargetRSMDepth[i], RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
            };
#else
            RenderTargetBarrier srvBarriers[] = {
                { pRenderTargetRSMAlbedo, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                { pRenderTargetRSMNormal, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                { pRenderTargetRSMDepth, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
            };
#endif
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, srvBarriers);

            CameraMatrix cascadeViewProjection = gPerFrame[frameIdx].gPerFrameVBUniformData.transform[VIEW_RSM_CASCADE0 + i].vp;
            // Convert into worldspace directly from [0..1] range instead of [-1..1] clip-space range
            mat4         inverseCascadeViewProjection =
                inverse(cascadeViewProjection.mCamera) * mat4::translation(vec3(-1.0f, 1.0f, 0.0f)) * mat4::scale(vec3(2.0f, -2.0f, 1.0f));

            vec4       camDir = normalize(gPerFrame[frameIdx].gRSMCascadeView[i].getRow(2));
            aura::vec3 cameraDir(camDir.getX(), camDir.getY(), camDir.getZ());

            float viewArea = gPerFrame[frameIdx].gRSMViewSize[i] * gPerFrame[frameIdx].gRSMViewSize[i];

#if TEST_RSM
            sprintf(label, "Inject RSM) Cascade #%u", i);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], label);
            injectRSM(cmd, pRenderer, pAura, i, aura::convertMatrix4FromClient((float*)&inverseCascadeViewProjection), cameraDir,
                      gRSMResolution, gRSMResolution, viewArea, pRenderTargetRSMAlbedo[i]->pTexture, pRenderTargetRSMNormal[i]->pTexture,
                      pRenderTargetRSMDepth[i]->pTexture);
            cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
#else
            sprintf(label, "Inject RSM) Cascade #%u", i);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], label);
            injectRSM(cmd, pRenderer, pAura, i, aura::convertMatrix4FromClient((float*)&inverseCascadeViewProjection), cameraDir,
                      gRSMResolution, gRSMResolution, viewArea, pRenderTargetRSMAlbedo->pTexture, pRenderTargetRSMNormal->pTexture,
                      pRenderTargetRSMDepth->pTexture);
            cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

#endif
        }
    }

    // Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
    // into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
    // to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
    // less memory bandwidth is used.
    void drawVisibilityBufferPass(Cmd* cmd, ProfileToken nGpuProfileToken, uint32_t frameIdx)
    {
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetVBPass, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
        cmdBindIndexBuffer(cmd, pIndexBuffer, pGeom->mIndexType, 0);

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, gProfileNames[i]);

            cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
            cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
    {
        RenderTargetBarrier* pBarriers =
            (RenderTargetBarrier*)alloca(NUM_GRIDS_PER_CASCADE * pAura->mCascadeCount * sizeof(RenderTargetBarrier));
        for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
        {
            for (uint32_t j = 0; j < NUM_GRIDS_PER_CASCADE; ++j)
            {
                pBarriers[i * NUM_GRIDS_PER_CASCADE + j] = { pAura->pCascades[i]->pLightGrids[j], RESOURCE_STATE_LPV,
                                                             RESOURCE_STATE_SHADER_RESOURCE };
            }
        }
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE, pBarriers);

        mat4 invMvp = CameraMatrix::inverse(mat4::scale(vec3(0.5, -0.5, 1)) * mat4::translation(vec3(1, -1, 0)) *
                                            gPerFrame[frameIdx].gPerFrameVBUniformData.transform[VIEW_CAMERA].vp)
                          .mCamera;
        aura::vec3 camPos =
            aura::vec3(gPerFrame[frameIdx].gPerFrameUniformData.camPos.getX(), gPerFrame[frameIdx].gPerFrameUniformData.camPos.getY(),
                       gPerFrame[frameIdx].gPerFrameUniformData.camPos.getZ());

        LightApplyData lightApplyData = {};
        aura::getLightApplyData(pAura, aura::convertMatrix4FromClient((float*)&invMvp), camPos, &lightApplyData);
        BufferUpdateDesc update = { pUniformBufferAuraLightApply[frameIdx] };
        beginUpdateResource(&update);
        if (update.pMappedData)
        {
            memcpy(update.pMappedData, &lightApplyData, sizeof(lightApplyData));
        }
        endUpdateResource(&update);

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pScreenRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

        int pipelineIndex = 2 * (int)gAppSettings.useLPV + (int)gAppSettings.enableSun;
        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[pipelineIndex]);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBShade[1]);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
        {
            for (uint32_t j = 0; j < NUM_GRIDS_PER_CASCADE; ++j)
            {
                pBarriers[i * NUM_GRIDS_PER_CASCADE + j] = { pAura->pCascades[i]->pLightGrids[j], RESOURCE_STATE_SHADER_RESOURCE,
                                                             RESOURCE_STATE_LPV };
            }
        }
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE, pBarriers);
    }

    void fillRSM(Cmd* cmd, uint32_t frameIdx, ProfileToken nGpuProfileToken, uint32_t cascadeIndex)
    {
        UNREF_PARAM(nGpuProfileToken);
        uint32_t viewId = VIEW_RSM_CASCADE0 + cascadeIndex;

        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 2;
#if TEST_RSM
        RenderTarget* pRSMRTs[] = { pRenderTargetRSMAlbedo[cascadeIndex], pRenderTargetRSMNormal[cascadeIndex] };
        bindRenderTargets.mDepthStencil = { pRenderTargetRSMDepth[cascadeIndex], LOAD_ACTION_CLEAR };
#else
        RenderTarget* pRSMRTs[] = { pRenderTargetRSMAlbedo, pRenderTargetRSMNormal };
        bindRenderTargets.mDepthStencil = { pRenderTargetRSMDepth, LOAD_ACTION_CLEAR };
#endif
        for (uint32_t i = 0; i < 2; ++i)
        {
            bindRenderTargets.mRenderTargets[i] = { pRSMRTs[i], LOAD_ACTION_CLEAR };
        }

        // Start render pass and apply load actions
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRSMRTs[0]->mWidth, (float)pRSMRTs[0]->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRSMRTs[0]->mWidth, pRSMRTs[0]->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + viewId];
        cmdBindIndexBuffer(cmd, pIndexBuffer, pGeom->mIndexType, 0);

        struct RSMConstants
        {
            uint32_t viewId;
            uint32_t useColorMaps;
        };

        for (uint32_t i = 0; i < gNumGeomSets; ++i)
        {
            cmdBindPipeline(cmd, pPipelineFillRSM[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
            cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);

            RSMConstants constantData = { viewId, gAppSettings.useColorMaps };
            cmdBindPushConstants(cmd, pRootSignatureVBPass, gVBPassRootConstantIndex, &constantData);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(viewId, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
        }

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
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "Shadow Pass");

        RenderTargetBarrier rtBarriers[] = { { pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                                             { pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

        drawShadowMapPass(cmd, gGpuProfileTokens[0], frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "VB Filling Pass");
        drawVisibilityBufferPass(cmd, gGpuProfileTokens[0], frameIdx);

        const int           barrierCount = 3;
        RenderTargetBarrier barriers[barrierCount] = {};
        barriers[0] = { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        barriers[1] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
        barriers[2] = { pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };

        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, barrierCount, barriers);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

        if (gAppSettings.useLPV || gDebugVisualizationSettings.isEnabled)
        {
            drawAura(cmd, frameIdx);
        }

        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "VB Shading Pass");
        drawVisibilityBufferShade(cmd, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

        if (gDebugVisualizationSettings.isEnabled)
        {
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "LPV Visualization");
            CameraMatrix projection = gPerFrame[frameIdx].gPerFrameVBUniformData.transform[VIEW_CAMERA].projection;
            mat4         view = gPerFrame[frameIdx].gCameraView;
            mat4         invView = inverse(view);
            aura::mat4   inverseView = aura::convertMatrix4FromClient((float*)&invView);

            RenderTarget* lpvVisualisationRenderTarget = pScreenRenderTarget;

            mat4 projectMat = projection.mCamera;
            aura::drawLpvVisualization(cmd, pRenderer, pAura, lpvVisualisationRenderTarget, pDepthBuffer,
                                       aura::convertMatrix4FromClient((float*)&projectMat), aura::convertMatrix4FromClient((float*)&view),
                                       inverseView, gDebugVisualizationSettings.cascadeIndex, gDebugVisualizationSettings.probeRadius);
            cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
        }
    }

    void drawSkybox(Cmd* cmd, int frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "Draw Skybox");

        // Load RT instead of Don't care
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

        cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
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

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

        if (gAppSettings.mAsyncCompute)
        {
            if (!gAppSettings.mHoldFilteredResults)
            {
                cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGpuProfileTokens[1], &gFrameTimeDraw);
                cmdDrawGpuProfile(cmd, float2(8.0f, 425.0f), gGpuProfileTokens[0], &gFrameTimeDraw);
            }
            else
            {
                cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGpuProfileTokens[0], &gFrameTimeDraw);
            }
        }
        else
        {
            cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGpuProfileTokens[0], &gFrameTimeDraw);
        }

        cmdDrawUserInterface(cmd);

        cmdBindRenderTargets(cmd, NULL);
    }

    void SetupGuiWindows(float2 screenSize)
    {
        SetupDebugTexturesWindow(screenSize);
        SetupDebugShadowTexturesWindow(screenSize);
#if TEST_RSM
        SetupDebugRSMTexturesWindow(screenSize);
#endif
    }

    void SetupAppSettingsWindow(float2 screenSize)
    {
        UIComponentDesc UIComponentDesc = {};
        UIComponentDesc.mStartPosition.setY(screenSize.getY() / 20.0f);
        uiCreateComponent("App Settings", &UIComponentDesc, &pGuiWindow);

        CheckboxWidget holdFilteredResults;
        holdFilteredResults.pData = &gAppSettings.mHoldFilteredResults;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Hold Filtered Results", &holdFilteredResults, WIDGET_TYPE_CHECKBOX));

        CheckboxWidget asyncCompute;
        asyncCompute.pData = &gAppSettings.mAsyncCompute;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Async compute", &asyncCompute, WIDGET_TYPE_CHECKBOX));

        // Light Settings
        //---------------------------------------------------------------------------------
        // offset max angle for sun control so the light won't bleed with
        // small glancing angles, i.e., when lightDir is almost parallel to the plane
        // const float maxAngleOffset = 0.017f;
        SliderFloat2Widget sun;
        sun.pData = &gDirectionalLight.mRotationXY;
        sun.mMin = float2(-PI);
        sun.mMax = float2(PI);
        sun.mStep = float2(0.001f);
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Directional Light Control", &sun, WIDGET_TYPE_SLIDER_FLOAT2));

        CheckboxWidget localLight;
        localLight.pData = &gAppSettings.mRenderLocalLights;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Random Point Lights", &localLight, WIDGET_TYPE_CHECKBOX));

        /************************************************************************/
        // Rendering Settings
        /************************************************************************/

#if TEST_RSM
        CheckboxWidget debugRSMTargets;
        debugRSMTargets.pData = &gAppSettings.mDrawRSMTargets;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw RSM Targets", &debugRSMTargets, WIDGET_TYPE_CHECKBOX));
#endif

        CheckboxWidget debugTargets;
        debugTargets.pData = &gAppSettings.mDrawDebugTargets;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw Debug Targets", &debugTargets, WIDGET_TYPE_CHECKBOX));

        CheckboxWidget debugShadowTargets;
        debugShadowTargets.pData = &gAppSettings.mDrawShadowTargets;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw Shadow Targets", &debugShadowTargets, WIDGET_TYPE_CHECKBOX));

        CheckboxWidget cameraProp;
        cameraProp.pData = &gAppSettings.cameraWalking;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking", &cameraProp, WIDGET_TYPE_CHECKBOX));

        SliderFloatWidget cameraSpeedProp;
        cameraSpeedProp.pData = &gAppSettings.cameraWalkingSpeed;
        cameraSpeedProp.mMin = 0.0f;
        cameraSpeedProp.mMax = 3.0f;
        luaRegisterWidget(
            uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraSpeedProp, WIDGET_TYPE_SLIDER_FLOAT));

        // Enable this to customize flag movement if the flags dont move as expected in a newer San Miguel model
#ifdef TEST_FLAG_SETTINGS
        const int NUM_FLAG_SETTINGS = 3;
        UIWidget  flagSettings[NUM_FLAG_SETTINGS];
        UIWidget* flagSettingsGroup[NUM_FLAG_SETTINGS];
        for (int i = 0; i < NUM_FLAG_SETTINGS; ++i)
        {
            flagSettings[i] = UIWidget{};
            flagSettingsGroup[i] = &flagSettings[i];
        }

        CollapsingHeaderWidget CollapsingFlagSettings;
        CollapsingFlagSettings.pGroupedWidgets = flagSettingsGroup;
        CollapsingFlagSettings.mWidgetsCount = NUM_FLAG_SETTINGS;

        SliderFloatWidget anchorH;
        anchorH.pData = &gFlagAnchorHeight;
        anchorH.mMin = -1000.0f;
        anchorH.mMax = 1000.0f;
        strcpy(flagSettingsGroup[0]->mLabel, "Anchor H");
        flagSettingsGroup[0]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        flagSettingsGroup[0]->pWidget = &anchorH;

        SliderFloatWidget randO;
        randO.pData = &gWindRandomOffset;
        randO.mMin = 0.0f;
        randO.mMax = 200.0f;
        strcpy(flagSettingsGroup[1]->mLabel, "Rand O");
        flagSettingsGroup[1]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        flagSettingsGroup[1]->pWidget = &randO;

        SliderFloatWidget randF;
        randF.pData = &gWindRandomFactor;
        randF.mMin = 0.0f;
        randF.mMax = 5000.0f;
        strcpy(flagSettingsGroup[2]->mLabel, "Rand F");
        flagSettingsGroup[2]->mType = WIDGET_TYPE_SLIDER_FLOAT;
        flagSettingsGroup[2]->pWidget = &randF;

        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Flag Settings", &CollapsingFlagSettings, WIDGET_TYPE_COLLAPSING_HEADER));
#endif
    }

    void SetupAuraSettingsWindow(float2 screenSize)
    {
        UIComponentDesc auraUIComponentDesc = {};
        auraUIComponentDesc.mStartPosition = vec2(screenSize.getX() / 2.0f, screenSize.getY() / 5.0f);
        uiCreateComponent("Aura Settings", &auraUIComponentDesc, &pAuraGuiWindow);

        /************************************************************************/
        // Aura Gui Settings
        /************************************************************************/
        CheckboxWidget    checkbox;
        SliderFloatWidget sliderFloat;

#ifdef ENABLE_CPU_PROPAGATION
        checkbox.pData = &gAppSettings.useCPUPropagation;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "CPU Propagation", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.useAdvancedDirectionsCPU;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Use CPU Advanced Directions", &checkbox, WIDGET_TYPE_CHECKBOX));
#endif
        checkbox.pData = &gAppSettings.useAdvancedDirectionsGPU;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Use GPU Advanced Directions", &checkbox, WIDGET_TYPE_CHECKBOX));

        // pAuraGuiWindow->AddWidget(UIProperty("CPU Asynchronous Mapping", gAppSettings.useCPUAsyncMapping));
        checkbox.pData = &gAppSettings.alternateGPUUpdates;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Alternate GPU Updates", &checkbox, WIDGET_TYPE_CHECKBOX));

        // pAuraGuiWindow->AddWidget(UIProperty("Multiple Bounces", useMultipleReflections)); // NOTE: broken! It's also broken in the
        // original LightPropagationVolumes in the "middleware" repo.
        sliderFloat.pData = &gAppSettings.propagationScale;
        sliderFloat.mMin = 1.0f;
        sliderFloat.mMax = 2.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Propagation Scale (GPU)", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.giStrength;
        sliderFloat.mMin = 0.5f;
        sliderFloat.mMax = 20.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "GI Strength", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        DropdownWidget propagationSteps;
        propagationSteps.pData = &gAppSettings.propagationValueIndex;
        propagationSteps.pNames = NSTEPS_STRINGS;
        propagationSteps.mCount = sizeof(NSTEPS_VALS) / sizeof(NSTEPS_VALS[0]);
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Propagation Steps", &propagationSteps, WIDGET_TYPE_DROPDOWN));

        DropdownWidget specularQuality;
        specularQuality.pData = &gAppSettings.specularQuality;
        specularQuality.pNames = aura::SPECULAR_QUALITY_STRINGS;
        specularQuality.mCount = sizeof(aura::SPECULAR_QUALITY_STRINGS) / sizeof(aura::SPECULAR_QUALITY_STRINGS[0]);
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Specular Quality", &specularQuality, WIDGET_TYPE_DROPDOWN));

        checkbox.pData = &gAppSettings.enableSun;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Sun Radiance", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.useLPV;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Use GI", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.useSpecular;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Use Specular", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAppSettings.useColorMaps;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Use Diffuse Mapping", &checkbox, WIDGET_TYPE_CHECKBOX));

        sliderFloat.pData = &gAppSettings.lightScale0;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 1.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Light Scale 0", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.lightScale1;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 5.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Light Scale 1", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.lightScale2;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 10.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Light Scale 2", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.specPow;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 100.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Specular Power", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.specScale;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 50.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Specular Scale", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

        sliderFloat.pData = &gAppSettings.specFresnel;
        sliderFloat.mMin = 0.0f;
        sliderFloat.mMax = 5.0f;
        sliderFloat.mStep = 0.05f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Specular Fresnel", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));
    }

    void SetupDebugTexturesWindow(float2 screenSize)
    {
        float  scale = 0.15f;
        float2 texSize = screenSize * scale;

        if (!pAuraDebugTexturesWindow)
        {
            UIComponentDesc UIComponentDesc = {};
            UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
            UIComponentDesc.mStartPosition.setY(screenSize.getY() - texSize.getY() - 100.0f);
            uiCreateComponent("DEBUG RTs", &UIComponentDesc, &pAuraDebugTexturesWindow);

            DebugTexturesWidget widget;
            luaRegisterWidget(uiCreateComponentWidget(pAuraDebugTexturesWindow, "Debug RTs", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
        }

        static const Texture* pVBRTs[2];
        pVBRTs[0] = pRenderTargetVBPass->pTexture;
        pVBRTs[1] = pDepthBuffer->pTexture;

        if (pAuraDebugTexturesWindow)
        {
            ((DebugTexturesWidget*)(pAuraDebugTexturesWindow->mWidgets[0]->pWidget))->pTextures = pVBRTs;
            ((DebugTexturesWidget*)(pAuraDebugTexturesWindow->mWidgets[0]->pWidget))->mTexturesCount = 2;
            ((DebugTexturesWidget*)(pAuraDebugTexturesWindow->mWidgets[0]->pWidget))->mTextureDisplaySize = texSize;
        }
    }

    void SetupDebugVisualizationWindow(float2 screenSize)
    {
        UIComponentDesc UIComponentDesc = {};
        UIComponentDesc.mStartPosition = vec2(screenSize.getX() / 2.0f, screenSize.getY() / (20.0f));
        uiCreateComponent("Debug Visualization Settings", &UIComponentDesc, &pAuraDebugVisualizationGuiWindow);

        CheckboxWidget enabled;
        enabled.pData = &gDebugVisualizationSettings.isEnabled;
        luaRegisterWidget(uiCreateComponentWidget(pAuraDebugVisualizationGuiWindow, "Enabled", &enabled, WIDGET_TYPE_CHECKBOX));

        SliderIntWidget cascadeNum;
        cascadeNum.pData = &gDebugVisualizationSettings.cascadeIndex;
        cascadeNum.mMin = 0;
        cascadeNum.mMax = pAura->mCascadeCount - 1;
        luaRegisterWidget(uiCreateComponentWidget(pAuraDebugVisualizationGuiWindow, "Cascade #", &cascadeNum, WIDGET_TYPE_SLIDER_INT));

        SliderFloatWidget probeSize;
        probeSize.pData = &gDebugVisualizationSettings.probeRadius;
        probeSize.mMin = 0.5f;
        probeSize.mMax = 6.0f;
        probeSize.mStep = 0.5f;
        luaRegisterWidget(uiCreateComponentWidget(pAuraDebugVisualizationGuiWindow, "Probe Size", &probeSize, WIDGET_TYPE_SLIDER_FLOAT));
    }

    void SetupDebugShadowTexturesWindow(float2 screenSize)
    {
        if (!pShadowTexturesWindow)
        {
            UIComponentDesc UIComponentDesc = {};
            UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
            UIComponentDesc.mStartPosition = vec2(screenSize.getX() * 0.15f, screenSize.getY() * 0.4f);
            uiCreateComponent("Shadow Textures", &UIComponentDesc, &pShadowTexturesWindow);

            DebugTexturesWidget widget;
            luaRegisterWidget(uiCreateComponentWidget(pShadowTexturesWindow, "Shadow Textures", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
        }

        if (pShadowTexturesWindow)
        {
            const float scale = 0.15f;
            ((DebugTexturesWidget*)(pShadowTexturesWindow->mWidgets[0]->pWidget))->pTextures = &pRenderTargetShadow->pTexture;
            ((DebugTexturesWidget*)(pShadowTexturesWindow->mWidgets[0]->pWidget))->mTexturesCount = 1;
            ((DebugTexturesWidget*)(pShadowTexturesWindow->mWidgets[0]->pWidget))->mTextureDisplaySize = screenSize * scale;
        }
    }

#if TEST_RSM
    void SetupDebugRSMTexturesWindow(float2 screenSize)
    {
        float  scale = 0.15f;
        float2 texSize = screenSize * scale;

        if (!pAuraRSMTexturesWindow)
        {
            UIComponentDesc UIComponentDesc = {};
            UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
            UIComponentDesc.mStartPosition = vec2(screenSize.getX() * 0.15f, screenSize.getY() * 0.4f);
            uiCreateComponent("RSM Textures", &UIComponentDesc, &pAuraRSMTexturesWindow);
            ASSERT(pAuraRSMTexturesWindow);

            DebugTexturesWidget widget;
            luaRegisterWidget(uiCreateComponentWidget(pAuraRSMTexturesWindow, "RSM Textures", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
        }

        static const Texture* pRSMRTs[NUM_RSM_CASCADES];
        for (int i = 0; i < NUM_RSM_CASCADES; ++i)
        {
            pRSMRTs[i] = pRenderTargetRSMAlbedo[i]->pTexture;
        }

        if (pAuraRSMTexturesWindow)
        {
            ((DebugTexturesWidget*)(pAuraRSMTexturesWindow->mWidgets[0]->pWidget))->pTextures = pRSMRTs;
            ((DebugTexturesWidget*)(pAuraRSMTexturesWindow->mWidgets[0]->pWidget))->mTexturesCount = NUM_RSM_CASCADES;
            ((DebugTexturesWidget*)(pAuraRSMTexturesWindow->mWidgets[0]->pWidget))->mTextureDisplaySize = texSize;
        }
    }
#endif

    void calculateRSMCascadesForLPV(const mat4& lightView, int cascadeCount, CameraMatrix* cascadeProjections, mat4* cascadeTransforms,
                                    float* viewSize)
    {
        aura::Box  boundsLightspace[gRSMCascadeCount] = {};
        aura::mat4 worldToLight = aura::convertMatrix4FromClient((float*)&lightView);
        aura::getGridBounds(pAura, worldToLight, boundsLightspace);

        for (int i = 0; i < cascadeCount; i++)
        {
            aura::Box cascadeBoundsLightspace = boundsLightspace[i];
            // float farPlane = cascadeBoundsLightspace.vMax.z - cascadeBoundsLightspace.vMin.z;

            float longestDiagonal = sqrt(3.0f) * gLPVCascadeSpans[i];

            float size = longestDiagonal * ((float)gRSMResolution) / ((float)(gRSMResolution - 1));
            float texelSize = size / ((float)gRSMResolution);

            vec3 lightSpacePosition =
                vec3((cascadeBoundsLightspace.vMin.x + cascadeBoundsLightspace.vMax.x) * 0.5f,
                     (cascadeBoundsLightspace.vMin.y + cascadeBoundsLightspace.vMax.y) * 0.5f, cascadeBoundsLightspace.vMin.z);
            vec3 lightSpaceBottomLeft =
                vec3(lightSpacePosition.getX() - size * 0.5f, lightSpacePosition.getY() - size * 0.5f, lightSpacePosition.getZ());

            lightSpaceBottomLeft.setX(floor(lightSpaceBottomLeft.getX() / texelSize) * texelSize);
            lightSpaceBottomLeft.setY(floor(lightSpaceBottomLeft.getY() / texelSize) * texelSize);
            lightSpacePosition = lightSpaceBottomLeft + vec3(size * 0.5f, size * 0.5f, 0.0f);

            mat4 translation = mat4::translation(-(inverse(lightView) * vec4(lightSpacePosition, 1)).getXYZ());
            mat4 worldToCascade = lightView * translation;

            float halfSize = size * 0.5f;
            cascadeProjections[i] = CameraMatrix::orthographic(-halfSize, halfSize, -halfSize, halfSize, -5000.0f, 5000.0f);
            cascadeTransforms[i] = worldToCascade;
            viewSize[i] = size;
        }
    }
};

DEFINE_APPLICATION_MAIN(AuraApp)
