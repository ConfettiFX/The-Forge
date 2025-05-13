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

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

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
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Resources/AnimationSystem/Animation/AnimatedObject.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Animation.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Clip.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/ClipController.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Rig.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/SkeletonBatcher.h"
#include "../../../../Common_3/Utilities/CustomUIWidgets.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../Visibility_Buffer/src/SanMiguel.h"

// fsl

#define NO_FSL_DEFINITIONS
#include "../../../../Common_3/Graphics/FSL/fsl_srt.h"
#include "Shaders/FSL/ShaderDefs.h.fsl"
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "./Shaders/FSL/Display.srt.h"
#include "./Shaders/FSL/GodrayBlur.srt.h"
#include "./Shaders/FSL/PreSkinVertexes.srt.h"
#include "./Shaders/FSL/VisibilityBufferPassTransparent.srt.h"
#include "./Shaders/FSL/Global.srt.h"
#include "./Shaders/FSL/ResolveVRS.srt.h"
#include "./Shaders/FSL/LightClusters.srt.h"
#include "./Shaders/FSL/TriangleFiltering.srt.h"
#include "./Shaders/FSL/ProgMSAAResolve.srt.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)       \
    X(BindlessSupported, 1)      \
    X(DisableGodRays, 0)         \
    X(MSAASampleCount, 4)        \
    X(AddGeometryPassThrough, 0) \
    X(MaxMSAALevel, 4)

#define GENERATE_ENUM(x, y)   x,
#define GENERATE_STRING(x, y) #x,
#define GENERATE_STRUCT(x, y) uint32_t m##x;
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

#define SCENE_SCALE 10.0f

typedef enum OutputMode
{
    OUTPUT_MODE_SDR = 0,
    OUTPUT_MODE_HDR10,

    OUTPUT_MODE_COUNT
} OutputMode;

struct TonemapInfo
{
    float linearScale;
    uint  outputMode;
};

TonemapInfo gTonemapInformation;

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

FORGE_CONSTEXPR uint32_t MSAA_STENCIL_MASK = 1;
FORGE_CONSTEXPR uint32_t VRS_STENCIL_MASK = 1;

//--------------------------------------------------------------------------------------------
// STATIC MESH DATA
//--------------------------------------------------------------------------------------------

struct StaticMeshInstance
{
    // Initialization data, cannot be changed after Init/Load
    const uint32_t mGeomSet;

    float mRotationSpeedYDeg;

    // Instance dynamic data
    vec3 mTranslation;
    vec3 mScale;
    Quat mRotation;
};

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

// VRS related constants
uint32_t        gDivider = 2;
SampleLocations gLocations[4] = { { -4, -4 }, { 4, -4 }, { -4, 4 }, { 4, 4 } };
#define GEOMSET_ALPHA_CUTOUT_VRS NUM_GEOMETRY_SETS

//--------------------------------------------------------------------------------------------
// ANIMATION DATA
//--------------------------------------------------------------------------------------------

// Filenames
const char* gAnimatedMeshFile = "stormtrooper/riggedMesh.bin"; // Also used for instances
const char* gSkeletonFile = "stormtrooper/skeleton.ozz";
const char* gClipFile = "stormtrooper/animations/dance.ozz";

struct AnimatedMeshInstance
{
    const uint32_t mGeomSet;
    const float    mPlaybackSpeed;

    // Instance dynamic data
    vec3 mTranslation;
    vec3 mScale;
    Quat mRotation;

    AnimatedObject mAnimObject;
    Animation      mAnimation;
    ClipController mClipController;

    const GeometryData* pGeomData;
    mat4*               pBoneMatrixes; // Size of this array is the size of Rig::mNumJoints

    uint32_t mJointMatrixOffset;
    uint32_t mPreSkinnedVertexOffset; // Relative to the first pre-skinned vertex in the buffer
};

Clip gClip;
Rig  gRig;

GeometryData* pAnimatedMeshGeomData;

// For now instances are hardcoded
StaticMeshInstance gStaticMeshInstances[] = {
    // 218 is the index for stormtrooper, the main scene has 218 meshes (index 217 contains the last mesh), stormtrooper mesh is placed
    // afterwards
    { GEOMSET_OPAQUE, 30.f, vec3(6.5f, 2.3f, 1.f), vec3(0.5f), Quat::identity() },
    { GEOMSET_OPAQUE, -50.f, vec3(8.f, 2.4f, 0.f), vec3(0.5f), Quat::identity() },
    { GEOMSET_OPAQUE, 60.f, vec3(4.f, 2.4f, 0.f), vec3(0.5f), Quat::identity() },
};

AnimatedMeshInstance gAnimatedMeshInstances[]{ { GEOMSET_OPAQUE, 2.f, vec3(5.f, 2.3f, 1.f), vec3(0.5f), Quat::rotationY(degToRad(80.f)) },
                                               { GEOMSET_OPAQUE, 1.f, vec3(9.f, 2.3f, 2.f), vec3(0.5f), Quat::rotationY(degToRad(270.f)) },
                                               { GEOMSET_OPAQUE, 0.5f, vec3(9.f, 2.3f, 4.f), vec3(0.5f),
                                                 Quat::rotationY(degToRad(240.f)) } };

const uint32_t gStaticMeshInstanceCount = TF_ARRAY_COUNT(gStaticMeshInstances);
const uint32_t gAnimatedMeshInstanceCount = TF_ARRAY_COUNT(gAnimatedMeshInstances);
const uint32_t gMaxAnimatedInstances = gAnimatedMeshInstanceCount;
const uint32_t gMaxMeshInstances = TF_ARRAY_COUNT(gStaticMeshInstances) + gMaxAnimatedInstances;
const uint32_t gMaxJointMatrixes =
    gAnimatedMeshInstanceCount * 128; // Assume a max of 128 joints per animated instance (stormtrooper has just 70)
uint32_t gSceneMeshCount = 0;
uint32_t gPreSkinnedVertexCountPerFrame = 0;
uint32_t gPreSkinnedVertexStartOffset = 0;

// Camera Walking
static float gCameraWalkingTime = 0.0f;
float3*      gCameraPathData;

uint  gCameraPoints;
float gTotalElpasedTime;
/************************************************************************/
// GUI CONTROLS
/************************************************************************/
#define MSAA_LEVELS_COUNT 3U

#if defined(ANDROID) || defined(NX64)
#define DEFAULT_ASYNC_COMPUTE false
#else
#define DEFAULT_ASYNC_COMPUTE true
#endif

// PVS-Studio V802 On 64-bit platform, structure size can be reduced from 136 to 120 bytes by rearranging the fields according to their
// sizes in decreasing order."
typedef struct AppSettings
{ //-V802
    OutputMode mOutputMode = OUTPUT_MODE_SDR;

    bool mUpdateSimulation = true;

    // Set this variable to true to bypass triangle filtering calculations, holding and representing the last filtered data.
    // This is useful for inspecting filtered geometry for debugging purposes.
    bool mHoldFilteredResults = false;

    bool mAsyncCompute = DEFAULT_ASYNC_COMPUTE;
    // toggle rendering of local point lights
    bool mRenderLocalLights = true;
#if defined(ENABLE_WORKGRAPH)
    bool mGpuPipelineWorkgraph = true;
#endif

    bool mDrawDebugTargets = false;

    bool mEnableVRS = true;

    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // adjust directional sunlight angle
    float2 mSunControl = { -2.1f, 0.164f };

    float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

    UIWidget*        pOutputSupportsHDRWidget = NULL;
    DynamicUIWidgets mDynamicUIWidgetsGR;

    bool     mEnableGodray = true;
    uint32_t mFilterRadius = 3;

    float mEsmControl = 225.0f;

    DynamicUIWidgets mLinearScale;
    float            LinearScale = 260.0f;

    // HDR10
    DynamicUIWidgets mDisplaySetting;

    DisplayColorSpace  mCurrentSwapChainColorSpace = ColorSpace_Rec2020;
    DisplayColorRange  mDisplayColorRange = ColorRange_RGB;
    DisplaySignalRange mDisplaySignalRange = Display_SIGNAL_RANGE_FULL;

    SampleCount mMsaaLevel = SAMPLE_COUNT_2;
    SampleCount mMaxMsaaLevel = SAMPLE_COUNT_4;
    uint32_t    mMsaaIndex = (uint32_t)log2((uint32_t)mMsaaLevel);
    uint32_t    mMsaaIndexRequested = mMsaaIndex;

    // Camera Walking
    bool  cameraWalking = false;
    float cameraWalkingSpeed = 1.0f;

} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/
Scene* pScene = NULL;

/************************************************************************/
// Constants
/************************************************************************/
const uint32_t gShadowMapSize = 1024;
const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

// The number of render targets to be resolved
FORGE_CONSTEXPR uint32_t gResolveTargetCount = 2;
FORGE_CONSTEXPR uint32_t gResolveGodRayPassIndex = 0;
FORGE_CONSTEXPR uint32_t gResolveFinalPassIndex = 1;

FORGE_CONSTEXPR TinyImageFormat gDebugRTFormat = TinyImageFormat_R8G8B8A8_UNORM;

struct UniformDataSkybox
{
    mat4 mProjectView;
    vec3 mCamPos;
};

struct UniformDataSkyboxTri
{
    mat4 mInverseViewProjection;
};

/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3                  gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    PerFrameConstantsData gPerFrameUniformData = {};
    UniformDataSkybox     gUniformDataSky;
    UniformDataSkyboxTri  gUniformDataSkyTri;

    MeshData* pMeshData = NULL;
    mat4      gJointMatrixes[gMaxJointMatrixes] = {};
};

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

Queue*     pComputeQueue = NULL;
GpuCmdRing gComputeCmdRing = {};
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pPresentSemaphore = NULL;

DescriptorSet* pDescriptorSetPersistent = { NULL };
DescriptorSet* pDescriptorSetPerFrame = { NULL };
DescriptorSet* pDescriptorSetDisplayPerDraw = { NULL };
DescriptorSet* pDescriptorSetVisibilityPassTransparentPerBatch = { NULL };
DescriptorSet* pDescriptorSetResolveVRSPerBatch = { NULL };
DescriptorSet* pDescriptorSetClusterLightsPerBatch = { NULL };
DescriptorSet* pDescriptorTriangleFilteringPerBatch = { NULL };
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*        pShaderClearBuffers = nullptr;
Pipeline*      pPipelineClearBuffers = nullptr;
/************************************************************************/
// Pre Skin Vertexes
/************************************************************************/
// Two of each resource for Sync and Async triangle filtering
const uint32_t PRE_SKIN_SYNC = 0;
const uint32_t PRE_SKIN_ASYNC = 1;
Shader*        pShaderPreSkinVertexes[2] = { NULL };
Pipeline*      pPipelinePreSkinVertexes[2] = { NULL };
DescriptorSet* pDescriptorSetPreSkinVertexes[2] = { NULL };
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*        pShaderTriangleFiltering = nullptr;
Pipeline*      pPipelineTriangleFiltering = nullptr;

#if defined(ENABLE_WORKGRAPH)
Shader*    pShaderGpuPipeline = {};
Pipeline*  pPipelineGpuPipeline = {};
Workgraph* pWorkgraphGpuPipeline = {};
#endif
/************************************************************************/
// Clear OIT Head Index pipeline
/************************************************************************/
Shader*   pShaderClearHeadIndexOIT = NULL;
Pipeline* pPipelineClearHeadIndexOIT = NULL;
/************************************************************************/
// Clear light clusters pipeline
/************************************************************************/
Shader*   pShaderClearLightClusters = nullptr;
Pipeline* pPipelineClearLightClusters = nullptr;

/************************************************************************/
// Compute light clusters pipeline
/************************************************************************/
Shader*   pShaderClusterLights = nullptr;
Pipeline* pPipelineClusterLights = nullptr;
/************************************************************************/
// Shadow pass pipeline
/************************************************************************/
Shader*   pShaderShadowPass[NUM_GEOMETRY_SETS] = { NULL };
Pipeline* pPipelineShadowPass[NUM_GEOMETRY_SETS] = { NULL };

