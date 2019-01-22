/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"

#include "../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../../../Common_3/Renderer/IRenderer.h"
#include "../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../Common_3/OS/Core/RingBuffer.h"
#include "../../../Common_3/OS/Image/Image.h"
#include "../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../Common_3/OS/Interfaces/IApp.h"

#include "../../../Middleware_3/UI/AppUI.h"
#include "../../../Common_3/OS/Core/DebugRenderer.h"

#include "Geometry.h"

#include "../../../Common_3/OS/Interfaces/IMemoryManager.h"

#if defined(_DURANGO)
#define BEGINALLOCATION(X) esramBeginAllocation(pRenderer->pESRAMManager, X, esramGetCurrentOffset(pRenderer->pESRAMManager))
#define ENDALLOCATION(X) esramEndAllocation(pRenderer->pESRAMManager)
#define DIRECT3D12
#else
#define BEGINALLOCATION(X)
#define ENDALLOCATION(X)
#endif

#if !defined(METAL)
#define MSAASAMPLECOUNT 2
#else
#define MSAASAMPLECOUNT 1
#endif

File       mFile;
FileSystem mFileSystem;
LogManager mLogManager;
HiresTimer gTimer;

#if defined(__linux__)
//_countof is MSVS macro, add define for Linux. a is expected to be static array type
#define _countof(a) ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

#define SCENE_SCALE 1.0f

const char* pszBases[FSR_Count] = {
	"../../../src/",                               // FSR_BinShaders
	"../../../src/",                               // FSR_SrcShaders
	"../../../../../Art/SanMiguel_3/Textures/",    // FSR_Textures
	"../../../../../Art/SanMiguel_3/Meshes/",      // FSR_Meshes
	"../../../Resources/",                         // FSR_Builtin_Fonts
	"../../../src/",                               // FSR_GpuConfig
	"",                                            // FSR_Animation
	"../../../Resources/",                         // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",           // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",             // FSR_MIDDLEWARE_UI
};

#ifdef _DURANGO
#include "../../../Xbox/CommonXBOXOne_3/Renderer/Direct3D12/Direct3D12X.h"
#endif

// Rendering modes
typedef enum RenderMode
{
	RENDERMODE_VISBUFF = 0,
	RENDERMODE_DEFERRED = 1,
	RENDERMODE_COUNT = 2
} RenderMode;

enum
{
	DEFERRED_RT_ALBEDO = 0,
	DEFERRED_RT_NORMAL,
	DEFERRED_RT_SPECULAR,
	DEFERRED_RT_SIMULATION,

	DEFERRED_RT_COUNT
};

typedef enum OutputMode
{
	OUTPUT_MODE_SDR = 0,
	OUTPUT_MODE_HDR10,

	OUTPUT_MODE_COUNT
} OutputMode;

typedef enum LightingMode
{
	LIGHTING_PHONG = 0,
	LIGHTING_PBR = 1,

	LIGHTINGMODE_COUNT = 2
} LightingMode;

struct RootConstantData
{
	float4 lightColor;
	uint   lightingMode;
	uint   outputMode;
	float4 CameraPlane;    //x : near, y : far
};

RootConstantData gRootConstantDrawsceneData;

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

struct GodrayInfo
{
	float exposure;
	float decay;
	float density;
	float weight;

	float2 lightPosInSS;

	uint NUM_SAMPLES;
};

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

//static const DisplayChromacities DisplayChromacityList[] =
//{
//	{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Display Gamut Rec709
//	{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // Display Gamut Rec2020
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Display Gamut P3D65
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31400f, 0.35100f }, // Display Gamut P3DCI(Theater)
//	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.32168f, 0.33767f }, // Display Gamut P3D60(ACES Cinema)
//};

//Camera Walking
static float            cameraWalkingTime = 0.0f;
tinystl::vector<float3> positions_directions;
float3                  CameraPathData[29084];

uint  cameraPoints;
float totalElpasedTime;

typedef struct VisBufferIndirectCommand
{
	// Metal does not use index buffer since there is no builtin primitive id
#if defined(METAL)
	IndirectDrawArguments arg;
#else
	// Draw ID is sent as indirect argument through root constant in DX12
#if defined(DIRECT3D12)
	uint32_t                   drawId;
#endif
	IndirectDrawIndexArguments arg;
#if defined(DIRECT3D12)
	uint32_t                   _pad0, _pad1;
#else
	uint32_t _pad0, _pad1, _pad2;
#endif
#endif
} VisBufferIndirectCommand;

#if defined(METAL)
// Constant data updated for every batch
struct PerBatchConstants
{
	uint32_t drawId;      // used to idendify the batch from the shader
	uint32_t twoSided;    // possible values: 0/1
};
#endif

/************************************************************************/
// GUI CONTROLS
/************************************************************************/
typedef struct AppSettings
{
	OutputMode mOutputMode = OUTPUT_MODE_SDR;

	// Current active rendering mode
	RenderMode mRenderMode = RENDERMODE_VISBUFF;

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

#if MSAASAMPLECOUNT == 1
	bool mDrawDebugTargets = false;
#else
	bool                       mDrawDebugTargets = false;
#endif

	float nearPlane = 10.0f;
	float farPlane = 3000.0f;

	// adjust directional sunlight angle
	float2 mSunControl = { -2.1f, 0.164f };

	float mSunSize = 300.0f;

	float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

	DynamicUIControls mDynamicUIControlsGR;
	GodrayInfo        gGodrayInfo;
	bool              mEnableGodray = true;
	uint              gGodrayInteration = 3;

	float mEsmControl = 80.0f;

	float mRetinaScaling = 1.5f;

	// HDAO data
	DynamicUIControls mDynamicUIControlsAO;
	bool              mEnableHDAO = true;

	float mRejectRadius = 5.2f;
	float mAcceptRadius = 0.12f;
	float mAOIntensity = 3.0f;
	int   mAOQuality = 2;

	DynamicUIControls mSCurve;
	float             SCurveScaleFactor = 1.0f;
	float             SCurveSMin = 0.00f;
	float             SCurveSMid = 0.84f;
	float             SCurveSMax = 65.65f;
	float             SCurveTMin = 0.0f;
	float             SCurveTMid = 139.76f;
	float             SCurveTMax = 1100.0f;
	float             SCurveSlopeFactor = 2.2f;

	DynamicUIControls mLinearScale;
	float             LinearScale = 140.0f;

	// HDR10
	DynamicUIControls mDisplaySetting;

	DisplayColorSpace  mCurrentSwapChainColorSpace = ColorSpace_Rec2020;
	DisplayColorRange  mDisplayColorRange = ColorRange_RGB;
	DisplaySignalRange mDisplaySignalRange = Display_SIGNAL_RANGE_FULL;

	CurveConversionMode mCurveConversionMode = CurveConversion_SCurve;

	float MaxOutputNits = 1000.0f;
	float MinOutputNits = 0.03f;
	float MaxCLL = 1000.0f;
	float MaxFALL = 20.0f;

	//Camera Walking
	bool  cameraWalking = false;
	float cameraWalkingSpeed = 1.0f;

} AppSettings;

/************************************************************************/
// Constants
/************************************************************************/
const char* gSceneName = "SanMiguel.obj";
const char* gSunName = "sun.obj";

// Number of in-flight buffers
const uint32_t gImageCount = 3;
uint32_t       gPresentFrameIdx = ~0u;

bool gToggleVSync = false;

// Constants
const uint32_t gShadowMapSize = 1024;
const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

struct UniformDataSkybox
{
	mat4 mProjectView;
	vec3 mCamPos;
};

UniformDataSkybox gUniformDataSky;
const uint32_t    gSkyboxSize = 1024;
const uint32_t    gSkyboxMips = 9;

struct UniformDataSunMatrices
{
	mat4 mProjectView;
	mat4 mModelMat;
	vec4 mLightColor;
};

mat4 SunMVP;
mat4 SunModel;

UniformDataSunMatrices gUniformDataSunMatrices;
int                    gGodrayScale = 8;

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = 2;
const uint32_t GEOMSET_OPAQUE = 0;
const uint32_t GEOMSET_ALPHATESTED = 1;

// Memory budget used for internal ring buffer
const uint32_t gMemoryBudget = 1024ULL * 1024ULL * 64ULL;
/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
	// Stores the camera/eye position in object space for cluster culling
	vec3              gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
	PerFrameConstants gPerFrameUniformData = {};

	// These are just used for statistical information
	uint32_t gTotalClusters = 0;
	uint32_t gCulledClusters = 0;
	uint32_t gDrawCount[gNumGeomSets];
};

/************************************************************************/
// Settings
/************************************************************************/
AppSettings gAppSettings;
/************************************************************************/
// Rendering data
/************************************************************************/
Renderer* pRenderer = nullptr;
/************************************************************************/
// Synchronization primitives
/************************************************************************/
Fence*     pTransitionFences = nullptr;
Fence*     pRenderCompleteFences[gImageCount] = { nullptr };
Fence*     pComputeCompleteFences[gImageCount] = { nullptr };
Semaphore* pImageAcquiredSemaphore = nullptr;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { nullptr };
Semaphore* pComputeCompleteSemaphores[gImageCount] = { nullptr };
/************************************************************************/
// Queues and Command buffers
/************************************************************************/
Queue*   pGraphicsQueue = nullptr;
CmdPool* pCmdPool = nullptr;
Cmd**    ppCmds = nullptr;

Queue*   pComputeQueue = nullptr;
CmdPool* pComputeCmdPool = nullptr;
Cmd**    ppComputeCmds = nullptr;
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain* pSwapChain = nullptr;
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*        pShaderClearBuffers = nullptr;
Pipeline*      pPipelineClearBuffers = nullptr;
RootSignature* pRootSignatureClearBuffers = nullptr;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*        pShaderTriangleFiltering = nullptr;
Pipeline*      pPipelineTriangleFiltering = nullptr;
RootSignature* pRootSignatureTriangleFiltering = nullptr;
/************************************************************************/
// Batch compaction pipeline
/************************************************************************/
#if !defined(METAL)
Shader*        pShaderBatchCompaction = nullptr;
Pipeline*      pPipelineBatchCompaction = nullptr;
RootSignature* pRootSignatureBatchCompaction = nullptr;
#endif
/************************************************************************/
// Clear light clusters pipeline
/************************************************************************/
Shader*        pShaderClearLightClusters = nullptr;
Pipeline*      pPipelineClearLightClusters = nullptr;
RootSignature* pRootSignatureClearLightClusters = nullptr;
/************************************************************************/
// Compute light clusters pipeline
/************************************************************************/
Shader*        pShaderClusterLights = nullptr;
Pipeline*      pPipelineClusterLights = nullptr;
RootSignature* pRootSignatureClusterLights = nullptr;
/************************************************************************/
// Shadow pass pipeline
/************************************************************************/
Shader*   pShaderShadowPass[gNumGeomSets] = { nullptr };
Pipeline* pPipelineShadowPass[gNumGeomSets] = { nullptr };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferPass[gNumGeomSets] = {};
Pipeline*         pPipelineVisibilityBufferPass[gNumGeomSets] = {};
RootSignature*    pRootSignatureVBPass = nullptr;
CommandSignature* pCmdSignatureVBPass = nullptr;
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*        pShaderVisibilityBufferShade[2] = { nullptr };
Pipeline*      pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
RootSignature* pRootSignatureVBShade = nullptr;
/************************************************************************/
// Deferred pass pipeline
/************************************************************************/
Shader*           pShaderDeferredPass[gNumGeomSets] = {};
Pipeline*         pPipelineDeferredPass[gNumGeomSets] = {};
RootSignature*    pRootSignatureDeferredPass = nullptr;
CommandSignature* pCmdSignatureDeferredPass = nullptr;
/************************************************************************/
// Deferred shade pipeline
/************************************************************************/
Shader*        pShaderDeferredShade[2] = { nullptr };
Pipeline*      pPipelineDeferredShadeSrgb[2] = { nullptr };
RootSignature* pRootSignatureDeferredShade = nullptr;
/************************************************************************/
// Deferred point light shade pipeline
/************************************************************************/
Shader*        pShaderDeferredShadePointLight = nullptr;
Pipeline*      pPipelineDeferredShadePointLightSrgb = nullptr;
RootSignature* pRootSignatureDeferredShadePointLight = nullptr;
/************************************************************************/
// AO pipeline
/************************************************************************/
Shader*        pShaderAO[4] = { nullptr };
Pipeline*      pPipelineAO[4] = { nullptr };
RootSignature* pRootSignatureAO = nullptr;
/************************************************************************/
// Resolve pipeline
/************************************************************************/
Shader*        pShaderResolve = nullptr;
Pipeline*      pPipelineResolve = nullptr;
Pipeline*      pPipelineResolvePost = nullptr;
RootSignature* pRootSignatureResolve = nullptr;

Shader*        pShaderGodrayResolve = nullptr;
Pipeline*      pPipelineGodrayResolve = nullptr;
Pipeline*      pPipelineGodrayResolvePost = nullptr;
RootSignature* pRootSignatureGodrayResolve = nullptr;
/************************************************************************/
// Skybox pipeline
/************************************************************************/
Shader*        pShaderSkybox = nullptr;
Pipeline*      pSkyboxPipeline = nullptr;
RootSignature* pRootSingatureSkybox = nullptr;

Buffer*  pSkyboxVertexBuffer = NULL;
Texture* pSkybox = NULL;
/************************************************************************/
// Godray pipeline
/************************************************************************/
Shader*        pSunPass = nullptr;
Pipeline*      pPipelineSunPass = nullptr;
RootSignature* pRootSigSunPass = nullptr;

Shader*        pGodRayPass = nullptr;
Pipeline*      pPipelineGodRayPass = nullptr;
RootSignature* pRootSigGodRayPass = nullptr;

uint SunVertexCount;
uint SunIndexCount;

Buffer* pSunVertexBuffer = NULL;
Buffer* pSunIndexBuffer = NULL;
/************************************************************************/
// Curve Conversion pipeline
/************************************************************************/
Shader*        pShaderCurveConversion = nullptr;
Pipeline*      pPipelineCurveConversionPass = nullptr;
RootSignature* pRootSigCurveConversionPass = nullptr;

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

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pDepthBuffer = nullptr;
RenderTarget* pRenderTargetVBPass = nullptr;
RenderTarget* pRenderTargetMSAA = nullptr;
RenderTarget* pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { nullptr };
RenderTarget* pRenderTargetShadow = nullptr;
RenderTarget* pRenderTargetAO = nullptr;
RenderTarget* pIntermediateRenderTarget = nullptr;
RenderTarget* pRenderTargetSun = nullptr;
RenderTarget* pRenderTargetSunResolved = nullptr;
RenderTarget* pRenderTargetGodRayA = nullptr;
RenderTarget* pRenderTargetGodRayB = nullptr;
RenderTarget* pCurveConversionRenderTarget = nullptr;
/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullBack = nullptr;
RasterizerState* pRasterizerStateCullFront = nullptr;
RasterizerState* pRasterizerStateCullNone = nullptr;
RasterizerState* pRasterizerStateCullBackMS = nullptr;
RasterizerState* pRasterizerStateCullFrontMS = nullptr;
RasterizerState* pRasterizerStateCullNoneMS = nullptr;
/************************************************************************/
// Depth states
/************************************************************************/
DepthState* pDepthStateEnable = nullptr;
DepthState* pDepthStateDisable = nullptr;
/************************************************************************/
// Blend state used in deferred point light shading
/************************************************************************/
BlendState* pBlendStateOneZero = nullptr;
BlendState* pBlendStateSkyBox = nullptr;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerTrilinearAniso = nullptr;
Sampler* pSamplerBilinear = nullptr;
Sampler* pSamplerPointClamp = nullptr;
Sampler* pSamplerBilinearClamp = nullptr;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture* gDiffuseMapsStorage = NULL;
Texture* gNormalMapsStorage = NULL;
Texture* gSpecularMapsStorage = NULL;

tinystl::vector<Texture*> gDiffuseMaps;
tinystl::vector<Texture*> gNormalMaps;
tinystl::vector<Texture*> gSpecularMaps;

tinystl::vector<Texture*> gDiffuseMapsPacked;
tinystl::vector<Texture*> gNormalMapsPacked;
tinystl::vector<Texture*> gSpecularMapsPacked;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
Buffer* pVertexBufferPosition = nullptr;
Buffer* pVertexBufferTexCoord = nullptr;
Buffer* pVertexBufferNormal = nullptr;
Buffer* pVertexBufferTangent = nullptr;
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer* pMaterialPropertyBuffer = nullptr;
Buffer* pPerFrameUniformBuffers[gImageCount] = { nullptr };

#if defined(METAL)
// Buffer containing all indirect draw commands for all geometry sets (no culling)
Buffer* pIndirectDrawArgumentsBufferAll = nullptr;
Buffer* pIndirectMaterialBufferAll = nullptr;
// Buffer containing filtered indirect draw commands for all geometry sets (culled)
Buffer* pFilteredIndirectDrawArgumentsBuffer[gImageCount][gNumViews] = {};
#else
// Buffers containing all indirect draw commands per geometry set (no culling)
Buffer* pIndirectDrawArgumentsBufferAll[gNumGeomSets] = { nullptr };
Buffer* pIndirectMaterialBufferAll = nullptr;
Buffer* pMeshConstantsBuffer = nullptr;
// Buffers containing filtered indirect draw commands per geometry set (culled)
Buffer* pFilteredIndirectDrawArgumentsBuffer[gImageCount][gNumGeomSets][gNumViews] = { nullptr };
// Buffer containing the draw args after triangle culling which will be stored compactly in the indirect buffer
Buffer* pUncompactedDrawArgumentsBuffer[gImageCount][gNumViews] = { nullptr };
Buffer* pFilterIndirectMaterialBuffer[gImageCount] = { nullptr };
#endif
/************************************************************************/
// Index buffers
/************************************************************************/
Buffer* pIndexBufferAll = nullptr;
Buffer* pFilteredIndexBuffer[gImageCount][gNumViews] = {};
/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*       pLightsBuffer = nullptr;
Buffer**      gPerBatchUniformBuffers = nullptr;
Buffer*       pVertexBufferCube = nullptr;
Buffer*       pIndexBufferCube = nullptr;
Buffer*       pLightClustersCount[gImageCount] = { nullptr };
Buffer*       pLightClusters[gImageCount] = { nullptr };
Buffer*       pUniformBufferSun[gImageCount] = { nullptr };
uint64_t      gFrameCount = 0;
Scene*        pScene = nullptr;
UIApp         gAppUI;
GuiComponent* pGuiWindow = nullptr;
TextDrawDesc  gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
/************************************************************************/
// Triangle filtering data
/************************************************************************/
#if defined(METAL)
FilterBatchChunk* pFilterBatchChunk[gImageCount] = { nullptr };
#else
const uint32_t gSmallBatchChunkCount = max(1U, 512U / CLUSTER_SIZE) * 16U;
FilterBatchChunk* pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = { nullptr };
UniformRingBuffer* pFilterBatchDataBuffer[gImageCount] = { nullptr };
#endif
/************************************************************************/
// GPU Profilers
/************************************************************************/
GpuProfiler*       pGraphicsGpuProfiler = nullptr;
GpuProfiler*       pComputeGpuProfiler = nullptr;
ICameraController* pCameraController = nullptr;
/************************************************************************/
// CPU staging data
/************************************************************************/
// CPU buffers for light data
LightData gLightData[LIGHT_COUNT] = {};

PerFrameData  gPerFrame[gImageCount] = {};
RenderTarget* pScreenRenderTarget = nullptr;
/************************************************************************/
// Screen resolution UI data
/************************************************************************/
#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
IWidget*                    gResolutionProperty = nullptr;
tinystl::vector<Resolution> gResolutions;
uint32_t                    gResolutionIndex = 0;
bool                        gResolutionChange = false;
#endif
/************************************************************************/
/************************************************************************/
class VisibilityBuffer* pVisibilityBuffer = nullptr;
/************************************************************************/
// Culling intrinsic data
/************************************************************************/
const uint32_t pdep_lut[8] = { 0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15 };
/************************************************************************/
// App implementation
/************************************************************************/
IWidget* addResolutionProperty(
	GuiComponent* pUIManager, uint32_t& resolutionIndex, uint32_t resCount, Resolution* pResolutions, WidgetCallback onResolutionChanged)
{
#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
	if (pUIManager)
	{
		struct ResolutionData
		{
			tinystl::vector<tinystl::string> resNameContainer;
			tinystl::vector<const char*>     resNamePointers;
			tinystl::vector<uint32_t>        resValues;
		};

		static tinystl::unordered_map<GuiComponent*, ResolutionData> guiResolution;
		ResolutionData&                                              data = guiResolution[pUIManager];

		data.resNameContainer.clear();
		data.resNamePointers.clear();
		data.resValues.clear();

		for (uint32_t i = 0; i < resCount; ++i)
		{
			data.resNameContainer.push_back(tinystl::string::format("%ux%u", pResolutions[i].mWidth, pResolutions[i].mHeight));
			data.resValues.push_back(i);
		}

		data.resNamePointers.resize(data.resNameContainer.size() + 1);
		for (uint32_t i = 0; i < (uint32_t)data.resNameContainer.size(); ++i)
		{
			data.resNamePointers[i] = data.resNameContainer[i];
		}
		data.resNamePointers[data.resNamePointers.size() - 1] = nullptr;

		DropdownWidget control(
			"Screen Resolution", &resolutionIndex, data.resNamePointers.data(), data.resValues.data(), (uint32_t)data.resValues.size());
		control.pOnEdited = onResolutionChanged;
		return pUIManager->AddWidget(control);
	}

#endif
	return NULL;
}

class VisibilityBuffer: public IApp
{
	public:
#if defined(DIRECT3D12)
	void SetHDRMetaData(float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
	{
#if !defined(_DURANGO)
		// Clean the hdr metadata if the display doesn't support HDR

		DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

		if (gAppSettings.mCurrentSwapChainColorSpace == ColorSpace_Rec2020)
		{
			colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		}
		else if (gAppSettings.mCurrentSwapChainColorSpace == ColorSpace_P3D65)
		{
		}

		UINT colorSpaceSupport = 0;
		if (SUCCEEDED(pSwapChain->pDxSwapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) &&
			((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
		{
			pSwapChain->pDxSwapChain->SetColorSpace1(colorSpace);
		}
#endif
	}
#endif

	bool Init()
	{
		// Overwrite rootpath is required because Textures and meshes are not in /Textures and /Meshes.
		// We need to set the modified root path so that filesystem can find the meshes and textures.
		FileSystem::SetRootPath(FSRoot::FSR_Meshes, "/");
		FileSystem::SetRootPath(FSRoot::FSR_Textures, "/");

		pVisibilityBuffer = this;
		/************************************************************************/
		// Initialize the Forge renderer with the appropriate parameters.
		/************************************************************************/
		RendererDesc settings = {};
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		//Camera Walking
		tinystl::string fn("cameraPath.bin");
		mFile.Open(fn, FM_ReadBinary, FSR_OtherFiles);
		mFile.Read(CameraPathData, sizeof(float3) * 29084);
		mFile.Close();

		cameraPoints = (uint)29084 / 2;
		totalElpasedTime = (float)cameraPoints * 0.00833f;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		// Create the command pool and the command lists used to store GPU commands.
		// One Cmd list per back buffer image is stored for triple buffering.
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		QueueDesc computeQueueDesc = {};
		computeQueueDesc.mType = CMD_POOL_COMPUTE;
		addQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

		// Create the command pool and the command lists used to store GPU commands.
		// One Cmd list per back buffer image is stored for triple buffering.
		addCmdPool(pRenderer, pComputeQueue, false, &pComputeCmdPool);
		addCmd_n(pComputeCmdPool, false, gImageCount, &ppComputeCmds);

		addFence(pRenderer, &pTransitionFences);

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);
		/************************************************************************/
		// Initialize helper interfaces (resource loader, profiler)
		/************************************************************************/
		initResourceLoaderInterface(pRenderer, gMemoryBudget);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		addGpuProfiler(pRenderer, pGraphicsQueue, &pGraphicsGpuProfiler);
		addGpuProfiler(pRenderer, pComputeQueue, &pComputeGpuProfiler);
		/************************************************************************/
		// Start timing the scene load
		/************************************************************************/
		HiresTimer timer;

		HiresTimer shaderTimer;
		// Load shaders
		addShaders();
		LOGINFOF("Load shaders : %f ms", shaderTimer.GetUSec(true) / 1000.0f);
		/************************************************************************/
		// Setup default depth, blend, rasterizer, sampler states
		/************************************************************************/
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		DepthStateDesc depthStateDisableDesc = {};
		addDepthState(pRenderer, &depthStateDesc, &pDepthStateEnable);
		addDepthState(pRenderer, &depthStateDisableDesc, &pDepthStateDisable);

		RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };
		RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
		RasterizerStateDesc rasterizerStateCullBackDesc = { CULL_MODE_BACK };
		RasterizerStateDesc rasterizerStateCullFrontMsDesc = { CULL_MODE_FRONT, 0, 0, FILL_MODE_SOLID, true };
		RasterizerStateDesc rasterizerStateCullNoneMsDesc = { CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID, true };
		RasterizerStateDesc rasterizerStateCullBackMsDesc = { CULL_MODE_BACK, 0, 0, FILL_MODE_SOLID, true };
		addRasterizerState(pRenderer, &rasterizerStateCullFrontDesc, &pRasterizerStateCullFront);
		addRasterizerState(pRenderer, &rasterizerStateCullNoneDesc, &pRasterizerStateCullNone);
		addRasterizerState(pRenderer, &rasterizerStateCullBackDesc, &pRasterizerStateCullBack);
		addRasterizerState(pRenderer, &rasterizerStateCullFrontMsDesc, &pRasterizerStateCullFrontMS);
		addRasterizerState(pRenderer, &rasterizerStateCullNoneMsDesc, &pRasterizerStateCullNoneMS);
		addRasterizerState(pRenderer, &rasterizerStateCullBackMsDesc, &pRasterizerStateCullBackMS);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateDesc.mSrcFactors[0] = BC_ONE;
		blendStateDesc.mDstFactors[0] = BC_ONE;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateDesc, &pBlendStateOneZero);

		BlendStateDesc blendStateSkyBoxDesc = {};
		blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
		blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;

		blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
		blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;

		blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;

		blendStateSkyBoxDesc.mMasks[0] = ALL;
		blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateSkyBoxDesc, &pBlendStateSkyBox);

		// Create sampler for VB render target
		SamplerDesc trilinearDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, 8.0f
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

		addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
		addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
		addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
		addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);

