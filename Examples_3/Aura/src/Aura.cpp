/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../../Custom-Middleware/Aura/LightPropagation/LightPropagationVolume.h"

#include "../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../Common_3/Utilities/RingBuffer.h"
#include "../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "Geometry.h"
#include "Shadows.hpp"
#include "Camera.hpp"

#if defined(XBOX)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#include "../../../Xbox/Common_3/Graphics/IESRAMManager.h"
#endif

#include "../../../Common_3/Utilities/Interfaces/IMemory.h"

#define NO_FSL_DEFINITIONS
#include "Shaders/FSL/wind.h.fsl"

ThreadSystem* pThreadSystem;

#define SCENE_SCALE 50.0f
#define CLOTH_SCALE 0.6f

#define TEST_RSM 0

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
	float4 CameraPlane;    //x : near, y : far
};

//Camera Walking
static float          gCameraWalkingTime = 0.0f;
eastl::vector<float3> gPositionsDirections;
float3                gCameraPathData[29084];

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

	// This variable enables or disables triangle filtering. When filtering is disabled, all the scene is rendered unconditionally.
	bool mFilterTriangles = true;
	// Turns off cluster culling by default
	// Cluster culling increases CPU time and does not provide enough benefit in terms of culling results to keep it enabled by default
	bool mClusterCulling = false;
	bool mAsyncCompute = true;
	// toggle rendering of local point lights
	bool mRenderLocalLights = false;

	bool mDrawDebugTargets = false;
	bool mDrawShadowTargets = false;
	#if TEST_RSM
	bool mDrawRSMTargets = false;
	#endif

	float nearPlane = 10.0f;
	float farPlane = 3000.0f;

	float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

	DynamicUIWidgets  mDynamicUIWidgetsGR;

	float mRetinaScaling = 1.5f;

	DynamicUIWidgets mLinearScale;
	float             LinearScale = 140.0f;

	// HDR10
	DynamicUIWidgets mDisplaySetting;

	//Camera Walking
	bool  cameraWalking = false;
	float cameraWalkingSpeed = 1.0f;

	// Aura settings
	bool  useCPUPropagation = false;
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
const char* gSceneName = "SanMiguel.gltf";

// Number of in-flight buffers
const uint32_t gImageCount = 3;

// Constants
const uint32_t gRSMResolution = 256;
const uint32_t gShadowMapSize = 1024;

const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;
const uint32_t gNumStages = 3;

struct UniformDataSkybox
{
	mat4 mProjectView;
	vec3 mCamPos;
};

const uint32_t    gSkyboxSize = 1024;
const uint32_t    gSkyboxMips = 9;

struct UniformDataSunMatrices
{
	mat4 mProjectView;
	mat4 mModelMat;
	vec4 mLightColor;
};

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = 2;
const uint32_t GEOMSET_OPAQUE = 0;
const uint32_t GEOMSET_ALPHATESTED = 1;
const char* gProfileNames[gNumGeomSets] = {"Opaque", "Alpha"};
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
	vec3              gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
	PerFrameConstants gPerFrameUniformData = {};
	UniformDataSkybox gUniformDataSky;
	mat4              gCameraView;
	mat4              gCameraModelView;

	// These are just used for statistical information
	uint32_t gTotalClusters = 0;
	uint32_t gCulledClusters = 0;
	uint32_t gDrawCount[gNumGeomSets] = {};
	uint32_t gTotalDrawCount = 0;
	
	mat4 gRSMCascadeProjection[gRSMCascadeCount] = {};
	mat4 gRSMCascadeView[gRSMCascadeCount] = {};
	float gRSMViewSize[gRSMCascadeCount] = {};
};

typedef struct DebugVisualizationSettings {
	bool isEnabled = false;
	int cascadeIndex = 0;
	float probeRadius = 2.0f;
} DebugVisualizationSettings;

/************************************************************************/
// Settings
/************************************************************************/
AppSettings gAppSettings;
DebugVisualizationSettings gDebugVisualizationSettings;

/************************************************************************/
// Rendering data
/************************************************************************/
Renderer* pRenderer = NULL;
/************************************************************************/
// Synchronization primitives
/************************************************************************/
Fence*     pTransitionFences = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Fence*     pComputeCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
Semaphore* pComputeCompleteSemaphores[gImageCount] = { NULL };
/************************************************************************/
// Queues and Command buffers
/************************************************************************/
Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount];
Cmd*     pCmds[gImageCount];

Queue*   pComputeQueue = NULL;
CmdPool* pComputeCmdPools[gImageCount];
Cmd*     pComputeCmds[gImageCount];
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain* pSwapChain = NULL;
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*           pShaderClearBuffers = nullptr;
Pipeline*         pPipelineClearBuffers = nullptr;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*           pShaderTriangleFiltering = nullptr;
Pipeline*         pPipelineTriangleFiltering = nullptr;
RootSignature*    pRootSignatureTriangleFiltering = nullptr;
DescriptorSet*    pDescriptorSetTriangleFiltering[2] = { NULL };
/************************************************************************/
// Batch compaction pipeline
/************************************************************************/
Shader*           pShaderBatchCompaction = nullptr;
Pipeline*         pPipelineBatchCompaction = nullptr;
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
const int		  gVbShadeConfigCount = 4;
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

Buffer*           pSkyboxVertexBuffer = NULL;
Texture*          pSkybox = NULL;
DescriptorSet*    pDescriptorSetSkybox[2] = { NULL };

/************************************************************************/
// RSM pipeline
/************************************************************************/
Shader* pShaderFillRSM[gNumGeomSets] = { nullptr };
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
Buffer* pMaterialPropertyBuffer = NULL;
Buffer* pPerFrameUniformBuffers[gImageCount] = { NULL };

// Buffers containing all indirect draw commands per geometry set (no culling)
Buffer* pIndirectDrawArgumentsBufferAll[gNumGeomSets] = { NULL };
Buffer* pIndirectMaterialBufferAll = NULL;
Buffer* pMeshConstantsBuffer = NULL;
// Buffers containing filtered indirect draw commands per geometry set (culled)
Buffer* pFilteredIndirectDrawArgumentsBuffer[gImageCount][gNumGeomSets][gNumViews] = { { { NULL } } };
// Buffer containing the draw args after triangle culling which will be stored compactly in the indirect buffer
Buffer* pUncompactedDrawArgumentsBuffer[gImageCount][gNumViews] = { { NULL } };
Buffer* pFilterIndirectMaterialBuffer[gImageCount] = { NULL };
/************************************************************************/
// Index buffers
/************************************************************************/
Buffer* pFilteredIndexBuffer[gImageCount][gNumViews] = {};
/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*       pLightsBuffer = NULL;
Buffer**      gPerBatchUniformBuffers = NULL;
Buffer*       pVertexBufferCube = NULL;
Buffer*       pIndexBufferCube = NULL;
Buffer*       pLightClustersCount[gImageCount] = { NULL };
Buffer*       pLightClusters[gImageCount] = { NULL };
Buffer*       pUniformBufferSky[gImageCount] = { NULL };
Buffer*		  pUniformBufferAuraLightApply[gImageCount] = { NULL };
uint64_t      gFrameCount = 0;
ClusterContainer* pMeshes = NULL;
uint32_t      gMeshCount = 0;
uint32_t      gMaterialCount = 0;
FontDrawDesc  gFrameTimeDraw;
uint32_t      gFontID = 0; 

UIComponent* pGuiWindow = NULL;
UIComponent* pDebugTexturesWindow = NULL;
UIComponent* pAuraDebugTexturesWindow = nullptr;
UIComponent* pAuraDebugVisualizationGuiWindow = nullptr;
UIComponent* pShadowTexturesWindow = nullptr;
UIComponent* pAuraGuiWindow = nullptr;
#if TEST_RSM
UIComponent* pAuraRSMTexturesWindow = nullptr;
#endif
/************************************************************************/
// Triangle filtering data
/************************************************************************/
const uint32_t gSmallBatchChunkCount = max(1U, 512U / CLUSTER_SIZE) * 16U;
FilterBatchChunk* pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = { { NULL } };
GPURingBuffer* pFilterBatchDataBuffer = { NULL };
/************************************************************************/
// GPU Profilers
/************************************************************************/
ProfileToken gGpuProfileTokens[2];
/************************************************************************/
// CPU staging data
/************************************************************************/

// CPU buffers for light data
LightData gLightData[LIGHT_COUNT] = {};

Camera gCamera;
Camera gDebugCamera;
Camera* gActiveCamera = &gCamera;

typedef struct DirectionalLight {
	float2 mRotationXY;
} DirectionalLight;

DirectionalLight gDirectionalLight;

PerFrameData  gPerFrame[gImageCount] = {};
RenderTarget* pScreenRenderTarget = NULL;
/************************************************************************/
// Screen resolution UI data
/************************************************************************/
#if defined(_WINDOWS)
struct ResolutionData
{
	// Buffer for all res name strings
	char*			mResNameContainer;
	// Array of const char*
	const char**	mResNamePointers;
};

static ResolutionData gGuiResolution = { NULL, NULL };
#endif
/************************************************************************/
/************************************************************************/
aura::Aura*         pAura = nullptr;
aura::ITaskManager* pTaskManager = nullptr;

Timer gAccumTimer;

const char* pPipelineCacheName = "PipelineCache.cache";
PipelineCache* pPipelineCache = NULL;

class AuraApp* pAuraApp = NULL;
/************************************************************************/
// Culling intrinsic data
/************************************************************************/
const uint32_t pdep_lut[8] = { 0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15 };