/************************************************************************/
// VB pass pipeline
/************************************************************************/
// Extra one for VRS ALPHA_CUTOUT shader
Shader*        pShaderVisibilityBufferPass[NUM_GEOMETRY_SETS + 1] = {};
Pipeline*      pPipelineVisibilityBufferPass[NUM_GEOMETRY_SETS] = {};
/************************************************************************/
// VB shade pipeline
/************************************************************************/
// MSAA + VRS + Godray Variants
const uint32_t kNumVisBufShaderVariants = 4 * (MSAA_LEVELS_COUNT + 1);
Shader*        pShaderVisibilityBufferShade[kNumVisBufShaderVariants] = { nullptr };
Pipeline*      pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
/************************************************************************/
// Resolve pipeline
/************************************************************************/
Shader*        pShaderResolve[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*      pPipelineResolve = nullptr;
Pipeline*      pPipelineResolveGodRay = nullptr;
DescriptorSet* pDescriptorSetResolve = nullptr;

Texture*  pSkyboxTri = NULL;
/************************************************************************/
// Godray pipeline
/************************************************************************/
// MSAA + VRS
Shader*   pGodRayPass[MSAA_LEVELS_COUNT + 1] = { nullptr };
Pipeline* pPipelineGodRayPass = nullptr;
Buffer*   pBufferGodRayConstant = nullptr;
uint32_t  gGodRayConstantIndex = 0;

Shader*        pShaderGodRayBlurPass = nullptr;
Pipeline*      pPipelineGodRayBlurPass = nullptr;
DescriptorSet* pDescriptorSetGodRayBlurPassPerBatch = nullptr;
Buffer*        pBufferBlurWeights = nullptr;
Buffer*        pGodRayBlurBuffer[2][gDataBufferCount] = { { NULL } };
uint32_t       gGodRayBlurConstantIndex = 0;

OutputMode         gWasOutputMode = gAppSettings.mOutputMode;
DisplayColorSpace  gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
DisplayColorRange  gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
DisplaySignalRange gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;

/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*                         pShaderPresentPass = nullptr;
Pipeline*                       pPipelinePresentPass = nullptr;
/************************************************************************/
// Filling VRS map pipeline
/************************************************************************/
Shader*                         pShaderFillStencil = NULL;
Pipeline*                       pPipelineFillStencil = NULL;
/************************************************************************/
// VRS Resolve pipeline
/************************************************************************/
Shader*                         pShaderResolveCompute = NULL;
Pipeline*                       pPipelineResolveCompute = nullptr;
uint32_t                        gShadingRateRootConstantIndex = 0;
/************************************************************************/
// Programmable MSAA resources
/************************************************************************/
Shader*                         pShaderDrawMSAAEdges[MSAA_LEVELS_COUNT - 1] = { nullptr };
Shader*                         pShaderDownscaleMSAAEdges[MSAA_LEVELS_COUNT - 1] = { nullptr };
Pipeline*                       pPipelineDrawMSAAEdges = nullptr;
Pipeline*                       pPipelineDownscaleMSAAEdges = nullptr;
Shader*                         pShaderDebugMSAA[MSAA_LEVELS_COUNT - 1] = { nullptr };
Pipeline*                       pPipelineDebugMSAA = nullptr;
/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget*                   pDepthBuffer = NULL;
RenderTarget*                   pDepthBufferOIT = NULL;
RenderTarget*                   pRenderTargetVBPass = NULL;
RenderTarget*                   pRenderTargetMSAA = NULL;
RenderTarget*                   pRenderTargetShadow = NULL;
RenderTarget*                   pIntermediateRenderTarget = NULL;
RenderTarget*                   pRenderTargetGodRay[2] = { NULL };
RenderTarget*                   pRenderTargetGodRayMS = NULL;
RenderTarget*                   pHistoryRenderTarget[2] = { NULL };
RenderTarget*                   pResolveVRSRenderTarget[gDataBufferCount] = {};
RenderTarget*                   pDebugVRSRenderTarget = NULL;
RenderTarget*                   pRenderTargetMSAAEdges = NULL;
RenderTarget*                   pRenderTargetMSAAEdgesDownscaled = NULL;
RenderTarget*                   pRenderTargetDebugMSAA = NULL;
RenderTarget*                   pRenderTargetDebugGodRayMSAA = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler*                        pSamplerTrilinearAniso = NULL;
Sampler*                        pSamplerPointClamp = NULL;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture**                       gDiffuseMapsStorage = NULL;
Texture**                       gNormalMapsStorage = NULL;
Texture**                       gSpecularMapsStorage = NULL;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
static GeometryBufferLoadDesc   gGeometryBufferLoadDesc;
static uint32_t                 gGeometryBufferVertexBufferCount = 0;
static GeometryBuffer*          pGeometryBuffer = NULL;
static Geometry*                gGeometry[2] = {};
static Geometry*&               pSanMiguelGeometry = gGeometry[0];
static Geometry*&               pTroopGeometry = gGeometry[1];
static GeometryBufferLayoutDesc gGeometryLayout = {};
#define GET_GEOMETRY_VERTEX_BINDING(semantic) (gGeometryLayout.mSemanticBindings[semantic])
#define GET_GEOMETRY_VERTEX_BUFFER(semantic)  (pGeometryBuffer->mVertex[GET_GEOMETRY_VERTEX_BINDING(semantic)].pBuffer)
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer* pMaterialPropertyBuffer = NULL;

enum
{
    VB_UB_COMPUTE = 0,
    VB_UB_GRAPHICS,
    VB_UB_COUNT
};
Buffer* pPerFrameVBUniformBuffers[VB_UB_COUNT][gDataBufferCount] = {};
Buffer* pMeshDataBuffer[gDataBufferCount] = { NULL };
Buffer* pJointMatrixBuffer[gDataBufferCount] = { NULL };
Buffer* pPreSkinBufferOffsets = NULL; // Offsets when not using AsyncCompute

PreSkinACVertexBuffers* pAsyncComputeVertexBuffers = NULL;

/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*          pLightsBuffer = NULL;
Buffer*          pLightClustersCount[gDataBufferCount] = { NULL };
Buffer*          pLightClusters[gDataBufferCount] = { NULL };
Buffer*          pUniformBufferSky[gDataBufferCount] = { NULL };
Buffer*          pUniformBufferSkyTri[gDataBufferCount] = { NULL };
uint64_t         gFrameCount = 0;
VBMeshInstance*  pVBMeshInstances = NULL;
PreSkinContainer gPreSkinContainers[gMaxAnimatedInstances] = {};
BufferChunk      gPreSkinGeometryChunks[2] = {}; // Position, Normal
uint32_t         gMaxMeshCount = 0;              // gMainSceneMeshCount + gMaxMeshInstances
uint32_t         gMaterialCount = 0;
UIComponent*     pGuiWindow = NULL;
UIComponent*     pDebugTexturesWindow = NULL;
FontDrawDesc     gFrameTimeDraw;
uint32_t         gFontID = 0;

UIComponent*              pHistogramWindow = NULL;
bool                      gScaleGeometryPlots = false;
UIWidget*                 pPlotSelector = NULL;
UIWidget*                 pIndicesPlot = NULL;
UIWidget*                 pVerticesPlot[MAX_VERTEX_BINDINGS] = {};
BufferAllocatorPlotWidget mBufferChunkAllocatorPlots[1 + TF_ARRAY_COUNT(pVerticesPlot)];

Semaphore* gPrevGraphicsSemaphore = NULL;
Semaphore* gComputeSemaphores[gDataBufferCount] = {};

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
// Order Independent Transparency Info
/************************************************************************/

struct TransparentNodeOIT
{
    uint32_t triangleData;
    uint32_t next;
};

struct GeometryCountOIT
{
    uint32_t count;
};

float gFlagAlphaPurple = 0.6f;
float gFlagAlphaBlue = 0.6f;
float gFlagAlphaGreen = 0.6f;
float gFlagAlphaRed = 0.6f;

Buffer* pGeometryCountBufferOIT = NULL;

Buffer* pHeadIndexBufferOIT = NULL;
Buffer* pVisBufLinkedListBufferOIT = NULL;

// We are rendering the scene (geometry, skybox, ...) at this resolution, UI at window resolution (mSettings.mWidth, mSettings.mHeight)
// Render scene at gSceneRes
// presentImage -> The scene rendertarget composed into the swapchain/backbuffer
// Render UI into backbuffer
static Resolution gSceneRes;

/************************************************************************/
// Screen resolution UI data
/************************************************************************/

const char*    pPipelineCacheName = "PipelineCache.cache";
PipelineCache* pPipelineCache = NULL;

static FORGE_CONSTEXPR inline uint32_t IndexTypeToSize(uint32_t type)
{
    ASSERT(type == INDEX_TYPE_UINT16 || type == INDEX_TYPE_UINT32);
    return type == INDEX_TYPE_UINT32 ? sizeof(uint32_t) : sizeof(uint16_t);
}

/************************************************************************/
// App implementation
/************************************************************************/
void SetupDebugTexturesWindow()
{
    float  scale = 0.15f;
    float2 screenSize = { (float)pRenderTargetVBPass->mWidth * gDivider, (float)pRenderTargetVBPass->mHeight * gDivider };
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

    static const Texture* VBRTs[6];
    uint32_t              textureCount = 0;

    VBRTs[textureCount++] = pRenderTargetShadow->pTexture;

    // VRS debug textures
    if (gAppSettings.mEnableVRS)
    {
        VBRTs[textureCount++] = pDebugVRSRenderTarget->pTexture;
    }

    // Prog MSAA debug textures
    if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 && !gAppSettings.mEnableVRS)
    {
        if (gAppSettings.mEnableGodray)
        {
            VBRTs[textureCount++] = pRenderTargetDebugGodRayMSAA->pTexture;
        }
        VBRTs[textureCount++] = pRenderTargetDebugMSAA->pTexture;
    }
    ASSERT(textureCount <= TF_ARRAY_COUNT(VBRTs));

    if (pDebugTexturesWindow)
    {
        ASSERT(pDebugTexturesWindow->mWidgets[0]->mType == WIDGET_TYPE_DEBUG_TEXTURES);
        DebugTexturesWidget* pTexturesWidget = (DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget;
        pTexturesWidget->pTextures = VBRTs;
        pTexturesWidget->mTexturesCount = textureCount;
        pTexturesWidget->mTextureDisplaySize = texSize;
    }
}

const char* gTestScripts[] = { "Test_MSAA.lua", "Test_VRS.lua" };
uint32_t    gCurrentScriptIndex = 0;
void        RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

class VisibilityBufferOIT: public IApp
{
public:
    bool Init()
    {
        // When we change RenderAPI we need to reset these because otherwise we won't populate the PreSkin data in the vertex buffers
        // buffers
        gAppSettings.mUpdateSimulation = true;
        gAppSettings.mHoldFilteredResults = false;

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
        gAppSettings.mEnableVRS &= pRenderer->pGpu->mSoftwareVRSSupported && (gGpuSettings.mMaxMSAALevel >= SAMPLE_COUNT_4);
        gAppSettings.mEnableGodray &= !gGpuSettings.mDisableGodRays;
        gAppSettings.mMaxMsaaLevel = (SampleCount)gGpuSettings.mMaxMSAALevel;
        if (gAppSettings.mEnableVRS)
        {
            gAppSettings.mMsaaLevel = SAMPLE_COUNT_4;
        }
        else
        {
            gAppSettings.mMsaaLevel =
                (SampleCount)clamp(gGpuSettings.mMSAASampleCount, (uint32_t)SAMPLE_COUNT_1, (uint32_t)gAppSettings.mMaxMsaaLevel);
        }
        gAppSettings.mMsaaIndex = (uint32_t)log2((uint32_t)gAppSettings.mMsaaLevel);
        gAppSettings.mMsaaIndexRequested = gAppSettings.mMsaaIndex;
#if defined(ENABLE_WORKGRAPH)
        gAppSettings.mGpuPipelineWorkgraph &= pRenderer->pGpu->mWorkgraphSupported;
#endif
        gDivider = gAppSettings.mEnableVRS ? 2 : 1;
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
        SamplerDesc pointDesc = { FILTER_NEAREST,
                                  FILTER_NEAREST,
                                  MIPMAP_MODE_NEAREST,
                                  ADDRESS_MODE_CLAMP_TO_EDGE,
                                  ADDRESS_MODE_CLAMP_TO_EDGE,
                                  ADDRESS_MODE_CLAMP_TO_EDGE };

        addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
        addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);

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
        skyboxTriDesc.ppTexture = &pSkyboxTri;
        addResource(&skyboxTriDesc, NULL);
        /************************************************************************/
        // Load the scene using the SceneLoader class
        /************************************************************************/
        HiresTimer sceneLoadTimer;
        initHiresTimer(&sceneLoadTimer);

        SyncToken token = {};

        /************************************************************************/
        // Allocate Visibility Buffer vertex buffers
        /************************************************************************/

        // TODO: Remove the magic values.
        //       These magic values are the total values of adding SanMiguel geometry to Stormtrooper geometry.
        //       This shouldn't be part of the App, the geometry should be packed by the AssetPipeline and generate a
        //       manifest with this information so that then we can load it and allocate the memory based on that.
        //       This would make the App data driven instead of hardcoding these values.
        const uint32_t mainSceneIndexCount = 7881594;
        const uint32_t mainSceneVertexCount = 2924717;

        const uint32_t stormtrooperIndexCount = 19554;
        const uint32_t stormtrooperVertexCount = 5860;

        const uint32_t visbuffMaxIndexCount = mainSceneIndexCount + stormtrooperIndexCount;

        const uint32_t visbuffMaxStaticVertexCount = mainSceneVertexCount + stormtrooperVertexCount;
        const uint32_t visBuffAnimatedVertexCount = stormtrooperVertexCount;
        gPreSkinnedVertexCountPerFrame = stormtrooperVertexCount * gMaxAnimatedInstances;

        // We need to create a geometry object that will contain all the data for all the geometry in the scene.
        // This includes Static geometry whose vertexes are already in world space and object instances whose
        // vertexes are in model space and we'll multiply by the model matrix in the shader.

        // Allocate PreSkin vertex buffers, this includes a ResourceHeap for skinned geometry and aliased PreSkin ouput buffers
        // to be able to run AsyncCompute. If an App doesn't use AsyncCompute triangle filtering this step can be skipped.
        {
            PreSkinACVertexBuffersDesc preSkinDesc = {};
            preSkinDesc.mNumBuffers = gDataBufferCount;
            preSkinDesc.mMaxStaticVertexCount = visbuffMaxStaticVertexCount;
            preSkinDesc.mMaxPreSkinnedVertexCountPerFrame = gPreSkinnedVertexCountPerFrame;

            initVBAsyncComputePreSkinVertexBuffers(pRenderer, &preSkinDesc, &pAsyncComputeVertexBuffers);
        }

        // Allocate the GeometryBuffer object that will contain all the geometry in the scene and additional space to store
        // pre-skinned vertex attributes
        {
            gGeometryLayout.mSemanticBindings[SEMANTIC_POSITION] = 0;
            gGeometryLayout.mSemanticBindings[SEMANTIC_TEXCOORD0] = 1;
            gGeometryLayout.mSemanticBindings[SEMANTIC_NORMAL] = 2;
            gGeometryLayout.mSemanticBindings[SEMANTIC_WEIGHTS] = 3;
            gGeometryLayout.mSemanticBindings[SEMANTIC_JOINTS] = 4;

            gGeometryLayout.mVerticesStrides[0] = sizeof(float3);
            gGeometryLayout.mVerticesStrides[1] = sizeof(uint32_t);
            gGeometryLayout.mVerticesStrides[2] = sizeof(uint32_t);
            gGeometryLayout.mVerticesStrides[3] = sizeof(float4);
            gGeometryLayout.mVerticesStrides[4] = sizeof(uint16_t[4]);

            GeometryBufferLoadDesc geometryBufferLoadDesc = {};
            geometryBufferLoadDesc.mStartState = RESOURCE_STATE_COPY_DEST;
            geometryBufferLoadDesc.pOutGeometryBuffer = &pGeometryBuffer;

            geometryBufferLoadDesc.mIndicesSize = sizeof(uint32_t) * visbuffMaxIndexCount;
            geometryBufferLoadDesc.mVerticesSizes[0] = (uint32_t)pAsyncComputeVertexBuffers->mPositions.mVBSize; // Position
            geometryBufferLoadDesc.mVerticesSizes[1] = sizeof(uint32_t) * visbuffMaxStaticVertexCount;           // UV
            geometryBufferLoadDesc.mVerticesSizes[2] = (uint32_t)pAsyncComputeVertexBuffers->mNormals.mVBSize;   // Normal
            geometryBufferLoadDesc.mVerticesSizes[3] = sizeof(float4) * visBuffAnimatedVertexCount;              // Weights
            geometryBufferLoadDesc.mVerticesSizes[4] = sizeof(uint16_t[4]) * visBuffAnimatedVertexCount;         // Joints

            geometryBufferLoadDesc.pNameIndexBuffer = "Geometry Indices";
            geometryBufferLoadDesc.pNamesVertexBuffers[0] = "Geometry Positions";
            geometryBufferLoadDesc.pNamesVertexBuffers[1] = "Geometry UVs";
            geometryBufferLoadDesc.pNamesVertexBuffers[2] = "Geometry Normals";
            geometryBufferLoadDesc.pNamesVertexBuffers[3] = "Geometry Weights";
            geometryBufferLoadDesc.pNamesVertexBuffers[4] = "Geometry Joints";

            // Skinned Atrributes need to be allocated in the PreSkinACVertexBuffers
            geometryBufferLoadDesc.pVerticesPlacements[0] = &pAsyncComputeVertexBuffers->mPositions.mVBPlacement;
            geometryBufferLoadDesc.pVerticesPlacements[2] = &pAsyncComputeVertexBuffers->mNormals.mVBPlacement;

            gGeometryBufferLoadDesc = geometryBufferLoadDesc;
            addGeometryBuffer(&geometryBufferLoadDesc);
        }

        // Allocate buffer parts for preskinned data, this lets the BufferChunkAllocator know we are using this memory
        // TODO: We need to make sure this matches with the Aliased Buffers we allocate in addVBAsyncComputePreSkinVertexBuffers
        // TODO: Move this to internal VisibilityBuffer interface?
        {
            const uint32_t preSkinRequiredVertexes = gPreSkinnedVertexCountPerFrame * gDataBufferCount;

            BufferChunk requestedChunk = { (uint32_t)pAsyncComputeVertexBuffers->mPositions.mOutputMemoryStartOffset,
                                           (uint32_t)pAsyncComputeVertexBuffers->mPositions.mOutputMemorySize };
            uint32_t    binding = gGeometryLayout.mSemanticBindings[SEMANTIC_POSITION];
            uint32_t    vertexStride = gGeometryLayout.mVerticesStrides[binding];
            addGeometryBufferPart(&pGeometryBuffer->mVertex[binding], preSkinRequiredVertexes * vertexStride, vertexStride,
                                  &gPreSkinGeometryChunks[0], &requestedChunk);

            requestedChunk = { (uint32_t)pAsyncComputeVertexBuffers->mNormals.mOutputMemoryStartOffset,
                               (uint32_t)pAsyncComputeVertexBuffers->mNormals.mOutputMemorySize };
            binding = gGeometryLayout.mSemanticBindings[SEMANTIC_NORMAL];
            vertexStride = gGeometryLayout.mVerticesStrides[binding];
            addGeometryBufferPart(&pGeometryBuffer->mVertex[binding], preSkinRequiredVertexes * vertexStride, vertexStride,
                                  &gPreSkinGeometryChunks[1], &requestedChunk);

            // Write memory for Pre-Skinned vertexes starts after main scene memory
            gPreSkinnedVertexStartOffset = gPreSkinGeometryChunks[0].mOffset / gGeometryLayout.mVerticesStrides[0];
            ASSERT(gPreSkinnedVertexStartOffset == (gPreSkinGeometryChunks[1].mOffset / gGeometryLayout.mVerticesStrides[1]) &&
                   "PreSkin output vertex offset needs to be the same for all attributes: Position, Normal");

            PreSkinBufferOffsets offsets = {};
            offsets.vertexOffset = (uint32_t)gPreSkinnedVertexStartOffset;

            BufferLoadDesc desc = {};
            desc.pData = &offsets;
            desc.ppBuffer = &pPreSkinBufferOffsets;
            desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            desc.mDesc.mElementCount = 1;
            desc.mDesc.mStructStride = sizeof(PreSkinBufferOffsets);
            desc.mDesc.mSize = desc.mDesc.mElementCount * desc.mDesc.mStructStride;
            desc.mDesc.pName = "PreSkinShaderBufferOffsets";
            addResource(&desc, NULL);
        }

        // Main Scene
        GeometryLoadDesc sceneLoadDesc = {};
        sceneLoadDesc.pGeometryBuffer = pGeometryBuffer;
        sceneLoadDesc.pGeometryBufferLayoutDesc = &gGeometryLayout;
        pScene = initSanMiguel(&sceneLoadDesc, token, true);
        if (!pScene)
            return false;
        gSceneMeshCount = pScene->geom->mDrawArgCount;

        pSanMiguelGeometry = pScene->geom;

        LOGF(LogLevel::eINFO, "Load scene : %f ms", getHiresTimerUSec(&sceneLoadTimer, true) / 1000.0f);

        gMaxMeshCount = pSanMiguelGeometry->mDrawArgCount + gMaxMeshInstances;

        const uint32_t mainSceneMaterialCount = pScene->materialCount;
        gMaterialCount = mainSceneMaterialCount + 1; // +1 to store the stormtrooper material too
        ASSERT(MAX_TEXTURE_UNITS >= gMaterialCount && "Not enough textures slots in the shader to bind all materials");

        // Load Instance/Animated Mesh
        VertexLayout vertexLayout = {};
        vertexLayout.mAttribCount = 5;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R16G16_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 1;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R16G16_UNORM;
        vertexLayout.mAttribs[2].mBinding = 2;
        vertexLayout.mAttribs[2].mLocation = 2;
        vertexLayout.mAttribs[3].mSemantic = SEMANTIC_WEIGHTS;
        vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayout.mAttribs[3].mBinding = 3;
        vertexLayout.mAttribs[3].mLocation = 3;
        vertexLayout.mAttribs[4].mSemantic = SEMANTIC_JOINTS;
        vertexLayout.mAttribs[4].mFormat = TinyImageFormat_R16G16B16A16_UINT;
        vertexLayout.mAttribs[4].mBinding = 4;
        vertexLayout.mAttribs[4].mLocation = 4;

        GeometryLoadDesc instancedMeshDesc = {};
        instancedMeshDesc.pFileName = gAnimatedMeshFile;
        instancedMeshDesc.ppGeometryData = &pAnimatedMeshGeomData;
        instancedMeshDesc.pVertexLayout = &vertexLayout;
        instancedMeshDesc.ppGeometry = &pTroopGeometry;
        instancedMeshDesc.pGeometryBuffer = pGeometryBuffer;
        instancedMeshDesc.pGeometryBufferLayoutDesc = &gGeometryLayout;
        addResource(&instancedMeshDesc, &token);

        waitForToken(&token);

        /************************************************************************/
        // Texture loading
        /************************************************************************/
        gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gNormalMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gSpecularMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);

        for (uint32_t i = 0; i < mainSceneMaterialCount; ++i)
        {
            TextureLoadDesc desc = {};
            desc.pFileName = pScene->textures[i];
            desc.ppTexture = &gDiffuseMapsStorage[i];
            // Textures representing color should be stored in SRGB or HDR format
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = pScene->normalMaps[i];
            desc.ppTexture = &gNormalMapsStorage[i];
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = pScene->specularMaps[i];
            desc.ppTexture = &gSpecularMapsStorage[i];
            addResource(&desc, NULL);
        }

        // Load stormtrooper materials and place them at the end of the material texture arrays
        {
            const uint32_t materialID = mainSceneMaterialCount; // Place after the main scene materials

            const char* pDiffuseFilename = "Stormtrooper_D.tex";
            // We don't have normal or specular for the stormtrooper
            // We use material at index 0 because it's the ones used for the ForgeFlags and contains default normal and specular textures
            const char* pNormalFilename = pScene->normalMaps[0];
            const char* pSpecularFilename = pScene->specularMaps[0];

            TextureLoadDesc desc = {};
            desc.pFileName = pDiffuseFilename;
            desc.ppTexture = &gDiffuseMapsStorage[materialID];
            // Textures representing color should be stored in SRGB or HDR format
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = pNormalFilename; // We don't have a
            desc.ppTexture = &gNormalMapsStorage[materialID];
            addResource(&desc, NULL);
            desc = {};

            desc.pFileName = pSpecularFilename;
            desc.ppTexture = &gSpecularMapsStorage[materialID];
            addResource(&desc, NULL);
        }

        /************************************************************************/
        // Init visibility buffer
        /************************************************************************/

        // We need to allocate enough indices for the entire static scene + mesh instances + animated instances
        uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};
        pVBMeshInstances = (VBMeshInstance*)tf_calloc(gMaxMeshCount, sizeof(VBMeshInstance));

        HiresTimer vbSetupTimer;
        initHiresTimer(&vbSetupTimer);

        // Get indices count per geomset
        for (uint32_t i = 0; i < pSanMiguelGeometry->mDrawArgCount; ++i)
        {
            const MaterialFlags materialFlags = pScene->materialFlags[i];

            uint32_t geomSet = GEOMSET_OPAQUE;
            if (materialFlags & MATERIAL_FLAG_ALPHA_TESTED)
                geomSet = GEOMSET_ALPHA_CUTOUT;
            if (materialFlags & MATERIAL_FLAG_TRANSPARENT)
                geomSet = GEOMSET_ALPHA_BLEND;

            visibilityBufferFilteredIndexCount[geomSet] += (pSanMiguelGeometry->pDrawArgs + i)->mIndexCount;
        }

        removeGeometryShadowData(pScene->geomData);

        // Add  dynamic instances
        for (uint32_t i = 0; i < gStaticMeshInstanceCount; ++i)
        {
            visibilityBufferFilteredIndexCount[gStaticMeshInstances[i].mGeomSet] += pTroopGeometry->pDrawArgs->mIndexCount;
        }
        /************************************************************************/
        // Load animation data
        /************************************************************************/
        gRig.Initialize(RD_ANIMATIONS, gSkeletonFile);

        gClip.Initialize(RD_ANIMATIONS, gClipFile, &gRig);

        uint32_t currJointMatrixOffset = 0;
        uint32_t currPreSkinnedOutputVertex = 0;
        for (uint32_t i = 0; i < gAnimatedMeshInstanceCount; ++i)
        {
            AnimatedMeshInstance* am = &gAnimatedMeshInstances[i];

            const GeometryData* pMeshGeomData = pAnimatedMeshGeomData;

            // Instance initialization
            {
                am->mClipController.Initialize(gClip.GetDuration());
                am->mClipController.mPlaybackSpeed = am->mPlaybackSpeed;

                AnimationDesc animationDesc{};
                animationDesc.mRig = &gRig;
                animationDesc.mNumLayers = 1;
                animationDesc.mLayerProperties[0].mClip = &gClip;
                animationDesc.mLayerProperties[0].mClipController = &am->mClipController;
                am->mAnimation.Initialize(animationDesc);
                am->mAnimObject.Initialize(&gRig, &am->mAnimation);

                am->pGeomData = pMeshGeomData;
                am->pBoneMatrixes = (mat4*)tf_calloc(pMeshGeomData->mJointCount, sizeof(mat4));

                am->mJointMatrixOffset = currJointMatrixOffset;

                am->mPreSkinnedVertexOffset = currPreSkinnedOutputVertex;
            }

            // Pre skin container (input to vertex pre skinning stage)
            {
                // We fill PreSkinContainers on Init because our scene is constant (we don't add/remove objects from the scene)
                // In a dynamic scenario these containers could be filled/updated per frame
                gPreSkinContainers[i].mVertexCount = pTroopGeometry->mVertexCount;

                const uint32_t posBinding = GET_GEOMETRY_VERTEX_BINDING(SEMANTIC_POSITION);
                const uint32_t jointBinding = GET_GEOMETRY_VERTEX_BINDING(SEMANTIC_JOINTS);
                gPreSkinContainers[i].mVertexPositionOffset =
                    pTroopGeometry->mVertexBufferChunks[posBinding].mOffset / pTroopGeometry->mVertexStrides[posBinding];
                gPreSkinContainers[i].mJointOffset =
                    pTroopGeometry->mVertexBufferChunks[jointBinding].mOffset / pTroopGeometry->mVertexStrides[jointBinding];
                gPreSkinContainers[i].mJointMatrixOffset = currJointMatrixOffset;

                gPreSkinContainers[i].mOutputVertexOffset = currPreSkinnedOutputVertex;
            }
            // TODO: If we want to support AlphaCutout and AlphaBlend we need to modify the shaders to get the correct UVs.
            //            We have the problem of needing texture UVs that are not animated, currently Shadow shaders take Position and
            //            UVs as input, this is fine as long as the mesh is animated because index of position is the same as UVs, but
            //            with animated objects the animated positions are stored at a different index than the UVs.
            ASSERT(gAnimatedMeshInstances[i].mGeomSet == GEOMSET_OPAQUE);

            currPreSkinnedOutputVertex += pTroopGeometry->mVertexCount;
            currJointMatrixOffset += pMeshGeomData->mJointCount;
            // Count total indices per geom set
            {
                visibilityBufferFilteredIndexCount[gAnimatedMeshInstances[i].mGeomSet] += pTroopGeometry->pDrawArgs->mIndexCount;
            }
        }

        // Init visibility buffer
        VisibilityBufferDesc vbDesc = {};
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = gDataBufferCount;
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
        // PreSkin Pass
        vbDesc.mEnablePreSkinPass = true;
        vbDesc.mPreSkinBatchSize = SKIN_BATCH_SIZE;
        vbDesc.mPreSkinBatchCount = SKIN_BATCH_COUNT;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        LOGF(LogLevel::eINFO, "Setup vb : %f ms", getHiresTimerUSec(&vbSetupTimer, true) / 1000.0f);

        const uint32_t numScripts = TF_ARRAY_COUNT(gTestScripts);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

        // Default NX settings for better performance.