		/************************************************************************/
		// Load resources for skybox
		/************************************************************************/
		LoadSkybox();

		/************************************************************************/
		// Load the scene using the SceneLoader class, which uses Assimp
		/************************************************************************/
		HiresTimer      sceneLoadTimer;
		tinystl::string sceneFullPath = FileSystem::FixPath(gSceneName, FSRoot::FSR_Meshes);
		pScene = loadScene(sceneFullPath.c_str(), 50.0f, -20.0f, 0.0f, 0.0f);
		if (!pScene)
			return false;
		LOGINFOF("Load assimp scene : %f ms", sceneLoadTimer.GetUSec(true) / 1000.0f);
		/************************************************************************/
		// IA buffers
		/************************************************************************/
		HiresTimer bufferLoadTimer;

#if !defined(METAL)
		// Default (non-filtered) index buffer for the scene
		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		ibDesc.mDesc.mElementCount = pScene->totalTriangles * 3;
		ibDesc.mDesc.mStructStride = sizeof(uint32_t);
		ibDesc.mDesc.mSize = ibDesc.mDesc.mElementCount * ibDesc.mDesc.mStructStride;
		ibDesc.pData = pScene->indices.data();
		ibDesc.ppBuffer = &pIndexBufferAll;
		ibDesc.mDesc.pDebugName = L"Non-filtered Index Buffer Desc";
		addResource(&ibDesc);
#else
		// Fill the pIndexBufferAll with triangle IDs for the whole scene (since metal implementation doesn't use scene indices).
		uint32_t* trianglesBuffer = (uint32_t*)conf_malloc(pScene->totalTriangles * 3 * sizeof(uint32_t));
		for (uint32_t i = 0, t = 0; i < pScene->numMeshes; i++)
		{
			for (uint32_t j = 0; j < pScene->meshes[i].triangleCount; j++, t++)
				trianglesBuffer[t] = j;
		}
		BufferLoadDesc ibDesc = {};
		ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		ibDesc.mDesc.mElementCount = pScene->totalTriangles * 3;
		ibDesc.mDesc.mStructStride = sizeof(uint32_t);
		ibDesc.mDesc.mSize = ibDesc.mDesc.mElementCount * ibDesc.mDesc.mStructStride;
		ibDesc.pData = trianglesBuffer;
		ibDesc.ppBuffer = &pIndexBufferAll;
		ibDesc.mDesc.pDebugName = L"Non-filtered Index Buffer Desc";
		addResource(&ibDesc);

		conf_free(trianglesBuffer);
#endif

		// Vertex position buffer for the scene
		BufferLoadDesc vbPosDesc = {};
		vbPosDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbPosDesc.mDesc.mVertexStride = sizeof(SceneVertexPos);
		vbPosDesc.mDesc.mElementCount = pScene->totalVertices;
		vbPosDesc.mDesc.mStructStride = sizeof(SceneVertexPos);
		vbPosDesc.mDesc.mSize = vbPosDesc.mDesc.mElementCount * vbPosDesc.mDesc.mStructStride;
		vbPosDesc.pData = pScene->positions.data();
		vbPosDesc.ppBuffer = &pVertexBufferPosition;
		vbPosDesc.mDesc.pDebugName = L"Vertex Position Buffer Desc";
		addResource(&vbPosDesc);

		// Vertex texcoord buffer for the scene
		BufferLoadDesc vbTexCoordDesc = {};
		vbTexCoordDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		vbTexCoordDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbTexCoordDesc.mDesc.mVertexStride = sizeof(SceneVertexTexCoord);
		vbTexCoordDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexTexCoord) / sizeof(uint32_t));
		vbTexCoordDesc.mDesc.mStructStride = sizeof(uint32_t);
		vbTexCoordDesc.mDesc.mSize = vbTexCoordDesc.mDesc.mElementCount * vbTexCoordDesc.mDesc.mStructStride;
		vbTexCoordDesc.pData = pScene->texCoords.data();
		vbTexCoordDesc.ppBuffer = &pVertexBufferTexCoord;
		vbTexCoordDesc.mDesc.pDebugName = L"Vertex TexCoord Buffer Desc";
		addResource(&vbTexCoordDesc);

		// Vertex normal buffer for the scene
		BufferLoadDesc vbNormalDesc = {};
		vbNormalDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		vbNormalDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbNormalDesc.mDesc.mVertexStride = sizeof(SceneVertexNormal);
		vbNormalDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexNormal) / sizeof(uint32_t));
		vbNormalDesc.mDesc.mStructStride = sizeof(uint32_t);
		vbNormalDesc.mDesc.mSize = vbNormalDesc.mDesc.mElementCount * vbNormalDesc.mDesc.mStructStride;
		vbNormalDesc.pData = pScene->normals.data();
		vbNormalDesc.ppBuffer = &pVertexBufferNormal;
		vbNormalDesc.mDesc.pDebugName = L"Vertex Normal Buffer Desc";
		addResource(&vbNormalDesc);

		// Vertex tangent buffer for the scene
		BufferLoadDesc vbTangentDesc = {};
		vbTangentDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		vbTangentDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vbTangentDesc.mDesc.mVertexStride = sizeof(SceneVertexTangent);
		vbTangentDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexTangent) / sizeof(uint32_t));
		vbTangentDesc.mDesc.mStructStride = sizeof(uint32_t);
		vbTangentDesc.mDesc.mSize = vbTangentDesc.mDesc.mElementCount * vbTangentDesc.mDesc.mStructStride;
		vbTangentDesc.pData = pScene->tangents.data();
		vbTangentDesc.ppBuffer = &pVertexBufferTangent;
		vbTangentDesc.mDesc.pDebugName = L"Vertex Tangent Buffer Desc";
		addResource(&vbTangentDesc);

		LOGINFOF("Load scene buffers : %f ms", bufferLoadTimer.GetUSec(true) / 1000.0f);
		/************************************************************************/
		// Cluster creation
		/************************************************************************/
		HiresTimer clusterTimer;
		// Calculate clusters
		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			MeshIn*   mesh = pScene->meshes + i;
			Material* material = pScene->materials + mesh->materialId;
			CreateClusters(material->twoSided, pScene, mesh);
		}
		LOGINFOF("Load clusters : %f ms", clusterTimer.GetUSec(true) / 1000.0f);
		/************************************************************************/
		// Texture loading
		/************************************************************************/
		HiresTimer textureLoadTimer;
		gDiffuseMaps = tinystl::vector<Texture*>(pScene->numMaterials);
		gNormalMaps = tinystl::vector<Texture*>(pScene->numMaterials);
		gSpecularMaps = tinystl::vector<Texture*>(pScene->numMaterials);

		for (uint32_t i = 0; i < pScene->numMaterials; ++i)
		{
			TextureLoadDesc diffuse = {};
			diffuse.pFilename = pScene->textures[i];
			diffuse.mRoot = FSR_Textures;
			diffuse.mUseMipmaps = true;
			diffuse.ppTexture = &gDiffuseMaps[i];
			diffuse.mSrgb = false;
			addResource(&diffuse);

			TextureLoadDesc normal = {};
			normal.pFilename = pScene->normalMaps[i];
			normal.mRoot = FSR_Textures;
			normal.mUseMipmaps = true;
			normal.ppTexture = &gNormalMaps[i];
			addResource(&normal);

			TextureLoadDesc specular = {};
			specular.pFilename = pScene->specularMaps[i];
			specular.mRoot = FSR_Textures;
			specular.mUseMipmaps = true;
			specular.ppTexture = &gSpecularMaps[i];
			addResource(&specular);
		}

		LOGINFOF("Load textures : %f ms", textureLoadTimer.GetUSec(true) / 1000.0f);
		/************************************************************************/
		// Setup root signatures
		/************************************************************************/
		// Graphics root signatures
		const char* pTextureSamplerName = "textureFilter";
		const char* pShadingSamplerNames[] = { "depthSampler", "textureSampler" };
		Sampler*    pShadingSamplers[] = { pSamplerBilinearClamp, pSamplerBilinear };
		const char* pAoSamplerName = "g_SamplePoint";

		Shader* pShaders[gNumGeomSets * 2] = {};
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			pShaders[i * 2] = pShaderVisibilityBufferPass[i];
			pShaders[i * 2 + 1] = pShaderShadowPass[i];
		}
		RootSignatureDesc vbRootDesc = { pShaders, gNumGeomSets * 2 };
		vbRootDesc.mMaxBindlessTextures = pScene->numMaterials;
		vbRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
		vbRootDesc.ppStaticSamplers = &pSamplerPointClamp;
		vbRootDesc.mStaticSamplerCount = 1;
		addRootSignature(pRenderer, &vbRootDesc, &pRootSignatureVBPass);

		RootSignatureDesc deferredPassRootDesc = { pShaderDeferredPass, gNumGeomSets };
		deferredPassRootDesc.mMaxBindlessTextures = pScene->numMaterials;
		deferredPassRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
		deferredPassRootDesc.ppStaticSamplers = &pSamplerTrilinearAniso;
		deferredPassRootDesc.mStaticSamplerCount = 1;
		addRootSignature(pRenderer, &deferredPassRootDesc, &pRootSignatureDeferredPass);

		RootSignatureDesc shadeRootDesc = { pShaderVisibilityBufferShade, 2 };
		// Set max number of bindless textures in the root signature
		shadeRootDesc.mMaxBindlessTextures = pScene->numMaterials;
		shadeRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
		shadeRootDesc.ppStaticSamplers = pShadingSamplers;
		shadeRootDesc.mStaticSamplerCount = 2;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureVBShade);

		shadeRootDesc.ppShaders = pShaderDeferredShade;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureDeferredShade);

		shadeRootDesc.ppShaders = &pShaderDeferredShadePointLight;
		shadeRootDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureDeferredShadePointLight);

		RootSignatureDesc aoRootDesc = { pShaderAO, 4 };
		aoRootDesc.ppStaticSamplerNames = &pAoSamplerName;
		aoRootDesc.ppStaticSamplers = &pSamplerPointClamp;
		aoRootDesc.mStaticSamplerCount = 1;
		addRootSignature(pRenderer, &aoRootDesc, &pRootSignatureAO);

		RootSignatureDesc resolveRootDesc = { &pShaderResolve, 1 };
		addRootSignature(pRenderer, &resolveRootDesc, &pRootSignatureResolve);

		RootSignatureDesc resolveGodrayRootDesc = { &pShaderGodrayResolve, 1 };
		addRootSignature(pRenderer, &resolveGodrayRootDesc, &pRootSignatureGodrayResolve);

		// Triangle filtering root signatures
		RootSignatureDesc clearBuffersRootDesc = { &pShaderClearBuffers, 1 };
		addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);
		RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
#if defined(VULKAN)
		const char* pBatchBufferName = "batchData";
		triangleFilteringRootDesc.mDynamicUniformBufferCount = 1;
		triangleFilteringRootDesc.ppDynamicUniformBufferNames = &pBatchBufferName;
#endif
		addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);

#if !defined(METAL)
		RootSignatureDesc batchCompactionRootDesc = { &pShaderBatchCompaction, 1 };
		addRootSignature(pRenderer, &batchCompactionRootDesc, &pRootSignatureBatchCompaction);
#endif

		RootSignatureDesc clearLightRootDesc = { &pShaderClearLightClusters, 1 };
		addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureClearLightClusters);
		RootSignatureDesc clusterRootDesc = { &pShaderClusterLights, 1 };
		addRootSignature(pRenderer, &clusterRootDesc, &pRootSignatureClusterLights);

		RootSignatureDesc CurveConversionRootSigDesc = { &pShaderCurveConversion, 1 };
		addRootSignature(pRenderer, &CurveConversionRootSigDesc, &pRootSigCurveConversionPass);

		RootSignatureDesc sunPassShaderRootSigDesc = { &pSunPass, 1 };
		addRootSignature(pRenderer, &sunPassShaderRootSigDesc, &pRootSigSunPass);

		RootSignatureDesc godrayPassShaderRootSigDesc = { &pGodRayPass, 1 };
		addRootSignature(pRenderer, &godrayPassShaderRootSigDesc, &pRootSigGodRayPass);

		RootSignatureDesc finalShaderRootSigDesc = { &pShaderPresentPass, 1 };
		addRootSignature(pRenderer, &finalShaderRootSigDesc, &pRootSigPresentPass);

		/************************************************************************/
		// Setup indirect command signatures
		/************************************************************************/
#if defined(DIRECT3D12)
		const DescriptorInfo*      pDrawId = NULL;
		IndirectArgumentDescriptor indirectArgs[2] = {};
		indirectArgs[0].mType = INDIRECT_CONSTANT;
		indirectArgs[0].mCount = 1;
		indirectArgs[1].mType = INDIRECT_DRAW_INDEX;

		CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 2, indirectArgs };
		pDrawId =
			&pRootSignatureVBPass->pDescriptors[pRootSignatureVBPass->pDescriptorNameToIndexMap[tinystl::hash("indirectRootConstant")]];
		indirectArgs[0].mRootParameterIndex = pRootSignatureVBPass->pDxRootConstantRootIndices[pDrawId->mIndexInParent];
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);

		CommandSignatureDesc deferredPassDesc = { pCmdPool, pRootSignatureDeferredPass, 2, indirectArgs };
		pDrawId = &pRootSignatureDeferredPass
					   ->pDescriptors[pRootSignatureDeferredPass->pDescriptorNameToIndexMap[tinystl::hash("indirectRootConstant")]];
		indirectArgs[0].mRootParameterIndex = pRootSignatureDeferredPass->pDxRootConstantRootIndices[pDrawId->mIndexInParent];
		addIndirectCommandSignature(pRenderer, &deferredPassDesc, &pCmdSignatureDeferredPass);
#else
		// Indicate the renderer that we want to use non-indexed geometry. We can't use indices because Metal doesn't provide triangle ID built-in
		// variable in the pixel shader. So, the only way to workaround this is to replicate vertices and use vertex_id from the vertex shader to
		// calculate the triangle ID (triangleId = vertexId / 3). This is not the optimal approach but works as a workaround.
		IndirectArgumentDescriptor indirectArgs[1] = {};
#if defined(METAL)
		indirectArgs[0].mType = INDIRECT_DRAW;
#else
		indirectArgs[0].mType = INDIRECT_DRAW_INDEX;
#endif
		CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 1, indirectArgs };
		CommandSignatureDesc deferredPassDesc = { pCmdPool, pRootSignatureDeferredPass, 1, indirectArgs };
		addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
		addIndirectCommandSignature(pRenderer, &deferredPassDesc, &pCmdSignatureDeferredPass);
#endif

		// Create geometry for light rendering
		createCubeBuffers(pRenderer, pCmdPool, &pVertexBufferCube, &pIndexBufferCube);
		/************************************************************************/
		// Setup compute pipelines for triangle filtering
		/************************************************************************/
		ComputePipelineDesc pipelineDesc = { pShaderClearBuffers, pRootSignatureClearBuffers };
		addComputePipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

		// Create the compute pipeline for GPU triangle filtering
		pipelineDesc = { pShaderTriangleFiltering, pRootSignatureTriangleFiltering };
		addComputePipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

#ifndef METAL
		pipelineDesc = { pShaderBatchCompaction, pRootSignatureBatchCompaction };
		addComputePipeline(pRenderer, &pipelineDesc, &pPipelineBatchCompaction);
#endif

		// Setup the clearing light clusters pipeline
		pipelineDesc = { pShaderClearLightClusters, pRootSignatureClearLightClusters };
		addComputePipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

		// Setup the compute the light clusters pipeline
		pipelineDesc = { pShaderClusterLights, pRootSignatureClusterLights };
		addComputePipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);
		/************************************************************************/
		// Setup the UI components for text rendering, UI controls...
		/************************************************************************/
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(225.0f, 100.0f);
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);

#if !defined(TARGET_IOS)
		CheckboxWidget vsyncProp("Toggle VSync", &gToggleVSync);
		pGuiWindow->AddWidget(vsyncProp);
#endif

#if !defined(METAL)
		static const char*      outputModeNames[] = { "SDR", "HDR10 (DirectX 12 Only)", nullptr };
		static const OutputMode outputModeValues[] = {
			OUTPUT_MODE_SDR,
			OUTPUT_MODE_HDR10,
		};

		DropdownWidget outputMode("Output Mode", (uint32_t*)&gAppSettings.mOutputMode, outputModeNames, (uint32_t*)outputModeValues, 2U);
		pGuiWindow->AddWidget(outputMode);
#else
		static const char* outputModeNames[] = { "SDR", nullptr };
		static const OutputMode outputModeValues[] = { OUTPUT_MODE_SDR };

		DropdownWidget outputMode("Output Mode", (uint32_t*)&gAppSettings.mOutputMode, outputModeNames, (uint32_t*)outputModeValues, 1U);
		pGuiWindow->AddWidget(outputMode);
#endif

		static const char* lightingModeNames[] = { "Phong", "Physically Based Rendering", nullptr };

		static const LightingMode lightingModeValues[] = {
			LIGHTING_PHONG,
			LIGHTING_PBR,
		};

		DropdownWidget lightingMode(
			"Lighting Mode", (uint32_t*)&gAppSettings.mLightingMode, lightingModeNames, (uint32_t*)lightingModeValues, 2U);
		pGuiWindow->AddWidget(lightingMode);

		CheckboxWidget cameraProp("Cinematic Camera walking", &gAppSettings.cameraWalking);
		pGuiWindow->AddWidget(cameraProp);

		SliderFloatWidget cameraSpeedProp("Cinematic Camera walking: Speed", &gAppSettings.cameraWalkingSpeed, 0.0f, 3.0f);
		pGuiWindow->AddWidget(cameraSpeedProp);

		// Light Settings
		//---------------------------------------------------------------------------------
		// offset max angle for sun control so the light won't bleed with
		// small glancing angles, i.e., when lightDir is almost parallel to the plane

		SliderFloat2Widget sunX("Sun Control", &gAppSettings.mSunControl, float2(-PI), float2(PI), float2(0.001f));
		pGuiWindow->AddWidget(sunX);

		gAppSettings.gGodrayInfo.exposure = 0.06f;
		gAppSettings.gGodrayInfo.decay = 0.9f;
		gAppSettings.gGodrayInfo.density = 2.0f;
		gAppSettings.gGodrayInfo.weight = 1.4f;
		gAppSettings.gGodrayInfo.NUM_SAMPLES = 80;

		SliderFloat4Widget lightColorUI("Light Color & Intensity", &gAppSettings.mLightColor, 0.0f, 30.0f, 0.01f);
		pGuiWindow->AddWidget(lightColorUI);

		CheckboxWidget toggleGR("Enable Godray", &gAppSettings.mEnableGodray);
		pGuiWindow->AddWidget(toggleGR);

		tinystl::vector<IWidget*>& dynamicPropsGR = gAppSettings.mDynamicUIControlsGR.mDynamicProperties;    // shorthand
		dynamicPropsGR.emplace_back(SliderFloatWidget("God Ray : Sun Size", &gAppSettings.mSunSize, 1.0f, 1000.0f).Clone());
		dynamicPropsGR.emplace_back(SliderFloatWidget("God Ray: Exposure", &gAppSettings.gGodrayInfo.exposure, 0.0f, 0.1f, 0.001f).Clone());
		dynamicPropsGR.emplace_back(SliderUintWidget("God Ray: Quality", &gAppSettings.gGodrayInteration, 1, 4).Clone());

		if (gAppSettings.mEnableGodray)
			gAppSettings.mDynamicUIControlsGR.ShowDynamicProperties(pGuiWindow);

		//SliderFloatWidget esm("Shadow Control", &gAppSettings.mEsmControl, 0, 200.0f);
		//pGuiWindow->AddWidget(esm);

		CheckboxWidget localLight("Enable Random Point Lights", &gAppSettings.mRenderLocalLights);
		pGuiWindow->AddWidget(localLight);

#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
		Resolution wantedResolutions[] = { { 3840, 2160 }, { 1920, 1080 }, { 1280, 720 }, { 1024, 768 } };
		gResolutions.emplace_back(getMonitor(0)->defaultResolution);
		for (uint32_t i = 0; i < sizeof(wantedResolutions) / sizeof(wantedResolutions[0]); ++i)
		{
			bool duplicate = false;
			for (uint32_t j = 0; j < (uint32_t)gResolutions.size(); ++j)
			{
				if (wantedResolutions[i].mWidth == gResolutions[j].mWidth && wantedResolutions[i].mHeight == gResolutions[j].mHeight)
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate && getResolutionSupport(getMonitor(0), &wantedResolutions[i]))
			{
				gResolutions.emplace_back(wantedResolutions[i]);
			}
		}
		gResolutionProperty = addResolutionProperty(
			pGuiWindow, gResolutionIndex, (uint32_t)gResolutions.size(), gResolutions.data(), []() { gResolutionChange = true; });
#endif
		/************************************************************************/
		// Rendering Settings
		/************************************************************************/
		static const char*      renderModeNames[] = { "Visibility Buffer", "Deferred Shading", nullptr };
		static const RenderMode renderModeValues[] = {
			RENDERMODE_VISBUFF,
			RENDERMODE_DEFERRED,
		};
		DropdownWidget renderMode("Render Mode", (uint32_t*)&gAppSettings.mRenderMode, renderModeNames, (uint32_t*)renderModeValues, 2U);
		pGuiWindow->AddWidget(renderMode);

		static const char* displayColorRangeNames[] = { "RGB", nullptr };

		static const DisplayColorRange displayColorRangeValues[] = { ColorRange_RGB };

		static const char* displaySignalRangeNames[] = { "Range Full", "Range Limited", nullptr };

		static const DisplaySignalRange displaySignalRangeValues[] = { Display_SIGNAL_RANGE_FULL, Display_SIGNAL_RANGE_NARROW };

		static const char* displayColorSpaceNames[] = { "ColorSpace Rec709", "ColorSpace Rec2020", "ColorSpace P3D65", nullptr };

		static const DisplayColorSpace displayColorSpaceValues[] = { ColorSpace_Rec709, ColorSpace_Rec2020, ColorSpace_P3D65 };

		tinystl::vector<IWidget*>& displaySetting = gAppSettings.mDisplaySetting.mDynamicProperties;
		displaySetting.emplace_back(DropdownWidget(
										"Display Color Range", (uint32_t*)&gAppSettings.mDisplayColorRange, displayColorRangeNames,
										(uint32_t*)displayColorRangeValues, 1U)
										.Clone());
		displaySetting.emplace_back(DropdownWidget(
										"Display Signal Range", (uint32_t*)&gAppSettings.mDisplaySignalRange, displaySignalRangeNames,
										(uint32_t*)displaySignalRangeValues, 2U)
										.Clone());
		displaySetting.emplace_back(DropdownWidget(
										"Display Color Space", (uint32_t*)&gAppSettings.mCurrentSwapChainColorSpace, displayColorSpaceNames,
										(uint32_t*)displayColorSpaceValues, 3U)
										.Clone());

		CheckboxWidget holdProp("Hold filtered results", &gAppSettings.mHoldFilteredResults);
		pGuiWindow->AddWidget(holdProp);

		CheckboxWidget filtering("Triangle Filtering", &gAppSettings.mFilterTriangles);
		pGuiWindow->AddWidget(filtering);

		CheckboxWidget cluster("Cluster Culling", &gAppSettings.mClusterCulling);
		pGuiWindow->AddWidget(cluster);

		CheckboxWidget asyncCompute("Async Compute", &gAppSettings.mAsyncCompute);
		pGuiWindow->AddWidget(asyncCompute);