/************************************************************************/
// App implementation
/************************************************************************/
UIWidget* addResolutionProperty(
	UIComponent* pUIManager, uint32_t& resolutionIndex, uint32_t resCount, Resolution* pResolutions, WidgetCallback onResolutionChanged)
{
#if defined(_WINDOWS)
	if (pUIManager)
	{
		ResolutionData& data = gGuiResolution;

		static const uint32_t maxResNameLength = 16;
		arrsetlen(data.mResNameContainer, 0);
		arrsetcap(data.mResNameContainer, maxResNameLength* resCount);
		arrsetlen(data.mResNamePointers, resCount);
		
		char* pBuf = data.mResNameContainer;
		int remainingLen = (int)arrcap(data.mResNameContainer);

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

#endif
	return NULL;
}

class AuraApp : public IApp
{
public:
	bool Init()
	{
		initTimer(&gAccumTimer);

		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES,  "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG,   RD_PIPELINE_CACHE,  "PipelineCaches");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG,      "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES,        "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS,           "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES,          "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS,          "Scripts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES,     "");

		initThreadSystem(&pThreadSystem);

		pAuraApp = this;

		/************************************************************************/
		// Initialize the Forge renderer with the appropriate parameters.
		/************************************************************************/
		
		RendererDesc settings = {};
#ifdef VULKAN
		const char* rtIndexVSExtension = VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME;
		settings.mVulkan.mDeviceExtensionCount = 1;
		settings.mVulkan.ppDeviceExtensions = &rtIndexVSExtension;
#endif
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create the command pool and the command lists used to store GPU commands.
		// One Cmd list per back buffer image is stored for triple buffering.
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);
		}

		QueueDesc computeQueueDesc = {};
		computeQueueDesc.mType = QUEUE_TYPE_COMPUTE;
		computeQueueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

		// Create the command pool and the command lists used to store GPU commands.
		// One Cmd list per back buffer image is stored for triple buffering.
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pComputeQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pComputeCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pComputeCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pComputeCmds[i]);
		}

		addFence(pRenderer, &pTransitionFences);

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addFence(pRenderer, &pComputeCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
			addSemaphore(pRenderer, &pComputeCompleteSemaphores[i]);
		}

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

		Queue* ppQueues[2] = { pGraphicsQueue, pComputeQueue };
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
		aura::CPUPropagationParams cpuParams;
		fillAuraParams(&params, &cpuParams);

		initAura(pRenderer, 1024, 1024, params, gImageCount, gRSMCascadeCount, &pAura);


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
		gAppSettings.useCPUPropagationDecoupled = false;
		gAppSettings.useMultipleReflections = false;
		gAppSettings.alternateGPUUpdates = false;
		gAppSettings.propagationScale = 1.0f;
		gAppSettings.giStrength = 12.0f;
		gAppSettings.propagationValueIndex = 5;
		gAppSettings.specularQuality = 2;
		gAppSettings.useLPV = true;
		gAppSettings.enableSun = false;
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
		SamplerDesc trilinearDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, false, 0.0f, 0.0f, 8.0f
		};
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

		SamplerDesc auraLinearBorderDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR,
			ADDRESS_MODE_CLAMP_TO_BORDER, ADDRESS_MODE_CLAMP_TO_BORDER, ADDRESS_MODE_CLAMP_TO_BORDER
		};

		addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
		addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
		addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
		addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);
		addSampler(pRenderer, &auraLinearBorderDesc, &pSamplerLinearBorder);

		/************************************************************************/
		// Load resources for skybox
		/************************************************************************/
		addThreadSystemTask(pThreadSystem, memberTaskFunc0<AuraApp, &AuraApp::LoadSkybox>, this);
		/************************************************************************/
		// Load the scene using the SceneLoader class, which uses Assimp
		/************************************************************************/
		HiresTimer      sceneLoadTimer;
		initHiresTimer(&sceneLoadTimer);

		Scene* pScene = loadScene(gSceneName, 50.0f, -20.0f, 0.0f, 0.0f);
		if (!pScene)
			return false;
		LOGF(LogLevel::eINFO, "Load scene : %f ms", getHiresTimerUSec(&sceneLoadTimer, true) / 1000.0f);

		gMeshCount = pScene->geom->mDrawArgCount;
		gMaterialCount = pScene->geom->mDrawArgCount;
		pMeshes = (ClusterContainer*)tf_malloc(gMeshCount * sizeof(ClusterContainer));
		pGeom = pScene->geom;
		/************************************************************************/
		// Texture loading
		/************************************************************************/
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
			addResource(&desc, NULL);
			desc.mCreationFlag = TEXTURE_CREATION_FLAG_NONE;
			desc.pFileName = pScene->normalMaps[i];
			desc.ppTexture = &gNormalMapsStorage[i];
			addResource(&desc, NULL);
			desc.pFileName = pScene->specularMaps[i];
			desc.ppTexture = &gSpecularMapsStorage[i];
			addResource(&desc, NULL);
		}
		/************************************************************************/
		// Cluster creation
		/************************************************************************/
		HiresTimer clusterTimer;
		initHiresTimer(&clusterTimer);

		// Calculate clusters
		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			ClusterContainer*   mesh = pMeshes + i;
			Material* material = pScene->materials + i;
			createClusters(material->twoSided, pScene, pScene->geom->pDrawArgs + i, mesh);
		}
		removeGeometryShadowData(pScene->geom);
		LOGF(LogLevel::eINFO, "Load clusters : %f ms", getHiresTimerUSec(&clusterTimer, true) / 1000.0f);

		// Create geometry for light rendering
		createCubeBuffers(pRenderer, pCmdPools[0], &pVertexBufferCube, &pIndexBufferCube);

		//Generate sky box vertex buffer
		static const float skyBoxPoints[] =
		{
			0.5f,  -0.5f, -0.5f, 1.0f,    // -z
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    //-x
			-0.5f, -0.5f, -0.5f, 1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,  -0.5f, 0.5f,
			-0.5f, 1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			0.5f,  -0.5f, -0.5f, 1.0f,    //+x
			0.5f,  -0.5f, 0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  -0.5f, -0.5f, 1.0f,

			-0.5f, -0.5f, 0.5f,  1.0f,    // +z
			-0.5f, 0.5f,  0.5f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  -0.5f, -0.5f, 0.5f,  1.0f,

			-0.5f, 0.5f,  -0.5f, 1.0f,    //+y
			0.5f,  0.5f,  -0.5f, 1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.5f,  0.5f,
			0.5f,  1.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  -0.5f, 0.5f,  -0.5f, 1.0f,

			0.5f,  -0.5f, 0.5f,  1.0f,    //-y
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

		UIComponentDesc UIComponentDesc = {};
		UIComponentDesc.mStartPosition = vec2(225.0f, 100.0f);
		uiCreateComponent(GetName(), &UIComponentDesc, &pGuiWindow);
		uiSetComponentFlags(pGuiWindow, GUI_COMPONENT_FLAGS_NO_RESIZE);
		/************************************************************************/
		// Most important options
		/************************************************************************/
		CheckboxWidget holdProp;
		holdProp.pData = &gAppSettings.mHoldFilteredResults;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Hold filtered results", &holdProp, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget filtering;
		filtering.pData = &gAppSettings.mFilterTriangles;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Triangle Filtering", &filtering, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget cluster;
		cluster.pData = &gAppSettings.mClusterCulling;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cluster Culling", &cluster, WIDGET_TYPE_CHECKBOX));

		CheckboxWidget asyncCompute;
		asyncCompute.pData = &gAppSettings.mAsyncCompute;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Async Compute", &asyncCompute, WIDGET_TYPE_CHECKBOX));

		// Light Settings
		//---------------------------------------------------------------------------------
		// offset max angle for sun control so the light won't bleed with
		// small glancing angles, i.e., when lightDir is almost parallel to the plane

		CheckboxWidget localLight;
		localLight.pData = &gAppSettings.mRenderLocalLights;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Random Point Lights", &localLight, WIDGET_TYPE_CHECKBOX));

		/************************************************************************/
		/************************************************************************/
		// Finish the resource loading process since the next code depends on the loaded resources
		waitThreadSystemIdle(pThreadSystem);
		waitForAllResourceLoads();

		HiresTimer setupBuffersTimer;
		initHiresTimer(&setupBuffersTimer);
		addTriangleFilteringBuffers(pScene);

		LOGF(LogLevel::eINFO, "Setup buffers : %f ms", getHiresTimerUSec(&setupBuffersTimer, true) / 1000.0f);

		BufferLoadDesc auraUbDesc = {};
		auraUbDesc.mDesc.mDescriptors = (::DescriptorType)(DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auraUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		auraUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		auraUbDesc.mDesc.mSize = sizeof(LightApplyData);
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			auraUbDesc.ppBuffer = &pUniformBufferAuraLightApply[frameIdx];
			addResource(&auraUbDesc, NULL);
		}

		LOGF(LogLevel::eINFO, "Total Load Time : %f ms", getHiresTimerUSec(&totalTimer, true) / 1000.0f);

		removeScene(pScene);
		
		// Camera Walking
		FileStream fh = {};
		if (fsOpenStreamFromPath(RD_OTHER_FILES, "cameraPath.bin", FM_READ_BINARY, NULL, &fh))
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
				index ? gActiveCamera->pCameraController->onRotate(delta) : gActiveCamera->pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) gActiveCamera->pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		gFrameCount = 0;

		return true;
	}

	void Exit()
	{
		// Remove default fences, semaphores
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
			removeSemaphore(pRenderer, pComputeCompleteSemaphores[i]);
		}

		removeResource(pSkybox);
		
		removeTriangleFilteringBuffers();

		uiDestroyDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
		uiDestroyDynamicWidgets(&gAppSettings.mLinearScale);
		uiDestroyDynamicWidgets(&gAppSettings.mDisplaySetting);

		exitProfiler();

		exitUserInterface();

		exitFontSystem(); 

		pDebugTexturesWindow = NULL;
		pAuraDebugTexturesWindow = nullptr;
		pAuraDebugVisualizationGuiWindow = nullptr;
		pShadowTexturesWindow = nullptr;
		pAuraGuiWindow = nullptr;
#if TEST_RSM
		pAuraRSMTexturesWindow = nullptr;
#endif

		// Destroy geometry for light rendering
		removeResource(pVertexBufferCube);
		removeResource(pIndexBufferCube);

		/************************************************************************/
		// Remove loaded scene
		/************************************************************************/
		// Destroy scene buffers
		removeResource(pGeom);

		removeResource(pSkyboxVertexBuffer);

		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			removeResource(pUniformBufferAuraLightApply[frameIdx]);
		}

		// Destroy clusters
		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			destroyClusters(&pMeshes[i]);
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
		tf_free(pMeshes);

		gPositionsDirections.set_capacity(0);
#if defined(_WINDOWS)
		arrfree(gGuiResolution.mResNameContainer);
		arrfree(gGuiResolution.mResNamePointers);
#endif
		/************************************************************************/
		/************************************************************************/

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeFence(pRenderer, pTransitionFences);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeQueue(pRenderer, pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pComputeCmds[i]);
			removeCmdPool(pRenderer, pComputeCmdPools[i]);
		}
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

		exitAura(pRenderer, pTaskManager, pAura);

		exitResourceLoaderInterface(pRenderer);

		exitRenderer(pRenderer);
		pRenderer = NULL; 

		exitInputSystem();
		exitThreadSystem(pThreadSystem);

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
				if (pDebugTexturesWindow)
				{
					uiDestroyComponent(pDebugTexturesWindow);
					pDebugTexturesWindow = NULL;
				}
#if defined(XBOX)
				esramResetAllocations(pRenderer->mD3D12.pESRAMManager);
#endif
			}
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

#if !defined(TARGET_IOS)
		gActiveCamera->pCameraController->update(deltaTime);