#if NX64
        // Async compute is not optimal on the NX platform. Turning this off to make use of default graphics queue for triangle visibility.
        gAppSettings.mAsyncCompute = false;
        // High fill rate features are also disabled by default for performance.
        gAppSettings.mEnableGodray = false;
#endif

        /************************************************************************/
        // MSAA Settings
        /************************************************************************/
        /************************************************************************/
        /************************************************************************/
        // Finish the resource loading process since the next code depends on the loaded resources
        waitForAllResourceLoads();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            gPerFrame[i].pMeshData = (MeshData*)tf_malloc(gMaxMeshCount * sizeof(MeshData));
        }
        HiresTimer setupBuffersTimer;
        initHiresTimer(&setupBuffersTimer);
        addTriangleFilteringBuffers(pScene);

        UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
        updateVBMeshFilterGroupsDesc.mNumMeshInstance = gMaxMeshCount;
        updateVBMeshFilterGroupsDesc.pVBMeshInstances = pVBMeshInstances;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            updateVBMeshFilterGroupsDesc.mFrameIndex = i;
            gVBPreFilterStats[i] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
        }

        LOGF(LogLevel::eINFO, "Setup buffers : %f ms", getHiresTimerUSec(&setupBuffersTimer, true) / 1000.0f);

        LOGF(LogLevel::eINFO, "Total Load Time : %f ms", getHiresTimerUSec(&totalTimer, true) / 1000.0f);

        /************************************************************************/
        // Setup the fps camera for navigating through the scene
        /************************************************************************/
        vec3                   startPosition(136.9f, 38.8f, 22.8f);
        vec3                   startLookAt = startPosition + vec3(-0.2f, 0.04f, 0.0f);
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
        exitCameraController(pCameraController);

        tf_free(gCameraPathData);

        removeResource(pBufferGodRayConstant);

        removeResource(pGodRayBlurBuffer[0][0]);
        removeResource(pGodRayBlurBuffer[0][1]);
        removeResource(pGodRayBlurBuffer[1][0]);
        removeResource(pGodRayBlurBuffer[1][1]);
        removeResource(pBufferBlurWeights);

        removeResource(pSkyboxTri);
        removeTriangleFilteringBuffers();

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        /************************************************************************/
        // Remove loaded scene
        /************************************************************************/
        removeResource(pScene->geomData);
        exitSanMiguel(pScene);

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            tf_free(gPerFrame[i].pMeshData);
        }
        // Destroy scene buffers
        removeResource(pSanMiguelGeometry);
        pSanMiguelGeometry = nullptr;

        removeResource(pTroopGeometry);
        pTroopGeometry = nullptr;

        exitVBAsyncComputePreSkinVertexBuffers(pRenderer, pAsyncComputeVertexBuffers);

        removeResource(pAnimatedMeshGeomData);
        pAnimatedMeshGeomData = nullptr;

        removeResource(pPreSkinBufferOffsets);

        removeGeometryBufferPart(&pGeometryBuffer->mVertex[GET_GEOMETRY_VERTEX_BINDING(SEMANTIC_POSITION)], &gPreSkinGeometryChunks[0]);
        removeGeometryBufferPart(&pGeometryBuffer->mVertex[GET_GEOMETRY_VERTEX_BINDING(SEMANTIC_NORMAL)], &gPreSkinGeometryChunks[1]);

        removeGeometryBuffer(pGeometryBuffer);
        pGeometryBuffer = nullptr;

        for (uint32_t i = 0; i < gAnimatedMeshInstanceCount; ++i)
        {
            AnimatedMeshInstance* am = &gAnimatedMeshInstances[i];

            am->mAnimObject.Exit();
            am->mAnimation.Exit();
            tf_free(am->pBoneMatrixes);
            am->pBoneMatrixes = nullptr;
        }

        gClip.Exit();
        gRig.Exit();

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

        /************************************************************************/
        /************************************************************************/
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitSemaphore(pRenderer, pPresentSemaphore);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitGpuCmdRing(pRenderer, &gComputeCmdRing);

        exitQueue(pRenderer, pGraphicsQueue);
        exitQueue(pRenderer, pComputeQueue);

        removeSampler(pRenderer, pSamplerTrilinearAniso);
        removeSampler(pRenderer, pSamplerPointClamp);

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
            ddTestScripts.mCount = TF_ARRAY_COUNT(gTestScripts);
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

            ButtonWidget bRunScript;
            UIWidget*    pRunScript = uiAddComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
            uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
            luaRegisterWidget(pRunScript);

            // Buffer chunk allocator stats
            {
                UIComponentDesc.mStartPosition[0] += 800;
                UIComponentDesc.mStartSize[0] = 516;
                UIComponentDesc.mStartSize[1] = 400;
                uiAddComponent("Geometry Buffer", &UIComponentDesc, &pHistogramWindow);
                uiSetComponentFlags(pHistogramWindow, GUI_COMPONENT_FLAGS_ALWAYS_AUTO_RESIZE);

                CheckboxWidget checkbox{ &gScaleGeometryPlots };
                pPlotSelector = uiAddComponentWidget(pHistogramWindow, "Width relative to size", &checkbox, WIDGET_TYPE_CHECKBOX);
                uiSetWidgetOnEditedCallback(pPlotSelector, &gGeometryBufferVertexBufferCount, recreateGeometryBufferPlots);

                const float2 defaultSize = { float(500), float(40) }; // Create with default size, actual size will be computed later
                mBufferChunkAllocatorPlots[0].pName = gGeometryBufferLoadDesc.pNameIndexBuffer;
                mBufferChunkAllocatorPlots[0].mSize = defaultSize;
                pIndicesPlot = uiAddBufferChunkAllocatorPlotWidget(pHistogramWindow, "", &mBufferChunkAllocatorPlots[0]);

                uint32_t vertexBufferCount = 0;
                for (uint32_t i = 0; i < TF_ARRAY_COUNT(pVerticesPlot); ++i)
                {
                    if (gGeometryBufferLoadDesc.pNamesVertexBuffers[i] == NULL)
                        break;

                    mBufferChunkAllocatorPlots[1 + i].pName = gGeometryBufferLoadDesc.pNamesVertexBuffers[i];
                    mBufferChunkAllocatorPlots[1 + i].mSize = defaultSize;
                    pVerticesPlot[i] = uiAddBufferChunkAllocatorPlotWidget(pHistogramWindow, "", &mBufferChunkAllocatorPlots[1 + i]);

                    vertexBufferCount++;
                }
                recreateGeometryBufferPlots(&vertexBufferCount);

                gGeometryBufferVertexBufferCount = vertexBufferCount;
            }

            /************************************************************************/
            // Most important options
            /************************************************************************/

            SliderFloatWidget transparencyAlphaWidgets[4] = {};
            transparencyAlphaWidgets[0].mMin = 0.0f;
            transparencyAlphaWidgets[0].mMax = 1.0f;
            transparencyAlphaWidgets[0].pData = &gFlagAlphaPurple;
            transparencyAlphaWidgets[1].mMin = 0.0f;
            transparencyAlphaWidgets[1].mMax = 1.0f;
            transparencyAlphaWidgets[1].pData = &gFlagAlphaBlue;
            transparencyAlphaWidgets[2].mMin = 0.0f;
            transparencyAlphaWidgets[2].mMax = 1.0f;
            transparencyAlphaWidgets[2].pData = &gFlagAlphaGreen;
            transparencyAlphaWidgets[3].mMin = 0.0f;
            transparencyAlphaWidgets[3].mMax = 1.0f;
            transparencyAlphaWidgets[3].pData = &gFlagAlphaRed;

            UIWidget  widgetBases[4] = {};
            UIWidget* widgets[4] = {};
            for (uint32_t i = 0; i < TF_ARRAY_COUNT(widgetBases); ++i)
                widgets[i] = &widgetBases[i];

            widgets[0]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            widgets[0]->pWidget = &transparencyAlphaWidgets[0];
            strcpy(widgets[0]->mLabel, "Purple Flag");
            widgets[1]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            widgets[1]->pWidget = &transparencyAlphaWidgets[1];
            strcpy(widgets[1]->mLabel, "Blue Flag");
            widgets[2]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            widgets[2]->pWidget = &transparencyAlphaWidgets[2];
            strcpy(widgets[2]->mLabel, "Green Flag");
            widgets[3]->mType = WIDGET_TYPE_SLIDER_FLOAT;
            widgets[3]->pWidget = &transparencyAlphaWidgets[3];
            strcpy(widgets[3]->mLabel, "Red Flag");

            CollapsingHeaderWidget collapsingAlphaControlOptions;
            collapsingAlphaControlOptions.mCollapsed = false;
            collapsingAlphaControlOptions.pGroupedWidgets = widgets;
            collapsingAlphaControlOptions.mWidgetsCount = TF_ARRAY_COUNT(widgets);

            luaRegisterWidget(
                uiAddComponentWidget(pGuiWindow, "Transparent Alphas", &collapsingAlphaControlOptions, WIDGET_TYPE_COLLAPSING_HEADER));

            static const char* msaaSampleNames[] = { "Off", "2 Samples", "4 Samples" };

            DropdownWidget ddMSAA = {};
            ddMSAA.pData = &gAppSettings.mMsaaIndexRequested;
            ddMSAA.pNames = msaaSampleNames;
            uint32_t msaaOptionsCount = 1 + (((uint32_t)gAppSettings.mMaxMsaaLevel) >> 1);
            ddMSAA.mCount = msaaOptionsCount;
            UIWidget* msaaWidget = uiAddComponentWidget(pGuiWindow, "MSAA", &ddMSAA, WIDGET_TYPE_DROPDOWN);
            uiSetWidgetOnEditedCallback(msaaWidget, nullptr,
                                        [](void* pUserData)
                                        {
                                            UNREF_PARAM(pUserData);
                                            if (gAppSettings.mEnableVRS)
                                                gAppSettings.mMsaaIndexRequested = 2;
                                            else
                                            {
                                                ReloadDesc reloadDescriptor;
                                                reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                                                requestReload(&reloadDescriptor);
                                            }
                                        });
            luaRegisterWidget(msaaWidget);

            CheckboxWidget checkbox;
            checkbox.pData = &gAppSettings.mEnableVRS;
            UIWidget* vrsWidget = uiAddComponentWidget(pGuiWindow, "Enable Variable Rate Shading", &checkbox, WIDGET_TYPE_CHECKBOX);
            uiSetWidgetOnEditedCallback(vrsWidget, nullptr,
                                        [](void* pUserData)
                                        {
                                            UNREF_PARAM(pUserData);
                                            if (pRenderer->pGpu->mSoftwareVRSSupported && gAppSettings.mMaxMsaaLevel >= SAMPLE_COUNT_4)
                                            {
                                                gAppSettings.mMsaaIndexRequested = gAppSettings.mEnableVRS ? 2 : 0;
                                                ReloadDesc reloadDescriptor;
                                                reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                                                requestReload(&reloadDescriptor);
                                            }
                                            else
                                            {
                                                LOGF(LogLevel::eWARNING, "Programmable sample locations are not supported on this device");
                                                gAppSettings.mEnableVRS = false;
                                            }
                                        });
            luaRegisterWidget(vrsWidget);

            checkbox.pData = &gAppSettings.mUpdateSimulation;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Update Simulation", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mHoldFilteredResults;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Hold filtered results", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mAsyncCompute;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Async Compute", &checkbox, WIDGET_TYPE_CHECKBOX));

            checkbox.pData = &gAppSettings.mDrawDebugTargets;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Draw Debug Targets", &checkbox, WIDGET_TYPE_CHECKBOX));

#if defined(ENABLE_WORKGRAPH)
            if (pRenderer->pGpu->mWorkgraphSupported)
            {
                checkbox.pData = &gAppSettings.mGpuPipelineWorkgraph;
                luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Enable GPU pipeline workgraph", &checkbox, WIDGET_TYPE_CHECKBOX));
            }