#if MSAASAMPLECOUNT == 1
		CheckboxWidget debugTargets("Draw Debug Targets", &gAppSettings.mDrawDebugTargets);
		pGuiWindow->AddWidget(debugTargets);
#endif
		/************************************************************************/
		// HDAO Settings
		/************************************************************************/
		CheckboxWidget toggleAO("Enable HDAO", &gAppSettings.mEnableHDAO);
		pGuiWindow->AddWidget(toggleAO);

		tinystl::vector<IWidget*>& dynamicPropsAO = gAppSettings.mDynamicUIControlsAO.mDynamicProperties;    // shorthand
		dynamicPropsAO.emplace_back(SliderFloatWidget("AO accept radius", &gAppSettings.mAcceptRadius, 0, 10).Clone());
		dynamicPropsAO.emplace_back(SliderFloatWidget("AO reject radius", &gAppSettings.mRejectRadius, 0, 10).Clone());
		dynamicPropsAO.emplace_back(SliderFloatWidget("AO intensity radius", &gAppSettings.mAOIntensity, 0, 10).Clone());
		dynamicPropsAO.emplace_back(SliderIntWidget("AO Quality", &gAppSettings.mAOQuality, 1, 4).Clone());
		if (gAppSettings.mEnableHDAO)
			gAppSettings.mDynamicUIControlsAO.ShowDynamicProperties(pGuiWindow);

		static const char* curveConversionModeNames[] = { "Linear Scale", "Scurve", nullptr };

		static const CurveConversionMode curveConversionValues[] = { CurveConversion_LinearScale, CurveConversion_SCurve };

		DropdownWidget curveConversionMode(
			"Curve Conversion", (uint32_t*)&gAppSettings.mCurveConversionMode, curveConversionModeNames, (uint32_t*)curveConversionValues,
			2U);
		pGuiWindow->AddWidget(curveConversionMode);

		tinystl::vector<IWidget*>& dynamicPropsLinearScale = gAppSettings.mLinearScale.mDynamicProperties;
		dynamicPropsLinearScale.emplace_back(SliderFloatWidget("Linear Scale", &gAppSettings.LinearScale, 0, 300.0f).Clone());

		if (gAppSettings.mCurveConversionMode == CurveConversion_LinearScale)
		{
			gAppSettings.mLinearScale.ShowDynamicProperties(pGuiWindow);
		}

		tinystl::vector<IWidget*>& dynamicPropsSCurve = gAppSettings.mSCurve.mDynamicProperties;    // shorthand
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: Scale Factor", &gAppSettings.SCurveScaleFactor, 0, 10.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: SMin", &gAppSettings.SCurveSMin, 0, 2.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: SMid", &gAppSettings.SCurveSMid, 0, 20.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: SMax", &gAppSettings.SCurveSMax, 0, 100.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: TMin", &gAppSettings.SCurveTMin, 0, 10.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: TMid", &gAppSettings.SCurveTMid, 0, 300.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: TMax", &gAppSettings.SCurveTMax, 0, 4000.0f).Clone());
		dynamicPropsSCurve.emplace_back(SliderFloatWidget("SCurve: Slope Factor", &gAppSettings.SCurveSlopeFactor, 0, 3.0f).Clone());

		if (gAppSettings.mOutputMode != OutputMode::OUTPUT_MODE_SDR && gAppSettings.mCurveConversionMode == CurveConversion_SCurve)
		{
			gAppSettings.mSCurve.ShowDynamicProperties(pGuiWindow);
			gSCurveInfomation.UseSCurve = 1.0f;
		}

#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
		if (!pWindow->fullScreen)
			pGuiWindow->RemoveWidget(gResolutionProperty);
#endif
		/************************************************************************/
		// Setup the fps camera for navigating through the scene
		/************************************************************************/
		vec3                   startPosition(600.0f, 490.0f, 70.0f);
		vec3                   startLookAt = startPosition + vec3(-1.0f - 0.0f, 0.1f, 0.0f);
		CameraMotionParameters camParams;
		camParams.acceleration = 1300 * 2.5f;
		camParams.braking = 1300 * 2.5f;
		camParams.maxSpeed = 200 * 2.5f;
		pCameraController = createFpsCameraController(startPosition, startLookAt);
		pCameraController->setMotionParameters(camParams);
		requestMouseCapture(true);
		/************************************************************************/
		/************************************************************************/
		// Finish the resource loading process since the next code depends on the loaded resources
		finishResourceLoading();

		gDiffuseMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gDiffuseMaps.size());
		gNormalMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gNormalMaps.size());
		gSpecularMapsStorage = (Texture*)conf_malloc(sizeof(Texture) * gSpecularMaps.size());

		for (uint32_t i = 0; i < (uint32_t)gDiffuseMaps.size(); ++i)
		{
			memcpy(&gDiffuseMapsStorage[i], gDiffuseMaps[i], sizeof(Texture));
			gDiffuseMapsPacked.push_back(&gDiffuseMapsStorage[i]);
		}
		for (uint32_t i = 0; i < (uint32_t)gDiffuseMaps.size(); ++i)
		{
			memcpy(&gNormalMapsStorage[i], gNormalMaps[i], sizeof(Texture));
			gNormalMapsPacked.push_back(&gNormalMapsStorage[i]);
		}
		for (uint32_t i = 0; i < (uint32_t)gDiffuseMaps.size(); ++i)
		{
			memcpy(&gSpecularMapsStorage[i], gSpecularMaps[i], sizeof(Texture));
			gSpecularMapsPacked.push_back(&gSpecularMapsStorage[i]);
		}

		HiresTimer setupBuffersTimer;
		addTriangleFilteringBuffers();

		LOGINFOF("Setup buffers : %f ms", setupBuffersTimer.GetUSec(true) / 1000.0f);

#ifdef _DURANGO
		// When async compute is on, we need to transition some resources in the graphics queue
		// because they can't be transitioned by the compute queue (incompatible)
		if (gAppSettings.mAsyncCompute)
			setResourcesToComputeCompliantState(0, true);
#endif

		LOGINFOF("Total Load Time : %f ms", timer.GetUSec(true) / 1000.0f);

		InputSystem::RegisterInputEvent(onInputEventHandler);

		return true;
	}

	void Exit()
	{
		removeResource(pSkybox);

		removeTriangleFilteringBuffers();

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

		gAppSettings.mDynamicUIControlsAO.Destroy();
		gAppUI.Exit();

		// Destroy geometry for light rendering
		destroyBuffers(pRenderer, pVertexBufferCube, pIndexBufferCube);

		// Destroy triangle filtering pipelines
		removePipeline(pRenderer, pPipelineClusterLights);
		removePipeline(pRenderer, pPipelineClearLightClusters);
		removePipeline(pRenderer, pPipelineTriangleFiltering);
#if !defined(METAL)
		removePipeline(pRenderer, pPipelineBatchCompaction);
#endif
		removePipeline(pRenderer, pPipelineClearBuffers);

		removeRootSignature(pRenderer, pRootSignatureResolve);
		removeRootSignature(pRenderer, pRootSignatureGodrayResolve);
		removeRootSignature(pRenderer, pRootSignatureAO);

		removeRootSignature(pRenderer, pRootSingatureSkybox);

		removeRootSignature(pRenderer, pRootSigSunPass);
		removeRootSignature(pRenderer, pRootSigGodRayPass);
		removeRootSignature(pRenderer, pRootSigCurveConversionPass);

		removeRootSignature(pRenderer, pRootSigPresentPass);

		removeRootSignature(pRenderer, pRootSignatureClusterLights);
		removeRootSignature(pRenderer, pRootSignatureClearLightClusters);
#if !defined(METAL)
		removeRootSignature(pRenderer, pRootSignatureBatchCompaction);
#endif
		removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
		removeRootSignature(pRenderer, pRootSignatureClearBuffers);
		removeRootSignature(pRenderer, pRootSignatureDeferredShadePointLight);
		removeRootSignature(pRenderer, pRootSignatureDeferredShade);
		removeRootSignature(pRenderer, pRootSignatureDeferredPass);
		removeRootSignature(pRenderer, pRootSignatureVBShade);
		removeRootSignature(pRenderer, pRootSignatureVBPass);

		removeIndirectCommandSignature(pRenderer, pCmdSignatureDeferredPass);
		removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
		/************************************************************************/
		// Remove loaded scene
		/************************************************************************/
		// Destroy scene buffers
#if !defined(METAL)
		removeResource(pIndexBufferAll);
#endif
		removeResource(pVertexBufferPosition);
		removeResource(pVertexBufferTexCoord);
		removeResource(pVertexBufferNormal);
		removeResource(pVertexBufferTangent);

		removeResource(pSunVertexBuffer);
		removeResource(pSunIndexBuffer);
		removeResource(pSkyboxVertexBuffer);

		// Destroy clusters
		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			conf_free(pScene->meshes[i].clusters);
			conf_free(pScene->meshes[i].clusterCompacts);
		}
		// Remove Textures
		for (uint32_t i = 0; i < pScene->numMaterials; ++i)
		{
			removeResource(gDiffuseMaps[i]);
			removeResource(gNormalMaps[i]);
			removeResource(gSpecularMaps[i]);
		}

		removeScene(pScene);

		conf_free(gDiffuseMapsStorage);
		conf_free(gNormalMapsStorage);
		conf_free(gSpecularMapsStorage);
		/************************************************************************/
		/************************************************************************/
		removeShaders();

		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeFence(pRenderer, pTransitionFences);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pGraphicsQueue);

		removeCmd_n(pComputeCmdPool, gImageCount, ppComputeCmds);
		removeCmdPool(pRenderer, pComputeCmdPool);
		removeQueue(pComputeQueue);

		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerPointClamp);
		removeSampler(pRenderer, pSamplerBilinearClamp);

		removeBlendState(pBlendStateOneZero);
		removeBlendState(pBlendStateSkyBox);

		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateDisable);

		removeRasterizerState(pRasterizerStateCullBack);
		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);
		removeRasterizerState(pRasterizerStateCullBackMS);
		removeRasterizerState(pRasterizerStateCullFrontMS);
		removeRasterizerState(pRasterizerStateCullNoneMS);

		removeGpuProfiler(pRenderer, pGraphicsGpuProfiler);
		removeGpuProfiler(pRenderer, pComputeGpuProfiler);

		removeResourceLoaderInterface(pRenderer);

		/*
#ifdef _DEBUG
		ID3D12DebugDevice *pDebugDevice = NULL;
		pRenderer->pDxDevice->QueryInterface(&pDebugDevice);
		
		pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		pDebugDevice->Release();
#endif
*/

		removeRenderer(pRenderer);
	}

	// Setup the render targets used in this demo.
	// The only render target that is being currently used stores the results of the Visibility Buffer pass.
	// As described earlier, this render target uses 32 bit per pixel to store draw / triangle IDs that will be
	// loaded later by the shade step to reconstruct interpolated triangle data per pixel.
	bool Load()
	{
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addFence(pRenderer, &pComputeCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
			addSemaphore(pRenderer, &pComputeCompleteSemaphores[i]);
		}

		gFrameCount = 0;

		if (!addRenderTargets())
			return false;

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#if defined(DIRECT3D12)
		if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
		{
			SetHDRMetaData(gAppSettings.MaxOutputNits, gAppSettings.MinOutputNits, gAppSettings.MaxCLL, gAppSettings.MaxFALL);
		}
#endif

		/************************************************************************/
		// Vertex layout used by all geometry passes (shadow, visibility, deferred)
		/************************************************************************/
#if !defined(METAL)
#if defined(__linux__)
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 4;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayout.mAttribs[1].mBinding = 1;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[2].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[2].mBinding = 2;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayout.mAttribs[3].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[3].mBinding = 3;
		vertexLayout.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = ImageFormat::RG32F;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

		// Position only vertex stream that is used in shadow opaque pass
		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;
#else
		VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = 4;
		vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayout.mAttribs[0].mBinding = 0;
		vertexLayout.mAttribs[0].mLocation = 0;
		vertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayout.mAttribs[1].mFormat = ImageFormat::R32UI;
		vertexLayout.mAttribs[1].mBinding = 1;
		vertexLayout.mAttribs[1].mLocation = 1;
		vertexLayout.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayout.mAttribs[2].mFormat = ImageFormat::R32UI;
		vertexLayout.mAttribs[2].mBinding = 2;
		vertexLayout.mAttribs[2].mLocation = 2;
		vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayout.mAttribs[3].mFormat = ImageFormat::R32UI;
		vertexLayout.mAttribs[3].mBinding = 3;
		vertexLayout.mAttribs[3].mLocation = 3;

		VertexLayout vertexLayoutPosAndTex = {};
		vertexLayoutPosAndTex.mAttribCount = 2;
		vertexLayoutPosAndTex.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosAndTex.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPosAndTex.mAttribs[0].mBinding = 0;
		vertexLayoutPosAndTex.mAttribs[0].mLocation = 0;
		vertexLayoutPosAndTex.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosAndTex.mAttribs[1].mFormat = ImageFormat::R32UI;
		vertexLayoutPosAndTex.mAttribs[1].mBinding = 1;
		vertexLayoutPosAndTex.mAttribs[1].mLocation = 1;

		// Position only vertex stream that is used in shadow opaque pass
		VertexLayout vertexLayoutPositionOnly = {};
		vertexLayoutPositionOnly.mAttribCount = 1;
		vertexLayoutPositionOnly.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPositionOnly.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutPositionOnly.mAttribs[0].mBinding = 0;
		vertexLayoutPositionOnly.mAttribs[0].mLocation = 0;
		vertexLayoutPositionOnly.mAttribs[0].mOffset = 0;
#endif
#endif
		/************************************************************************/
		// Setup the Shadow Pass Pipeline
		/************************************************************************/
		// Setup pipeline settings
		GraphicsPipelineDesc shadowPipelineSettings = {};
		shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowPipelineSettings.pDepthState = pDepthStateEnable;
		shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mDesc.mFormat;
		shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mDesc.mSampleCount;
		shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mDesc.mSampleQuality;
		shadowPipelineSettings.pRootSignature = pRootSignatureVBPass;
#if (MSAASAMPLECOUNT > 1)
		shadowPipelineSettings.pRasterizerState = pRasterizerStateCullFrontMS;
#else
		shadowPipelineSettings.pRasterizerState = pRasterizerStateCullFront;
#endif
#if !defined(METAL)
		shadowPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
#endif
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[0];
		addPipeline(pRenderer, &shadowPipelineSettings, &pPipelineShadowPass[0]);

#if !defined(METAL)
		shadowPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
#endif
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[1];
		addPipeline(pRenderer, &shadowPipelineSettings, &pPipelineShadowPass[1]);

		/************************************************************************/
		// Setup the Visibility Buffer Pass Pipeline
		/************************************************************************/
		// Setup pipeline settings
		GraphicsPipelineDesc vbPassPipelineSettings = {};
		vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbPassPipelineSettings.mRenderTargetCount = 1;
		vbPassPipelineSettings.pDepthState = pDepthStateEnable;
		vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mDesc.mFormat;
		vbPassPipelineSettings.pSrgbValues = &pRenderTargetVBPass->mDesc.mSrgb;
		vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mDesc.mSampleCount;
		vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mDesc.mSampleQuality;
		vbPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
#if !defined(METAL)
		vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
#endif

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
#if !defined(METAL)
			if (i == GEOMSET_OPAQUE)
				vbPassPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
			else
				vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
#endif

#if (MSAASAMPLECOUNT > 1)
			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNoneMS : pRasterizerStateCullFrontMS;
#else
			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNone : pRasterizerStateCullFront;
#endif
			vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

#if defined(_DURANGO)
			ExtendedGraphicsPipelineDesc edescs[2] = {};

			edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
			initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
			edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

			edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
			edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;

			if (i == 0)
				edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;

			addPipelineExt(pRenderer, &vbPassPipelineSettings, _countof(edescs), edescs, &pPipelineVisibilityBufferPass[i]);
#else
			addPipeline(pRenderer, &vbPassPipelineSettings, &pPipelineVisibilityBufferPass[i]);
#endif
		}
		/************************************************************************/
		// Setup the resources needed for the Visibility Buffer Shade Pipeline
		/************************************************************************/
		// Create pipeline
		// Note: the vertex layout is set to null because the positions of the fullscreen triangle are being calculated automatically
		// in the vertex shader using each vertex_id.
		GraphicsPipelineDesc vbShadePipelineSettings = {};
		vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbShadePipelineSettings.mRenderTargetCount = 1;
		vbShadePipelineSettings.pDepthState = pDepthStateDisable;
#if (MSAASAMPLECOUNT > 1)
		vbShadePipelineSettings.pRasterizerState = pRasterizerStateCullNoneMS;
#else
		vbShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
#endif
		vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;

		for (uint32_t i = 0; i < 2; ++i)
		{
			vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[i];
			vbShadePipelineSettings.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
#if (MSAASAMPLECOUNT > 1)
			vbShadePipelineSettings.pColorFormats = &pRenderTargetMSAA->mDesc.mFormat;
			vbShadePipelineSettings.pSrgbValues = &pRenderTargetMSAA->mDesc.mSrgb;
			vbShadePipelineSettings.mSampleQuality = pRenderTargetMSAA->mDesc.mSampleQuality;
#else

			//vbShadePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			vbShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mDesc.mFormat;
			vbShadePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
			vbShadePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

#endif

#if defined(_DURANGO) && 1
			ExtendedGraphicsPipelineDesc edescs[2];
			memset(edescs, 0, sizeof(edescs));

			edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
			initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
			//edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

			edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
			edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;

			if (i == 0)
				edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;
			addPipelineExt(pRenderer, &vbShadePipelineSettings, _countof(edescs), edescs, &pPipelineVisibilityBufferShadeSrgb[i]);
#else
			addPipeline(pRenderer, &vbShadePipelineSettings, &pPipelineVisibilityBufferShadeSrgb[i]);
#endif
		}
		/************************************************************************/
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		ImageFormat::Enum deferredFormats[DEFERRED_RT_COUNT] = {};
		bool              deferredSrgb[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[i]->mDesc.mFormat;
			deferredSrgb[i] = pRenderTargetDeferredPass[i]->mDesc.mSrgb;
		}

		GraphicsPipelineDesc deferredPassPipelineSettings = {};
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		deferredPassPipelineSettings.pDepthState = pDepthStateEnable;
		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.pSrgbValues = deferredSrgb;
		deferredPassPipelineSettings.mSampleCount = pDepthBuffer->mDesc.mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pDepthBuffer->mDesc.mSampleQuality;
		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSignatureDeferredPass;
#if !defined(METAL)
		deferredPassPipelineSettings.pVertexLayout = &vertexLayout;
#endif

		// Create pipelines for geometry sets
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
#if (MSAASAMPLECOUNT > 1)
			deferredPassPipelineSettings.pRasterizerState =
				i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNoneMS : pRasterizerStateCullFrontMS;
#else
			deferredPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNone : pRasterizerStateCullFront;
#endif
			deferredPassPipelineSettings.pShaderProgram = pShaderDeferredPass[i];
			addPipeline(pRenderer, &deferredPassPipelineSettings, &pPipelineDeferredPass[i]);
		}
		/************************************************************************/
		// Setup the resources needed for the Deferred Shade Pipeline
		/************************************************************************/
		// Setup pipeline settings

		// Create pipeline
		// Note: the vertex layout is set to null because the positions of the fullscreen triangle are being calculated automatically
		// in the vertex shader using each vertex_id.
		GraphicsPipelineDesc deferredShadePipelineSettings = {};
		deferredShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredShadePipelineSettings.mRenderTargetCount = 1;
		deferredShadePipelineSettings.pDepthState = pDepthStateDisable;
#if (MSAASAMPLECOUNT > 1)
		deferredShadePipelineSettings.pRasterizerState = pRasterizerStateCullNoneMS;
#else
		deferredShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