#endif

		//Camera Walking Update

		if (gAppSettings.cameraWalking)
		{
			if (gTotalElpasedTime - (0.033333f * gAppSettings.cameraWalkingSpeed) <= gCameraWalkingTime)
			{
				gCameraWalkingTime = 0.0f;
			}

			gCameraWalkingTime += deltaTime * gAppSettings.cameraWalkingSpeed;

			uint  currentCameraFrame = (uint)(gCameraWalkingTime / 0.00833f);
			float remind = gCameraWalkingTime - (float)currentCameraFrame * 0.00833f;

			float3 newPos =
				v3ToF3(lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame]), f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1)]), remind));
			gActiveCamera->pCameraController->moveTo(f3Tov3(newPos));

			float3 newLookat = v3ToF3(
				lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]), f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
			gActiveCamera->pCameraController->lookAt(f3Tov3(newLookat));
		}

		updateDynamicUIElements();
		updateLPVParams();

		updateUniformData(gFrameCount % gImageCount);
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t presentIndex = 0;
		uint32_t frameIdx = gFrameCount % gImageCount;

		if (!gAppSettings.mAsyncCompute)
		{
			// check to see if we can use the cmd buffer
			Fence*      pRenderFence = pRenderCompleteFences[frameIdx];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
			if (gAppSettings.useCPUPropagation || fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pRenderFence);

			resetCmdPool(pRenderer, pCmdPools[frameIdx]);
		}
		else
		{
			// check to see if we can use the cmd buffer
			Fence*      pComputeFence = pComputeCompleteFences[frameIdx];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pComputeFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pComputeFence);

			resetCmdPool(pRenderer, pComputeCmdPools[frameIdx]);

			// check to see if we can use the cmd buffer
			Fence*      pRenderFence = pRenderCompleteFences[frameIdx];
			//FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pRenderFence);

			resetCmdPool(pRenderer, pCmdPools[frameIdx]);
		}
		/************************************************************************/
		// Update uniform buffer to gpu
		/************************************************************************/
		BufferUpdateDesc update = { pPerFrameUniformBuffers[frameIdx] };
		beginUpdateResource(&update);
		*(PerFrameConstants*)update.pMappedData = gPerFrame[frameIdx].gPerFrameUniformData;
		endUpdateResource(&update, NULL);
		
		// Update uniform buffers
		update = { pUniformBufferSky[frameIdx] };
		beginUpdateResource(&update);
		*(UniformDataSkybox*)update.pMappedData = gPerFrame[frameIdx].gUniformDataSky;
		endUpdateResource(&update, NULL);
		/************************************************************************/
		// Async compute pass
		/************************************************************************/
		if (gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
		{
			/************************************************************************/
			// Triangle filtering async compute pass
			/************************************************************************/
			Cmd* computeCmd = pComputeCmds[frameIdx];

			beginCmd(computeCmd);

			cmdBeginGpuFrameProfile(computeCmd, gGpuProfileTokens[1], true);

			triangleFilteringPass(computeCmd, gGpuProfileTokens[1], frameIdx);

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
				BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS } };
				cmdResourceBarrier(computeCmd, 1, barriers, 0, NULL, 0, NULL);
				computeLightClusters(computeCmd, frameIdx);
				cmdEndGpuTimestampQuery(computeCmd, gGpuProfileTokens[1]);
			}

			cmdEndGpuFrameProfile(computeCmd, gGpuProfileTokens[1]);

			endCmd(computeCmd);
			QueueSubmitDesc submitDesc = {};
			submitDesc.mCmdCount = 1;
			submitDesc.mSignalSemaphoreCount = 1;
			submitDesc.ppCmds = &computeCmd;
			submitDesc.ppSignalSemaphores = &pComputeCompleteSemaphores[frameIdx];
			submitDesc.pSignalFence = pComputeCompleteFences[frameIdx];
			submitDesc.mSubmitDone = (gFrameCount < 1);
			queueSubmit(pComputeQueue, &submitDesc);
			/************************************************************************/
			/************************************************************************/
		}

		/************************************************************************/
		// Draw Pass
		/************************************************************************/
		if (gAppSettings.useLPV || gDebugVisualizationSettings.isEnabled)
		{
			const float4& baseCamPos = gPerFrame[gFrameCount % gImageCount].gPerFrameUniformData.camPos;
			const aura::vec3 camPos(
				baseCamPos.getX(),
				baseCamPos.getY(),
				baseCamPos.getZ());
			const vec4& baseCamDir = gPerFrame[gFrameCount % gImageCount].gCameraModelView.getRow(2);
			const aura::vec3 camDir(
				baseCamDir.getX(),
				baseCamDir.getY(),
				baseCamDir.getZ());
			aura::beginFrame(pRenderer, pAura, camPos, camDir);
		}
		/************************************************************************/
		// Draw Pass - Skip first frame since draw will always be one frame behind compute
		/************************************************************************/
		if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
		{
			Cmd* graphicsCmd = NULL;

			if (gAppSettings.mAsyncCompute)
				frameIdx = ((gFrameCount - 1) % gImageCount);

			acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
			pScreenRenderTarget = pSwapChain->ppRenderTargets[presentIndex];

			// Get command list to store rendering commands for this frame
			graphicsCmd = pCmds[frameIdx];
			// Submit all render commands for this frame
			beginCmd(graphicsCmd);

			cmdBeginGpuFrameProfile(graphicsCmd, gGpuProfileTokens[0]);

			if (!gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
			{
				triangleFilteringPass(graphicsCmd, gGpuProfileTokens[0], frameIdx);
			}

			if (!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles)
			{
				cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "Clear Light Clusters");
				clearLightClusters(graphicsCmd, frameIdx);
				cmdEndGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0]);
			}

			if ((!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles) && gAppSettings.mRenderLocalLights)
			{
				// Update Light clusters on the GPU

				cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "Compute Light Clusters");
				BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS } };
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

			const uint32_t maxNumBarriers = (gNumGeomSets * gNumViews) + gNumViews + 3;
			uint32_t barrierCount = 0;
			BufferBarrier barriers2[maxNumBarriers] = {};
			barriers2[barrierCount++] = { pLightClusters[frameIdx],      RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

			if (gAppSettings.mFilterTriangles)
			{
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[barrierCount++] = { pFilteredIndexBuffer[frameIdx][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
				}
				barriers2[barrierCount++] = { pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			}

			cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 2, rtBarriers);

			drawScene(graphicsCmd, frameIdx);
			drawSkybox(graphicsCmd, frameIdx);

			barrierCount = 0;
			barriers2[barrierCount++] = { pLightClusters[frameIdx],      RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };

			if (gAppSettings.mFilterTriangles)
			{
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[barrierCount++] = { pFilteredIndexBuffer[frameIdx][i],
						RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
				}
				barriers2[barrierCount++] = { pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			}

			cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 0, NULL);

			cmdBeginGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0], "UI Pass");
			drawGUI(graphicsCmd, frameIdx);
			cmdEndGpuTimestampQuery(graphicsCmd, gGpuProfileTokens[0]);

			rtBarriers[0] = { pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
			cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 1, rtBarriers);

			cmdEndGpuFrameProfile(graphicsCmd, gGpuProfileTokens[0]);
			endCmd(graphicsCmd);

			if (gAppSettings.useCPUPropagation || gAppSettings.useCPUPropagationDecoupled)
			{
				waitForAllResourceLoads();
			}

			// Submit all the work to the GPU and present
			Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, pComputeCompleteSemaphores[frameIdx] };
			QueueSubmitDesc submitDesc = {};
			submitDesc.mCmdCount = 1;
			submitDesc.mSignalSemaphoreCount = 1;
			submitDesc.ppCmds = &graphicsCmd;
			submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphores[frameIdx];
			submitDesc.pSignalFence = pRenderCompleteFences[frameIdx];

			if (gAppSettings.mAsyncCompute)
			{
				submitDesc.mWaitSemaphoreCount = 2;
				submitDesc.ppWaitSemaphores = pWaitSemaphores;
			}
			else
			{
				submitDesc.mWaitSemaphoreCount = 1;
				submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
			}

			queueSubmit(pGraphicsQueue, &submitDesc);
			QueuePresentDesc presentDesc = {};
			presentDesc.mIndex = presentIndex;
			presentDesc.mWaitSemaphoreCount = 1;
			presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphores[frameIdx];
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

		// Triangle Filtering
		DescriptorSetDesc setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);

		setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * gNumStages };

		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);
		// Light Clustering
		setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[0]);
		setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[1]);
		// VB, Shadow
		setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[0]);
		setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[1]);
		// VB Shade
		setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[0]);
		setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[1]);
	
		// Sky
		setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
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
		removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[1]);
	}

	void prepareDescriptorSets()
	{
		aura::prepareDescriptorSets();

		// Triangle Filtering
		{
			DescriptorData filterParams[4] = {};
			filterParams[0].pName = "vertexDataBuffer";
			filterParams[0].ppBuffers = &pGeom->pVertexBuffers[0];
			filterParams[1].pName = "indexDataBuffer";
			filterParams[1].ppBuffers = &pGeom->pIndexBuffer;
			filterParams[2].pName = "meshConstantsBuffer";
			filterParams[2].ppBuffers = &pMeshConstantsBuffer;
			filterParams[3].pName = "materialProps";
			filterParams[3].ppBuffers = &pMaterialPropertyBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], 4, filterParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData clearParams[3] = {};
				clearParams[0].pName = "indirectDrawArgsBufferAlpha";
				clearParams[0].mCount = gNumViews;
				clearParams[0].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_ALPHATESTED];
				clearParams[1].pName = "indirectDrawArgsBufferNoAlpha";
				clearParams[1].mCount = gNumViews;
				clearParams[1].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_OPAQUE];
				clearParams[2].pName = "uncompactedDrawArgsRW";
				clearParams[2].mCount = gNumViews;
				clearParams[2].ppBuffers = pUncompactedDrawArgumentsBuffer[i];
				updateDescriptorSet(pRenderer, i * gNumStages + 0, pDescriptorSetTriangleFiltering[1], 3, clearParams);

				DescriptorData filterParams[3] = {};
				filterParams[0].pName = "filteredIndicesBuffer";
				filterParams[0].mCount = gNumViews;
				filterParams[0].ppBuffers = pFilteredIndexBuffer[i];
				filterParams[1].pName = "uncompactedDrawArgsRW";
				filterParams[1].mCount = gNumViews;
				filterParams[1].ppBuffers = pUncompactedDrawArgumentsBuffer[i];
				filterParams[2].pName = "PerFrameConstants";
				filterParams[2].ppBuffers = &pPerFrameUniformBuffers[i];
				updateDescriptorSet(pRenderer, i * gNumStages + 1, pDescriptorSetTriangleFiltering[1], 3, filterParams);

				DescriptorData compactParams[5] = {};
				compactParams[0].pName = "indirectMaterialBuffer";
				compactParams[0].ppBuffers = &pFilterIndirectMaterialBuffer[i];
				compactParams[1].pName = "indirectDrawArgsBufferAlpha";
				compactParams[1].mCount = gNumViews;
				compactParams[1].mBindICB = true;
				compactParams[1].pICBName = "icbAlpha";
				compactParams[1].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_ALPHATESTED];
				compactParams[2].pName = "indirectDrawArgsBufferNoAlpha";
				compactParams[2].mCount = gNumViews;
				compactParams[2].mBindICB = true;
				compactParams[2].pICBName = "icbNoAlpha";
				compactParams[2].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_OPAQUE];
				compactParams[3].pName = "uncompactedDrawArgs";
				compactParams[3].mCount = gNumViews;
				compactParams[3].ppBuffers = pUncompactedDrawArgumentsBuffer[i];
				// Required to generate ICB (to bind index buffer)
				compactParams[4].pName = "filteredIndicesBuffer";
				compactParams[4].mCount = gNumViews;
				compactParams[4].ppBuffers = pFilteredIndexBuffer[i];
				updateDescriptorSet(pRenderer, i * gNumStages + 2, pDescriptorSetTriangleFiltering[1], 5, compactParams);
			}
		}
		// Light Clustering
		{
			DescriptorData params[3] = {};
			params[0].pName = "lights";
			params[0].ppBuffers = &pLightsBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetLightClusters[0], 1, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "lightClustersCount";
				params[0].ppBuffers = &pLightClustersCount[i];
				params[1].pName = "lightClusters";
				params[1].ppBuffers = &pLightClusters[i];
				params[2].pName = "PerFrameConstants";
				params[2].ppBuffers = &pPerFrameUniformBuffers[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetLightClusters[1], 3, params);
			}
		}
		// VB, Shadow
		{
			DescriptorData params[2] = {};
			params[0].pName = "diffuseMaps";
			params[0].mCount = gMaterialCount;
			params[0].ppTextures = gDiffuseMapsStorage;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 1, params);
			
			params[0] = {};
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				for (uint32_t j = 0; j < 2; ++j)
				{
					params[0].pName = "indirectMaterialBuffer";
					params[0].ppBuffers = j == 0 ? &pFilterIndirectMaterialBuffer[i] : &pIndirectMaterialBufferAll;
					params[1].pName = "PerFrameConstants";
					params[1].ppBuffers = &pPerFrameUniformBuffers[i];
					updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetVBPass[1], 2, params);
				}
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
			vbShadeParams[7].pName = "vertexTangent";
			vbShadeParams[7].ppBuffers = &pGeom->pVertexBuffers[3];
			vbShadeParams[8].pName = "lights";
			vbShadeParams[8].ppBuffers = &pLightsBuffer;
			vbShadeParams[9].pName = "shadowMap";
			vbShadeParams[9].ppTextures = &pRenderTargetShadow->pTexture;
			vbShadeParams[10].pName = "LPVGridCascades";
			vbShadeParams[10].ppTextures = ppTextures;
			vbShadeParams[10].mCount = pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE;
			vbShadeParams[11].pName = "meshConstantsBuffer";
			vbShadeParams[11].ppBuffers = &pMeshConstantsBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 12, vbShadeParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				uint32_t count = 0;
				vbShadeParamsPerFrame[count].pName = "lightClustersCount";
				vbShadeParamsPerFrame[count++].ppBuffers = &pLightClustersCount[i];
				vbShadeParamsPerFrame[count].pName = "lightClusters";
				vbShadeParamsPerFrame[count++].ppBuffers = &pLightClusters[i];
				vbShadeParamsPerFrame[count].pName = "PerFrameConstants";
				vbShadeParamsPerFrame[count++].ppBuffers = &pPerFrameUniformBuffers[i];
				
				Buffer* pIndirectBuffers[gNumGeomSets] = { NULL };
				for (uint32_t g = 0; g < gNumGeomSets; ++g)
				{
					pIndirectBuffers[g] = pFilteredIndirectDrawArgumentsBuffer[i][g][VIEW_CAMERA];
				}

				vbShadeParamsPerFrame[count].pName = "auraApplyLightData";
				vbShadeParamsPerFrame[count++].ppBuffers = &pUniformBufferAuraLightApply[i];

				uint32_t numDesc = count;
				for (uint32_t j = 0; j < 2; ++j)
				{
					count = numDesc;
					vbShadeParamsPerFrame[count].pName = "indirectMaterialBuffer";
					vbShadeParamsPerFrame[count++].ppBuffers =
						j == 0 ? &pFilterIndirectMaterialBuffer[i] : &pIndirectMaterialBufferAll;
					vbShadeParamsPerFrame[count].pName = "filteredIndexBuffer";
					vbShadeParamsPerFrame[count++].ppBuffers = j == 0 ? &pFilteredIndexBuffer[i][VIEW_CAMERA] : &pGeom->pIndexBuffer;
					vbShadeParamsPerFrame[count].pName = "indirectDrawArgs";
					vbShadeParamsPerFrame[count].mCount = gNumGeomSets;
					vbShadeParamsPerFrame[count++].ppBuffers = j == 0 ? pIndirectBuffers : pIndirectDrawArgumentsBufferAll;
					updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetVBShade[1], count, vbShadeParamsPerFrame);
				}
			}
		}
		// Sky
		{
			DescriptorData skyParams[1] = {};
			skyParams[0].pName = "skyboxTex";
			skyParams[0].ppTextures = &pSkybox;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);
			for (uint32_t i = 0; i < gImageCount; ++i)
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
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);

		swapChainDesc.mColorClearValue = { {1, 1, 1, 1} };
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addRenderTargets()
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		aura::LightPropagationCascadeDesc cascades[gRSMCascadeCount] =
		{
			{ gLPVCascadeSpans[0], 0.1f, 0 },
			{ gLPVCascadeSpans[1], 1.0f, 0 },
			{ gLPVCascadeSpans[2], 10.0f, 0 },
		};
		aura::addRenderTargets(cascades);

		/************************************************************************/
		/************************************************************************/
		ClearValue optimizedDepthClear = {{1.0f, 0}};
		ClearValue optimizedColorClearBlack = {{0.0f, 0.0f, 0.0f, 0.0f}};
		ClearValue optimizedColorClearWhite = {{1.0f, 1.0f, 1.0f, 1.0f}};
		/************************************************************************/
		// Main depth buffer
		/************************************************************************/
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = optimizedDepthClear;
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
		rsmRTDesc.mClearValue = optimizedColorClearBlack;
		rsmRTDesc.mDepth = 1;
		rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		rsmRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
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
			rsmRTDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
			addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMAlbedo[i]);
			addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMNormal[i]);

			rsmRTDesc.mClearValue = optimizedDepthClear;
			rsmRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
			addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMDepth[i]);
		}