#endif
            /************************************************************************/
            /************************************************************************/
            if (pRenderer->pGpu->mHDRSupported)
            {
                LabelWidget labelWidget = {};
                gAppSettings.pOutputSupportsHDRWidget =
                    uiAddComponentWidget(pGuiWindow, "Output Supports HDR", &labelWidget, WIDGET_TYPE_LABEL);
                REGISTER_LUA_WIDGET(gAppSettings.pOutputSupportsHDRWidget);

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
            sliderFloat.pData = &gGodRayConstant.mScatterFactor;
            sliderFloat.mMin = 0.0f;
            sliderFloat.mMax = 1.0f;
            uiAddDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray : Scatter Factor", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

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
            gDivider = gAppSettings.mEnableVRS ? 2 : 1;

            addRenderTargets();

            SetupDebugTexturesWindow();

            if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_SCENE_RESOLUTION))
            {
                addOrderIndependentTransparencyResources();
            }
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

            if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_SCENE_RESOLUTION))
            {
                removeOrderIndependentTransparencyResources();

                ESRAM_RESET_ALLOCS(pRenderer);
            }
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveComponent(pHistogramWindow);
            uiRemoveComponent(pGuiWindow);
            uiRemoveDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
            uiRemoveDynamicWidgets(&gAppSettings.mLinearScale);
            uiRemoveDynamicWidgets(&gAppSettings.mDisplaySetting);

            unloadProfilerUI();
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
            if (gWasColorSpace != gAppSettings.mCurrentSwapChainColorSpace && gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
            {
                ReloadDesc reloadDescriptor;
                reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
                requestReload(&reloadDescriptor);
            }

            gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
            gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
            gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;
            gWasOutputMode = gAppSettings.mOutputMode;
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

        // We don't update the matrix if we are holding the filtered results because the meshes would keep moving leaving culled triangles
        // visible to the screen, because the triangles where filtered with an older modelMtx
        if (gAppSettings.mUpdateSimulation)
        {
            // Instances update
            for (uint32_t i = 0; i < gStaticMeshInstanceCount; ++i)
            {
                StaticMeshInstance* instance = &gStaticMeshInstances[i];
                instance->mRotation *= Quat::rotationY(degToRad(instance->mRotationSpeedYDeg) * deltaTime);
            }

            // Animation update
            for (uint32_t i = 0; i < gAnimatedMeshInstanceCount; ++i)
            {
                AnimatedMeshInstance* am = &gAnimatedMeshInstances[i];
                AnimatedObject*       pAnimObject = &am->mAnimObject;

                pAnimObject->Update(deltaTime);
                pAnimObject->ComputePose(pAnimObject->mRootTransform);
            }
        }

        // We have to update bone matrixes always because if we do it in the if(gAppSettings.mUpdateSimulation) above each frameIdx will
        // have different values for these matrixes when gAppSettings.mUpdateSimulation == false
        for (uint32_t i = 0; i < gAnimatedMeshInstanceCount; ++i)
        {
            AnimatedMeshInstance* am = &gAnimatedMeshInstances[i];
            const GeometryData*   pGeomData = am->pGeomData;
            AnimatedObject*       pAnimObject = &am->mAnimObject;
            mat4*                 pBoneMatrixes = am->pBoneMatrixes;

            for (uint32_t j = 0; j < pGeomData->mJointCount; ++j)
            {
                pBoneMatrixes[j] = pAnimObject->mJointWorldMats[pGeomData->pJointRemaps[j]] * pGeomData->pInverseBindPoses[j];
            }
        }

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

        uint32_t presentIndex = 0;
        uint32_t frameIdx = gFrameCount % gDataBufferCount;

        GpuCmdRingElement computeElem = getNextGpuCmdRingElement(&gComputeCmdRing, true, 1);
        GpuCmdRingElement graphicsElem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        BufferUpdateDesc lightsUpdate = { pLightsBuffer };
        beginUpdateResource(&lightsUpdate);
        memcpy(lightsUpdate.pMappedData, &gLightData, sizeof(LightData) * 128);
        endUpdateResource(&lightsUpdate);

        /************************************************************************/
        // Async compute pass
        /************************************************************************/
        if (gAppSettings.mAsyncCompute && gAppSettings.mUpdateSimulation)
        {
            // check to see if we can use the cmd buffer
            FenceStatus fenceStatus;
            getFenceStatus(pRenderer, computeElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                waitForFences(pRenderer, 1, &computeElem.pFence);

            /************************************************************************/
            // Update uniform buffer to gpu
            /************************************************************************/
            if (!gAppSettings.mHoldFilteredResults)
            {
                BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
                beginUpdateResource(&update);
                memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameUniformData, sizeof(gPerFrame[frameIdx].gPerFrameUniformData));
                endUpdateResource(&update);
            }

            /************************************************************************/
            // Triangle filtering async compute pass
            /************************************************************************/
            Cmd* computeCmd = computeElem.pCmds[0];

            resetCmdPool(pRenderer, computeElem.pCmdPool);
            beginCmd(computeCmd);
            cmdBindDescriptorSet(computeCmd, 0, pDescriptorSetPersistent);
            cmdBindDescriptorSet(computeCmd, frameIdx, pDescriptorSetPerFrame);
            cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);

            PreSkinVertexesPassDesc preSkinDesc = {};
            preSkinDesc.pPreSkinContainers = gPreSkinContainers;
            preSkinDesc.mPreSkinContainerCount = gAnimatedMeshInstanceCount;
            preSkinDesc.pPipelinePreSkinVertexes = pPipelinePreSkinVertexes[PRE_SKIN_ASYNC];
            preSkinDesc.pDescriptorSetPreSkinVertexesPerDraw = pDescriptorSetPreSkinVertexes[PRE_SKIN_ASYNC];

            Buffer* pPreSkinnedVertexBuffers[] = {
                pAsyncComputeVertexBuffers->mPositions.pPreSkinBuffers[frameIdx],
                pAsyncComputeVertexBuffers->mNormals.pPreSkinBuffers[frameIdx],
            };
            preSkinDesc.ppPreSkinOutputVertexBuffers = pPreSkinnedVertexBuffers;
            preSkinDesc.mPreSkinOutputVertexBufferCount = TF_ARRAY_COUNT(pPreSkinnedVertexBuffers);
            preSkinDesc.mFrameIndex = frameIdx;
            preSkinDesc.mGpuProfileToken = gComputeProfileToken;
            preSkinDesc.mPreSkinBatchIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gBatchData);
            preSkinDesc.pDescriptorSetPreSkinVertexes = pDescriptorSetPersistent;
            preSkinDesc.pDescriptorSetPreSkinVertexesPerFrame = pDescriptorSetPerFrame;
            cmdVisibilityBufferPreSkinVertexesPass(pVisibilityBuffer, computeCmd, &preSkinDesc);

            TriangleFilteringPassDesc triangleFilteringDesc = {};
            triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
            triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;
            triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorTriangleFilteringPerBatch;

#if defined(ENABLE_WORKGRAPH)
            if (gAppSettings.mGpuPipelineWorkgraph)
            {
                triangleFilteringDesc.pWorkgraph = pWorkgraphGpuPipeline;
                triangleFilteringDesc.mInitWorkgraph = gFrameCount == 0;
            }
#endif

            triangleFilteringDesc.mFrameIndex = frameIdx;
            triangleFilteringDesc.mBuffersIndex = frameIdx;
            triangleFilteringDesc.mGpuProfileToken = gComputeProfileToken;
            triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIdx];
            cmdVBTriangleFilteringPass(pVisibilityBuffer, computeCmd, &triangleFilteringDesc);

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
            Semaphore* waitSemaphores[] = { flushUpdateDesc.pOutSubmittedSemaphore, gFrameCount > 1 ? gPrevGraphicsSemaphore : NULL };

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

            gComputeSemaphores[frameIdx] = computeElem.pSemaphore;
        }
        /************************************************************************/
        // Draw Pass - Skip first frame since draw will always be one frame behind compute
        /************************************************************************/
        if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
        {
            frameIdx = gAppSettings.mAsyncCompute ? ((gFrameCount - 1) % gDataBufferCount) : frameIdx;

            // check to see if we can use the cmd buffer
            FenceStatus fenceStatus;
            getFenceStatus(pRenderer, graphicsElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                waitForFences(pRenderer, 1, &graphicsElem.pFence);

            pScreenRenderTarget = pIntermediateRenderTarget;
            /************************************************************************/
            // Update uniform buffer to gpu
            /************************************************************************/
            if ((!gAppSettings.mAsyncCompute && !gAppSettings.mHoldFilteredResults) || !gAppSettings.mUpdateSimulation)
            {
                BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
                beginUpdateResource(&update);
                memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameUniformData, sizeof(gPerFrame[frameIdx].gPerFrameUniformData));
                endUpdateResource(&update);
            }

            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameUniformData, sizeof(gPerFrame[frameIdx].gPerFrameUniformData));
            endUpdateResource(&update);

            // Update uniform buffers
            update = { pUniformBufferSky[frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gUniformDataSky, sizeof(gPerFrame[frameIdx].gUniformDataSky));
            endUpdateResource(&update);

            update = { pUniformBufferSkyTri[frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gUniformDataSkyTri, sizeof(gPerFrame[frameIdx].gUniformDataSkyTri));
            endUpdateResource(&update);

            // Update MeshData
            update = { pMeshDataBuffer[frameIdx], sizeof(MeshData) * gSceneMeshCount };
            update.mSize = sizeof(MeshData) * gMaxMeshInstances;
            beginUpdateResource(&update);
            memcpy(update.pMappedData, gPerFrame[frameIdx].pMeshData + gSceneMeshCount, update.mSize);
            endUpdateResource(&update);

            // Update Joint matrixes
            update = { pJointMatrixBuffer[frameIdx], 0 };
            update.mSize = sizeof(gPerFrame[frameIdx].gJointMatrixes);
            COMPILE_ASSERT(sizeof(mat4) * gMaxJointMatrixes == sizeof(gPerFrame[frameIdx].gJointMatrixes));
            beginUpdateResource(&update);
            memcpy(update.pMappedData, gPerFrame[frameIdx].gJointMatrixes, sizeof(gPerFrame[frameIdx].gJointMatrixes));
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

            if (!gAppSettings.mAsyncCompute && gAppSettings.mUpdateSimulation)
            {
                PreSkinVertexesPassDesc preSkinDesc = {};
                preSkinDesc.pPreSkinContainers = gPreSkinContainers;
                preSkinDesc.mPreSkinContainerCount = gAnimatedMeshInstanceCount;
                preSkinDesc.pPipelinePreSkinVertexes = pPipelinePreSkinVertexes[PRE_SKIN_SYNC];
                preSkinDesc.pDescriptorSetPreSkinVertexes = pDescriptorSetPersistent;
                preSkinDesc.pDescriptorSetPreSkinVertexesPerFrame = pDescriptorSetPerFrame;
                preSkinDesc.pDescriptorSetPreSkinVertexesPerDraw = pDescriptorSetPreSkinVertexes[PRE_SKIN_SYNC];
                // When we do Sync Triangle Filtering we can use the same buffers as we use for rendering on the PreSkin vertexes stage
                // because there is no conflict on the ResourceState
                Buffer* pPreSkinOutputVtxBuffers[] = { GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_POSITION),
                                                       GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_NORMAL) };
                preSkinDesc.ppPreSkinOutputVertexBuffers = pPreSkinOutputVtxBuffers;
                preSkinDesc.mPreSkinOutputVertexBufferCount = TF_ARRAY_COUNT(pPreSkinOutputVtxBuffers);

                preSkinDesc.mFrameIndex = frameIdx;
                preSkinDesc.mGpuProfileToken = gGraphicsProfileToken;

                BufferBarrier barriers[2] = { { pPreSkinOutputVtxBuffers[0], gVertexBufferState, RESOURCE_STATE_UNORDERED_ACCESS },
                                              { pPreSkinOutputVtxBuffers[1], gVertexBufferState, RESOURCE_STATE_UNORDERED_ACCESS } };
                cmdResourceBarrier(graphicsCmd, TF_ARRAY_COUNT(barriers), barriers, 0, NULL, 0, NULL);

                preSkinDesc.mPreSkinBatchIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gBatchData);
                cmdVisibilityBufferPreSkinVertexesPass(pVisibilityBuffer, graphicsCmd, &preSkinDesc);

                barriers[0] = { pPreSkinOutputVtxBuffers[0], RESOURCE_STATE_UNORDERED_ACCESS, gVertexBufferState };
                barriers[1] = { pPreSkinOutputVtxBuffers[1], RESOURCE_STATE_UNORDERED_ACCESS, gVertexBufferState };
                cmdResourceBarrier(graphicsCmd, TF_ARRAY_COUNT(barriers), barriers, 0, NULL, 0, NULL);

                TriangleFilteringPassDesc triangleFilteringDesc = {};
                triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
                triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;
                triangleFilteringDesc.pDescriptorSetTriangleFilteringPerBatch = pDescriptorTriangleFilteringPerBatch;

#if defined(ENABLE_WORKGRAPH)
                if (gAppSettings.mGpuPipelineWorkgraph)
                {
                    triangleFilteringDesc.pWorkgraph = pWorkgraphGpuPipeline;
                    triangleFilteringDesc.mInitWorkgraph = gFrameCount == 0;
                }