#endif
		deferredShadePipelineSettings.pRootSignature = pRootSignatureDeferredShade;

		for (uint32_t i = 0; i < 2; ++i)
		{
			deferredShadePipelineSettings.pShaderProgram = pShaderDeferredShade[i];
			deferredShadePipelineSettings.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
#if (MSAASAMPLECOUNT > 1)
			deferredShadePipelineSettings.pColorFormats = &pRenderTargetMSAA->mDesc.mFormat;
			deferredShadePipelineSettings.pSrgbValues = &pRenderTargetMSAA->mDesc.mSrgb;
			deferredShadePipelineSettings.mSampleQuality = pRenderTargetMSAA->mDesc.mSampleQuality;
#else

			//deferredShadePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			deferredShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mDesc.mFormat;
			deferredShadePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
			deferredShadePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

#endif
			addPipeline(pRenderer, &deferredShadePipelineSettings, &pPipelineDeferredShadeSrgb[i]);
		}
		/************************************************************************/
		// Setup the resources needed for the Deferred Point Light Shade Pipeline
		/************************************************************************/
		// Create vertex layout
		VertexLayout vertexLayoutPointLightShade = {};
		vertexLayoutPointLightShade.mAttribCount = 1;
		vertexLayoutPointLightShade.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPointLightShade.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutPointLightShade.mAttribs[0].mBinding = 0;
		vertexLayoutPointLightShade.mAttribs[0].mLocation = 0;
		vertexLayoutPointLightShade.mAttribs[0].mOffset = 0;

		// Setup pipeline settings
		GraphicsPipelineDesc deferredPointLightPipelineSettings = { 0 };
		deferredPointLightPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPointLightPipelineSettings.mRenderTargetCount = 1;
		deferredPointLightPipelineSettings.pBlendState = pBlendStateOneZero;
		deferredPointLightPipelineSettings.pDepthState = pDepthStateDisable;
		deferredPointLightPipelineSettings.pColorFormats = deferredShadePipelineSettings.pColorFormats;
		deferredPointLightPipelineSettings.pSrgbValues = deferredShadePipelineSettings.pSrgbValues;
		deferredPointLightPipelineSettings.mSampleCount = deferredShadePipelineSettings.mSampleCount;
		deferredPointLightPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
		deferredPointLightPipelineSettings.pRootSignature = pRootSignatureDeferredShadePointLight;
		deferredPointLightPipelineSettings.pShaderProgram = pShaderDeferredShadePointLight;
		deferredPointLightPipelineSettings.pVertexLayout = &vertexLayoutPointLightShade;

		addPipeline(pRenderer, &deferredPointLightPipelineSettings, &pPipelineDeferredShadePointLightSrgb);
		/************************************************************************/
		// Setup HDAO post process pipeline
		/************************************************************************/
		GraphicsPipelineDesc aoPipelineSettings = {};
		aoPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		aoPipelineSettings.mRenderTargetCount = 1;
		aoPipelineSettings.pDepthState = pDepthStateDisable;
		aoPipelineSettings.pColorFormats = &pRenderTargetAO->mDesc.mFormat;
		aoPipelineSettings.pSrgbValues = &pRenderTargetAO->mDesc.mSrgb;
		aoPipelineSettings.mSampleCount = pRenderTargetAO->mDesc.mSampleCount;
		aoPipelineSettings.mSampleQuality = pRenderTargetAO->mDesc.mSampleQuality;
		aoPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		aoPipelineSettings.pRootSignature = pRootSignatureAO;
		for (uint32_t i = 0; i < 4; ++i)
		{
			aoPipelineSettings.pShaderProgram = pShaderAO[i];
			addPipeline(pRenderer, &aoPipelineSettings, &pPipelineAO[i]);
		}

		/************************************************************************/
		// Setup Skybox pipeline
		/************************************************************************/

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		GraphicsPipelineDesc pipelineSettings = { 0 };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;

		pipelineSettings.pBlendState = pBlendStateSkyBox;

		pipelineSettings.pColorFormats = &pIntermediateRenderTarget->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pIntermediateRenderTarget->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		//pipelineSettings.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSingatureSkybox;
		pipelineSettings.pShaderProgram = pShaderSkybox;
		pipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pSkyboxPipeline);

		/************************************************************************/
		// Setup Sun pipeline
		/************************************************************************/

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSun = {};
		vertexLayoutSun.mAttribCount = 3;
		vertexLayoutSun.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSun.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutSun.mAttribs[0].mBinding = 0;
		vertexLayoutSun.mAttribs[0].mLocation = 0;
		vertexLayoutSun.mAttribs[0].mOffset = 0;

		vertexLayoutSun.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutSun.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutSun.mAttribs[1].mBinding = 0;
		vertexLayoutSun.mAttribs[1].mLocation = 1;
		vertexLayoutSun.mAttribs[1].mOffset = sizeof(float3);

		vertexLayoutSun.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutSun.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayoutSun.mAttribs[2].mBinding = 0;
		vertexLayoutSun.mAttribs[2].mLocation = 2;
		vertexLayoutSun.mAttribs[2].mOffset = sizeof(float3) * 2;

		//Draw Sun
		GraphicsPipelineDesc pipelineSettingsSun = { 0 };
		pipelineSettingsSun.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsSun.pRasterizerState = pRasterizerStateCullBack;
		pipelineSettingsSun.pDepthState = pDepthStateEnable;
		pipelineSettingsSun.mDepthStencilFormat = pDepthBuffer->mDesc.mFormat;

		pipelineSettingsSun.mRenderTargetCount = 1;
		pipelineSettingsSun.pColorFormats = &pRenderTargetSun->mDesc.mFormat;
		pipelineSettingsSun.pSrgbValues = &pRenderTargetSun->mDesc.mSrgb;
		pipelineSettingsSun.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		pipelineSettingsSun.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;

		pipelineSettingsSun.pVertexLayout = &vertexLayoutSun;
		pipelineSettingsSun.pRootSignature = pRootSigSunPass;
		pipelineSettingsSun.pShaderProgram = pSunPass;
		addPipeline(pRenderer, &pipelineSettingsSun, &pPipelineSunPass);

		/************************************************************************/
		// Setup Godray pipeline
		/************************************************************************/
		VertexLayout vertexLayoutCopyShaders = {};
		vertexLayoutCopyShaders.mAttribCount = 0;

		GraphicsPipelineDesc pipelineSettingsGodRay = { 0 };
		pipelineSettingsGodRay.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsGodRay.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettingsGodRay.mRenderTargetCount = 1;
		pipelineSettingsGodRay.pColorFormats = &pRenderTargetGodRayA->mDesc.mFormat;
		pipelineSettingsGodRay.pSrgbValues = &pRenderTargetGodRayA->mDesc.mSrgb;
		pipelineSettingsGodRay.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsGodRay.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsGodRay.pVertexLayout = &vertexLayoutCopyShaders;
		pipelineSettingsGodRay.pRootSignature = pRootSigGodRayPass;
		pipelineSettingsGodRay.pShaderProgram = pGodRayPass;
		addPipeline(pRenderer, &pipelineSettingsGodRay, &pPipelineGodRayPass);

		/************************************************************************/
		// Setup Curve Conversion pipeline
		/************************************************************************/

		GraphicsPipelineDesc pipelineSettingsCurveConversion = { 0 };
		pipelineSettingsCurveConversion.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;

		pipelineSettingsCurveConversion.pRasterizerState = pRasterizerStateCullNone;

		pipelineSettingsCurveConversion.mRenderTargetCount = 1;
		pipelineSettingsCurveConversion.pColorFormats = &pCurveConversionRenderTarget->mDesc.mFormat;
		pipelineSettingsCurveConversion.pSrgbValues = &pCurveConversionRenderTarget->mDesc.mSrgb;
		pipelineSettingsCurveConversion.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsCurveConversion.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsCurveConversion.pVertexLayout = &vertexLayoutCopyShaders;
		pipelineSettingsCurveConversion.pRootSignature = pRootSigCurveConversionPass;
		pipelineSettingsCurveConversion.pShaderProgram = pShaderCurveConversion;
		addPipeline(pRenderer, &pipelineSettingsCurveConversion, &pPipelineCurveConversionPass);

		/************************************************************************/
		// Setup Present pipeline
		/************************************************************************/

		GraphicsPipelineDesc pipelineSettingsFinalPass = { 0 };
		pipelineSettingsFinalPass.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsFinalPass.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettingsFinalPass.mRenderTargetCount = 1;
		pipelineSettingsFinalPass.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettingsFinalPass.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettingsFinalPass.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettingsFinalPass.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettingsFinalPass.pVertexLayout = &vertexLayoutCopyShaders;
		pipelineSettingsFinalPass.pRootSignature = pRootSigPresentPass;
		pipelineSettingsFinalPass.pShaderProgram = pShaderPresentPass;

		addPipeline(pRenderer, &pipelineSettingsFinalPass, &pPipelinePresentPass);

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
		waitForFences(pComputeQueue, gImageCount, pComputeCompleteFences, true);

		// Remove default fences, semaphores
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
			removeSemaphore(pRenderer, pComputeCompleteSemaphores[i]);
		}

		gAppUI.Unload();

		for (uint32_t i = 0; i < 4; ++i)
			removePipeline(pRenderer, pPipelineAO[i]);
		removePipeline(pRenderer, pPipelineResolve);
		removePipeline(pRenderer, pPipelineResolvePost);

		removePipeline(pRenderer, pPipelineSunPass);
		removePipeline(pRenderer, pPipelineGodRayPass);

		removePipeline(pRenderer, pPipelineGodrayResolve);
		removePipeline(pRenderer, pPipelineGodrayResolvePost);

		removePipeline(pRenderer, pPipelineCurveConversionPass);
		removePipeline(pRenderer, pPipelinePresentPass);

		// Destroy graphics pipelines
		removePipeline(pRenderer, pPipelineDeferredShadePointLightSrgb);
		for (uint32_t i = 0; i < 2; ++i)
		{
			removePipeline(pRenderer, pPipelineDeferredShadeSrgb[i]);
		}

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
			removePipeline(pRenderer, pPipelineDeferredPass[i]);

		for (uint32_t i = 0; i < 2; ++i)
		{
			removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
		}

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
			removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
			removePipeline(pRenderer, pPipelineShadowPass[i]);

		removePipeline(pRenderer, pSkyboxPipeline);

		removeRenderTargets();
	}

	void Update(float deltaTime)
	{
#if !defined(TARGET_IOS)
		if (pSwapChain->mDesc.mEnableVsync != gToggleVSync)
		{
			waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
			::toggleVSync(pRenderer, &pSwapChain);
		}
#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
		if (gResolutionChange)
		{
			gResolutionChange = false;
			setResolution(getMonitor(0), &gResolutions[gResolutionIndex]);
			pVisibilityBuffer->Unload();
			pVisibilityBuffer->Load();
		}
#endif
#endif

		if (gWasColorSpace != gAppSettings.mCurrentSwapChainColorSpace || gWasDisplayColorRange != gAppSettings.mDisplayColorRange ||
			gWasDisplaySignalRange != gAppSettings.mDisplaySignalRange)
		{
#if defined(DIRECT3D12) && !defined(_DURANGO)
			if (gWasColorSpace != gAppSettings.mCurrentSwapChainColorSpace && gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
			{
				pVisibilityBuffer->Unload();
				pVisibilityBuffer->Load();
			}
#endif

			gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
			gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
			gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;
		}

		//Change swapchain
		if (gWasOutputMode != gAppSettings.mOutputMode)
		{
			waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, false);

#if defined(_DURANGO)
			//garuantee that every fence for each index has same value
			if (pRenderCompleteFences[0]->mFenceValue == pRenderCompleteFences[1]->mFenceValue &&
				pRenderCompleteFences[0]->mFenceValue == pRenderCompleteFences[2]->mFenceValue)
			{
#endif

				if (gWasOutputMode != OUTPUT_MODE_HDR10)
				{
					if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
					{
						pVisibilityBuffer->Unload();
						pVisibilityBuffer->Load();
					}
				}
				else if (gWasOutputMode == OUTPUT_MODE_HDR10)
				{
					pVisibilityBuffer->Unload();
					pVisibilityBuffer->Load();
				}

				gWasOutputMode = gAppSettings.mOutputMode;
#if defined(_DURANGO)
			}
#endif
		}

		// Process user input
		handleKeyboardInput(deltaTime);

#if !defined(TARGET_IOS)
		pCameraController->update(deltaTime);
#endif

		//Camera Walking Update

		if (gAppSettings.cameraWalking)
		{
			if (totalElpasedTime - (0.033333f * gAppSettings.cameraWalkingSpeed) <= cameraWalkingTime)
			{
				cameraWalkingTime = 0.0f;
			}

			cameraWalkingTime += deltaTime * gAppSettings.cameraWalkingSpeed;

			uint  currentCameraFrame = (uint)(cameraWalkingTime / 0.00833f);
			float remind = cameraWalkingTime - (float)currentCameraFrame * 0.00833f;

			float3 newPos =
				v3ToF3(lerp(f3Tov3(CameraPathData[2 * currentCameraFrame]), f3Tov3(CameraPathData[2 * (currentCameraFrame + 1)]), remind));
			pCameraController->moveTo(f3Tov3(newPos));

			float3 newLookat = v3ToF3(
				lerp(f3Tov3(CameraPathData[2 * currentCameraFrame + 1]), f3Tov3(CameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
			pCameraController->lookAt(f3Tov3(newLookat));
		}

		gAppUI.Update(deltaTime);

		updateDynamicUIElements();
	}

	void Draw()
	{
		if (!gAppSettings.mAsyncCompute)
		{
			// Get the current render target for this frame
			acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &gPresentFrameIdx);

			// check to see if we can use the cmd buffer
			Fence*      pRenderFence = pRenderCompleteFences[gPresentFrameIdx];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pGraphicsQueue, 1, &pRenderFence, false);
		}
		else
		{
			if (gFrameCount < gImageCount)
			{
				// Set gPresentFrameIdx as gFrameCount
				// This gaurantees that every precomputed resources with compute shader have data
				gPresentFrameIdx = (uint)gFrameCount;
			}
			else
			{
				// Get the current render target for this frame
				acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &gPresentFrameIdx);
			}

			// check to see if we can use the cmd buffer
			Fence*      pComputeFence = pComputeCompleteFences[gPresentFrameIdx];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pComputeFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pComputeQueue, 1, &pComputeFence, false);

			if (gFrameCount >= gImageCount)
			{
				// check to see if we can use the cmd buffer
				Fence*      pRenderFence = pRenderCompleteFences[gPresentFrameIdx];
				FenceStatus fenceStatus;
				getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
				if (fenceStatus == FENCE_STATUS_INCOMPLETE)
					waitForFences(pGraphicsQueue, 1, &pRenderFence, false);
			}
		}

		updateUniformData(gPresentFrameIdx);

		/************************************************************************/
		// Compute pass
		/************************************************************************/
		if (gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
		{
			/************************************************************************/
			// Update uniform buffer to gpu
			/************************************************************************/
			BufferUpdateDesc update = { pPerFrameUniformBuffers[gPresentFrameIdx], &gPerFrame[gPresentFrameIdx].gPerFrameUniformData, 0, 0,
										sizeof(PerFrameConstants) };
			updateResource(&update);
			/************************************************************************/
			// Triangle filtering async compute pass
			/************************************************************************/
			Cmd* computeCmd = ppComputeCmds[gPresentFrameIdx];

			beginCmd(computeCmd);
			cmdBeginGpuFrameProfile(computeCmd, pComputeGpuProfiler, true);

			triangleFilteringPass(computeCmd, pComputeGpuProfiler, gPresentFrameIdx);

			cmdBeginGpuTimestampQuery(computeCmd, pComputeGpuProfiler, "Clear Light Clusters", true);
			clearLightClusters(computeCmd, gPresentFrameIdx);
			cmdEndGpuTimestampQuery(computeCmd, pComputeGpuProfiler);

			if (gAppSettings.mRenderLocalLights)
			{
				/************************************************************************/
				// Synchronization
				/************************************************************************/
				// Update Light clusters on the GPU
				cmdBeginGpuTimestampQuery(computeCmd, pComputeGpuProfiler, "Compute Light Clusters", true);
				cmdSynchronizeResources(computeCmd, 1, &pLightClustersCount[gPresentFrameIdx], 0, NULL, false);
				computeLightClusters(computeCmd, gPresentFrameIdx);
				cmdEndGpuTimestampQuery(computeCmd, pComputeGpuProfiler);
			}

			cmdEndGpuFrameProfile(computeCmd, pComputeGpuProfiler);
			endCmd(computeCmd);
			queueSubmit(
				pComputeQueue, 1, &computeCmd, pComputeCompleteFences[gPresentFrameIdx], 0, NULL, 1,
				&pComputeCompleteSemaphores[gPresentFrameIdx]);
			/************************************************************************/
			/************************************************************************/
		}
		else
		{
			if (gPresentFrameIdx != -1)
			{
				BufferUpdateDesc update = { pPerFrameUniformBuffers[gPresentFrameIdx], &gPerFrame[gPresentFrameIdx].gPerFrameUniformData, 0,
											0, sizeof(PerFrameConstants) };
				updateResource(&update);
			}
		}
		/************************************************************************/
		// Draw Pass
		/************************************************************************/
		if (!gAppSettings.mAsyncCompute || gFrameCount >= gImageCount)
		{
			Cmd* graphicsCmd = NULL;

			pScreenRenderTarget = pIntermediateRenderTarget;
			//pScreenRenderTarget = pSwapChain->ppSwapchainRenderTargets[gPresentFrameIdx];

			// Get command list to store rendering commands for this frame
			graphicsCmd = ppCmds[gPresentFrameIdx];
			// Submit all render commands for this frame
			beginCmd(graphicsCmd);

			cmdBeginGpuFrameProfile(graphicsCmd, pGraphicsGpuProfiler, true);

			if (!gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
			{
				triangleFilteringPass(graphicsCmd, pGraphicsGpuProfiler, gPresentFrameIdx);
			}

			if (!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles)
			{
				cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "Clear Light Clusters", true);
				clearLightClusters(graphicsCmd, gPresentFrameIdx);
				cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);
			}

			if ((!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles) && gAppSettings.mRenderLocalLights)
			{
				// Update Light clusters on the GPU
				cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "Compute Light Clusters", true);
				cmdSynchronizeResources(graphicsCmd, 1, &pLightClustersCount[gPresentFrameIdx], 0, NULL, false);
				computeLightClusters(graphicsCmd, gPresentFrameIdx);
				cmdSynchronizeResources(graphicsCmd, 1, &pLightClusters[gPresentFrameIdx], 0, NULL, false);
				cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);
			}

			// Transition swapchain buffer to be used as a render target
			TextureBarrier barriers[] = {
				{ pScreenRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetMSAA->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
			};
			cmdResourceBarrier(graphicsCmd, 0, NULL, 3, barriers, true);

#ifndef METAL
			if (gAppSettings.mFilterTriangles)
			{
				const uint32_t numBarriers = (gNumGeomSets * gNumViews) + gNumViews + 1 + 2;
				uint32_t       index = 0;
				BufferBarrier  barriers2[numBarriers] = {};
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[gPresentFrameIdx][GEOMSET_ALPHATESTED][i],
										   RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[gPresentFrameIdx][GEOMSET_OPAQUE][i],
										   RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[index++] = { pFilteredIndexBuffer[gPresentFrameIdx][i],
										   RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
				}
				barriers2[index++] = { pFilterIndirectMaterialBuffer[gPresentFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
				barriers2[index++] = { pLightClusters[gPresentFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
				barriers2[index++] = { pLightClustersCount[gPresentFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
				cmdResourceBarrier(graphicsCmd, numBarriers, barriers2, 0, NULL, true);
			}
#endif

			drawScene(graphicsCmd, gPresentFrameIdx);
			drawSkybox(graphicsCmd, gPresentFrameIdx);

#ifdef _DURANGO
			// When async compute is on, we need to transition some resources in the graphics queue
			// because they can't be transitioned by the compute queue (incompatible)
			if (gAppSettings.mAsyncCompute)
				setResourcesToComputeCompliantState(gPresentFrameIdx, false);
#else
#ifndef METAL
			if (gAppSettings.mFilterTriangles)
			{
				const uint32_t numBarriers = (gNumGeomSets * gNumViews) + gNumViews + 1 + 2;
				uint32_t index = 0;
				BufferBarrier barriers2[numBarriers] = {};
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[gPresentFrameIdx][GEOMSET_ALPHATESTED][i],
										   RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[gPresentFrameIdx][GEOMSET_OPAQUE][i],
										   RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[index++] = { pFilteredIndexBuffer[gPresentFrameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS };
				}
				barriers2[index++] = { pFilterIndirectMaterialBuffer[gPresentFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
				barriers2[index++] = { pLightClusters[gPresentFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
				barriers2[index++] = { pLightClustersCount[gPresentFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(graphicsCmd, numBarriers, barriers2, 0, NULL, true);
			}
#endif
#endif
			if (gAppSettings.mEnableGodray)
			{
				// Update uniform buffers
				BufferUpdateDesc update = { pUniformBufferSun[gPresentFrameIdx], &gUniformDataSunMatrices };
				updateResource(&update);

				drawGodray(graphicsCmd, gPresentFrameIdx);
				drawColorconversion(graphicsCmd);
			}

			cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "UI Pass", true);
			drawGUI(graphicsCmd, gPresentFrameIdx);
			cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);

			presentImage(graphicsCmd, pScreenRenderTarget->pTexture, pSwapChain->ppSwapchainRenderTargets[gPresentFrameIdx]);

			cmdEndGpuFrameProfile(graphicsCmd, pGraphicsGpuProfiler);
			endCmd(graphicsCmd);

			if (gAppSettings.mAsyncCompute)
			{
				// Submit all the work to the GPU and present
				Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, pComputeCompleteSemaphores[gPresentFrameIdx] };
				queueSubmit(
					pGraphicsQueue, 1, &graphicsCmd, pRenderCompleteFences[gPresentFrameIdx], 2, pWaitSemaphores, 1,
					&pRenderCompleteSemaphores[gPresentFrameIdx]);
			}
			else
			{
				queueSubmit(
					pGraphicsQueue, 1, &graphicsCmd, pRenderCompleteFences[gPresentFrameIdx], 1, &pImageAcquiredSemaphore, 1,
					&pRenderCompleteSemaphores[gPresentFrameIdx]);
			}

			Semaphore* pWaitSemaphores[] = { pRenderCompleteSemaphores[gPresentFrameIdx] };
			queuePresent(pGraphicsQueue, pSwapChain, gPresentFrameIdx, 1, pWaitSemaphores);
		}

		++gFrameCount;
	}

	tinystl::string GetName() { return "Visibility Buffer"; }

	/************************************************************************/
	// Add render targets
	/************************************************************************/
	bool addRenderTargets()
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = width;
		swapChainDesc.mHeight = height;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;

		if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
			swapChainDesc.mColorFormat = ImageFormat::RGB10A2;
		else
			swapChainDesc.mColorFormat = ImageFormat::BGRA8;

		swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };
		swapChainDesc.mSrgb = false;
		swapChainDesc.mEnableVsync = false;
		addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		/************************************************************************/
		/************************************************************************/
		ClearValue optimizedDepthClear = { 1.0f, 0 };
		ClearValue optimizedColorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };
		ClearValue optimizedColorClearWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

		BEGINALLOCATION("RTs");

		/************************************************************************/
		// Main depth buffer
		/************************************************************************/
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = optimizedDepthClear;
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mHeight = height;
		depthRT.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		depthRT.mSampleQuality = 0;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		depthRT.mWidth = width;
		depthRT.pDebugName = L"Depth Buffer RT";
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);
		/************************************************************************/
		// Shadow pass render target
		/************************************************************************/
		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue = optimizedDepthClear;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = ImageFormat::D32F;
		shadowRTDesc.mWidth = gShadowMapSize;
		shadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
		shadowRTDesc.mSampleQuality = 0;
		//shadowRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		shadowRTDesc.mHeight = gShadowMapSize;
		shadowRTDesc.pDebugName = L"Shadow Map RT";
		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadow);
		/************************************************************************/
		// Visibility buffer pass render target
		/************************************************************************/
		RenderTargetDesc vbRTDesc = {};
		vbRTDesc.mArraySize = 1;
		vbRTDesc.mClearValue = optimizedColorClearWhite;
		vbRTDesc.mDepth = 1;
		vbRTDesc.mFormat = ImageFormat::RGBA8;
		vbRTDesc.mHeight = height;
		vbRTDesc.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		vbRTDesc.mSampleQuality = 0;
		vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		vbRTDesc.mWidth = width;
		vbRTDesc.pDebugName = L"VB RT";
		addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;
		deferredRTDesc.mFormat = ImageFormat::RGBA8;
		deferredRTDesc.mHeight = height;
		deferredRTDesc.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.mWidth = width;
		deferredRTDesc.pDebugName = L"G-Buffer RTs";
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
		}
		/************************************************************************/
		// MSAA render target
		/************************************************************************/
		RenderTargetDesc msaaRTDesc = {};
		msaaRTDesc.mArraySize = 1;
		msaaRTDesc.mClearValue = optimizedColorClearBlack;
		msaaRTDesc.mDepth = 1;
		msaaRTDesc.mFormat = gAppSettings.mOutputMode == OutputMode::OUTPUT_MODE_SDR ? ImageFormat::RGBA8 : ImageFormat::RGB10A2;
		msaaRTDesc.mHeight = height;
		msaaRTDesc.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		msaaRTDesc.mSampleQuality = 0;
		msaaRTDesc.mWidth = width;
		msaaRTDesc.pDebugName = L"MSAA RT";
		// Disabling compression data will avoid decompression phase before resolve pass.
		// However, the shading pass will require more memory bandwidth.
		// We measured with and without compression and without compression is faster in our case.
		msaaRTDesc.mFlags = TEXTURE_CREATION_FLAG_NO_COMPRESSION;
		addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);

		/************************************************************************/
		// HDAO render target
		/************************************************************************/
		RenderTargetDesc aoRTDesc = {};
		aoRTDesc.mArraySize = 1;
		aoRTDesc.mClearValue = optimizedColorClearBlack;
		aoRTDesc.mDepth = 1;
		aoRTDesc.mFormat = ImageFormat::R8;
		aoRTDesc.mHeight = height;
		aoRTDesc.mSampleCount = SAMPLE_COUNT_1;
		aoRTDesc.mSampleQuality = 0;
		aoRTDesc.mWidth = width;
		aoRTDesc.pDebugName = L"AO RT";
		addRenderTarget(pRenderer, &aoRTDesc, &pRenderTargetAO);

		/************************************************************************/
		// Intermediate render target
		/************************************************************************/
		RenderTargetDesc postProcRTDesc = {};
		postProcRTDesc.mArraySize = 1;
		postProcRTDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		postProcRTDesc.mDepth = 1;
		postProcRTDesc.mFormat = gAppSettings.mOutputMode == OutputMode::OUTPUT_MODE_SDR ? ImageFormat::RGBA8 : ImageFormat::RGB10A2;
		postProcRTDesc.mHeight = mSettings.mHeight;
		postProcRTDesc.mWidth = mSettings.mWidth;
		postProcRTDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		postProcRTDesc.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		postProcRTDesc.pDebugName = L"pIntermediateRenderTarget";
		addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

		/************************************************************************/
		// Setup MSAA resolve pipeline
		/************************************************************************/
		GraphicsPipelineDesc resolvePipelineSettings = { 0 };
		resolvePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		resolvePipelineSettings.mRenderTargetCount = 1;
		resolvePipelineSettings.pDepthState = pDepthStateDisable;
		resolvePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mDesc.mFormat;
		resolvePipelineSettings.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		resolvePipelineSettings.mSampleCount = pSwapChain->mDesc.mSampleCount;
		resolvePipelineSettings.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		resolvePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		resolvePipelineSettings.pRootSignature = pRootSignatureResolve;
		resolvePipelineSettings.pShaderProgram = pShaderResolve;
		addPipeline(pRenderer, &resolvePipelineSettings, &pPipelineResolve);
		addPipeline(pRenderer, &resolvePipelineSettings, &pPipelineResolvePost);

		/************************************************************************/
		// GodRay render target
		/************************************************************************/

		RenderTargetDesc GRRTDesc = {};
		GRRTDesc.mArraySize = 1;
		GRRTDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
		GRRTDesc.mDepth = 1;
		GRRTDesc.mFormat =
			pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat == ImageFormat::BGRA8 ? ImageFormat::RGBA8 : ImageFormat::RGB10A2;
		GRRTDesc.mHeight = mSettings.mHeight;
		GRRTDesc.mWidth = mSettings.mWidth;
		GRRTDesc.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
		GRRTDesc.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		GRRTDesc.pDebugName = L"Sun RT";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetSun);

		GRRTDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		GRRTDesc.pDebugName = L"Sun Resolve RT";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetSunResolved);

		GraphicsPipelineDesc resolveGodrayPipelineSettings = { 0 };
		resolveGodrayPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		resolveGodrayPipelineSettings.mRenderTargetCount = 1;
		resolveGodrayPipelineSettings.pDepthState = pDepthStateDisable;
		resolveGodrayPipelineSettings.pColorFormats = &GRRTDesc.mFormat;
		resolveGodrayPipelineSettings.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		resolveGodrayPipelineSettings.mSampleCount = pSwapChain->mDesc.mSampleCount;
		resolveGodrayPipelineSettings.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		resolveGodrayPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		resolveGodrayPipelineSettings.pRootSignature = pRootSignatureGodrayResolve;
		resolveGodrayPipelineSettings.pShaderProgram = pShaderGodrayResolve;
		addPipeline(pRenderer, &resolveGodrayPipelineSettings, &pPipelineGodrayResolve);
		addPipeline(pRenderer, &resolveGodrayPipelineSettings, &pPipelineGodrayResolvePost);

		GRRTDesc.mHeight = mSettings.mHeight / gGodrayScale;
		GRRTDesc.mWidth = mSettings.mWidth / gGodrayScale;
		GRRTDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		GRRTDesc.mFormat = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;

		GRRTDesc.pDebugName = L"GodRay RT A";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRayA);
		GRRTDesc.pDebugName = L"GodRay RT B";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetGodRayB);

		/************************************************************************/
		// Color Conversion render target
		/************************************************************************/
		RenderTargetDesc postCurveConversionRTDesc = {};
		postCurveConversionRTDesc.mArraySize = 1;
		postCurveConversionRTDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		postCurveConversionRTDesc.mDepth = 1;
		postCurveConversionRTDesc.mFormat = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		postCurveConversionRTDesc.mHeight = mSettings.mHeight;
		postCurveConversionRTDesc.mWidth = mSettings.mWidth;
		postCurveConversionRTDesc.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		postCurveConversionRTDesc.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		postCurveConversionRTDesc.pDebugName = L"pCurveConversionRenderTarget";
		addRenderTarget(pRenderer, &postCurveConversionRTDesc, &pCurveConversionRenderTarget);

		/************************************************************************/
		/************************************************************************/

		ENDALLOCATION("RTs");
		return true;
	}

	void removeRenderTargets()
	{
		removeRenderTarget(pRenderer, pCurveConversionRenderTarget);

		removeRenderTarget(pRenderer, pRenderTargetSun);
		removeRenderTarget(pRenderer, pRenderTargetSunResolved);
		removeRenderTarget(pRenderer, pRenderTargetGodRayA);
		removeRenderTarget(pRenderer, pRenderTargetGodRayB);

		removeRenderTarget(pRenderer, pIntermediateRenderTarget);
		removeRenderTarget(pRenderer, pRenderTargetMSAA);
		removeRenderTarget(pRenderer, pRenderTargetAO);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pRenderTargetVBPass);
		removeRenderTarget(pRenderer, pRenderTargetShadow);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);

		removeSwapChain(pRenderer, pSwapChain);
	}
	/************************************************************************/
	// Load all the shaders needed for the demo
	/************************************************************************/
	void addShaders()
	{
		ShaderMacro shadingMacros[2][2] = {
			{ { "SAMPLE_COUNT", tinystl::string::format("%d", MSAASAMPLECOUNT) }, { "USE_AMBIENT_OCCLUSION", "" } },
			{ { "SAMPLE_COUNT", tinystl::string::format("%d", MSAASAMPLECOUNT) }, { "USE_AMBIENT_OCCLUSION", "" } },
		};
		ShaderMacro hdaoMacros[4][2] = {};

		ShaderLoadDesc shadowPass = {};
		ShaderLoadDesc shadowPassAlpha = {};
		ShaderLoadDesc vbPass = {};
		ShaderLoadDesc vbPassAlpha = {};
		ShaderLoadDesc deferredPassAlpha = {};
		ShaderLoadDesc deferredPass = {};
		ShaderLoadDesc vbShade[2] = {};
		ShaderLoadDesc deferredShade[2] = {};
		ShaderLoadDesc deferredPointlights = {};
		ShaderLoadDesc ao[4] = {};
		ShaderLoadDesc resolvePass = {};
		ShaderLoadDesc resolveGodrayPass = {};
		ShaderLoadDesc clearBuffer = {};
		ShaderLoadDesc triangleCulling = {};
#ifndef METAL
		ShaderLoadDesc batchCompaction = {};
#endif
		ShaderLoadDesc clearLights = {};
		ShaderLoadDesc clusterLights = {};
		ShaderLoadDesc depthCopyShader = {};
		ShaderLoadDesc finalShaderDesc = {};

		shadowPass.mStages[0] = { "shadow_pass.vert", NULL, 0, FSR_SrcShaders };
		shadowPassAlpha.mStages[0] = { "shadow_pass_alpha.vert", NULL, 0, FSR_SrcShaders };
		shadowPassAlpha.mStages[1] = { "shadow_pass_alpha.frag", NULL, 0, FSR_SrcShaders };

		vbPass.mStages[0] = { "visibilityBuffer_pass.vert", NULL, 0, FSR_SrcShaders };
		vbPass.mStages[1] = { "visibilityBuffer_pass.frag", NULL, 0, FSR_SrcShaders };
		vbPassAlpha.mStages[0] = { "visibilityBuffer_pass_alpha.vert", NULL, 0, FSR_SrcShaders };
		vbPassAlpha.mStages[1] = { "visibilityBuffer_pass_alpha.frag", NULL, 0, FSR_SrcShaders };

		deferredPass.mStages[0] = { "deferred_pass.vert", NULL, 0, FSR_SrcShaders };
		deferredPass.mStages[1] = { "deferred_pass.frag", NULL, 0, FSR_SrcShaders };
		deferredPassAlpha.mStages[0] = { "deferred_pass.vert", NULL, 0, FSR_SrcShaders };
		deferredPassAlpha.mStages[1] = { "deferred_pass_alpha.frag", NULL, 0, FSR_SrcShaders };

		for (uint32_t i = 0; i < 2; ++i)
		{
			shadingMacros[i][1].value = tinystl::string::format("%d", i);    //USE_AMBIENT_OCCLUSION
			vbShade[i].mStages[0] = { "visibilityBuffer_shade.vert", NULL, 0, FSR_SrcShaders };
			vbShade[i].mStages[1] = { "visibilityBuffer_shade.frag", shadingMacros[i], 2, FSR_SrcShaders };

			deferredShade[i].mStages[0] = { "deferred_shade.vert", NULL, 0, FSR_SrcShaders };
			deferredShade[i].mStages[1] = { "deferred_shade.frag", shadingMacros[i], 2, FSR_SrcShaders };
		}

		deferredPointlights.mStages[0] = { "deferred_shade_pointlight.vert", shadingMacros[0], 1, FSR_SrcShaders };
		deferredPointlights.mStages[1] = { "deferred_shade_pointlight.frag", shadingMacros[0], 1, FSR_SrcShaders };

		// Resolve shader
		resolvePass.mStages[0] = { "resolve.vert", shadingMacros[0], 1, FSR_SrcShaders };
		resolvePass.mStages[1] = { "resolve.frag", shadingMacros[0], 1, FSR_SrcShaders };

		// Resolve shader
		resolveGodrayPass.mStages[0] = { "resolve.vert", shadingMacros[0], 1, FSR_SrcShaders };
		resolveGodrayPass.mStages[1] = { "resolveGodray.frag", shadingMacros[0], 1, FSR_SrcShaders };

		// HDAO post-process shader
		for (uint32_t i = 0; i < 4; ++i)
		{
			hdaoMacros[i][0] = shadingMacros[0][0];
			hdaoMacros[i][1] = { "AO_QUALITY", tinystl::string::format("%u", (i + 1)) };
			ao[i].mStages[0] = { "HDAO.vert", hdaoMacros[i], 2, FSRoot::FSR_SrcShaders };
			ao[i].mStages[1] = { "HDAO.frag", hdaoMacros[i], 2, FSRoot::FSR_SrcShaders };
		}

		// Triangle culling compute shader
		triangleCulling.mStages[0] = { "triangle_filtering.comp", 0, NULL, FSRoot::FSR_SrcShaders };
#if !defined(METAL)
		// Batch compaction compute shader
		batchCompaction.mStages[0] = { "batch_compaction.comp", 0, NULL, FSRoot::FSR_SrcShaders };
#endif
		// Clear buffers compute shader
		clearBuffer.mStages[0] = { "clear_buffers.comp", 0, NULL, FSRoot::FSR_SrcShaders };
		// Clear light clusters compute shader
		clearLights.mStages[0] = { "clear_light_clusters.comp", 0, NULL, FSRoot::FSR_SrcShaders };
		// Cluster lights compute shader
		clusterLights.mStages[0] = { "cluster_lights.comp", 0, NULL, FSRoot::FSR_SrcShaders };

		ShaderLoadDesc sunShaderDesc = {};

		sunShaderDesc.mStages[0] = { "sun.vert", NULL, 0, FSR_SrcShaders };
		sunShaderDesc.mStages[1] = { "sun.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &sunShaderDesc, &pSunPass);

		ShaderLoadDesc godrayShaderDesc = {};

		godrayShaderDesc.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		godrayShaderDesc.mStages[1] = { "godray.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &godrayShaderDesc, &pGodRayPass);

		ShaderLoadDesc CurveConversionShaderDesc = {};

		CurveConversionShaderDesc.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		CurveConversionShaderDesc.mStages[1] = { "CurveConversion.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &CurveConversionShaderDesc, &pShaderCurveConversion);

		ShaderLoadDesc presentShaderDesc = {};

		presentShaderDesc.mStages[0] = { "display.vert", NULL, 0, FSR_SrcShaders };
		presentShaderDesc.mStages[1] = { "display.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);

		addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHATESTED]);
		addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < 2; ++i)
			addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
		addShader(pRenderer, &deferredPass, &pShaderDeferredPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &deferredPassAlpha, &pShaderDeferredPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < 2; ++i)
			addShader(pRenderer, &deferredShade[i], &pShaderDeferredShade[i]);
		addShader(pRenderer, &deferredPointlights, &pShaderDeferredShadePointLight);
		addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
		addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
		addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
		addShader(pRenderer, &clusterLights, &pShaderClusterLights);
		for (uint32_t i = 0; i < 4; ++i)
			addShader(pRenderer, &ao[i], &pShaderAO[i]);
		addShader(pRenderer, &resolvePass, &pShaderResolve);
		addShader(pRenderer, &resolveGodrayPass, &pShaderGodrayResolve);
#ifndef METAL
		addShader(pRenderer, &batchCompaction, &pShaderBatchCompaction);
#endif
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHATESTED]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < 2; ++i)
			removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
		removeShader(pRenderer, pShaderDeferredPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderDeferredPass[GEOMSET_ALPHATESTED]);
		for (uint32_t i = 0; i < 2; ++i)
			removeShader(pRenderer, pShaderDeferredShade[i]);
		removeShader(pRenderer, pShaderDeferredShadePointLight);
		removeShader(pRenderer, pShaderTriangleFiltering);