#else
		addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMAlbedo);
		addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMNormal);

		rsmRTDesc.mClearValue = optimizedDepthClear;
		rsmRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		rsmRTDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		addRenderTarget(pRenderer, &rsmRTDesc, &pRenderTargetRSMDepth);
#endif

		/************************************************************************/
		/************************************************************************/
	}

	void removeRenderTargets()
	{
		aura::removeRenderTargets();

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
		Shader* pShaders[gNumGeomSets * shaderCount] = {};
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

		// Triangle filtering root signatures
		Shader* pCullingShaders[] = { pShaderClearBuffers, pShaderTriangleFiltering, pShaderBatchCompaction };
		RootSignatureDesc triangleFilteringRootDesc = { pCullingShaders, TF_ARRAY_COUNT(pCullingShaders) };

		addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);
		Shader* pClusterShaders[] = { pShaderClearLightClusters, pShaderClusterLights };
		RootSignatureDesc clearLightRootDesc = { pClusterShaders, 2 };
		addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureLightClusters);

		const char*       pSkyboxSamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxSamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSingatureSkybox);

		/************************************************************************/
		// Setup indirect command signatures
		/************************************************************************/
		uint32_t indirectArgCount = 0;
		IndirectArgumentDescriptor indirectArgs[2] = {};
		if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
		{
			indirectArgs[0].mType = INDIRECT_CONSTANT;
			indirectArgs[0].mIndex = getDescriptorIndexFromName(pRootSignatureVBPass, "indirectRootConstant");
			indirectArgs[0].mByteSize = sizeof(uint32_t);
			++indirectArgCount;
		}
		indirectArgs[indirectArgCount++].mType = INDIRECT_DRAW_INDEX;
		CommandSignatureDesc vbPassDesc = { pRootSignatureVBPass, indirectArgs, indirectArgCount };
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
	}

	void removeRootSignatures()
	{
		aura::removeRootSignatures();

		removeRootSignature(pRenderer, pRootSingatureSkybox);

		removeRootSignature(pRenderer, pRootSignatureLightClusters);
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
		//ShaderLoadDesc resolvePass = {};
		ShaderLoadDesc clearBuffer = {};
		ShaderLoadDesc triangleCulling = {};
		ShaderLoadDesc batchCompaction = {};
		ShaderLoadDesc clearLights = {};
		ShaderLoadDesc clusterLights = {};
		ShaderLoadDesc fillRSM = {};
		ShaderLoadDesc fillRSMAlpha = {};

		shadowPass.mStages[0] = { "shadow_pass.vert", NULL, 0 };
		shadowPassAlpha.mStages[0] = { "shadow_pass_alpha.vert", NULL, 0 };
		shadowPassAlpha.mStages[1] = { "shadow_pass_alpha.frag", NULL, 0 };

		vbPass.mStages[0] = { "visibilityBuffer_pass.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPass.mStages[1] = { "visibilityBuffer_pass.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassAlpha.mStages[0] = { "visibilityBuffer_pass_alpha.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassAlpha.mStages[1] = { "visibilityBuffer_pass_alpha.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
#if defined(ORBIS) || defined(PROSPERO)
		// No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
		// a passthrough gs
		vbPass.mStages[2] = { "visibilityBuffer_pass.geom", NULL, 0 };
		vbPassAlpha.mStages[2] = { "visibilityBuffer_pass_alpha.geom", NULL, 0 };
#endif

		const char* vbShadeShaderNames[] =
		{
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

				vbShade[configIndex].mStages[0] = { "visibilityBuffer_shade.vert", NULL, 0 };
				vbShade[configIndex].mStages[1] = { vbShadeShaderNames[configIndex], NULL, 0 };
			}
		}
		// Triangle culling compute shader
		triangleCulling.mStages[0] = { pRenderer->pActiveGpuSettings->mIndirectCommandBuffer ? "triangle_filtering_icb.comp" : "triangle_filtering.comp", NULL, 0 };
		// Batch compaction compute shader
		batchCompaction.mStages[0] = { pRenderer->pActiveGpuSettings->mIndirectCommandBuffer ? "batch_compaction_icb.comp" : "batch_compaction.comp", NULL, 0 };
		// Clear buffers compute shader
		clearBuffer.mStages[0] = { pRenderer->pActiveGpuSettings->mIndirectCommandBuffer ? "clear_buffers_icb.comp" : "clear_buffers.comp", NULL, 0 };
		// Clear light clusters compute shader
		clearLights.mStages[0] = { "clear_light_clusters.comp", NULL, 0 };
		// Cluster lights compute shader
		clusterLights.mStages[0] = { "cluster_lights.comp", NULL, 0 };

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0 };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0 };

		// Fill RSM shader
		fillRSM.mStages[0] = { "fill_rsm.vert", NULL, 0 };
		fillRSM.mStages[1] = { "fill_rsm.frag", NULL, 0 };
		fillRSMAlpha.mStages[0] = { "fill_rsm_alpha.vert", NULL, 0 };
		fillRSMAlpha.mStages[1] = { "fill_rsm_alpha.frag", NULL, 0 };

		addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHATESTED]);
		addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < gVbShadeConfigCount; ++i)
			addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
		addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
		addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
		addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
		addShader(pRenderer, &clusterLights, &pShaderClusterLights);
		addShader(pRenderer, &batchCompaction, &pShaderBatchCompaction);
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
		addShader(pRenderer, &fillRSM, &pShaderFillRSM[GEOMSET_OPAQUE]);
 		addShader(pRenderer, &fillRSMAlpha, &pShaderFillRSM[GEOMSET_ALPHATESTED]);
	}

	void removeShaders()
	{
		aura::removeShaders();

		removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHATESTED]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < gVbShadeConfigCount; ++i)
			removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
		removeShader(pRenderer, pShaderTriangleFiltering);
		removeShader(pRenderer, pShaderBatchCompaction);
		removeShader(pRenderer, pShaderClearBuffers);
		removeShader(pRenderer, pShaderClusterLights);
		removeShader(pRenderer, pShaderClearLightClusters);
	
		removeShader(pRenderer, pShaderSkybox);

		removeShader(pRenderer, pShaderFillRSM[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderFillRSM[GEOMSET_ALPHATESTED]);
	}

	void addPipelines()
	{
		aura::addPipelines(pPipelineCache, (aura::TinyImageFormat)pSwapChain->ppRenderTargets[0]->mFormat, (aura::TinyImageFormat)pDepthBuffer->mFormat,
			(aura::SampleCount)pSwapChain->ppRenderTargets[0]->mSampleCount, pSwapChain->ppRenderTargets[0]->mSampleQuality);

		/************************************************************************/
		// Setup compute pipelines for triangle filtering
		/************************************************************************/
		PipelineDesc pipelineDesc = {};
		pipelineDesc.pCache = pPipelineCache;
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineSettings = pipelineDesc.mComputeDesc;
		computePipelineSettings.pShaderProgram = pShaderClearBuffers;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		pipelineDesc.pName = "Clear Filtering Buffers";
		addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

		// Create the compute pipeline for GPU triangle filtering
		pipelineDesc.pName = "Triangle Filtering";
		computePipelineSettings.pShaderProgram = pShaderTriangleFiltering;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

		pipelineDesc.pName = "Batch Compaction";
		computePipelineSettings.pShaderProgram = pShaderBatchCompaction;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineBatchCompaction);

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
		// Vertex layout used by all geometry passes (shadow, visibility)
		/************************************************************************/
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 4;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
		vertexLayout.mAttribs[1].mBinding = 1;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
		vertexLayout.mAttribs[2].mBinding = 2;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayout.mAttribs[3].mFormat = TinyImageFormat_R32_UINT;
		vertexLayout.mAttribs[3].mBinding = 3;
		vertexLayout.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

		// Position only vertex stream that is used in shadow opaque pass
		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;

		/************************************************************************/
		// Setup the Shadow Pass Pipeline
		/************************************************************************/
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		DepthStateDesc depthStateDisableDesc = {};

		RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };
		RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
		//RasterizerStateDesc rasterizerStateCullBackDesc = { CULL_MODE_BACK };
		RasterizerStateDesc rasterizerStateCullFrontDepthClampedDesc = rasterizerStateCullFrontDesc;
		rasterizerStateCullFrontDepthClampedDesc.mDepthClampEnable = true;
		RasterizerStateDesc rasterizerStateCullNoneDepthClampedDesc = rasterizerStateCullNoneDesc;
		rasterizerStateCullNoneDepthClampedDesc.mDepthClampEnable = true;

		BlendStateDesc blendStateSkyBoxDesc = {};
		blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
		blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;

		blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
		blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;

		blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;

		blendStateSkyBoxDesc.mMasks[0] = ALL;
		blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;

		// Setup pipeline settings
		pipelineDesc = {};
		pipelineDesc.pCache = pPipelineCache;
		pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
		shadowPipelineSettings = { 0 };
		shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowPipelineSettings.pDepthState = &depthStateDesc;
		shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
		shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
		shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
		shadowPipelineSettings.pRootSignature = pRootSignatureVBPass;
		shadowPipelineSettings.mSupportIndirectCommandBuffer = true;
		shadowPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDepthClampedDesc;
		shadowPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[0];
		pipelineDesc.pName = "Shadow Opaque";
		addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[0]);

		shadowPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
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
		vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
		vbPassPipelineSettings.mSupportIndirectCommandBuffer = true;

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			if (i == GEOMSET_OPAQUE)
				vbPassPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
			else
				vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;

			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? &rasterizerStateCullNoneDesc : &rasterizerStateCullFrontDesc;
			vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