#endif

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
                //{ pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
                { pDepthBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
                { pRenderTargetMSAA, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            };

            const uint32_t maxNumBarriers = NUM_CULLING_VIEWPORTS * 2 + 6;
            uint32_t       barrierCount = 0;
            BufferBarrier  barriers2[maxNumBarriers] = {};
            barriers2[barrierCount++] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
            barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

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

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, rtBarriersCount, rtBarriers);

            drawScene(graphicsCmd, frameIdx);

            if (gAppSettings.mMsaaLevel > 1)
            {
                if (gAppSettings.mEnableVRS)
                {
                    cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "VRS Resolve Pass");
                    pScreenRenderTarget = pResolveVRSRenderTarget[1 - frameIdx];
                    RenderTargetBarrier barriers[] = {
                        { pRenderTargetMSAA, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
                        { pIntermediateRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
                        { pScreenRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                        { pDebugVRSRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
                    };
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 4, barriers);

                    cmdBindPipeline(graphicsCmd, pPipelineResolveCompute);
                    cmdBindDescriptorSet(graphicsCmd, 1 - frameIdx, pDescriptorSetResolveVRSPerBatch);
                    const uint32_t* pThreadGroupSize = pShaderResolveCompute->mNumThreadsPerGroup;
                    cmdDispatch(graphicsCmd, gSceneRes.mWidth / (gDivider * pThreadGroupSize[0]) + 1,
                                gSceneRes.mHeight / (gDivider * pThreadGroupSize[1]) + 1, pThreadGroupSize[2]);

                    cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                    barriers[0] = { pScreenRenderTarget, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_RENDER_TARGET };
                    barriers[1] = { pRenderTargetMSAA, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    barriers[2] = { pDebugVRSRenderTarget, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 3, barriers);
                }
                else
                {
                    // Pixel Puzzle needs the unresolved MSAA texture
                    cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "MSAA Resolve Pass");
                    resolveMSAA(graphicsCmd, frameIdx, gResolveFinalPassIndex, pRenderTargetMSAA, pScreenRenderTarget);
                    cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
                }
            }

            if (gAppSettings.mDrawDebugTargets && !gAppSettings.mEnableVRS && gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
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
            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx],
                                          RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT,
                                          RESOURCE_STATE_UNORDERED_ACCESS };

            barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE,
                                          RESOURCE_STATE_UNORDERED_ACCESS };

            for (uint32_t i = 0; i < gNumViews; ++i)
            {
                barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + i],
                                              RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE,
                                              RESOURCE_STATE_UNORDERED_ACCESS };
            }

            rtBarriers[0] = { pDepthBuffer,
                              gAppSettings.mEnableVRS && gAppSettings.mEnableGodray ? RESOURCE_STATE_DEPTH_READ
                                                                                    : RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                              RESOURCE_STATE_DEPTH_WRITE };
            bool didReadDepthBuffer = gAppSettings.mEnableGodray || (!gAppSettings.mEnableVRS && gAppSettings.mMsaaLevel > SAMPLE_COUNT_1);

            cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, didReadDepthBuffer ? 1 : 0, rtBarriers);

            // Get the current render target for this frame
            acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
            presentImage(graphicsCmd, pScreenRenderTarget, pSwapChain->ppRenderTargets[presentIndex], frameIdx);

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
            Semaphore* waitSemaphores[] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore, gComputeSemaphores[frameIdx] };
            Semaphore* signalSemaphores[] = { graphicsElem.pSemaphore, pPresentSemaphore };

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.mSignalSemaphoreCount = TF_ARRAY_COUNT(signalSemaphores);
            submitDesc.ppCmds = &graphicsCmd;
            submitDesc.ppSignalSemaphores = signalSemaphores;
            submitDesc.pSignalFence = graphicsElem.pFence;
            submitDesc.ppWaitSemaphores = waitSemaphores;
            submitDesc.mWaitSemaphoreCount =
                (gAppSettings.mAsyncCompute && gComputeSemaphores[frameIdx]) ? TF_ARRAY_COUNT(waitSemaphores) : 2;
            queueSubmit(pGraphicsQueue, &submitDesc);

            gPrevGraphicsSemaphore = graphicsElem.pSemaphore;

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

    const char* GetName() { return "15a_VisibilityBufferOIT"; }

    bool addDescriptorSets()
    {
        DescriptorSetDesc setDesc = SRT_SET_DESC(SrtData, Persistent, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPersistent);

        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);

        setDesc = SRT_SET_DESC(SrtDisplay, PerDraw, 3, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDisplayPerDraw);

        // Pre Skin Vertexes
        setDesc = SRT_SET_DESC(SrtPreSkinVertexesComp, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPreSkinVertexes[PRE_SKIN_SYNC]);

        setDesc = SRT_SET_DESC(SrtPreSkinVertexesComp, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPreSkinVertexes[PRE_SKIN_ASYNC]);

        // God Ray Blur
        setDesc = SRT_SET_DESC(SrtGodrayBlurComp, PerDraw, 4, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayBlurPassPerBatch);

        // VB Pass transparent
        setDesc = SRT_SET_DESC(SrtVisibilityPassData, PerBatch, 1, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVisibilityPassTransparentPerBatch);

        // resolve VRS
        setDesc = SRT_SET_DESC(SrtResolveVRSData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetResolveVRSPerBatch);

        // cluster lights
        setDesc = SRT_SET_DESC(SrtClusterLightsData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClusterLightsPerBatch);

        // triangle filtering
        setDesc = SRT_SET_DESC(TriangleFilteringSrtData, PerBatch, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorTriangleFilteringPerBatch);

        const uint32_t resolveMaxSets = gDataBufferCount * gResolveTargetCount;
        setDesc = SRT_SET_DESC(SrtResolve, PerDraw, resolveMaxSets, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetResolve);

        return true;
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetResolve);
        removeDescriptorSet(pRenderer, pDescriptorTriangleFilteringPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetClusterLightsPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetResolveVRSPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetVisibilityPassTransparentPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetDisplayPerDraw);
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);
        removeDescriptorSet(pRenderer, pDescriptorSetPersistent);
        removeDescriptorSet(pRenderer, pDescriptorSetGodRayBlurPassPerBatch);
        removeDescriptorSet(pRenderer, pDescriptorSetPreSkinVertexes[PRE_SKIN_SYNC]);
        removeDescriptorSet(pRenderer, pDescriptorSetPreSkinVertexes[PRE_SKIN_ASYNC]);
    }

    void prepareDescriptorSets()
    {
        // Persistent set
        Texture*       godrayTextures[] = { pRenderTargetGodRay[0]->pTexture, pRenderTargetGodRay[1]->pTexture };
        DescriptorData persistentSetParams[22] = {};
        persistentSetParams[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBTex);
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
        persistentSetParams[4].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_POSITION);
        persistentSetParams[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexTexCoordBuffer);
        persistentSetParams[5].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_TEXCOORD0);
        persistentSetParams[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexNormalBuffer);
        persistentSetParams[6].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_NORMAL);
        persistentSetParams[7].mIndex = SRT_RES_IDX(SrtData, Persistent, gLights);
        persistentSetParams[7].ppBuffers = &pLightsBuffer;
        persistentSetParams[8].mIndex = SRT_RES_IDX(SrtData, Persistent, gShadowMap);
        persistentSetParams[8].ppTextures = &pRenderTargetShadow->pTexture;
        persistentSetParams[9].mIndex = SRT_RES_IDX(SrtData, Persistent, gHeadIndexBufferSRV);
        persistentSetParams[9].ppBuffers = &pHeadIndexBufferOIT;
        persistentSetParams[10].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBDepthLinkedListSRV);
        persistentSetParams[10].ppBuffers = &pVisBufLinkedListBufferOIT;
        persistentSetParams[11].mIndex = SRT_RES_IDX(SrtData, Persistent, gGodrayTexture);
        persistentSetParams[11].ppTextures = &pRenderTargetGodRay[0]->pTexture;
        persistentSetParams[11].mCount = 1;
        persistentSetParams[12].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBTextureSampler);
        persistentSetParams[12].ppSamplers = &pSamplerTrilinearAniso;
        persistentSetParams[13].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBTextureFilter);
        persistentSetParams[13].ppSamplers = &pSamplerPointClamp;
        persistentSetParams[14].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexWeightsBuffer);
        persistentSetParams[14].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_WEIGHTS);
        persistentSetParams[15].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexJointsBuffer);
        persistentSetParams[15].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_JOINTS);
        persistentSetParams[16].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexDataBuffer);
        persistentSetParams[16].ppBuffers = &pGeometryBuffer->mIndex.pBuffer;
        persistentSetParams[17].mIndex = SRT_RES_IDX(SrtData, Persistent, gDepthTexture);
        persistentSetParams[17].ppTextures = &pDepthBuffer->pTexture;
        persistentSetParams[18].mIndex = SRT_RES_IDX(SrtData, Persistent, gGodrayBlurWeights);
        persistentSetParams[18].ppBuffers = &pBufferBlurWeights;
        persistentSetParams[19].mCount = 1;
        persistentSetParams[19].mIndex = SRT_RES_IDX(SrtData, Persistent, gSkyboxTexture);
        persistentSetParams[19].ppTextures = &pSkyboxTri;
        persistentSetParams[20].mIndex = SRT_RES_IDX(SrtData, Persistent, gVBConstantBuffer);
        persistentSetParams[20].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
        persistentSetParams[21].mIndex = SRT_RES_IDX(SrtData, Persistent, gMSAAStencil);
        persistentSetParams[21].ppTextures = &pRenderTargetMSAAEdges->pTexture;
        persistentSetParams[21].mBindStencilResource = true;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetPersistent, 22, persistentSetParams);

        DescriptorData vbPassTransparentParams[3] = {};
        vbPassTransparentParams[0].mIndex = SRT_RES_IDX(SrtVisibilityPassData, PerBatch, gHeadIndexBufferUAV);
        vbPassTransparentParams[0].ppBuffers = &pHeadIndexBufferOIT;
        vbPassTransparentParams[1].mIndex = SRT_RES_IDX(SrtVisibilityPassData, PerBatch, gVBDepthLinkedListUAV);
        vbPassTransparentParams[1].ppBuffers = &pVisBufLinkedListBufferOIT;
        vbPassTransparentParams[2].mIndex = SRT_RES_IDX(SrtVisibilityPassData, PerBatch, gGeometryCountBuffer);
        vbPassTransparentParams[2].ppBuffers = &pGeometryCountBufferOIT;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetVisibilityPassTransparentPerBatch, 3, vbPassTransparentParams);
        // PerFrame set
        DescriptorData perFrameParams[18] = {};

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            perFrameParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gJointMatrixes);
            perFrameParams[0].ppBuffers = &pJointMatrixBuffer[i];
            perFrameParams[1].mIndex = SRT_RES_IDX(SrtData, PerFrame, gOutputBufferOffsets);
            perFrameParams[1].ppBuffers = &pAsyncComputeVertexBuffers->pShaderOffsets[i];
            perFrameParams[2].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameConstants);
            perFrameParams[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
            perFrameParams[3].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPerFrameConstantsComp);
            perFrameParams[3].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
            perFrameParams[4].mIndex = SRT_RES_IDX(SrtData, PerFrame, gMeshDataBuffer);
            perFrameParams[4].ppBuffers = &pMeshDataBuffer[i];
            perFrameParams[5].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilterDispatchGroupDataBuffer);
            perFrameParams[5].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
            perFrameParams[6].mIndex = SRT_RES_IDX(SrtData, PerFrame, gIndirectDataBuffer);
            perFrameParams[6].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            perFrameParams[7].mIndex = SRT_RES_IDX(SrtData, PerFrame, gIndirectDataBuffer);
            perFrameParams[7].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            perFrameParams[8].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClustersCount);
            perFrameParams[8].ppBuffers = &pLightClustersCount[i];
            perFrameParams[9].mIndex = SRT_RES_IDX(SrtData, PerFrame, gLightClusters);
            perFrameParams[9].ppBuffers = &pLightClusters[i];
            perFrameParams[10].mIndex = SRT_RES_IDX(SrtData, PerFrame, gFilteredIndexBuffer);
            perFrameParams[10].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
            perFrameParams[11].mIndex = SRT_RES_IDX(SrtData, PerFrame, gIndirectDataBuffer);
            perFrameParams[11].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            perFrameParams[12].mIndex = SRT_RES_IDX(SrtData, PerFrame, gSkyboxUniformBuffer);
            perFrameParams[12].ppBuffers = &pUniformBufferSkyTri[i];
            perFrameParams[13].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPrevFrameTex);
            perFrameParams[13].ppTextures = &pResolveVRSRenderTarget[1 - i]->pTexture;
            perFrameParams[14].mIndex = SRT_RES_IDX(SrtData, PerFrame, gPrevHistoryTex);
            perFrameParams[14].ppTextures = &pHistoryRenderTarget[1 - i]->pTexture;
            perFrameParams[15].mIndex = SRT_RES_IDX(SrtData, PerFrame, gHistoryTex);
            perFrameParams[15].ppTextures = &pHistoryRenderTarget[i]->pTexture;
            perFrameParams[16].mIndex = SRT_RES_IDX(SrtData, PerFrame, gMsaaSource);
            perFrameParams[16].ppTextures = &pRenderTargetMSAA->pTexture;
            perFrameParams[17].mIndex = SRT_RES_IDX(SrtData, PerFrame, gHistoryTexVBShade);
            perFrameParams[17].ppTextures = &pHistoryRenderTarget[i]->pTexture;
            updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, 18, perFrameParams);
        }

        // resolve VRS set
        DescriptorData resolveVRSParams[2] = {};

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            resolveVRSParams[0].mIndex = SRT_RES_IDX(SrtResolveVRSData, PerBatch, gResolvedTex);
            resolveVRSParams[0].ppTextures = &pResolveVRSRenderTarget[i]->pTexture;
            resolveVRSParams[1].mIndex = SRT_RES_IDX(SrtResolveVRSData, PerBatch, gDebugVRSTex);
            resolveVRSParams[1].ppTextures = &pDebugVRSRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, i, pDescriptorSetResolveVRSPerBatch, 2, resolveVRSParams);
        }

        // cluster lights set
        DescriptorData clusterLightsParams[2] = {};

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            clusterLightsParams[0].mIndex = SRT_RES_IDX(SrtClusterLightsData, PerBatch, gLightClustersCountRW);
            clusterLightsParams[0].ppBuffers = &pLightClustersCount[i];
            clusterLightsParams[1].mIndex = SRT_RES_IDX(SrtClusterLightsData, PerBatch, gLightClustersRW);
            clusterLightsParams[1].ppBuffers = &pLightClusters[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetClusterLightsPerBatch, 2, clusterLightsParams);
        }

        // triangle filtering set
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            DescriptorData triangleFilteringParams[3] = {};
            triangleFilteringParams[0].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDrawClearArgsRW);
            triangleFilteringParams[0].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
            triangleFilteringParams[1].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gFilteredIndexBufferRW);
            triangleFilteringParams[1].mCount = gNumViews;
            triangleFilteringParams[1].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[i * gNumViews];
            triangleFilteringParams[2].mIndex = SRT_RES_IDX(TriangleFilteringSrtData, PerBatch, gIndirectDataBufferRW);
            triangleFilteringParams[2].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorTriangleFilteringPerBatch, 3, triangleFilteringParams);
        }

        // Pre Skin Triangles
        {
            DescriptorData filterParams[5] = {};

            filterParams[0].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexPositionBufferSkinned);
            filterParams[0].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_POSITION);
            filterParams[1].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexNormalBufferSkinned);
            filterParams[1].ppBuffers = &GET_GEOMETRY_VERTEX_BUFFER(SEMANTIC_NORMAL);
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                filterParams[2].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexPositionBufferSkinnedAsync);
                filterParams[2].ppBuffers = &pAsyncComputeVertexBuffers->mPositions.pPreSkinBuffers[i];
                filterParams[3].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexNormalBufferSkinnedAsync);
                filterParams[3].ppBuffers = &pAsyncComputeVertexBuffers->mNormals.pPreSkinBuffers[i];
                filterParams[4].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gBatchData);
                filterParams[4].ppBuffers = &pPreSkinBufferOffsets;
                updateDescriptorSet(pRenderer, i, pDescriptorSetPreSkinVertexes[PRE_SKIN_SYNC], 5, filterParams);
                filterParams[2].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexPositionBufferSkinnedAsync);
                filterParams[2].ppBuffers = &pAsyncComputeVertexBuffers->mPositions.pPreSkinBuffers[i];
                filterParams[3].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gVertexNormalBufferSkinnedAsync);
                filterParams[3].ppBuffers = &pAsyncComputeVertexBuffers->mNormals.pPreSkinBuffers[i];
                filterParams[4].mIndex = SRT_RES_IDX(SrtPreSkinVertexesComp, PerBatch, gBatchData);
                filterParams[4].ppBuffers = &pAsyncComputeVertexBuffers->pShaderOffsets[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetPreSkinVertexes[PRE_SKIN_ASYNC], 5, filterParams);
            }
        }

        // God Ray Blur
        {
            DescriptorData params[2] = {};
            params[0].mIndex = SRT_RES_IDX(SrtGodrayBlurComp, PerDraw, gGodrayTextures);
            params[0].ppTextures = godrayTextures;
            params[0].mCount = 2;
            params[1].mIndex = SRT_RES_IDX(SrtGodrayBlurComp, PerDraw, gBlurParams);
            for (uint32_t i = 0; i < gDataBufferCount; i++)
            {
                params[1].ppBuffers = &pGodRayBlurBuffer[i][0];
                params[1].mCount = 1;
                updateDescriptorSet(pRenderer, i * 2, pDescriptorSetGodRayBlurPassPerBatch, 2, params);
                params[1].ppBuffers = &pGodRayBlurBuffer[i][1];
                params[1].mCount = 1;
                updateDescriptorSet(pRenderer, i * 2 + 1, pDescriptorSetGodRayBlurPassPerBatch, 2, params);
            }
        }

        // Present
        {
            DescriptorData params[3] = {};
            params[0].mIndex = SRT_RES_IDX(SrtDisplay, PerDraw, gDisplayTexture);
            params[0].ppTextures = &pIntermediateRenderTarget->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetDisplayPerDraw, 1, params);
            params[0].ppTextures = &pResolveVRSRenderTarget[0]->pTexture;
            updateDescriptorSet(pRenderer, 1, pDescriptorSetDisplayPerDraw, 1, params);
            params[0].ppTextures = &pResolveVRSRenderTarget[1]->pTexture;
            updateDescriptorSet(pRenderer, 2, pDescriptorSetDisplayPerDraw, 1, params);
        }

        // Resolve
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
    // Order Independent Transparency
    /************************************************************************/

    void addOrderIndependentTransparencyResources()
    {
        const uint32_t width = gSceneRes.mWidth;
        const uint32_t height = gSceneRes.mHeight;

        const uint32_t maxNodeCountOIT = OIT_MAX_FRAG_COUNT * width * height;

        BufferLoadDesc oitHeadIndexBufferDesc = {};
        oitHeadIndexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        oitHeadIndexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        oitHeadIndexBufferDesc.mDesc.mElementCount = width * height;
        oitHeadIndexBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
        oitHeadIndexBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        oitHeadIndexBufferDesc.mDesc.mSize = oitHeadIndexBufferDesc.mDesc.mElementCount * oitHeadIndexBufferDesc.mDesc.mStructStride;
        oitHeadIndexBufferDesc.ppBuffer = &pHeadIndexBufferOIT;
        oitHeadIndexBufferDesc.mDesc.pName = "OIT Head Index";
        addResource(&oitHeadIndexBufferDesc, NULL);

        BufferLoadDesc geometryCountBufferDesc = {};
        geometryCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        geometryCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        geometryCountBufferDesc.mDesc.mElementCount = 1;
        geometryCountBufferDesc.mDesc.mStructStride = sizeof(GeometryCountOIT);
        geometryCountBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        geometryCountBufferDesc.mDesc.mSize = geometryCountBufferDesc.mDesc.mElementCount * geometryCountBufferDesc.mDesc.mStructStride;
        geometryCountBufferDesc.mDesc.pName = "OIT Geometry Count";
        geometryCountBufferDesc.ppBuffer = &pGeometryCountBufferOIT;
        addResource(&geometryCountBufferDesc, NULL);

        BufferLoadDesc linkedListBufferDesc = {};
        linkedListBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
        linkedListBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        linkedListBufferDesc.mDesc.mElementCount = maxNodeCountOIT;
        linkedListBufferDesc.mDesc.mStructStride = sizeof(TransparentNodeOIT);
        linkedListBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        linkedListBufferDesc.mDesc.mSize = linkedListBufferDesc.mDesc.mElementCount * linkedListBufferDesc.mDesc.mStructStride;
        linkedListBufferDesc.ppBuffer = &pVisBufLinkedListBufferOIT;
        linkedListBufferDesc.mDesc.pName = "OIT VisBuf Linked List";
        addResource(&linkedListBufferDesc, NULL);
    }

    void removeOrderIndependentTransparencyResources()
    {
        removeResource(pGeometryCountBufferOIT);

        removeResource(pHeadIndexBufferOIT);
        removeResource(pVisBufLinkedListBufferOIT);
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
        const bool      wantsHDR = OUTPUT_MODE_HDR10 == gAppSettings.mOutputMode;
        const bool      supportsHDR = TinyImageFormat_UNDEFINED != hdrFormat;
        if (pRenderer->pGpu->mHDRSupported)
        {
            strcpy(gAppSettings.pOutputSupportsHDRWidget->mLabel,
                   supportsHDR ? "Current Output Supports HDR" : "Current Output Does Not Support HDR");
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
        const uint32_t width = gSceneRes.mWidth;
        const uint32_t height = gSceneRes.mHeight;

        /************************************************************************/
        /************************************************************************/
        ClearValue optimizedDepthClear = { { 0.0f, 0 } };
        ClearValue optimizedColorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        ClearValue optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

        uint32_t currentOffsetESRAM = 0;

        /************************************************************************/
        // Shadow pass render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "Shadow Maps", currentOffsetESRAM);
        RenderTargetDesc shadowRTDesc = {};
        shadowRTDesc.mArraySize = 1;
        shadowRTDesc.mClearValue = optimizedDepthClear;
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
        ESRAM_CURRENT_OFFSET(pRenderer, shadowMapOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        currentOffsetESRAM = max(shadowMapOffsetESRAM, currentOffsetESRAM);

        /************************************************************************/
        // Main depth buffer
        /************************************************************************/
        // Add depth buffer
        ESRAM_BEGIN_ALLOC(pRenderer, "Depth Buffer", currentOffsetESRAM);
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 0.0f; // optimizedDepthClear;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = height / gDivider;
        depthRT.mSampleCount = gAppSettings.mMsaaLevel;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = width / gDivider;
        depthRT.pName = "Depth Buffer RT";
        depthRT.mFlags = TEXTURE_CREATION_FLAG_SAMPLE_LOCATIONS_COMPATIBLE;
        depthRT.mFlags |= TEXTURE_CREATION_FLAG_ESRAM;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
        ESRAM_CURRENT_OFFSET(pRenderer, depthOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        currentOffsetESRAM = max(depthOffsetESRAM, currentOffsetESRAM);

        /************************************************************************/
        // Visibility buffer pass render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "VB RT", currentOffsetESRAM);
        RenderTargetDesc vbRTDesc = {};
        vbRTDesc.mArraySize = 1;
        vbRTDesc.mClearValue = optimizedColorClearWhite;
        vbRTDesc.mDepth = 1;
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        vbRTDesc.mHeight = height / gDivider;
        vbRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        vbRTDesc.mWidth = width / gDivider;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
        ESRAM_CURRENT_OFFSET(pRenderer, vbOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        currentOffsetESRAM = max(vbOffsetESRAM, currentOffsetESRAM);

        ESRAM_BEGIN_ALLOC(pRenderer, "OIT Depth", currentOffsetESRAM);
        // Add OIT dummy depth buffer
        RenderTargetDesc oitDepthRT = {};
        oitDepthRT.mArraySize = 1;
        oitDepthRT.mClearValue = optimizedDepthClear;
        oitDepthRT.mDepth = 1;
        oitDepthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        oitDepthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        oitDepthRT.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        oitDepthRT.mHeight = height;
        oitDepthRT.mSampleCount = SAMPLE_COUNT_1;
        oitDepthRT.mSampleQuality = 0;
        oitDepthRT.mFlags = TEXTURE_CREATION_FLAG_NONE;
        oitDepthRT.mFlags |= TEXTURE_CREATION_FLAG_ESRAM;
        oitDepthRT.mWidth = width;
        oitDepthRT.pName = "Depth Buffer OIT RT";
        addRenderTarget(pRenderer, &oitDepthRT, &pDepthBufferOIT);
        ESRAM_CURRENT_OFFSET(pRenderer, oitOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        /************************************************************************/
        // Intermediate render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "Intermediate", currentOffsetESRAM);
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
        postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        postProcRTDesc.pName = "pIntermediateRenderTarget";
        addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);
        ESRAM_CURRENT_OFFSET(pRenderer, intermediateOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        /************************************************************************/
        // MSAA render target
        /************************************************************************/
        ESRAM_BEGIN_ALLOC(pRenderer, "MSAA RT", currentOffsetESRAM);
        RenderTargetDesc msaaRTDesc = {};
        msaaRTDesc.mArraySize = 1;
        msaaRTDesc.mClearValue = optimizedColorClearBlack;
        msaaRTDesc.mDepth = 1;
        msaaRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        msaaRTDesc.mFormat = pSwapChain->mFormat;
        msaaRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        msaaRTDesc.mHeight = height / gDivider;
        msaaRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
        msaaRTDesc.mSampleQuality = 0;
        msaaRTDesc.mWidth = width / gDivider;
        msaaRTDesc.pName = "MSAA RT";
        msaaRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
        addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);
        ESRAM_CURRENT_OFFSET(pRenderer, msaaOffsetESRAM);
        ESRAM_END_ALLOC(pRenderer);

        // Debug MSAA Render Target. It will only be used if drawing debug targets is enabled.
        msaaRTDesc.mSampleCount = SAMPLE_COUNT_1;
        msaaRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        msaaRTDesc.mFormat = gDebugRTFormat;
        msaaRTDesc.pName = "MSAA Debug RT";
        addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetDebugMSAA);

        currentOffsetESRAM = max(msaaOffsetESRAM, max(intermediateOffsetESRAM, max(oitOffsetESRAM, currentOffsetESRAM)));

        /************************************************************************/
        // VRS History render targets
        /************************************************************************/
        RenderTargetDesc historyRTDesc = {};
        historyRTDesc.mWidth = width / gDivider;
        historyRTDesc.mHeight = height / gDivider;
        historyRTDesc.mDepth = 1;
        historyRTDesc.mArraySize = 1;
        historyRTDesc.mMipLevels = 1;
        historyRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        historyRTDesc.mSampleCount = SAMPLE_COUNT_4;
        historyRTDesc.mFormat = TinyImageFormat_R8_UINT;
        historyRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        historyRTDesc.mClearValue.r = 0.0f;
        historyRTDesc.mSampleQuality = 0;
        historyRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        historyRTDesc.pName = "Color History RT A";
        addRenderTarget(pRenderer, &historyRTDesc, &pHistoryRenderTarget[0]);
        historyRTDesc.pName = "Color History RT B";
        addRenderTarget(pRenderer, &historyRTDesc, &pHistoryRenderTarget[1]);
        /************************************************************************/
        // VRS Resolve and Debug render targets
        /************************************************************************/
        RenderTargetDesc resolveVRSRTDesc = {};
        resolveVRSRTDesc.mWidth = width;
        resolveVRSRTDesc.mHeight = height;
        resolveVRSRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        resolveVRSRTDesc.mDepth = 1;
        resolveVRSRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        resolveVRSRTDesc.mArraySize = 1;
        resolveVRSRTDesc.mMipLevels = 1;
        resolveVRSRTDesc.mSampleCount = SAMPLE_COUNT_1;
        resolveVRSRTDesc.mFormat = TinyImageFormat_B10G11R11_UFLOAT;
        resolveVRSRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        resolveVRSRTDesc.mSampleQuality = 0;
        resolveVRSRTDesc.pName = "VRS Resolve RT A";
        addRenderTarget(pRenderer, &resolveVRSRTDesc, &pResolveVRSRenderTarget[0]);
        resolveVRSRTDesc.pName = "VRS Resolve RT B";
        addRenderTarget(pRenderer, &resolveVRSRTDesc, &pResolveVRSRenderTarget[1]);
        resolveVRSRTDesc.pName = "VRS Debug RT";
        resolveVRSRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        addRenderTarget(pRenderer, &resolveVRSRTDesc, &pDebugVRSRenderTarget);

        /************************************************************************/
        // GodRay render target
        /************************************************************************/
        TinyImageFormat  GRRTFormat = (pRenderer->pGpu->mFormatCaps[TinyImageFormat_R10G10B10A2_UNORM] & (FORMAT_CAP_READ_WRITE))
                                          ? TinyImageFormat_R10G10B10A2_UNORM
                                          : TinyImageFormat_R8G8B8A8_UNORM;
        RenderTargetDesc GRRTDesc = {};
        GRRTDesc.mArraySize = 1;
        GRRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        GRRTDesc.mDepth = 1;
        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mHeight = height / GODRAY_SCALE;
        GRRTDesc.mWidth = width / GODRAY_SCALE;
        GRRTDesc.mFormat = GRRTFormat;
        GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        GRRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        GRRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        GRRTDesc.pName = "GodRay RT A";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[0]);
        GRRTDesc.pName = "GodRay RT B";
        GRRTDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRay[1]);

        GRRTDesc.pName = "GodRay RT - MSAA";
        GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRayMS);

        GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GRRTDesc.mSampleCount = SAMPLE_COUNT_1;
        GRRTDesc.mFormat = gDebugRTFormat;
        GRRTDesc.pName = "GodRay RT - Debug MSAA View";
        addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetDebugGodRayMSAA);

        /************************************************************************/
        // MSAA edges render target
        /************************************************************************/
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
        RenderTargetDesc msaaStencilRT = {};
        msaaStencilRT.mArraySize = 1;
        msaaStencilRT.mClearValue = optimizedDepthClear;
        msaaStencilRT.mDepth = 1;
        msaaStencilRT.mFormat = stencilImageFormat;
        msaaStencilRT.mStartState = RESOURCE_STATE_DEPTH_READ;
        msaaStencilRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        msaaStencilRT.mHeight = height;
        msaaStencilRT.mWidth = width;
        msaaStencilRT.mSampleCount = gAppSettings.mMsaaLevel;
        msaaStencilRT.mSampleQuality = 0;
        msaaStencilRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        msaaStencilRT.pName = "Stencil Buffer MSAA";
        addRenderTarget(pRenderer, &msaaStencilRT, &pRenderTargetMSAAEdges);

        msaaStencilRT.pName = "Downscaled Stencil Buffer MSAA";
        msaaStencilRT.mHeight = height / GODRAY_SCALE;
        msaaStencilRT.mWidth = width / GODRAY_SCALE;
        addRenderTarget(pRenderer, &msaaStencilRT, &pRenderTargetMSAAEdgesDownscaled);

        /************************************************************************/
        /************************************************************************/
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pRenderTargetGodRayMS);
        removeRenderTarget(pRenderer, pRenderTargetGodRay[0]);
        removeRenderTarget(pRenderer, pRenderTargetGodRay[1]);

        removeRenderTarget(pRenderer, pHistoryRenderTarget[0]);
        removeRenderTarget(pRenderer, pHistoryRenderTarget[1]);

        removeRenderTarget(pRenderer, pResolveVRSRenderTarget[0]);
        removeRenderTarget(pRenderer, pResolveVRSRenderTarget[1]);
        removeRenderTarget(pRenderer, pDebugVRSRenderTarget);

        removeRenderTarget(pRenderer, pRenderTargetDebugMSAA);
        removeRenderTarget(pRenderer, pRenderTargetDebugGodRayMSAA);
        removeRenderTarget(pRenderer, pRenderTargetMSAAEdgesDownscaled);
        removeRenderTarget(pRenderer, pRenderTargetMSAAEdges);
        removeRenderTarget(pRenderer, pIntermediateRenderTarget);
        removeRenderTarget(pRenderer, pRenderTargetMSAA);
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pDepthBufferOIT);
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
        ShaderLoadDesc shadowPassTrans = {};
        ShaderLoadDesc vbPass = {};
        ShaderLoadDesc vbPassAlpha[2] = {};
        ShaderLoadDesc vbPassTrans = {};
        // MSAA + VRS + Godray variants...
        ShaderLoadDesc vbShade[kNumVisBufShaderVariants] = {};
        ShaderLoadDesc resolvePass[MSAA_LEVELS_COUNT] = {};
        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc preSkinVertexes = {};
        ShaderLoadDesc preSkinVertexesAsync = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};

        shadowPass.mVert.pFileName = "shadow_pass.vert";

        shadowPassAlpha.mVert = {
            "shadow_pass_alpha.vert",
        };
        shadowPassAlpha.mFrag = {
            "shadow_pass_alpha.frag",
        };

        shadowPassTrans.mVert = { "shadow_pass_transparent.vert" };
        shadowPassTrans.mFrag = { "shadow_pass_transparent.frag" };

        vbPass.mVert = { "visibilityBuffer_pass.vert" };
        vbPass.mFrag = { "visibilityBuffer_pass.frag" };

        vbPassAlpha[0].mVert = { "visibilityBuffer_pass_alpha.vert" };
        vbPassAlpha[0].mFrag = { "visibilityBuffer_pass_alpha.frag" };

        vbPassAlpha[1].mVert = { "visibilityBuffer_pass_alpha.vert" };
        vbPassAlpha[1].mFrag = { "visibilityBuffer_pass_alpha_vrs.frag" };

        vbPassTrans.mVert = { "visibilityBuffer_pass_transparent_ret.vert" };
        vbPassTrans.mFrag = { "visibilityBuffer_pass_transparent_void.frag" };

        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough;
        if (addGeometryPassThrough)
        {
            // a passthrough gs
            vbPass.mGeom = { "visibilityBuffer_pass.geom" };
            vbPassAlpha[0].mGeom = { "visibilityBuffer_pass_alpha.geom" };
            vbPassAlpha[1].mGeom = { "visibilityBuffer_pass_alpha.geom" };
            vbPassTrans.mGeom = { "visibilityBuffer_pass_transparent_ret.geom" };
            vbPassTrans.mFrag = { "visibilityBuffer_pass_transparent_ret.frag" };
        }

        const char* visibilityBuffer_shade[kNumVisBufShaderVariants] = { "visibilityBuffer_shade_SAMPLE_COUNT_1.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_1_AO.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_2.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_2_AO.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_4.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_4_AO.frag",
                                                                         "visibilityBuffer_shade_VRS.frag",
                                                                         "visibilityBuffer_shade_VRS_AO.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_1_GRAY.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_1_AO_GRAY.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_2_GRAY.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_2_AO_GRAY.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_4_GRAY.frag",
                                                                         "visibilityBuffer_shade_SAMPLE_COUNT_4_AO_GRAY.frag",
                                                                         "visibilityBuffer_shade_VRS_GRAY.frag",
                                                                         "visibilityBuffer_shade_VRS_AO_GRAY.frag" };

        for (uint32_t i = 0; i < TF_ARRAY_COUNT(visibilityBuffer_shade); ++i)
        {
            vbShade[i].mVert.pFileName = "visibilityBuffer_shade.vert";
            vbShade[i].mFrag.pFileName = visibilityBuffer_shade[i];
        }

        const char* resolve[] = { "progMSAAResolve_SAMPLE_COUNT_1.frag", "progMSAAResolve_SAMPLE_COUNT_2.frag",
                                  "progMSAAResolve_SAMPLE_COUNT_4.frag" };

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            // Resolve shader
            resolvePass[i].mVert.pFileName = "progMSAAResolve.vert";
            resolvePass[i].mFrag.pFileName = resolve[i];
        }

        preSkinVertexes.mComp.pFileName = "pre_skin_vertexes.comp";
        preSkinVertexesAsync.mComp.pFileName = "pre_skin_vertexes_async.comp";

        // Triangle culling compute shader
        triangleCulling.mComp.pFileName = "triangle_filtering.comp";
        // Clear buffers compute shader
        clearBuffer.mComp.pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mComp.pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mComp.pFileName = "cluster_lights.comp";

        ShaderLoadDesc oitHeadIndexClearDesc = {};
        oitHeadIndexClearDesc.mVert.pFileName = "display.vert";
        oitHeadIndexClearDesc.mFrag.pFileName = "oitClear.frag";
        addShader(pRenderer, &oitHeadIndexClearDesc, &pShaderClearHeadIndexOIT);

        const char* godrayShaderFileName[] = { "godray_SAMPLE_COUNT_1.frag", "godray_SAMPLE_COUNT_2.frag", "godray_SAMPLE_COUNT_4.frag",
                                               "godray_VRS.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT + 1; ++i)
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

        ShaderLoadDesc fillStencilDesc = {};
        fillStencilDesc.mVert.pFileName = "fillStencil.vert";
        fillStencilDesc.mFrag.pFileName = "fillStencil.frag";

        ShaderLoadDesc resolveComputeDesc = {};
        resolveComputeDesc.mComp.pFileName = "resolveVRS.comp";

        ShaderLoadDesc msaaEdgesShader[MSAA_LEVELS_COUNT - 1] = {};
        const char*    edgeDetectShaders[] = { "msaa_edge_detect_SAMPLE_2.frag", "msaa_edge_detect_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            msaaEdgesShader[i].mVert.pFileName = "display.vert";
            msaaEdgesShader[i].mFrag.pFileName = edgeDetectShaders[i];
        }

        ShaderLoadDesc downscaleMSAAEdgesShader[MSAA_LEVELS_COUNT - 1] = {};
        const char*    edgeDetectDownscaleShaders[] = { "msaa_stencil_downscale_SAMPLE_2.frag", "msaa_stencil_downscale_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            downscaleMSAAEdgesShader[i].mVert.pFileName = "display.vert";
            downscaleMSAAEdgesShader[i].mFrag.pFileName = edgeDetectDownscaleShaders[i];
        }

        ShaderLoadDesc msaaDebugShader[MSAA_LEVELS_COUNT - 1] = {};
        const char*    debugMSAAFrag[] = { "msaa_debug_SAMPLE_2.frag", "msaa_debug_SAMPLE_4.frag" };
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            msaaDebugShader[i].mVert.pFileName = "display.vert";
            msaaDebugShader[i].mFrag.pFileName = debugMSAAFrag[i];
        }

        addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);

        addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &shadowPassTrans, &pShaderShadowPass[GEOMSET_ALPHA_BLEND]);
        addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &vbPassAlpha[0], &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbPassTrans, &pShaderVisibilityBufferPass[GEOMSET_ALPHA_BLEND]);
        addShader(pRenderer, &vbPassAlpha[1], &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT_VRS]);
        for (uint32_t i = 0; i < kNumVisBufShaderVariants; ++i)
            addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &preSkinVertexes, &pShaderPreSkinVertexes[PRE_SKIN_SYNC]);
        addShader(pRenderer, &preSkinVertexesAsync, &pShaderPreSkinVertexes[PRE_SKIN_ASYNC]);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            addShader(pRenderer, &resolvePass[i], &pShaderResolve[i]);
        }
        addShader(pRenderer, &fillStencilDesc, &pShaderFillStencil);
        addShader(pRenderer, &resolveComputeDesc, &pShaderResolveCompute);