#if !defined(METAL)
		removeShader(pRenderer, pShaderBatchCompaction);
#endif
		removeShader(pRenderer, pShaderClearBuffers);
		removeShader(pRenderer, pShaderClusterLights);
		removeShader(pRenderer, pShaderClearLightClusters);
		for (uint32_t i = 0; i < 4; ++i)
			removeShader(pRenderer, pShaderAO[i]);
		removeShader(pRenderer, pShaderResolve);
		removeShader(pRenderer, pShaderGodrayResolve);

		removeShader(pRenderer, pSunPass);
		removeShader(pRenderer, pGodRayPass);

		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderCurveConversion);
		removeShader(pRenderer, pShaderPresentPass);
	}

	// This method sets the contents of the buffers to indicate the rendering pass that
	// the whole scene triangles must be rendered (no cluster / triangle filtering).
	// This is useful for testing purposes to compare visual / performance results.
	void addTriangleFilteringBuffers()
	{
		/************************************************************************/
		// Material props
		/************************************************************************/
		uint32_t* alphaTestMaterials = (uint32_t*)conf_malloc(pScene->numMaterials * sizeof(uint32_t));
		for (uint32_t i = 0; i < pScene->numMaterials; ++i)
		{
			alphaTestMaterials[i] = pScene->materials[i].alphaTested ? 1 : 0;
		}

		BufferLoadDesc materialPropDesc = {};
		materialPropDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		materialPropDesc.mDesc.mElementCount = pScene->numMaterials;
		materialPropDesc.mDesc.mStructStride = sizeof(uint32_t);
		materialPropDesc.mDesc.mSize = materialPropDesc.mDesc.mElementCount * materialPropDesc.mDesc.mStructStride;
		materialPropDesc.pData = alphaTestMaterials;
		materialPropDesc.ppBuffer = &pMaterialPropertyBuffer;
		materialPropDesc.mDesc.pDebugName = L"Material Prop Desc";
		addResource(&materialPropDesc);

		conf_free(alphaTestMaterials);
		/************************************************************************/
		// Indirect draw arguments to draw all triangles
		/************************************************************************/
#if defined(METAL)
		uint32_t bufSize = pScene->numMeshes * sizeof(VisBufferIndirectCommand);
		// Set the indirect draw buffer for indirect drawing to the maximum amount of triangles per batch
		VisBufferIndirectCommand* indirectDrawArgumentsMax = (VisBufferIndirectCommand*)conf_malloc(bufSize);
		memset(indirectDrawArgumentsMax, 0, bufSize);

		tinystl::vector<uint32_t> materialIDPerDrawCall(pScene->numMeshes);
		for (uint32_t i = 0; i < pScene->numMeshes; i++)
		{
			VisBufferIndirectCommand* arg = &indirectDrawArgumentsMax[i];
			arg->arg.mStartVertex = pScene->meshes[i].startVertex;
			arg->arg.mVertexCount = pScene->meshes[i].vertexCount;
			arg->arg.mInstanceCount = 1;
			arg->arg.mStartInstance = 0;
			materialIDPerDrawCall[i] = pScene->meshes[i].materialId;
			;
		}

		// Setup uniform data for draw batch data.
		BufferLoadDesc indirectBufferDesc = {};
		indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectBufferDesc.mDesc.mStructStride = sizeof(VisBufferIndirectCommand);
		indirectBufferDesc.mDesc.mFirstElement = 0;
		indirectBufferDesc.mDesc.mElementCount = pScene->numMeshes;
		indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
		indirectBufferDesc.pData = indirectDrawArgumentsMax;
		indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll;
		indirectBufferDesc.mDesc.pDebugName = L"Indirect Buffer Desc";
		addResource(&indirectBufferDesc);

		// Setup indirect material buffer.
		BufferLoadDesc indirectDesc = {};
		indirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectDesc.mDesc.mElementCount = pScene->numMeshes;
		indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
		indirectDesc.pData = materialIDPerDrawCall.data();
		indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
		indirectDesc.mDesc.pDebugName = L"Indirect Desc";
		addResource(&indirectDesc);

		conf_free(indirectDrawArgumentsMax);
#else
		const uint32_t numBatches = (const uint32_t)pScene->numMeshes;
		tinystl::vector<uint32_t> materialIDPerDrawCall(MATERIAL_BUFFER_SIZE);
		tinystl::vector<VisBufferIndirectCommand> indirectArgsNoAlpha(MAX_DRAWS_INDIRECT, VisBufferIndirectCommand{ 0 });
		tinystl::vector<VisBufferIndirectCommand> indirectArgsAlpha(MAX_DRAWS_INDIRECT, VisBufferIndirectCommand{ 0 });
		uint32_t iAlpha = 0, iNoAlpha = 0;
		for (uint32_t i = 0; i < numBatches; ++i)
		{
			uint matID = pScene->meshes[i].materialId;
			Material* mat = &pScene->materials[matID];
			uint32 numIDX = pScene->meshes[i].indexCount;
			uint32 startIDX = pScene->meshes[i].startIndex;

			if (mat->alphaTested)
			{
#if defined(DIRECT3D12)
				indirectArgsAlpha[iAlpha].drawId = iAlpha;
#endif
				indirectArgsAlpha[iAlpha].arg.mInstanceCount = 1;
				indirectArgsAlpha[iAlpha].arg.mIndexCount = numIDX;
				indirectArgsAlpha[iAlpha].arg.mStartIndex = startIDX;
				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(true, j) + iAlpha] = matID;
				iAlpha++;
			}
			else
			{
#if defined(DIRECT3D12)
				indirectArgsNoAlpha[iNoAlpha].drawId = iNoAlpha;
#endif
				indirectArgsNoAlpha[iNoAlpha].arg.mInstanceCount = 1;
				indirectArgsNoAlpha[iNoAlpha].arg.mIndexCount = numIDX;
				indirectArgsNoAlpha[iNoAlpha].arg.mStartIndex = startIDX;
				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(false, j) + iNoAlpha] = matID;
				iNoAlpha++;
			}
		}
		*(((UINT*)indirectArgsAlpha.data()) + DRAW_COUNTER_SLOT_POS) = iAlpha;
		*(((UINT*)indirectArgsNoAlpha.data()) + DRAW_COUNTER_SLOT_POS) = iNoAlpha;

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
			indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(VisBufferIndirectCommand) / sizeof(uint32_t));
			indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
			indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
			indirectBufferDesc.pData = i == 0 ? indirectArgsNoAlpha.data() : indirectArgsAlpha.data();
			indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll[i];
			indirectBufferDesc.mDesc.pDebugName = L"Indirect Buffer Desc";
			addResource(&indirectBufferDesc);
		}

		BufferLoadDesc indirectDesc = {};
		indirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
		indirectDesc.pData = materialIDPerDrawCall.data();
		indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
		indirectDesc.mDesc.pDebugName = L"Indirect Desc";
		addResource(&indirectDesc);
#endif
		/************************************************************************/
		// Indirect buffers for culling
		/************************************************************************/
		BufferLoadDesc filterIbDesc = {};
		filterIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIbDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		filterIbDesc.mDesc.mElementCount = pScene->totalTriangles * 3;
		filterIbDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIbDesc.mDesc.mSize = filterIbDesc.mDesc.mElementCount * filterIbDesc.mDesc.mStructStride;
		filterIbDesc.mDesc.pDebugName = L"Filtered IB Desc";
		filterIbDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gNumViews; ++j)
			{
				filterIbDesc.ppBuffer = &pFilteredIndexBuffer[i][j];
				addResource(&filterIbDesc);
			}
		}
#if defined(METAL)
		// Set the initial indirect draw buffer contents that won't change here to avoid doing this in the compute shader.
		// The filter compute shader will only update mVertexCount accordingly depending on the filtered triangles.
		VisBufferIndirectCommand* indirectDrawArguments =
			(VisBufferIndirectCommand*)conf_malloc(pScene->numMeshes * sizeof(VisBufferIndirectCommand));
		for (uint32_t i = 0; i < pScene->numMeshes; i++)
		{
			indirectDrawArguments[i].arg.mStartVertex = pScene->meshes[i].startVertex;
			indirectDrawArguments[i].arg.mVertexCount = 0;
			indirectDrawArguments[i].arg.mInstanceCount = 1;
			indirectDrawArguments[i].arg.mStartInstance = 0;
		}

		BufferLoadDesc filterIndirectDesc = {};
		filterIndirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIndirectDesc.mDesc.mElementCount = pScene->numMeshes;
		filterIndirectDesc.mDesc.mStructStride = sizeof(VisBufferIndirectCommand);
		filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
		filterIndirectDesc.mDesc.pDebugName = L"Filter Indirect Desc";
		filterIndirectDesc.pData = indirectDrawArguments;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < gNumViews; ++j)
			{
				filterIndirectDesc.ppBuffer = &pFilteredIndirectDrawArgumentsBuffer[i][j];
				addResource(&filterIndirectDesc);
			}
		}
#else
		VisBufferIndirectCommand* indirectDrawArguments =
			(VisBufferIndirectCommand*)conf_malloc(MAX_DRAWS_INDIRECT * sizeof(VisBufferIndirectCommand));
		memset(indirectDrawArguments, 0, MAX_DRAWS_INDIRECT * sizeof(VisBufferIndirectCommand));
		for (uint32_t i = 0; i < MAX_DRAWS_INDIRECT; ++i)
		{
#if defined(DIRECT3D12)
			indirectDrawArguments[i].drawId = i;
#endif
			if (i < pScene->numMeshes)
				indirectDrawArguments[i].arg.mInstanceCount = 1;
		}

		BufferLoadDesc filterIndirectDesc = {};
		filterIndirectDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterIndirectDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(VisBufferIndirectCommand) / sizeof(uint32_t));
		filterIndirectDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
		filterIndirectDesc.mDesc.pDebugName = L"Filtered Indirect Desc";
		filterIndirectDesc.pData = indirectDrawArguments;

		BufferLoadDesc uncompactedDesc = {};
		uncompactedDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		uncompactedDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		uncompactedDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT;
		uncompactedDesc.mDesc.mStructStride = sizeof(UncompactedDrawArguments);
		uncompactedDesc.mDesc.mSize = uncompactedDesc.mDesc.mElementCount * uncompactedDesc.mDesc.mStructStride;
		uncompactedDesc.mDesc.pDebugName = L"Uncompacted Draw Arguments Desc";
		uncompactedDesc.pData = NULL;

		BufferLoadDesc filterMaterialDesc = {};
		filterMaterialDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		filterMaterialDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		filterMaterialDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
		filterMaterialDesc.mDesc.mStructStride = sizeof(uint32_t);
		filterMaterialDesc.mDesc.mSize = filterMaterialDesc.mDesc.mElementCount * filterMaterialDesc.mDesc.mStructStride;
		filterMaterialDesc.mDesc.pDebugName = L"Filtered Indirect Material Desc";
		filterMaterialDesc.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			filterMaterialDesc.ppBuffer = &pFilterIndirectMaterialBuffer[i];
			addResource(&filterMaterialDesc);

			for (uint32_t view = 0; view < gNumViews; ++view)
			{
				uncompactedDesc.ppBuffer = &pUncompactedDrawArgumentsBuffer[i][view];
				addResource(&uncompactedDesc);

				for (uint32_t geom = 0; geom < gNumGeomSets; ++geom)
				{
					filterIndirectDesc.ppBuffer = &pFilteredIndirectDrawArgumentsBuffer[i][geom][view];
					addResource(&filterIndirectDesc);
				}
			}
		}
#endif

		conf_free(indirectDrawArguments);
		/************************************************************************/
		// Triangle filtering buffers
		/************************************************************************/
		// Create buffers to store the list of filtered triangles. These buffers
		// contain the triangle IDs of the triangles that passed the culling tests.
		// One buffer per back buffer image is created for triple buffering.
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
#if defined(METAL)
			uint64_t bufferSize = NUM_BATCHES * BATCH_COUNT * sizeof(FilterBatchData);
			pFilterBatchChunk[i] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));
			pFilterBatchChunk[i]->batches = (FilterBatchData*)conf_malloc(bufferSize);
			pFilterBatchChunk[i]->currentBatchCount = 0;
			pFilterBatchChunk[i]->currentDrawCallCount = 0;

			BufferLoadDesc ubDesc = {};
			ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
			ubDesc.mDesc.mSize = bufferSize;
			ubDesc.pData = nullptr;
			ubDesc.ppBuffer = &(pFilterBatchChunk[i]->batchDataBuffer);
			ubDesc.mDesc.pDebugName = L"Uniform Buffer Desc";
			addResource(&ubDesc);
