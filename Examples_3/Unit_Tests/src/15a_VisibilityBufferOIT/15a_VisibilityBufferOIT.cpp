/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/IThread.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "Geometry.h"

#if defined(XBOX)
#include "../../../Xbox/Common_3/Graphics/Direct3D12/Direct3D12X.h"
#include "../../../Xbox/Common_3/Graphics/IESRAMManager.h"
#define BEGINALLOCATION(X, O) esramBeginAllocation(pRenderer->mD3D12.pESRAMManager, X, O)
#define ALLOCATIONOFFSET() esramGetCurrentOffset(pRenderer->mD3D12.pESRAMManager)
#define ENDALLOCATION(X) esramEndAllocation(pRenderer->mD3D12.pESRAMManager)
#else
#define BEGINALLOCATION(X, O)
#define ALLOCATIONOFFSET() 0u
#define ENDALLOCATION(X)
#endif

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

ThreadSystem* pThreadSystem;

#define SCENE_SCALE 50.0f

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

struct UniformShadingData
{
	float4 lightColor;
	uint   lightingMode;
	uint   outputMode;
	float4 CameraPlane;    //x : near, y : far
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

//Camera Walking
static float          gCameraWalkingTime = 0.0f;
eastl::vector<float3> gPositionsDirections;
float3                gCameraPathData[29084];

uint  gCameraPoints;
float gTotalElpasedTime;
/************************************************************************/
// GUI CONTROLS
/************************************************************************/
#if defined(_WINDOWS) || defined(__linux__)
#define MSAA_LEVELS_COUNT 3U
#else
#define MSAA_LEVELS_COUNT 2U
#endif

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

	bool mDrawDebugTargets = false;

	float nearPlane = 10.0f;
	float farPlane = 30000.0f;

	// adjust directional sunlight angle
	float2 mSunControl = { -2.1f, 0.164f };

	float mSunSize = 300.0f;

	float4 mLightColor = { 1.0f, 0.8627f, 0.78f, 2.5f };

	DynamicUIWidgets  mDynamicUIWidgetsGR;
	GodrayInfo        gGodrayInfo = {};
    bool              mEnableGodray = true;
	uint              gGodrayInteration = 3;

	float mEsmControl = 80.0f;

	float mRetinaScaling = 1.5f;

	DynamicUIWidgets mSCurve;
	float             SCurveScaleFactor = 1.0f;
	float             SCurveSMin = 0.00f;
	float             SCurveSMid = 0.84f;
	float             SCurveSMax = 65.65f;
	float             SCurveTMin = 0.0f;
	float             SCurveTMid = 139.76f;
	float             SCurveTMax = 1100.0f;
	float             SCurveSlopeFactor = 2.2f;

	DynamicUIWidgets mLinearScale;
	float             LinearScale = 140.0f;

	// HDR10
	DynamicUIWidgets mDisplaySetting;

	DisplayColorSpace  mCurrentSwapChainColorSpace = ColorSpace_Rec2020;
	DisplayColorRange  mDisplayColorRange = ColorRange_RGB;
	DisplaySignalRange mDisplaySignalRange = Display_SIGNAL_RANGE_FULL;

	SampleCount mMsaaLevel = SAMPLE_COUNT_2;
	uint32_t mMsaaIndex = (uint32_t)log2((uint32_t)mMsaaLevel);
	uint32_t mMsaaIndexRequested = mMsaaIndex;

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
const char* gSceneName = "SanMiguel.gltf";
const char* gSunName = "sun.gltf";

// Number of in-flight buffers
const uint32_t gImageCount = 3;

// Constants
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

int                    gGodrayScale = 8;

/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
	// Stores the camera/eye position in object space for cluster culling
	vec3              gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
	PerFrameConstants gPerFrameUniformData = {};
	UniformDataSkybox gUniformDataSky;
	UniformDataSunMatrices gUniformDataSunMatrices;
	GodrayInfo        gGodrayInfo = {};

	// These are just used for statistical information
	uint32_t gTotalClusters = 0;
	uint32_t gCulledClusters = 0;
	uint32_t gDrawCount[GEOMSET_COUNT] = {};
	uint32_t gTotalDrawCount = {};
};

/************************************************************************/
// Settings
/************************************************************************/
AppSettings gAppSettings;
/************************************************************************/
// Profiling
/************************************************************************/
ProfileToken gGraphicsProfileToken;
ProfileToken gComputeProfileToken;
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
CmdPool* pCmdPool[gImageCount] = { NULL };
Cmd*     ppCmds[gImageCount] = { NULL };