#if defined(ENABLE_WORKGRAPH)
        if (pRenderer->pGpu->mWorkgraphSupported)
        {
            ShaderLoadDesc gpuPipelineDesc = {};
            gpuPipelineDesc.mGraph = { "gpu_pipeline.graph" };
            addShader(pRenderer, &gpuPipelineDesc, &pShaderGpuPipeline);
        }
#endif
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
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHA_BLEND]);

        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_BLEND]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT_VRS]);

        for (uint32_t i = 0; i < kNumVisBufShaderVariants; ++i)
            removeShader(pRenderer, pShaderVisibilityBufferShade[i]);

        removeShader(pRenderer, pShaderPreSkinVertexes[PRE_SKIN_SYNC]);
        removeShader(pRenderer, pShaderPreSkinVertexes[PRE_SKIN_ASYNC]);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
        {
            removeShader(pRenderer, pShaderResolve[i]);
        }

        removeShader(pRenderer, pShaderClearHeadIndexOIT);

        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT + 1; ++i)
            removeShader(pRenderer, pGodRayPass[i]);
        removeShader(pRenderer, pShaderGodRayBlurPass);

        removeShader(pRenderer, pShaderPresentPass);
        removeShader(pRenderer, pShaderFillStencil);
        removeShader(pRenderer, pShaderResolveCompute);

#if defined(ENABLE_WORKGRAPH)
        if (pShaderGpuPipeline)
        {
            removeShader(pRenderer, pShaderGpuPipeline);
            pShaderGpuPipeline = {};
        }
#endif
        for (uint32_t i = 0; i < MSAA_LEVELS_COUNT - 1; ++i)
        {
            removeShader(pRenderer, pShaderDebugMSAA[i]);
            removeShader(pRenderer, pShaderDownscaleMSAAEdges[i]);
            removeShader(pRenderer, pShaderDrawMSAAEdges[i]);
        }
    }

    void addPipelines()
    {
        /************************************************************************/
        // Vertex layout used by all geometry passes (shadow, visibility)
        /************************************************************************/
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GREATER;
        DepthStateDesc depthStateDisableDesc = {};
        DepthStateDesc depthStateRWStencilDesc = {};
        depthStateRWStencilDesc.mDepthTest = false;
        depthStateRWStencilDesc.mDepthWrite = false;
        depthStateRWStencilDesc.mStencilWriteMask = 0xFF;
        depthStateRWStencilDesc.mStencilReadMask = 0xFF;
        depthStateRWStencilDesc.mStencilTest = true;
        depthStateRWStencilDesc.mStencilFrontFunc = CMP_ALWAYS;
        depthStateRWStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilFrontPass = STENCIL_OP_REPLACE;
        depthStateRWStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilBackFunc = CMP_ALWAYS;
        depthStateRWStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilBackPass = STENCIL_OP_REPLACE;
        depthStateRWStencilDesc.mDepthBackFail = STENCIL_OP_KEEP;
        DepthStateDesc depthStateOnlyStencilDesc = {};
        depthStateOnlyStencilDesc.mStencilWriteMask = 0x00;
        depthStateOnlyStencilDesc.mStencilReadMask = 0xFF;
#if defined(METAL)
        depthStateOnlyStencilDesc.mStencilTest = false;

#else
        depthStateOnlyStencilDesc.mStencilTest = true;
#endif
        depthStateOnlyStencilDesc.mStencilFrontFunc = CMP_EQUAL;
        depthStateOnlyStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyStencilDesc.mStencilFrontPass = STENCIL_OP_KEEP;
        depthStateOnlyStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyStencilDesc.mStencilBackFunc = CMP_EQUAL;
        depthStateOnlyStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateOnlyStencilDesc.mStencilBackPass = STENCIL_OP_KEEP;
        depthStateOnlyStencilDesc.mDepthBackFail = STENCIL_OP_KEEP;

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };

        RasterizerStateDesc rasterizerStateCullNoneMsDesc = { CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID };
        rasterizerStateCullNoneMsDesc.mMultiSample = true;
        RasterizerStateDesc rasterizerStateCullFrontMsDesc = { CULL_MODE_FRONT, 0, 0, FILL_MODE_SOLID };
        rasterizerStateCullFrontMsDesc.mMultiSample = true;

        // Setup pipeline settings
        PipelineDesc pipelineDesc = {};
        pipelineDesc.pCache = pPipelineCache;

        /************************************************************************/
        // Setup compute pipelines for triangle filtering
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(TriangleFilteringSrtData, Persistent),
                             SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerFrame), SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerBatch),
                             NULL);
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& computePipelineSettings = pipelineDesc.mComputeDesc;
        computePipelineSettings.pShaderProgram = pShaderClearBuffers;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

        // God Ray Blur Pass
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtGodrayBlurComp, Persistent), NULL, NULL,
                             SRT_LAYOUT_DESC(SrtGodrayBlurComp, PerDraw));
        computePipelineSettings.pShaderProgram = pShaderGodRayBlurPass;
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayBlurPass);

        // Pre Skin Vertexes
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtPreSkinVertexesComp, Persistent),
                             SRT_LAYOUT_DESC(SrtPreSkinVertexesComp, PerFrame), SRT_LAYOUT_DESC(SrtPreSkinVertexesComp, PerBatch), NULL);
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        computePipelineSettings.pShaderProgram = pShaderPreSkinVertexes[PRE_SKIN_SYNC];
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePreSkinVertexes[PRE_SKIN_SYNC]);

        computePipelineSettings.pShaderProgram = pShaderPreSkinVertexes[PRE_SKIN_ASYNC];
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePreSkinVertexes[PRE_SKIN_ASYNC]);

        // Create the compute pipeline for GPU triangle filtering
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(TriangleFilteringSrtData, Persistent),
                             SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerFrame), SRT_LAYOUT_DESC(TriangleFilteringSrtData, PerBatch),
                             NULL);
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        computePipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

        // Setup the clearing light clusters pipeline
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtClusterLightsData, Persistent),
                             SRT_LAYOUT_DESC(SrtClusterLightsData, PerFrame), SRT_LAYOUT_DESC(SrtClusterLightsData, PerBatch), NULL);
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        computePipelineSettings.pShaderProgram = pShaderClearLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // Setup the compute the light clusters pipeline
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtClusterLightsData, Persistent),
                             SRT_LAYOUT_DESC(SrtClusterLightsData, PerFrame), SRT_LAYOUT_DESC(SrtClusterLightsData, PerBatch), NULL);
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        computePipelineSettings.pShaderProgram = pShaderClusterLights;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);

#if defined(ENABLE_WORKGRAPH)
        if (pRenderer->pGpu->mWorkgraphSupported)
        {
            pipelineDesc.mType = PIPELINE_TYPE_WORKGRAPH;
            pipelineDesc.mWorkgraphDesc.pShaderProgram = pShaderGpuPipeline;
            pipelineDesc.mWorkgraphDesc.pWorkgraphName = "GPUPipeline";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineGpuPipeline);

            WorkgraphDesc workgraphDesc = {};
            workgraphDesc.pPipeline = pPipelineGpuPipeline;
            addWorkgraph(pRenderer, &workgraphDesc, &pWorkgraphGpuPipeline);
        }