#else
			uint32_t bufferSizeTotal = 0;
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				const uint32_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
				bufferSizeTotal += bufferSize;
				pFilterBatchChunk[i][j] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));
				pFilterBatchChunk[i][j]->batches = (FilterBatchData*)conf_calloc(1, bufferSize);
				pFilterBatchChunk[i][j]->currentBatchCount = 0;
				pFilterBatchChunk[i][j]->currentDrawCallCount = 0;
			}

			addUniformRingBuffer(pRenderer, bufferSizeTotal, &pFilterBatchDataBuffer[i]);
#endif
		}
#ifndef METAL
		/************************************************************************/
		// Mesh constants
		/************************************************************************/
		// create mesh constants buffer
		MeshConstants* meshConstants = (MeshConstants*)conf_malloc(pScene->numMeshes * sizeof(MeshConstants));

		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			meshConstants[i].faceCount = pScene->meshes[i].indexCount / 3;
			meshConstants[i].indexOffset = pScene->meshes[i].startIndex;
			meshConstants[i].materialID = pScene->meshes[i].materialId;
			meshConstants[i].twoSided = pScene->materials[pScene->meshes[i].materialId].twoSided ? 1 : 0;
		}

		BufferLoadDesc meshConstantDesc = {};
		meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		meshConstantDesc.mDesc.mElementCount = pScene->numMeshes;
		meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
		meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
		meshConstantDesc.pData = meshConstants;
		meshConstantDesc.ppBuffer = &pMeshConstantsBuffer;
		meshConstantDesc.mDesc.pDebugName = L"Mesh Constant Desc";

		addResource(&meshConstantDesc);

		conf_free(meshConstants);
#endif

		/************************************************************************/
		// Per Frame Constant Buffers
		/************************************************************************/
		uint64_t       size = sizeof(PerFrameConstants);
		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mSize = size;
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = nullptr;
		ubDesc.mDesc.pDebugName = L"Uniform Buffer Desc";

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pPerFrameUniformBuffers[i];
			addResource(&ubDesc);
		}
		/************************************************************************/
		// Lighting buffers
		/************************************************************************/
#if defined(METAL)
		// Allocate buffers for per-batch uniform data
		gPerBatchUniformBuffers = (Buffer**)conf_malloc(pScene->numMeshes * sizeof(Buffer*));
		for (uint32_t j = 0; j < pScene->numMeshes; j++)
		{
			PerBatchConstants perBatchData;
			const Material*   mat = &pScene->materials[pScene->meshes[j].materialId];
			perBatchData.drawId = j;
			perBatchData.twoSided = (mat->twoSided ? 1 : 0);

			BufferLoadDesc batchUb = {};
			batchUb.mDesc.mSize = sizeof(PerBatchConstants);
			batchUb.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			batchUb.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			batchUb.pData = &perBatchData;
			batchUb.ppBuffer = &gPerBatchUniformBuffers[j];
			batchUb.mDesc.pDebugName = L"Batch UB Desc";
			addResource(&batchUb);
		}
#endif

		// Setup lights uniform buffer
		for (uint32_t i = 0; i < LIGHT_COUNT; i++)
		{
			gLightData[i].position.setX(float(rand() % 2000) - 1000.0f);
			gLightData[i].position.setY(100);
			gLightData[i].position.setZ(float(rand() % 2000) - 1000.0f);
			gLightData[i].color.setX(float(rand() % 255) / 255.0f);
			gLightData[i].color.setY(float(rand() % 255) / 255.0f);
			gLightData[i].color.setZ(float(rand() % 255) / 255.0f);
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
		batchUb.mDesc.pDebugName = L"Batch UB Desc";
		addResource(&batchUb);

		// Setup lights cluster data
		uint32_t       lightClustersInitData[LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT] = {};
		BufferLoadDesc lightClustersCountBufferDesc = {};
		lightClustersCountBufferDesc.mDesc.mSize = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
		lightClustersCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		lightClustersCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		lightClustersCountBufferDesc.mDesc.mFirstElement = 0;
		lightClustersCountBufferDesc.mDesc.mElementCount = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
		lightClustersCountBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		lightClustersCountBufferDesc.pData = lightClustersInitData;
		lightClustersCountBufferDesc.mDesc.pDebugName = L"Light Cluster Count Buffer Desc";
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			lightClustersCountBufferDesc.ppBuffer = &pLightClustersCount[frameIdx];
			addResource(&lightClustersCountBufferDesc);
		}

		BufferLoadDesc lightClustersDataBufferDesc = {};
		lightClustersDataBufferDesc.mDesc.mSize = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
		lightClustersDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		lightClustersDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		lightClustersDataBufferDesc.mDesc.mFirstElement = 0;
		lightClustersDataBufferDesc.mDesc.mElementCount = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
		lightClustersDataBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		lightClustersDataBufferDesc.pData = nullptr;
		lightClustersDataBufferDesc.mDesc.pDebugName = L"Light Cluster Data Buffer Desc";
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			lightClustersDataBufferDesc.ppBuffer = &pLightClusters[frameIdx];
			addResource(&lightClustersDataBufferDesc);
		}

		BufferLoadDesc sunDataBufferDesc = {};
		sunDataBufferDesc.mDesc.mSize = sizeof(UniformDataSunMatrices);
		sunDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sunDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sunDataBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sunDataBufferDesc.pData = nullptr;
		sunDataBufferDesc.mDesc.pDebugName = L"Sun matrices Data Buffer Desc";
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			sunDataBufferDesc.ppBuffer = &pUniformBufferSun[frameIdx];
			addResource(&sunDataBufferDesc);
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
#if defined(METAL)
		removeResource(pIndirectDrawArgumentsBufferAll);
		removeResource(pIndirectMaterialBufferAll);
#else
		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			removeResource(pIndirectDrawArgumentsBufferAll[i]);
		}

		removeResource(pIndirectMaterialBufferAll);
#endif
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
#if defined(METAL)
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t view = 0; view < gNumViews; ++view)
			{
				removeResource(pFilteredIndirectDrawArgumentsBuffer[i][view]);
			}
		}
#else
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
#endif
		/************************************************************************/
		// Triangle filtering buffers
		/************************************************************************/
		// Create buffers to store the list of filtered triangles. These buffers
		// contain the triangle IDs of the triangles that passed the culling tests.
		// One buffer per back buffer image is created for triple buffering.
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
#if defined(METAL)
			removeResource(pFilterBatchChunk[i]->batchDataBuffer);
			conf_free(pFilterBatchChunk[i]->batches);
			conf_free(pFilterBatchChunk[i]);
#else
			for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
			{
				conf_free(pFilterBatchChunk[i][j]->batches);
				conf_free(pFilterBatchChunk[i][j]);
			}

			removeUniformRingBuffer(pFilterBatchDataBuffer[i]);
#endif
		}
#ifndef METAL
		/************************************************************************/
		// Mesh constants
		/************************************************************************/
		removeResource(pMeshConstantsBuffer);
#endif

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
#if defined(METAL)
		for (uint32_t j = 0; j < pScene->numMeshes; j++)
		{
			removeResource(gPerBatchUniformBuffers[j]);
		}
		conf_free(gPerBatchUniformBuffers);
#endif
		removeResource(pLightsBuffer);
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			removeResource(pLightClustersCount[frameIdx]);
			removeResource(pLightClusters[frameIdx]);
			removeResource(pUniformBufferSun[frameIdx]);
		}
		/************************************************************************/
		/************************************************************************/
	}

#if !defined(METAL)
	void setResourcesToComputeCompliantState(uint32_t frameIdx, bool submitAndWait)
	{
		if (submitAndWait)
			beginCmd(ppCmds[frameIdx]);

		BufferBarrier barrier[] = { { pVertexBufferPosition, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
									{ pIndexBufferAll, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
									{ pMeshConstantsBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
									{ pMaterialPropertyBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
									{ pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS },
									{ pUncompactedDrawArgumentsBuffer[frameIdx][VIEW_SHADOW], RESOURCE_STATE_UNORDERED_ACCESS },
									{ pUncompactedDrawArgumentsBuffer[frameIdx][VIEW_CAMERA], RESOURCE_STATE_UNORDERED_ACCESS },
									{ pLightClustersCount[frameIdx],
									  RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE } };
		cmdResourceBarrier(ppCmds[frameIdx], 8, barrier, 0, NULL, false);

		BufferBarrier indirectDrawBarriers[gNumGeomSets * gNumViews] = {};
		for (uint32_t i = 0, k = 0; i < gNumGeomSets; i++)
		{
			for (uint32_t j = 0; j < gNumViews; j++, k++)
			{
				indirectDrawBarriers[k].pBuffer = pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][j];
				indirectDrawBarriers[k].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
				indirectDrawBarriers[k].mSplit = false;
			}
		}
		cmdResourceBarrier(ppCmds[frameIdx], gNumGeomSets * gNumViews, indirectDrawBarriers, 0, NULL, true);

		BufferBarrier filteredIndicesBarriers[gNumViews] = {};
		for (uint32_t j = 0; j < gNumViews; j++)
		{
			filteredIndicesBarriers[j].pBuffer = pFilteredIndexBuffer[frameIdx][j];
			filteredIndicesBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			filteredIndicesBarriers[j].mSplit = false;
		}
		cmdResourceBarrier(ppCmds[frameIdx], gNumViews, filteredIndicesBarriers, 0, NULL, true);

		if (submitAndWait)
		{
			endCmd(ppCmds[frameIdx]);
			queueSubmit(pGraphicsQueue, 1, ppCmds, pTransitionFences, 0, nullptr, 0, nullptr);
			waitForFences(pGraphicsQueue, 1, &pTransitionFences, false);
		}
	}
#endif
	/************************************************************************/
	// Scene update
	/************************************************************************/
	// Updates uniform data for the given frame index.
	// This includes transform matrices, render target resolution and global information about the scene.
	void updateUniformData(uint currentFrameIdx)
	{
		gRootConstantDrawsceneData.lightColor = gAppSettings.mLightColor;
		gRootConstantDrawsceneData.lightingMode = (uint)gAppSettings.mLightingMode;
		gRootConstantDrawsceneData.outputMode = (uint)gAppSettings.mOutputMode;
		gRootConstantDrawsceneData.CameraPlane.x = gAppSettings.nearPlane;
		gRootConstantDrawsceneData.CameraPlane.y = gAppSettings.farPlane;

		const uint32_t width = pSwapChain->mDesc.mWidth;
		const uint32_t height = pSwapChain->mDesc.mHeight;
		const float    aspectRatioInv = (float)height / width;
		const uint32_t frameIdx = currentFrameIdx;
		PerFrameData*  currentFrame = &gPerFrame[frameIdx];

		mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
		mat4 cameraView = pCameraController->getViewMatrix();
		mat4 cameraProj = mat4::perspective(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

		// Compute light matrices
		Point3 lightSourcePos(50.0f, 000.0f, 450.0f);

		// directional light rotation & translation
		mat4 rotation = mat4::rotationXY(gAppSettings.mSunControl.x, gAppSettings.mSunControl.y);
		mat4 translation = mat4::translation(-vec3(lightSourcePos));
		vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));

		mat4 lightModel = mat4::scale(vec3(SCENE_SCALE));
		mat4 lightView = rotation * translation;
		mat4 lightProj = mat4::orthographic(-600, 600, -950, 350, -1100, 500);

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

		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].sampleCount = MSAASAMPLECOUNT;
		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

		// Cache eye position in object space for cluster culling on the CPU
		currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
		currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
			(inverse(cameraView * cameraModel) * vec4(0, 0, 0, 1)).getXYZ();    // vec4(0,0,0,1) is the camera position in eye space

		/************************************************************************/
		// Sun, God ray
		/************************************************************************/
		SunModel = mat4::identity();

		mat4 sunScale = mat4::scale(vec3(gAppSettings.mSunSize, gAppSettings.mSunSize, gAppSettings.mSunSize));
		mat4 sunTrans = mat4::translation(vec3(-lightDir.getX() * 2000.0f, -lightDir.getY() * 1400.0f, -lightDir.getZ() * 2000.0f));

		SunMVP = cameraProj * cameraView * sunTrans * sunScale;

		gUniformDataSunMatrices.mProjectView = cameraProj * cameraView;
		gUniformDataSunMatrices.mModelMat = sunTrans * sunScale;
		gUniformDataSunMatrices.mLightColor = f4Tov4(gAppSettings.mLightColor);

		vec4 lightPos = SunMVP[3];
		lightPos /= lightPos.getW();

		float2 lightPosSS;

		lightPosSS.x = (lightPos.getX() + 1.0f) * 0.5f;
		lightPosSS.y = (1.0f - lightPos.getY()) * 0.5f;

		gAppSettings.gGodrayInfo.lightPosInSS = lightPosSS;

		/************************************************************************/
		// Skybox
		/************************************************************************/
		cameraView.setTranslation(vec3(0));
		gUniformDataSky.mCamPos = pCameraController->getViewPosition();
		gUniformDataSky.mProjectView = cameraProj * cameraView;

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

		gSCurveInfomation.linearScale = gAppSettings.LinearScale;
		gSCurveInfomation.outputMode = (uint)gAppSettings.mOutputMode;
	}
	/************************************************************************/
	// Process user keyboard input
	/************************************************************************/
	void handleKeyboardInput(float deltaTime)
	{
		UNREF_PARAM(deltaTime);

		// Pressing space holds / unholds triangle filtering results
		if (getKeyUp(KEY_LEFT_TRIGGER))
			gAppSettings.mHoldFilteredResults = !gAppSettings.mHoldFilteredResults;

		if (getKeyUp(KEY_MENU))
			gAppSettings.mFilterTriangles = !gAppSettings.mFilterTriangles;

		if (getKeyUp(KEY_RIGHT_TRIGGER))
			gAppSettings.mRenderMode = (RenderMode)((gAppSettings.mRenderMode + 1) % RENDERMODE_COUNT);

		if (getKeyUp(KEY_BUTTON_X))
			gAppSettings.mRenderLocalLights = !gAppSettings.mRenderLocalLights;

		if (getKeyUp(KEY_BUTTON_Y))
			gAppSettings.mDrawDebugTargets = !gAppSettings.mDrawDebugTargets;
	}
	/************************************************************************/
	// UI
	/************************************************************************/
	void updateDynamicUIElements()
	{
		static OutputMode          gWasHDR10 = gAppSettings.mOutputMode;
		static CurveConversionMode gWasLinear = gAppSettings.mCurveConversionMode;

		if (gAppSettings.mCurveConversionMode != gWasLinear)
		{
			if (gAppSettings.mCurveConversionMode == CurveConversion_LinearScale)
			{
				gSCurveInfomation.UseSCurve = 0.0f;

				gAppSettings.mLinearScale.ShowDynamicProperties(pGuiWindow);
				gAppSettings.mSCurve.HideDynamicProperties(pGuiWindow);
			}
			else
			{
				gAppSettings.mLinearScale.HideDynamicProperties(pGuiWindow);

				if (gAppSettings.mOutputMode != OUTPUT_MODE_SDR)
				{
					gAppSettings.mSCurve.ShowDynamicProperties(pGuiWindow);
					gSCurveInfomation.UseSCurve = 1.0f;
				}
			}

			gWasLinear = gAppSettings.mCurveConversionMode;
		}

		if (gAppSettings.mOutputMode != gWasHDR10)
		{
			if (gAppSettings.mOutputMode == OUTPUT_MODE_SDR)
			{
				gAppSettings.mSCurve.HideDynamicProperties(pGuiWindow);
				gSCurveInfomation.UseSCurve = 0.0f;
			}
			else
			{
				if (gWasHDR10 == OUTPUT_MODE_SDR && gAppSettings.mCurveConversionMode != CurveConversion_LinearScale)
				{
					gAppSettings.mSCurve.ShowDynamicProperties(pGuiWindow);
					gSCurveInfomation.UseSCurve = 1.0f;
				}
			}

			gWasHDR10 = gAppSettings.mOutputMode;
		}

		static OutputMode gWasOutputMode;

		if (gWasOutputMode != gAppSettings.mOutputMode)
		{
			if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
				gAppSettings.mDisplaySetting.ShowDynamicProperties(pGuiWindow);
			else
			{
				if (gWasOutputMode == OUTPUT_MODE_HDR10)
					gAppSettings.mDisplaySetting.HideDynamicProperties(pGuiWindow);
			}
		}

		gWasOutputMode = gAppSettings.mOutputMode;

		static bool gWasAOEnabled = gAppSettings.mEnableHDAO;

		if (gAppSettings.mEnableHDAO != gWasAOEnabled)
		{
			gWasAOEnabled = gAppSettings.mEnableHDAO;
			if (gWasAOEnabled)
			{
				gAppSettings.mDynamicUIControlsAO.ShowDynamicProperties(pGuiWindow);
			}
			else
			{
				gAppSettings.mDynamicUIControlsAO.HideDynamicProperties(pGuiWindow);
			}
		}

		static bool gWasGREnabled = gAppSettings.mEnableGodray;

		if (gAppSettings.mEnableGodray != gWasGREnabled)
		{
			gWasGREnabled = gAppSettings.mEnableGodray;
			if (gWasGREnabled)
			{
				gAppSettings.mDynamicUIControlsGR.ShowDynamicProperties(pGuiWindow);
			}
			else
			{
				gAppSettings.mDynamicUIControlsGR.HideDynamicProperties(pGuiWindow);
			}
		}

#if !defined(_DURANGO) && !defined(METAL) && !defined(__linux__)
		static bool gWasFullscreen = pWindow->fullScreen;
		if (pWindow->fullScreen != gWasFullscreen)
		{
			gWasFullscreen = pWindow->fullScreen;
			if (gWasFullscreen)
			{
				gResolutionProperty = addResolutionProperty(
					pGuiWindow, gResolutionIndex, (uint32_t)gResolutions.size(), gResolutions.data(), []() { gResolutionChange = true; });
			}
			else
			{
				pGuiWindow->RemoveWidget(gResolutionProperty);
			}
		}
#endif
	}
	/************************************************************************/
	// Rendering
	/************************************************************************/

	// Render the shadow mapping pass. This pass updates the shadow map texture
	void drawShadowMapPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
	{
		const char* profileNames[gNumGeomSets] = { "Opaque", "Alpha" };
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadow->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetShadow, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mDesc.mWidth, (float)pRenderTargetShadow->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mDesc.mWidth, pRenderTargetShadow->mDesc.mHeight);

#if defined(METAL)
		Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pIndexBufferAll);

		DescriptorData shadowParams[3] = {};
		shadowParams[0].pName = "vertexPos";
		shadowParams[0].ppBuffers = &pVertexBufferPosition;
		shadowParams[1].pName = "uniforms";
		shadowParams[1].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		shadowParams[2].pName = "filteredTriangles";
		shadowParams[2].ppBuffers = &filteredTrianglesBuffer;
		cmdBindDescriptors(cmd, pRootSignatureVBPass, 3, shadowParams);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i], true);

			Buffer* indirectDrawArguments =
				(gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_SHADOW]
											   : pIndirectDrawArgumentsBufferAll);

			DescriptorData indirectParams[1] = {};
			indirectParams[0].pName = "indirectDrawArgs";
			indirectParams[0].ppBuffers = &indirectDrawArguments;
			cmdBindDescriptors(cmd, pRootSignatureVBPass, 1, indirectParams);
			cmdBindPipeline(cmd, pPipelineShadowPass[i]);

			for (uint32_t m = 0; m < pScene->numMeshes; ++m)
			{
				// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
				//uint32_t materialGeometrySet = (pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
				//if (materialGeometrySet != i)
				//	continue;

				DescriptorData meshParams[2] = {};
				meshParams[0].pName = "perBatch";
				meshParams[0].ppBuffers = &gPerBatchUniformBuffers[m];
				meshParams[1].pName = "diffuseMap";
				meshParams[1].ppTextures = &gDiffuseMaps[pScene->meshes[m].materialId];
				cmdBindDescriptors(cmd, pRootSignatureVBPass, 2, meshParams);
				cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, indirectDrawArguments, m * sizeof(VisBufferIndirectCommand), nullptr, 0);
			}

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
#else
		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pIndexBufferAll;
		Buffer* pIndirectMaterialBuffer =
			gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
		cmdBindIndexBuffer(cmd, pIndexBuffer, 0);

		DescriptorData params[3] = {};
		params[0].pName = "diffuseMaps";
		params[0].mCount = (uint32_t)gDiffuseMaps.size();
		params[0].ppTextures = gDiffuseMaps.data();
		params[1].pName = "indirectMaterialBuffer";
		params[1].ppBuffers = &pIndirectMaterialBuffer;
		params[2].pName = "uniforms";
		params[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureVBPass, 3, params);

		// Position only opaque shadow pass
		Buffer* pVertexBuffersPositionOnly[] = { pVertexBufferPosition };
		cmdBindVertexBuffer(cmd, 1, pVertexBuffersPositionOnly, NULL);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[0], true);
		cmdBindPipeline(cmd, pPipelineShadowPass[0]);
		Buffer* pIndirectBufferPositionOnly = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][0][VIEW_SHADOW]
																			: pIndirectDrawArgumentsBufferAll[0];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[0], pIndirectBufferPositionOnly, 0, pIndirectBufferPositionOnly,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		// Alpha tested shadow pass with extra vetex attribute stream
		Buffer* pVertexBuffers[] = { pVertexBufferPosition, pVertexBufferTexCoord };
		cmdBindVertexBuffer(cmd, 2, pVertexBuffers, NULL);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[1], true);
		cmdBindPipeline(cmd, pPipelineShadowPass[1]);
		Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][1][VIEW_SHADOW]
																: pIndirectDrawArgumentsBufferAll[1];
		cmdExecuteIndirect(
			cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[1], pIndirectBuffer, 0, pIndirectBuffer,
			DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
#endif

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
	// into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
	// to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
	// less memory bandwidth is used.
	void drawVisibilityBufferPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
	{
		const char* profileNames[gNumGeomSets] = { "Opaque", "Alpha" };
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetVBPass->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 1, &pRenderTargetVBPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mDesc.mWidth, (float)pRenderTargetVBPass->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mDesc.mWidth, pRenderTargetVBPass->mDesc.mHeight);

#if defined(METAL)
		Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll);

		DescriptorData vbPassParams[4] = {};
		vbPassParams[0].pName = "vertexPos";
		vbPassParams[0].ppBuffers = &pVertexBufferPosition;
		vbPassParams[1].pName = "vertexTexcoord";
		vbPassParams[1].ppBuffers = &pVertexBufferTexCoord;
		vbPassParams[2].pName = "uniforms";
		vbPassParams[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		vbPassParams[3].pName = "filteredTriangles";
		vbPassParams[3].ppBuffers = &filteredTrianglesBuffer;
		cmdBindDescriptors(cmd, pRootSignatureVBPass, 4, vbPassParams);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i], true);

			Buffer* indirectDrawArguments =
				(gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA]
											   : pIndirectDrawArgumentsBufferAll);

			DescriptorData indirectParams[1] = {};
			indirectParams[0].pName = "indirectDrawArgs";
			indirectParams[0].ppBuffers = &indirectDrawArguments;
			cmdBindDescriptors(cmd, pRootSignatureVBPass, 1, indirectParams);
			cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);

			for (uint32_t m = 0; m < pScene->numMeshes; ++m)
			{
				// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
				uint32_t materialGeometrySet =
					(pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
				if (materialGeometrySet != i)
					continue;

				DescriptorData meshParams[2] = {};
				meshParams[0].pName = "perBatch";
				meshParams[0].ppBuffers = &gPerBatchUniformBuffers[m];
				meshParams[1].pName = "diffuseMap";
				meshParams[1].ppTextures = &gDiffuseMaps[pScene->meshes[m].materialId];
				cmdBindDescriptors(cmd, pRootSignatureVBPass, 2, meshParams);
				cmdExecuteIndirect(cmd, pCmdSignatureVBPass, 1, indirectDrawArguments, m * sizeof(VisBufferIndirectCommand), nullptr, 0);
			}

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
#else
		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll;
		Buffer* pIndirectMaterialBuffer =
			gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
		cmdBindIndexBuffer(cmd, pIndexBuffer, 0);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i], true);
			cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA]
																	: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