Queue*   pComputeQueue = NULL;
CmdPool* pComputeCmdPool[gImageCount] = { NULL };
Cmd*     ppComputeCmds[gImageCount] = { NULL };
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
// Clear OIT Head Index pipeline
/************************************************************************/
Shader*		   pShaderClearHeadIndexOIT = NULL;
Pipeline*	   pPipelineClearHeadIndexOIT = NULL;
RootSignature* pRootSignatureClearHeadIndexOIT = NULL;
DescriptorSet* pDescriptorSetClearHeadIndexOIT[2] = { NULL };
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
Shader*           pShaderShadowPass[GEOMSET_COUNT] = { NULL };
Pipeline*         pPipelineShadowPass[GEOMSET_COUNT] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferPass[GEOMSET_COUNT] = {};
Pipeline*         pPipelineVisibilityBufferPass[GEOMSET_COUNT] = {};
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
// Deferred pass pipeline
/************************************************************************/
Shader*           pShaderDeferredPass[GEOMSET_COUNT] = {};
Pipeline*         pPipelineDeferredPass[GEOMSET_COUNT] = {};
RootSignature*    pRootSignatureDeferredPass = nullptr;
CommandSignature* pCmdSignatureDeferredPass = nullptr;
DescriptorSet*    pDescriptorSetDeferredPass[2] = { NULL };
/************************************************************************/
// Deferred shade pipeline
/************************************************************************/
Shader*           pShaderDeferredShade[2 * MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineDeferredShadeSrgb[2] = { nullptr };
RootSignature*    pRootSignatureDeferredShade = nullptr;
DescriptorSet*    pDescriptorSetDeferredShade[2] = { NULL };
/************************************************************************/
// Deferred point light shade pipeline
/************************************************************************/
Shader*           pShaderDeferredShadePointLight[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineDeferredShadePointLightSrgb = nullptr;
RootSignature*    pRootSignatureDeferredShadePointLight = nullptr;
DescriptorSet*    pDescriptorSetDeferredShadePointLight[2] = { NULL };
/************************************************************************/
// Resolve pipeline
/************************************************************************/
Shader*           pShaderResolve[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineResolve = nullptr;
Pipeline*         pPipelineResolvePost = nullptr;
RootSignature*    pRootSignatureResolve = nullptr;
DescriptorSet*    pDescriptorSetResolve = { NULL };

Shader*           pShaderGodrayResolve[MSAA_LEVELS_COUNT] = { nullptr };
Pipeline*         pPipelineGodrayResolve = nullptr;
Pipeline*         pPipelineGodrayResolvePost = nullptr;
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
// Godray pipeline
/************************************************************************/
Shader*           pSunPass = nullptr;
Pipeline*         pPipelineSunPass = nullptr;
RootSignature*    pRootSigSunPass = nullptr;
DescriptorSet*    pDescriptorSetSunPass = { NULL };
VertexLayout      gVertexLayoutSun = {};

Shader*           pGodRayPass = nullptr;
Pipeline*         pPipelineGodRayPass = nullptr;
RootSignature*    pRootSigGodRayPass = nullptr;
DescriptorSet*    pDescriptorSetGodRayPass = NULL;
uint32_t          gGodRayRootConstantIndex = 0;
Geometry*         pSun = NULL;
/************************************************************************/
// Curve Conversion pipeline
/************************************************************************/
Shader*           pShaderCurveConversion = nullptr;
Pipeline*         pPipelineCurveConversionPass = nullptr;
RootSignature*    pRootSigCurveConversionPass = nullptr;
DescriptorSet*    pDescriptorSetCurveConversionPass = NULL;

OutputMode         gWasOutputMode = gAppSettings.mOutputMode;
DisplayColorSpace  gWasColorSpace = gAppSettings.mCurrentSwapChainColorSpace;
DisplayColorRange  gWasDisplayColorRange = gAppSettings.mDisplayColorRange;
DisplaySignalRange gWasDisplaySignalRange = gAppSettings.mDisplaySignalRange;

/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*           pShaderPresentPass = nullptr;
Pipeline*         pPipelinePresentPass = nullptr;
RootSignature*    pRootSigPresentPass = nullptr;
DescriptorSet*    pDescriptorSetPresentPass = { NULL };
uint32_t          gSCurveRootConstantIndex = 0;
/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pDepthBuffer = NULL;
RenderTarget* pDepthBufferOIT = NULL;
RenderTarget* pRenderTargetVBPass = NULL;
RenderTarget* pRenderTargetMSAA = NULL;
RenderTarget* pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { NULL };
RenderTarget* pRenderTargetShadow = NULL;
RenderTarget* pIntermediateRenderTarget = NULL;
RenderTarget* pRenderTargetSun = NULL;
RenderTarget* pRenderTargetSunResolved = NULL;
RenderTarget* pRenderTargetGodRay[2] = { NULL };
RenderTarget* pCurveConversionRenderTarget = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerBilinearClamp = NULL;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture** gDiffuseMapsStorage = NULL;
Texture** gNormalMapsStorage = NULL;
Texture** gSpecularMapsStorage = NULL;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
Geometry* pMainSceneGeom = NULL;
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer* pMaterialPropertyBuffer = NULL;
Buffer* pPerFrameUniformBuffers[gImageCount] = { NULL };
// Buffers containing all indirect draw commands per geometry set (no culling)
Buffer* pIndirectDrawArgumentsBufferAll[GEOMSET_COUNT] = { NULL };
Buffer* pIndirectMaterialBufferAll = NULL;
Buffer* pMeshConstantsBuffer = NULL;
// Buffers containing filtered indirect draw commands per geometry set (culled)
Buffer* pFilteredIndirectDrawArgumentsBuffer[gImageCount][GEOMSET_COUNT][gNumViews] = { { { NULL } } };
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
Buffer*       pUniformBufferSun[gImageCount] = { NULL };
Buffer*       pUniformBufferSky[gImageCount] = { NULL };
uint64_t      gFrameCount = 0;
ClusterContainer* pMeshes = NULL;
uint32_t      gMainSceneMeshCount = 0;
uint32_t      gMainSceneMaterialCount = 0;
UIComponent* pGuiWindow = NULL;
UIComponent* pDebugTexturesWindow = NULL;
FontDrawDesc  gFrameTimeDraw;
uint32_t      gFontID = 0;
/************************************************************************/
// Triangle filtering data
/************************************************************************/
const uint32_t gSmallBatchChunkCount = max(1U, 512U / CLUSTER_SIZE) * 16U;
FilterBatchChunk* pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = { { NULL } };
GPURingBuffer* pFilterBatchDataBuffer = { NULL };
/************************************************************************/
ICameraController* pCameraController = NULL;
/************************************************************************/
// CPU staging data
/************************************************************************/
// CPU buffers for light data
LightData gLightData[LIGHT_COUNT] = {};

PerFrameData  gPerFrame[gImageCount] = {};
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

uint32_t gMaxNodeCountOIT;
float    gFlagAlpha = 0.6f;

Buffer*  pGeometryCountBufferOIT = NULL;

Buffer*  pHeadIndexBufferOIT = NULL;
Buffer*  pVisBufLinkedListBufferOIT = NULL;
Buffer*  pDeferredLinkedListBufferOIT = NULL;

/************************************************************************/
// Screen resolution UI data
/************************************************************************/

const char* pPipelineCacheName = "PipelineCache.cache";
PipelineCache* pPipelineCache = NULL;

/************************************************************************/
// Culling intrinsic data
/************************************************************************/
const uint32_t pdep_lut[8] = { 0x0, 0x1, 0x4, 0x5, 0x10, 0x11, 0x14, 0x15 };

/************************************************************************/
// App implementation
/************************************************************************/
void SetupDebugTexturesWindow()
{
	float scale = 0.15f;
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

	static const Texture* VBRTs[6];
	int textureCount = 0;

	if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
	{
		VBRTs[textureCount++] = pRenderTargetVBPass->pTexture;
	}
	else
	{
		VBRTs[textureCount++] = pRenderTargetDeferredPass[0]->pTexture;
		VBRTs[textureCount++] = pRenderTargetDeferredPass[1]->pTexture;
		VBRTs[textureCount++] = pRenderTargetDeferredPass[2]->pTexture;
		VBRTs[textureCount++] = pRenderTargetDeferredPass[3]->pTexture;
	}
	VBRTs[textureCount++] = pRenderTargetShadow->pTexture;
	ASSERT(textureCount <= (int)(sizeof(VBRTs)/sizeof(VBRTs[0])));

	if (pDebugTexturesWindow)
	{
		ASSERT(pDebugTexturesWindow->mWidgets[0]->mType == WIDGET_TYPE_DEBUG_TEXTURES);
		DebugTexturesWidget* pTexturesWidget = (DebugTexturesWidget*)pDebugTexturesWindow->mWidgets[0]->pWidget;
		pTexturesWidget->pTextures = VBRTs;
		pTexturesWidget->mTexturesCount = textureCount;
		pTexturesWidget->mTextureDisplaySize = texSize;
	}
}

#if defined(_WINDOWS)
const char* gTestScripts[] = { "Test_Cluster_Culling.lua", "Test_MSAA_0.lua", "Test_MSAA_2.lua", "Test_MSAA_4.lua" };
#else
const char* gTestScripts[] = { "Test_Cluster_Culling.lua", "Test_MSAA_0.lua", "Test_MSAA_2.lua" };
#endif

uint32_t gCurrentScriptIndex = 0;
void RunScript(void* pUserData)
{
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

// LIMIT RESOLUTION FOR THIS EXAMPLE
// 
#define RES_LIMIT_WIDTH  1920
#define RES_LIMIT_HEIGHT 1080

class VisibilityBufferOIT : public IApp
{
public:

	VisibilityBufferOIT()
	{
		mSettings.mWidth = RES_LIMIT_WIDTH;
		mSettings.mHeight = RES_LIMIT_HEIGHT;
	}

	void SetHDRMetaData(float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
	{
#if defined(DIRECT3D12)
#if !defined(XBOX)
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
		if (SUCCEEDED(pSwapChain->mD3D12.pDxSwapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) &&
			((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
		{
			pSwapChain->mD3D12.pDxSwapChain->SetColorSpace1(colorSpace);
		}
#endif
#endif
	}

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_PIPELINE_CACHE, "PipelineCaches");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_OTHER_FILES, "");

		initThreadSystem(&pThreadSystem);

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
		// Initialize the Forge renderer with the appropriate parameters.
		/************************************************************************/

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		QueueDesc computeQueueDesc = {};
		computeQueueDesc.mType = QUEUE_TYPE_COMPUTE;
		computeQueueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &computeQueueDesc, &pComputeQueue);

		addFence(pRenderer, &pTransitionFences);

		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addFence(pRenderer, &pComputeCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
			addSemaphore(pRenderer, &pComputeCompleteSemaphores[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			// Create the command pool and the command lists used to store GPU commands.
			// One Cmd list per back buffer image is stored for triple buffering.
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPool[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPool[i];
			addCmd(pRenderer, &cmdDesc, &ppCmds[i]);

			// Create the command pool and the command lists used to store GPU commands.
			// One Cmd list per back buffer image is stored for triple buffering.
			cmdPoolDesc.pQueue = pComputeQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pComputeCmdPool[i]);
			cmdDesc.pPool = pComputeCmdPool[i];
			addCmd(pRenderer, &cmdDesc, &ppComputeCmds[i]);
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

		addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
		addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
		addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
		addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);

		/************************************************************************/
		// Load resources for skybox
		/************************************************************************/
		addThreadSystemTask(pThreadSystem, memberTaskFunc0<VisibilityBufferOIT, &VisibilityBufferOIT::LoadSkybox>, this);
		/************************************************************************/
		// Load the scene using the SceneLoader class
		/************************************************************************/
		HiresTimer      sceneLoadTimer;
		initHiresTimer(&sceneLoadTimer); 

		Scene* pScene = loadScene(gSceneName, 50.0f, -20.0f, 0.0f, 0.0f);
		if (!pScene)
			return false;

		LOGF(LogLevel::eINFO, "Load scene : %f ms", getHiresTimerUSec(&sceneLoadTimer, true) / 1000.0f);

		gMainSceneMeshCount = pScene->geom->mDrawArgCount;
		gMainSceneMaterialCount = pScene->geom->mDrawArgCount;
		pMainSceneGeom = pScene->geom;
		/************************************************************************/
		// Texture loading
		/************************************************************************/
		gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMainSceneMaterialCount);
		gNormalMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMainSceneMaterialCount);
		gSpecularMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMainSceneMaterialCount);

		for (uint32_t i = 0; i < gMainSceneMaterialCount; ++i)
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
		/************************************************************************/
		// Cluster creation
		/************************************************************************/
		pMeshes = (ClusterContainer*)tf_malloc(gMainSceneMeshCount * sizeof(ClusterContainer));

		HiresTimer clusterTimer;
		initHiresTimer(&clusterTimer);

		// Calculate clusters
		for (uint32_t i = 0; i < gMainSceneMeshCount; ++i)
		{
			ClusterContainer*   mesh = pMeshes + i;
			Material* material = pScene->materials + i;
			createClusters(material->twoSided, pScene, pScene->geom->pDrawArgs + i, mesh);
		}

		removeGeometryShadowData(pScene->geom);
		LOGF(LogLevel::eINFO, "Load clusters : %f ms", getHiresTimerUSec(&clusterTimer, true) / 1000.0f);

		// Create geometry for light rendering
		createCubeBuffers(pRenderer, &pVertexBufferCube, &pIndexBufferCube);

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

		gVertexLayoutSun.mAttribCount = 3;
		gVertexLayoutSun.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutSun.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutSun.mAttribs[0].mBinding = 0;
		gVertexLayoutSun.mAttribs[0].mLocation = 0;
		gVertexLayoutSun.mAttribs[0].mOffset = 0;
		gVertexLayoutSun.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutSun.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutSun.mAttribs[1].mBinding = 0;
		gVertexLayoutSun.mAttribs[1].mLocation = 1;
		gVertexLayoutSun.mAttribs[1].mOffset = sizeof(float3);
		gVertexLayoutSun.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutSun.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutSun.mAttribs[2].mBinding = 0;
		gVertexLayoutSun.mAttribs[2].mLocation = 2;
		gVertexLayoutSun.mAttribs[2].mOffset = sizeof(float3) * 2;

		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = gSunName;
		loadDesc.ppGeometry = &pSun;
		loadDesc.pVertexLayout = &gVertexLayoutSun;
		addResource(&loadDesc, NULL);


		UIComponentDesc UIComponentDesc = {};
		UIComponentDesc.mStartPosition = vec2(225.0f, 100.0f);
		uiCreateComponent(GetName(), &UIComponentDesc, &pGuiWindow);
		uiSetComponentFlags(pGuiWindow, GUI_COMPONENT_FLAGS_NO_RESIZE);

		const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		LuaScriptDesc scriptDescs[numScripts] = {};
		for (uint32_t i = 0; i < numScripts; ++i)
			scriptDescs[i].pScriptFileName = gTestScripts[i];
		luaDefineScripts(scriptDescs, numScripts);
		
		DropdownWidget ddTestScripts;
		ddTestScripts.pData = &gCurrentScriptIndex;
		ddTestScripts.pNames = gTestScripts;
		ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

		ButtonWidget bRunScript;
		UIWidget* pRunScript = uiCreateComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript); 
		luaRegisterWidget(pRunScript);

		/************************************************************************/
		// Most important options
		/************************************************************************/

		static const char*      renderModeNames[] = { "Visibility Buffer", "Deferred Shading"};

		DropdownWidget renderMode = {};
		renderMode.pData = (uint32_t*)&gAppSettings.mRenderMode;
		renderMode.pNames = renderModeNames;
		renderMode.mCount = sizeof(renderModeNames) / sizeof(renderModeNames[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Render Mode", &renderMode, WIDGET_TYPE_DROPDOWN));

		SliderFloatWidget transparencyAlpha;
		transparencyAlpha.pData = &gFlagAlpha;
		transparencyAlpha.mMin = 0.0f;
		transparencyAlpha.mMax = 1.0f;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Transparent Flag Alpha", &transparencyAlpha, WIDGET_TYPE_SLIDER_FLOAT));

		// Default NX settings for better performance.
#if NX64
	// Async compute is not optimal on the NX platform. Turning this off to make use of default graphics queue for triangle visibility.
		gAppSettings.mAsyncCompute = false;
		// High fill rate features are also disabled by default for performance.
		gAppSettings.mEnableGodray = false;
		gAppSettings.mEnableHDAO = false;
#endif

		/************************************************************************/
		// MSAA Settings
		/************************************************************************/
#if defined(_WINDOWS) || defined(__linux__)
		static const char* msaaSampleNames[] = { "Off", "2 Samples", "4 Samples" };
#else
		static const char* msaaSampleNames[] = { "Off", "2 Samples" };
#endif

		DropdownWidget ddMSAA = {};
		ddMSAA.pData = &gAppSettings.mMsaaIndexRequested;
		ddMSAA.pNames = msaaSampleNames;
		ddMSAA.mCount = MSAA_LEVELS_COUNT;
		UIWidget* msaaWidget = uiCreateComponentWidget(pGuiWindow, "MSAA", &ddMSAA, WIDGET_TYPE_DROPDOWN);
		uiSetWidgetOnEditedCallback(msaaWidget, nullptr, [](void* pUserData) {ReloadDesc reloadDescriptor; reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET; requestReload(&reloadDescriptor); });
		luaRegisterWidget(msaaWidget);

		CheckboxWidget checkbox;
		checkbox.pData = &gAppSettings.mHoldFilteredResults;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Hold filtered results", &checkbox, WIDGET_TYPE_CHECKBOX));

		checkbox.pData = &gAppSettings.mFilterTriangles;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Triangle Filtering", &checkbox, WIDGET_TYPE_CHECKBOX));

		checkbox.pData = &gAppSettings.mClusterCulling;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cluster Culling", &checkbox, WIDGET_TYPE_CHECKBOX));

		checkbox.pData = &gAppSettings.mAsyncCompute;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Async Compute", &checkbox, WIDGET_TYPE_CHECKBOX));

		checkbox.pData = &gAppSettings.mDrawDebugTargets;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw Debug Targets", &checkbox, WIDGET_TYPE_CHECKBOX));

		/************************************************************************/
		/************************************************************************/
		// DirectX 12 Only
#if defined(DIRECT3D12)
		if ( pRenderer->pActiveGpuSettings->mHDRSupported )
		{
			static const char*      outputModeNames[] = { "SDR", "HDR10" };

			DropdownWidget outputMode;
			outputMode.pData = ( uint32_t* )&gAppSettings.mOutputMode;
			outputMode.pNames = outputModeNames;
			outputMode.mCount = sizeof(outputModeNames) / sizeof(outputModeNames[0]);
			luaRegisterWidget( uiCreateComponentWidget( pGuiWindow, "Output Mode", &outputMode, WIDGET_TYPE_DROPDOWN ) );
		}
#endif

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
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraSpeedProp, WIDGET_TYPE_SLIDER_FLOAT));

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

		gAppSettings.gGodrayInfo.exposure = 0.06f;
		gAppSettings.gGodrayInfo.decay = 0.9f;
		gAppSettings.gGodrayInfo.density = 2.0f;
		gAppSettings.gGodrayInfo.weight = 1.4f;
		gAppSettings.gGodrayInfo.NUM_SAMPLES = 80;

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

		sliderFloat.pData = &gAppSettings.gGodrayInfo.exposure;
		sliderFloat.mMin = 0.0f;
		sliderFloat.mMax = 0.1f;
		sliderFloat.mStep = 0.001f;
		uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray: Exposure", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT);

		SliderUintWidget sliderUint;
		sliderUint.pData = &gAppSettings.gGodrayInteration;
		sliderUint.mMin = 1;
		sliderUint.mMax = 4;
		uiCreateDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, "God Ray: Quality", &sliderUint, WIDGET_TYPE_SLIDER_UINT);

		if (gAppSettings.mEnableGodray)
			uiShowDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR, pGuiWindow);

		//SliderFloatWidget esm("Shadow Control", &gAppSettings.mEsmControl, 0, 200.0f);
		//pGuiWindow->AddWidget(esm);

		checkbox.pData = &gAppSettings.mRenderLocalLights;
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Enable Random Point Lights", &checkbox, WIDGET_TYPE_CHECKBOX));

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

		static const char* curveConversionModeNames[] = { "Linear Scale", "Scurve" };

		DropdownWidget curveConversionMode;
		curveConversionMode.pData = (uint32_t*)&gAppSettings.mCurveConversionMode;
		curveConversionMode.pNames = curveConversionModeNames;
		curveConversionMode.mCount = sizeof(curveConversionModeNames) / sizeof(curveConversionModeNames[0]);
		luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Curve Conversion", &curveConversionMode, WIDGET_TYPE_DROPDOWN));

		sliderFloat.pData = &gAppSettings.LinearScale;
		sliderFloat.mMin = 0.0f;
		sliderFloat.mMax = 300.0f;
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
	/************************************************************************/
	// Finish the resource loading process since the next code depends on the loaded resources
		waitThreadSystemIdle(pThreadSystem);
		waitForAllResourceLoads();

		HiresTimer setupBuffersTimer;
		initHiresTimer(&setupBuffersTimer);
		addTriangleFilteringBuffers(pScene);

		LOGF(LogLevel::eINFO, "Setup buffers : %f ms", getHiresTimerUSec(&setupBuffersTimer, true) / 1000.0f);

		LOGF(LogLevel::eINFO, "Total Load Time : %f ms", getHiresTimerUSec(&totalTimer, true) / 1000.0f);

		removeScene(pScene);
		
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
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true; }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this};
		setGlobalInputAction(&globalInputActionDesc);

		return true;
	}
	
    void Exit()
    {
		exitInputSystem();
		exitThreadSystem(pThreadSystem);
		exitCameraController(pCameraController);

		removeResource(pSkybox);
		removeTriangleFilteringBuffers();
		uiDestroyDynamicWidgets(&gAppSettings.mDynamicUIWidgetsGR);
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
		// Destroy scene buffers
		removeResource(pMainSceneGeom);

		removeResource(pSun);
		removeResource(pSkyboxVertexBuffer);

		// Destroy clusters
		for (uint32_t i = 0; i < gMainSceneMeshCount; ++i)
		{
			destroyClusters(&pMeshes[i]);
		}
		// Remove Textures
		for (uint32_t i = 0; i < gMainSceneMaterialCount; ++i)
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
		/************************************************************************/
		/************************************************************************/
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeFence(pRenderer, pTransitionFences);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeFence(pRenderer, pComputeCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
			removeSemaphore(pRenderer, pComputeCompleteSemaphores[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, ppCmds[i]);
			removeCmdPool(pRenderer, pCmdPool[i]);
			removeCmd(pRenderer, ppComputeCmds[i]);
			removeCmdPool(pRenderer, pComputeCmdPool[i]);
		}

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

		exitResourceLoaderInterface(pRenderer);

		/*
		 #ifdef _DEBUG
		 ID3D12DebugDevice *pDebugDevice = NULL;
		 pRenderer->pDxDevice->QueryInterface(&pDebugDevice);

		 pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		 pDebugDevice->Release();
		 #endif
		 */

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
			if (gAppSettings.mMsaaIndex != gAppSettings.mMsaaIndexRequested)
			{
				gAppSettings.mMsaaIndex = gAppSettings.mMsaaIndexRequested;
				gAppSettings.mMsaaLevel = (SampleCount)(1 << gAppSettings.mMsaaIndex);
			}

			if (!addSwapChain())
				return false;

			addRenderTargets();

			if (pReloadDesc->mType & RELOAD_TYPE_RENDERTARGET)
			{
				if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
				{
					SetHDRMetaData(gAppSettings.MaxOutputNits, gAppSettings.MinOutputNits, gAppSettings.MaxCLL, gAppSettings.MaxFALL);
				}
			}

			if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
			{
				SetupDebugTexturesWindow();
				addOrderIndependentTransparencyResources();
			}
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
				removeOrderIndependentTransparencyResources();

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

		//Change format
		if (gWasOutputMode != gAppSettings.mOutputMode)
		{
			ReloadDesc reloadDescriptor;
			reloadDescriptor.mType = RELOAD_TYPE_RENDERTARGET;
			requestReload(&reloadDescriptor);

			gWasOutputMode = gAppSettings.mOutputMode;
		}

#if !defined(TARGET_IOS)
		pCameraController->update(deltaTime);
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
			pCameraController->moveTo(f3Tov3(newPos));

			float3 newLookat = v3ToF3(
				lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]), f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
			pCameraController->lookAt(f3Tov3(newLookat));
		}

		updateDynamicUIElements();

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
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pRenderFence);
		}
		else
		{
			// check to see if we can use the cmd buffer
			Fence*      pComputeFence = pComputeCompleteFences[frameIdx];
			FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pComputeFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pComputeFence);

			// check to see if we can use the cmd buffer
			Fence*      pRenderFence = pRenderCompleteFences[frameIdx];
			//FenceStatus fenceStatus;
			getFenceStatus(pRenderer, pRenderFence, &fenceStatus);
			if (fenceStatus == FENCE_STATUS_INCOMPLETE)
				waitForFences(pRenderer, 1, &pRenderFence);
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

		// Update uniform buffers
		update = { pUniformBufferSun[frameIdx] };
		update.mSize = sizeof(gPerFrame[frameIdx].gUniformDataSunMatrices);
		beginUpdateResource(&update);
		*(UniformDataSunMatrices*)update.pMappedData = gPerFrame[frameIdx].gUniformDataSunMatrices;
		endUpdateResource(&update, NULL);

		/************************************************************************/
		// Async compute pass
		/************************************************************************/

		if (gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
		{
			/************************************************************************/
			// Triangle filtering async compute pass
			/************************************************************************/
			Cmd* computeCmd = ppComputeCmds[frameIdx];

			resetCmdPool(pRenderer, pComputeCmdPool[frameIdx]);
			beginCmd(computeCmd);
			cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);

			triangleFilteringPass(computeCmd, gComputeProfileToken, frameIdx);

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
				BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS } };
				cmdResourceBarrier(computeCmd, 1, barriers, 0, NULL, 0, NULL);
				computeLightClusters(computeCmd, frameIdx);
				cmdEndGpuTimestampQuery(computeCmd, gComputeProfileToken);
			}

			cmdEndGpuFrameProfile(computeCmd, gComputeProfileToken);
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
		// Draw Pass - Skip first frame since draw will always be one frame behind compute
		/************************************************************************/

		if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
		{
			Cmd* graphicsCmd = NULL;

			if (gAppSettings.mAsyncCompute)
				frameIdx = ((gFrameCount - 1) % gImageCount);

			pScreenRenderTarget = pIntermediateRenderTarget;
			//pScreenRenderTarget = pSwapChain->ppRenderTargets[gPresentFrameIdx];

			// Get command list to store rendering commands for this frame
			graphicsCmd = ppCmds[frameIdx];
			// Submit all render commands for this frame
			resetCmdPool(pRenderer, pCmdPool[frameIdx]);
			beginCmd(graphicsCmd);

			cmdBeginGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);

			if (!gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
			{
				triangleFilteringPass(graphicsCmd, gGraphicsProfileToken, frameIdx);
			}

			if (!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles)
			{
				cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Clear Light Clusters");
				clearLightClusters(graphicsCmd, frameIdx);
				cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
			}

			if ((!gAppSettings.mAsyncCompute || !gAppSettings.mFilterTriangles) && gAppSettings.mRenderLocalLights)
			{
				// Update Light clusters on the GPU
				cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Compute Light Clusters");
				BufferBarrier barriers[] = { { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS } };
				cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
				computeLightClusters(graphicsCmd, frameIdx);
				barriers[0] = { pLightClusters[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
				cmdResourceBarrier(graphicsCmd, 1, barriers, 0, NULL, 0, NULL);
				cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
			}

			// Transition swapchain buffer to be used as a render target
			uint32_t rtBarriersCount = gAppSettings.mMsaaLevel > 1 ? 4 : 3;
			RenderTargetBarrier rtBarriers[] = {
				{ pScreenRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
				{ pDepthBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
				{ pRenderTargetMSAA, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			};

			const uint32_t maxNumBarriers = (GEOMSET_COUNT * gNumViews) + gNumViews + 3;
			uint32_t barrierCount = 0;
			BufferBarrier barriers2[maxNumBarriers] = {};
			barriers2[barrierCount++] = { pLightClusters[frameIdx],      RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };

			if (gAppSettings.mFilterTriangles)
			{
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_TRANSPARENT][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
					barriers2[barrierCount++] = { pFilteredIndexBuffer[frameIdx][i],
						RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
				}
				barriers2[barrierCount++] = { pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
			}

			cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, rtBarriersCount, rtBarriers);

			drawScene(graphicsCmd, frameIdx);
			drawSkybox(graphicsCmd, frameIdx);

			barrierCount = 0;
			barriers2[barrierCount++] = { pLightClusters[frameIdx],      RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			barriers2[barrierCount++] = { pLightClustersCount[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };

			if (gAppSettings.mFilterTriangles)
			{
				for (uint32_t i = 0; i < gNumViews; ++i)
				{
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[barrierCount++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_TRANSPARENT][i],
						RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
					barriers2[barrierCount++] = { pFilteredIndexBuffer[frameIdx][i],
						RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
				}
				barriers2[barrierCount++] = { pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
			}

			cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 0, NULL);

			if (gAppSettings.mEnableGodray)
			{
				drawGodray(graphicsCmd, frameIdx);
				drawColorconversion(graphicsCmd);
			}

			cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "UI Pass");
			drawGUI(graphicsCmd, frameIdx);
			cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

			// Get the current render target for this frame
			acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
			presentImage(graphicsCmd, pScreenRenderTarget, pSwapChain->ppRenderTargets[presentIndex]);

			cmdEndGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);
			endCmd(graphicsCmd);

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
			flipProfiler();
		}

		++gFrameCount;
	}

	const char* GetName() { return "15a_VisibilityBufferOIT"; }

	bool addDescriptorSets()
	{
		// Triangle Filtering
		DescriptorSetDesc setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);

        setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * gNumStages };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);

		setDesc = { pRootSignatureClearHeadIndexOIT, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClearHeadIndexOIT[0]);
		setDesc = { pRootSignatureClearHeadIndexOIT, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClearHeadIndexOIT[1]);

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
		// Deferred Pass
		setDesc = { pRootSignatureDeferredPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredPass[0]);
		setDesc = { pRootSignatureDeferredPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredPass[1]);
		// Deferred Shade
		setDesc = { pRootSignatureDeferredShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredShade[0]);
		setDesc = { pRootSignatureDeferredShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredShade[1]);
		// Deferred Shade Lighting
		setDesc = { pRootSignatureDeferredShadePointLight, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredShadePointLight[0]);
		setDesc = { pRootSignatureDeferredShadePointLight, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDeferredShadePointLight[1]);
		// Resolve
		setDesc = { pRootSignatureResolve, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetResolve);
		// Sun
		setDesc = { pRootSigSunPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSunPass);
		// God Ray
		setDesc = { pRootSigGodRayPass, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, 3 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGodRayPass);
		// Curve Conversion
		setDesc = { pRootSigCurveConversionPass, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCurveConversionPass);
		// Sky
		setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pRootSingatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		// Present
		setDesc = { pRootSigPresentPass, DESCRIPTOR_UPDATE_FREQ_NONE, 2 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPresentPass);

		return true;
	}

	void removeDescriptorSets()
	{
		removeDescriptorSet(pRenderer, pDescriptorSetPresentPass);
		removeDescriptorSet(pRenderer, pDescriptorSetClearHeadIndexOIT[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetClearHeadIndexOIT[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetResolve);
		removeDescriptorSet(pRenderer, pDescriptorSetCurveConversionPass);
		removeDescriptorSet(pRenderer, pDescriptorSetGodRayPass);
		removeDescriptorSet(pRenderer, pDescriptorSetSunPass);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredShadePointLight[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredShadePointLight[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredShade[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredShade[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredPass[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetDeferredPass[1]);
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
		// Triangle Filtering
		{
            const uint32_t paramsCount = 4;
			DescriptorData filterParams[paramsCount] = {};
			filterParams[0].pName = "vertexDataBuffer";
			filterParams[0].ppBuffers = &pMainSceneGeom->pVertexBuffers[0];
			filterParams[1].pName = "indexDataBuffer";
			filterParams[1].ppBuffers = &pMainSceneGeom->pIndexBuffer;
			filterParams[2].pName = "meshConstantsBuffer";
			filterParams[2].ppBuffers = &pMeshConstantsBuffer;
			filterParams[3].pName = "materialProps";
			filterParams[3].ppBuffers = &pMaterialPropertyBuffer;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], paramsCount, filterParams);
            
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData clearParams[4] = {};
				clearParams[0].pName = "indirectDrawArgsBufferAlpha";
				clearParams[0].mCount = gNumViews;
				clearParams[0].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_ALPHATESTED];
				clearParams[1].pName = "indirectDrawArgsBufferNoAlpha";
				clearParams[1].mCount = gNumViews;
				clearParams[1].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_OPAQUE];
				clearParams[2].pName = "indirectDrawArgsBufferTransparent";
				clearParams[2].mCount = gNumViews;
				clearParams[2].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_TRANSPARENT];
				clearParams[3].pName = "uncompactedDrawArgsRW";
				clearParams[3].mCount = gNumViews;
				clearParams[3].ppBuffers = pUncompactedDrawArgumentsBuffer[i];
				updateDescriptorSet(pRenderer, i * gNumStages + 0, pDescriptorSetTriangleFiltering[1], 4, clearParams);

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

				DescriptorData compactParams[6] = {};
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
				compactParams[3].pName = "indirectDrawArgsBufferTransparent";
				compactParams[3].mCount = gNumViews;
				compactParams[3].mBindICB = true;
				compactParams[3].pICBName = "icbTransparent";
				compactParams[3].ppBuffers = pFilteredIndirectDrawArgumentsBuffer[i][GEOMSET_TRANSPARENT];
				compactParams[4].pName = "uncompactedDrawArgs";
				compactParams[4].mCount = gNumViews;
				compactParams[4].ppBuffers = pUncompactedDrawArgumentsBuffer[i];
				// Required to generate ICB (to bind index buffer)
				compactParams[5].pName = "filteredIndicesBuffer";
				compactParams[5].mCount = gNumViews;
				compactParams[5].ppBuffers = pFilteredIndexBuffer[i];
				updateDescriptorSet(pRenderer, i * gNumStages + 2, pDescriptorSetTriangleFiltering[1], 6, compactParams);
			}
		}
		// OIT Head Index Clear
		{
			DescriptorData params[2] = {};
			params[0].pName = "headIndexBufferUAV";
			params[0].ppBuffers = &pHeadIndexBufferOIT;
			params[1].pName = "geometryCountBuffer";
			params[1].ppBuffers = &pGeometryCountBufferOIT;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetClearHeadIndexOIT[0], 2, params); 
			params[0] = {};
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "PerFrameConstants";
				params[0].ppBuffers = &pPerFrameUniformBuffers[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetClearHeadIndexOIT[1], 1, params);
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
            DescriptorData params[4] = {};
            params[0].pName = "diffuseMaps";
            params[0].mCount = gMainSceneMaterialCount;
            params[0].ppTextures = gDiffuseMapsStorage;
			params[1].pName = "headIndexBufferUAV";
			params[1].ppBuffers = &pHeadIndexBufferOIT;
			params[2].pName = "vbDepthLinkedListUAV";
			params[2].ppBuffers = &pVisBufLinkedListBufferOIT;
			params[3].pName = "geometryCountBuffer";
			params[3].ppBuffers = &pGeometryCountBufferOIT;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 4, params);
			params[0] = {};
			params[1] = {};
			params[2] = {};
			params[3] = {};
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
			DescriptorData vbShadeParams[13] = {};
			DescriptorData vbShadeParamsPerFrame[7] = {};
			vbShadeParams[0].pName = "vbTex";
			vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
			vbShadeParams[1].pName = "diffuseMaps";
			vbShadeParams[1].mCount = gMainSceneMaterialCount;
			vbShadeParams[1].ppTextures = gDiffuseMapsStorage;
			vbShadeParams[2].pName = "normalMaps";
			vbShadeParams[2].mCount = gMainSceneMaterialCount;
			vbShadeParams[2].ppTextures = gNormalMapsStorage;
			vbShadeParams[3].pName = "specularMaps";
			vbShadeParams[3].mCount = gMainSceneMaterialCount;
			vbShadeParams[3].ppTextures = gSpecularMapsStorage;
			vbShadeParams[4].pName = "vertexPos";
			vbShadeParams[4].ppBuffers = &pMainSceneGeom->pVertexBuffers[0];
			vbShadeParams[5].pName = "vertexTexCoord";
			vbShadeParams[5].ppBuffers = &pMainSceneGeom->pVertexBuffers[1];
			vbShadeParams[6].pName = "vertexNormal";
			vbShadeParams[6].ppBuffers = &pMainSceneGeom->pVertexBuffers[2];
			vbShadeParams[7].pName = "vertexTangent";
			vbShadeParams[7].ppBuffers = &pMainSceneGeom->pVertexBuffers[3];
			vbShadeParams[8].pName = "lights";
			vbShadeParams[8].ppBuffers = &pLightsBuffer;
			vbShadeParams[9].pName = "shadowMap";
			vbShadeParams[9].ppTextures = &pRenderTargetShadow->pTexture;
			vbShadeParams[10].pName = "meshConstantsBuffer";
			vbShadeParams[10].ppBuffers = &pMeshConstantsBuffer;
			vbShadeParams[11].pName = "headIndexBufferSRV";
			vbShadeParams[11].ppBuffers = &pHeadIndexBufferOIT;
			vbShadeParams[12].pName = "vbDepthLinkedListSRV";
			vbShadeParams[12].ppBuffers = &pVisBufLinkedListBufferOIT;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 13, vbShadeParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				vbShadeParamsPerFrame[0].pName = "lightClustersCount";
				vbShadeParamsPerFrame[0].ppBuffers = &pLightClustersCount[i];
				vbShadeParamsPerFrame[1].pName = "lightClusters";
				vbShadeParamsPerFrame[1].ppBuffers = &pLightClusters[i];
				vbShadeParamsPerFrame[2].pName = "PerFrameConstants";
				vbShadeParamsPerFrame[2].ppBuffers = &pPerFrameUniformBuffers[i];

				Buffer* pIndirectBuffers[GEOMSET_COUNT] = { NULL };
				for (uint32_t g = 0; g < GEOMSET_COUNT; ++g)
				{
					pIndirectBuffers[g] = pFilteredIndirectDrawArgumentsBuffer[i][g][VIEW_CAMERA];
				}

				for (uint32_t j = 0; j < 2; ++j)
				{
					vbShadeParamsPerFrame[3].pName = "indirectMaterialBuffer";
					vbShadeParamsPerFrame[3].ppBuffers =
						j == 0 ? &pFilterIndirectMaterialBuffer[i] : &pIndirectMaterialBufferAll;
					vbShadeParamsPerFrame[4].pName = "filteredIndexBuffer";
					vbShadeParamsPerFrame[4].ppBuffers = j == 0 ? &pFilteredIndexBuffer[i][VIEW_CAMERA] : &pMainSceneGeom->pIndexBuffer;
					vbShadeParamsPerFrame[5].pName = "indirectDrawArgs";
					vbShadeParamsPerFrame[5].mCount = GEOMSET_COUNT;
					vbShadeParamsPerFrame[5].ppBuffers = j == 0 ? pIndirectBuffers : pIndirectDrawArgumentsBufferAll;
					updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetVBShade[1], 6, vbShadeParamsPerFrame);
				}
			}
		}
		// Deferred Pass
		{
			DescriptorData params[7] = {};
			params[0].pName = "diffuseMaps";
			params[0].mCount = gMainSceneMaterialCount;
			params[0].ppTextures = gDiffuseMapsStorage;
			params[1].pName = "normalMaps";
			params[1].mCount = gMainSceneMaterialCount;
			params[1].ppTextures = gNormalMapsStorage;
			params[2].pName = "specularMaps";
			params[2].mCount = gMainSceneMaterialCount;
			params[2].ppTextures = gSpecularMapsStorage;
			params[3].pName = "meshConstantsBuffer";
			params[3].ppBuffers = &pMeshConstantsBuffer;
			params[4].pName = "geometryCountBuffer";
			params[4].ppBuffers = &pGeometryCountBufferOIT;
			params[5].pName = "headIndexBufferUAV";
			params[5].ppBuffers = &pHeadIndexBufferOIT;
			params[6].pName = "defDepthLinkedListUAV";
			params[6].ppBuffers = &pDeferredLinkedListBufferOIT;
			uint32_t paramCount = 7;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetDeferredPass[0], paramCount, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				for (uint32_t j = 0; j < 2; ++j)
				{
					DescriptorData params[2] = {};
					params[0].pName = "indirectMaterialBuffer";
					params[0].ppBuffers = j == 0 ? &pFilterIndirectMaterialBuffer[i] : &pIndirectMaterialBufferAll;
					params[1].pName = "PerFrameConstants";
					params[1].ppBuffers = &pPerFrameUniformBuffers[i];
					updateDescriptorSet(pRenderer, i * 2 + j, pDescriptorSetDeferredPass[1], 2, params);
				}
			}
		}

		// Deferred Shade
		{
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
			params[6].pName = "headIndexBufferSRV";
			params[6].ppBuffers = &pHeadIndexBufferOIT;
			params[7].pName = "defDepthLinkedListSRV";
			params[7].ppBuffers = &pDeferredLinkedListBufferOIT;
			uint32_t paramCount = 8;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetDeferredShade[0], paramCount, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "PerFrameConstants";
				params[0].mCount = 1;
				params[0].ppBuffers = &pPerFrameUniformBuffers[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetDeferredShade[1], 1, params);
			}
		}
		// Deferred Shade Lighting
		{
			DescriptorData params[7] = {};
			params[0].pName = "lights";
			params[0].ppBuffers = &pLightsBuffer;
			params[1].pName = "gBufferColor";
			params[1].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture;
			params[2].pName = "gBufferNormal";
			params[2].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture;
			params[3].pName = "gBufferSpecular";
			params[3].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
			params[4].pName = "gBufferSimulation";
			params[4].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture;
			params[5].pName = "gBufferDepth";
			params[5].ppTextures = &pDepthBuffer->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetDeferredShadePointLight[0], 6, params);
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "PerFrameConstants";
				params[0].ppBuffers = &pPerFrameUniformBuffers[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetDeferredShadePointLight[1], 1, params);
			}
		}

		// Resolve
		{
			DescriptorData params[2] = {};
			params[0].pName = "msaaSource";
			params[0].ppTextures = &pRenderTargetMSAA->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetResolve, 1, params);
			params[0].ppTextures = &pRenderTargetSun->pTexture;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetResolve, 1, params);
		}
		// Sun
		{
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				DescriptorData sunParams[1] = {};
				sunParams[0].pName = "UniformBufferSunMatrices";
				sunParams[0].ppBuffers = &pUniformBufferSun[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetSunPass, 1, sunParams);
			}
		}
		// God Ray
		{
			DescriptorData GodrayParams[1] = {};
			GodrayParams[0].pName = "uTex0";

			GodrayParams[0].ppTextures = gAppSettings.mMsaaLevel > 1 ? &pRenderTargetSunResolved->pTexture : &pRenderTargetSun->pTexture;

			updateDescriptorSet(pRenderer, 0, pDescriptorSetGodRayPass, 1, GodrayParams);

			GodrayParams[0].ppTextures = &pRenderTargetGodRay[0]->pTexture;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetGodRayPass, 1, GodrayParams);
			GodrayParams[0].ppTextures = &pRenderTargetGodRay[1]->pTexture;
			updateDescriptorSet(pRenderer, 2, pDescriptorSetGodRayPass, 1, GodrayParams);
		}
		// Curve conversion
		{
			DescriptorData CurveConversionParams[3] = {};
			CurveConversionParams[0].pName = "SceneTex";
			CurveConversionParams[0].ppTextures = &pIntermediateRenderTarget->pTexture;
			CurveConversionParams[1].pName = "GodRayTex";
			CurveConversionParams[1].ppTextures = &pRenderTargetGodRay[0]->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetCurveConversionPass, 2, CurveConversionParams);
			CurveConversionParams[1].ppTextures = &pRenderTargetGodRay[1]->pTexture;
			updateDescriptorSet(pRenderer, 1, pDescriptorSetCurveConversionPass, 2, CurveConversionParams);
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
	// Order Independent Transparency
	/************************************************************************/

	void addOrderIndependentTransparencyResources()
	{
		gMaxNodeCountOIT = OIT_MAX_FRAG_COUNT * mSettings.mWidth * mSettings.mHeight;

		BufferLoadDesc oitHeadIndexBufferDesc = {};
		oitHeadIndexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		oitHeadIndexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		oitHeadIndexBufferDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight;
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
		linkedListBufferDesc.mDesc.mElementCount = gMaxNodeCountOIT;
		linkedListBufferDesc.mDesc.mStructStride = sizeof(TransparentNodeOIT);
		linkedListBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		linkedListBufferDesc.mDesc.mSize = linkedListBufferDesc.mDesc.mElementCount * linkedListBufferDesc.mDesc.mStructStride;
		linkedListBufferDesc.ppBuffer = &pVisBufLinkedListBufferOIT;
		linkedListBufferDesc.mDesc.pName = "OIT VisBuf Linked List";
		addResource(&linkedListBufferDesc, NULL);

		linkedListBufferDesc.mDesc.mStructStride = sizeof(DeferredNodeOIT);
		linkedListBufferDesc.mDesc.mSize = linkedListBufferDesc.mDesc.mElementCount * linkedListBufferDesc.mDesc.mStructStride;
		linkedListBufferDesc.mDesc.pName = "OIT Deferred Linked List";
		linkedListBufferDesc.ppBuffer = &pDeferredLinkedListBufferOIT;
		addResource(&linkedListBufferDesc, NULL);
	}

	void removeOrderIndependentTransparencyResources()
	{
		removeResource(pGeometryCountBufferOIT);

		removeResource(pHeadIndexBufferOIT);
		removeResource(pVisBufLinkedListBufferOIT);
		removeResource(pDeferredLinkedListBufferOIT);
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
		if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
			swapChainDesc.mColorFormat = TinyImageFormat_R10G10B10A2_UNORM;
		else
			swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(false, true);

		swapChainDesc.mColorClearValue = { {1, 1, 1, 1} };
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	void addRenderTargets()
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

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
		depthRT.mSampleCount = gAppSettings.mMsaaLevel;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = width;
		depthRT.pName = "Depth Buffer RT";
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

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
		oitDepthRT.mWidth = width;
		oitDepthRT.pName = "Depth Buffer OIT RT";
		addRenderTarget(pRenderer, &oitDepthRT, &pDepthBufferOIT);

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
		//shadowRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		shadowRTDesc.mHeight = gShadowMapSize;
		shadowRTDesc.pName = "Shadow Map RT";
		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadow);