#endif
        /************************************************************************/
        // Setup MSAA resolve pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, NULL, NULL, NULL, SRT_LAYOUT_DESC(SrtResolve, PerDraw));
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
        // Setup Head Index Clear Pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtVisibilityPassData, Persistent),
                             SRT_LAYOUT_DESC(SrtVisibilityPassData, PerFrame), SRT_LAYOUT_DESC(SrtVisibilityPassData, PerBatch), NULL);
        GraphicsPipelineDesc& headIndexPipelineSettings = pipelineDesc.mGraphicsDesc;
        headIndexPipelineSettings = { 0 };
        headIndexPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        headIndexPipelineSettings.mRenderTargetCount = 0;
        headIndexPipelineSettings.pColorFormats = NULL;
        headIndexPipelineSettings.mSampleCount = pDepthBufferOIT->mSampleCount;
        headIndexPipelineSettings.mSampleQuality = 0;
        headIndexPipelineSettings.mDepthStencilFormat = pDepthBufferOIT->mFormat;
        headIndexPipelineSettings.pVertexLayout = NULL;
        headIndexPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        headIndexPipelineSettings.pDepthState = &depthStateDisableDesc;
        headIndexPipelineSettings.pBlendState = NULL;
        headIndexPipelineSettings.pShaderProgram = pShaderClearHeadIndexOIT;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearHeadIndexOIT);

        /************************************************************************/
        // Setup the Shadow Pass Pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
        shadowPipelineSettings = { 0 };
        shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        shadowPipelineSettings.pDepthState = &depthStateDesc;
        shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
        shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
        shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
        shadowPipelineSettings.pRasterizerState =
            gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_OPAQUE];
        shadowPipelineSettings.pVertexLayout = NULL;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_OPAQUE]);

        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_ALPHA_CUTOUT];
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_ALPHA_CUTOUT]);

        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_ALPHA_BLEND];
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_ALPHA_BLEND]);

        /************************************************************************/
        // Setup the Visibility Buffer Pass Pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtVisibilityPassData, Persistent),
                             SRT_LAYOUT_DESC(SrtVisibilityPassData, PerFrame), SRT_LAYOUT_DESC(SrtVisibilityPassData, PerBatch), NULL);
        TinyImageFormat       formats[] = { pRenderTargetVBPass->mFormat };
        // Setup pipeline settings
        GraphicsPipelineDesc& vbPassPipelineSettings = pipelineDesc.mGraphicsDesc;
        vbPassPipelineSettings = { 0 };
        vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbPassPipelineSettings.pColorFormats = formats;
        vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
        vbPassPipelineSettings.pVertexLayout = NULL;

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            vbPassPipelineSettings.pDepthState = (i == GEOMSET_ALPHA_BLEND) ? &depthStateDisableDesc : &depthStateDesc;
            vbPassPipelineSettings.mSampleCount = (i == GEOMSET_ALPHA_BLEND) ? SAMPLE_COUNT_1 : pRenderTargetVBPass->mSampleCount;
            vbPassPipelineSettings.mRenderTargetCount = (i == GEOMSET_ALPHA_BLEND) ? 0 : 1;
            vbPassPipelineSettings.mDepthStencilFormat = (i == GEOMSET_ALPHA_BLEND) ? pDepthBufferOIT->mFormat : pDepthBuffer->mFormat;
            vbPassPipelineSettings.mUseCustomSampleLocations = (i != GEOMSET_ALPHA_BLEND) && gAppSettings.mEnableVRS;
            vbPassPipelineSettings.pRasterizerState = (i == GEOMSET_ALPHA_BLEND) || (gAppSettings.mMsaaLevel == 1)
                                                          ? &rasterizerStateCullNoneDesc
                                                          : &rasterizerStateCullNoneMsDesc;
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
            pipelineDesc.mExtensionCount = TF_ARRAY_COUNT(edescs);
#endif
            addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferPass[i]);

            pipelineDesc.mExtensionCount = 0;
        }
        /************************************************************************/
        // Setup the resources needed for the Visibility Buffer Shade Pipeline
        /************************************************************************/
        // Create pipeline
        // Note: the vertex layout is set to null because the positions of the fullscreen triangle are being calculated automatically
        // in the vertex shader using each vertex_id.
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& vbShadePipelineSettings = pipelineDesc.mGraphicsDesc;
        vbShadePipelineSettings = { 0 };
        vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbShadePipelineSettings.mRenderTargetCount = 1;
        vbShadePipelineSettings.pDepthState = &depthStateDisableDesc;
        vbShadePipelineSettings.pRasterizerState =
            gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullFrontMsDesc : &rasterizerStateCullFrontDesc;

        // Can't re-use VRS stencil because it has different settings on Metal
        DepthStateDesc depthStateOnlyReadStencilProgMSAADesc = {};
        depthStateOnlyReadStencilProgMSAADesc.mStencilWriteMask = 0x00;
        depthStateOnlyReadStencilProgMSAADesc.mStencilReadMask = 0xFF;
        depthStateOnlyReadStencilProgMSAADesc.mStencilTest = true;
        depthStateOnlyReadStencilProgMSAADesc.mStencilFrontFunc = CMP_EQUAL;
        depthStateOnlyReadStencilProgMSAADesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilProgMSAADesc.mStencilFrontPass = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilProgMSAADesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilProgMSAADesc.mStencilBackFunc = CMP_EQUAL;
        depthStateOnlyReadStencilProgMSAADesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilProgMSAADesc.mStencilBackPass = STENCIL_OP_KEEP;
        depthStateOnlyReadStencilProgMSAADesc.mDepthBackFail = STENCIL_OP_KEEP;

        for (uint32_t i = 0; i < 2; ++i)
        {
            uint32_t shaderIndex = (gAppSettings.mMsaaIndex * 2 + i) + (2 * (MSAA_LEVELS_COUNT + 1) * gAppSettings.mEnableGodray);
            vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[shaderIndex];
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

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_PIXEL_SHADER_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader =
                !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

            if (!gAppSettings.mEnableVRS)
            {
                pipelineDesc.pPipelineExtensions = edescs;
                pipelineDesc.mExtensionCount = TF_ARRAY_COUNT(edescs);
            }
#endif
            if (gAppSettings.mEnableVRS)
            {
                vbShadePipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
                vbShadePipelineSettings.pDepthState = &depthStateOnlyStencilDesc;
                shaderIndex = (gAppSettings.mMsaaIndex + 1) * 2 + i + (2 * (MSAA_LEVELS_COUNT + 1) * gAppSettings.mEnableGodray);
                vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[shaderIndex];
            }
            else if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
            {
                vbShadePipelineSettings.mDepthStencilFormat = pRenderTargetMSAAEdges->mFormat;
                vbShadePipelineSettings.pDepthState = &depthStateOnlyReadStencilProgMSAADesc;
            }
            addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferShadeSrgb[i]);

            pipelineDesc.mExtensionCount = 0;
        }

        if (gAppSettings.mMsaaLevel > 1)
        {
            vbShadePipelineSettings.pColorFormats = &pRenderTargetMSAA->mFormat;
            vbShadePipelineSettings.mSampleQuality = pRenderTargetMSAA->mSampleQuality;
        }
        else
        {
            vbShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
            vbShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        }

        /************************************************************************/
        // Setup Godray pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& pipelineSettingsGodRay = pipelineDesc.mGraphicsDesc;
        pipelineSettingsGodRay = { 0 };
        pipelineSettingsGodRay.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsGodRay.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsGodRay.mRenderTargetCount = 1;
        pipelineSettingsGodRay.pColorFormats = &pRenderTargetGodRay[0]->mFormat;
        bool isProgMSAAEnabled = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 && !gAppSettings.mEnableVRS;
        pipelineSettingsGodRay.mSampleCount = isProgMSAAEnabled ? gAppSettings.mMsaaLevel : SAMPLE_COUNT_1;
        pipelineSettingsGodRay.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsGodRay.pShaderProgram = pGodRayPass[gAppSettings.mMsaaIndex + gAppSettings.mEnableVRS];
        pipelineSettingsGodRay.pDepthState = isProgMSAAEnabled ? &depthStateOnlyReadStencilProgMSAADesc : &depthStateDisableDesc;
        pipelineSettingsGodRay.mDepthStencilFormat =
            isProgMSAAEnabled ? pRenderTargetMSAAEdgesDownscaled->mFormat : TinyImageFormat_UNDEFINED;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineGodRayPass);

        /************************************************************************/
        // Setup Present pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtDisplay, Persistent), SRT_LAYOUT_DESC(SrtDisplay, PerFrame), NULL,
                             SRT_LAYOUT_DESC(SrtDisplay, PerDraw));
        GraphicsPipelineDesc& pipelineSettingsFinalPass = pipelineDesc.mGraphicsDesc;
        pipelineSettingsFinalPass = { 0 };
        pipelineSettingsFinalPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettingsFinalPass.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelineSettingsFinalPass.mRenderTargetCount = 1;
        pipelineSettingsFinalPass.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;

        addPipeline(pRenderer, &pipelineDesc, &pPipelinePresentPass);

        /************************************************************************/
        // Setup Fill VRS map pipeline
        /************************************************************************/
        PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
        GraphicsPipelineDesc& pipelinefillStencilPass = pipelineDesc.mGraphicsDesc;
        pipelinefillStencilPass = { 0 };
        pipelinefillStencilPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelinefillStencilPass.mRenderTargetCount = 1;
        pipelinefillStencilPass.pColorFormats = &pHistoryRenderTarget[0]->mFormat;
        pipelinefillStencilPass.mSampleCount = pDepthBuffer->mSampleCount;
        pipelinefillStencilPass.mSampleQuality = pDepthBuffer->mSampleQuality;
        pipelinefillStencilPass.mDepthStencilFormat = pDepthBuffer->mFormat;
        pipelinefillStencilPass.pVertexLayout = NULL;
        pipelinefillStencilPass.pRasterizerState = &rasterizerStateCullNoneDesc;
        pipelinefillStencilPass.pBlendState = NULL;
        pipelinefillStencilPass.pDepthState = &depthStateRWStencilDesc;
        pipelinefillStencilPass.pShaderProgram = pShaderFillStencil;
        pipelinefillStencilPass.mUseCustomSampleLocations = gAppSettings.mEnableVRS;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineFillStencil);

        /************************************************************************/
        // Setup Resolve VRS pipeline
        /************************************************************************/
        PipelineDesc pipelineComputeDesc = {};
        PIPELINE_LAYOUT_DESC(pipelineComputeDesc, SRT_LAYOUT_DESC(SrtResolveVRSData, Persistent),
                             SRT_LAYOUT_DESC(SrtResolveVRSData, PerFrame), SRT_LAYOUT_DESC(SrtResolveVRSData, PerBatch), NULL);

        pipelineComputeDesc.pName = "Resolve Pipeline";
        pipelineComputeDesc.mType = PIPELINE_TYPE_COMPUTE;

        ComputePipelineDesc& pipelineResolveComputeDesc = pipelineComputeDesc.mComputeDesc;
        pipelineResolveComputeDesc.pShaderProgram = pShaderResolveCompute;
        addPipeline(pRenderer, &pipelineComputeDesc, &pPipelineResolveCompute);

        /************************************************************************/
        // Setup MSAA edge detect pipeline
        /************************************************************************/
        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
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

            pipelineDesc = {};
            PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame), NULL, NULL);
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            GraphicsPipelineDesc& msaaEdgesPipeline = pipelineDesc.mGraphicsDesc;
            msaaEdgesPipeline = { 0 };
            msaaEdgesPipeline.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            msaaEdgesPipeline.mRenderTargetCount = 0;
            msaaEdgesPipeline.pDepthState = &depthStateOnlyWriteStencilDesc;
            msaaEdgesPipeline.mDepthStencilFormat = pRenderTargetMSAAEdges->mFormat;
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
        removePipeline(pRenderer, pPipelineResolveCompute);
        removePipeline(pRenderer, pPipelineFillStencil);
        removePipeline(pRenderer, pPipelineResolveGodRay);
        removePipeline(pRenderer, pPipelineResolve);

        removePipeline(pRenderer, pPipelineGodRayPass);
        removePipeline(pRenderer, pPipelineGodRayBlurPass);

        removePipeline(pRenderer, pPipelineClearHeadIndexOIT);

        removePipeline(pRenderer, pPipelinePresentPass);

        // Destroy graphics pipelines
        for (uint32_t i = 0; i < 2; ++i)
        {
            removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
        }

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
            removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
            removePipeline(pRenderer, pPipelineShadowPass[i]);

        removePipeline(pRenderer, pPipelineClusterLights);
        removePipeline(pRenderer, pPipelineClearLightClusters);
        removePipeline(pRenderer, pPipelinePreSkinVertexes[PRE_SKIN_SYNC]);
        removePipeline(pRenderer, pPipelinePreSkinVertexes[PRE_SKIN_ASYNC]);
        removePipeline(pRenderer, pPipelineTriangleFiltering);
        removePipeline(pRenderer, pPipelineClearBuffers);

#if defined(ENABLE_WORKGRAPH)
        if (pPipelineGpuPipeline)
        {
            removePipeline(pRenderer, pPipelineGpuPipeline);
            removeWorkgraph(pRenderer, pWorkgraphGpuPipeline);
            pPipelineGpuPipeline = {};
            pWorkgraphGpuPipeline = {};
        }