#endif

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawVisibilityBufferPass
	// to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index buffer because
	// the triangle positions are calculated internally using vertex_id.
	void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget;
#if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
#else
		pDestinationRenderTarget = pScreenRenderTarget;
#endif
		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestinationRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[gAppSettings.mEnableHDAO]);

#if defined(METAL)
		const uint32_t numDescriptors = 17;
#else
		const uint32_t numDescriptors = 19;
		Buffer* pIndirectBuffers[gNumGeomSets] = { nullptr };
		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			pIndirectBuffers[i] = pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA];
		}
#endif

		DescriptorData vbShadeParams[numDescriptors] = {};
		vbShadeParams[0].pName = "vbTex";
		vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
		vbShadeParams[1].pName = "diffuseMaps";
		vbShadeParams[1].mCount = (uint32_t)gDiffuseMapsPacked.size();
		vbShadeParams[1].ppTextures = gDiffuseMapsPacked.data();
		vbShadeParams[2].pName = "normalMaps";
		vbShadeParams[2].mCount = (uint32_t)gNormalMapsPacked.size();
		vbShadeParams[2].ppTextures = gNormalMapsPacked.data();
		vbShadeParams[3].pName = "specularMaps";
		vbShadeParams[3].mCount = (uint32_t)gSpecularMapsPacked.size();
		vbShadeParams[3].ppTextures = gSpecularMapsPacked.data();
		vbShadeParams[4].pName = "vertexPos";
		vbShadeParams[4].ppBuffers = &pVertexBufferPosition;
		vbShadeParams[5].pName = "vertexTexCoord";
		vbShadeParams[5].ppBuffers = &pVertexBufferTexCoord;
		vbShadeParams[6].pName = "vertexNormal";
		vbShadeParams[6].ppBuffers = &pVertexBufferNormal;
		vbShadeParams[7].pName = "vertexTangent";
		vbShadeParams[7].ppBuffers = &pVertexBufferTangent;
		vbShadeParams[8].pName = "lights";
		vbShadeParams[8].ppBuffers = &pLightsBuffer;
		vbShadeParams[9].pName = "lightClustersCount";
		vbShadeParams[9].ppBuffers = &pLightClustersCount[frameIdx];
		vbShadeParams[10].pName = "lightClusters";
		vbShadeParams[10].ppBuffers = &pLightClusters[frameIdx];
		vbShadeParams[11].pName = "indirectDrawArgs";
#if defined(METAL)
		vbShadeParams[11].ppBuffers =
			gAppSettings.mFilterTriangles ? &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA] : &pIndirectDrawArgumentsBufferAll;
#else
		vbShadeParams[11].mCount = gNumGeomSets;
		vbShadeParams[11].ppBuffers = gAppSettings.mFilterTriangles ? pIndirectBuffers : pIndirectDrawArgumentsBufferAll;
#endif
		vbShadeParams[12].pName = "uniforms";
		vbShadeParams[12].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		vbShadeParams[13].pName = "aoTex";
		vbShadeParams[13].ppTextures = &pRenderTargetAO->pTexture;
		vbShadeParams[14].pName = "shadowMap";
		vbShadeParams[14].ppTextures = &pRenderTargetShadow->pTexture;

		vbShadeParams[15].pName = "RootConstantDrawScene";
		vbShadeParams[15].pRootConstant = &gRootConstantDrawsceneData;

		vbShadeParams[16].pName = "indirectMaterialBuffer";

#if defined(METAL)
		vbShadeParams[16].ppBuffers = &pIndirectMaterialBufferAll;
#else
		vbShadeParams[16].ppBuffers =
			gAppSettings.mFilterTriangles ? &pFilterIndirectMaterialBuffer[frameIdx] : &pIndirectMaterialBufferAll;
		vbShadeParams[17].pName = "filteredIndexBuffer";
		vbShadeParams[17].ppBuffers = gAppSettings.mFilterTriangles ? &pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : &pIndexBufferAll;
		vbShadeParams[18].pName = "meshConstantsBuffer";
		vbShadeParams[18].ppBuffers = &pMeshConstantsBuffer;
#endif

		cmdBindDescriptors(cmd, pRootSignatureVBShade, numDescriptors, vbShadeParams);

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render the scene to perform the Deferred geometry pass. In this pass the (filtered) scene geometry is rendered
	// into a gBuffer, containing per-pixel geometric information such as normals, textures or depth. This information
	// will be used later to calculate per pixel color in the shading pass.
	void drawDeferredPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
	{
		const char* profileNames[gNumGeomSets] = { "Opaque", "Alpha" };
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[i]->mDesc.mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);

		Buffer* pVertexBuffers[] = { pVertexBufferPosition, pVertexBufferTexCoord, pVertexBufferNormal, pVertexBufferTangent };
		cmdBindVertexBuffer(cmd, 4, pVertexBuffers, NULL);

#if defined(METAL)
		Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll);

		DescriptorData deferredPassParams[7] = {};
		deferredPassParams[0].pName = "vertexPos";
		deferredPassParams[0].ppBuffers = &pVertexBufferPosition;
		deferredPassParams[1].pName = "vertexTexcoord";
		deferredPassParams[1].ppBuffers = &pVertexBufferTexCoord;
		deferredPassParams[2].pName = "vertexNormal";
		deferredPassParams[2].ppBuffers = &pVertexBufferNormal;
		deferredPassParams[3].pName = "vertexTangent";
		deferredPassParams[3].ppBuffers = &pVertexBufferTangent;
		deferredPassParams[4].pName = "uniforms";
		deferredPassParams[4].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		deferredPassParams[5].pName = "filteredTriangles";
		deferredPassParams[5].ppBuffers = &filteredTrianglesBuffer;
		deferredPassParams[6].pName = "textureSampler";
		deferredPassParams[6].ppSamplers = &pSamplerBilinear;
		cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 7, deferredPassParams);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i], true);

			Buffer* indirectDrawArguments =
				(gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA]
											   : pIndirectDrawArgumentsBufferAll);

			DescriptorData indirectParams[1] = {};
			indirectParams[0].pName = "indirectDrawArgs";
			indirectParams[0].ppBuffers = &indirectDrawArguments;
			cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 1, indirectParams);
			cmdBindPipeline(cmd, pPipelineDeferredPass[i]);

			for (uint32_t m = 0; m < pScene->numMeshes; ++m)
			{
				// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
				uint32_t materialGeometrySet =
					(pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
				if (materialGeometrySet != i)
					continue;

				DescriptorData meshParams[4] = {};
				meshParams[0].pName = "perBatch";
				meshParams[0].ppBuffers = &gPerBatchUniformBuffers[m];
				meshParams[1].pName = "diffuseMap";
				meshParams[1].ppTextures = &gDiffuseMaps[pScene->meshes[m].materialId];
				meshParams[2].pName = "normalMap";
				meshParams[2].ppTextures = &gNormalMaps[pScene->meshes[m].materialId];
				meshParams[3].pName = "specularMap";
				meshParams[3].ppTextures = &gSpecularMaps[pScene->meshes[m].materialId];
				cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 4, meshParams);
				cmdExecuteIndirect(
					cmd, pCmdSignatureDeferredPass, 1, indirectDrawArguments, m * sizeof(VisBufferIndirectCommand), nullptr, 0);
			}

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
#else
		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll;
		Buffer* pIndirectMaterialBuffer =
			gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
		cmdBindIndexBuffer(cmd, pIndexBuffer, 0);

		DescriptorData params[6] = {};
		params[0].pName = "diffuseMaps";
		params[0].mCount = (uint32_t)gDiffuseMaps.size();
		params[0].ppTextures = gDiffuseMaps.data();
		params[1].pName = "normalMaps";
		params[1].mCount = (uint32_t)gNormalMaps.size();
		params[1].ppTextures = gNormalMaps.data();
		params[2].pName = "specularMaps";
		params[2].mCount = (uint32_t)gSpecularMaps.size();
		params[2].ppTextures = gSpecularMaps.data();
		params[3].pName = "indirectMaterialBuffer";
		params[3].ppBuffers = &pIndirectMaterialBuffer;
		params[4].pName = "uniforms";
		params[4].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		params[5].pName = "meshConstantsBuffer";
		params[5].ppBuffers = &pMeshConstantsBuffer;
		cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 6, params);

		for (uint32_t i = 0; i < gNumGeomSets; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i], true);
			cmdBindPipeline(cmd, pPipelineDeferredPass[i]);
			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA]
																	: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureDeferredPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}
#endif

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawDeferredPass
	// to get per pixel geometry data to calculate the final color.
	void drawDeferredShade(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget;
#if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
#else
		pDestinationRenderTarget = pScreenRenderTarget;
#endif
		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestinationRenderTarget->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineDeferredShadeSrgb[gAppSettings.mEnableHDAO]);

		DescriptorData params[9] = {};
		params[0].pName = "gBufferColor";
		params[0].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture;
		params[1].pName = "gBufferNormal";
		params[1].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture;
		params[2].pName = "gBufferSpecular";
		params[2].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
		params[3].pName = "gBufferSimulation";
		params[3].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture;
		params[4].pName = "gBufferDepth";
		params[4].ppTextures = &pDepthBuffer->pTexture;
		params[5].pName = "shadowMap";
		params[5].ppTextures = &pRenderTargetShadow->pTexture;
		params[6].pName = "uniforms";
		params[6].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		params[7].pName = "aoTex";
		params[7].ppTextures = &pRenderTargetAO->pTexture;
		params[8].pName = "RootConstantDrawScene";
		params[8].pRootConstant = &gRootConstantDrawsceneData;

		cmdBindDescriptors(cmd, pRootSignatureDeferredShade, 9, params);

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render light geometry on the screen to evaluate lighting at those points.
	void drawDeferredShadePointLights(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget;
#if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
#else
		pDestinationRenderTarget = pScreenRenderTarget;
#endif
		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineDeferredShadePointLightSrgb);

		const uint32_t numDescriptors = 9;
		DescriptorData params[numDescriptors] = {};
		params[0].pName = "lights";
		params[0].ppBuffers = &pLightsBuffer;
		params[1].pName = "gBufferColor";
		params[1].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture;
		params[2].pName = "gBufferNormal";
		params[2].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture;
		params[3].pName = "gBufferSpecular";
		params[3].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
		params[4].pName = "gBufferSpecular";
		params[4].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
		params[5].pName = "gBufferSimulation";
		params[5].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture;
		params[6].pName = "gBufferDepth";
		params[6].ppTextures = &pDepthBuffer->pTexture;
		params[7].pName = "uniforms";
		params[7].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		params[8].pName = "RootConstantDrawScene";
		params[8].pRootConstant = &gRootConstantDrawsceneData;

		cmdBindDescriptors(cmd, pRootSignatureDeferredShadePointLight, numDescriptors, params);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferCube, NULL);
		cmdBindIndexBuffer(cmd, pIndexBufferCube, 0);
		cmdDrawIndexedInstanced(cmd, 36, 0, LIGHT_COUNT, 0, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Reads the depth buffer to apply ambient occlusion post process
	void drawHDAO(Cmd* cmd, uint32_t frameIdx)
	{
		struct HDAORootConstants
		{
			float2 g_f2RTSize;             // Used by HDAO shaders for scaling texture coords
			float  g_fHDAORejectRadius;    // HDAO param
			float  g_fHDAOIntensity;       // HDAO param
			float  g_fHDAOAcceptRadius;    // HDAO param
			float  g_fQ;                   // far / (far - near)
			float  g_fQTimesZNear;         // Q * near
		} data;

		const mat4& mainProj = gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_CAMERA].projection;

		data.g_f2RTSize = { (float)pDepthBuffer->mDesc.mWidth, (float)pDepthBuffer->mDesc.mHeight };
		data.g_fHDAOAcceptRadius = gAppSettings.mAcceptRadius;
		data.g_fHDAOIntensity = gAppSettings.mAOIntensity;
		data.g_fHDAORejectRadius = gAppSettings.mRejectRadius;
		data.g_fQ = mainProj[2][2];
		data.g_fQTimesZNear = mainProj[3][2];

		cmdBindRenderTargets(cmd, 1, &pRenderTargetAO, NULL, NULL, NULL, NULL, -1, -1);
		DescriptorData params[2] = {};
		params[0].pName = "g_txDepth";
		params[0].ppTextures = &pDepthBuffer->pTexture;
		params[1].pName = "HDAORootConstants";
		params[1].pRootConstant = &data;
		cmdBindDescriptors(cmd, pRootSignatureAO, 2, params);
		cmdBindPipeline(cmd, pPipelineAO[gAppSettings.mAOQuality - 1]);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	void resolveMSAA(Cmd* cmd, RenderTarget* msaaRT, RenderTarget* destRT)
	{
		// transition world render target to be used as input texture in post process pass
		TextureBarrier barrier = { msaaRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = destRT->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &destRT, NULL, &loadActions, NULL, NULL, -1, -1);
		DescriptorData params[2] = {};
		params[0].pName = "msaaSource";
		params[0].ppTextures = &msaaRT->pTexture;

		cmdBindDescriptors(cmd, pRootSignatureResolve, 1, params);
		cmdBindPipeline(cmd, pPipelineResolve);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	void resolveGodrayMSAA(Cmd* cmd, RenderTarget* msaaRT, RenderTarget* destRT)
	{
		// transition world render target to be used as input texture in post process pass
		TextureBarrier barrier[] = { { msaaRT->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									 { destRT->pTexture, RESOURCE_STATE_RENDER_TARGET } };

		cmdResourceBarrier(cmd, 0, NULL, 2, barrier, true);
		cmdFlushBarriers(cmd);
		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = destRT->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &destRT, NULL, &loadActions, NULL, NULL, -1, -1);
		DescriptorData params[2] = {};
		params[0].pName = "msaaSource";
		params[0].ppTextures = &msaaRT->pTexture;

		cmdBindDescriptors(cmd, pRootSignatureGodrayResolve, 1, params);
		cmdBindPipeline(cmd, pPipelineGodrayResolve);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Executes a compute shader to clear (reset) the the light clusters on the GPU
	void clearLightClusters(Cmd* cmd, uint32_t frameIdx)
	{
		cmdBindPipeline(cmd, pPipelineClearLightClusters);

		DescriptorData params[1] = {};
		params[0].pName = "lightClustersCount";
		params[0].ppBuffers = &pLightClustersCount[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureClearLightClusters, 1, params);

		cmdDispatch(cmd, 1, 1, 1);
	}

	// Executes a compute shader that computes the light clusters on the GPU
	void computeLightClusters(Cmd* cmd, uint32_t frameIdx)
	{
		cmdBindPipeline(cmd, pPipelineClusterLights);

		DescriptorData params[4] = {};
		params[0].pName = "lightClustersCount";
		params[0].ppBuffers = &pLightClustersCount[frameIdx];
		params[1].pName = "lightClusters";
		params[1].ppBuffers = &pLightClusters[frameIdx];
		params[2].pName = "lights";
		params[2].ppBuffers = &pLightsBuffer;
		params[3].pName = "uniforms";
		params[3].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureClusterLights, 4, params);

		cmdDispatch(cmd, LIGHT_COUNT, 1, 1);
	}

	// Executes the compute shader that performs triangle filtering on the GPU.
	// This step performs different visibility tests per triangle to determine whether they
	// potentially affect to the final image or not.
	// The results of executing this shader are stored in:
	// - pFilteredTriangles: list of triangle IDs that passed the culling tests
	// - pIndirectDrawArguments: the vertexCount member of this structure is calculated in order to
	//						 indicate the renderer the amount of vertices per batch to render.
#if defined(METAL)
	void filterTriangles(Cmd* cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk, uint64_t bufferOffset)
	{
		// Check if there are batches to filter
		if (batchChunk->currentBatchCount == 0)
			return;

		// Select current batch data buffers to filter
		Buffer* batchDataBuffer = batchChunk->batchDataBuffer;

		// Copy batch data to GPU buffer
		BufferUpdateDesc dataBufferUpdate = { batchDataBuffer, batchChunk->batches, 0, bufferOffset,
											  batchChunk->currentBatchCount * sizeof(FilterBatchData) };
		updateResource(&dataBufferUpdate, true);

		DescriptorData params[1] = {};
		params[0].pName = "perBatch";
		params[0].pOffsets = &bufferOffset;
		params[0].ppBuffers = &batchDataBuffer;
		cmdBindDescriptors(cmd, pRootSignatureTriangleFiltering, 1, params);

		// This compute shader executes one thread per triangle and one group per batch
		ASSERT(pShaderTriangleFiltering->mReflection.mStageReflections[0].mNumThreadsPerGroup[0] == CLUSTER_SIZE);
		cmdDispatch(cmd, batchChunk->currentBatchCount, 1, 1);

		// Reset batch chunk to start adding triangles to it
		batchChunk->currentBatchCount = 0;
		batchChunk->currentDrawCallCount = 0;
	}
#else
	void filterTriangles(Cmd* cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk)
	{
		UNREF_PARAM(frameIdx);
		// Check if there are batches to filter
		if (batchChunk->currentBatchCount == 0)
			return;

		uint32_t batchSize = batchChunk->currentBatchCount * sizeof(SmallBatchData);
		RingBufferOffset offset = getUniformBufferOffset(pFilterBatchDataBuffer[frameIdx], batchSize);
		BufferUpdateDesc updateDesc = { offset.pBuffer, batchChunk->batches, 0, offset.mOffset, batchSize };
		updateResource(&updateDesc, true);

		DescriptorData params[1] = {};
		params[0].pName = "batchData";
		params[0].pOffsets = &offset.mOffset;
		params[0].ppBuffers = &offset.pBuffer;
		cmdBindDescriptors(cmd, pRootSignatureTriangleFiltering, 1, params);
		cmdDispatch(cmd, batchChunk->currentBatchCount, 1, 1);

		// Reset batch chunk to start adding triangles to it
		batchChunk->currentBatchCount = 0;
		batchChunk->currentDrawCallCount = 0;
	}
#endif

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
	void triangleFilteringPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Triangle Filtering Pass", true);

		gPerFrame[frameIdx].gTotalClusters = 0;
		gPerFrame[frameIdx].gCulledClusters = 0;

#if defined(METAL)
		/************************************************************************/
		// Clear previous indirect arguments
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers", true);
		DescriptorData clearParams[3] = {};
		clearParams[0].pName = "indirectDrawArgsCamera";
		clearParams[0].ppBuffers = &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA];
		clearParams[1].pName = "indirectDrawArgsShadow";
		clearParams[1].ppBuffers = &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_SHADOW];
		clearParams[2].pName = "rootConstant";
		clearParams[2].pRootConstant = (void*)&pScene->numMeshes;

		cmdBindPipeline(cmd, pPipelineClearBuffers);
		cmdBindDescriptors(cmd, pRootSignatureClearBuffers, 3, clearParams);
		uint32_t numGroups = (pScene->numMeshes / CLEAR_THREAD_COUNT) + 1;

		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		DescriptorData filterParams[6] = {};
		filterParams[0].pName = "indirectDrawArgsCamera";
		filterParams[0].ppBuffers = &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA];
		filterParams[1].pName = "indirectDrawArgsShadow";
		filterParams[1].ppBuffers = &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_SHADOW];
		filterParams[2].pName = "uniforms";
		filterParams[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		filterParams[3].pName = "vertexPos";
		filterParams[3].ppBuffers = &pVertexBufferPosition;
		filterParams[4].pName = "filteredTrianglesCamera";
		filterParams[4].ppBuffers = &pFilteredIndexBuffer[frameIdx][VIEW_CAMERA];
		filterParams[5].pName = "filteredTrianglesShadow";
		filterParams[5].ppBuffers = &pFilteredIndexBuffer[frameIdx][VIEW_SHADOW];
		cmdBindDescriptors(cmd, pRootSignatureTriangleFiltering, 6, filterParams);

		// Iterate mesh clusters and perform cluster culling
		uint32_t batchBufferOffset = 0;
		for (uint32_t i = 0; i < pScene->numMeshes; i++)
		{
			const MeshIn*   mesh = pScene->meshes + i;
			const Material* material = pScene->materials + mesh->materialId;
			gPerFrame[frameIdx].gTotalClusters += mesh->clusterCount;
			for (uint32_t j = 0; j < mesh->clusterCount; j++)
			{
				const Cluster*        cluster = mesh->clusters + j;
				const ClusterCompact* pClusterCompact = &mesh->clusterCompacts[j];

				// Perform CPU-based cluster culling before adding the cluster for GPU filtering
				if (cullCluster(cluster, gPerFrame[frameIdx].gEyeObjectSpace))
				{
					gPerFrame[frameIdx].gCulledClusters++;
					continue;
				}

				// The cluster was not culled: add cluster to the cluster batch chunk for the GPU filtering step
				addClusterToBatchChunk(pClusterCompact, mesh, i, material->twoSided, pFilterBatchChunk[frameIdx]);

				// Check if we filled the whole batch of clusters
				if (pFilterBatchChunk[frameIdx]->currentBatchCount >= BATCH_COUNT)
				{
					uint32_t oldBatchOffset = batchBufferOffset + pFilterBatchChunk[frameIdx]->currentBatchCount * sizeof(FilterBatchData);
					filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx], batchBufferOffset);
					batchBufferOffset = oldBatchOffset;
				}
			}
		}

		// End of the mesh, filter the remaining clusters of the current batch
		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx], batchBufferOffset);

		// Flush the pending resource updates.
		flushResourceUpdates();
#else
		/************************************************************************/
		// Barriers to transition uncompacted draw buffer to uav
		/************************************************************************/
		BufferBarrier uavBarriers[gNumViews] = {};
		for (uint32_t i = 0; i < gNumViews; ++i)
			uavBarriers[i] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS };
		cmdResourceBarrier(cmd, gNumViews, uavBarriers, 0, NULL, false);
		/************************************************************************/
		// Clear previous indirect arguments
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers", true);
		DescriptorData clearParams[3] = {};
		clearParams[0].pName = "indirectDrawArgsBufferAlpha";
		clearParams[0].mCount = gNumViews;
		clearParams[0].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED];
		clearParams[1].pName = "indirectDrawArgsBufferNoAlpha";
		clearParams[1].mCount = gNumViews;
		clearParams[1].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE];
		clearParams[2].pName = "uncompactedDrawArgs";
		clearParams[2].mCount = gNumViews;
		clearParams[2].ppBuffers = pUncompactedDrawArgumentsBuffer[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureClearBuffers, 3, clearParams);
		cmdBindPipeline(cmd, pPipelineClearBuffers);
		uint32_t numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers Synchronization", true);
		uint32_t numBarriers = (gNumViews * gNumGeomSets) + gNumViews;
		Buffer** clearBarriers = (Buffer**)alloca(numBarriers * sizeof(Buffer*));
		uint32_t index = 0;
		for (uint32_t i = 0; i < gNumViews; ++i)
		{
			clearBarriers[index++] = pUncompactedDrawArgumentsBuffer[frameIdx][i];
			clearBarriers[index++] = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i];
			clearBarriers[index++] = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i];
		}
		cmdSynchronizeResources(cmd, numBarriers, clearBarriers, 0, NULL, false);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/
		uint32_t currentSmallBatchChunk = 0;
		uint accumDrawCount = 0;
		uint accumNumTriangles = 0;
		uint accumNumTrianglesAtStartOfBatch = 0;
		uint batchStart = 0;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Filter Triangles", true);
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		DescriptorData filterParams[6] = {};
		filterParams[0].pName = "vertexDataBuffer";
		filterParams[0].ppBuffers = &pVertexBufferPosition;
		filterParams[1].pName = "indexDataBuffer";
		filterParams[1].ppBuffers = &pIndexBufferAll;
		filterParams[2].pName = "meshConstantsBuffer";
		filterParams[2].ppBuffers = &pMeshConstantsBuffer;
		filterParams[3].pName = "filteredIndicesBuffer";
		filterParams[3].mCount = gNumViews;
		filterParams[3].ppBuffers = pFilteredIndexBuffer[frameIdx];
		filterParams[4].pName = "uncompactedDrawArgs";
		filterParams[4].mCount = gNumViews;
		filterParams[4].ppBuffers = pUncompactedDrawArgumentsBuffer[frameIdx];
		filterParams[5].pName = "uniforms";
		filterParams[5].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureTriangleFiltering, 6, filterParams);