#if defined(XBOX)
		const uint32_t depthAllocationOffset = ALLOCATIONOFFSET();
#endif

		/************************************************************************/
		// Visibility buffer pass render target
		/************************************************************************/
		BEGINALLOCATION("VB RT", depthAllocationOffset);
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
		vbRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		vbRTDesc.mWidth = width;
		vbRTDesc.pName = "VB RT";
		addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);
		ENDALLOCATION("VB RT");
		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = optimizedColorClearBlack;
		deferredRTDesc.mDepth = 1;
		deferredRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		deferredRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
		deferredRTDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		deferredRTDesc.mHeight = height;
		deferredRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.mWidth = width;
		deferredRTDesc.pName = "G-Buffer RTs";

		const size_t maxNameBufferLength = 64;
		static char namebuffers[DEFERRED_RT_COUNT][maxNameBufferLength];
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			char * namebuf = namebuffers[i];
			snprintf(namebuf, maxNameBufferLength, "G-Buffer RT %u of %u", i + 1, DEFERRED_RT_COUNT);
			deferredRTDesc.pName = namebuf;
			addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
		}
		/************************************************************************/
		// MSAA render target
		/************************************************************************/
		RenderTargetDesc msaaRTDesc = {};
		msaaRTDesc.mArraySize = 1;
		msaaRTDesc.mClearValue = optimizedColorClearBlack;
		msaaRTDesc.mDepth = 1;
		msaaRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		msaaRTDesc.mFormat = gAppSettings.mOutputMode == OutputMode::OUTPUT_MODE_SDR ? getRecommendedSwapchainFormat(false, true) : TinyImageFormat_R10G10B10A2_UNORM;
		msaaRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		msaaRTDesc.mHeight = height;
		msaaRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
		msaaRTDesc.mSampleQuality = 0;
		msaaRTDesc.mWidth = width;
		msaaRTDesc.pName = "MSAA RT";
		addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);

		/************************************************************************/
		// Intermediate render target
		/************************************************************************/
		BEGINALLOCATION("Intermediate", depthAllocationOffset);
		RenderTargetDesc postProcRTDesc = {};
		postProcRTDesc.mArraySize = 1;
		postProcRTDesc.mClearValue = {{0.0f, 0.0f, 0.0f, 0.0f}};
		postProcRTDesc.mDepth = 1;
		postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		postProcRTDesc.mFormat = gAppSettings.mOutputMode == OutputMode::OUTPUT_MODE_SDR ? getRecommendedSwapchainFormat(false, true) : TinyImageFormat_R10G10B10A2_UNORM;
		postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postProcRTDesc.mHeight = mSettings.mHeight;
		postProcRTDesc.mWidth = mSettings.mWidth;
		postProcRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		postProcRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		postProcRTDesc.pName = "pIntermediateRenderTarget";
		addRenderTarget(pRenderer, &postProcRTDesc, &pIntermediateRenderTarget);

		/************************************************************************/
		// GodRay render target
		/************************************************************************/
		RenderTargetDesc GRRTDesc = {};
		GRRTDesc.mArraySize = 1;
		GRRTDesc.mClearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
		GRRTDesc.mDepth = 1;
		GRRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		GRRTDesc.mFormat = gAppSettings.mOutputMode == OutputMode::OUTPUT_MODE_SDR ? getRecommendedSwapchainFormat(false, true) : TinyImageFormat_R10G10B10A2_UNORM;
		GRRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		GRRTDesc.mHeight = mSettings.mHeight;
		GRRTDesc.mWidth = mSettings.mWidth;
		GRRTDesc.mSampleCount = gAppSettings.mMsaaLevel;
		GRRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		GRRTDesc.pName = "Sun RT";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetSun);

		GRRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		GRRTDesc.pName = "Sun Resolve RT";
		addRenderTarget(pRenderer, &GRRTDesc, &pRenderTargetSunResolved);

		GRRTDesc.mHeight = mSettings.mHeight / gGodrayScale;
		GRRTDesc.mWidth = mSettings.mWidth / gGodrayScale;
		GRRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		GRRTDesc.mFormat = pSwapChain->ppRenderTargets[0]->mFormat;
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
		postCurveConversionRTDesc.mHeight = mSettings.mHeight;
		postCurveConversionRTDesc.mWidth = mSettings.mWidth;
		postCurveConversionRTDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		postCurveConversionRTDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		postCurveConversionRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
		postCurveConversionRTDesc.pName = "pCurveConversionRenderTarget";
		addRenderTarget(pRenderer, &postCurveConversionRTDesc, &pCurveConversionRenderTarget);
		ENDALLOCATION("CurveConversion");
		/************************************************************************/
		/************************************************************************/
	}

	void removeRenderTargets()
	{
		removeRenderTarget(pRenderer, pCurveConversionRenderTarget);

		removeRenderTarget(pRenderer, pRenderTargetSun);
		removeRenderTarget(pRenderer, pRenderTargetSunResolved);
		removeRenderTarget(pRenderer, pRenderTargetGodRay[0]);
		removeRenderTarget(pRenderer, pRenderTargetGodRay[1]);

		removeRenderTarget(pRenderer, pIntermediateRenderTarget);
		removeRenderTarget(pRenderer, pRenderTargetMSAA);
		removeRenderTarget(pRenderer, pDepthBuffer);
		removeRenderTarget(pRenderer, pDepthBufferOIT);
		removeRenderTarget(pRenderer, pRenderTargetVBPass);
		removeRenderTarget(pRenderer, pRenderTargetShadow);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);
	}
	/************************************************************************/
	// Load all the shaders needed for the demo
	/************************************************************************/
	void addRootSignatures()
	{
		// Graphics root signatures
		const char* pTextureSamplerName = "textureFilter";
		const char* pShadingSamplerNames[] = { "depthSampler", "textureSampler" };
		Sampler*    pShadingSamplers[] = { pSamplerBilinearClamp, pSamplerBilinear };

		Shader* pShaders[GEOMSET_COUNT * 2] = {};
		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			pShaders[i * 2 + 0] = pShaderVisibilityBufferPass[i];
			pShaders[i * 2 + 1] = pShaderShadowPass[i];
		}
		RootSignatureDesc vbRootDesc = { pShaders, GEOMSET_COUNT * 2 };
		vbRootDesc.mMaxBindlessTextures = gMainSceneMaterialCount;
		vbRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
		vbRootDesc.ppStaticSamplers = &pSamplerPointClamp;
		vbRootDesc.mStaticSamplerCount = 1;
		addRootSignature(pRenderer, &vbRootDesc, &pRootSignatureVBPass);

		RootSignatureDesc deferredPassRootDesc = { pShaderDeferredPass, GEOMSET_COUNT };
		deferredPassRootDesc.mMaxBindlessTextures = gMainSceneMaterialCount;
		deferredPassRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
		deferredPassRootDesc.ppStaticSamplers = &pSamplerTrilinearAniso;
		deferredPassRootDesc.mStaticSamplerCount = 1;
		addRootSignature(pRenderer, &deferredPassRootDesc, &pRootSignatureDeferredPass);

		RootSignatureDesc shadeRootDesc = { pShaderVisibilityBufferShade, 2 * MSAA_LEVELS_COUNT };
		// Set max number of bindless textures in the root signature
		shadeRootDesc.mMaxBindlessTextures = gMainSceneMaterialCount;
		shadeRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
		shadeRootDesc.ppStaticSamplers = pShadingSamplers;
		shadeRootDesc.mStaticSamplerCount = 2;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureVBShade);

		shadeRootDesc.ppShaders = pShaderDeferredShade;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureDeferredShade);

		shadeRootDesc.ppShaders = pShaderDeferredShadePointLight;
		shadeRootDesc.mShaderCount = MSAA_LEVELS_COUNT;
		addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureDeferredShadePointLight);

		RootSignatureDesc resolveRootDesc = { pShaderResolve, MSAA_LEVELS_COUNT };
		addRootSignature(pRenderer, &resolveRootDesc, &pRootSignatureResolve);

		// Triangle filtering root signatures
		Shader* pCullingShaders[] = { pShaderClearBuffers, pShaderTriangleFiltering, pShaderBatchCompaction };
		RootSignatureDesc triangleFilteringRootDesc = { pCullingShaders, sizeof(pCullingShaders) / sizeof(pCullingShaders[0]) };

		addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);
		Shader* pClusterShaders[] = { pShaderClearLightClusters, pShaderClusterLights };
		RootSignatureDesc clearLightRootDesc = { pClusterShaders, 2 };
		addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureLightClusters);

		const char* pColorConvertStaticSamplerNames[] = { "uSampler0" };
		RootSignatureDesc CurveConversionRootSigDesc = { &pShaderCurveConversion, 1 };
		CurveConversionRootSigDesc.mStaticSamplerCount = 1;
		CurveConversionRootSigDesc.ppStaticSamplerNames = pColorConvertStaticSamplerNames;
		CurveConversionRootSigDesc.ppStaticSamplers = &pSamplerBilinearClamp;
		addRootSignature(pRenderer, &CurveConversionRootSigDesc, &pRootSigCurveConversionPass);

		RootSignatureDesc sunPassShaderRootSigDesc = { &pSunPass, 1 };
		addRootSignature(pRenderer, &sunPassShaderRootSigDesc, &pRootSigSunPass);

		const char* pGodRayStaticSamplerNames[] = { "uSampler0" };
		RootSignatureDesc godrayPassShaderRootSigDesc = { &pGodRayPass, 1 };
		godrayPassShaderRootSigDesc.mStaticSamplerCount = 1;
		godrayPassShaderRootSigDesc.ppStaticSamplerNames = pGodRayStaticSamplerNames;
		godrayPassShaderRootSigDesc.ppStaticSamplers = &pSamplerBilinearClamp;
		addRootSignature(pRenderer, &godrayPassShaderRootSigDesc, &pRootSigGodRayPass);
		gGodRayRootConstantIndex = getDescriptorIndexFromName(pRootSigGodRayPass, "RootConstantGodrayInfo");

		RootSignatureDesc oitHeadIndexClearRootSigDesc = { &pShaderClearHeadIndexOIT, 1 };
		addRootSignature(pRenderer, &oitHeadIndexClearRootSigDesc, &pRootSignatureClearHeadIndexOIT);

		const char* pPresentStaticSamplerNames[] = { "uSampler0" };
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

		CommandSignatureDesc deferredPassDesc = { pRootSignatureDeferredPass, indirectArgs, indirectArgCount };
		addIndirectCommandSignature(pRenderer, &deferredPassDesc, &pCmdSignatureDeferredPass);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureResolve);

		removeRootSignature(pRenderer, pRootSingatureSkybox);

		removeRootSignature(pRenderer, pRootSignatureClearHeadIndexOIT);

		removeRootSignature(pRenderer, pRootSigSunPass);
		removeRootSignature(pRenderer, pRootSigGodRayPass);
		removeRootSignature(pRenderer, pRootSigCurveConversionPass);

		removeRootSignature(pRenderer, pRootSigPresentPass);

		removeRootSignature(pRenderer, pRootSignatureLightClusters);
		removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
		removeRootSignature(pRenderer, pRootSignatureDeferredShadePointLight);
		removeRootSignature(pRenderer, pRootSignatureDeferredShade);
		removeRootSignature(pRenderer, pRootSignatureDeferredPass);
		removeRootSignature(pRenderer, pRootSignatureVBShade);
		removeRootSignature(pRenderer, pRootSignatureVBPass);

		// Remove indirect command signatures
		removeIndirectCommandSignature(pRenderer, pCmdSignatureDeferredPass);
		removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
	}

	void addShaders()
	{

		ShaderLoadDesc shadowPass = {};
		ShaderLoadDesc shadowPassAlpha = {};
		ShaderLoadDesc shadowPassTrans = {};
		ShaderLoadDesc vbPass = {};
		ShaderLoadDesc vbPassAlpha = {};
		ShaderLoadDesc vbPassTrans = {};
		ShaderLoadDesc deferredPass = {};
		ShaderLoadDesc deferredPassAlpha = {};
		ShaderLoadDesc deferredPassTrans = {};
		ShaderLoadDesc vbShade[2 * MSAA_LEVELS_COUNT] = {};
		ShaderLoadDesc deferredShade[2 * MSAA_LEVELS_COUNT] = {};
		ShaderLoadDesc deferredPointlights[MSAA_LEVELS_COUNT] = {};
		ShaderLoadDesc resolvePass[MSAA_LEVELS_COUNT] = {};
		ShaderLoadDesc resolveGodrayPass[MSAA_LEVELS_COUNT] = {};
		ShaderLoadDesc clearBuffer = {};
		ShaderLoadDesc triangleCulling = {};
		ShaderLoadDesc batchCompaction = {};
		ShaderLoadDesc clearLights = {};
		ShaderLoadDesc clusterLights = {};

		shadowPass.mStages[0] = { "shadow_pass.vert", NULL, 0 };

		shadowPassAlpha.mStages[0] = { "shadow_pass_alpha.vert", NULL, 0 };
		shadowPassAlpha.mStages[1] = { "shadow_pass_alpha.frag", NULL, 0 };

		shadowPassTrans.mStages[0] = { "shadow_pass_transparent.vert", NULL, 0 };
		shadowPassTrans.mStages[1] = { "shadow_pass_transparent.frag", NULL, 0 };

		vbPass.mStages[0] = { "visibilityBuffer_pass.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPass.mStages[1] = { "visibilityBuffer_pass.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };

		vbPassAlpha.mStages[0] = { "visibilityBuffer_pass_alpha.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassAlpha.mStages[1] = { "visibilityBuffer_pass_alpha.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };

#if defined(ORBIS) || defined(PROSPERO)
		// No SV_PrimitiveID in pixel shader on ORBIS. Only available in gs stage so we need
		// a passthrough gs
		vbPass.mStages[2] = { "visibilityBuffer_pass.geom", NULL, 0 };
		vbPassAlpha.mStages[2] = { "visibilityBuffer_pass_alpha.geom", NULL, 0 };

		vbPassTrans.mStages[0] = { "visibilityBuffer_pass_transparent_ret.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassTrans.mStages[1] = { "visibilityBuffer_pass_transparent_ret.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassTrans.mStages[2] = { "visibilityBuffer_pass_transparent_ret.geom", NULL, 0 };
#else
		vbPassTrans.mStages[0] = { "visibilityBuffer_pass_transparent_ret.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
		vbPassTrans.mStages[1] = { "visibilityBuffer_pass_transparent_void.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_PS_PRIMITIVEID };
#endif

		deferredPass.mStages[0] = { "deferred_pass.vert", NULL, 0 };
		deferredPass.mStages[1] = { "deferred_pass.frag", NULL, 0 };

		deferredPassAlpha.mStages[0] = { "deferred_pass.vert", NULL, 0 };
		deferredPassAlpha.mStages[1] = { "deferred_pass_alpha.frag", NULL, 0 };

		deferredPassTrans.mStages[0] = { "deferred_pass.vert", NULL, 0 };
		deferredPassTrans.mStages[1] = { "deferred_pass_transparent.frag", NULL, 0 };

		const char* visibilityBuffer_shade[] = {
			"visibilityBuffer_shade_SAMPLE_COUNT_1.frag",
			"visibilityBuffer_shade_SAMPLE_COUNT_1_AO.frag",
			"visibilityBuffer_shade_SAMPLE_COUNT_2.frag",
			"visibilityBuffer_shade_SAMPLE_COUNT_2_AO.frag",
			"visibilityBuffer_shade_SAMPLE_COUNT_4.frag",
			"visibilityBuffer_shade_SAMPLE_COUNT_4_AO.frag"
		};

		const char* deferred_shade[] = {
			"deferred_shade_SAMPLE_COUNT_1.frag",
			"deferred_shade_SAMPLE_COUNT_1_AO.frag",
			"deferred_shade_SAMPLE_COUNT_2.frag",
			"deferred_shade_SAMPLE_COUNT_2_AO.frag",
			"deferred_shade_SAMPLE_COUNT_4.frag",
			"deferred_shade_SAMPLE_COUNT_4_AO.frag"
		};

		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
		{
			for (uint32_t j = 0; j < 2; ++j)
			{
				uint32_t index = i * 2 + j;

				vbShade[index].mStages[0] = { "visibilityBuffer_shade.vert", NULL, 0 };
				vbShade[index].mStages[1] = { visibilityBuffer_shade[index], NULL, 0 };

				deferredShade[index].mStages[0] = { "deferred_shade.vert", NULL, 0 };
				deferredShade[index].mStages[1] = { deferred_shade[index], NULL, 0 };
			}
		}

		const char* deferred_shade_pointlight[] = {
			"deferred_shade_pointlight_SAMPLE_COUNT_1.frag",
			"deferred_shade_pointlight_SAMPLE_COUNT_2.frag",
			"deferred_shade_pointlight_SAMPLE_COUNT_4.frag"
		};
		const char* resolve[] = {
			"resolve_SAMPLE_COUNT_1.frag",
			"resolve_SAMPLE_COUNT_2.frag",
			"resolve_SAMPLE_COUNT_4.frag"
		};
		const char* resolveGodray[] = {
			"resolveGodray_SAMPLE_COUNT_1.frag",
			"resolveGodray_SAMPLE_COUNT_2.frag",
			"resolveGodray_SAMPLE_COUNT_4.frag"
		};

		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
		{
			deferredPointlights[i].mStages[0] = { "deferred_shade_pointlight.vert", NULL, 0 };
			deferredPointlights[i].mStages[1] = { deferred_shade_pointlight[i], NULL, 0 };

			// Resolve shader
			resolvePass[i].mStages[0] = { "resolve.vert", NULL, 0 };
			resolvePass[i].mStages[1] = { resolve[i], NULL, 0 };

			resolveGodrayPass[i].mStages[0] = { "resolve.vert", NULL, 0 };
			resolveGodrayPass[i].mStages[1] = { resolveGodray[i], NULL, 0 };
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

		ShaderLoadDesc oitHeadIndexClearDesc = {};
		oitHeadIndexClearDesc.mStages[0] = { "display.vert", NULL, 0 };
		oitHeadIndexClearDesc.mStages[1] = { "oitClear.frag", NULL, 0 };
		addShader(pRenderer, &oitHeadIndexClearDesc, &pShaderClearHeadIndexOIT);

		ShaderLoadDesc sunShaderDesc = {};
		sunShaderDesc.mStages[0] = { "sun.vert", NULL, 0 };
		sunShaderDesc.mStages[1] = { "sun.frag", NULL, 0 };
		addShader(pRenderer, &sunShaderDesc, &pSunPass);

		ShaderLoadDesc godrayShaderDesc = {};
		godrayShaderDesc.mStages[0] = { "display.vert", NULL, 0 };
		godrayShaderDesc.mStages[1] = { "godray.frag", NULL, 0 };
		addShader(pRenderer, &godrayShaderDesc, &pGodRayPass);

		ShaderLoadDesc CurveConversionShaderDesc = {};
		CurveConversionShaderDesc.mStages[0] = { "display.vert", NULL, 0 };
		CurveConversionShaderDesc.mStages[1] = { "CurveConversion.frag", NULL, 0 };
		addShader(pRenderer, &CurveConversionShaderDesc, &pShaderCurveConversion);

		ShaderLoadDesc presentShaderDesc = {};
		presentShaderDesc.mStages[0] = { "display.vert", NULL, 0 };
		presentShaderDesc.mStages[1] = { "display.frag", NULL, 0 };

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0 };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0 };

		addShader(pRenderer, &presentShaderDesc, &pShaderPresentPass);

		addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHATESTED]);
		addShader(pRenderer, &shadowPassTrans, &pShaderShadowPass[GEOMSET_TRANSPARENT]);
		addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		addShader(pRenderer, &vbPassTrans, &pShaderVisibilityBufferPass[GEOMSET_TRANSPARENT]);
		addShader(pRenderer, &deferredPass, &pShaderDeferredPass[GEOMSET_OPAQUE]);
		addShader(pRenderer, &deferredPassAlpha, &pShaderDeferredPass[GEOMSET_ALPHATESTED]);
		addShader(pRenderer, &deferredPassTrans, &pShaderDeferredPass[GEOMSET_TRANSPARENT]);
		for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
			addShader(pRenderer, &deferredShade[i], &pShaderDeferredShade[i]);
		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
			addShader(pRenderer, &deferredPointlights[i], &pShaderDeferredShadePointLight[i]);

		for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
			addShader(pRenderer, &vbShade[i], &pShaderVisibilityBufferShade[i]);
		addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
		addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
		addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
		addShader(pRenderer, &clusterLights, &pShaderClusterLights);
		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
		{
			addShader(pRenderer, &resolvePass[i], &pShaderResolve[i]);
			addShader(pRenderer, &resolveGodrayPass[i], &pShaderGodrayResolve[i]);
		}
		addShader(pRenderer, &batchCompaction, &pShaderBatchCompaction);
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHATESTED]);
		removeShader(pRenderer, pShaderShadowPass[GEOMSET_TRANSPARENT]);

		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHATESTED]);
		removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_TRANSPARENT]);

		for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
			removeShader(pRenderer, pShaderVisibilityBufferShade[i]);
		removeShader(pRenderer, pShaderDeferredPass[GEOMSET_OPAQUE]);
		removeShader(pRenderer, pShaderDeferredPass[GEOMSET_ALPHATESTED]);
		removeShader(pRenderer, pShaderDeferredPass[GEOMSET_TRANSPARENT]);
		for (uint32_t i = 0; i < 2 * MSAA_LEVELS_COUNT; ++i)
			removeShader(pRenderer, pShaderDeferredShade[i]);
		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
			removeShader(pRenderer, pShaderDeferredShadePointLight[i]);

		removeShader(pRenderer, pShaderTriangleFiltering);
		removeShader(pRenderer, pShaderBatchCompaction);
		removeShader(pRenderer, pShaderClearBuffers);
		removeShader(pRenderer, pShaderClusterLights);
		removeShader(pRenderer, pShaderClearLightClusters);
		for (uint32_t i = 0; i < MSAA_LEVELS_COUNT; ++i)
		{
			removeShader(pRenderer, pShaderResolve[i]);
			removeShader(pRenderer, pShaderGodrayResolve[i]);
		}

		removeShader(pRenderer, pShaderClearHeadIndexOIT); 

		removeShader(pRenderer, pSunPass);
		removeShader(pRenderer, pGodRayPass);

		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderCurveConversion);
		removeShader(pRenderer, pShaderPresentPass);
	}

	void addPipelines()
	{
		/************************************************************************/
		// Vertex layout used by all geometry passes (shadow, visibility)
		/************************************************************************/
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		DepthStateDesc depthStateDisableDesc = {};

		RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
		RasterizerStateDesc rasterizerStateCullBackDesc = { CULL_MODE_BACK };
		RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };

		RasterizerStateDesc rasterizerStateCullNoneMsDesc = { CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID };
		rasterizerStateCullNoneMsDesc.mMultiSample = true;
		RasterizerStateDesc rasterizerStateCullFrontMsDesc = { CULL_MODE_FRONT, 0, 0, FILL_MODE_SOLID };
		rasterizerStateCullFrontMsDesc.mMultiSample = true;

		BlendStateDesc blendStateSkyBoxDesc = {};
		blendStateSkyBoxDesc.mBlendModes[0] = BM_ADD;
		blendStateSkyBoxDesc.mBlendAlphaModes[0] = BM_ADD;

		blendStateSkyBoxDesc.mSrcFactors[0] = BC_ONE_MINUS_DST_ALPHA;
		blendStateSkyBoxDesc.mDstFactors[0] = BC_DST_ALPHA;

		blendStateSkyBoxDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateSkyBoxDesc.mDstAlphaFactors[0] = BC_ONE;

		blendStateSkyBoxDesc.mMasks[0] = ALL;
		blendStateSkyBoxDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateSkyBoxDesc.mIndependentBlend = false;

		VertexLayout vertexLayoutPosTexNormTan = {};
		vertexLayoutPosTexNormTan.mAttribCount = 4;
		vertexLayoutPosTexNormTan.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPosTexNormTan.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		vertexLayoutPosTexNormTan.mAttribs[0].mBinding = 0;
		vertexLayoutPosTexNormTan.mAttribs[0].mLocation = 0;
		vertexLayoutPosTexNormTan.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutPosTexNormTan.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutPosTexNormTan.mAttribs[1].mBinding = 1;
		vertexLayoutPosTexNormTan.mAttribs[1].mLocation = 1;
		vertexLayoutPosTexNormTan.mAttribs[2].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutPosTexNormTan.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutPosTexNormTan.mAttribs[2].mBinding = 2;
		vertexLayoutPosTexNormTan.mAttribs[2].mLocation = 2;
		vertexLayoutPosTexNormTan.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
		vertexLayoutPosTexNormTan.mAttribs[3].mFormat = TinyImageFormat_R32_UINT;
		vertexLayoutPosTexNormTan.mAttribs[3].mBinding = 3;
		vertexLayoutPosTexNormTan.mAttribs[3].mLocation = 3;

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

		// Setup pipeline settings
		PipelineDesc pipelineDesc = {};
		pipelineDesc.pCache = pPipelineCache;

		/************************************************************************/
		// Setup compute pipelines for triangle filtering
		/************************************************************************/
		pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineSettings = pipelineDesc.mComputeDesc;
		computePipelineSettings.pShaderProgram = pShaderClearBuffers;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

		// Create the compute pipeline for GPU triangle filtering
		computePipelineSettings.pShaderProgram = pShaderTriangleFiltering;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

		computePipelineSettings.pShaderProgram = pShaderBatchCompaction;
		computePipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineBatchCompaction);

		// Setup the clearing light clusters pipeline
		computePipelineSettings.pShaderProgram = pShaderClearLightClusters;
		computePipelineSettings.pRootSignature = pRootSignatureLightClusters;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

		// Setup the compute the light clusters pipeline
		computePipelineSettings.pShaderProgram = pShaderClusterLights;
		computePipelineSettings.pRootSignature = pRootSignatureLightClusters;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);
		/************************************************************************/
		// Setup MSAA resolve pipeline
		/************************************************************************/
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
		addPipeline(pRenderer, &pipelineDesc, &pPipelineResolve);
		addPipeline(pRenderer, &pipelineDesc, &pPipelineResolvePost);

		/************************************************************************/
		// Setup Head Index Clear Pipeline
		/************************************************************************/
		GraphicsPipelineDesc& headIndexPipelineSettings = pipelineDesc.mGraphicsDesc;
		headIndexPipelineSettings = { 0 };
		headIndexPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		headIndexPipelineSettings.mRenderTargetCount = 0;
		headIndexPipelineSettings.pColorFormats = NULL;
		headIndexPipelineSettings.mSampleCount = pDepthBuffer->mSampleCount;
		headIndexPipelineSettings.mSampleQuality = 0;
		headIndexPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		headIndexPipelineSettings.pVertexLayout = NULL;
		headIndexPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		headIndexPipelineSettings.pDepthState = &depthStateDisableDesc;
		headIndexPipelineSettings.pBlendState = NULL;
		headIndexPipelineSettings.pRootSignature = pRootSignatureClearHeadIndexOIT;
		headIndexPipelineSettings.pShaderProgram = pShaderClearHeadIndexOIT;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineClearHeadIndexOIT);

		GraphicsPipelineDesc& resolveGodrayPipelineSettings = pipelineDesc.mGraphicsDesc;
		resolveGodrayPipelineSettings = { 0 };
		resolveGodrayPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		resolveGodrayPipelineSettings.mRenderTargetCount = 1;
		resolveGodrayPipelineSettings.pDepthState = &depthStateDisableDesc;
		resolveGodrayPipelineSettings.pColorFormats = &pRenderTargetSun->mFormat;
		resolveGodrayPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		resolveGodrayPipelineSettings.mSampleQuality = 0;
		resolveGodrayPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		resolveGodrayPipelineSettings.pRootSignature = pRootSignatureResolve;
		resolveGodrayPipelineSettings.pShaderProgram = pShaderGodrayResolve[gAppSettings.mMsaaIndex];
		addPipeline(pRenderer, &pipelineDesc, &pPipelineGodrayResolve);
		addPipeline(pRenderer, &pipelineDesc, &pPipelineGodrayResolvePost);

		/************************************************************************/
		// Setup the Shadow Pass Pipeline
		/************************************************************************/
		GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
		shadowPipelineSettings = { 0 };
		shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowPipelineSettings.pDepthState = &depthStateDesc;
		shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
		shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
		shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
		shadowPipelineSettings.pRootSignature = pRootSignatureVBPass;
		shadowPipelineSettings.mSupportIndirectCommandBuffer = true;

		shadowPipelineSettings.pRasterizerState = gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullFrontMsDesc : &rasterizerStateCullFrontDesc;
		shadowPipelineSettings.pVertexLayout = &vertexLayoutPositionOnly;
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_OPAQUE];
		addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_OPAQUE]);

		shadowPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_ALPHATESTED];
		addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_ALPHATESTED]);

		shadowPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[GEOMSET_TRANSPARENT];
		addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[GEOMSET_TRANSPARENT]);

		/************************************************************************/
		// Setup the Visibility Buffer Pass Pipeline
		/************************************************************************/
		// Setup pipeline settings
		GraphicsPipelineDesc& vbPassPipelineSettings = pipelineDesc.mGraphicsDesc;
		vbPassPipelineSettings = { 0 };
		vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		vbPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
		vbPassPipelineSettings.pVertexLayout = &vertexLayoutPosAndTex;
		vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mFormat;
		vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
		vbPassPipelineSettings.mSupportIndirectCommandBuffer = true;
		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			vbPassPipelineSettings.pVertexLayout = (i == GEOMSET_OPAQUE) ? &vertexLayoutPositionOnly : &vertexLayoutPosAndTex;
			vbPassPipelineSettings.pDepthState = (i == GEOMSET_TRANSPARENT) ? &depthStateDisableDesc : &depthStateDesc;
			vbPassPipelineSettings.mSampleCount = (i == GEOMSET_TRANSPARENT) ? SAMPLE_COUNT_1 : pRenderTargetVBPass->mSampleCount;
			vbPassPipelineSettings.mRenderTargetCount = (i == GEOMSET_TRANSPARENT) ? 0 : 1;

			if (gAppSettings.mMsaaLevel > 1)
			{
				switch (i)
				{
				case GEOMSET_OPAQUE: vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullFrontMsDesc; break;
				case GEOMSET_ALPHATESTED: vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneMsDesc; break;
				case GEOMSET_TRANSPARENT: vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc; break;   // Transparent not multisampled
				default: break;
				}
			}
			else
			{
				switch (i)
				{
				case GEOMSET_OPAQUE: vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullFrontDesc; break;
				case GEOMSET_ALPHATESTED:
				case GEOMSET_TRANSPARENT: vbPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc; break;
				default: break;
				}
			}

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
		vbShadePipelineSettings.pRasterizerState = gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullFrontMsDesc : &rasterizerStateCullFrontDesc;
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
				//vbShadePipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
				vbShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
				vbShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			}

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
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		TinyImageFormat deferredFormats[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[i]->mFormat;
		}

		GraphicsPipelineDesc& deferredPassPipelineSettings = pipelineDesc.mGraphicsDesc;
		deferredPassPipelineSettings = { 0 };
		deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPassPipelineSettings.pColorFormats = deferredFormats;
		deferredPassPipelineSettings.mSampleCount = pDepthBuffer->mSampleCount;
		deferredPassPipelineSettings.mSampleQuality = pDepthBuffer->mSampleQuality;
		deferredPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		deferredPassPipelineSettings.pRootSignature = pRootSignatureDeferredPass;
		deferredPassPipelineSettings.pVertexLayout = &vertexLayoutPosTexNormTan;
		deferredPassPipelineSettings.mSupportIndirectCommandBuffer = true;

		// Create pipelines for geometry sets
		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			deferredPassPipelineSettings.pDepthState = (i == GEOMSET_TRANSPARENT) ? &depthStateDisableDesc : &depthStateDesc;
			deferredPassPipelineSettings.mSampleCount = (i == GEOMSET_TRANSPARENT) ? SAMPLE_COUNT_1 : pDepthBuffer->mSampleCount;
			deferredPassPipelineSettings.mRenderTargetCount = (i == GEOMSET_TRANSPARENT) ? 0 : DEFERRED_RT_COUNT;

			if (gAppSettings.mMsaaLevel > 1)
			{
				switch (i)
				{
				case GEOMSET_OPAQUE: deferredPassPipelineSettings.pRasterizerState = &rasterizerStateCullFrontMsDesc; break;
				case GEOMSET_ALPHATESTED: deferredPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneMsDesc; break;
				case GEOMSET_TRANSPARENT: deferredPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc; break;   // Transparent not multisampled
				default: break;
				}
			}
			else
			{
				switch (i)
				{
				case GEOMSET_OPAQUE: deferredPassPipelineSettings.pRasterizerState = &rasterizerStateCullFrontDesc; break;
				case GEOMSET_ALPHATESTED:
				case GEOMSET_TRANSPARENT: deferredPassPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc; break;
				default: break;
				}
			}
			deferredPassPipelineSettings.pShaderProgram = pShaderDeferredPass[i];
			addPipeline(pRenderer, &pipelineDesc, &pPipelineDeferredPass[i]);
		}

		/************************************************************************/
		// Setup the resources needed for the Deferred Shade Pipeline
		/************************************************************************/
		// Setup pipeline settings

		// Create pipeline
		// Note: the vertex layout is set to null because the positions of the fullscreen triangle are being calculated automatically
		// in the vertex shader using each vertex_id.
		GraphicsPipelineDesc& deferredShadePipelineSettings = pipelineDesc.mGraphicsDesc;
		deferredShadePipelineSettings = { 0 };
		deferredShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredShadePipelineSettings.mRenderTargetCount = 1;
		deferredShadePipelineSettings.pDepthState = &depthStateDisableDesc;
		deferredShadePipelineSettings.pRasterizerState = gAppSettings.mMsaaLevel > 1 ? &rasterizerStateCullNoneMsDesc : &rasterizerStateCullNoneDesc;
		deferredShadePipelineSettings.pRootSignature = pRootSignatureDeferredShade;

		for (uint32_t i = 0; i < 2; ++i)
		{
			deferredShadePipelineSettings.pShaderProgram = pShaderDeferredShade[gAppSettings.mMsaaIndex * 2 + i];
			deferredShadePipelineSettings.mSampleCount = gAppSettings.mMsaaLevel;
			if (gAppSettings.mMsaaLevel > 1)
			{
				deferredShadePipelineSettings.pColorFormats = &pRenderTargetMSAA->mFormat;
				deferredShadePipelineSettings.mSampleQuality = pRenderTargetMSAA->mSampleQuality;
			}
			else
			{
				//deferredShadePipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
				deferredShadePipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
				deferredShadePipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
			}
			addPipeline(pRenderer, &pipelineDesc, &pPipelineDeferredShadeSrgb[i]);
		}
		/************************************************************************/
		// Setup the resources needed for the Deferred Point Light Shade Pipeline
		/************************************************************************/
		// Create vertex layout
		VertexLayout vertexLayoutPointLightShade = {};
		vertexLayoutPointLightShade.mAttribCount = 1;
		vertexLayoutPointLightShade.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutPointLightShade.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		vertexLayoutPointLightShade.mAttribs[0].mBinding = 0;
		vertexLayoutPointLightShade.mAttribs[0].mLocation = 0;
		vertexLayoutPointLightShade.mAttribs[0].mOffset = 0;

		// Setup pipeline settings
		GraphicsPipelineDesc& deferredPointLightPipelineSettings = pipelineDesc.mGraphicsDesc;
		//deferredPointLightPipelineSettings = {0};
		deferredPointLightPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredPointLightPipelineSettings.mRenderTargetCount = 1;
		deferredPointLightPipelineSettings.pDepthState = &depthStateDisableDesc;
		deferredPointLightPipelineSettings.pColorFormats = deferredShadePipelineSettings.pColorFormats;
		deferredPointLightPipelineSettings.mSampleCount = deferredShadePipelineSettings.mSampleCount;
		deferredPointLightPipelineSettings.pRasterizerState = &rasterizerStateCullBackDesc;
		deferredPointLightPipelineSettings.pRootSignature = pRootSignatureDeferredShadePointLight;
		deferredPointLightPipelineSettings.pShaderProgram = pShaderDeferredShadePointLight[gAppSettings.mMsaaIndex];
		deferredPointLightPipelineSettings.pVertexLayout = &vertexLayoutPointLightShade;

		addPipeline(pRenderer, &pipelineDesc, &pPipelineDeferredShadePointLightSrgb);

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

		pipelineSettings.pColorFormats = &pIntermediateRenderTarget->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		//pipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		pipelineSettings.pRootSignature = pRootSingatureSkybox;
		pipelineSettings.pShaderProgram = pShaderSkybox;
		pipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &pipelineDesc, &pSkyboxPipeline);

		/************************************************************************/
		// Setup Sun pipeline
		/************************************************************************/
		//layout and pipeline for skybox draw
		//Draw Sun
		GraphicsPipelineDesc& pipelineSettingsSun = pipelineDesc.mGraphicsDesc;
		pipelineSettingsSun = { 0 };
		pipelineSettingsSun.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettingsSun.pRasterizerState = &rasterizerStateCullBackDesc;
		pipelineSettingsSun.pDepthState = &depthStateDesc;
		pipelineSettingsSun.mDepthStencilFormat = pDepthBuffer->mFormat;

		pipelineSettingsSun.mRenderTargetCount = 1;
		pipelineSettingsSun.pColorFormats = &pRenderTargetSun->mFormat;
		pipelineSettingsSun.mSampleCount = gAppSettings.mMsaaLevel;
		pipelineSettingsSun.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;

		pipelineSettingsSun.pVertexLayout = &gVertexLayoutSun;
		pipelineSettingsSun.pRootSignature = pRootSigSunPass;
		pipelineSettingsSun.pShaderProgram = pSunPass;
		addPipeline(pRenderer, &pipelineDesc, &pPipelineSunPass);

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
		pipelineSettingsGodRay.pShaderProgram = pGodRayPass;
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

		addPipeline(pRenderer, &pipelineDesc, &pPipelinePresentPass);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineResolve);
		removePipeline(pRenderer, pPipelineResolvePost);

		removePipeline(pRenderer, pPipelineSunPass);
		removePipeline(pRenderer, pPipelineGodRayPass);

		removePipeline(pRenderer, pPipelineClearHeadIndexOIT);

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

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
			removePipeline(pRenderer, pPipelineDeferredPass[i]);

		for (uint32_t i = 0; i < 2; ++i)
		{
			removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
		}

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
			removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
			removePipeline(pRenderer, pPipelineShadowPass[i]);

		removePipeline(pRenderer, pSkyboxPipeline);

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
		uint32_t* alphaTestMaterials = (uint32_t*)tf_malloc(gMainSceneMaterialCount * sizeof(uint32_t));
		for (uint32_t i = 0; i < gMainSceneMaterialCount; ++i)
		{
			alphaTestMaterials[i] = pScene->materials[i].alphaTested ? 1 : 0;
			alphaTestMaterials[i] = pScene->materials[i].transparent ? 2 : alphaTestMaterials[i];
		}

		BufferLoadDesc materialPropDesc = {};
		materialPropDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		materialPropDesc.mDesc.mElementCount = gMainSceneMaterialCount;
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
		const uint32_t numBatches = (const uint32_t)gMainSceneMeshCount;
		const uint32_t argOffset = pRenderer->pActiveGpuSettings->mIndirectRootConstant ? 1 : 0;
		uint32_t materialIDPerDrawCall[MATERIAL_BUFFER_SIZE] = {};
		uint32_t indirectArgsNoAlphaDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};
		uint32_t indirectArgsDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};
		uint32_t indirectArgsTransDwords[MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = {};
		uint32_t iAlpha = 0, iNoAlpha = 0, iTransparent = 0;
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
					materialIDPerDrawCall[BaseMaterialBuffer(GEOMSET_ALPHATESTED, j) + iAlpha] = matID;
				iAlpha++;
			}
			else if (mat->transparent)
			{
				IndirectDrawIndexArguments* arg = (IndirectDrawIndexArguments*)&indirectArgsTransDwords[iTransparent * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS + argOffset];
				*arg = pScene->geom->pDrawArgs[i];
				if (pRenderer->pActiveGpuSettings->mIndirectRootConstant)
				{
					indirectArgsTransDwords[iTransparent * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS] = iTransparent;
				}
				else
				{
					// No drawId or gl_DrawId but instance id works as expected so use that as the draw id
					arg->mStartInstance = iTransparent;
				}
				for (uint32_t j = 0; j < gNumViews; ++j)
					materialIDPerDrawCall[BaseMaterialBuffer(GEOMSET_TRANSPARENT, j) + iTransparent] = matID;
				iTransparent++;
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
					materialIDPerDrawCall[BaseMaterialBuffer(GEOMSET_OPAQUE, j) + iNoAlpha] = matID;
				iNoAlpha++;
			}
		}
		indirectArgsDwords[DRAW_COUNTER_SLOT_POS] = iAlpha;
		indirectArgsTransDwords[DRAW_COUNTER_SLOT_POS] = iTransparent;
		indirectArgsNoAlphaDwords[DRAW_COUNTER_SLOT_POS] = iNoAlpha;

		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = iNoAlpha;
			gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHATESTED] = iAlpha;
			gPerFrame[frameIdx].gDrawCount[GEOMSET_TRANSPARENT] = iTransparent;
		}

		// DX12 / Vulkan needs two indirect buffers since ExecuteIndirect is not called per mesh but per geometry set (ALPHA_TEST and OPAQUE)
		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			// Setup uniform data for draw batch data
			BufferLoadDesc indirectBufferDesc = {};
			indirectBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDIRECT_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS;
			indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
			indirectBufferDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE | RESOURCE_STATE_INDIRECT_ARGUMENT;
			indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
			indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll[i];
			indirectBufferDesc.mDesc.pName = "Indirect Buffer Desc";

			switch (i)
			{
				case GEOMSET_OPAQUE:      indirectBufferDesc.pData = indirectArgsNoAlphaDwords; break;
				case GEOMSET_ALPHATESTED: indirectBufferDesc.pData = indirectArgsDwords;   break;
				case GEOMSET_TRANSPARENT: indirectBufferDesc.pData = indirectArgsTransDwords;   break;
				default: break; 
			}

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
		filterIbDesc.mDesc.mElementCount = pScene->geom->mIndexCount;
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
			arg[argOffset + INDIRECT_DRAW_INDEX_ELEM_INDEX(mInstanceCount)] = (i < gMainSceneMeshCount) ? 1 : 0;
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

				for (uint32_t geom = 0; geom < GEOMSET_COUNT; ++geom)
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
		MeshConstants* meshConstants = (MeshConstants*)tf_malloc(gMainSceneMeshCount * sizeof(MeshConstants));

		for (uint32_t i = 0; i < gMainSceneMeshCount; ++i)
		{
			meshConstants[i].faceCount = pScene->geom->pDrawArgs[i].mIndexCount / 3;
			meshConstants[i].indexOffset = pScene->geom->pDrawArgs[i].mStartIndex;
			meshConstants[i].materialID = i;
			meshConstants[i].twoSided = pScene->materials[i].twoSided ? 1 : 0;
		}

		BufferLoadDesc meshConstantDesc = {};
		meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		meshConstantDesc.mDesc.mElementCount = gMainSceneMeshCount;
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

		BufferLoadDesc sunDataBufferDesc = {};
		sunDataBufferDesc.mDesc.mSize = sizeof(UniformDataSunMatrices);
		sunDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sunDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sunDataBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sunDataBufferDesc.pData = NULL;
		sunDataBufferDesc.mDesc.pName = "Sun matrices Data Buffer Desc";
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			sunDataBufferDesc.ppBuffer = &pUniformBufferSun[frameIdx];
			addResource(&sunDataBufferDesc, NULL);
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
		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
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
				for (uint32_t geom = 0; geom < GEOMSET_COUNT; ++geom)
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
			removeResource(pUniformBufferSun[frameIdx]);
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
		//mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
		mat4 cameraView = pCameraController->getViewMatrix();
		mat4 cameraProj = mat4::perspective(PI / 2.0f, aspectRatioInv, gAppSettings.nearPlane, gAppSettings.farPlane);

		// Compute light matrices
		Point3 lightSourcePos(50.0f, 000.0f, 450.0f);

		// directional light rotation & translation
		mat4 rotation = mat4::rotationXY(gAppSettings.mSunControl.x, gAppSettings.mSunControl.y);
		mat4 translation = mat4::translation(-vec3(lightSourcePos));
		vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));

		mat4 lightModel = mat4::translation(vec3(-20, 0, 0)) * mat4::scale(vec3(SCENE_SCALE));
		mat4 lightView = rotation * translation;
		mat4 lightProj = mat4::orthographic(-600, 600, -950, 350, -1100, 500);

		float2 twoOverRes;
		twoOverRes.setX(gAppSettings.mRetinaScaling / float(width));
		twoOverRes.setY(gAppSettings.mRetinaScaling / float(height));
		/************************************************************************/
		// Order independent transparency data
		/************************************************************************/
		currentFrame->gPerFrameUniformData.maxNodeCount = gMaxNodeCountOIT;
		currentFrame->gPerFrameUniformData.transAlpha = gFlagAlpha;
		currentFrame->gPerFrameUniformData.screenWidth = width;
		currentFrame->gPerFrameUniformData.screenHeight = height;
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

		currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].sampleCount = gAppSettings.mMsaaLevel;
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
		currentFrame->gPerFrameUniformData.outputMode = (uint)gAppSettings.mOutputMode;
		currentFrame->gPerFrameUniformData.CameraPlane = { gAppSettings.nearPlane, gAppSettings.farPlane };
		/************************************************************************/
		// Sun, God ray
		/************************************************************************/
		mat4 sunScale = mat4::scale(vec3(gAppSettings.mSunSize, gAppSettings.mSunSize, gAppSettings.mSunSize));
		mat4 sunTrans = mat4::translation(vec3(-lightDir.getX() * 2000.0f, -lightDir.getY() * 1400.0f, -lightDir.getZ() * 2000.0f));
		mat4  SunMVP = cameraProj * cameraView * sunTrans * sunScale;

		currentFrame->gUniformDataSunMatrices.mProjectView = cameraProj * cameraView;
		currentFrame->gUniformDataSunMatrices.mModelMat = sunTrans * sunScale;
		currentFrame->gUniformDataSunMatrices.mLightColor = f4Tov4(gAppSettings.mLightColor);

		vec4 lightPos = SunMVP[3];
		lightPos /= lightPos.getW();

		float2 lightPosSS;

		lightPosSS.x = (lightPos.getX() + 1.0f) * 0.5f;
		lightPosSS.y = (1.0f - lightPos.getY()) * 0.5f;

		gAppSettings.gGodrayInfo.lightPosInSS = lightPosSS;
		currentFrame->gGodrayInfo = gAppSettings.gGodrayInfo;
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

		gSCurveInfomation.linearScale = gAppSettings.LinearScale;
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
			if (gAppSettings.mOutputMode == OUTPUT_MODE_HDR10)
				uiShowDynamicWidgets(&gAppSettings.mDisplaySetting, pGuiWindow);
			else
			{
				if (prevOutputMode == OUTPUT_MODE_HDR10)
					uiHideDynamicWidgets(&gAppSettings.mDisplaySetting, pGuiWindow);
			}
		}

		prevOutputMode = gAppSettings.mOutputMode;

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
				uiLoad.mColorFormat = gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
				uiLoad.mHeight = mSettings.mHeight;
				uiLoad.mWidth = mSettings.mWidth;
				uiLoad.mLoadType = RELOAD_TYPE_RENDERTARGET;
				loadUserInterface(&uiLoad);

				FontSystemLoadDesc fontLoad = {};
				fontLoad.mColorFormat = gAppSettings.mEnableGodray ? pSwapChain->ppRenderTargets[0]->mFormat : pIntermediateRenderTarget->mFormat;
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

		// Render mode
		{
			static RenderMode gPrevRenderMode = gAppSettings.mRenderMode;
			if (gPrevRenderMode != gAppSettings.mRenderMode)
			{
				SetupDebugTexturesWindow();
				gPrevRenderMode = gAppSettings.mRenderMode;
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
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadow->mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetShadow, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mWidth, (float)pRenderTargetShadow->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mWidth, pRenderTargetShadow->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pMainSceneGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);
		
        const char* profileNames[GEOMSET_COUNT] = { "SM Opaque", "SM Alpha", "SM Transparent" };

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

			cmdBindPipeline(cmd, pPipelineShadowPass[0]);

			Buffer* pVertexBuffersPositionOnly[] = { pMainSceneGeom->pVertexBuffers[0] };
			cmdBindVertexBuffer(cmd, 1, pVertexBuffersPositionOnly, pMainSceneGeom->mVertexStrides, NULL);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
			cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBPass[1]);

			Buffer* pIndirectBufferPositionOnly = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_SHADOW]
				: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBufferPositionOnly, 0, pIndirectBufferPositionOnly,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	// Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
	// into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
	// to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
	// less memory bandwidth is used.
	void drawVisibilityBufferPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
	{
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetVBPass->mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mClearValue;

		/************************************************************************/
		// Clear OIT Head Index Texture
		/************************************************************************/

		cmdBindRenderTargets(cmd, 0, NULL, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);
		
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear OIT Head Index");

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdBindPipeline(cmd, pPipelineClearHeadIndexOIT); 
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetClearHeadIndexOIT[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClearHeadIndexOIT[1]);
		cmdDraw(cmd, 3, 0);
		
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler); 
		
		BufferBarrier bufferBarriers[2] = {
			{ pGeometryCountBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
			{ pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);

		/************************************************************************/
		// Visibility Buffer Pass
		/************************************************************************/

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 1, &pRenderTargetVBPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pMainSceneGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

		const char* profileNames[GEOMSET_COUNT] = { "VB Opaque", "VB Alpha", "VB Transparent" };

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			if (i == GEOMSET_TRANSPARENT)
			{
				cmdBindRenderTargets(cmd, 0, NULL, pDepthBufferOIT, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);
			}

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
		
			cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
			
			Buffer* pVertexBuffers[] = { pMainSceneGeom->pVertexBuffers[0], pMainSceneGeom->pVertexBuffers[1] };
			cmdBindVertexBuffer(cmd, 2, pVertexBuffers, pMainSceneGeom->mVertexStrides, NULL);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
			cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBPass[1]);
			
			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA]
				: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		bufferBarriers[0] = { pVisBufLinkedListBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
		bufferBarriers[1] = { pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);
	}

	// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawVisibilityBufferPass
	// to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index buffer because
	// the triangle positions are calculated internally using vertex_id.
	void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestinationRenderTarget->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb[0]);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
		cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetVBShade[1]);
		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		BufferBarrier bufferBarriers[3] = {
			{ pGeometryCountBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pVisBufLinkedListBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pHeadIndexBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);
	}

	// Render the scene to perform the Deferred geometry pass. In this pass the (filtered) scene geometry is rendered
	// into a gBuffer, containing per-pixel geometric information such as normals, textures or depth. This information
	// will be used later to calculate per pixel color in the shading pass.
	void drawDeferredPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
	{
		// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[i]->mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pDepthBuffer->mClearValue;

		/************************************************************************/
		// Clear OIT Head Index Texture
		/************************************************************************/

		cmdBindRenderTargets(cmd, 0, NULL, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear OIT Head Index");

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdBindPipeline(cmd, pPipelineClearHeadIndexOIT);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetClearHeadIndexOIT[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetClearHeadIndexOIT[1]);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

		BufferBarrier bufferBarriers[2] = {
			{ pGeometryCountBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
			{ pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);

		/************************************************************************/
		// Deferred Shading Pass
		/************************************************************************/

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mWidth, (float)pRenderTargetDeferredPass[0]->mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mWidth, pRenderTargetDeferredPass[0]->mHeight);

		Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pMainSceneGeom->pIndexBuffer;
		cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

		const char* profileNames[GEOMSET_COUNT] = { "DP Opaque", "DP Alpha", "DP Transparent" };

		for (uint32_t i = 0; i < GEOMSET_COUNT; ++i)
		{
			if (i == GEOMSET_TRANSPARENT)
			{
				cmdBindRenderTargets(cmd, 0, NULL, pDepthBufferOIT, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mWidth, (float)pRenderTargetDeferredPass[0]->mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mWidth, pRenderTargetDeferredPass[0]->mHeight);
			}

			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

			cmdBindPipeline(cmd, pPipelineDeferredPass[i]);

			Buffer* pVertexBuffers[] = { pMainSceneGeom->pVertexBuffers[0], pMainSceneGeom->pVertexBuffers[1], pMainSceneGeom->pVertexBuffers[2], pMainSceneGeom->pVertexBuffers[3] };
			cmdBindVertexBuffer(cmd, 4, pVertexBuffers, pMainSceneGeom->mVertexStrides, NULL);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetDeferredPass[0]);
			cmdBindDescriptorSet(cmd, frameIdx * 2 + (uint32_t)(!gAppSettings.mFilterTriangles), pDescriptorSetDeferredPass[1]);

			Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA]
				: pIndirectDrawArgumentsBufferAll[i];
			cmdExecuteIndirect(
				cmd, pCmdSignatureDeferredPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer,
				DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		}

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		bufferBarriers[0] = { pDeferredLinkedListBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
		bufferBarriers[1] = { pHeadIndexBufferOIT, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);
	}

	// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawDeferredPass
	// to get per pixel geometry data to calculate the final color.
	void drawDeferredShade(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pDestinationRenderTarget->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPipelineDeferredShadeSrgb[0]);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetDeferredShade[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetDeferredShade[1]);
		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		BufferBarrier bufferBarriers[3] = {
			{ pGeometryCountBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pDeferredLinkedListBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pHeadIndexBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
		};
		cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);
	}

	// Render light geometry on the screen to evaluate lighting at those points.
	void drawDeferredShadePointLights(Cmd* cmd, uint32_t frameIdx)
	{
		RenderTarget* pDestinationRenderTarget = gAppSettings.mMsaaLevel > 1 ? pRenderTargetMSAA : pScreenRenderTarget;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mWidth, (float)pDestinationRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mWidth, pDestinationRenderTarget->mHeight);

		const uint32_t stride = sizeof(float) * 4;
		cmdBindPipeline(cmd, pPipelineDeferredShadePointLightSrgb);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetDeferredShadePointLight[0]);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetDeferredShadePointLight[1]);
		cmdBindVertexBuffer(cmd, 1, &pVertexBufferCube, &stride, NULL);
		cmdBindIndexBuffer(cmd, pIndexBufferCube, INDEX_TYPE_UINT16, 0);
		cmdDrawIndexedInstanced(cmd, 36, 0, LIGHT_COUNT, 0, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	void resolveMSAA(Cmd* cmd, RenderTarget* msaaRT, RenderTarget* destRT)
	{
		// transition world render target to be used as input texture in post process pass
		RenderTargetBarrier barrier = { msaaRT, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrier);

		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = destRT->mClearValue;

		cmdBindRenderTargets(cmd, 1, &destRT, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdBindPipeline(cmd, pPipelineResolve);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetResolve);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
	}

	void resolveGodrayMSAA(Cmd* cmd, RenderTarget* msaaRT, RenderTarget* destRT)
	{
		// Set load actions to clear the screen to black
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = destRT->mClearValue;

		cmdBindRenderTargets(cmd, 1, &destRT, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)msaaRT->mWidth, (float)msaaRT->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, msaaRT->mWidth, msaaRT->mHeight);
		cmdBindPipeline(cmd, pPipelineGodrayResolve);
		cmdBindDescriptorSet(cmd, 1, pDescriptorSetResolve);
		cmdDraw(cmd, 3, 0);

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
	void triangleFilteringPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Triangle Filtering Pass");

		if (pRenderer->pActiveGpuSettings->mIndirectCommandBuffer)
		{
			CommandSignature resetCmd = {};
			resetCmd.mDrawType = INDIRECT_COMMAND_BUFFER_RESET;
			for (uint32_t g = 0; g < GEOMSET_COUNT; ++g)
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
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers");
		cmdBindPipeline(cmd, pPipelineClearBuffers);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetTriangleFiltering[0]);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 0, pDescriptorSetTriangleFiltering[1]);
		uint32_t numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers Synchronization");
		uint32_t numBarriers = (gNumViews * GEOMSET_COUNT) + gNumViews;
		BufferBarrier* clearBarriers = (BufferBarrier*)alloca(numBarriers * sizeof(BufferBarrier));
		uint32_t index = 0;
		for (uint32_t i = 0; i < gNumViews; ++i)
		{
			clearBarriers[index++] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_OPAQUE][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_ALPHATESTED][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
			clearBarriers[index++] = { pFilteredIndirectDrawArgumentsBuffer[frameIdx][GEOMSET_TRANSPARENT][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
		}
		cmdResourceBarrier(cmd, numBarriers, clearBarriers, 0, NULL, 0, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Run triangle filtering shader
		/************************************************************************/
		uint32_t currentSmallBatchChunk = 0;
		uint accumDrawCount = 0;
		uint accumNumTriangles = 0;
		uint accumNumTrianglesAtStartOfBatch = 0;
		uint batchStart = 0;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Filter Triangles");
		cmdBindPipeline(cmd, pPipelineTriangleFiltering);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 1, pDescriptorSetTriangleFiltering[1]);

		uint64_t size = BATCH_COUNT * sizeof(SmallBatchData) * gSmallBatchChunkCount;
		GPURingBufferOffset offset = getGPURingBufferOffset(pFilterBatchDataBuffer, (uint32_t)size, (uint32_t)size);
		BufferUpdateDesc updateDesc = { offset.pBuffer, offset.mOffset };
		beginUpdateResource(&updateDesc);

		FilterBatchData* batches = (FilterBatchData*)updateDesc.pMappedData;
		FilterBatchData* origin = batches;

		for (uint32_t i = 0; i < gMainSceneMeshCount; ++i)
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
				else
				{
					++gPerFrame[frameIdx].gCulledClusters;
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
		gPerFrame[frameIdx].gDrawCount[GEOMSET_TRANSPARENT] = accumDrawCount;
		gPerFrame[frameIdx].gTotalDrawCount = accumDrawCount * GEOMSET_COUNT;
		gPerFrame[frameIdx].gTotalDrawCount = accumDrawCount * 2;

		filterTriangles(cmd, frameIdx, pFilterBatchChunk[frameIdx][currentSmallBatchChunk],
			offset.pBuffer, (batches - origin) * sizeof(FilterBatchData));
		endUpdateResource(&updateDesc, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		// Synchronization
		/************************************************************************/
		for (uint32_t i = 0; i < gNumViews; ++i)
			uavBarriers[i] = { pUncompactedDrawArgumentsBuffer[frameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, gNumViews, uavBarriers, 0, NULL, 0, NULL);
		/************************************************************************/
		// Batch compaction
		/************************************************************************/
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Batch Compaction");
		cmdBindPipeline(cmd, pPipelineBatchCompaction);
		cmdBindDescriptorSet(cmd, frameIdx * gNumStages + 2, pDescriptorSetTriangleFiltering[1]);
		numGroups = (MAX_DRAWS_INDIRECT / CLEAR_THREAD_COUNT) + 1;
		cmdDispatch(cmd, numGroups, 1, 1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		/************************************************************************/
		/************************************************************************/

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	// This is the main scene rendering function. It shows the different steps / rendering passes.
	void drawScene(Cmd* cmd, uint32_t frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Shadow Pass");

		if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
		{
			RenderTargetBarrier rtBarriers[] = {
					{ pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
					{ pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);
		}
		else if (gAppSettings.mRenderMode == RENDERMODE_DEFERRED)
		{
			RenderTargetBarrier rtBarriers[] = {
				{ pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_NORMAL], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 1, rtBarriers);
		}

		drawShadowMapPass(cmd, gGraphicsProfileToken, frameIdx);
		cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

		if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
		{
			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Filling Pass");
			drawVisibilityBufferPass(cmd, gGraphicsProfileToken, frameIdx);
			RenderTargetBarrier barriers[] = {
				{ pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				{ pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				{ pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBufferOIT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 4, barriers);
			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "VB Shading Pass");

			drawVisibilityBufferShade(cmd, frameIdx);

			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
		}
		else if (gAppSettings.mRenderMode == RENDERMODE_DEFERRED)
		{
			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "GBuffer Pass");
			drawDeferredPass(cmd, gGraphicsProfileToken, frameIdx);
			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);

			RenderTargetBarrier barriers[] = {
				{ pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_NORMAL], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				{ pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBufferOIT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, DEFERRED_RT_COUNT + 3, barriers);

			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Shading Pass");

			drawDeferredShade(cmd, frameIdx);

			if (gAppSettings.mRenderLocalLights)
			{
				drawDeferredShadePointLights(cmd, frameIdx);
			}

			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
		}

		
		if (gAppSettings.mMsaaLevel > 1)
		{
			// Pixel Puzzle needs the unresolved MSAA texture
			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "MSAA Resolve Pass");
			resolveMSAA(cmd, pRenderTargetMSAA, pScreenRenderTarget);
			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
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

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pName = "skyboxImgBuff";

		SyncToken token = {};
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
		Cmd* cmd = ppCmds[0];

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

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.pSignalFence = pTransitionFences;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitForFences(pRenderer, 1, &pTransitionFences);

		removePipeline(pRenderer, pPanoToCubePipeline);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[1]);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);
	}

	void drawSkybox(Cmd* cmd, int frameIdx)
	{
		cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Draw Skybox");

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

		cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
	}

	void drawGodray(Cmd* cmd, uint frameIdx)
	{
		RenderTargetBarrier barrier[] = {
			{ pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
			{ pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
			{ pDepthBufferOIT, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
			{ pRenderTargetSun, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 4, barrier);

		cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "God ray");

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetSun->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTargetSun, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetSun->mWidth, (float)pRenderTargetSun->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetSun->mWidth, pRenderTargetSun->mHeight);

		cmdBindPipeline(cmd, pPipelineSunPass);
		cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetSunPass);
		cmdBindVertexBuffer(cmd, 1, &pSun->pVertexBuffers[0], pSun->mVertexStrides, NULL);
		cmdBindIndexBuffer(cmd, pSun->pIndexBuffer, pSun->mIndexType, 0);

		cmdDrawIndexed(cmd, pSun->mIndexCount, 0, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		if (gAppSettings.mMsaaLevel > 1)
		{
			RenderTargetBarrier barrier2[] = {
				{ pRenderTargetSun, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				{ pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBufferOIT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
				{ pRenderTargetSunResolved, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, sizeof(barrier2) / sizeof(barrier2[0]), barrier2);
		}
		else
		{
			RenderTargetBarrier barrier2[] = {
				{ pRenderTargetSun, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
				{ pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pDepthBufferOIT, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE },
				{ pRenderTargetGodRay[0], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
			};
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, sizeof(barrier2) / sizeof(barrier2[0]), barrier2);
		}

		if (gAppSettings.mMsaaLevel > 1)
		{
			// Pixel Puzzle needs the unresolved MSAA texture
			cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "GR Resolve Pass");
			resolveGodrayMSAA(cmd, pRenderTargetSun, pRenderTargetSunResolved);
			RenderTargetBarrier barrier33[] = { { pRenderTargetSunResolved, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier33);
			cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
		}

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetGodRay[0]->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTargetGodRay[0], NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetGodRay[0]->mWidth, (float)pRenderTargetGodRay[0]->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetGodRay[0]->mWidth, pRenderTargetGodRay[0]->mHeight);

		cmdBindPipeline(cmd, pPipelineGodRayPass);
		cmdBindPushConstants(cmd, pRootSigGodRayPass, gGodRayRootConstantIndex, &gPerFrame[frameIdx].gGodrayInfo);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetGodRayPass);
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier barrier3[] = { { pRenderTargetGodRay[0], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier3);

		for (uint loop = 0; loop < gAppSettings.gGodrayInteration - 1; loop++)
		{
			RenderTargetBarrier barrier2[] = { { pRenderTargetGodRay[!(loop & 0x1)], RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET } };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier2);

			//LoadActionsDesc loadActions = {};
			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = pRenderTargetGodRay[!(loop & 0x1)]->mClearValue;

			cmdBindRenderTargets(cmd, 1, &pRenderTargetGodRay[!(loop & 0x1)], NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(
				cmd, 0.0f, 0.0f, (float)pRenderTargetGodRay[!(loop & 0x1)]->mWidth, (float)pRenderTargetGodRay[!(loop & 0x1)]->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetGodRay[!(loop & 0x1)]->mWidth, pRenderTargetGodRay[!(loop & 0x1)]->mHeight);

#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)
			cmdBindPipeline(cmd, pPipelineGodRayPass);
#if defined(METAL)
			cmdBindPushConstants(cmd, pRootSigGodRayPass, gGodRayRootConstantIndex, &gPerFrame[frameIdx].gGodrayInfo);
#endif
#endif
			cmdBindDescriptorSet(cmd, 1 + (loop & 0x1), pDescriptorSetGodRayPass);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			RenderTargetBarrier barrier3[] = { { pRenderTargetGodRay[!(loop & 0x1)], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrier3);
		}

		cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
	}

	void drawColorconversion(Cmd* cmd)
	{
		cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Curve Conversion");

		// Transfer our render target to a render target state
		RenderTargetBarrier barrierCurveConversion[] = {
			{ pCurveConversionRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET }
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barrierCurveConversion);

		LoadActionsDesc loadAction = {};
		loadAction.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pCurveConversionRenderTarget, NULL, &loadAction, NULL, NULL, -1, -1);

		//CurveConversion
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pCurveConversionRenderTarget->mWidth, (float)pCurveConversionRenderTarget->mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pCurveConversionRenderTarget->mWidth, pCurveConversionRenderTarget->mHeight);

		cmdBindPipeline(cmd, pPipelineCurveConversionPass);
		cmdBindDescriptorSet(cmd, !(gAppSettings.gGodrayInteration & 0x1), pDescriptorSetCurveConversionPass);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		pScreenRenderTarget = pCurveConversionRenderTarget;

		cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
	}

	void presentImage(Cmd* const cmd, RenderTarget* pSrc, RenderTarget* pDstCol)
	{
		cmdBeginGpuTimestampQuery(cmd, gGraphicsProfileToken, "Present Image");

		RenderTargetBarrier barrier[] = {
			{ pSrc, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
			{ pDstCol, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET }
		};

		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barrier);

		cmdBindRenderTargets(cmd, 1, &pDstCol, NULL, NULL, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDstCol->mWidth, (float)pDstCol->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDstCol->mWidth, pDstCol->mHeight);

		cmdBindPipeline(cmd, pPipelinePresentPass);
		cmdBindPushConstants(cmd, pRootSigPresentPass, gSCurveRootConstantIndex, &gSCurveInfomation);
		cmdBindDescriptorSet(cmd, gAppSettings.mEnableGodray ? 1 : 0, pDescriptorSetPresentPass);
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier barrierPresent = { pDstCol, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &barrierPresent);

		cmdEndGpuTimestampQuery(cmd, gGraphicsProfileToken);
	}

	// Draw GUI / 2D elements
    void drawGUI(Cmd* cmd, uint32_t frameIdx)
    {
        UNREF_PARAM(frameIdx);

		LoadActionsDesc loadAction = {};
		loadAction.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
        cmdBindRenderTargets(cmd, 1, &pScreenRenderTarget, NULL, &loadAction, NULL, NULL, -1, -1);

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID; 
        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        if (gAppSettings.mAsyncCompute)
        {
            if (gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
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

        cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
    }
};

DEFINE_APPLICATION_MAIN(VisibilityBufferOIT)