#endif
        if (gAppSettings.mMsaaLevel > SAMPLE_COUNT_1)
        {
            removePipeline(pRenderer, pPipelineDebugMSAA);
            removePipeline(pRenderer, pPipelineDownscaleMSAAEdges);
            removePipeline(pRenderer, pPipelineDrawMSAAEdges);
        }
    }

    // This method sets the contents of the buffers to indicate the rendering pass that
    // the whole scene triangles must be rendered (no cluster / triangle filtering).
    // This is useful for testing purposes to compare visual / performance results.
    void addTriangleFilteringBuffers(Scene* pScene_)
    {
        UNREF_PARAM(pScene_);
        /************************************************************************/
        // Mesh constants
        /************************************************************************/
        // create mesh constants buffer
        uint32_t meshCount = 0;
        uint32_t vertexCount = 0;
        uint32_t materialID = 0;
        for (size_t pi = 0; pi < TF_ARRAY_COUNT(gGeometry); ++pi)
        {
            Geometry* part = gGeometry[pi];
            for (uint32_t instanceIdx = 0; instanceIdx < (pi == 0 ? 1 : gMaxMeshInstances); ++instanceIdx)
            {
                for (uint32_t di = 0; di < part->mDrawArgCount; ++di)
                {
                    uint32_t flag = pi == 0 ? pScene->materialFlags[meshCount] : 0;
                    uint32_t materialId = pi == 0 ? materialID++ : materialID;
                    for (uint32_t i = 0; i < gDataBufferCount; ++i)
                    {
                        gPerFrame[i].pMeshData[meshCount].indexOffset =
                            part->mIndexBufferChunk.mOffset / IndexTypeToSize(part->mIndexType) + part->pDrawArgs[di].mStartIndex;
                        gPerFrame[i].pMeshData[meshCount].vertexOffset =
                            part->mVertexBufferChunks[0].mOffset / part->mVertexStrides[0] + part->pDrawArgs[di].mVertexOffset;
                        gPerFrame[i].pMeshData[meshCount].materialID_flags =
                            ((flag & FLAG_MASK) << FLAG_LOW_BIT) | ((materialId & MATERIAL_ID_MASK) << MATERIAL_ID_LOW_BIT);
                        gPerFrame[i].pMeshData[meshCount].modelMtx = mat4::identity();
                        gPerFrame[i].pMeshData[meshCount].invModelMtx = mat4::identity();
                        gPerFrame[i].pMeshData[meshCount].prevModelMtx = mat4::identity();
                        gPerFrame[i].pMeshData[meshCount].preSkinnedVertexOffset = PRE_SKINNED_VERTEX_OFFSET_NONE;
                        gPerFrame[i].pMeshData[meshCount].indirectVertexOffset = vertexCount;
                    }

                    uint32_t geomSet = GEOMSET_OPAQUE;
                    if (flag & MATERIAL_FLAG_ALPHA_TESTED)
                        geomSet = GEOMSET_ALPHA_CUTOUT;
                    if (flag & MATERIAL_FLAG_TRANSPARENT)
                        geomSet = GEOMSET_ALPHA_BLEND;

                    pVBMeshInstances[meshCount].mGeometrySet = geomSet;
                    pVBMeshInstances[meshCount].mMeshIndex = meshCount;
                    pVBMeshInstances[meshCount].mTriangleCount = part->pDrawArgs[di].mIndexCount / 3;
                    pVBMeshInstances[meshCount].mInstanceIndex = instanceIdx;

                    ++meshCount;
                }
                // TODO: Reduce to an offset per mesh to align with VB_COMPUTE_THREADS
                // Requires to have vertex count information within submeshes "part->pDrawArgs[di]"
                vertexCount += part->mVertexCount;
            }
        }

        BufferLoadDesc meshConstantDesc = {};
        meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        meshConstantDesc.mDesc.mElementCount = gMaxMeshCount;
        meshConstantDesc.mDesc.mStructStride = sizeof(MeshData);
        meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
        meshConstantDesc.mDesc.pName = "Mesh Constant Desc";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            meshConstantDesc.pData = gPerFrame[i].pMeshData;
            meshConstantDesc.ppBuffer = &pMeshDataBuffer[i];
            addResource(&meshConstantDesc, NULL);
        }

        /************************************************************************/
        // InstanceData
        /************************************************************************/
        BufferLoadDesc jointMatrixDesc = {};
        jointMatrixDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        jointMatrixDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        jointMatrixDesc.mDesc.mElementCount = gMaxJointMatrixes;
        jointMatrixDesc.mDesc.mStructStride = sizeof(mat4);
        jointMatrixDesc.mDesc.mSize = jointMatrixDesc.mDesc.mElementCount * jointMatrixDesc.mDesc.mStructStride;
        jointMatrixDesc.mDesc.pName = "Joint Matrix Buffer";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            jointMatrixDesc.ppBuffer = &pJointMatrixBuffer[i];
            addResource(&jointMatrixDesc, NULL);
        }

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
        ubDesc.mDesc.pName = "Uniform Buffer PerFrame";

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

        BufferLoadDesc skyTriDataBufferDesc = {};
        skyTriDataBufferDesc.mDesc.mSize = sizeof(UniformDataSkyboxTri);
        skyTriDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        skyTriDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        skyTriDataBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        skyTriDataBufferDesc.pData = NULL;
        skyTriDataBufferDesc.mDesc.pName = "Sky Tri Uniforms";
        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            skyTriDataBufferDesc.ppBuffer = &pUniformBufferSkyTri[frameIdx];
            addResource(&skyTriDataBufferDesc, NULL);
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
        // Cleanup
        /************************************************************************/
        waitForAllResourceLoads();
    }

    void removeTriangleFilteringBuffers()
    {
        /************************************************************************/
        // Mesh constants and JointMatrix
        /************************************************************************/
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pMeshDataBuffer[i]);
            removeResource(pJointMatrixBuffer[i]);
        }

        /************************************************************************/
        // Per Frame Constant Buffers
        /************************************************************************/

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
            removeResource(pUniformBufferSkyTri[frameIdx]);
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
        vec3 lightSourcePos(5.0f, 000.0f, 90.0f);

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
        // Order independent transparency data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.transAlphaPerFlag[0] = gFlagAlphaRed;
        currentFrame->gPerFrameUniformData.transAlphaPerFlag[1] = gFlagAlphaGreen;
        currentFrame->gPerFrameUniformData.transAlphaPerFlag[2] = gFlagAlphaBlue;
        currentFrame->gPerFrameUniformData.transAlphaPerFlag[3] = gFlagAlphaPurple;
        currentFrame->gPerFrameUniformData.screenWidth = width;
        currentFrame->gPerFrameUniformData.screenHeight = height;
        /************************************************************************/
        // Lighting data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.camPos = v4ToF4(vec4(pCameraController->getViewPosition()));
        currentFrame->gPerFrameUniformData.nearPlane = gAppSettings.nearPlane;
        currentFrame->gPerFrameUniformData.farPlane = gAppSettings.farPlane;
        currentFrame->gPerFrameUniformData.lightDir = v4ToF4(vec4(lightDir));
        currentFrame->gPerFrameUniformData.twoOverRes = twoOverRes;
        currentFrame->gPerFrameUniformData.esmControl = gAppSettings.mEsmControl;
        currentFrame->gPerFrameUniformData.mGodRayScatterFactor = gGodRayConstant.mScatterFactor;
        currentFrame->gPerFrameUniformData.drawVRSDebug = gAppSettings.mDrawDebugTargets ? 1.0f : 0.0f;
        /************************************************************************/
        // Matrix data
        /************************************************************************/

        currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp = lightProj * lightView;
        currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp);
        currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].projection = lightProj;
        currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].mvp =
            currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp * lightModel;

        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp = cameraProj * cameraView;
        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp);
        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].projection = cameraProj;
        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].prevMVP = currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].mvp;
        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].mvp =
            currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp * cameraModel;
        currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].invMVP =
            CameraMatrix::inverse(currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].mvp);
        /************************************************************************/
        // Culling data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].sampleCount = 1;
        currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)gShadowMapSize, (float)gShadowMapSize };

        currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].sampleCount = gAppSettings.mMsaaLevel;
        currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

        // Cache eye position in object space for cluster culling on the CPU
        currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
        currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
            (inverse(cameraView.mCamera * cameraModel) * vec4(0, 0, 0, 1)).getXYZ(); // vec4(0,0,0,1) is the camera position in eye space
        /************************************************************************/
        // Shading data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.lightColor = gAppSettings.mLightColor;
        currentFrame->gPerFrameUniformData.outputMode = (uint32_t)gAppSettings.mOutputMode;
        /************************************************************************/
        // Skybox
        /************************************************************************/
        cameraView.setTranslation(vec3(0));
        currentFrame->gUniformDataSky.mCamPos = pCameraController->getViewPosition();
        currentFrame->gUniformDataSky.mProjectView = cameraProj.mCamera * cameraView.mCamera;
        currentFrame->gUniformDataSkyTri.mInverseViewProjection = inverse(cameraProj.mCamera * cameraView.mCamera);
        /************************************************************************/
        // Tonemap
        /************************************************************************/
        gTonemapInformation.outputMode = (uint)gAppSettings.mOutputMode;
        gTonemapInformation.linearScale = gAppSettings.LinearScale / 10000.0f;

        currentFrame->gPerFrameUniformData.mLinearScale = gTonemapInformation.linearScale;
        currentFrame->gPerFrameUniformData.mOutputMode = gTonemapInformation.outputMode;
        /************************************************************************/
        // MeshData
        /************************************************************************/
        for (uint32_t i = 0; i < gStaticMeshInstanceCount; ++i)
        {
            StaticMeshInstance* sm = &gStaticMeshInstances[i];

            MeshData* meshData = &currentFrame->pMeshData[i + gSceneMeshCount];
            meshData->prevModelMtx = meshData->modelMtx;
            meshData->modelMtx = mat4::translation(sm->mTranslation) * mat4::rotation(sm->mRotation) * mat4::scale(sm->mScale);
            meshData->invModelMtx = inverse(meshData->modelMtx);
        }

        /************************************************************************/
        // Animation Data
        /************************************************************************/

        // When we use Async Compute we need one PreSkinned buffer per frame, this is the offset to the first pre-skinned
        // vertex for the current frame we are rendering.
        // If we don't use Async Compute we don't need different offsets, just use one buffer and write to it every frame
        const uint32_t preSkinnedVertexStartOffsetInUnifiedBuffer =
            gAppSettings.mAsyncCompute ? gPreSkinnedVertexStartOffset + gPreSkinnedVertexCountPerFrame * frameIdx
                                       : gPreSkinnedVertexStartOffset;

        for (uint32_t i = 0; i < gMaxAnimatedInstances; ++i)
        {
            const AnimatedMeshInstance* am = &gAnimatedMeshInstances[i];

            {
                const mat4* src = am->pBoneMatrixes;
                mat4*       dst = currentFrame->gJointMatrixes + am->mJointMatrixOffset;
                // Cast to void* to suppress warning due to doing memcpy on a class that has an assignment operator
                memcpy((void*)dst, (void*)src, sizeof(mat4) * am->pGeomData->mJointCount);
            }

            // Mesh Data
            {
                const uint32_t meshDataIdx = gSceneMeshCount + gStaticMeshInstanceCount + i;

                MeshData* meshData = &currentFrame->pMeshData[meshDataIdx];
                meshData->prevModelMtx = meshData->modelMtx;
                meshData->modelMtx = mat4::translation(am->mTranslation) * mat4::rotation(am->mRotation) * mat4::scale(am->mScale);
                meshData->invModelMtx = inverse(meshData->modelMtx);
                meshData->preSkinnedVertexOffset = preSkinnedVertexStartOffsetInUnifiedBuffer + am->mPreSkinnedVertexOffset;
            }
        }
    }
    /************************************************************************/
    // UI
    /************************************************************************/
    void updateDynamicUIElements()
    {
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
                gPrevGraphicsSemaphore = NULL;
                memset(gComputeSemaphores, 0, sizeof(gComputeSemaphores));
            }
        }
    }
    /************************************************************************/
    // Rendering
    /************************************************************************/
    // Render the shadow mapping pass. This pass updates the shadow map texture
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
        const char* profileNames[NUM_GEOMETRY_SETS] = { "SM Opaque", "SM Alpha", "SM Transparent" };
        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
            cmdBindPipeline(cmd, pPipelineShadowPass[i]);
            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_SHADOW, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        }

        cmdBindRenderTargets(cmd, NULL);
    }

    void fillVRSMap(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        // Generate the shading rate image
        RenderTargetBarrier barriers[] = {
            { pHistoryRenderTarget[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);

        cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "VRS Filling Pass");
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pHistoryRenderTarget[frameIdx], LOAD_ACTION_LOAD };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_LOAD, LOAD_ACTION_LOAD };
        bindRenderTargets.mSampleLocation = { gLocations, 1, 1 };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDepthBuffer->mWidth, pDepthBuffer->mHeight);

        cmdBindPipeline(cmd, pPipelineFillStencil);

        cmdSetStencilReferenceValue(cmd, 1);
        cmdDraw(cmd, 3, 0);

        cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

        cmdBindRenderTargets(cmd, NULL);

        barriers[0] = { pHistoryRenderTarget[frameIdx], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
        barriers[1] = { pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },

        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);
    }

    // Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
    // into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
    // to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
    // less memory bandwidth is used.
    void drawVisibilityBufferPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch

        /************************************************************************/
        // Clear OIT Head Index Texture
        /************************************************************************/

        cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear OIT Head Index");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pDepthBufferOIT, LOAD_ACTION_DONTCARE, LOAD_ACTION_DONTCARE, STORE_ACTION_DONTCARE,
                                            STORE_ACTION_DONTCARE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth * gDivider, (float)pRenderTargetVBPass->mHeight * gDivider, 0.0f,
                       1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth * gDivider, pRenderTargetVBPass->mHeight * gDivider);

        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdBindPipeline(cmd, pPipelineClearHeadIndexOIT);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVisibilityPassTransparentPerBatch);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

        BufferBarrier bufferBarrier = { pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 1, &bufferBarrier, 0, NULL, 0, NULL);

        /************************************************************************/
        // Visibility Buffer Pass
        /************************************************************************/

        cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "VB Pass");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetVBPass, LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pDepthBuffer, LOAD_ACTION_CLEAR, LOAD_ACTION_CLEAR };
        if (gAppSettings.mEnableVRS)
        {
            bindRenderTargets.mSampleLocation = { gLocations, 1, 1 };
        }

        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[frameIdx * NUM_CULLING_VIEWPORTS + VIEW_CAMERA];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);
        const char* profileNames[NUM_GEOMETRY_SETS] = { "VB Opaque", "VB Alpha", "VB Transparent" };
        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
            if (i == GEOMSET_ALPHA_BLEND)
            {
                // Need set load action only to force barrier on some GPUs to make sure depth buffer writes are synchronized correctly
                bindRenderTargets = {};
                bindRenderTargets.mDepthStencil = { pDepthBufferOIT, LOAD_ACTION_LOAD };
                cmdBindRenderTargets(cmd, &bindRenderTargets);
                cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth * gDivider,
                               (float)pRenderTargetVBPass->mHeight * gDivider, 0.0f, 1.0f);
                cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth * gDivider, pRenderTargetVBPass->mHeight * gDivider);
            }

            cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVisibilityPassTransparentPerBatch);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectBuffer, indirectBufferByteOffset, NULL, 0);
            cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        }

        cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        cmdBindRenderTargets(cmd, NULL);

        BufferBarrier bufferBarriers[2] = {};
        bufferBarriers[0] = { pVisBufLinkedListBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
        bufferBarriers[1] = { pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd)
    {
        bool          isProgMSAAEnabled = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 && !gAppSettings.mEnableVRS;
        RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

        if (isProgMSAAEnabled)
        {
            ResourceState       prevState = gAppSettings.mEnableGodray ? RESOURCE_STATE_PIXEL_SHADER_RESOURCE : RESOURCE_STATE_DEPTH_WRITE;
            RenderTargetBarrier barriers[] = {
                { pRenderTargetMSAAEdges, prevState, RESOURCE_STATE_DEPTH_READ },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barriers), barriers);
        }

        if (gAppSettings.mEnableGodray && gAppSettings.mEnableVRS)
        {
            RenderTargetBarrier barriers[] = {
                { pDepthBuffer, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_READ },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barriers), barriers);
        }

        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pDestinationRenderTarget, LOAD_ACTION_CLEAR };

        if (isProgMSAAEnabled)
        {
            bindRenderTargets.mDepthStencil = { pRenderTargetMSAAEdges, LOAD_ACTION_DONTCARE, LOAD_ACTION_LOAD, STORE_ACTION_DONTCARE,
                                                STORE_ACTION_NONE };
        }
        else
        {
            bindRenderTargets.mDepthStencil = { gAppSettings.mEnableVRS ? pDepthBuffer : NULL, LOAD_ACTION_LOAD, LOAD_ACTION_LOAD };
        }
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[0]);

        if (gAppSettings.mEnableVRS)
        {
            cmdSetStencilReferenceValue(cmd, VRS_STENCIL_MASK);
        }
        else
        {
            cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        }
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        BufferBarrier bufferBarriers[2] = { { pVisBufLinkedListBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                                            { pHeadIndexBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS } };

        cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);
    }

    void resolveMSAA(Cmd* cmd, uint32_t frameIdx, uint32_t passIndex, RenderTarget* msaaRT, RenderTarget* destRT)
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
        cmdBindPipeline(cmd, passIndex == gResolveFinalPassIndex ? pPipelineResolve : pPipelineResolveGodRay);
        cmdBindDescriptorSet(cmd, frameIdx * gDataBufferCount + passIndex, pDescriptorSetResolve);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);
    }

    // Executes a compute shader to clear (reset) the the light clusters on the GPU
    void clearLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClearLightClusters);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLightsPerBatch);
        cmdDispatch(cmd, 1, 1, 1);
    }

    // Executes a compute shader that computes the light clusters on the GPU
    void computeLightClusters(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBindPipeline(cmd, pPipelineClusterLights);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClusterLightsPerBatch);
        cmdDispatch(cmd, LIGHT_COUNT, 1, 1);
    }

    // This is the main scene rendering function. It shows the different steps / rendering passes.
    void drawScene(Cmd* cmd, uint32_t frameIdx)
    {
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Shadow Pass");

        RenderTargetBarrier rtBarriers[] = {
            { pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            { pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

        drawShadowMapPass(cmd, gGraphicsProfileToken, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
        BufferBarrier oitGeomCountBarrier = { pGeometryCountBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 1, &oitGeomCountBarrier, 0, NULL, 0, NULL);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Filling Pass");
        drawVisibilityBufferPass(cmd, gGraphicsProfileToken, frameIdx);
        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

        if (gAppSettings.mEnableVRS)
        {
            fillVRSMap(cmd, gGraphicsProfileToken, frameIdx);
        }

        RenderTargetBarrier barriers[] = {
            { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            { pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            { pDepthBufferOIT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, barriers);

        const bool isRunningProgMsaa = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 && !gAppSettings.mEnableVRS;

        if (isRunningProgMsaa)
        {
            drawMSAAEdgesStencil(cmd);
        }

        // Draw GodRay after Initial VB Pass
        // Accumulate in shade pass...
        if (gAppSettings.mEnableGodray)
        {
            if (isRunningProgMsaa)
            {
                downscaleMSAAEdgesStencil(cmd);
            }
            drawGodray(cmd);
            if (isRunningProgMsaa)
            {
                cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Resolve GodRay Pass");
                resolveMSAA(cmd, frameIdx, gResolveGodRayPassIndex, pRenderTargetGodRayMS, pRenderTargetGodRay[0]);
                cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
            }

            blurGodRay(cmd, frameIdx);
        }

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Shading Pass");

        drawVisibilityBufferShade(cmd);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawGodray(Cmd* cmd)
    {
        bool          isProgMsaa = gAppSettings.mMsaaLevel > SAMPLE_COUNT_1 && !gAppSettings.mEnableVRS;
        RenderTarget* pGodRayActiveRenderTarget = pRenderTargetGodRay[0];
        if (isProgMsaa)
        {
            pGodRayActiveRenderTarget = pRenderTargetGodRayMS;
            RenderTargetBarrier barrier[] = {
                { pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetGodRayMS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pRenderTargetMSAAEdgesDownscaled, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_DEPTH_READ },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barrier), barrier);
        }
        else
        {
            RenderTargetBarrier barrier[] = {
                { pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barrier), barrier);
        }

        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pGodRayActiveRenderTarget, LOAD_ACTION_CLEAR };
        if (isProgMsaa)
        {
            bindRenderTargets.mDepthStencil = { pRenderTargetMSAAEdgesDownscaled, LOAD_ACTION_DONTCARE, LOAD_ACTION_LOAD,
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

    void blurGodRay(Cmd* cmd, uint frameIdx)
    {
        UNREF_PARAM(frameIdx);
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God Ray Blur");

        BufferUpdateDesc bufferUpdate = { pBufferBlurWeights };
        beginUpdateResource(&bufferUpdate);
        memcpy(bufferUpdate.pMappedData, &gBlurWeightsUniform, sizeof(gBlurWeightsUniform));
        endUpdateResource(&bufferUpdate);

        cmdBindRenderTargets(cmd, NULL);

        RenderTargetBarrier renderTargetBarrier[] = {
            { pRenderTargetGodRay[0], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS },
            { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(renderTargetBarrier), renderTargetBarrier);

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
        cmdBindDescriptorSet(cmd, frameIdx * 2, pDescriptorSetGodRayBlurPassPerBatch);
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
        cmdBindDescriptorSet(cmd, frameIdx * 2 + 1, pDescriptorSetGodRayBlurPassPerBatch);
        cmdDispatch(cmd, threadGroupSizeX, threadGroupSizeY, 1);

        renderTargetBarrier[0] = { pRenderTargetGodRay[0], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        renderTargetBarrier[1] = { pRenderTargetGodRay[1], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, renderTargetBarrier);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void drawMSAAEdgesStencil(Cmd* cmd)
    {
        // This depth-only pass will render to the stencil buffer. Samples that must be shaded in
        // future shading or post-processing passes will be set to 0x01.
        cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Edges Stencil Pass");

        {
            RenderTargetBarrier barriers[] = {
                { pRenderTargetMSAAEdges, RESOURCE_STATE_DEPTH_READ, RESOURCE_STATE_DEPTH_WRITE },
                { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, TF_ARRAY_COUNT(barriers), barriers);
        }

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pRenderTargetMSAAEdges, LOAD_ACTION_DONTCARE, LOAD_ACTION_CLEAR, STORE_ACTION_NONE,
                                            STORE_ACTION_STORE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetMSAAEdges->mWidth, (float)pRenderTargetMSAAEdges->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetMSAAEdges->mWidth, pRenderTargetMSAAEdges->mHeight);

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
                                               pRenderTargetMSAAEdges,
                                               RESOURCE_STATE_DEPTH_WRITE,
                                               RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                           },
                                           { pRenderTargetMSAAEdgesDownscaled, RESOURCE_STATE_DEPTH_READ, RESOURCE_STATE_DEPTH_WRITE } };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pRenderTargetMSAAEdgesDownscaled, LOAD_ACTION_DONTCARE, LOAD_ACTION_CLEAR,
                                            STORE_ACTION_DONTCARE, STORE_ACTION_STORE };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetMSAAEdgesDownscaled->mWidth, (float)pRenderTargetMSAAEdgesDownscaled->mHeight,
                       0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetMSAAEdgesDownscaled->mWidth, pRenderTargetMSAAEdgesDownscaled->mHeight);

        cmdBindPipeline(cmd, pPipelineDownscaleMSAAEdges);
        cmdSetStencilReferenceValue(cmd, MSAA_STENCIL_MASK);
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);

        cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
    }

    void presentImage(Cmd* const cmd, RenderTarget* pSrc, RenderTarget* pDstCol, uint frameIdx)
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
        cmdBindDescriptorSet(cmd, (gAppSettings.mEnableVRS ? (2 - frameIdx) : 0), pDescriptorSetDisplayPerDraw);
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

        RenderTarget*         rt = pSwapChain->ppRenderTargets[frameIdx];
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { rt, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(cmd, &bindRenderTargets);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)rt->mWidth, (float)rt->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, rt->mWidth, rt->mHeight);

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        if (gAppSettings.mAsyncCompute)
        {
            if (!gAppSettings.mHoldFilteredResults || gAppSettings.mUpdateSimulation)
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

    static void recreateGeometryBufferPlots(void* data)
    {
        uint32_t plotCount = *(uint32_t*)data;

        size_t maxSize = pGeometryBuffer->mIndex.mSize;
        for (uint32_t i = 0; i < plotCount; ++i)
            if (pGeometryBuffer->mVertex[i].mSize > maxSize)
                maxSize = pGeometryBuffer->mVertex[i].mSize;

        const float2 defaultSize(500.f, 40.f);
        float2       size = defaultSize;

        if (gScaleGeometryPlots)
            size[0] = ((float)pGeometryBuffer->mIndex.mSize * defaultSize.x) / (float)maxSize;

        uiSetWidgetAllocatorPlotBufferChunkAllocatorData(pIndicesPlot, size, &pGeometryBuffer->mIndex);

        for (uint32_t i = 0; i < plotCount; ++i)
        {
            size = defaultSize;
            if (gScaleGeometryPlots)
                size[0] = ((float)pGeometryBuffer->mVertex[i].mSize * defaultSize.x) / (float)maxSize;

            uiSetWidgetAllocatorPlotBufferChunkAllocatorData(pVerticesPlot[i], size, &pGeometryBuffer->mVertex[i]);
        }
    }
};

DEFINE_APPLICATION_MAIN(VisibilityBufferOIT)