#if 0
#define SORT_CLUSTERS 1

#if SORT_CLUSTERS
		uint32_t maxClusterCount = 0;
		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			Mesh* drawBatch = &pScene->meshes[i];
			maxClusterCount = max(maxClusterCount, drawBatch->clusterCount);
		}
		Cluster** temporaryClusters = (Cluster**)conf_malloc(sizeof(Cluster) * maxClusterCount);
#endif

		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			Mesh* drawBatch = &pScene->meshes[i];
			FilterBatchChunk* batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];

#if SORT_CLUSTERS
			uint32_t temporaryClusterCount = 0;

			//Cull clusters
			for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
			{
				++gPerFrame[frameIdx].gTotalClusters;
				Cluster* clusterInfo = &drawBatch->clusters[j];

				// Run cluster culling
				{
					//Calculate distance from the camera
					vec4 p0 = vec4((f3Tov3(clusterInfo->aabbMin) + f3Tov3(clusterInfo->aabbMax)) * .5f, 1.0f);
					mat4 vm = gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_CAMERA].vp;

#if defined(VECTORMATH_MODE_SCALAR) && 0
					float z =
						vm.getCol0().getZ() * p0.getX() +
						vm.getCol1().getZ() * p0.getY() +
						vm.getCol2().getZ() * p0.getZ() +
						vm.getCol3().getZ() * p0.getW();
					float w =
						vm.getCol0().getW() * p0.getX() +
						vm.getCol1().getW() * p0.getY() +
						vm.getCol2().getW() * p0.getZ() +
						vm.getCol3().getW() * p0.getW();
					clusterInfo->distanceFromCamera = z / w;
#else
					p0 = vm * p0;
					clusterInfo->distanceFromCamera = p0.getZ() / p0.getW();
#endif

					if (std::isnan(clusterInfo->distanceFromCamera))
						clusterInfo->distanceFromCamera = 0;

					temporaryClusters[temporaryClusterCount++] = clusterInfo;

				}
			}

			//Sort the clusters
			sortClusters(temporaryClusters, temporaryClusterCount);

			//Add clusters to batch chunk
			for (uint32_t j = 0; j < temporaryClusterCount; ++j)
			{
				addClusterToBatchChunk(
					temporaryClusters[j],
					batchStart,
					accumDrawCount,
					accumNumTrianglesAtStartOfBatch,
					i,
					batchChunk);
				accumNumTriangles += temporaryClusters[j]->triangleCount;

				// check to see if we filled the batch
				if (batchChunk->currentBatchCount >= BATCH_COUNT)
				{
					++accumDrawCount;

					// run the triangle filtering and switch to the next small batch chunk
					filterTriangles(cmd, frameIdx, batchChunk);
					currentSmallBatchChunk = (currentSmallBatchChunk + 1) % gSmallBatchChunkCount;
					batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];

					batchStart = 0;
					accumNumTrianglesAtStartOfBatch = accumNumTriangles;
				}
			}
#else
			//Cull clusters
			for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
			{
				++gPerFrame[frameIdx].gTotalClusters;
				Cluster* clusterInfo = &drawBatch->clusters[j];

				// Run cluster culling
				{
					addClusterToBatchChunk(
						clusterInfo,
						batchStart,
						accumDrawCount,
						accumNumTrianglesAtStartOfBatch,
						i,
						batchChunk);
					accumNumTriangles += clusterInfo->triangleCount;
				}

				// check to see if we filled the batch
				if (batchChunk->currentBatchCount >= BATCH_COUNT)
				{
					++accumDrawCount;

					// run the triangle filtering and switch to the next small batch chunk
					filterTriangles(cmd, frameIdx, batchChunk);
					currentSmallBatchChunk = (currentSmallBatchChunk + 1) % gSmallBatchChunkCount;
					batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];

					batchStart = 0;
					accumNumTrianglesAtStartOfBatch = accumNumTriangles;
				}

			}
#endif

			// end of that mash, set it up so we can add the next mesh to this culling batch
			if (batchChunk->currentBatchCount > 0)
			{
				FilterBatchChunk* batchChunk2 = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
				++accumDrawCount;

				batchStart = batchChunk2->currentBatchCount;
				accumNumTrianglesAtStartOfBatch = accumNumTriangles;
			}

		}
#if SORT_CLUSTERS
		conf_free(temporaryClusters);
#endif

#else
		for (uint32_t i = 0; i < pScene->numMeshes; ++i)
		{
			MeshIn*           drawBatch = &pScene->meshes[i];
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
					addClusterToBatchChunk(clusterCompactInfo, batchStart, accumDrawCount, accumNumTrianglesAtStartOfBatch, i, batchChunk);
					accumNumTriangles += clusterCompactInfo->triangleCount;
				}

				// check to see if we filled the batch
				if (batchChunk->currentBatchCount >= BATCH_COUNT)
				{
					++accumDrawCount;

					// run the triangle filtering and switch to the next small batch chunk
					filterTriangles(cmd, frameIdx, batchChunk);
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
#endif

		gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = accumDrawCount;
		gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = accumDrawCount;

		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx][currentSmallBatchChunk]);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		for (uint32_t i = 0; i < gNumViews; ++i)
			uavBarriers[i] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, gNumViews, uavBarriers, 0, NULL, false);
		/************************************************************************/
		// Batch compaction
		/************************************************************************/

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Batch Compaction", true);
		cmdBindPipeline(cmd, pPipelineBatchCompaction);
		DescriptorData compactParams[5] = {};
		compactParams[0].pName = "materialProps";
		compactParams[0].ppBuffers = &pMaterialPropertyBuffer;
		compactParams[1].pName = "indirectMaterialBuffer";
		compactParams[1].ppBuffers = &pFilterIndirectMaterialBuffer[frameIdx];
		compactParams[2].pName = "indirectDrawArgsBufferAlpha";
		compactParams[2].mCount = gNumViews;
		compactParams[2].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED];
		compactParams[3].pName = "indirectDrawArgsBufferNoAlpha";
		compactParams[3].mCount = gNumViews;
		compactParams[3].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE];
		compactParams[4].pName = "uncompactedDrawArgs";
		compactParams[4].mCount = gNumViews;
		compactParams[4].ppBuffers = pUncompactedDrawArgumentsBuffer[frameIdx];
		cmdBindDescriptors(cmd, pRootSignatureBatchCompaction, 5, compactParams);
		numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		/************************************************************************/
		/************************************************************************/
#endif

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	// This is the main scene rendering function. It shows the different steps / rendering passes.
	void drawScene(Cmd* cmd, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Shadow Pass", true);

		if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
		{
			TextureBarrier rtBarriers[] = {
				{ pRenderTargetVBPass->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetShadow->pTexture, RESOURCE_STATE_DEPTH_WRITE },
				{ pRenderTargetAO->pTexture, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 3, rtBarriers, true);
		}
		else if (gAppSettings.mRenderMode == RENDERMODE_DEFERRED)
		{
			TextureBarrier rtBarriers[] = {
				{ pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetShadow->pTexture, RESOURCE_STATE_DEPTH_WRITE },
				{ pRenderTargetAO->pTexture, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, DEFERRED_RT_COUNT + 2, rtBarriers, true);
		}

		cmdFlushBarriers(cmd);

		drawShadowMapPass(cmd, pGraphicsGpuProfiler, frameIdx);
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);

		if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
		{
			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "VB Filling Pass", true);
			drawVisibilityBufferPass(cmd, pGraphicsGpuProfiler, frameIdx);
			TextureBarrier barriers[] = {
				{ pRenderTargetVBPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetShadow->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			};

			cmdResourceBarrier(cmd, 0, NULL, 3, barriers, true);
			cmdFlushBarriers(cmd);
			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);

			if (gAppSettings.mEnableHDAO)
			{
				cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "HDAO Pass", true);
				drawHDAO(cmd, frameIdx);
				cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
			}

			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "VB Shading Pass", true);

			TextureBarrier aoBarrier = { pRenderTargetAO->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, NULL, 1, &aoBarrier, false);

			drawVisibilityBufferShade(cmd, frameIdx);

			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
		}
		else if (gAppSettings.mRenderMode == RENDERMODE_DEFERRED)
		{
			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "GBuffer Pass", true);
			drawDeferredPass(cmd, pGraphicsGpuProfiler, frameIdx);
			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);

			TextureBarrier barriers[] = {
				{ pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetShadow->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, DEFERRED_RT_COUNT + 2, barriers, true);
			cmdFlushBarriers(cmd);

			if (gAppSettings.mEnableHDAO)
			{
				cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "HDAO Pass", true);
				drawHDAO(cmd, frameIdx);
				cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
			}

			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Shading Pass", true);

			TextureBarrier aoBarrier = { pRenderTargetAO->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, NULL, 1, &aoBarrier, false);

			drawDeferredShade(cmd, frameIdx);

			if (gAppSettings.mRenderLocalLights)
			{
				drawDeferredShadePointLights(cmd, frameIdx);
			}
			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
		}

#if (MSAASAMPLECOUNT > 1)
		// Pixel Puzzle needs the unresolved MSAA texture
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Resolve Pass", true);
		resolveMSAA(cmd, pRenderTargetMSAA, pScreenRenderTarget);
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
#endif
	}

	void LoadSkybox()
	{
		Texture*       pPanoSkybox = NULL;
		Shader*        pPanoToCubeShader = NULL;
		RootSignature* pPanoToCubeRootSignature = NULL;
		Pipeline*      pPanoToCubePipeline = NULL;

		Sampler* pSkyboxSampler = NULL;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = ImageFormat::RGBA16F;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mSrgb = false;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pSkybox;
		addResource(&skyboxLoadDesc);

		// Load the skybox panorama texture.
		TextureLoadDesc panoDesc = {};
#ifndef TARGET_IOS
		panoDesc.mRoot = FSR_Textures;
#else
		panoDesc.mRoot = FSRoot::FSR_Absolute;    // Resources on iOS are bundled with the application.
#endif
		panoDesc.mUseMipmaps = true;
		panoDesc.pFilename = "daytime.hdr";
		//panoDesc.pFilename = "LA_Helipad.hdr";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;

		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);

		ComputePipelineDesc pipelineSettings = { 0 };
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addComputePipeline(pRenderer, &pipelineSettings, &pPanoToCubePipeline);

		// Since this happens on iniatilization, use the first cmd/fence pair available.
		Cmd* cmd = ppCmds[0];

		// Compute the BRDF Integration map.
		beginCmd(cmd);

		TextureBarrier uavBarriers[1] = { { pSkybox, RESOURCE_STATE_UNORDERED_ACCESS } };
		cmdResourceBarrier(cmd, 0, NULL, 1, uavBarriers, false);

		DescriptorData params[2] = {};

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(cmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 1, params);

		struct Data
		{
			uint mip;
			uint textureSize;
		} data = { 0, gSkyboxSize };

		for (int i = 0; i < gSkyboxMips; i++)
		{
			data.mip = i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pSkybox;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 2, params);

			const uint32_t* pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers, false);

		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[1] = { { pSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers2, false);

		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pTransitionFences, 0, 0, 0, 0);
		waitForFences(pGraphicsQueue, 1, &pTransitionFences, false);

		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };

		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

		const char*       pSkyboxSamplerName = "skyboxSampler";
		RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
		skyboxRootDesc.mStaticSamplerCount = 1;
		skyboxRootDesc.ppStaticSamplerNames = &pSkyboxSamplerName;
		skyboxRootDesc.ppStaticSamplers = &pSamplerBilinear;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSingatureSkybox);

		//Generate sky box vertex buffer
		float skyBoxPoints[] = {
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
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		skyboxVbDesc.ppBuffer = &pSkyboxVertexBuffer;
		addResource(&skyboxVbDesc);

		tinystl::string sunFullPath = FileSystem::FixPath(gSunName, FSRoot::FSR_Meshes);
		loadModel(sunFullPath, pSunVertexBuffer, SunVertexCount, pSunIndexBuffer, SunIndexCount);
	}

	void drawSkybox(Cmd* cmd, int frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Draw Skybox", true);

		// Transfer our render target to a render target state
		TextureBarrier barrier[] = { { pScreenRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier, true);

		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mDesc.mWidth, (float)pScreenRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mDesc.mWidth, pScreenRenderTarget->mDesc.mHeight);

		// Draw the skybox
		cmdBindPipeline(cmd, pSkyboxPipeline);

		DescriptorData skyParams[2] = {};
		skyParams[0].pName = "RootConstantCameraSky";
		skyParams[0].pRootConstant = &gUniformDataSky;
		skyParams[1].pName = "skyboxTex";
		skyParams[1].ppTextures = &pSkybox;

		cmdBindDescriptors(cmd, pRootSingatureSkybox, 2, skyParams);
		cmdBindVertexBuffer(cmd, 1, &pSkyboxVertexBuffer, NULL);

		cmdDraw(cmd, 36, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	void drawGodray(Cmd* cmd, uint gPresentFrameIdx)
	{
		TextureBarrier barrierTwo[2] = { { pScreenRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
										 { pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE } };

		cmdResourceBarrier(cmd, 0, NULL, 2, barrierTwo, true);
		cmdFlushBarriers(cmd);

		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "God ray", true);

		TextureBarrier barrier[] = { { pRenderTargetSun->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier, false);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetSun->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTargetSun, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetSun->mDesc.mWidth, (float)pRenderTargetSun->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetSun->mDesc.mWidth, pRenderTargetSun->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineSunPass);
		DescriptorData sunParams[1] = {};
		sunParams[0].pName = "UniformBufferSunMatrices";
		sunParams[0].ppBuffers = &pUniformBufferSun[gPresentFrameIdx];

		cmdBindDescriptors(cmd, pRootSigSunPass, 1, sunParams);
		cmdBindVertexBuffer(cmd, 1, &pSunVertexBuffer, NULL);
		cmdBindIndexBuffer(cmd, pSunIndexBuffer, 0);

		cmdDrawIndexed(cmd, SunIndexCount, 0, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier barrier2[] = { { pRenderTargetSun->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									  { pRenderTargetGodRayA->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 2, barrier2, false);

#if (MSAASAMPLECOUNT > 1)
		// Pixel Puzzle needs the unresolved MSAA texture
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Resolve Pass", true);
		resolveGodrayMSAA(cmd, pRenderTargetSun, pRenderTargetSunResolved);
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);

		TextureBarrier barrier33[] = { { pRenderTargetSunResolved->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier33, false);
#endif

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetGodRayA->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTargetGodRayA, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetGodRayA->mDesc.mWidth, (float)pRenderTargetGodRayA->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetGodRayA->mDesc.mWidth, pRenderTargetGodRayA->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineGodRayPass);
		DescriptorData GodrayParams[3] = {};
		GodrayParams[0].pName = "uTex0";
#if (MSAASAMPLECOUNT > 1)
		GodrayParams[0].ppTextures = &pRenderTargetSunResolved->pTexture;
#else
		GodrayParams[0].ppTextures = &pRenderTargetSun->pTexture;
#endif
		GodrayParams[1].pName = "uSampler0";
		GodrayParams[1].ppSamplers = &pSamplerBilinearClamp;
		GodrayParams[2].pName = "RootConstantGodrayInfo";
		GodrayParams[2].pRootConstant = &gAppSettings.gGodrayInfo;

		cmdBindDescriptors(cmd, pRootSigGodRayPass, 3, GodrayParams);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier barrier3[] = { { pRenderTargetGodRayA->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, barrier3, false);

		for (uint loop = 1; loop < gAppSettings.gGodrayInteration; loop++)
		{
			TextureBarrier barrier2[] = { { pRenderTargetGodRayB->pTexture, RESOURCE_STATE_RENDER_TARGET } };
			cmdResourceBarrier(cmd, 0, NULL, 1, barrier2, false);

			//LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = pRenderTargetGodRayB->mDesc.mClearValue;

			cmdBindRenderTargets(cmd, 1, &pRenderTargetGodRayB, NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(
				cmd, 0.0f, 0.0f, (float)pRenderTargetGodRayB->mDesc.mWidth, (float)pRenderTargetGodRayB->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetGodRayB->mDesc.mWidth, pRenderTargetGodRayB->mDesc.mHeight);

			cmdBindPipeline(cmd, pPipelineGodRayPass);
			DescriptorData GodrayParams[3] = {};
			GodrayParams[0].pName = "uTex0";
			GodrayParams[0].ppTextures = &pRenderTargetGodRayA->pTexture;
			GodrayParams[1].pName = "uSampler0";
			GodrayParams[1].ppSamplers = &pSamplerBilinearClamp;
			GodrayParams[2].pName = "RootConstantGodrayInfo";
			GodrayParams[2].pRootConstant = &gAppSettings.gGodrayInfo;

			cmdBindDescriptors(cmd, pRootSigGodRayPass, 3, GodrayParams);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			TextureBarrier barrier3[] = { { pRenderTargetGodRayB->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
			cmdResourceBarrier(cmd, 0, NULL, 1, barrier3, false);

			//pingpong
			RenderTarget* tempRT = pRenderTargetGodRayB;
			pRenderTargetGodRayB = pRenderTargetGodRayA;
			pRenderTargetGodRayA = tempRT;
		}

		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	void drawColorconversion(Cmd* cmd)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Curve Conversion", true);

		// Transfer our render target to a render target state
		TextureBarrier barrierCurveConversion[] = { { pScreenRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
													{ pCurveConversionRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET } };
		cmdResourceBarrier(cmd, 0, NULL, 2, barrierCurveConversion, false);

		cmdBindRenderTargets(cmd, 1, &pCurveConversionRenderTarget, NULL, NULL, NULL, NULL, -1, -1);

		//CurveConversion
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pCurveConversionRenderTarget->mDesc.mWidth, (float)pCurveConversionRenderTarget->mDesc.mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pCurveConversionRenderTarget->mDesc.mWidth, pCurveConversionRenderTarget->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineCurveConversionPass);
		DescriptorData CurveConversionParams[3] = {};
		CurveConversionParams[0].pName = "SceneTex";
		CurveConversionParams[0].ppTextures = &pScreenRenderTarget->pTexture;
		CurveConversionParams[1].pName = "uSampler0";
		CurveConversionParams[1].ppSamplers = &pSamplerBilinearClamp;
		CurveConversionParams[2].pName = "GodRayTex";
		CurveConversionParams[2].ppTextures = &pRenderTargetGodRayA->pTexture;

		cmdBindDescriptors(cmd, pRootSigCurveConversionPass, 3, CurveConversionParams);

		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		pScreenRenderTarget = pCurveConversionRenderTarget;

		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	void presentImage(Cmd* const cmd, Texture* pSrc, RenderTarget* pDstCol)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Present Image", true);

		TextureBarrier barrier[] = { { pSrc, RESOURCE_STATE_SHADER_RESOURCE }, { pDstCol->pTexture, RESOURCE_STATE_RENDER_TARGET } };

		cmdResourceBarrier(cmd, 0, NULL, 2, barrier, true);
		cmdFlushBarriers(cmd);

		cmdBindRenderTargets(cmd, 1, &pDstCol, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDstCol->mDesc.mWidth, (float)pDstCol->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDstCol->mDesc.mWidth, pDstCol->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelinePresentPass);
		DescriptorData params[3] = {};
		params[0].pName = "uTex0";
		params[0].ppTextures = &pSrc;
		params[1].pName = "uSampler0";
		params[1].ppSamplers = &pSamplerBilinear;
		params[2].pName = "RootConstantSCurveInfo";
		params[2].pRootConstant = &gSCurveInfomation;

		cmdBindDescriptors(cmd, pRootSigPresentPass, 3, params);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		TextureBarrier barrierPresent = { pDstCol->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, &barrierPresent, true);

		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	// Draw GUI / 2D elements
	void drawGUI(Cmd* cmd, uint32_t frameIdx)
	{
		UNREF_PARAM(frameIdx);
#if !defined(TARGET_IOS)
		cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, NULL, NULL, NULL, -1, -1);

		gTimer.GetUSec(true);
		drawDebugText(cmd, 8.0f, 15.0f, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

#if 1
		// NOTE: Realtime GPU Profiling is not supported on Metal.
#ifndef METAL
		if (gAppSettings.mAsyncCompute)
		{
			if (gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
			{
				float time =
					max((float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f, (float)pComputeGpuProfiler->mCumulativeTime * 1000.0f);
				drawDebugText(cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", time), &gFrameTimeDraw);

				drawDebugText(
					cmd, 8.0f, 65.0f, tinystl::string::format("Compute Queue %f ms", (float)pComputeGpuProfiler->mCumulativeTime * 1000.0f),
					&gFrameTimeDraw);
				drawDebugGpuProfile(cmd, 8.0f, 90.0f, pComputeGpuProfiler, NULL);
				drawDebugText(
					cmd, 8.0f, 300.0f,
					tinystl::string::format("Graphics Queue %f ms", (float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f),
					&gFrameTimeDraw);
				drawDebugGpuProfile(cmd, 8.0f, 325.0f, pGraphicsGpuProfiler, NULL);
			}
			else
			{
				float time = (float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f;
				drawDebugText(cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", time), &gFrameTimeDraw);

				drawDebugGpuProfile(cmd, 8.0f, 65.0f, pGraphicsGpuProfiler, NULL);
			}
		}
		else
		{
			drawDebugText(
				cmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f),
				&gFrameTimeDraw);
			drawDebugGpuProfile(cmd, 8.0f, 65.0f, pGraphicsGpuProfiler, NULL);
		}
#endif

		// Draw Debug Textures
#if (MSAASAMPLECOUNT == 1)
		if (gAppSettings.mDrawDebugTargets)
		{
			float scale = 0.15f;

			if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
			{
				RenderTarget* pVBRTs[] = { pRenderTargetVBPass, pRenderTargetAO };

				float2 screenSize = { (float)pRenderTargetVBPass->mDesc.mWidth, (float)pRenderTargetVBPass->mDesc.mHeight };
				float2 texSize = screenSize * scale;
				float2 texPos = screenSize + float2(0.0f, -texSize.getY());

				for (uint32_t i = 0; i < sizeof(pVBRTs) / sizeof(pVBRTs[0]); ++i)
				{
					texPos.setX(texPos.getX() - texSize.getX());
					drawDebugTexture(cmd, texPos.x, texPos.y, texSize.x, texSize.y, pVBRTs[i]->pTexture, 1.0f, 1.0f, 1.0f);
				}

				screenSize = { (float)pRenderTargetVBPass->mDesc.mHeight, (float)pRenderTargetVBPass->mDesc.mHeight };
				texSize = screenSize * scale;
				texPos.setX(texPos.getX() - texSize.getX());
				drawDebugTexture(cmd, texPos.x, texPos.y, texSize.x, texSize.y, pRenderTargetShadow->pTexture, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				RenderTarget* pDeferredRTs[] = { pRenderTargetDeferredPass[0], pRenderTargetDeferredPass[1], pRenderTargetDeferredPass[2],
												 pRenderTargetAO };

				float2 screenSize = { (float)pDeferredRTs[0]->mDesc.mWidth, (float)pDeferredRTs[0]->mDesc.mHeight };
				float2 texSize = screenSize * scale;
				float2 texPos = screenSize + float2(0.0f, -texSize.getY());

				for (uint32_t i = 0; i < sizeof(pDeferredRTs) / sizeof(pDeferredRTs[0]); ++i)
				{
					texPos.setX(texPos.getX() - texSize.getX());
					drawDebugTexture(cmd, texPos.x, texPos.y, texSize.x, texSize.y, pDeferredRTs[i]->pTexture, 1.0f, 1.0f, 1.0f);
				}

				screenSize = { (float)pDeferredRTs[0]->mDesc.mHeight, (float)pDeferredRTs[0]->mDesc.mHeight };
				texSize = screenSize * scale;
				texPos.setX(texPos.getX() - texSize.getX());
				drawDebugTexture(cmd, texPos.x, texPos.y, texSize.x, texSize.y, pRenderTargetShadow->pTexture, 1.0f, 1.0f, 1.0f);
			}
		}
#endif
		gAppUI.Gui(pGuiWindow);

#endif

		gAppUI.Draw(cmd);
#endif

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}
	/************************************************************************/
	// Event handling
	/************************************************************************/
	// Set the camera handlers
	static bool onInputEventHandler(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

DEFINE_APPLICATION_MAIN(VisibilityBuffer)