#if defined(XBOX)
			ExtendedGraphicsPipelineDesc edescs[2] = {};
			edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
			initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
			edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

			edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
			edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
			edescs[1].pixelShaderOptions.depthBeforeShader = !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

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

#if defined(XBOX)
			ExtendedGraphicsPipelineDesc edescs[2] = {};
			edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
			initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
			//edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

			edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
			edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
			edescs[1].pixelShaderOptions.depthBeforeShader = !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

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

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
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
		//pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
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
		TinyImageFormat rsmColorFormats[] =
		{
			pRenderTargetRSMAlbedo[0]->mFormat,
			pRenderTargetRSMNormal[0]->mFormat,
		};
#else
		TinyImageFormat rsmColorFormats[] =
		{
			pRenderTargetRSMAlbedo->mFormat,
			pRenderTargetRSMNormal->mFormat,
		};
#endif
		PipelineDesc fillRSMPipelineSettings = {};
		fillRSMPipelineSettings.mType = PIPELINE_TYPE_GRAPHICS;
		fillRSMPipelineSettings.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		fillRSMPipelineSettings.mGraphicsDesc.mRenderTargetCount = 2;
		fillRSMPipelineSettings.mGraphicsDesc.pDepthState = &depthStateDesc;
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
		fillRSMPipelineSettings.mGraphicsDesc.pVertexLayout = &vertexLayout;
		fillRSMPipelineSettings.mGraphicsDesc.mSupportIndirectCommandBuffer = true;

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			fillRSMPipelineSettings.mGraphicsDesc.pRasterizerState = i == GEOMSET_ALPHATESTED ? &rasterizerStateCullNoneDepthClampedDesc : &rasterizerStateCullFrontDepthClampedDesc;
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
		removePipeline(pRenderer, pPipelineBatchCompaction);
		removePipeline(pRenderer, pPipelineClearBuffers);
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
			alphaTestMaterials[i] = pScene->materials[i].alphaTested ? 1 : 0;
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

		tf_free(alphaTestMaterials);
		/************************************************************************/
		// Indirect draw arguments to draw all triangles
		/************************************************************************/
		const uint32_t numBatches = (const uint32_t)gMeshCount;
		uint32_t materialIDPerDrawCall[MATERIAL_BUFFER_SIZE] = {};
		uint32_t indirectArgsNoAlphaDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};
		uint32_t indirectArgsDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};

		uint32_t iAlpha = 0, iNoAlpha = 0;
		const uint32_t argOffset = pRenderer->pActiveGpuSettings->mIndirectRootConstant ? 1 : 0;
		for (uint32_t i = 0; i < numBatches; ++i)
		{
			uint matID = i;
			Material* mat = &pScene->materials[matID];

			if (mat->alphaTested)
			{
				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsDwords[iAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + argOffset];
				*arg = pScene->geom->pDrawArgs[i];
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					indirectArgsDwords[iAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = iAlpha;
				}
				else
				{
					// No drawId or gl_DrawId but instance id works as expected so use that as the draw id
					arg->mStartInstance = iAlpha;
				}

				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(true, j) + iAlpha] = matID;
				iAlpha++;
			}
			else
			{
				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsNoAlphaDwords[iNoAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + argOffset];
				*arg = pScene->geom->pDrawArgs[i];
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					indirectArgsNoAlphaDwords[iNoAlpha * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = iNoAlpha;
				}
				else
				{
					// No drawId or gl_DrawId but instance id works as expected so use that as the draw id
					arg->mStartInstance = iNoAlpha;
				}

				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(false, j) + iNoAlpha] = matID;
				iNoAlpha++;
			}
		}
		indirectArgsDwords[DRAW_COUNTER_SLOT_POS] = iAlpha;
		indirectArgsNoAlphaDwords[DRAW_COUNTER_SLOT_POS] = iNoAlpha;

		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = iNoAlpha;
			gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = iAlpha;
		}

		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			// Setup uniform data for draw batch data
			BufferLoadDesc indirectBufferDesc = {};
			indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS;
			indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
			indirectBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT;
			indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
			indirectBufferDesc.pData = i == 0 ? indirectArgsNoAlphaDwords : indirectArgsDwords;
			indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll[i];
			indirectBufferDesc.mDesc.pName = "Indirect Buffer Desc";
			addResource(&indirectBufferDesc, NULL);
		}

		BufferLoadDesc indirectDesc = {};
		indirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
		indirectDesc.pData = materialIDPerDrawCall;
		indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
		indirectDesc.mDesc.pName = "Indirect Desc";
		addResource(&indirectDesc, NULL);

		/************************************************************************/
		// Indirect buffers for culling
		/************************************************************************/
		BufferLoadDesc filterIbDesc = {};
		filterIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW;
		filterIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIbDesc.mDesc.mElementCount = pGeom->mIndexCount;
		filterIbDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIbDesc.mDesc.mSize = filterIbDesc.mDesc.mElementCount * filterIbDesc.mDesc.mStructStride;
		filterIbDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		filterIbDesc.mDesc.pName = "Filtered IB Desc";
		filterIbDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gNumViews; ++j)
			{
				filterIbDesc.ppBuffer = &pFilteredIndexBuffer[i][j];
				addResource(&filterIbDesc, NULL);
			}
		}

		memset(indirectArgsDwords, 0, sizeof(indirectArgsDwords));
		for (uint32_t i = 0; i < MAX_DRAWS_INDIRECT; ++i)
		{
			uint32_t* arg = &indirectArgsDwords[i * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS];
			if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
			{
				arg[0] = i;
			}
			else
			{
				arg[argOffset + INDIRECT_DRAW_INDEX_ELEM_INDEX(mStartInstance)] = i;
			}
			arg[argOffset + INDIRECT_DRAW_INDEX_ELEM_INDEX(mInstanceCount)] = (i < gMeshCount) ? 1 : 0;
		}

		BufferLoadDesc filterIndirectDesc = {};
		filterIndirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_INDIRECT_COMMAND_BUFFER;
		filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIndirectDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS;
		filterIndirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
		filterIndirectDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		filterIndirectDesc.mDesc.mICBDrawType = INDIRECT_DRAW_INDEX;
		filterIndirectDesc.mDesc.mICBMaxCommandCount = MAX_DRAWS_INDIRECT;
		filterIndirectDesc.mDesc.pName = "Filtered Indirect Desc";
		filterIndirectDesc.pData = indirectArgsDwords;

		BufferLoadDesc uncompactedDesc = {};
		uncompactedDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		uncompactedDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		uncompactedDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT;
		uncompactedDesc.mDesc.mStructStride = sizeof(UncompactedDrawArguments);
		uncompactedDesc.mDesc.mSize = uncompactedDesc.mDesc.mElementCount * uncompactedDesc.mDesc.mStructStride;
		uncompactedDesc.mDesc.mStartState = RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		uncompactedDesc.mDesc.pName = "Uncompacted Draw Arguments Desc";
		uncompactedDesc.pData = NULL;

		BufferLoadDesc filterMaterialDesc = {};
		filterMaterialDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterMaterialDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterMaterialDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		filterMaterialDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterMaterialDesc.mDesc.mSize = filterMaterialDesc.mDesc.mElementCount * filterMaterialDesc.mDesc.mStructStride;
		filterMaterialDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		filterMaterialDesc.mDesc.pName = "Filtered Indirect Material Desc";
		filterMaterialDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			filterMaterialDesc.ppBuffer = &pFilterIndirectMaterialBuffer[i];
			addResource(&filterMaterialDesc, NULL);

			for (uint32_t view = 0; view < gNumViews; ++view)
			{
				uncompactedDesc.ppBuffer = &pUncompactedDrawArgumentsBuffer[i][view];
				addResource(&uncompactedDesc, NULL);

				for (uint32_t geom = 0; geom < gNumGeomSets; ++geom)
				{
					filterIndirectDesc.ppBuffer = &pFilteredIndirectDrawArgumentsBuffer[i][geom][view];
					addResource(&filterIndirectDesc, NULL);
				}
			}
		}
		/************************************************************************/
		// Triangle filtering buffers
		/************************************************************************/
		// Create buffers to store the list of filtered triangles. These buffers
		// contain the triangle IDs of the triangles that passed the culling tests.
		// One buffer per back buffer image is created for triple buffering.
		uint32_t bufferSizeTotal = 0;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				const uint32_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
				bufferSizeTotal += bufferSize;
				pFilterBatchChunk[i][j] = (FilterBatchChunk*)tf_malloc(sizeof(FilterBatchChunk));
				pFilterBatchChunk[i][j]->currentBatchCount = 0;
				pFilterBatchChunk[i][j]->currentDrawCallCount = 0;
			}
		}

		addUniformGPURingBuffer(pRenderer, bufferSizeTotal, &pFilterBatchDataBuffer);
		/************************************************************************/
		// Mesh constants
		/************************************************************************/
		// create mesh constants buffer
		MeshConstants* meshConstants = (MeshConstants*)tf_malloc(gMeshCount * sizeof(MeshConstants));

		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			meshConstants[i].faceCount = pScene->geom->pDrawArgs[i].mIndexCount / 3;
			meshConstants[i].indexOffset = pScene->geom->pDrawArgs[i].mStartIndex;
			meshConstants[i].materialID = i;
			meshConstants[i].twoSided = pScene->materials[i].twoSided ? 1 : 0;
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

		tf_free(meshConstants);

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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pPerFrameUniformBuffers[i];
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
		lightClustersCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER;
		lightClustersCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		lightClustersCountBufferDesc.mDesc.mFirstElement = 0;
		lightClustersCountBufferDesc.mDesc.mElementCount = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
		lightClustersCountBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		lightClustersCountBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		lightClustersCountBufferDesc.pData = lightClustersInitData;
		lightClustersCountBufferDesc.mDesc.pName = "Light Cluster Count Buffer Desc";
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
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
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
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
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			skyDataBufferDesc.ppBuffer = &pUniformBufferSky[frameIdx];
			addResource(&skyDataBufferDesc, NULL);
		}
		/************************************************************************/
		/************************************************************************/
	}

	void removeTriangleFilteringBuffers()
	{
		/************************************************************************/
		// Material props
		/************************************************************************/
		removeResource(pMaterialPropertyBuffer);
		/************************************************************************/
		// Indirect draw arguments to draw all triangles
		/************************************************************************/
		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			removeResource(pIndirectDrawArgumentsBufferAll[i]);
		}

		removeResource(pIndirectMaterialBufferAll);

		/************************************************************************/
		// Indirect buffers for culling
		/************************************************************************/
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gNumViews; ++j)
			{
				removeResource(pFilteredIndexBuffer[i][j]);
			}
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pFilterIndirectMaterialBuffer[i]);
			for (uint32_t view = 0; view < gNumViews; ++view)
			{
				removeResource(pUncompactedDrawArgumentsBuffer[i][view]);
				for (uint32_t geom = 0; geom < gNumGeomSets; ++geom)
				{
					removeResource(pFilteredIndirectDrawArgumentsBuffer[i][geom][view]);
				}
			}
		}

		/************************************************************************/
		// Triangle filtering buffers
		/************************************************************************/
		// Create buffers to store the list of filtered triangles. These buffers
		// contain the triangle IDs of the triangles that passed the culling tests.
		// One buffer per back buffer image is created for triple buffering.
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				tf_free(pFilterBatchChunk[i][j]);
			}

		}
		removeGPURingBuffer(pFilterBatchDataBuffer);
		/************************************************************************/
		// Mesh constants
		/************************************************************************/
		removeResource(pMeshConstantsBuffer);

		/************************************************************************/
		// Per Frame Constant Buffers
		/************************************************************************/
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pPerFrameUniformBuffers[i]);
		}
		/************************************************************************/
		// Lighting buffers
		/************************************************************************/
		removeResource(pLightsBuffer);
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
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

		mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
		mat4 cameraView = currentFrame->gCameraView = gCamera.pCameraController->getViewMatrix();
		mat4 cameraProj = mat4::perspective(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);
		
		currentFrame->gCameraModelView = cameraView * cameraModel;

		// directional light rotation & translation
		mat4 rotation = mat4::rotationXY(gDirectionalLight.mRotationXY.x, gDirectionalLight.mRotationXY.y);
		vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));

		mat4 lightModel = mat4::scale(vec3(SCENE_SCALE));
		mat4 lightView = rotation;
		mat4 lightProj = mat4::orthographic(-600, 600, -950, 350, -1100, 500);

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
		/************************************************************************/
		// Matrix data
		/************************************************************************/
		currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp = lightProj * lightView;
		currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].invVP =
			inverse(currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp);
		currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].projection = lightProj;
		currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].mvp =
			currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp * lightModel;

		currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp = cameraProj * cameraView;
		currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].invVP =
			inverse(currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp);
		currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].projection = cameraProj;
		currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].mvp =
			currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp * cameraModel;
		/************************************************************************/
		// Culling data
		/************************************************************************/
		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].sampleCount = 1;
		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)gShadowMapSize, (float)gShadowMapSize };

		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].sampleCount = 1;
		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

		// Cache eye position in object space for cluster culling on the CPU
		currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
		currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
			(inverse(cameraView * cameraModel) * vec4(0, 0, 0, 1)).getXYZ();    // vec4(0,0,0,1) is the camera position in eye space
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
		currentFrame->gUniformDataSky.mProjectView = cameraProj * cameraView;

		if (gAppSettings.useLPV) 
		{
			calculateRSMCascadesForLPV(lightView,
				gRSMCascadeCount,
				&currentFrame->gRSMCascadeProjection[0],
				&currentFrame->gRSMCascadeView[0],
				&currentFrame->gRSMViewSize[0]);

			for (uint32_t i = 0; i < gRSMCascadeCount; i++)
			{
				uint32_t viewID = VIEW_RSM_CASCADE0 + i;
								
				/************************************************************************/
				// Matrix data
				/************************************************************************/
				mat4 cascadeProjection = currentFrame->gRSMCascadeProjection[i];
				mat4 cascadeTransform = currentFrame->gRSMCascadeView[i];
				currentFrame->gPerFrameUniformData.transform[viewID].projection = cascadeProjection;
				currentFrame->gPerFrameUniformData.transform[viewID].vp = cascadeProjection * cascadeTransform;
				currentFrame->gPerFrameUniformData.transform[viewID].invVP =
					inverse(currentFrame->gPerFrameUniformData.transform[viewID].vp);
				currentFrame->gPerFrameUniformData.transform[viewID].mvp =
					currentFrame->gPerFrameUniformData.transform[viewID].vp * cameraModel;
				/************************************************************************/
				// Culling data
				/************************************************************************/
				currentFrame->gPerFrameUniformData.cullingViewports[viewID].sampleCount = 1;
				currentFrame->gPerFrameUniformData.cullingViewports[viewID] = { float2((float)gRSMResolution, (float)gRSMResolution), 1 };

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
		currentFrame->gPerFrameUniformData.windData.g_Time = getTimerMSec(&gAccumTimer, false) / 1000.0f;
	}

	void updateLPVParams()
	{
		fillAuraParams(&pAura->mParams, &pAura->mCPUParams);
	}

	void fillAuraParams(aura::LightPropagationVolumeParams* params, aura::CPUPropagationParams* cpuParams)
	{
		params->bUseMultipleReflections = gAppSettings.useMultipleReflections;
		params->bUseCPUPropagation = gAppSettings.useCPUPropagation;
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
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadow->mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetShadow, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mWidth, (float)pRenderTargetShadow->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mWidth, pRenderTargetShadow->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

		const char* profileNames[gNumGeomSets] = { "SM Opaque", "SM Alpha" };

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[0]);

		cmdBindPipeline(cmd, pPipelineShadowPass[0]);
		// Position only opaque shadow pass
		Buffer* pVertexBuffersPositionOnly[] = { pGeom->pVertexBuffers[0] };
		cmdBindVertexBuffer(cmd, 1, pVertexBuffersPositionOnly, pGeom->mVertexStrides, NULL);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBPass[1]);

		Buffer* pIndirectBufferPositionOnly = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][0][VIEW_SHADOW]
			: pIndirectDrawArgumentsBufferAll[0];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[0], pIndirectBufferPositionOnly, 0, pIndirectBufferPositionOnly,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[1]);

		cmdBindPipeline(cmd, pPipelineShadowPass[1]);
		// Alpha tested shadow pass with extra vetex attribute stream
		Buffer* pVertexBuffers[] = { pGeom->pVertexBuffers[0], pGeom->pVertexBuffers[1] };
		cmdBindVertexBuffer(cmd, 2, pVertexBuffers, pGeom->mVertexStrides, NULL);

		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
		cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBPass[1]);

		Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][1][VIEW_SHADOW]
			: pIndirectDrawArgumentsBufferAll[1];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[1], pIndirectBuffer, 0, pIndirectBuffer,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
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

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	void updateAuraCascades(Cmd* cmd, uint32_t frameIdx) {
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

			char label[32];// Increased array size to 32 to avoid overflow warning (-Wformat-overflow)
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

			mat4 cascadeViewProjection = gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_RSM_CASCADE0 + i].vp;
			// Convert into worldspace directly from [0..1] range instead of [-1..1] clip-space range
			mat4 inverseCascadeViewProjection = inverse(cascadeViewProjection) * mat4::translation(vec3(-1.0f, 1.0f, 0.0f)) * mat4::scale(vec3(2.0f, -2.0f, 1.0f));

			vec4       camDir = normalize(gPerFrame[frameIdx].gRSMCascadeView[i].getRow(2));
			aura::vec3 cameraDir(camDir.getX(), camDir.getY(), camDir.getZ());

			float viewArea = gPerFrame[frameIdx].gRSMViewSize[i] * gPerFrame[frameIdx].gRSMViewSize[i];

#if TEST_RSM
			sprintf(label, "Inject RSM) Cascade #%u", i);
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], label);
			injectRSM(
			cmd, pRenderer, pAura, i, aura::convertMatrix4FromClient((float*)&inverseCascadeViewProjection), cameraDir, gRSMResolution, gRSMResolution, viewArea,
			pRenderTargetRSMAlbedo[i]->pTexture, pRenderTargetRSMNormal[i]->pTexture, pRenderTargetRSMDepth[i]->pTexture);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
#else
			sprintf(label, "Inject RSM) Cascade #%u", i);
			cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], label);
			injectRSM(
				cmd, pRenderer, pAura, i, aura::convertMatrix4FromClient((float*)&inverseCascadeViewProjection), cameraDir, gRSMResolution, gRSMResolution,
				viewArea,
				pRenderTargetRSMAlbedo->pTexture, pRenderTargetRSMNormal->pTexture, pRenderTargetRSMDepth->pTexture);
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
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetVBPass->mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 1, &pRenderTargetVBPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, pGeom->mIndexType, 0);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, gProfileNames[i]);

			cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);

			Buffer* pVertexBuffers[] = { pGeom->pVertexBuffers[0], pGeom->pVertexBuffers[1] };
			cmdBindVertexBuffer(cmd, 2, pVertexBuffers, pGeom->mVertexStrides, NULL);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
			cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBPass[1]);

			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA]
				: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawVisibilityBufferPass
	// to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index buffer because
	// the triangle positions are calculated internally using vertex_id.
	void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTargetBarrier* pBarriers = (RenderTargetBarrier*)alloca(NUM_GRIDS_PER_CASCADE * pAura->mCascadeCount * sizeof(RenderTargetBarrier));
		for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
		{
			for (uint32_t j = 0; j < NUM_GRIDS_PER_CASCADE; ++j)
			{
				pBarriers[i * NUM_GRIDS_PER_CASCADE + j] = { pAura->pCascades[i]->pLightGrids[j], RESOURCE_STATE_LPV, RESOURCE_STATE_SHADER_RESOURCE };
			}
		}
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE, pBarriers);

		mat4 invMvp = inverse(
			mat4::scale(vec3(0.5, -0.5, 1)) * mat4::translation(vec3(1, -1, 0)) *
			gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_CAMERA].vp);
		aura::vec3 camPos = aura::vec3(
			gPerFrame[frameIdx].gPerFrameUniformData.camPos.getX(), gPerFrame[frameIdx].gPerFrameUniformData.camPos.getY(),
			gPerFrame[frameIdx].gPerFrameUniformData.camPos.getZ());

		LightApplyData lightApplyData = {};
		aura::getLightApplyData(pAura, aura::convertMatrix4FromClient((float*)&invMvp), camPos, &lightApplyData);
		BufferUpdateDesc update = { pUniformBufferAuraLightApply[frameIdx] };
		beginUpdateResource(&update);
		memcpy(update.pMappedData, &lightApplyData, sizeof(lightApplyData));
		endUpdateResource(&update, NULL);

		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = { {0.0f, 0.0f, 0.0f, 0.0f} };

		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

		int pipelineIndex = 2 * (int)gAppSettings.useLPV + (int)gAppSettings.enableSun;
		cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[pipelineIndex]);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
		cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBShade[1]);
		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		for (uint32_t i = 0; i < pAura->mCascadeCount; ++i)
		{
			for (uint32_t j = 0; j < NUM_GRIDS_PER_CASCADE; ++j)
			{
				pBarriers[i * NUM_GRIDS_PER_CASCADE + j] = { pAura->pCascades[i]->pLightGrids[j], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_LPV };
			}
		}
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, pAura->mCascadeCount * NUM_GRIDS_PER_CASCADE, pBarriers);
	}

	void fillRSM(Cmd* cmd, uint32_t frameIdx, ProfileToken nGpuProfileToken, uint32_t cascadeIndex)
	{
		uint32_t viewId = VIEW_RSM_CASCADE0 + cascadeIndex;

#if TEST_RSM
		RenderTarget* pRSMRTs[] = { pRenderTargetRSMAlbedo[cascadeIndex], pRenderTargetRSMNormal[cascadeIndex] };
#else
		RenderTarget* pRSMRTs[] = { pRenderTargetRSMAlbedo, pRenderTargetRSMNormal };
#endif
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < 2; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRSMRTs[i]->mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
#if TEST_RSM
		loadActions.mClearDepth = pRenderTargetRSMDepth[cascadeIndex]->mClearValue;
#else
		loadActions.mClearDepth = pRenderTargetRSMDepth->mClearValue;
#endif

		// Start render pass and apply load actions
#if TEST_RSM
		cmdBindRenderTargets(cmd, 2, pRSMRTs, pRenderTargetRSMDepth[cascadeIndex], &loadActions, NULL, NULL, -1, -1);
#else
		cmdBindRenderTargets(cmd, 2, pRSMRTs, pRenderTargetRSMDepth, &loadActions, NULL, NULL, -1, -1);
#endif
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRSMRTs[0]->mWidth, (float)pRSMRTs[0]->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRSMRTs[0]->mWidth, pRSMRTs[0]->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][viewId] : pGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, pGeom->mIndexType, 0);

		struct RSMConstants
		{
			uint32_t viewId;
			uint32_t useColorMaps;
		};
		
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBindPipeline(cmd, pPipelineFillRSM[i]);
			cmdBindVertexBuffer(cmd, pGeom->mVertexBufferCount, pGeom->pVertexBuffers, pGeom->mVertexStrides, NULL);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
			cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)!gAppSettings.mFilterTriangles, pDescriptorSetVBPass[1]);
			
			RSMConstants constantData = { viewId, gAppSettings.useColorMaps };
			cmdBindPushConstants(cmd, pRootSignatureVBPass, gVBPassRootConstantIndex, &constantData);

			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][viewId]
				: pIndirectDrawArgumentsBufferAll[i];

			cmdExecuteIndirect(
				cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
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

	// Executes the compute shader that performs triangle filtering on the GPU.
	// This step performs different visibility tests per triangle to determine whether they
	// potentially affect to the final image or not.
	// The results of executing this shader are stored in:
	// - pFilteredTriangles: list of triangle IDs that passed the culling tests
	// - pIndirectDrawArguments: the vertexCount member of this structure is calculated in order to
	// indicate the renderer the amount of vertices per batch to render.
	void filterTriangles(Cmd* cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk, Buffer* pBuffer, uint64_t offset)
	{
		UNREF_PARAM(frameIdx);
		// Check if there are batches to filter
		if (batchChunk->currentBatchCount == 0)
			return;

		DescriptorDataRange range = { (uint32_t)offset, BATCH_COUNT * sizeof(SmallBatchData) };
		DescriptorData params[1] = {};
		params[0].pName = "batchData_rootcbv";
		params[0].pRanges = &range;
		params[0].ppBuffers = &pBuffer;
		cmdBindDescriptorSetWithRootCbvs(cmd, 0, pDescriptorSetTriangleFiltering[0], 1, params);
		cmdDispatch(cmd, batchChunk->currentBatchCount, 1, 1);

		// Reset batch chunk to start adding triangles to it
		batchChunk->currentBatchCount = 0;
		batchChunk->currentDrawCallCount = 0;
	}

	// Determines if the cluster can be safely culled performing quick cone-based test on the CPU.
	// Since the triangle filtering kernel operates with 2 views in the same pass, this method must
	// only cull those clusters that are not visible from ANY of the views (camera and shadow views).
	bool cullCluster(const Cluster* cluster, vec3 eyes[gNumViews])
	{
		// Invalid clusters can't be safely culled using the cone based test
		if (cluster->valid)
		{
			uint visibility = 0;
			for (uint32_t i = 0; i < gNumViews; i++)
			{
				// We move camera position into object space
				vec3 testVec = normalize(eyes[i] - f3Tov3(cluster->coneCenter));

				// Check if we are inside the cone
				if (dot(testVec, f3Tov3(cluster->coneAxis)) < cluster->coneAngleCosine)
					visibility |= (1 << i);
			}
			return (visibility == 0);
		}
		return false;
	}

	static inline int genClipMask(__m128 v)
	{
		//this checks a vertex against the 6 planes, and stores if they are inside
		// or outside of the plane

		//w contains the w component of the vector in all 4 slots
		const __m128 w0 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 3));
		const __m128 w1 = _mm_shuffle_ps(v, _mm_setzero_ps(), _MM_SHUFFLE(3, 3, 3, 3));

		//subtract the vector from w, and store in a
		const __m128 a = _mm_sub_ps(w0, v);
		//add the vector to w, and store in b
		const __m128 b = _mm_add_ps(w1, v);

		//compare if a and b are less than zero,
		// and store the result in fmaska, and fmaskk
		const __m128 fmaska = _mm_cmplt_ps(a, _mm_setzero_ps());
		const __m128 fmaskb = _mm_cmplt_ps(b, _mm_setzero_ps());

		//convert those masks to integers, and spread the bits using pdep
		//const int maska = _pdep_u32(_mm_movemask_ps(fmaska), 0x55);
		//const int maskb = _pdep_u32(_mm_movemask_ps(fmaskb), 0xAA);
		const int maska = pdep_lut[(_mm_movemask_ps(fmaska) & 0x7)];
		const int maskb = pdep_lut[(_mm_movemask_ps(fmaskb) & 0x7)] << 1;

		//or the masks together and and the together with all bits set to 1
		// NOTE only the bits 0x3f are actually used
		return (maska | maskb) & 0x3f;
	}

	//static inline uint32_t genClipMask(float4 f)
	//{
	//  uint32_t result = 0;
	//
	//  //X
	//  if (f.x <= f.w)  result |=  0x1;
	//  if (f.x >= -f.w) result |=  0x2;
	//
	//  //Y
	//  if (f.y <= f.w)  result |=  0x4;
	//  if (f.y >= -f.w) result |=  0x8;
	//
	//  //Z
	//  if (f.z <= f.w)  result |= 0x10;
	//  if (f.z >= 0)   result |= 0x20;
	//  return result;
	//}

	void sortClusters(Cluster** clusters, uint32_t len)
	{
		struct StackItem
		{
			Cluster** a;
			uint32_t  l;
		};
		StackItem stack[512];
		int       stackidx = 0;

		Cluster** current_a = clusters;
		uint32_t  current_l = len;

		for (;;)
		{
			Cluster* pivot = current_a[current_l / 2];

			int i, j;
			for (i = 0, j = current_l - 1;; ++i, --j)
			{
				while (current_a[i]->distanceFromCamera < pivot->distanceFromCamera)
					i++;
				while (current_a[j]->distanceFromCamera > pivot->distanceFromCamera)
					j--;

				if (i >= j)
					break;

				Cluster* temp = current_a[i];
				current_a[i] = current_a[j];
				current_a[j] = temp;
			}

			if (i > 1)
			{
				stack[stackidx].a = current_a;
				stack[stackidx++].l = i;
			}
			if (current_l - i > 1)
			{
				stack[stackidx].a = current_a + i;
				stack[stackidx++].l = current_l - i;
			}

			if (stackidx == 0)
				break;

			--stackidx;
			current_a = stack[stackidx].a;
			current_l = stack[stackidx].l;
		}
	}

	// This function decides how to do the triangle filtering pass, depending on the flags (hold, filter triangles)
	// - filterTriangles: enables / disables triangle filtering at all. Disabling filtering makes the CPU to set the buffer states to render the whole scene.
	// - hold: bypasses any triangle filtering step. This is useful to inspect the filtered geometry from another viewpoint.
	// This function first performs CPU based cluster culling and only runs the fine-grained GPU-based tests for those clusters that passed the CPU test
	void triangleFilteringPass(Cmd* cmd, ProfileToken nGpuProfileToken, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, "Triangle Filtering Pass");

		if (pRenderer->pActiveGpuSettings->mIndirectCommandBuffer)
		{
			CommandSignature resetCmd = {};
			resetCmd.mDrawType = INDIRECT_COMMAND_BUFFER_RESET;
			for (uint32_t g = 0; g < gNumGeomSets; ++g)
			{
				for (uint32_t v = 0; v < NUM_CULLING_VIEWPORTS; ++v)
				{
					cmdExecuteIndirect(cmd, &resetCmd, MAX_DRAWS_INDIRECT, pFilteredIndirectDrawArgumentsBuffer[frameIdx][g][v], 0, NULL, 0);
				}
			}
		}

		gPerFrame[frameIdx].gTotalClusters = 0;
		gPerFrame[frameIdx].gCulledClusters = 0;

		/************************************************************************/
		// Barriers to transition uncompacted draw buffer to uav
		/************************************************************************/
		BufferBarrier uavBarriers[gNumViews] = {};
		for (uint32_t i = 0; i < gNumViews; ++i)
			uavBarriers[i] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, gNumViews, uavBarriers, 0, NULL, 0, NULL);
		/************************************************************************/
		// Clear previous indirect arguments
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, "Clear Buffers");
		cmdBindPipeline(cmd, pPipelineClearBuffers);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTriangleFiltering[0]);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 0, pDescriptorSetTriangleFiltering[1]);
		uint32_t numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, "Clear Buffers Synchronization");
		uint32_t numBarriers = (gNumViews * gNumGeomSets) + gNumViews;
		BufferBarrier* clearBarriers = (BufferBarrier*)alloca(numBarriers * sizeof(BufferBarrier));
		uint32_t index = 0;
		for (uint32_t i = 0; i < gNumViews; ++i)
		{
			clearBarriers[index++] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
		}
		cmdResourceBarrier(cmd, numBarriers, clearBarriers, 0, NULL, 0, NULL);
		cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/
		uint32_t currentSmallBatchChunk = 0;
		uint accumDrawCount = 0;
		uint accumNumTriangles = 0;
		uint accumNumTrianglesAtStartOfBatch = 0;
		uint batchStart = 0;

		cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, "Filter Triangles");
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 1, pDescriptorSetTriangleFiltering[1]);

		uint64_t size = BATCH_COUNT * sizeof(SmallBatchData) * gSmallBatchChunkCount;
		GPURingBufferOffset offset = getGPURingBufferOffset(pFilterBatchDataBuffer, (uint32_t)size, (uint32_t)size);
		BufferUpdateDesc updateDesc = { offset.pBuffer, offset.mOffset };
		beginUpdateResource(&updateDesc);

		FilterBatchData* batches = (FilterBatchData*)updateDesc.pMappedData;
		FilterBatchData* origin = batches;

		for (uint32_t i = 0; i < gMeshCount; ++i)
		{
			ClusterContainer* drawBatch = &pMeshes[i];
			FilterBatchChunk* batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
			for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
			{
				++gPerFrame[frameIdx].gTotalClusters;
				const ClusterCompact* clusterCompactInfo = &drawBatch->clusterCompacts[j];
				// Run cluster culling
				if (!gAppSettings.mClusterCulling || !cullCluster(&drawBatch->clusters[j], gPerFrame[frameIdx].gEyeObjectSpace))
				{
					// cluster culling passed or is turned off
					// We will now add the cluster to the batch to be triangle filtered
					addClusterToBatchChunk(clusterCompactInfo, batchStart, accumDrawCount, accumNumTrianglesAtStartOfBatch, i, batchChunk, batches);
					accumNumTriangles += clusterCompactInfo->triangleCount;
				}

				// check to see if we filled the batch
				if (batchChunk->currentBatchCount >= BATCH_COUNT)
				{
					uint32_t batchCount = batchChunk->currentBatchCount;
					++accumDrawCount;

					// run the triangle filtering and switch to the next small batch chunk
					filterTriangles(cmd, frameIdx, batchChunk, offset.pBuffer, (batches - origin) * sizeof(FilterBatchData));
					batches += batchCount;
					currentSmallBatchChunk = (currentSmallBatchChunk + 1) % gSmallBatchChunkCount;
					batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];

					batchStart = 0;
					accumNumTrianglesAtStartOfBatch = accumNumTriangles;
				}
			}

			// end of that mash, set it up so we can add the next mesh to this culling batch
			if (batchChunk->currentBatchCount > 0)
			{
				FilterBatchChunk* batchChunk2 = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
				++accumDrawCount;

				batchStart = batchChunk2->currentBatchCount;
				accumNumTrianglesAtStartOfBatch = accumNumTriangles;
			}
		}

		gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = accumDrawCount;
		gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = accumDrawCount;
		gPerFrame[frameIdx].gTotalDrawCount = accumDrawCount * 2;
		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx][currentSmallBatchChunk],
			offset.pBuffer, (batches - origin) * sizeof(FilterBatchData));
		endUpdateResource(&updateDesc, NULL);
		cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		for (uint32_t i = 0; i < gNumViews; ++i)
			uavBarriers[i] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, gNumViews, uavBarriers, 0, NULL, 0, NULL);
		/************************************************************************/
		// Batch compaction
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, nGpuProfileToken, "Batch Compaction");
		cmdBindPipeline(cmd, pPipelineBatchCompaction);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 2, pDescriptorSetTriangleFiltering[1]);
		numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
		/************************************************************************/
		/************************************************************************/
		cmdEndGpuTimestampQuery(cmd, nGpuProfileToken);
	}

	// This is the main scene rendering function. It shows the different steps / rendering passes.
	void drawScene(Cmd* cmd, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "Shadow Pass");

		RenderTargetBarrier rtBarriers[] = {
			{ pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			{ pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE }
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

		drawShadowMapPass(cmd, gGpuProfileTokens[0], frameIdx);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);

		cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "VB Filling Pass");
		drawVisibilityBufferPass(cmd, gGpuProfileTokens[0], frameIdx);
		
		const int barrierCount = 3;
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
			mat4 projection = gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_CAMERA].projection;
			mat4 view = gPerFrame[frameIdx].gCameraView;
			mat4 invView = inverse(view);
			aura::mat4 inverseView = aura::convertMatrix4FromClient((float*)&invView);

			RenderTarget* lpvVisualisationRenderTarget = pScreenRenderTarget;

			aura::drawLpvVisualization(cmd, pRenderer, pAura, lpvVisualisationRenderTarget, pDepthBuffer, aura::convertMatrix4FromClient((float*)&projection), aura::convertMatrix4FromClient((float*)&view), inverseView, gDebugVisualizationSettings.cascadeIndex, gDebugVisualizationSettings.probeRadius);
			cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
		}
	
	}

	void LoadSkybox()
	{
		Texture*          pPanoSkybox = NULL;
		Shader*           pPanoToCubeShader = NULL;
		RootSignature*    pPanoToCubeRootSignature = NULL;
		Pipeline*         pPanoToCubePipeline = NULL;
		DescriptorSet*    pDescriptorSetPanoToCube[2] = { NULL };

		Sampler* pSkyboxSampler = NULL;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, false, 0.0f, 0.0f, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		SyncToken token = {};
		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pName = "skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pSkybox;
		addResource(&skyboxLoadDesc, &token);

		// Load the skybox panorama texture.
		TextureLoadDesc panoDesc = {};
		panoDesc.pFileName = "daytime";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, &token);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0 };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;

		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);

		DescriptorSetDesc setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[0]);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[1]);

		PipelineDesc pipelineDesc = {};
		pipelineDesc.pCache = pPipelineCache;
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = pipelineDesc.mComputeDesc;
		pipelineSettings = { 0 };
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &pipelineDesc, &pPanoToCubePipeline);

		waitForToken(&token);

		// Since this happens on iniatilization, use the first cmd/fence pair available.
		Cmd* cmd = pCmds[0];

		// Compute the BRDF Integration map.
		beginCmd(cmd);

		DescriptorData params[2] = {};

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(cmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetPanoToCube[0], 1, params);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetPanoToCube[0]);

		struct Data
		{
			uint mip;
			uint textureSize;
		} data = { 0, gSkyboxSize };

		uint32_t rootConstantIndex = getDescriptorIndexFromName(pPanoToCubeRootSignature, "RootConstant");

		for (uint32_t i = 0; i < gSkyboxMips; i++)
		{
			data.mip = i;
			cmdBindPushConstants(cmd, pPanoToCubeRootSignature, rootConstantIndex, &data);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pSkybox;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetPanoToCube[1], 1, params);
			cmdBindDescriptorSet(cmd, i, pDescriptorSetPanoToCube[1]);

			const uint32_t* pThreadGroupSize = pPanoToCubeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pSkybox, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers, 0, NULL);

		endCmd(cmd);

		waitForAllResourceLoads();

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.pSignalFence = pTransitionFences;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitForFences(pRenderer, 1, &pTransitionFences);

		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[1]);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);
		removePipeline(pRenderer, pPanoToCubePipeline);

		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);
	}

	void drawSkybox(Cmd* cmd, int frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileTokens[0], "Draw Skybox");

		// Load RT instead of Don't care
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

		// Draw the skybox
		const uint32_t stride = sizeof(float) * 4;
		cmdBindPipeline(cmd, pSkyboxPipeline);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetSkybox[1]);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, &stride, NULL);

		cmdDraw(cmd, 36, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, gGpuProfileTokens[0]);
	}

	// Draw GUI / 2D elements
    void drawGUI(Cmd* cmd, uint32_t frameIdx)
    {
        UNREF_PARAM(frameIdx);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);

        if (gAppSettings.mAsyncCompute)
        {
            if (gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
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

#if TEST_RSM
        if (pAuraRSMTexturesWindow)
        {
			appUIGui(pAppUI, pAuraRSMTexturesWindow);
        }
#endif

		cmdDrawUserInterface(cmd);

        cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
    }

	void SetupGuiWindows(float2 screenSize)
	{
		SetupAppSettingsWindow(screenSize);
		SetupAuraSettingsWindow(screenSize);
		SetupDebugTexturesWindow(screenSize);
		SetupDebugShadowTexturesWindow(screenSize);
		SetupDebugVisualizationWindow(screenSize);

		#if TEST_RSM
		SetupDebugRSMTexturesWindow(screenSize);
		#endif
	}

	void SetupAppSettingsWindow(float2 screenSize)
	{
		UIComponentDesc UIComponentDesc = {};
		UIComponentDesc.mStartPosition.setY(screenSize.getY() / 20.0f);
		uiCreateComponent("App Settings", &UIComponentDesc, &pGuiWindow);

		CheckboxWidget filterTriangles;
		filterTriangles.pData = &gAppSettings.mFilterTriangles;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Triangle Filtering", &filterTriangles, WIDGET_TYPE_CHECKBOX));

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
		//const float maxAngleOffset = 0.017f;
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
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraSpeedProp, WIDGET_TYPE_SLIDER_FLOAT));

		// Enable this to customize flag movement if the flags dont move as expected in a newer San Miguel model
#ifdef TEST_FLAG_SETTINGS
		CollapsingHeaderWidget CollapsingFlagSettings;

		SliderFloatWidget anchorH;
		anchorH.pData = &gFlagAnchorHeight;
		anchorH.mMin = -1000.0f;
		anchorH.mMax = 1000.0f;
		addCollapsingHeaderSubWidget(&CollapsingFlagSettings, "Anchor H", &anchorH, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget randO;
		randO.pData = &gWindRandomOffset;
		randO.mMin = 0.0f;
		randO.mMax = 200.0f;
		addCollapsingHeaderSubWidget(&CollapsingFlagSettings, "Rand O", &randO, WIDGET_TYPE_SLIDER_FLOAT);

		SliderFloatWidget randF;
		randF.pData = &gWindRandomFactor;
		randF.mMin = 0.0f;
		randF.mMax = 5000.0f;
		addCollapsingHeaderSubWidget(&CollapsingFlagSettings, "Rand F", &randF, WIDGET_TYPE_SLIDER_FLOAT);

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
		CheckboxWidget checkbox;
		SliderFloatWidget sliderFloat;

#ifdef ENABLE_CPU_PROPAGATION
		checkbox.pData = &gAppSettings.useCPUPropagation;
		luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "CPU Propagation", &checkbox, WIDGET_TYPE_CHECKBOX));
#endif
		//pAuraGuiWindow->AddWidget(UIProperty("CPU Asynchronous Mapping", gAppSettings.useCPUAsyncMapping));
		checkbox.pData = &gAppSettings.alternateGPUUpdates;
		luaRegisterWidget(uiCreateComponentWidget(pAuraGuiWindow, "Alternate GPU Updates", &checkbox, WIDGET_TYPE_CHECKBOX));

		//pAuraGuiWindow->AddWidget(UIProperty("Multiple Bounces", useMultipleReflections)); // NOTE: broken! It's also broken in the original LightPropagationVolumes in the "middleware" repo.
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
		float scale = 0.15f;
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
		float scale = 0.15f;
		float2 texSize = screenSize * scale;

		if (!pAuraRSMTexturesWindow)
		{
			UIComponentDesc UIComponentDesc = {};
			UIComponentDesc.mStartSize = vec2(UIComponentDesc.mStartSize.getX(), UIComponentDesc.mStartSize.getY());
			UIComponentDesc.mStartPosition = vec2(screenSize.getX() * 0.15f, screenSize.getY() * 0.4f);
			pAuraRSMTexturesWindow = uiCreateComponent(pAppUI, "RSM Textures", &UIComponentDesc);
			ASSERT(pAuraRSMTexturesWindow);

			DebugTexturesWidget widget;
			luaRegisterWidget(uiCreateComponentWidget(pAuraRSMTexturesWindow, "RSM Textures", &widget, WIDGET_TYPE_DEBUG_TEXTURES));
		}

		eastl::vector<Texture*> pRSMTextures;
		for (uint32_t i = 0; i < gRSMCascadeCount; i++)
		{
			pRSMTextures.push_back(pRenderTargetRSMAlbedo[i]->pTexture);
		}

		((DebugTexturesWidget*)(pAuraRSMTexturesWindow->mWidgets[0]->pWidget))->mTextures = pRSMTextures;
		((DebugTexturesWidget*)(pAuraRSMTexturesWindow->mWidgets[0]->pWidget))->mTextureDisplaySize = texSize;
	}
#endif

	void calculateRSMCascadesForLPV(const mat4& lightView, const int& cascadeCount, mat4* cascadeProjections, mat4* cascadeTransforms, float* viewSize)
	{
		aura::Box boundsLightspace[gRSMCascadeCount] = {};
		aura::mat4 worldToLight = aura::convertMatrix4FromClient((float*)&lightView);
		aura::getGridBounds(pAura, worldToLight, boundsLightspace);

		for (int i = 0; i < cascadeCount; i++)
		{
			aura::Box cascadeBoundsLightspace = boundsLightspace[i];
			//float farPlane = cascadeBoundsLightspace.vMax.z - cascadeBoundsLightspace.vMin.z;

			float longestDiagonal = sqrt(3.0f) * gLPVCascadeSpans[i];

			float size = longestDiagonal * ((float)gRSMResolution)/((float)(gRSMResolution - 1));
			float texelSize = size / ((float)gRSMResolution);

			vec3 lightSpacePosition = vec3((cascadeBoundsLightspace.vMin.x + cascadeBoundsLightspace.vMax.x) * 0.5f, 
										   (cascadeBoundsLightspace.vMin.y + cascadeBoundsLightspace.vMax.y) * 0.5f, 
										   cascadeBoundsLightspace.vMin.z);
			vec3 lightSpaceBottomLeft = vec3(lightSpacePosition.getX() - size * 0.5f, lightSpacePosition.getY() - size * 0.5f, lightSpacePosition.getZ());

			lightSpaceBottomLeft.setX(floor(lightSpaceBottomLeft.getX() / texelSize) * texelSize);
			lightSpaceBottomLeft.setY(floor(lightSpaceBottomLeft.getY() / texelSize) * texelSize);
			lightSpacePosition = lightSpaceBottomLeft + vec3(size * 0.5f, size * 0.5f, 0.0f);
			
			mat4 translation = mat4::translation(-(inverse(lightView) * vec4(lightSpacePosition, 1)).getXYZ());
			mat4 worldToCascade = lightView * translation;

			float halfSize = size * 0.5f;
			cascadeProjections[i] = mat4::orthographic(-halfSize, halfSize, -halfSize, halfSize, -5000.0f, 5000.0f);
			cascadeTransforms[i] = worldToCascade;
			viewSize[i] = size;
		}
	}	
};

DEFINE_APPLICATION_MAIN(AuraApp)
