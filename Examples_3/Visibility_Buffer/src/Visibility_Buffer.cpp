/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

#include "../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../../../Common_3/Renderer/IRenderer.h"
#include "../../../Common_3/Renderer/GpuProfiler.h"
#include "../../../Common_3/OS/UI/UI.h"
#include "../../../Common_3/OS/UI/UIRenderer.h"
#include "../../../Common_3/OS/Core/RingBuffer.h"
#include "../../../Common_3/OS/Image/Image.h"
#include "../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../Common_3/OS/Interfaces/IThread.h"
#include "../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../Common_3/OS/Interfaces/IUIManager.h"
#include "../../../Common_3/OS/Interfaces/IApp.h"
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

#define SANMIGUEL
#if !defined(METAL)
#define MSAASAMPLECOUNT 2
#else
#define MSAASAMPLECOUNT 1
#endif

FileSystem mFileSystem;
LogManager mLogManager;
HiresTimer gTimer;

#if defined(DIRECT3D12)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#else
#error PLATFORM NOT SUPPORTED
#endif

#define SCENE_SCALE 1.0f

// Define the root folders for dynamically loaded assets on every platform
const char* pszRoots[FSR_Count] =
{
#if defined(_DURANGO)
	"Shaders/Binary/",													// FSR_BinShaders
	"Shaders/",															// FSR_SrcShaders
	"Shaders/Binary/",													// FSR_BinShaders_Common
	"Shaders/",															// FSR_SrcShaders_Common
#if defined(SPONZA)
	"Meshes/Sponza/Textures/Compressed/",								// FSR_Textures
	"Meshes/Sponza/",													// FSR_Meshes
#elif defined(SANMIGUEL)
	"Meshes/SanMiguel/Textures/Compressed/",							// FSR_Textures
	"Meshes/SanMiguel/",												// FSR_Meshes
#endif

	"Fonts/",															// FSR_Builtin_Fonts
#elif !defined(TARGET_IOS)
	"../../../src/" RESOURCE_DIR  "/Binary/",							// FSR_BinShaders
	"../../../src/" RESOURCE_DIR "/",									// FSR_SrcShaders
	"../../../src/" RESOURCE_DIR "/Binary/",							// FSR_BinShaders_Common (Currently just in same folder as other shaders)
	"../../../src/" RESOURCE_DIR "/",									// FSR_SrcShaders_Common

#if OLD_MODELS

#if defined(SPONZA)
	"../../../../../../Examples_2/Aura/DX11Assets/Textures/",			// FSR_Textures
	"../../../../../../Examples_2/Aura/DX11Assets/Meshes/SponzaNew/",	// FSR_Meshes
#elif defined(SANMIGUEL)
	"../../../../../../Art/SanMiguel_2/",								// FSR_Textures
	"../../../../../../Art/SanMiguel_2/",								// FSR_Meshes
#endif

#else

#if defined(SPONZA)
	"../../../../../../Art/Sponza_Deprecated/Textures/Compressed/",		// FSR_Textures
	"../../../../../../Art/Sponza_Deprecated/",							// FSR_Meshes
#elif defined(SANMIGUEL)
	"../../../../../../Art/SanMiguel_3/",								// FSR_Textures
	"../../../../../../Art/SanMiguel_3/",								// FSR_Meshes
#endif

#endif
	"../../../Resources/Fonts/",										// Fonts
#else   // !defined(TARGET_IOS)
	"",																	// FSR_BinShaders
	"",																	// FSR_SrcShaders
	"",																	// FSR_BinShaders_Common (Currently just in same folder as other shaders)
	"",																	// FSR_SrcShaders_Common
	"",																	// FSR_Textures
	"",																	// FSR_Meshes
	"",																	// Fonts
#endif  // !defined(TARGET_IOS)
#ifdef _DURANGO
	"",
	"Shaders/",															// FSR_Lib0_SrcShaders
#else
	"../../../Resources/",												// FSR_OtherFiles
#endif
};

#ifdef SPONZA
#if OLD_MODELS
const char*    gSceneName = "sponza.cmesh";
#else
const char*    gSceneName = "Sponza.obj";
#endif
#else
#if OLD_MODELS
const char*		gSceneName = "SanMiguel.cmesh";
#else
const char*		gSceneName = "SanMiguel.FBX";
#endif
#endif

// Number of in-flight buffers
const uint32_t gImageCount = 3;

// Constants
const uint32_t gShadowMapSize = 1024;
const uint32_t gNumViews = NUM_CULLING_VIEWPORTS;

// Define different geometry sets (opaque and alpha tested geometry)
const uint32_t gNumGeomSets = 2;
const uint32_t GEOMSET_OPAQUE = 0;
const uint32_t GEOMSET_ALPHATESTED = 1;

// Memory budget used for internal ring buffer
const uint32_t gMemoryBudget = 1024ULL * 1024ULL * 64ULL;

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

typedef struct VisBufferIndirectCommand
{
	// Metal does not use index buffer since there is no builtin primitive id
#if defined(METAL)
	IndirectDrawArguments arg;
#else
	// Draw ID is sent as indirect argument through root constant in DX12
#if defined(DIRECT3D12)
	uint32_t drawId;
#endif
	IndirectDrawIndexArguments arg;
#if defined(DIRECT3D12)
	uint32_t _pad0, _pad1;
#else
	uint32_t _pad0, _pad1, _pad2;
#endif
#endif
} VisBufferIndirectCommand;

#if defined(METAL)
// Constant data updated for every batch
struct PerBatchConstants
{
	uint32_t drawId;  // used to idendify the batch from the shader
	uint32_t twoSided;  // possible values: 0/1
};
#endif

// Panini projection settings
// source:	http://tksharpless.net/vedutismo/Pannini/
// paper:   http://tksharpless.net/vedutismo/Pannini/panini.pdf
struct PaniniParameters
{
	// horizontal field of view in degrees
	float FoVH = 90.0f;

	// D parameter: Distance of projection's center from the Panini frame's origin.
	//				i.e. controls horizontal compression.
	//				D = 0.0f    : regular rectilinear projection
	//				D = 1.0f    : Panini projection
	//				D->Infinity : cylindrical orthographic projection
	float D = 1.0f;

	// S parameter: A scalar that controls 'Hard Vertical Compression' of the projection.
	//				Panini projection produces curved horizontal lines, which can feel
	//				unnatural. Vertical compression attempts to straighten those curved lines.
	//				S parameter works for FoVH < 180 degrees
	//				S = 0.0f	: No compression
	//				S = 1.0f	: Full straightening
	float S = 0.0f;

	float scale = 1.0f;
};

// Panini projection renders into a tessellated rectangle
const unsigned gPaniniDistortionTessellation[2] = { 64, 32 };

// GUI CONTROLS
//---------------------------------------------------------------------------------------------------------------------------------
typedef struct DynamicUIControls
{
	tinystl::vector<UIProperty> mDynamicProperties;
	tinystl::vector<uint32_t>   mDynamicPropHandles;

	void ShowDynamicProperties(Gui* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); ++i)
		{
			mDynamicPropHandles.push_back(0);
			addProperty(pGui, &mDynamicProperties[i], &mDynamicPropHandles[i]);
		}
	}

	void HideDynamicProperties(Gui* pGui)
	{
		for (int i = 0; i < mDynamicProperties.size(); i++)
		{
			removeProperty(pGui, mDynamicPropHandles[i]);
		}
		mDynamicPropHandles.clear();
	}

} DynamicUIControls;

typedef struct AppSettings
{
	// Current active rendering mode
    RenderMode mRenderMode = RENDERMODE_VISBUFF;

	// Set this variable to true to bypass triangle filtering calculations, holding and representing the last filtered data.
	// This is useful for inspecting filtered geometry for debugging purposes.
	bool mHoldFilteredResults = false;

	// This variable enables or disables triangle filtering. When filtering is disabled, all the scene is rendered unconditionally.
	bool mFilterTriangles = true;
	bool mClusterCulling = true;

	bool mAsyncCompute = true;

	// toggle rendering of local point lights
	bool mRenderLocalLights = false;

#if MSAASAMPLECOUNT == 1
	bool mDrawDebugTargets = true;
#else
	bool mDrawDebugTargets = false;
#endif

	// adjust directional sunlight angle
	float mSunControl = 1.879000f;
	float mEsmControl = 100.0f;

	float mRetinaScaling = 1.5f;

	// toggle Panini projection
	bool mEnablePaniniProjection = false;
	PaniniParameters mPaniniSettings;
	DynamicUIControls mDynamicUIControlsPanini;

	// HDAO data
	DynamicUIControls mDynamicUIControlsAO;
	bool mEnableHDAO = true;

	float mRejectRadius = 5.2f;
	float mAcceptRadius = 0.12f;
	float mAOIntensity = 3.0f;
	int   mAOQuality = 2;
} AppSettings;

AppSettings gAppSettings;
//---------------------------------------------------------------------------------------------------------------------------------

// Global variables
Renderer*	pRenderer = nullptr;

Fence*			pRenderCompleteFences[gImageCount] = { nullptr };
Fence*			pComputeCompleteFences[gImageCount] = { nullptr };
Semaphore*		pImageAcquiredSemaphore = nullptr;
Semaphore*		pRenderCompleteSemaphores[gImageCount] = { nullptr };
Semaphore*		pComputeCompleteSemaphores[gImageCount] = { nullptr };

Queue*			pGraphicsQueue = nullptr;
CmdPool*		pCmdPool = nullptr;
Cmd**			ppCmds = nullptr;

Queue*			pComputeQueue = nullptr;
CmdPool*		pComputeCmdPool = nullptr;
Cmd**			ppComputeCmds = nullptr;

SwapChain*		pSwapChain = nullptr;

Shader*			pShaderClearBuffers = nullptr;
Shader*			pShaderTriangleFiltering = nullptr;
Shader*			pShaderShadowPass[gNumGeomSets] = { nullptr };
Shader*			pShaderVisibilityBufferPass[gNumGeomSets] = {};
Shader*			pShaderDeferredPass[gNumGeomSets] = {};
Shader*			pShaderVisibilityBufferShade[2] = { nullptr };
Shader*			pShaderDeferredShade[2] = { nullptr };
Shader*			pShaderDeferredShadePointLight = nullptr;
Shader*			pShaderClusterLights = nullptr;
Shader*			pShaderClearLightClusters = nullptr;
Shader*			pShaderPanini = nullptr;
Shader*			pShaderAO[4] = { nullptr };
Shader*			pShaderResolve = nullptr;
Pipeline*		pPipelineClearBuffers = nullptr;
Pipeline*		pPipelineTriangleFiltering = nullptr;
Pipeline*		pPipelineShadowPass[gNumGeomSets] = { nullptr };
Pipeline*		pPipelineVisibilityBufferPass[gNumGeomSets] = {};
Pipeline*		pPipelineDeferredPass[gNumGeomSets] = {};
Pipeline*		pPipelineVisibilityBufferShadePost[2] = { nullptr };
Pipeline*		pPipelineVisibilityBufferShadeSrgb[2] = { nullptr };
Pipeline*		pPipelineDeferredShadePost[2] = { nullptr };
Pipeline*		pPipelineDeferredShadeSrgb[2] = { nullptr };
Pipeline*		pPipelineDeferredShadePointLightPost = nullptr;
Pipeline*		pPipelineDeferredShadePointLightSrgb = nullptr;
Pipeline*		pPipelineClusterLights = nullptr;
Pipeline*		pPipelineClearLightClusters = nullptr;
Pipeline*		pPipelineAO[4] = { nullptr };
Pipeline*		pPipelineResolve = nullptr;
Pipeline*		pPipelineResolvePost = nullptr;
RenderTarget*	pDepthBuffer = nullptr;
RenderTarget*	pWorldRenderTarget = nullptr;
RenderTarget*	pRenderTargetVBPass = nullptr;
RenderTarget*	pRenderTargetMSAA = nullptr;
RenderTarget*	pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { nullptr };
RenderTarget*	pRenderTargetShadow = nullptr;
RenderTarget*	pRenderTargetAO = nullptr;

RasterizerState* pRasterizerStateCullBack = nullptr;
RasterizerState* pRasterizerStateCullFront = nullptr;
RasterizerState* pRasterizerStateCullNone = nullptr;
RasterizerState* pRasterizerStateCullBackMS = nullptr;
RasterizerState* pRasterizerStateCullFrontMS = nullptr;
RasterizerState* pRasterizerStateCullNoneMS = nullptr;
DepthState*		pDepthStateEnable = nullptr;
DepthState*		pDepthStateDisable = nullptr;
BlendState*		pBlendStateOneZero = nullptr;

Sampler*		pSamplerTrilinearAniso = nullptr;
Sampler*		pSamplerPointClamp = nullptr;
Sampler*		pSamplerPointWrap = nullptr;

#ifndef METAL
Shader*         pShaderBatchCompaction = nullptr;
Pipeline*       pPipelineBatchCompaction = nullptr;
#endif

Pipeline*       pPipielinePaniniPostProcess = nullptr;

tinystl::vector<Texture*> gDiffuseMaps;
tinystl::vector<Texture*> gNormalMaps;
tinystl::vector<Texture*> gSpecularMaps;

Buffer*			pVertexBufferPosition = nullptr;
Buffer*			pVertexBufferAttr = nullptr;
Buffer*			pMaterialPropertyBuffer = nullptr;
Buffer*			pPerFrameUniformBuffers[gImageCount] = { nullptr };

#if defined(METAL)
// Buffer containing all indirect draw commands for all geometry sets (no culling)
Buffer*         pIndirectDrawArgumentsBufferAll = nullptr;
Buffer*         pIndirectMaterialBufferAll = nullptr;
// Buffer containing filtered indirect draw commands for all geometry sets (culled)
Buffer*         pFilteredIndirectDrawArgumentsBuffer[gImageCount][gNumViews] = {};
#else
// Buffers containing all indirect draw commands per geometry set (no culling)
Buffer*			pIndirectDrawArgumentsBufferAll[gNumGeomSets] = { nullptr };
Buffer*			pIndirectMaterialBufferAll = nullptr;
Buffer*			pMeshConstantsBuffer = nullptr;
// Buffers containing filtered indirect draw commands per geometry set (culled)
Buffer*            pFilteredIndirectDrawArgumentsBuffer[gImageCount][gNumGeomSets][gNumViews] = { nullptr };
// Buffer containing the draw args after triangle culling which will be stored compactly in the indirect buffer
Buffer*         pUncompactedDrawArgumentsBuffer[gImageCount][gNumViews] = { nullptr };
Buffer*			pFilterIndirectMaterialBuffer[gImageCount] = { nullptr };
#endif

Buffer*			pIndexBufferAll = nullptr;
Buffer*			pFilteredIndexBuffer[gImageCount][gNumViews] = {};
Buffer*			pLightsBuffer = nullptr;
Buffer**		gPerBatchUniformBuffers = nullptr;
Buffer*			pVertexBufferCube = nullptr;
Buffer*			pIndexBufferCube = nullptr;
Buffer*			pLightClustersCount[gImageCount] = { nullptr };
Buffer*			pLightClusters[gImageCount] = { nullptr };
Buffer*			pVertexBufferTessellatedQuad = nullptr;
Buffer*			pIndexBufferTessellatedQuad = nullptr;
uint64_t		gFrameCount = 0;
Scene*			pScene = nullptr;
UIManager*		pUIManager = nullptr;
Gui*			pGuiWindow = nullptr;

#if defined(METAL)
FilterBatchChunk*   pFilterBatchChunk[gImageCount] = { nullptr };
#else
const uint32_t gSmallBatchChunkCount = 16;
FilterBatchChunk*   pFilterBatchChunk[gImageCount][gSmallBatchChunkCount] = { nullptr };
#endif
ICameraController*  pCameraController = nullptr;

CommandSignature*	pCmdSignatureDeferredPass = nullptr;
CommandSignature*	pCmdSignatureShadowPass = nullptr;
CommandSignature*	pCmdSignatureVBPass = nullptr;

RootSignature*		pRootSignatureShadowPass = nullptr;
RootSignature*		pRootSignatureVBPass = nullptr;
RootSignature*		pRootSignatureDeferredPass = nullptr;
RootSignature*		pRootSignatureVBShade = nullptr;
RootSignature*		pRootSignatureDeferredShade = nullptr;
RootSignature*		pRootSignatureDeferredShadePointLight = nullptr;
RootSignature*		pRootSignatureTriangleFiltering = nullptr;
RootSignature*		pRootSignatureBatchCompaction = nullptr;
RootSignature*		pRootSignatureClearBuffers = nullptr;
RootSignature*		pRootSignatureClearLightClusters = nullptr;
RootSignature*		pRootSignatureClusterLights = nullptr;
RootSignature*		pRootSignatureRenderTexture = nullptr;
RootSignature*		pRootSignaturePaniniPostProcess = nullptr;
RootSignature*		pRootSignatureAO = nullptr;
RootSignature*		pRootSignatureResolve = nullptr;

GpuProfiler* pGraphicsGpuProfiler =  nullptr;
GpuProfiler* pComputeGpuProfiler  =  nullptr;

// CPU buffers for light data
LightData gLightData[LIGHT_COUNT] = {};

struct PerFrameData
{
	// Stores the camera/eye position in object space for cluster culling
	vec3 gEyeObjectSpace[gNumViews] = {};
	PerFrameConstants gPerFrameUniformData = {};

	// These are just used for statistical information
	uint32_t gTotalClusters = 0;
	uint32_t gCulledClusters = 0;
	uint32_t gDrawCount[gNumGeomSets];
};

PerFrameData  gPerFrame[gImageCount] = {};
RenderTarget* pScreenRenderTarget = nullptr;

WindowsDesc	gWindow = {};
String		gAppName;

#ifdef METAL
uint32_t gSwapchainWidth;
uint32_t gSwapchainHeight;
#endif

void applyMacros(ShaderDesc &description, ShaderMacro &macros)
{
	description.mVert.mMacros.add(macros);
	description.mHull.mMacros.add(macros);
	description.mGeom.mMacros.add(macros);
	description.mFrag.mMacros.add(macros);
	description.mDomain.mMacros.add(macros);
	description.mComp.mMacros.add(macros);
}
// Load all the shaders needed for the demo
void loadShaders()
{
	ShaderDesc shadowPass = { SHADER_STAGE_VERT };
	ShaderDesc shadowPassAlpha = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc vbPass = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc vbPassAlpha = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc vbShade[2] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc deferredPass = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc deferredPassAlpha = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc deferredShade[2] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc deferredPointlights = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc clearBuffer = { SHADER_STAGE_COMP };
	ShaderDesc triangleCulling = { SHADER_STAGE_COMP };
	ShaderDesc batchCompaction = { SHADER_STAGE_COMP };
	ShaderDesc clearLights = { SHADER_STAGE_COMP };
	ShaderDesc clusterLights = { SHADER_STAGE_COMP };
	ShaderDesc paniniPass = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc ao[4] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };
	ShaderDesc resolvePass = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG };

	tinystl::string extraValue = "";
	extraValue.sprintf("%i", MSAASAMPLECOUNT);
	ShaderMacro extra = { "SAMPLE_COUNT" , extraValue };

#if defined(METAL)
	File metalFile = {};
    
    metalFile.Open("shader_defs.h", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    String shaderDefs = "#define MTL_SHADER 1\n" + metalFile.ReadText(); // Add a preprocessor define to differentiate between the structs used by shaders and app.
	metalFile.Open("shading.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	String shading = shaderDefs + metalFile.ReadText();

	// Shadow pass shader
	metalFile.Open("shadow_pass.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    String metal = shaderDefs + metalFile.ReadText();
	shadowPass.mVert = { metalFile.GetName(), metal, "VSMain" };
	shadowPassAlpha = { shadowPassAlpha.mStages, shadowPass.mVert,{ metalFile.GetName(), metal, "PSMain" } };
    applyMacros(shadowPassAlpha, extra);

	// Visibility buffer geometry pass shaders
	metalFile.Open("visibilityBuffer_pass.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	metal = shaderDefs + metalFile.ReadText();
	vbPass = { vbPass.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMainOpaque" } };
	vbPassAlpha = { vbPass.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMainAlphaTested" } };
    applyMacros(vbPassAlpha, extra);

	// Visibility buffer shading pass shader
    tinystl::vector<ShaderMacro> aoMacro(1);
    metalFile.Open("visibilityBuffer_shade.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	metal = shading + metalFile.ReadText();
    aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "0" };
    vbShade[0] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        { metalFile.GetName(), metal, "VSMain", aoMacro },{ metalFile.GetName(), metal, "PSMain", aoMacro } };
    aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "1" };
    vbShade[1] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        { metalFile.GetName(), metal, "VSMain", aoMacro },{ metalFile.GetName(), metal, "PSMain", aoMacro } };
    applyMacros(vbShade[0], extra);
    applyMacros(vbShade[1], extra);

	// Deferred geometry pass shaders
	metalFile.Open("deferred_pass.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	metal = shaderDefs + metalFile.ReadText();
	deferredPass = { deferredPass.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMainOpaque" } };
	deferredPassAlpha = { deferredPassAlpha.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMainAlphaTested" } };
    applyMacros(deferredPass, extra);
    applyMacros(deferredPassAlpha, extra);

	// Deferred shading pass shader
	metalFile.Open("deferred_shade.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	metal = shading + metalFile.ReadText();
    aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "0" };
    deferredShade[0] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        { metalFile.GetName(), metal, "VSMain", aoMacro },{ metalFile.GetName(), metal, "PSMain", aoMacro } };
    aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "1" };
    deferredShade[1] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
        { metalFile.GetName(), metal, "VSMain", aoMacro },{ metalFile.GetName(), metal, "PSMain", aoMacro } };
    applyMacros(deferredShade[0], extra);
    applyMacros(deferredShade[1], extra);

	// Deferred shading pass shader for point light accumulation
	metalFile.Open("deferred_shade_pointlight.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	metal = shading + metalFile.ReadText();
	deferredPointlights = { deferredPointlights.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMain" } };
    applyMacros(deferredPointlights, extra);

	// Triangle culling compute shader
	metalFile.Open("triangle_filtering.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    metal = shaderDefs + metalFile.ReadText();
	triangleCulling.mComp = { metalFile.GetName(), metal, "CSMain" };
    applyMacros(triangleCulling, extra);

	// Clear buffers compute shader
	metalFile.Open("clear_buffers.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    metal = shaderDefs + metalFile.ReadText();
	clearBuffer.mComp = { metalFile.GetName(), metal, "CSMain" };
    applyMacros(clearBuffer, extra);

	// Clear light clusters compute shader
	metalFile.Open("clear_light_clusters.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	clearLights.mComp = { metalFile.GetName(), shading + metalFile.ReadText(), "CSMain" };
    applyMacros(clearLights, extra);

	// Cluster lights compute shader
	metalFile.Open("cluster_lights.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
	clusterLights.mComp = { metalFile.GetName(), shading + metalFile.ReadText(), "CSMain" };
    applyMacros(clusterLights, extra);

    // Panini Projection post-process shader
    metalFile.Open("panini_projection.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    metal = shading + metalFile.ReadText();
    paniniPass = { paniniPass.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMain" } };
    applyMacros(paniniPass, extra);
    
    // Resolve shader
    metalFile.Open("resolve.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    metal = metalFile.ReadText();
    resolvePass = { resolvePass.mStages,{ metalFile.GetName(), metal, "VSMain" },{ metalFile.GetName(), metal, "PSMain" } };
    applyMacros(resolvePass, extra);

    // HDAO post-process shader
    metalFile.Open("HDAO.metal", FileMode::FM_Read, FSRoot::FSR_SrcShaders);
    metal = metalFile.ReadText();
    for (uint32_t i = 0; i < 4; ++i)
    {
        tinystl::vector<ShaderMacro> qualityMacro(1);
        String val; val.appendInt(i + 1);
        qualityMacro[0] = { "AO_QUALITY", val };
        ao[i] = {
            SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
            { metalFile.GetName(), metal, "VSMain", qualityMacro },
            { metalFile.GetName(), metal, "PSMain", qualityMacro }
        };
        applyMacros(ao[i], extra);
    }

	metalFile.Close();
#elif defined(DIRECT3D12)
	File hlslFile = {};

	// Shadow pass shader
	hlslFile.Open("shadow_pass.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	String hlsl = hlslFile.ReadText();
	shadowPass.mVert = { hlslFile.GetName(), hlsl, "VSMain" };
	shadowPass.mFrag = { hlslFile.GetName(), hlsl, "PSMain" };
	shadowPass.mFrag.mMacros.push_back({ "CONF_EARLY_DEPTH_STENCIL", "[earlydepthstencil]" }); //only use this for opaque
	shadowPassAlpha = { shadowPassAlpha.mStages, shadowPass.mVert,{ hlslFile.GetName(), hlsl, "PSMainAlphaTested" } };
	applyMacros(shadowPassAlpha, extra);

	// Visibility buffer geometry pass shaders
	hlslFile.Open("visibilityBuffer_pass.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	vbPass = { vbPass.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMainOpaque" } };
	vbPass.mFrag.mMacros.push_back({"CONF_EARLY_DEPTH_STENCIL", "[earlydepthstencil]"}); //only use this for opaque
	vbPassAlpha = { vbPass.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMainAlphaTested" } };
	applyMacros(vbPassAlpha, extra);

	// Visibility buffer shading pass shader
	tinystl::vector<ShaderMacro> aoMacro(1);
	hlslFile.Open("visibilityBuffer_shade.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "0" };
	vbShade[0] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ hlslFile.GetName(), hlsl, "VSMain", aoMacro },{ hlslFile.GetName(), hlsl, "PSMain", aoMacro } };
	aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "1" };
	vbShade[1] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ hlslFile.GetName(), hlsl, "VSMain", aoMacro },{ hlslFile.GetName(), hlsl, "PSMain", aoMacro } };
	applyMacros(vbShade[0], extra);
	applyMacros(vbShade[1], extra);

	// Deferred geometry pass shaders
	hlslFile.Open("deferred_pass.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	deferredPass = { deferredPass.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMainOpaque" } };
	deferredPassAlpha = { deferredPassAlpha.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMainAlphaTested" } };
	applyMacros(deferredPassAlpha, extra);

	// Deferred shading pass shader
	hlslFile.Open("deferred_shade.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "0" };
	deferredShade[0] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ hlslFile.GetName(), hlsl, "VSMain", aoMacro },{ hlslFile.GetName(), hlsl, "PSMain", aoMacro } };
	aoMacro[0] = { "USE_AMBIENT_OCCLUSION", "1" };
	deferredShade[1] = { SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
	{ hlslFile.GetName(), hlsl, "VSMain", aoMacro },{ hlslFile.GetName(), hlsl, "PSMain", aoMacro } };
	applyMacros(deferredShade[0], extra);
	applyMacros(deferredShade[1], extra);

	// Deferred shading pass shader for point light accumulation
	hlslFile.Open("deferred_shade_pointlight.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	deferredPointlights = { deferredPointlights.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMain" } };
	applyMacros(deferredPointlights, extra);

	// Triangle culling compute shader
	hlslFile.Open("triangle_filtering.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	triangleCulling.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };
	applyMacros(triangleCulling, extra);

	// Batch compaction compute shader
	hlslFile.Open("batch_compaction.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	batchCompaction.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };
	applyMacros(batchCompaction, extra);

	// Clear buffers compute shader
	hlslFile.Open("clear_buffers.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	clearBuffer.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };
	applyMacros(clearBuffer, extra);

	// Clear light clusters compute shader
	hlslFile.Open("clear_light_clusters.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	clearLights.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };
	applyMacros(clearLights, extra);

	// Cluster lights compute shader
	hlslFile.Open("cluster_lights.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	clusterLights.mComp = { hlslFile.GetName(), hlslFile.ReadText(), "CSMain" };
	applyMacros(clusterLights, extra);

	// Panini Projection post-process shader
	hlslFile.Open("panini_projection.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	paniniPass = { paniniPass.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMain" } };
	applyMacros(paniniPass, extra);

	// Resolve shader
	hlslFile.Open("resolve.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	resolvePass = { resolvePass.mStages,{ hlslFile.GetName(), hlsl, "VSMain" },{ hlslFile.GetName(), hlsl, "PSMain" } };
	applyMacros(resolvePass, extra);

	// HDAO post-process shader	
	hlslFile.Open("HDAO.hlsl", FileMode::FM_ReadBinary, FSRoot::FSR_SrcShaders);
	hlsl = hlslFile.ReadText();
	for (uint32_t i = 0; i < 4; ++i)
	{
		tinystl::vector<ShaderMacro> qualityMacro(1);
		String val; val.appendInt(i + 1);
		qualityMacro[0] = { "AO_QUALITY", val };
		ao[i] = {
			SHADER_STAGE_VERT | SHADER_STAGE_FRAG,
			{ hlslFile.GetName(), hlsl, "VSMain", qualityMacro },
			{ hlslFile.GetName(), hlsl, "PSMain", qualityMacro }
		};
		applyMacros(ao[i], extra);
	}

	hlslFile.Close();
#elif defined(VULKAN)
	File vertFile = {};
	File fragFile = {};
	File computeFile = {};

	// Shadow pass shader
	vertFile.Open("shadow_pass.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	fragFile.Open("shadow_pass_alpha.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	shadowPass.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	shadowPassAlpha = { shadowPassAlpha.mStages, shadowPass.mVert,{ fragFile.GetName(), fragFile.ReadText(), "main" } };

	// Visibility buffer geometry pass shaders
	vertFile.Open("visibilityBuffer_pass.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	fragFile.Open("visibilityBuffer_pass.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	vbPass.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	vbPass.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };
	fragFile.Open("visibilityBuffer_pass_alpha.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	vbPassAlpha.mVert = { vertFile.GetName(), vbPass.mVert.mCode, "main" };
	vbPassAlpha.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	// Deferred geometry pass shaders
	vertFile.Open("deferred_pass.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	fragFile.Open("deferred_pass.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	deferredPass.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	deferredPass.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	fragFile.Open("deferred_pass_alpha.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	deferredPassAlpha.mVert = { vertFile.GetName(), deferredPass.mVert.mCode, "main" };
	deferredPassAlpha.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Open("visibilityBuffer_shade.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "visibilityBuffer_shade" + val + "x_no_ao.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	//fragFile.Open("visibilityBuffer_shade_no_ao.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	vbShade[0].mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	vbShade[0].mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	vbShade[0].mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "visibilityBuffer_shade" + val + "x_ao.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	//fragFile.Open("visibilityBuffer_shade_ao.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	vbShade[1].mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	vbShade[1].mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	vbShade[1].mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	// Deferred geometry pass shaders
	vertFile.Open("deferred_shade.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "deferred_shade" + val + "x_no_ao.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	//fragFile.Open("deferred_shade_no_ao.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	deferredShade[0].mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	deferredShade[0].mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	deferredShade[0].mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "deferred_shade" + val + "x_ao.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	//fragFile.Open("deferred_shade_ao.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	deferredShade[1].mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	deferredShade[1].mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	deferredShade[1].mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	vertFile.Open("deferred_shade_pointlight.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "deferred_shade" + val + "x_pointlight.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	//fragFile.Open("deferred_shade_pointlight.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	deferredPointlights.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	deferredPointlights.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	// Triangle culling compute shader
	computeFile.Open("triangle_filtering.comp.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	triangleCulling.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	// Triangle culling compute shader
	computeFile.Open("batch_compaction.comp.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	batchCompaction.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	// Clear buffers compute shader
	computeFile.Open("clear_buffers.comp.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	clearBuffer.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	// Clear light clusters compute shader
	computeFile.Open("clear_light_clusters.comp.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	clearLights.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	// Cluster lights compute shader
	computeFile.Open("cluster_lights.comp.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	clusterLights.mComp = { computeFile.GetName(), computeFile.ReadText(), "main" };

	// Panini Projection post-process shader
	vertFile.Open("panini_projection.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	fragFile.Open("panini_projection.frag.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	paniniPass.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	paniniPass.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

	// Resolve shader
	vertFile.Open("resolve.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	{
		String val; val.appendInt(MSAASAMPLECOUNT);
		String filename = "resolve" + val + "x.frag.spv";
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	}
	resolvePass.mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
	resolvePass.mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
	resolvePass.mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };

    // HDAO post-process shader
	vertFile.Open("HDAO.vert.spv", FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
	for (uint32_t i = 0; i < 4; ++i)
	{
		String val; val.appendInt(i + 1);
		String val2; val2.appendInt(MSAASAMPLECOUNT);
		String filename = "HDAO" + val2 + "x_" + val + ".frag.spv";
		// Panini Projection post-process shader
		fragFile.Open(filename, FileMode::FM_ReadBinary, FSRoot::FSR_BinShaders);
		ao[i].mStages = SHADER_STAGE_VERT | SHADER_STAGE_FRAG;
		ao[i].mVert = { vertFile.GetName(), vertFile.ReadText(), "main" };
		ao[i].mFrag = { fragFile.GetName(), fragFile.ReadText(), "main" };
	}

	vertFile.Close();
	fragFile.Close();
	computeFile.Close();
#endif

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
	addShader(pRenderer, &paniniPass, &pShaderPanini);
	for (uint32_t i = 0; i < 4; ++i)
		addShader(pRenderer, &ao[i], &pShaderAO[i]);
	addShader(pRenderer, &resolvePass, &pShaderResolve);

#ifndef METAL
	addShader(pRenderer, &batchCompaction, &pShaderBatchCompaction);
#endif
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
	materialPropDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV;
	materialPropDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	materialPropDesc.mDesc.mElementCount = pScene->numMaterials;
	materialPropDesc.mDesc.mStructStride = sizeof(uint32_t);
	materialPropDesc.mDesc.mSize = materialPropDesc.mDesc.mElementCount * materialPropDesc.mDesc.mStructStride;
	materialPropDesc.pData = alphaTestMaterials;
	materialPropDesc.ppBuffer = &pMaterialPropertyBuffer;
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
        materialIDPerDrawCall[i] = pScene->meshes[i].materialId;;
	}

	// Setup uniform data for draw batch data.
	BufferLoadDesc indirectBufferDesc = {};
	indirectBufferDesc.mDesc.mUsage = BUFFER_USAGE_INDIRECT | BUFFER_USAGE_STORAGE_SRV;
	indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectBufferDesc.mDesc.mStructStride = sizeof(VisBufferIndirectCommand);
	indirectBufferDesc.mDesc.mFirstElement = 0;
	indirectBufferDesc.mDesc.mElementCount = pScene->numMeshes;
	indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
	indirectBufferDesc.pData = indirectDrawArgumentsMax;
	indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll;
	addResource(&indirectBufferDesc);
    
    // Setup indirect material buffer.
    BufferLoadDesc indirectDesc = {};
    indirectDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV;
    indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    indirectDesc.mDesc.mElementCount = pScene->numMeshes;
    indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
    indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
    indirectDesc.pData = materialIDPerDrawCall.data();
    indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
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
	*(((UINT*)indirectArgsAlpha.getArray()) + DRAW_COUNTER_SLOT_POS) = iAlpha;
	*(((UINT*)indirectArgsNoAlpha.getArray()) + DRAW_COUNTER_SLOT_POS) = iNoAlpha;

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
		indirectBufferDesc.mDesc.mUsage = BUFFER_USAGE_INDIRECT | BUFFER_USAGE_STORAGE_SRV;
		indirectBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indirectBufferDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(VisBufferIndirectCommand) / sizeof(uint32_t));
		indirectBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
		indirectBufferDesc.mDesc.mSize = indirectBufferDesc.mDesc.mElementCount * indirectBufferDesc.mDesc.mStructStride;
		indirectBufferDesc.pData = i == 0 ? indirectArgsNoAlpha.data() : indirectArgsAlpha.data();
		indirectBufferDesc.ppBuffer = &pIndirectDrawArgumentsBufferAll[i];
		addResource(&indirectBufferDesc);
	}

	BufferLoadDesc indirectDesc = {};
	indirectDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV;
	indirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	indirectDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
	indirectDesc.mDesc.mStructStride = sizeof(uint32_t);
	indirectDesc.mDesc.mSize = indirectDesc.mDesc.mElementCount * indirectDesc.mDesc.mStructStride;
	indirectDesc.pData = materialIDPerDrawCall.data();
	indirectDesc.ppBuffer = &pIndirectMaterialBufferAll;
	addResource(&indirectDesc);
#endif
	/************************************************************************/
	// Indirect buffers for culling
	/************************************************************************/
	BufferLoadDesc filterIbDesc = {};
	filterIbDesc.mDesc.mUsage = BUFFER_USAGE_INDEX | BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	filterIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	filterIbDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
	filterIbDesc.mDesc.mElementCount = pScene->totalTriangles;
	filterIbDesc.mDesc.mStructStride = sizeof(uint32_t);
	filterIbDesc.mDesc.mSize = filterIbDesc.mDesc.mElementCount * filterIbDesc.mDesc.mStructStride;
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
	VisBufferIndirectCommand* indirectDrawArguments = (VisBufferIndirectCommand*)conf_malloc(pScene->numMeshes * sizeof(VisBufferIndirectCommand));
	for (uint32_t i = 0; i < pScene->numMeshes; i++)
	{
		indirectDrawArguments[i].arg.mStartVertex = pScene->meshes[i].startVertex;
		indirectDrawArguments[i].arg.mVertexCount = 0;
		indirectDrawArguments[i].arg.mInstanceCount = 1;
		indirectDrawArguments[i].arg.mStartInstance = 0;
	}

	BufferLoadDesc filterIndirectDesc = {};
	filterIndirectDesc.mDesc.mUsage = BUFFER_USAGE_INDIRECT | BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	filterIndirectDesc.mDesc.mElementCount = pScene->numMeshes;
	filterIndirectDesc.mDesc.mStructStride = sizeof(VisBufferIndirectCommand);
	filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
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
	VisBufferIndirectCommand* indirectDrawArguments = (VisBufferIndirectCommand*)conf_malloc(MAX_DRAWS_INDIRECT * sizeof(VisBufferIndirectCommand));
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
	filterIndirectDesc.mDesc.mUsage = BUFFER_USAGE_INDIRECT | BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	filterIndirectDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	filterIndirectDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT * (sizeof(VisBufferIndirectCommand) / sizeof(uint32_t));
	filterIndirectDesc.mDesc.mStructStride = sizeof(uint32_t);
	filterIndirectDesc.mDesc.mSize = filterIndirectDesc.mDesc.mElementCount * filterIndirectDesc.mDesc.mStructStride;
	filterIndirectDesc.pData = indirectDrawArguments;

	BufferLoadDesc uncompactedDesc = {};
	uncompactedDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	uncompactedDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	uncompactedDesc.mDesc.mElementCount = MAX_DRAWS_INDIRECT;
	uncompactedDesc.mDesc.mStructStride = sizeof(UncompactedDrawArguments);
	uncompactedDesc.mDesc.mSize = uncompactedDesc.mDesc.mElementCount * uncompactedDesc.mDesc.mStructStride;
	uncompactedDesc.pData = NULL;

	BufferLoadDesc filterMaterialDesc = {};
	filterMaterialDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	filterMaterialDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	filterMaterialDesc.mDesc.mElementCount = MATERIAL_BUFFER_SIZE;
	filterMaterialDesc.mDesc.mStructStride = sizeof(uint32_t);
	filterMaterialDesc.mDesc.mSize = filterMaterialDesc.mDesc.mElementCount * filterMaterialDesc.mDesc.mStructStride;
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
		uint64_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
		pFilterBatchChunk[i] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));
		pFilterBatchChunk[i]->batches = (FilterBatchData*)conf_malloc(bufferSize);
		pFilterBatchChunk[i]->currentBatchCount = 0;
		pFilterBatchChunk[i]->currentDrawCallCount = 0;
		pFilterBatchChunk[i]->batchDataBuffer = (Buffer***)conf_malloc(pScene->numMeshes * sizeof(Buffer**));
		for (uint32_t j = 0; j < pScene->numMeshes; j++)
		{
			// Clusters are batched in BATCH_COUNT groups, which correspond to compute shader groups
			uint32_t numFilterBatches = pScene->meshes[j].clusterCount / BATCH_COUNT + 1;

			// Allocate room for max amount of batches for triangle filtering
			pFilterBatchChunk[i]->batchDataBuffer[j] = (Buffer**)conf_malloc(numFilterBatches * sizeof(Buffer*));

			for (uint32_t k = 0; k < numFilterBatches; k++)
			{
				BufferLoadDesc ubDesc = {};
				ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
				ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
				ubDesc.mDesc.mSize = bufferSize;
				ubDesc.pData = nullptr;
				ubDesc.ppBuffer = &(pFilterBatchChunk[i]->batchDataBuffer[j][k]);
				addResource(&ubDesc);
			}
		}
#else
		for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
		{
			uint32_t bufferSize = BATCH_COUNT * sizeof(FilterBatchData);
			pFilterBatchChunk[i][j] = (FilterBatchChunk*)conf_malloc(sizeof(FilterBatchChunk));
			pFilterBatchChunk[i][j]->batches = (FilterBatchData*)conf_malloc(bufferSize);
			pFilterBatchChunk[i][j]->currentBatchCount = 0;
			pFilterBatchChunk[i][j]->currentDrawCallCount = 0;

			addUniformRingBuffer(pRenderer, bufferSize, &pFilterBatchChunk[i][j]->pRingBuffer);
		}
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
	meshConstantDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV;
	meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	meshConstantDesc.mDesc.mElementCount = pScene->numMeshes;
	meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
	meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
	meshConstantDesc.pData = meshConstants;
	meshConstantDesc.ppBuffer = &pMeshConstantsBuffer;
	addResource(&meshConstantDesc);

	conf_free(meshConstants);
#endif

	/************************************************************************/
	// Per Frame Constant Buffers
	/************************************************************************/
	uint64_t size = sizeof(PerFrameConstants);
	BufferLoadDesc ubDesc = {};
	ubDesc.mDesc.mSize = size;
	ubDesc.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
	ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	ubDesc.pData = nullptr;

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
	for (uint32_t j = 0; j < pScene->numMeshes; j++) {

		PerBatchConstants perBatchData;
		const Material* mat = &pScene->materials[pScene->meshes[j].materialId];
		perBatchData.drawId = j;
		perBatchData.twoSided = (mat->twoSided ? 1 : 0);

		BufferLoadDesc batchUb = {};
		batchUb.mDesc.mSize = sizeof(PerBatchConstants);
		batchUb.mDesc.mUsage = BUFFER_USAGE_UNIFORM;
		batchUb.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		batchUb.pData = &perBatchData;
		batchUb.ppBuffer = &gPerBatchUniformBuffers[j];
		addResource(&batchUb);
	}
#endif

	// Setup lights uniform buffer
	for (uint32_t i = 0; i < LIGHT_COUNT; i++)
	{
#ifdef SPONZA
		gLightData[i].position.setX(float(rand() % 3000) - 1500.0f);
		gLightData[i].position.setY(100);
		gLightData[i].position.setZ(float(rand() % 3000) - 1500.0f);
#else
		gLightData[i].position.setX(float(rand() % 2000) - 1000.0f);
		gLightData[i].position.setY(100);
		gLightData[i].position.setZ(float(rand() % 2000) - 1000.0f);
#endif
		gLightData[i].color.setX(float(rand() % 255) / 255.0f);
		gLightData[i].color.setY(float(rand() % 255) / 255.0f);
		gLightData[i].color.setZ(float(rand() % 255) / 255.0f);
	}
	BufferLoadDesc batchUb = {};
	batchUb.mDesc.mSize = sizeof(gLightData);
	batchUb.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_UNIFORM;
	batchUb.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	batchUb.mDesc.mFirstElement = 0;
	batchUb.mDesc.mElementCount = LIGHT_COUNT;
	batchUb.mDesc.mStructStride = sizeof(LightData);
	batchUb.pData = gLightData;
	batchUb.ppBuffer = &pLightsBuffer;
	addResource(&batchUb);

	// Setup lights cluster data
	uint32_t lightClustersInitData[LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT] = {};
	BufferLoadDesc lightClustersCountBufferDesc = {};
	lightClustersCountBufferDesc.mDesc.mSize = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
	lightClustersCountBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	lightClustersCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	lightClustersCountBufferDesc.mDesc.mFirstElement = 0;
	lightClustersCountBufferDesc.mDesc.mElementCount = LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
	lightClustersCountBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
	lightClustersCountBufferDesc.pData = lightClustersInitData;
	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		lightClustersCountBufferDesc.ppBuffer = &pLightClustersCount[frameIdx];
		addResource(&lightClustersCountBufferDesc);
	}

	BufferLoadDesc lightClustersDataBufferDesc = {};
	lightClustersDataBufferDesc.mDesc.mSize = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * sizeof(uint32_t);
	lightClustersDataBufferDesc.mDesc.mUsage = BUFFER_USAGE_STORAGE_SRV | BUFFER_USAGE_STORAGE_UAV;
	lightClustersDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	lightClustersDataBufferDesc.mDesc.mFirstElement = 0;
	lightClustersDataBufferDesc.mDesc.mElementCount = LIGHT_COUNT * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT;
	lightClustersDataBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
	lightClustersDataBufferDesc.pData = nullptr;
	for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
	{
		lightClustersDataBufferDesc.ppBuffer = &pLightClusters[frameIdx];
		addResource(&lightClustersDataBufferDesc);
	}
	/************************************************************************/
	/************************************************************************/
}

// Setup the render targets used in this demo.
// The only render target that is being currently used stores the results of the Visibility Buffer pass.
// As described earlier, this render target uses 32 bit per pixel to store draw / triangle IDs that will be
// loaded later by the shade step to reconstruct interpolated triangle data per pixel.
void addRenderTargets()
{
#ifndef METAL
	const uint32_t width = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth;
	const uint32_t height = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight;
#else
    const uint32_t width = gSwapchainWidth;
    const uint32_t height = gSwapchainHeight;
#endif

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
	depthRT.mType = RENDER_TARGET_TYPE_2D;
	depthRT.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
	depthRT.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
	depthRT.mWidth = width;
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
	shadowRTDesc.mType = RENDER_TARGET_TYPE_2D;
	shadowRTDesc.mUsage = RENDER_TARGET_USAGE_DEPTH_STENCIL;
	shadowRTDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
	shadowRTDesc.mHeight = gShadowMapSize;
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
	vbRTDesc.mType = RENDER_TARGET_TYPE_2D;
	vbRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;
	vbRTDesc.mWidth = width;
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
	deferredRTDesc.mType = RENDER_TARGET_TYPE_2D;
	deferredRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;
	deferredRTDesc.mWidth = width;
	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
	{
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[i]);
	}
	/************************************************************************/
	// MSAA render target
	/************************************************************************/
	RenderTargetDesc msaaRTDesc = {};
	msaaRTDesc.mArraySize = 1;
	msaaRTDesc.mClearValue = optimizedColorClearWhite;
	msaaRTDesc.mDepth = 1;
	msaaRTDesc.mFormat = ImageFormat::RGBA8;
	msaaRTDesc.mHeight = height;
	msaaRTDesc.mSampleCount = (SampleCount)MSAASAMPLECOUNT;
	msaaRTDesc.mSampleQuality = 0;
	msaaRTDesc.mType = RENDER_TARGET_TYPE_2D;
	msaaRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;
	msaaRTDesc.mWidth = width;
	addRenderTarget(pRenderer, &msaaRTDesc, &pRenderTargetMSAA);
	
	/************************************************************************/
	// Composition pass render target
	/************************************************************************/
	// all other passes render into this RT which is then fed into post process pass
	RenderTargetDesc postProcessRTDesc = {};
	postProcessRTDesc.mArraySize = 1;
	postProcessRTDesc.mClearValue = optimizedColorClearWhite;
	postProcessRTDesc.mDepth = 1;
	postProcessRTDesc.mFormat = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
	postProcessRTDesc.mHeight = height;
	postProcessRTDesc.mSampleCount = SAMPLE_COUNT_1;
	postProcessRTDesc.mSampleQuality = 0;
	postProcessRTDesc.mType = RENDER_TARGET_TYPE_2D;
	postProcessRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;
	postProcessRTDesc.mWidth = width;
	addRenderTarget(pRenderer, &postProcessRTDesc, &pWorldRenderTarget);

	ENDALLOCATION("RTs");
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
	aoRTDesc.mType = RENDER_TARGET_TYPE_2D;
	aoRTDesc.mUsage = RENDER_TARGET_USAGE_COLOR;
	aoRTDesc.mWidth = width;
	addRenderTarget(pRenderer, &aoRTDesc, &pRenderTargetAO);
	/************************************************************************/
	/************************************************************************/
}

void addSwapChain()
{
#ifndef METAL
	const uint32_t width = gWindow.fullScreen ? getRectWidth(gWindow.fullscreenRect) : getRectWidth(gWindow.windowedRect);
	const uint32_t height = gWindow.fullScreen ? getRectHeight(gWindow.fullscreenRect) : getRectHeight(gWindow.windowedRect);
#else
    const uint32_t width = gSwapchainWidth;
    const uint32_t height = gSwapchainHeight;
#endif

	SwapChainDesc swapChainDesc = {};
	swapChainDesc.pWindow = &gWindow;
	swapChainDesc.pQueue = pGraphicsQueue;
	swapChainDesc.mWidth = width;
	swapChainDesc.mHeight = height;
	swapChainDesc.mImageCount = gImageCount;
	swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
	swapChainDesc.mColorFormat = ImageFormat::BGRA8;
	swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };
#if !defined(METAL)
    swapChainDesc.mSrgb = true;
#else
    swapChainDesc.mSrgb = false; // SRGB RenderTargets are not supported on Metal yet.
#endif
	
	swapChainDesc.mEnableVsync = false;
	addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
}
/************************************************************************/
// Event handlers
/************************************************************************/
void onWindowResize(const WindowResizeEventData* event)
{
  UNREF_PARAM(event);
	waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);
	gFrameCount = 0;

	unload();
	load();
}

#ifndef _DURANGO
// Set the camera handlers
bool onMouseMoveHandler(const MouseMoveEventData* data)
{
	pCameraController->onMouseMove(data);
	return true;
}

bool onMouseButtonHandler(const MouseButtonEventData* data)
{
	pCameraController->onMouseButton(data);
	return true;
}

bool onMouseWheelHandler(const MouseWheelEventData* data)
{
	pCameraController->onMouseWheel(data);
	return true;
}
#endif

#if !defined(METAL)
void setResourcesToComputeCompliantState(uint32_t frameIdx, bool submitAndWait)
{
	if (submitAndWait)
		beginCmd(ppCmds[frameIdx]);

	BufferBarrier barrier[] = {
		{ pVertexBufferPosition, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
		{ pIndexBufferAll, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
		{ pMeshConstantsBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
		{ pMaterialPropertyBuffer, RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE },
		{ pFilterIndirectMaterialBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS },
		{ pUncompactedDrawArgumentsBuffer[frameIdx][VIEW_SHADOW], RESOURCE_STATE_UNORDERED_ACCESS },
		{ pUncompactedDrawArgumentsBuffer[frameIdx][VIEW_CAMERA], RESOURCE_STATE_UNORDERED_ACCESS },
		{ pLightClustersCount[frameIdx], RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE}
	}; 
	cmdResourceBarrier(ppCmds[frameIdx], 8, barrier, 0, NULL, false);
	
	BufferBarrier indirectDrawBarriers[gNumGeomSets*gNumViews] = {};
	for (uint32_t i = 0, k = 0; i < gNumGeomSets; i++)
	{
		for (uint32_t j = 0; j < gNumViews; j++, k++)
		{
			indirectDrawBarriers[k].pBuffer = pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][j];
			indirectDrawBarriers[k].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			indirectDrawBarriers[k].mSplit = false;
		}
	}
	cmdResourceBarrier(ppCmds[frameIdx], gNumGeomSets*gNumViews, indirectDrawBarriers, 0, NULL, true);

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
		queueSubmit(pGraphicsQueue, 1, ppCmds, pRenderCompleteFences[0], 0, nullptr, 0, nullptr);
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[0]);
	}
}
#endif

// Main entry point for configuring the demo. This method sets up the renderer and all resources needed for the demo,
// including scene and shader loading, and setup up the necessary buffers and initial states.
bool initApp()
{
	/************************************************************************/
	// Initialize the Forge renderer with the appropriate parameters.
	/************************************************************************/
	RendererDesc settings = {};
	initRenderer(gAppName, &settings, &pRenderer);

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

	addSwapChain();

	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		addFence(pRenderer, &pRenderCompleteFences[i]);
		addFence(pRenderer, &pComputeCompleteFences[i]);
		addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		addSemaphore(pRenderer, &pComputeCompleteSemaphores[i]);
	}
	addSemaphore(pRenderer, &pImageAcquiredSemaphore);
	/************************************************************************/
	// Initialize helper interfaces (resource loader, profiler)
	/************************************************************************/
	initResourceLoaderInterface(pRenderer, gMemoryBudget);

	
	addGpuProfiler(pRenderer, pGraphicsQueue, &pGraphicsGpuProfiler);
	addGpuProfiler(pRenderer, pComputeQueue, &pComputeGpuProfiler);
	
	/************************************************************************/
	// Start timing the scene load
	/************************************************************************/
	HiresTimer timer;

	// Load shaders
	loadShaders();
	/************************************************************************/
	// Load the scene using the SceneLoader class, which uses Assimp
	/************************************************************************/
	HiresTimer sceneLoadTimer;
	String sceneFullPath = FileSystem::FixPath(gSceneName, FSRoot::FSR_Meshes);
	pScene = loadScene(pRenderer, sceneFullPath.c_str());
	LOGINFOF("Load assimp scene : %f ms", sceneLoadTimer.GetUSec(true) / 1000.0f);
	/************************************************************************/
	// IA buffers
	/************************************************************************/
	HiresTimer bufferLoadTimer;

#if !defined(METAL)
	// Default (non-filtered) index buffer for the scene
	BufferLoadDesc ibDesc = {};
	ibDesc.mDesc.mUsage = BUFFER_USAGE_INDEX | BUFFER_USAGE_STORAGE_SRV;
	ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
	ibDesc.mDesc.mElementCount = pScene->totalTriangles;
	ibDesc.mDesc.mStructStride = sizeof(uint32_t);
	ibDesc.mDesc.mSize = ibDesc.mDesc.mElementCount * ibDesc.mDesc.mStructStride;
	ibDesc.pData = pScene->indices.data();
	ibDesc.ppBuffer = &pIndexBufferAll;
    addResource(&ibDesc);
#else
    // Fill the pIndexBufferAll with triangle IDs for the whole scene (since metal implementation doesn't use scene indices).
    uint32_t* trianglesBuffer = (uint32_t*)conf_malloc(pScene->totalTriangles*sizeof(uint32_t));
    for (uint32_t i=0, t=0; i<pScene->numMeshes; i++)
    {
       for (uint32_t j=0; j<pScene->meshes[i].triangleCount; j++, t++)
           trianglesBuffer[t] = j;
    }
    BufferLoadDesc ibDesc = {};
    ibDesc.mDesc.mUsage = BUFFER_USAGE_INDEX | BUFFER_USAGE_STORAGE_SRV;
    ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    ibDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
    ibDesc.mDesc.mElementCount = pScene->totalTriangles;
    ibDesc.mDesc.mStructStride = sizeof(uint32_t);
    ibDesc.mDesc.mSize = ibDesc.mDesc.mElementCount * ibDesc.mDesc.mStructStride;
    ibDesc.pData = trianglesBuffer;
    ibDesc.ppBuffer = &pIndexBufferAll;
    addResource(&ibDesc);
    
    conf_free(trianglesBuffer);
#endif

	// Vertex position buffer for the scene
	BufferLoadDesc vbPosDesc = {};
	vbPosDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX | BUFFER_USAGE_STORAGE_SRV;
	vbPosDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbPosDesc.mDesc.mVertexStride = sizeof(SceneVertexPos);
	vbPosDesc.mDesc.mElementCount = pScene->totalVertices;
	vbPosDesc.mDesc.mStructStride = sizeof(SceneVertexPos);
	vbPosDesc.mDesc.mSize = vbPosDesc.mDesc.mElementCount * vbPosDesc.mDesc.mStructStride;
	vbPosDesc.pData = pScene->positions.data();
	vbPosDesc.ppBuffer = &pVertexBufferPosition;
	addResource(&vbPosDesc);

	// Vertex attributes buffer for the scene
	BufferLoadDesc vbAttrDesc = {};
	vbAttrDesc.mDesc.mUsage = BUFFER_USAGE_VERTEX | BUFFER_USAGE_STORAGE_SRV;
	vbAttrDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
	vbAttrDesc.mDesc.mVertexStride = sizeof(SceneVertexAttr);
	vbAttrDesc.mDesc.mElementCount = pScene->totalVertices * (sizeof(SceneVertexAttr) / sizeof(uint32_t));
	vbAttrDesc.mDesc.mStructStride = sizeof(uint32_t);
	vbAttrDesc.mDesc.mSize = vbAttrDesc.mDesc.mElementCount * vbAttrDesc.mDesc.mStructStride;
	vbAttrDesc.pData = pScene->attributes.data();
	vbAttrDesc.ppBuffer = &pVertexBufferAttr;
	addResource(&vbAttrDesc);

	LOGINFOF("Load scene buffers : %f ms", bufferLoadTimer.GetUSec(true) / 1000.0f);
	/************************************************************************/
	// Cluster creation
	/************************************************************************/
	HiresTimer clusterTimer;
	// Calculate clusters
	for (uint32_t i = 0; i < pScene->numMeshes; ++i)
	{
		Mesh* mesh = pScene->meshes + i;
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
		diffuse.mSrgb = true;
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
	/************************************************************************/
	// Set up buffers, resources and initial states
	addRenderTargets();
	/************************************************************************/
	// Setup default depth, blend, rasterizer, sampler states
	/************************************************************************/
	addDepthState(pRenderer, &pDepthStateEnable, true, true);
	addDepthState(pRenderer, &pDepthStateDisable, false, false);
	addRasterizerState(&pRasterizerStateCullFront, CULL_MODE_FRONT);
	addRasterizerState(&pRasterizerStateCullNone, CULL_MODE_NONE);
	addRasterizerState(&pRasterizerStateCullBack, CULL_MODE_BACK);
	addRasterizerState(&pRasterizerStateCullFrontMS, CULL_MODE_FRONT, 0, 0, FILL_MODE_SOLID, true);
	addRasterizerState(&pRasterizerStateCullNoneMS, CULL_MODE_NONE, 0, 0, FILL_MODE_SOLID, true);
	addRasterizerState(&pRasterizerStateCullBackMS, CULL_MODE_BACK, 0, 0, FILL_MODE_SOLID, true);

	addBlendState(&pBlendStateOneZero, BC_ONE, BC_ONE, BC_ZERO, BC_ZERO);

	// Create sampler for VB render target
	addSampler(pRenderer, &pSamplerTrilinearAniso, FILTER_TRILINEAR_ANISO, FILTER_BILINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0.0f, 8.0f);
	addSampler(pRenderer, &pSamplerPointClamp, FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE);
	/************************************************************************/
	// Vertex layout used by all geometry passes (shadow, visibility, deferred)
	/************************************************************************/
#if !defined(METAL)
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
	vertexLayout.mAttribs[2].mBinding = 1;
	vertexLayout.mAttribs[2].mLocation = 2;
	vertexLayout.mAttribs[2].mOffset = sizeof(uint32_t);
	vertexLayout.mAttribs[3].mSemantic = SEMANTIC_TANGENT;
	vertexLayout.mAttribs[3].mFormat = ImageFormat::R32UI;
	vertexLayout.mAttribs[3].mBinding = 1;
	vertexLayout.mAttribs[3].mLocation = 3;
	vertexLayout.mAttribs[3].mOffset = sizeof(uint32_t) + sizeof(uint32_t);
#endif
	/************************************************************************/
	// Setup root signatures
	/************************************************************************/
	RootSignatureDesc geomPassRootDesc = {};
	// Set max number of bindless textures in the root signature
	geomPassRootDesc.mMaxBindlessDescriptors[DESCRIPTOR_TYPE_TEXTURE] = pScene->numMaterials;
	geomPassRootDesc.mStaticSamplers["textureFilter"] = pSamplerTrilinearAniso;

	RootSignatureDesc deferredPassRootDesc = {};
	// Set max number of bindless textures in the root signature
	deferredPassRootDesc.mMaxBindlessDescriptors[DESCRIPTOR_TYPE_TEXTURE] = pScene->numMaterials;
	deferredPassRootDesc.mStaticSamplers["textureFilter"] = pSamplerTrilinearAniso;

	RootSignatureDesc shadeRootDesc = {};
	// Set max number of bindless textures in the root signature
	shadeRootDesc.mMaxBindlessDescriptors[DESCRIPTOR_TYPE_TEXTURE] = pScene->numMaterials;
	shadeRootDesc.mStaticSamplers["depthSampler"] = pSamplerTrilinearAniso;

	RootSignatureDesc paninniRootDesc = {};
	paninniRootDesc.mStaticSamplers["uSampler"] = pSamplerTrilinearAniso;

	RootSignatureDesc aoRootDesc = {};
	aoRootDesc.mStaticSamplers["g_SamplePoint"] = pSamplerPointClamp;

	// Root signature description for bindless
	RootSignatureDesc triangleFilteringRootDesc = {};
#if defined(VULKAN)
	triangleFilteringRootDesc.mDynamicUniformBuffers.push_back("batchData");
#endif

	// Graphics root signatures
	addRootSignature(pRenderer, gNumGeomSets, pShaderShadowPass, &pRootSignatureShadowPass, &geomPassRootDesc);
	addRootSignature(pRenderer, gNumGeomSets, pShaderVisibilityBufferPass, &pRootSignatureVBPass, &geomPassRootDesc);
	addRootSignature(pRenderer, gNumGeomSets, pShaderDeferredPass, &pRootSignatureDeferredPass, &deferredPassRootDesc);

	addRootSignature(pRenderer, 2, pShaderVisibilityBufferShade, &pRootSignatureVBShade, &shadeRootDesc);
	addRootSignature(pRenderer, 2, pShaderDeferredShade, &pRootSignatureDeferredShade, &shadeRootDesc);
	addRootSignature(pRenderer, 1, &pShaderDeferredShadePointLight, &pRootSignatureDeferredShadePointLight, &shadeRootDesc);

	// Panini projection post process
	addRootSignature(pRenderer, 1, &pShaderPanini, &pRootSignaturePaniniPostProcess, &paninniRootDesc);

	addRootSignature(pRenderer, 4, pShaderAO, &pRootSignatureAO, &aoRootDesc);
	addRootSignature(pRenderer, 1, &pShaderResolve, &pRootSignatureResolve);

	// Triangle filtering root signatures
	addRootSignature(pRenderer, 1, &pShaderClearBuffers, &pRootSignatureClearBuffers);
	addRootSignature(pRenderer, 1, &pShaderTriangleFiltering, &pRootSignatureTriangleFiltering, &triangleFilteringRootDesc);

#if !defined(METAL)
	addRootSignature(pRenderer, 1, &pShaderBatchCompaction, &pRootSignatureBatchCompaction);
#endif

	addRootSignature(pRenderer, 1, &pShaderClearLightClusters, &pRootSignatureClearLightClusters);
	addRootSignature(pRenderer, 1, &pShaderClusterLights, &pRootSignatureClusterLights);
	/************************************************************************/
	// Setup indirect command signatures
	/************************************************************************/
#if defined(DIRECT3D12)
	const DescriptorInfo* pDrawId = NULL;
	IndirectArgumentDescriptor indirectArgs[2] = {};
	indirectArgs[0].mType = INDIRECT_CONSTANT;
	indirectArgs[0].mCount = 1;
	indirectArgs[1].mType = INDIRECT_DRAW_INDEX;

	CommandSignatureDesc shadowPassDesc = { pCmdPool, pRootSignatureShadowPass, 2, indirectArgs };
	pDrawId = &pRootSignatureShadowPass->pDescriptors[(*pRootSignatureShadowPass->pDescriptorNameToIndexMap)[tinystl::hash("indirectRootConstant")]];
	indirectArgs[0].mRootParameterIndex = pRootSignatureShadowPass->pRootConstantLayouts[pDrawId->mIndexInParent].mRootIndex;
	addIndirectCommandSignature(pRenderer, &shadowPassDesc, &pCmdSignatureShadowPass);

	CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 2, indirectArgs };
	pDrawId = &pRootSignatureVBPass->pDescriptors[(*pRootSignatureVBPass->pDescriptorNameToIndexMap)[tinystl::hash("indirectRootConstant")]];
	indirectArgs[0].mRootParameterIndex = pRootSignatureVBPass->pRootConstantLayouts[pDrawId->mIndexInParent].mRootIndex;
	addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);

	CommandSignatureDesc deferredPassDesc = { pCmdPool, pRootSignatureDeferredPass, 2, indirectArgs };
	pDrawId = &pRootSignatureDeferredPass->pDescriptors[(*pRootSignatureDeferredPass->pDescriptorNameToIndexMap)[tinystl::hash("indirectRootConstant")]];
	indirectArgs[0].mRootParameterIndex = pRootSignatureDeferredPass->pRootConstantLayouts[pDrawId->mIndexInParent].mRootIndex;
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
	CommandSignatureDesc shadowPassDesc = { pCmdPool, pRootSignatureShadowPass, 1, indirectArgs };
	CommandSignatureDesc vbPassDesc = { pCmdPool, pRootSignatureVBPass, 1, indirectArgs };
	CommandSignatureDesc deferredPassDesc = { pCmdPool, pRootSignatureDeferredPass, 1, indirectArgs };
	addIndirectCommandSignature(pRenderer, &shadowPassDesc, &pCmdSignatureShadowPass);
	addIndirectCommandSignature(pRenderer, &vbPassDesc, &pCmdSignatureVBPass);
	addIndirectCommandSignature(pRenderer, &deferredPassDesc, &pCmdSignatureDeferredPass);
#endif
	/************************************************************************/
	// Setup the Shadow Pass Pipeline
	/************************************************************************/
	// Setup pipeline settings
	GraphicsPipelineDesc shadowPipelineSettings = {};
	shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	shadowPipelineSettings.pDepthState = pDepthStateEnable;
	shadowPipelineSettings.pDepthStencil = pRenderTargetShadow;
	shadowPipelineSettings.pRootSignature = pRootSignatureShadowPass;
#if !defined(METAL)
	shadowPipelineSettings.pVertexLayout = &vertexLayout;
#endif

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		if (MSAASAMPLECOUNT > 1)
			shadowPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNoneMS : pRasterizerStateCullFrontMS;
		else
			shadowPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNone : pRasterizerStateCullFront;
		shadowPipelineSettings.pShaderProgram = pShaderShadowPass[i];
		addPipeline(pRenderer, &shadowPipelineSettings, &pPipelineShadowPass[i]);
	}
	/************************************************************************/
	// Setup the Visibility Buffer Pass Pipeline
	/************************************************************************/
	// Setup pipeline settings
	GraphicsPipelineDesc vbPassPipelineSettings = {};
	vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	vbPassPipelineSettings.mRenderTargetCount = 1;
	vbPassPipelineSettings.pDepthState = pDepthStateEnable;
	vbPassPipelineSettings.pDepthStencil = pDepthBuffer;
	vbPassPipelineSettings.ppRenderTargets = &pRenderTargetVBPass;
	vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;
#if !defined(METAL)
	vbPassPipelineSettings.pVertexLayout = &vertexLayout;
#endif

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		if (MSAASAMPLECOUNT > 1)
			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNoneMS : pRasterizerStateCullFrontMS;
		else
			vbPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNone : pRasterizerStateCullFront;
		vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

		addPipeline(pRenderer, &vbPassPipelineSettings, &pPipelineVisibilityBufferPass[i]);
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
	if (MSAASAMPLECOUNT > 1)
	{
		vbShadePipelineSettings.pRasterizerState = pRasterizerStateCullNoneMS;
	}
	else
	{
		vbShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	}
	vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;

	for (uint32_t i = 0; i < 2; ++i)
	{
		vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade[i];
		vbShadePipelineSettings.ppRenderTargets = &pWorldRenderTarget;
		if (MSAASAMPLECOUNT > 1)
			vbShadePipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
		else
			vbShadePipelineSettings.ppRenderTargets = &pWorldRenderTarget;

		addPipeline(pRenderer, &vbShadePipelineSettings, &pPipelineVisibilityBufferShadePost[i]);

		if (MSAASAMPLECOUNT > 1)
			vbShadePipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
		else
			vbShadePipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	
		addPipeline(pRenderer, &vbShadePipelineSettings, &pPipelineVisibilityBufferShadeSrgb[i]);
	}
	/************************************************************************/
	// Setup the resources needed for the Deferred Pass Pipeline
	/************************************************************************/
	GraphicsPipelineDesc deferredPassPipelineSettings = {};
	deferredPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	deferredPassPipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
	deferredPassPipelineSettings.pDepthState = pDepthStateEnable;
	deferredPassPipelineSettings.pDepthStencil = pDepthBuffer;
	deferredPassPipelineSettings.ppRenderTargets = pRenderTargetDeferredPass;
	deferredPassPipelineSettings.pRootSignature = pRootSignatureDeferredPass;
#if !defined(METAL)
	deferredPassPipelineSettings.pVertexLayout = &vertexLayout;
#endif

	// Create pipelines for geometry sets
	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		if (MSAASAMPLECOUNT > 1)
			deferredPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNoneMS : pRasterizerStateCullFrontMS;
		else
			deferredPassPipelineSettings.pRasterizerState = i == GEOMSET_ALPHATESTED ? pRasterizerStateCullNone : pRasterizerStateCullFront;
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
	if (MSAASAMPLECOUNT > 1)
	{
		deferredShadePipelineSettings.pRasterizerState = pRasterizerStateCullNoneMS;
	}
	else
	{
		deferredShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	}
	deferredShadePipelineSettings.pRootSignature = pRootSignatureDeferredShade;

	for (uint32_t i = 0; i < 2; ++i)
	{
		deferredShadePipelineSettings.pShaderProgram = pShaderDeferredShade[i];
	if (MSAASAMPLECOUNT > 1)
		deferredShadePipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
	else
		deferredShadePipelineSettings.ppRenderTargets = &pWorldRenderTarget;
		addPipeline(pRenderer, &deferredShadePipelineSettings, &pPipelineDeferredShadePost[i]);
	if (MSAASAMPLECOUNT > 1)
		deferredShadePipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
	else
		deferredShadePipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
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

	if (MSAASAMPLECOUNT > 1)
		deferredPointLightPipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
	else
		deferredPointLightPipelineSettings.ppRenderTargets = &pWorldRenderTarget;

	deferredPointLightPipelineSettings.pRasterizerState = pRasterizerStateCullBack;
	deferredPointLightPipelineSettings.pRootSignature = pRootSignatureDeferredShadePointLight;
	deferredPointLightPipelineSettings.pShaderProgram = pShaderDeferredShadePointLight;
	deferredPointLightPipelineSettings.pVertexLayout = &vertexLayoutPointLightShade;

	addPipeline(pRenderer, &deferredPointLightPipelineSettings, &pPipelineDeferredShadePointLightPost);

	if (MSAASAMPLECOUNT > 1)
		deferredPointLightPipelineSettings.ppRenderTargets = &pRenderTargetMSAA;
	else
		deferredPointLightPipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];

	addPipeline(pRenderer, &deferredPointLightPipelineSettings, &pPipelineDeferredShadePointLightSrgb);

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
	// Setup the panini projection post process pipeline
	/************************************************************************/
	// Setup pipeline settings
	// Create vertex layout
	VertexLayout vertexLayoutPanini = {};
	vertexLayoutPanini.mAttribCount = 1;
	vertexLayoutPanini.mAttribs[0].mSemantic = SEMANTIC_POSITION;
	vertexLayoutPanini.mAttribs[0].mFormat = ImageFormat::RGBA32F;
	vertexLayoutPanini.mAttribs[0].mBinding = 0;
	vertexLayoutPanini.mAttribs[0].mLocation = 0;
	vertexLayoutPanini.mAttribs[0].mOffset = 0;

	GraphicsPipelineDesc pipelineSettings = { 0 };
	pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	pipelineSettings.mRenderTargetCount = 1;
	pipelineSettings.pDepthState = pDepthStateDisable;
	pipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	pipelineSettings.pRootSignature = pRootSignaturePaniniPostProcess;
	pipelineSettings.pShaderProgram = pShaderPanini;
	pipelineSettings.pVertexLayout = &vertexLayoutPanini;
	addPipeline(pRenderer, &pipelineSettings, &pPipielinePaniniPostProcess);

	createTessellatedQuadBuffers(&pVertexBufferTessellatedQuad, &pIndexBufferTessellatedQuad, gPaniniDistortionTessellation[0], gPaniniDistortionTessellation[1]);
	/************************************************************************/
	// Setup HDAO post process pipeline
	/************************************************************************/
	GraphicsPipelineDesc aoPipelineSettings = {};
	aoPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	aoPipelineSettings.mRenderTargetCount = 1;
	aoPipelineSettings.pDepthState = pDepthStateDisable;
	aoPipelineSettings.ppRenderTargets = &pRenderTargetAO;
	aoPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	aoPipelineSettings.pRootSignature = pRootSignatureAO;
	for (uint32_t i = 0; i < 4; ++i)
	{
		aoPipelineSettings.pShaderProgram = pShaderAO[i];
		addPipeline(pRenderer, &aoPipelineSettings, &pPipelineAO[i]);
	}
	/************************************************************************/
	// Setup MSAA resolve pipeline
	/************************************************************************/
	GraphicsPipelineDesc resolvePipelineSettings = { 0 };
	resolvePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
	resolvePipelineSettings.mRenderTargetCount = 1;
	resolvePipelineSettings.pDepthState = pDepthStateDisable;
	resolvePipelineSettings.ppRenderTargets = &pSwapChain->ppSwapchainRenderTargets[0];
	resolvePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
	resolvePipelineSettings.pRootSignature = pRootSignatureResolve;
	resolvePipelineSettings.pShaderProgram = pShaderResolve;
	//resolvePipelineSettings.pVertexLayout = &vertexLayoutPanini;
	addPipeline(pRenderer, &resolvePipelineSettings, &pPipelineResolve);
	resolvePipelineSettings.ppRenderTargets = &pWorldRenderTarget;
	addPipeline(pRenderer, &resolvePipelineSettings, &pPipelineResolvePost);

	/************************************************************************/
	// Setup the UI components for text rendering, UI controls...
	/************************************************************************/
	UISettings uiSettings = {};
	uiSettings.pDefaultFontName = "NeoSans-Bold.ttf";
	addUIManagerInterface(pRenderer, &uiSettings, &pUIManager);

	GuiDesc guiDesc = {};
	addGui(pUIManager, &guiDesc, &pGuiWindow);

	// Light Settings
	//---------------------------------------------------------------------------------
	// offset max angle for sun control so the light won't bleed with
	// small glancing angles, i.e., when lightDir is almost parallel to the plane
	const float maxAngleOffset = 0.017f;
	UIProperty sun("Sun Control", gAppSettings.mSunControl, 0, PI - maxAngleOffset, 0.001f);
	addProperty(pGuiWindow, &sun);

	UIProperty esm("Shadow Control", gAppSettings.mEsmControl, 0, 200.0f);
	addProperty(pGuiWindow, &esm);

	UIProperty retina("Retina Scale", gAppSettings.mRetinaScaling, 0, 5.0f);
	addProperty(pGuiWindow, &retina);

	UIProperty localLight("Enable Random Point Lights", gAppSettings.mRenderLocalLights);
	addProperty(pGuiWindow, &localLight);
	/************************************************************************/
	// Rendering Settings
	/************************************************************************/
	static const char* renderModeNames[] = {
		"Visibility Buffer",
		"Deferred Shading",
		nullptr
	};
	static const RenderMode renderModeValues[] = {
		RENDERMODE_VISBUFF,
		RENDERMODE_DEFERRED,
		RENDERMODE_COUNT
	};
	UIProperty renderMode("Render Mode", gAppSettings.mRenderMode, renderModeNames, renderModeValues);
	addProperty(pGuiWindow, &renderMode);

	UIProperty holdProp = UIProperty("Hold filtered results", gAppSettings.mHoldFilteredResults);
	addProperty(pGuiWindow, &holdProp);

	UIProperty filtering("Triangle Filtering", gAppSettings.mFilterTriangles);
	addProperty(pGuiWindow, &filtering);

	UIProperty cluster("Cluster Culling", gAppSettings.mClusterCulling);
	addProperty(pGuiWindow, &cluster);

	UIProperty asyncCompute("Async Compute", gAppSettings.mAsyncCompute);
	addProperty(pGuiWindow, &asyncCompute);

#if MSAASAMPLECOUNT == 1
	UIProperty debugTargets("Draw Debug Targets", gAppSettings.mDrawDebugTargets);
	addProperty(pGuiWindow, &debugTargets);
#endif
	/************************************************************************/
	// Panini Settings
	/************************************************************************/
	UIProperty fov("Camera Horizontal FoV", gAppSettings.mPaniniSettings.FoVH, 30.0f, 179.0f, 1.0f);
	addProperty(pGuiWindow, &fov);

	UIProperty toggle("Enable Panini Projection", gAppSettings.mEnablePaniniProjection);
	addProperty(pGuiWindow, &toggle);

	tinystl::vector<UIProperty>& dynamicProps = gAppSettings.mDynamicUIControlsPanini.mDynamicProperties;	// shorthand
	dynamicProps.push_back(UIProperty("Panini D Parameter", gAppSettings.mPaniniSettings.D, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Panini S Parameter", gAppSettings.mPaniniSettings.S, 0.0f, 1.0f, 0.001f));
	dynamicProps.push_back(UIProperty("Screen Scale", gAppSettings.mPaniniSettings.scale, 1.0f, 10.0f, 0.01f));

	if (gAppSettings.mEnablePaniniProjection)
	{
		gAppSettings.mDynamicUIControlsPanini.ShowDynamicProperties(pGuiWindow);
	}
	/************************************************************************/
	// HDAO Settings
	/************************************************************************/
	UIProperty toggleAO("Enable HDAO", gAppSettings.mEnableHDAO);
	addProperty(pGuiWindow, &toggleAO);

	tinystl::vector<UIProperty>& dynamicPropsAO = gAppSettings.mDynamicUIControlsAO.mDynamicProperties;	// shorthand
	dynamicPropsAO.push_back(UIProperty("AO accept radius", gAppSettings.mAcceptRadius, 0, 10));
	dynamicPropsAO.push_back(UIProperty("AO reject radius", gAppSettings.mRejectRadius, 0, 10));
	dynamicPropsAO.push_back(UIProperty("AO intensity radius", gAppSettings.mAOIntensity, 0, 10));
	dynamicPropsAO.push_back(UIProperty("AO Quality", gAppSettings.mAOQuality, 1, 4));
	if (gAppSettings.mEnableHDAO)
	{
		gAppSettings.mDynamicUIControlsAO.ShowDynamicProperties(pGuiWindow);
	}
	/************************************************************************/
	// Setup the fps camera for navigating through the scene
	/************************************************************************/
#if OLD_MODELS
	vec3 startPosition(1306.29614f, 490.245087f, 86.3251801f);
	vec3 startLookAt = startPosition + vec3(-0.882f, -0.372f, 1.0f);
#else
	/*
	vec3 startPosition(-30, 800, 0);
	vec3 startLookAt = startPosition + vec3(1, -1, -1);
	*/
	vec3 startPosition(911, 807, -554);
	vec3 startLookAt = startPosition + vec3(-0.882f, -0.372f, 1.0f); //(-30, 800, 0);
#endif
	CameraMotionParameters camParams;
	camParams.acceleration = 2600 * 2.5f;
	camParams.braking = 2600 * 2.5f;
	camParams.maxSpeed = 500 * 2.5f;
	pCameraController = createFpsCameraController(startPosition, startLookAt);
	pCameraController->setMotionParameters(camParams);
	requestMouseCapture(true);
	/************************************************************************/
	/************************************************************************/
	// Finish the resource loading process since the next code depends on the loaded resources
	finishResourceLoading();

#if !OLD_MODELS
	for (uint32_t i = 0; i < pScene->numMaterials; ++i)
	{
		pScene->materials[i].alphaTested = ImageFormat::GetChannelCount(gDiffuseMaps[i]->mDesc.mFormat) > 3;
	}
#endif

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
    
	return true;
}

void removeSwapChain()
{
	removeSwapChain(pRenderer, pSwapChain);
}

void removeRenderTargets()
{
	removeRenderTarget(pRenderer, pRenderTargetMSAA);
	removeRenderTarget(pRenderer, pRenderTargetAO);
	removeRenderTarget(pRenderer, pDepthBuffer);
	removeRenderTarget(pRenderer, pWorldRenderTarget);
	removeRenderTarget(pRenderer, pRenderTargetVBPass);
	removeRenderTarget(pRenderer, pRenderTargetShadow);

	for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);
}

void destroyTriangleFilteringBuffers()
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
		for (uint32_t i = 0; i < gImageCount; i++)
		{
			for (uint32_t j = 0; j < pScene->numMeshes; j++)
			{
				// Clusters are batched in BATCH_COUNT groups, which correspond to compute shader groups
				uint32_t numFilterBatches = pScene->meshes[j].clusterCount / BATCH_COUNT + 1;

				for (uint32_t k = 0; k < numFilterBatches; k++)
				{
					removeResource(pFilterBatchChunk[i]->batchDataBuffer[j][k]);
				}

				conf_free(pFilterBatchChunk[i]->batchDataBuffer[j]);
			}

			conf_free(pFilterBatchChunk[i]->batchDataBuffer);
			conf_free(pFilterBatchChunk[i]->batches);
			conf_free(pFilterBatchChunk[i]);
		}
#else
		for (uint32_t j = 0; j < gSmallBatchChunkCount; ++j)
		{
			removeUniformRingBuffer(pFilterBatchChunk[i][j]->pRingBuffer);
			conf_free(pFilterBatchChunk[i][j]->batches);
			conf_free(pFilterBatchChunk[i][j]);
		}
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
	}
	/************************************************************************/
	/************************************************************************/
}

void unloadShaders()
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
	removeShader(pRenderer, pShaderPanini);
	for (uint32_t i = 0; i < 4; ++i)
		removeShader(pRenderer, pShaderAO[i]);
	removeShader(pRenderer, pShaderResolve);
}

// Destroy allocated resources
void exitApp()
{
	waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);
	waitForFences(pComputeQueue, gImageCount, pComputeCompleteFences);

	destroyTriangleFilteringBuffers();

	destroyCameraController(pCameraController);

	removeGui(pUIManager, pGuiWindow);
	removeUIManagerInterface(pRenderer, pUIManager);

	destroyBuffers(pRenderer, pVertexBufferTessellatedQuad, pIndexBufferTessellatedQuad);
	// Destroy geometry for light rendering
	destroyBuffers(pRenderer, pVertexBufferCube, pIndexBufferCube);

	removePipeline(pRenderer, pPipielinePaniniPostProcess);

	for (uint32_t i = 0; i < 4; ++i)
		removePipeline(pRenderer, pPipelineAO[i]);
	removePipeline(pRenderer, pPipelineResolve);
	removePipeline(pRenderer, pPipelineResolvePost);

	// Destroy triangle filtering pipelines
	removePipeline(pRenderer, pPipelineClusterLights);
	removePipeline(pRenderer, pPipelineClearLightClusters);
	removePipeline(pRenderer, pPipelineTriangleFiltering);
#if !defined(METAL)
	removePipeline(pRenderer, pPipelineBatchCompaction);
#endif
	removePipeline(pRenderer, pPipelineClearBuffers);

	// Destroy graphics pipelines
	removePipeline(pRenderer, pPipelineDeferredShadePointLightPost);
	removePipeline(pRenderer, pPipelineDeferredShadePointLightSrgb);
	for (uint32_t i = 0; i < 2; ++i)
	{
		removePipeline(pRenderer, pPipelineDeferredShadePost[i]);
		removePipeline(pRenderer, pPipelineDeferredShadeSrgb[i]);
	}

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
		removePipeline(pRenderer, pPipelineDeferredPass[i]);

	for (uint32_t i = 0; i < 2; ++i)
	{
		removePipeline(pRenderer, pPipelineVisibilityBufferShadePost[i]);
		removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb[i]);
	}

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
		removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
		removePipeline(pRenderer, pPipelineShadowPass[i]);

	removeRootSignature(pRenderer, pRootSignatureResolve);
	removeRootSignature(pRenderer, pRootSignatureAO);
	removeRootSignature(pRenderer, pRootSignaturePaniniPostProcess);
	removeRootSignature(pRenderer, pRootSignatureClusterLights);
	removeRootSignature(pRenderer, pRootSignatureClearLightClusters);
	removeRootSignature(pRenderer, pRootSignatureBatchCompaction);
	removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
	removeRootSignature(pRenderer, pRootSignatureClearBuffers);
	removeRootSignature(pRenderer, pRootSignatureDeferredShadePointLight);
	removeRootSignature(pRenderer, pRootSignatureDeferredShade);
	removeRootSignature(pRenderer, pRootSignatureDeferredPass);
	removeRootSignature(pRenderer, pRootSignatureVBShade);
	removeRootSignature(pRenderer, pRootSignatureVBPass);
	removeRootSignature(pRenderer, pRootSignatureShadowPass);

	removeIndirectCommandSignature(pRenderer, pCmdSignatureDeferredPass);
	removeIndirectCommandSignature(pRenderer, pCmdSignatureVBPass);
	removeIndirectCommandSignature(pRenderer, pCmdSignatureShadowPass);

	removeRenderTargets();
	/************************************************************************/
	// Remove loaded scene
	/************************************************************************/
	// Destroy scene buffers
#if !defined(METAL)
	removeResource(pIndexBufferAll);
#endif
	removeResource(pVertexBufferPosition);
	removeResource(pVertexBufferAttr);
	// Destroy clusters
	for (uint32_t i = 0; i < pScene->numMeshes; ++i)
	{
		conf_free(pScene->meshes[i].clusters);
	}
	// Remove Textures
	for (uint32_t i = 0; i < pScene->numMaterials; ++i)
	{
		removeResource(gDiffuseMaps[i]);
		removeResource(gNormalMaps[i]);
		removeResource(gSpecularMaps[i]);
	}

	removeScene(pScene);
	/************************************************************************/
	/************************************************************************/
	unloadShaders();

	removeSwapChain();

	// Remove default fences, semaphores
	for (uint32_t i = 0; i < gImageCount; ++i)
	{
		removeFence(pRenderer, pRenderCompleteFences[i]);
		removeFence(pRenderer, pComputeCompleteFences[i]);
		removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		removeSemaphore(pRenderer, pComputeCompleteSemaphores[i]);
	}
	removeSemaphore(pRenderer, pImageAcquiredSemaphore);

	removeCmd_n(pCmdPool, gImageCount, ppCmds);
	removeCmdPool(pRenderer, pCmdPool);
	removeQueue(pGraphicsQueue);

	removeCmd_n(pComputeCmdPool, gImageCount, ppComputeCmds);
	removeCmdPool(pRenderer, pComputeCmdPool);
	removeQueue(pComputeQueue);

	removeSampler(pRenderer, pSamplerTrilinearAniso);
	removeSampler(pRenderer, pSamplerPointClamp);

	removeBlendState(pBlendStateOneZero);

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
	removeRenderer(pRenderer);
}

// Updates uniform data for the given frame index.
// This includes transform matrices, render target resolution and global information about the scene.
void updateUniformData(float deltaTime)
{
	static float t = 0.0f;
	//t += deltaTime;
  UNREF_PARAM(deltaTime);
	const uint32_t width = pSwapChain->mDesc.mWidth;
	const uint32_t height = pSwapChain->mDesc.mHeight;
	const float aspectRatioInv = (float)height / width;
	const uint32_t frameIdx = gFrameCount % gImageCount;
	PerFrameData* currentFrame = &gPerFrame[frameIdx];

	mat4 cameraModel = mat4::scale(vec3(SCENE_SCALE));
	mat4 cameraView = mat4::translation(vec3(0, 0, -t * 100)) * pCameraController->getViewMatrix();;
	mat4 cameraProj = mat4::perspective(gAppSettings.mPaniniSettings.FoVH * (PI / 180.f), aspectRatioInv, 10.0f, 8000.0f);

	// Compute light matrices
#if OLD_MODELS
	Point3 lightSourcePos(674.087891f, 721.951294f, 767.546692f);
#else
	Point3 lightSourcePos(0, 1300, 0);
#endif

	// directional light rotation & translation
	mat4 rotation = mat4::rotation(gAppSettings.mSunControl, vec3(-1, 0, 0));
	mat4 translation = mat4::translation(-vec3(lightSourcePos));
	vec4 lightDir = (inverse(rotation) * vec4(0, 0, 1, 0));

	mat4 lightModel = mat4::scale(vec3(SCENE_SCALE));
	mat4 lightView = rotation * translation;
	mat4 lightProj = mat4::orthographic(-1000, 700, -300, 1100, -4000, 4000);

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
	currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].invVP = inverse(currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp);
	currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].projection = lightProj;
	currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].mvp = currentFrame->gPerFrameUniformData.transform[VIEW_SHADOW].vp * lightModel;

	currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp = cameraProj * cameraView;
	currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].invVP = inverse(currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp);
	currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].projection = cameraProj;
	currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].mvp = currentFrame->gPerFrameUniformData.transform[VIEW_CAMERA].vp * cameraModel;
	/************************************************************************/
	// Culling data
	/************************************************************************/
	currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].sampleCount = MSAASAMPLECOUNT;
	currentFrame->gPerFrameUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)gShadowMapSize, (float)gShadowMapSize };

	currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].sampleCount = MSAASAMPLECOUNT;
	currentFrame->gPerFrameUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

	// Cache eye position in object space for cluster culling on the CPU
	currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView * lightModel) * vec4(0, 0, 0, 1)).getXYZ();
	currentFrame->gEyeObjectSpace[VIEW_CAMERA] = (inverse(cameraView * cameraModel) * vec4(0, 0, 0, 1)).getXYZ();  // vec4(0,0,0,1) is the camera position in eye space
}

// Render the shadow mapping pass. This pass updates the shadow map texture
void drawShadowMapPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
{
	const char* profileNames[gNumGeomSets] = { "Opaque", "Alpha" };
	// Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
	loadActions.mClearDepth = pRenderTargetShadow->mDesc.mClearValue;

	// Start render pass and apply load actions
	cmdBeginRender(cmd, 0, NULL, pRenderTargetShadow, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mDesc.mWidth, (float)pRenderTargetShadow->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mDesc.mWidth, pRenderTargetShadow->mDesc.mHeight);

	Buffer* pVertexBuffers[] = { pVertexBufferPosition, pVertexBufferAttr };
	cmdBindVertexBuffer(cmd, 2, pVertexBuffers);

#if defined(METAL)
	Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pIndexBufferAll);
    
    DescriptorData shadowParams[3];
    shadowParams[0].pName = "vertexPos";
    shadowParams[0].ppBuffers = &pVertexBufferPosition;
    shadowParams[1].pName = "uniforms";
    shadowParams[1].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
    shadowParams[2].pName = "filteredTriangles";
    shadowParams[2].ppBuffers = &filteredTrianglesBuffer;
    cmdBindDescriptors(cmd, pRootSignatureShadowPass, 3, shadowParams);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

		Buffer* indirectDrawArguments = (gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_SHADOW] : pIndirectDrawArgumentsBufferAll);

        DescriptorData indirectParams[1] = {};
        indirectParams[0].pName = "indirectDrawArgs";
        indirectParams[0].ppBuffers = &indirectDrawArguments;
        cmdBindDescriptors(cmd, pRootSignatureShadowPass, 1, indirectParams);
		cmdBindPipeline(cmd, pPipelineShadowPass[i]);

		for (uint32_t m = 0; m < pScene->numMeshes; ++m)
		{
			// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
			uint32_t materialGeometrySet = (pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
			if (materialGeometrySet != i)
				continue;

            DescriptorData meshParams[1] = {};
            meshParams[0].pName = "perBatch";
            meshParams[0].ppBuffers = &gPerBatchUniformBuffers[m];
            cmdBindDescriptors(cmd, pRootSignatureShadowPass, 1, meshParams);
			cmdExecuteIndirect(cmd, pCmdSignatureShadowPass, 1, indirectDrawArguments, m * sizeof(VisBufferIndirectCommand), nullptr, 0);
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
#else
	Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_SHADOW] : pIndexBufferAll;
	Buffer* pIndirectMaterialBuffer = gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
	cmdBindIndexBuffer(cmd, pIndexBuffer);

	DescriptorData params[3] = {};
	params[0].pName = "diffuseMaps";
	params[0].mCount = (uint32_t)gDiffuseMaps.size();
	params[0].ppTextures = gDiffuseMaps.data();
	params[1].pName = "indirectMaterialBuffer";
	params[1].ppBuffers = &pIndirectMaterialBuffer;
	params[2].pName = "uniforms";
	params[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
	cmdBindDescriptors(cmd, pRootSignatureShadowPass, 3, params);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
		cmdBindPipeline(cmd, pPipelineShadowPass[i]);
		Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_SHADOW] : pIndirectDrawArgumentsBufferAll[i];
		cmdExecuteIndirect(cmd, pCmdSignatureShadowPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer, DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
#endif

	cmdEndRender(cmd, 0, NULL, pRenderTargetShadow);
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
	cmdBeginRender(cmd, 1, &pRenderTargetVBPass, pDepthBuffer, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mDesc.mWidth, (float)pRenderTargetVBPass->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mDesc.mWidth, pRenderTargetVBPass->mDesc.mHeight);

	Buffer* pVertexBuffers[] = { pVertexBufferPosition, pVertexBufferAttr };
	cmdBindVertexBuffer(cmd, 2, pVertexBuffers);

#if defined(METAL)
	Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll);
    
    DescriptorData vbPassParams[4];
    vbPassParams[0].pName = "vertexPos";
    vbPassParams[0].ppBuffers = &pVertexBufferPosition;
    vbPassParams[1].pName = "vertexAttrs";
    vbPassParams[1].ppBuffers = &pVertexBufferAttr;
    vbPassParams[2].pName = "uniforms";
    vbPassParams[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
    vbPassParams[3].pName = "filteredTriangles";
    vbPassParams[3].ppBuffers = &filteredTrianglesBuffer;
    cmdBindDescriptors(cmd, pRootSignatureVBPass, 4, vbPassParams);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

		Buffer* indirectDrawArguments = (gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA] : pIndirectDrawArgumentsBufferAll);
        
        DescriptorData indirectParams[1] = {};
        indirectParams[0].pName = "indirectDrawArgs";
        indirectParams[0].ppBuffers = &indirectDrawArguments;
        cmdBindDescriptors(cmd, pRootSignatureVBPass, 1, indirectParams);
        cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);

		for (uint32_t m = 0; m < pScene->numMeshes; ++m)
		{
			// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
			uint32_t materialGeometrySet = (pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
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
	Buffer* pIndirectMaterialBuffer = gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
	cmdBindIndexBuffer(cmd, pIndexBuffer);

	DescriptorData params[3] = {};
	params[0].pName = "diffuseMaps";
	params[0].mCount = (uint32_t)gDiffuseMaps.size();
	params[0].ppTextures = gDiffuseMaps.data();
	params[1].pName = "indirectMaterialBuffer";
	params[1].ppBuffers = &pIndirectMaterialBuffer;
	params[2].pName = "uniforms";
	params[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
	cmdBindDescriptors(cmd, pRootSignatureVBPass, 3, params);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
		cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
		Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA] : pIndirectDrawArgumentsBufferAll[i];
		cmdExecuteIndirect(cmd, pCmdSignatureVBPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer, DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
#endif

	cmdEndRender(cmd, 1, &pRenderTargetVBPass, pDepthBuffer);
}

// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawVisibilityBufferPass
// to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method doesn't set any vertex/index buffer because
// the triangle positions are calculated internally using vertex_id.
void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
{
	RenderTarget* pDestinationRenderTarget;
	if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
	else
		pDestinationRenderTarget = gAppSettings.mEnablePaniniProjection ? pWorldRenderTarget : pScreenRenderTarget;

	// Set load actions to clear the screen to black
	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	loadActions.mClearColorValues[0] = pDestinationRenderTarget->mDesc.mClearValue;

	cmdBeginRender(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

	cmdBindPipeline(cmd, gAppSettings.mEnablePaniniProjection ?
		pPipelineVisibilityBufferShadePost[gAppSettings.mEnableHDAO] : pPipelineVisibilityBufferShadeSrgb[gAppSettings.mEnableHDAO]);

#if defined(METAL)
	const uint32_t numDescriptors = 15;
#else
	const uint32_t numDescriptors = 17;
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
	vbShadeParams[1].mCount = gDiffuseMaps.getCount();
	vbShadeParams[1].ppTextures = gDiffuseMaps.data();
    vbShadeParams[2].pName = "normalMaps";
	vbShadeParams[2].mCount = gNormalMaps.getCount();
	vbShadeParams[2].ppTextures = gNormalMaps.data();
    vbShadeParams[3].pName = "specularMaps";
	vbShadeParams[3].mCount = gSpecularMaps.getCount();
    vbShadeParams[3].ppTextures = gSpecularMaps.data();
    vbShadeParams[4].pName = "textureSampler";
    vbShadeParams[4].ppSamplers = &pSamplerTrilinearAniso;
    vbShadeParams[5].pName = "vertexPos";
    vbShadeParams[5].ppBuffers = &pVertexBufferPosition;
    vbShadeParams[6].pName = "vertexAttr";
	vbShadeParams[6].ppBuffers = &pVertexBufferAttr;
	vbShadeParams[7].pName = "lights";
	vbShadeParams[7].ppBuffers = &pLightsBuffer;
	vbShadeParams[8].pName = "lightClustersCount";
	vbShadeParams[8].ppBuffers = &pLightClustersCount[frameIdx];
	vbShadeParams[9].pName = "lightClusters";
	vbShadeParams[9].ppBuffers = &pLightClusters[frameIdx];
	vbShadeParams[10].pName = "indirectDrawArgs";
#if defined(METAL)
	vbShadeParams[10].ppBuffers = gAppSettings.mFilterTriangles ? &pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA] : &pIndirectDrawArgumentsBufferAll;
#else
	vbShadeParams[10].mCount = gNumGeomSets;
	vbShadeParams[10].ppBuffers = gAppSettings.mFilterTriangles ? pIndirectBuffers : pIndirectDrawArgumentsBufferAll;
#endif
	vbShadeParams[11].pName = "uniforms";
	vbShadeParams[11].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
	vbShadeParams[12].pName = "aoTex";
	vbShadeParams[12].ppTextures = &pRenderTargetAO->pTexture;
	vbShadeParams[13].pName = "shadowMap";
	vbShadeParams[13].ppTextures = &pRenderTargetShadow->pTexture;
	vbShadeParams[14].pName = "indirectMaterialBuffer";
#if defined(METAL)
	vbShadeParams[14].ppBuffers = &pIndirectMaterialBufferAll;
#else
	vbShadeParams[14].ppBuffers = gAppSettings.mFilterTriangles ? &pFilterIndirectMaterialBuffer[frameIdx] : &pIndirectMaterialBufferAll;
	vbShadeParams[15].pName = "filteredIndexBuffer";
	vbShadeParams[15].ppBuffers = gAppSettings.mFilterTriangles ? &pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : &pIndexBufferAll;
	vbShadeParams[16].pName = "meshConstantsBuffer";
	vbShadeParams[16].ppBuffers = &pMeshConstantsBuffer;
#endif
	cmdBindDescriptors(cmd, pRootSignatureVBShade, numDescriptors, vbShadeParams);

	// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
	cmdDraw(cmd, 3, 0);

	cmdEndRender(cmd, 1, &pDestinationRenderTarget, NULL);
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
	cmdBeginRender(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);

	Buffer* pVertexBuffers[] = { pVertexBufferPosition, pVertexBufferAttr };
	cmdBindVertexBuffer(cmd, 2, pVertexBuffers);

#if defined(METAL)
	Buffer* filteredTrianglesBuffer = (gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll);
    
    DescriptorData deferredPassParams[5];
    deferredPassParams[0].pName = "vertexPos";
    deferredPassParams[0].ppBuffers = &pVertexBufferPosition;
    deferredPassParams[1].pName = "vertexAttrs";
    deferredPassParams[1].ppBuffers = &pVertexBufferAttr;
    deferredPassParams[2].pName = "uniforms";
    deferredPassParams[2].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
    deferredPassParams[3].pName = "filteredTriangles";
    deferredPassParams[3].ppBuffers = &filteredTrianglesBuffer;
    deferredPassParams[4].pName = "textureSampler";
    deferredPassParams[4].ppSamplers = &pSamplerTrilinearAniso;
    cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 5, deferredPassParams);

	for (uint32_t i = 0; i < gNumGeomSets; ++i)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);

		Buffer* indirectDrawArguments = (gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][VIEW_CAMERA] : pIndirectDrawArgumentsBufferAll);

        DescriptorData indirectParams[1] = {};
        indirectParams[0].pName = "indirectDrawArgs";
        indirectParams[0].ppBuffers = &indirectDrawArguments;
        cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 1, indirectParams);
        cmdBindPipeline(cmd, pPipelineDeferredPass[i]);

		for (uint32_t m = 0; m < pScene->numMeshes; ++m)
		{
			// Ignore meshes that do not correspond to the geometry set (opaque / alpha tested) that we want to render
			uint32_t materialGeometrySet = (pScene->materials[pScene->meshes[m].materialId].alphaTested ? GEOMSET_ALPHATESTED : GEOMSET_OPAQUE);
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
            cmdExecuteIndirect(cmd, pCmdSignatureDeferredPass, 1, indirectDrawArguments, m * sizeof(VisBufferIndirectCommand), nullptr, 0);
		}

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
#else
	Buffer* pIndexBuffer = gAppSettings.mFilterTriangles ? pFilteredIndexBuffer[frameIdx][VIEW_CAMERA] : pIndexBufferAll;
	Buffer* pIndirectMaterialBuffer = gAppSettings.mFilterTriangles ? pFilterIndirectMaterialBuffer[frameIdx] : pIndirectMaterialBufferAll;
	cmdBindIndexBuffer(cmd, pIndexBuffer);

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
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, profileNames[i]);
		cmdBindPipeline(cmd, pPipelineDeferredPass[i]);
		Buffer* pIndirectBuffer = gAppSettings.mFilterTriangles ? pFilteredIndirectDrawArgumentsBuffer[frameIdx][i][VIEW_CAMERA] : pIndirectDrawArgumentsBufferAll[i];
		cmdExecuteIndirect(cmd, pCmdSignatureDeferredPass, gPerFrame[frameIdx].gDrawCount[i], pIndirectBuffer, 0, pIndirectBuffer, DRAW_COUNTER_SLOT_OFFSET_IN_BYTES);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
#endif

	cmdEndRender(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pDepthBuffer);
}

// Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by DrawDeferredPass
// to get per pixel geometry data to calculate the final color.
void drawDeferredShade(Cmd* cmd, uint32_t frameIdx)
{
	RenderTarget* pDestinationRenderTarget;
	if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
	else
		pDestinationRenderTarget = gAppSettings.mEnablePaniniProjection ? pWorldRenderTarget : pScreenRenderTarget;

	// Set load actions to clear the screen to black
	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	loadActions.mClearColorValues[0] = pDestinationRenderTarget->mDesc.mClearValue;

	cmdBeginRender(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

	cmdBindPipeline(cmd, gAppSettings.mEnablePaniniProjection ?
		pPipelineDeferredShadePost[gAppSettings.mEnableHDAO] : pPipelineDeferredShadeSrgb[gAppSettings.mEnableHDAO]);

	DescriptorData params[8] = {};
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
	cmdBindDescriptors(cmd, pRootSignatureDeferredShade, 8, params);

	// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
	cmdDraw(cmd, 3, 0);

	cmdEndRender(cmd, 1, &pDestinationRenderTarget, NULL);
}

// Render light geometry on the screen to evaluate lighting at those points.
void drawDeferredShadePointLights(Cmd* cmd, uint32_t frameIdx)
{
	RenderTarget* pDestinationRenderTarget;
	if (MSAASAMPLECOUNT > 1)
		pDestinationRenderTarget = pRenderTargetMSAA;
	else
		pDestinationRenderTarget = gAppSettings.mEnablePaniniProjection ? pWorldRenderTarget : pScreenRenderTarget;

	cmdBeginRender(cmd, 1, &pDestinationRenderTarget, NULL);
	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

	cmdBindPipeline(cmd, gAppSettings.mEnablePaniniProjection ? pPipelineDeferredShadePointLightPost : pPipelineDeferredShadePointLightSrgb);

	const uint32_t numDescriptors = 7;
	DescriptorData params[numDescriptors] = {};
	params[0].pName = "lights";
	params[0].ppBuffers = &pLightsBuffer;
	params[1].pName = "gBufferNormal";
	params[1].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture;
	params[2].pName = "gBufferSpecular";
	params[2].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
	params[3].pName = "gBufferSpecular";
	params[3].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SPECULAR]->pTexture;
	params[4].pName = "gBufferSimulation";
	params[4].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_SIMULATION]->pTexture;
	params[5].pName = "gBufferDepth";
	params[5].ppTextures = &pDepthBuffer->pTexture;
	params[6].pName = "uniforms";
	params[6].ppBuffers = &pPerFrameUniformBuffers[frameIdx];
	cmdBindDescriptors(cmd, pRootSignatureDeferredShadePointLight, numDescriptors, params);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferCube);
	cmdBindIndexBuffer(cmd, pIndexBufferCube);
	cmdDrawIndexedInstanced(cmd, 36, 0, LIGHT_COUNT);

	cmdEndRender(cmd, 1, &pDestinationRenderTarget, NULL);
}

// Reads the screen render target and applies Panini projection as a post process.
void drawPaniniPostProcess(Cmd* cmd, uint32_t frameIdx)
{
  UNREF_PARAM(frameIdx);
	// transition world render target to be used as input texture in post process pass
	TextureBarrier barrier = { pWorldRenderTarget->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
	cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, false);

	// Set load actions to clear the screen to black
	LoadActionsDesc loadActions = {};
	loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
	loadActions.mClearColorValues[0] = pScreenRenderTarget->mDesc.mClearValue;
	cmdBeginRender(cmd, 1, &pScreenRenderTarget, NULL, &loadActions);

	cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mDesc.mWidth, (float)pScreenRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
	cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mDesc.mWidth, pScreenRenderTarget->mDesc.mHeight);

	DescriptorData params[2] = {};
	params[0].pName = "uTex";
	params[0].ppTextures = &pWorldRenderTarget->pTexture;
	params[1].pName = "PaniniRootConstants";
	params[1].pRootConstant = &gAppSettings.mPaniniSettings;
	cmdBindDescriptors(cmd, pRootSignaturePaniniPostProcess, 2, params);
	cmdBindPipeline(cmd, pPipielinePaniniPostProcess);

	const uint32_t numIndices = gPaniniDistortionTessellation[0] * gPaniniDistortionTessellation[1] * 6;
	cmdBindIndexBuffer(cmd, pIndexBufferTessellatedQuad);
	cmdBindVertexBuffer(cmd, 1, &pVertexBufferTessellatedQuad);
	cmdDrawIndexed(cmd, numIndices, 0);

	cmdEndRender(cmd, 1, &pScreenRenderTarget, NULL);
}

// Reads the depth buffer to apply ambient occlusion post process
void drawHDAO(Cmd* cmd, uint32_t frameIdx)
{
	struct HDAORootConstants
	{
		float2 g_f2RTSize;                  // Used by HDAO shaders for scaling texture coords
		float g_fHDAORejectRadius;          // HDAO param
		float g_fHDAOIntensity;             // HDAO param
		float g_fHDAOAcceptRadius;          // HDAO param
		float g_fQ;                         // far / (far - near)
		float g_fQTimesZNear;               // Q * near
	} data;

	const mat4& mainProj = gPerFrame[frameIdx].gPerFrameUniformData.transform[VIEW_CAMERA].projection;

	data.g_f2RTSize = { (float)pDepthBuffer->mDesc.mWidth, (float)pDepthBuffer->mDesc.mHeight };
	data.g_fHDAOAcceptRadius = gAppSettings.mAcceptRadius;
	data.g_fHDAOIntensity = gAppSettings.mAOIntensity;
	data.g_fHDAORejectRadius = gAppSettings.mRejectRadius;
	data.g_fQ = mainProj[2][2];
	data.g_fQTimesZNear = mainProj[3][2];

	cmdBeginRender(cmd, 1, &pRenderTargetAO, NULL, NULL);
	DescriptorData params[2] = {};
	params[0].pName = "g_txDepth";
	params[0].ppTextures = &pDepthBuffer->pTexture;
	params[1].pName = "HDAORootConstants";
	params[1].pRootConstant = &data;
	cmdBindDescriptors(cmd, pRootSignatureAO, 2, params);
	cmdBindPipeline(cmd, pPipelineAO[gAppSettings.mAOQuality - 1]);
	cmdDraw(cmd, 3, 0);
	cmdEndRender(cmd, 1, &pRenderTargetAO, NULL);
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

	cmdBeginRender(cmd, 1, &destRT, NULL, &loadActions);
	DescriptorData params[2] = {};
	params[0].pName = "msaaSource";
	params[0].ppTextures =  &msaaRT->pTexture;

	cmdBindDescriptors(cmd, pRootSignatureResolve, 1, params);
	cmdBindPipeline(cmd, gAppSettings.mEnablePaniniProjection ? pPipelineResolvePost : pPipelineResolve);
	cmdDraw(cmd, 3, 0);
	cmdEndRender(cmd, 1, &destRT, NULL);
}

// Executes a compute shader to clear (reset) the the light clusters on the GPU
void clearLightClusters(Cmd *cmd, uint32_t frameIdx)
{
  UNREF_PARAM(frameIdx);
	cmdBindPipeline(cmd, pPipelineClearLightClusters);

	DescriptorData params[1] = {};
	params[0].pName = "lightClustersCount";
	params[0].ppBuffers = &pLightClustersCount[frameIdx];
	cmdBindDescriptors(cmd, pRootSignatureClearLightClusters, 1, params);

	cmdDispatch(cmd, 1, 1, 1);
}

// Executes a compute shader that computes the light clusters on the GPU
void computeLightClusters(Cmd *cmd, uint32_t frameIdx)
{
  UNREF_PARAM(frameIdx);
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
//                           indicate the renderer the amount of vertices per batch to render.
#if defined(METAL)
void filterTriangles(Cmd *cmd, uint32_t frameIdx, uint32_t meshIdx, uint32_t currentBatch, FilterBatchChunk* batchChunk)
{
	// Check if there are batches to filter
	if (batchChunk->currentBatchCount == 0)
		return;

	// Select current batch data buffers to filter
	Buffer* batchDataBuffer = batchChunk->batchDataBuffer[meshIdx][currentBatch];

	// Copy batch data to GPU buffer
	BufferUpdateDesc dataBufferUpdate = { batchDataBuffer, batchChunk->batches, 0, 0, batchChunk->currentBatchCount * sizeof(FilterBatchData) };
	updateResource(&dataBufferUpdate, true);

	DescriptorData params[2] = {};
	params[0].pName = "perBatch";
	params[0].ppBuffers = &batchDataBuffer;
	params[1].pName = "perMeshUniforms";
	params[1].ppBuffers = &gPerBatchUniformBuffers[meshIdx];
	cmdBindDescriptors(cmd, pRootSignatureTriangleFiltering, 2, params);

	// This compute shader executes one thread per triangle and one group per batch
	//uint32_t numGroupsX = (pScene->meshes[i].triangleCount / pShaderTriangleFiltering->mNumThreadsPerGroup[0]) + 1;
	ASSERT(pShaderTriangleFiltering->mNumThreadsPerGroup[0] == CLUSTER_SIZE);
	cmdDispatch(cmd, batchChunk->currentBatchCount, 1, 1);

	// Reset batch chunk to start adding triangles to it
	batchChunk->currentBatchCount = 0;
	batchChunk->currentDrawCallCount = 0;
}
#else
void filterTriangles(Cmd *cmd, uint32_t frameIdx, FilterBatchChunk* batchChunk)
{
  UNREF_PARAM(frameIdx);
	// Check if there are batches to filter
	if (batchChunk->currentBatchCount == 0)
		return;

	uint32_t batchSize = batchChunk->currentBatchCount * sizeof(SmallBatchData);
	UniformBufferOffset offset = getUniformBufferOffset(batchChunk->pRingBuffer, batchSize);
	BufferUpdateDesc updateDesc = { offset.pUniformBuffer, batchChunk->batches, 0, offset.mOffset, batchSize };
	updateResource(&updateDesc, true);

	DescriptorData params[1] = {};
	params[0].pName = "batchData";
	params[0].mOffset = offset.mOffset;
	params[0].ppBuffers = &offset.pUniformBuffer;
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
		for (uint32_t i = 0; i<gNumViews; i++)
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

// This function decides how to do the triangle filtering pass, depending on the flags (hold, filter triangles)
// - filterTriangles: enables / disables triangle filtering at all. Disabling filtering makes the CPU to set the buffer states to render the whole scene.
// - hold: bypasses any triangle filtering step. This is useful to inspect the filtered geometry from another viewpoint.
// This function first performs CPU based cluster culling and only runs the fine-grained GPU-based tests for those clusters that passed the CPU test
void triangleFilteringPass(Cmd* cmd, GpuProfiler* pGpuProfiler, uint32_t frameIdx)
{
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Triangle Filtering Pass");

	gPerFrame[frameIdx].gTotalClusters = 0;
	gPerFrame[frameIdx].gCulledClusters = 0;

#if defined(METAL)
	/************************************************************************/
	// Clear previous indirect arguments
	/************************************************************************/
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers");
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
    
	cmdDispatch(cmd, numGroups * CLEAR_THREAD_COUNT, 1, 1);
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
	for (uint32_t i = 0; i < pScene->numMeshes; i++)
	{
		uint32_t currentClusterBatchThisMesh = 0;
		const Mesh* mesh = pScene->meshes + i;
		gPerFrame[frameIdx].gTotalClusters += mesh->clusterCount;
		for (uint32_t j = 0; j < mesh->clusterCount; j++)
		{
			const Cluster* cluster = mesh->clusters + j;

			// Perform CPU-based cluster culling before adding the cluster for GPU filtering
			if (cullCluster(cluster, gPerFrame[frameIdx].gEyeObjectSpace))
			{
				gPerFrame[frameIdx].gCulledClusters++;
				continue;
			}

			// The cluster was not culled: add cluster to the cluster batch chunk for the GPU filtering step
			addClusterToBatchChunk(cluster, mesh, pFilterBatchChunk[frameIdx]);

			// Check if we filled the whole batch of clusters
			if (pFilterBatchChunk[frameIdx]->currentBatchCount >= BATCH_COUNT)
			{
				// Run triangle filtering and switch to the next batch chunk
				filterTriangles(cmd, frameIdx, i, currentClusterBatchThisMesh, pFilterBatchChunk[frameIdx]);
				currentClusterBatchThisMesh++;
			}
		}

		// End of the mesh, filter the remaining clusters of the current batch
		filterTriangles(cmd, frameIdx, i, currentClusterBatchThisMesh, pFilterBatchChunk[frameIdx]);
	}
    
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
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers");
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
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Clear Buffers Synchronization");
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

	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Filter Triangles");
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

	for (uint32_t i = 0; i < pScene->numMeshes; ++i)
	{
		Mesh* drawBatch = &pScene->meshes[i];
		FilterBatchChunk* batchChunk = pFilterBatchChunk[frameIdx][currentSmallBatchChunk];
		for (uint32_t j = 0; j < drawBatch->clusterCount; ++j)
		{
			++gPerFrame[frameIdx].gTotalClusters;
			const Cluster* clusterInfo = &drawBatch->clusters[j];

			// Run cluster culling
			if (!gAppSettings.mClusterCulling || !cullCluster(clusterInfo, gPerFrame[frameIdx].gEyeObjectSpace))
			{
				// cluster culling passed or is turned off
				// We will now add the cluster to the batch to be triangle filtered
				addClusterToBatchChunk(clusterInfo, batchStart, accumDrawCount, accumNumTrianglesAtStartOfBatch, i, batchChunk);
				accumNumTriangles += clusterInfo->triangleCount;
			}
			else
				++gPerFrame[frameIdx].gCulledClusters;

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
	
	cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Batch Compaction");
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
	cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Shadow Pass");

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
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Color Pass");
		drawVisibilityBufferPass(cmd, pGraphicsGpuProfiler, frameIdx);
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);

		TextureBarrier barriers[] = {
			{ pRenderTargetVBPass->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			{ pRenderTargetShadow->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
			{ pDepthBuffer->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 3, barriers, true);
		cmdFlushBarriers(cmd);

		if (gAppSettings.mEnableHDAO)
		{
			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "HDAO Pass");
			drawHDAO(cmd, frameIdx);
			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
		}

		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Shading Pass");

		TextureBarrier aoBarrier = { pRenderTargetAO->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &aoBarrier, false);

		drawVisibilityBufferShade(cmd, frameIdx);

		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}
	else if (gAppSettings.mRenderMode == RENDERMODE_DEFERRED)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Color Pass");
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
			cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "HDAO Pass");
			drawHDAO(cmd, frameIdx);
			cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
		}

		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Shading Pass");

		TextureBarrier aoBarrier = { pRenderTargetAO->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 1, &aoBarrier, false);

		drawDeferredShade(cmd, frameIdx);

		if (gAppSettings.mRenderLocalLights)
		{
			drawDeferredShadePointLights(cmd, frameIdx);
		}
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}

	if (MSAASAMPLECOUNT > 1)
	{
		cmdBeginGpuTimestampQuery(cmd, pGraphicsGpuProfiler, "Resolve Pass");
		resolveMSAA(cmd, pRenderTargetMSAA, gAppSettings.mEnablePaniniProjection ? pWorldRenderTarget : pScreenRenderTarget);
		cmdEndGpuTimestampQuery(cmd, pGraphicsGpuProfiler);
	}
}

// Draw GUI / 2D elements
void drawGUI(Cmd* cmd, uint32_t frameIdx)
{
  UNREF_PARAM(frameIdx);
#if !defined(TARGET_IOS)
	cmdBeginRender(cmd, 1, &pScreenRenderTarget, NULL);
	cmdUIBeginRender(cmd, pUIManager, 1, &pScreenRenderTarget, NULL);

	gTimer.GetUSec(true);
	cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 15.0f), "CPU ", gTimer.GetUSecAverage() / 1000.0f);

	if (gAppSettings.mAsyncCompute)
	{
		// NOTE: Not sure whether this logic to calculate total gpu time is correct
		float time = max((float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f,
			(float)pComputeGpuProfiler->mCumulativeTime * 1000.0f);
		cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 40.0f), "GPU ", time);
        
#ifndef METAL
		if (gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
		{
			cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 65.0f), "Compute Queue ", (float)pComputeGpuProfiler->mCumulativeTime * 1000.0f);
			cmdUIDrawGpuProfileData(cmd, pUIManager, vec2(8.0f, 90.0f), pComputeGpuProfiler);
			cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 300.0f), "Graphics Queue ", (float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f);
			cmdUIDrawGpuProfileData(cmd, pUIManager, vec2(8.0f, 325.0f), pGraphicsGpuProfiler);
		}
		else
		{
			cmdUIDrawGpuProfileData(cmd, pUIManager, vec2(8.0f, 65.0f), pGraphicsGpuProfiler);
		}
#endif
	}
	else
	{
		cmdUIDrawFrameTime(cmd, pUIManager, vec2(8.0f, 40.0f), "GPU ", (float)pGraphicsGpuProfiler->mCumulativeTime * 1000.0f);
		cmdUIDrawGpuProfileData(cmd, pUIManager, vec2(8.0f, 65.0f), pGraphicsGpuProfiler);
	}

	// Draw Debug Textures
#if MSAASAMPLECOUNT == 1
	if (gAppSettings.mDrawDebugTargets)
	{
		float scale = 0.15f;

		if (gAppSettings.mRenderMode == RENDERMODE_VISBUFF)
		{
			RenderTarget* pVBRTs[] = { pRenderTargetVBPass, pRenderTargetAO };

			vec2 screenSize = { (float)pRenderTargetVBPass->mDesc.mWidth, (float)pRenderTargetVBPass->mDesc.mHeight };
			vec2 texSize = screenSize * scale;
			vec2 texPos = screenSize + vec2(0.0f, -texSize.getY());

			for (uint32_t i = 0; i < sizeof(pVBRTs) / sizeof(pVBRTs[0]); ++i)
			{
				texPos.setX(texPos.getX() - texSize.getX());
				cmdUIDrawTexturedQuad(cmd, pUIManager, texPos, texSize, pVBRTs[i]->pTexture);
			}

			screenSize = { (float)pRenderTargetVBPass->mDesc.mHeight, (float)pRenderTargetVBPass->mDesc.mHeight };
			texSize = screenSize * scale;
			texPos.setX(texPos.getX() - texSize.getX());
			cmdUIDrawTexturedQuad(cmd, pUIManager, texPos, texSize, pRenderTargetShadow->pTexture);
		}
		else
		{
			RenderTarget* pDeferredRTs[] = { pRenderTargetDeferredPass[0], pRenderTargetDeferredPass[1], pRenderTargetDeferredPass[2], pRenderTargetAO };

			vec2 screenSize = { (float)pDeferredRTs[0]->mDesc.mWidth, (float)pDeferredRTs[0]->mDesc.mHeight };
			vec2 texSize = screenSize * scale;
			vec2 texPos = screenSize + vec2(0.0f, -texSize.getY());

			for (uint32_t i = 0; i < sizeof(pDeferredRTs) / sizeof(pDeferredRTs[0]); ++i)
			{
				texPos.setX(texPos.getX() - texSize.getX());
				cmdUIDrawTexturedQuad(cmd, pUIManager, texPos, texSize, pDeferredRTs[i]->pTexture);
			}

			screenSize = { (float)pDeferredRTs[0]->mDesc.mHeight, (float)pDeferredRTs[0]->mDesc.mHeight };
			texSize = screenSize * scale;
			texPos.setX(texPos.getX() - texSize.getX());
			cmdUIDrawTexturedQuad(cmd, pUIManager, texPos, texSize, pRenderTargetShadow->pTexture);
		}
	}
#endif

#ifdef METAL
	// TODO: Remove text drawing from METAL after DrawGUI is fixed
	char buffer[128];
	sprintf(buffer, "SPACE - Hold triangle results - %s", gAppSettings.mHoldFilteredResults ? "On" : "Off");
	cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 65.0f), buffer);

	sprintf(buffer, "ENTER - Triangle filtering - %s", gAppSettings.mFilterTriangles ? "On" : "Off");
	cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 80.0f), buffer);

	sprintf(buffer, "BACKSPACE - Render Mode - %s", gAppSettings.mRenderMode == RENDERMODE_VISBUFF ? "Visibility Buffer" : "Deferred");
	cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 95.0f), buffer);

	sprintf(buffer, "L - Enable Random Point Lights - %s", gAppSettings.mRenderLocalLights ? "On" : "Off");
	cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 110.0f), buffer);

	sprintf(buffer, "T - Draw Debug Targets- %s", gAppSettings.mDrawDebugTargets ? "On" : "Off");
	cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 125.0f), buffer);
    
    sprintf(buffer, "P - Panini Projection- %s", gAppSettings.mEnablePaniniProjection ? "On" : "Off");
    cmdUIDrawText(cmd, pUIManager, vec2(8.0f, 140.0f), buffer);
#else
	cmdUIDrawGUI(cmd, pUIManager, pGuiWindow);
#endif

	cmdUIEndRender(cmd, pUIManager);

	cmdEndRender(cmd, 1, &pScreenRenderTarget, NULL);
#endif
}

// Process user keyboard input
void handleKeyboardInput(float deltaTime)
{
  UNREF_PARAM(deltaTime);
#if !defined(TARGET_IOS)
#ifdef _DURANGO
	// Pressing space holds / unholds triangle filtering results
	if (getJoystickButtonUp(BUTTON_A))
		gAppSettings.mHoldFilteredResults = !gAppSettings.mHoldFilteredResults;

	if (getJoystickButtonUp(BUTTON_B))
		gAppSettings.mFilterTriangles = !gAppSettings.mFilterTriangles;

	if (getJoystickButtonUp(BUTTON_X))
		gAppSettings.mRenderMode = (RenderMode)((gAppSettings.mRenderMode + 1) % RENDERMODE_COUNT);

	if (getJoystickButtonUp(BUTTON_Y))
		gAppSettings.mRenderLocalLights = !gAppSettings.mRenderLocalLights;
#else
	// Pressing space holds / unholds triangle filtering results
	if (getKeyUp(KEY_SPACE))
		gAppSettings.mHoldFilteredResults = !gAppSettings.mHoldFilteredResults;

	if (getKeyUp(KEY_ENTER))
		gAppSettings.mFilterTriangles = !gAppSettings.mFilterTriangles;

	if (getKeyUp(KEY_BACKSPACE))
		gAppSettings.mRenderMode = (RenderMode)((gAppSettings.mRenderMode + 1) % RENDERMODE_COUNT);

	if (getKeyUp(KEY_L))
		gAppSettings.mRenderLocalLights = !gAppSettings.mRenderLocalLights;

	if (getKeyUp(KEY_T))
		gAppSettings.mDrawDebugTargets = !gAppSettings.mDrawDebugTargets;
    
#if defined(METAL)
    if (getKeyUp(KEY_P))
        gAppSettings.mEnablePaniniProjection = !gAppSettings.mEnablePaniniProjection;
#endif
    
#endif
#endif
}

void updateDynamicUIElements()
{
	static bool gbWasPaniniEnabled = gAppSettings.mEnablePaniniProjection;
	static bool gWasAOEnabled = gAppSettings.mEnableHDAO;
	if (gAppSettings.mEnablePaniniProjection != gbWasPaniniEnabled)
	{
		gbWasPaniniEnabled = gAppSettings.mEnablePaniniProjection;
		if (gbWasPaniniEnabled)
		{
			gAppSettings.mDynamicUIControlsPanini.ShowDynamicProperties(pGuiWindow);
		}
		else
		{
			gAppSettings.mDynamicUIControlsPanini.HideDynamicProperties(pGuiWindow);
		}
	}
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
}

void update(float deltaTime)
{
	// Process user input
	handleKeyboardInput(deltaTime);

#if !defined(TARGET_IOS)
	pCameraController->update(deltaTime);
#endif

	updateUniformData(deltaTime);
	updateGui(pUIManager, pGuiWindow, deltaTime);

	updateDynamicUIElements();
}

// Main render method for the whole frame. This is the entry point for frame rendering and issues all
// all GPU commands for the current frame.
void drawFrame(float deltaTime)
{
    UNREF_PARAM(deltaTime);
	uint32_t graphicsFrameIdx = ~0u;

	if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
	{
		// Get the current render target for this frame
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, nullptr, &graphicsFrameIdx);

		// check to see if we can use the cmd buffer
		Fence* pNextFence = pRenderCompleteFences[(graphicsFrameIdx) % gImageCount];
		FenceStatus fenceStatus;
		getFenceStatus(pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence);
	}

	/************************************************************************/
	// Compute pass
	/************************************************************************/
	if (gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
	{
		uint32_t computeFrameIdx = (graphicsFrameIdx + 1) % gImageCount;

		// check to see if we can use the cmd buffer
		Fence* pNextComputeFence = pComputeCompleteFences[computeFrameIdx];
		FenceStatus fenceStatus;
		getFenceStatus(pNextComputeFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pComputeQueue, 1, &pNextComputeFence);

		// check to see if we can reuse the resoucres yet..
		// should be replaced with a semaphore
		Fence* pNextGraphicsFence = pRenderCompleteFences[computeFrameIdx];
		getFenceStatus(pNextGraphicsFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextGraphicsFence);
		/************************************************************************/
		// Update uniform buffer to gpu
		/************************************************************************/
		BufferUpdateDesc update = { pPerFrameUniformBuffers[computeFrameIdx], &gPerFrame[computeFrameIdx].gPerFrameUniformData, 0, 0, sizeof(PerFrameConstants) };
		updateResource(&update);
		/************************************************************************/
		// Triangle filtering async compute pass
		/************************************************************************/
		Cmd* computeCmd = ppComputeCmds[computeFrameIdx];

		beginCmd(computeCmd);
		cmdBeginGpuFrameProfile(computeCmd, pComputeGpuProfiler);

		triangleFilteringPass(computeCmd, pComputeGpuProfiler, computeFrameIdx);

		clearLightClusters(computeCmd, computeFrameIdx);

		if (gAppSettings.mRenderLocalLights)
		{
			// Update Light clusters on the GPU
			cmdBeginGpuTimestampQuery(computeCmd, pComputeGpuProfiler, "Compute Light Clusters");
			computeLightClusters(computeCmd, computeFrameIdx);
			cmdEndGpuTimestampQuery(computeCmd, pComputeGpuProfiler);
		}

		cmdEndGpuFrameProfile(computeCmd, pComputeGpuProfiler);
		endCmd(computeCmd);
		queueSubmit(pComputeQueue, 1, &computeCmd, pNextComputeFence, 1, &pRenderCompleteSemaphores[computeFrameIdx], 1, &pComputeCompleteSemaphores[computeFrameIdx]);
		/************************************************************************/
		/************************************************************************/
	}
	else
	{
		if (graphicsFrameIdx != -1)
		{
			BufferUpdateDesc update = { pPerFrameUniformBuffers[graphicsFrameIdx], &gPerFrame[graphicsFrameIdx].gPerFrameUniformData, 0, 0, sizeof(PerFrameConstants) };
			updateResource(&update);
		}
	}
	/************************************************************************/
	// Draw Pass
	/************************************************************************/
	if (!gAppSettings.mAsyncCompute || gFrameCount > 0)
	{
		Cmd* graphicsCmd = NULL;

		pScreenRenderTarget = pSwapChain->ppSwapchainRenderTargets[graphicsFrameIdx];
		// Get command list to store rendering commands for this frame
		graphicsCmd = ppCmds[graphicsFrameIdx];
		// Submit all render commands for this frame
		beginCmd(graphicsCmd);

		cmdBeginGpuFrameProfile(graphicsCmd, pGraphicsGpuProfiler);

		if (!gAppSettings.mAsyncCompute && gAppSettings.mFilterTriangles && !gAppSettings.mHoldFilteredResults)
		{
			triangleFilteringPass(graphicsCmd, pGraphicsGpuProfiler, graphicsFrameIdx);
		}

		if (!gAppSettings.mAsyncCompute)
			clearLightClusters(graphicsCmd, (graphicsFrameIdx + 1) % gImageCount);

		if (!gAppSettings.mAsyncCompute && gAppSettings.mRenderLocalLights)
		{
			// Update Light clusters on the GPU
			cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "Compute Light Clusters");
			computeLightClusters(graphicsCmd, (graphicsFrameIdx + 1) % gImageCount);
			cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);
		}

		// Transition swapchain buffer to be used as a render target
		TextureBarrier barriers[] = {
			{ pScreenRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pWorldRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pRenderTargetMSAA->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pDepthBuffer->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(graphicsCmd, 0, NULL, 4, barriers, true);

#ifndef METAL
		if (gAppSettings.mFilterTriangles)
		{
			const uint32_t numBarriers = (gNumGeomSets * gNumViews) + gNumViews + 1 + 2;
			uint32_t index = 0;
			BufferBarrier barriers2[numBarriers] = {};
			for (uint32_t i = 0; i < gNumViews; ++i)
			{
				barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[graphicsFrameIdx][GEOMSET_ALPHATESTED][i], RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
				barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[graphicsFrameIdx][GEOMSET_OPAQUE][i], RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
				barriers2[index++] = { pFilteredIndexBuffer[graphicsFrameIdx][i], RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
			}
			barriers2[index++] = { pFilterIndirectMaterialBuffer[graphicsFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
			barriers2[index++] = { pLightClusters[graphicsFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
			barriers2[index++] = { pLightClustersCount[graphicsFrameIdx], RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(graphicsCmd, numBarriers, barriers2, 0, NULL, true);
		}
#endif

		drawScene(graphicsCmd, graphicsFrameIdx);

#ifdef _DURANGO
		// When async compute is on, we need to transition some resources in the graphics queue
		// because they can't be transitioned by the compute queue (incompatible)
		if (gAppSettings.mAsyncCompute)
			setResourcesToComputeCompliantState(graphicsFrameIdx, false);
#else
#ifndef METAL
		if (gAppSettings.mFilterTriangles)
		{
			const uint32_t numBarriers = (gNumGeomSets * gNumViews) + gNumViews + 1 + 2;
			uint32_t index = 0;
			BufferBarrier barriers2[numBarriers] = {};
			for (uint32_t i = 0; i < gNumViews; ++i)
			{
				barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[graphicsFrameIdx][GEOMSET_ALPHATESTED][i], RESOURCE_STATE_UNORDERED_ACCESS };
				barriers2[index++] = { pFilteredIndirectDrawArgumentsBuffer[graphicsFrameIdx][GEOMSET_OPAQUE][i], RESOURCE_STATE_UNORDERED_ACCESS };
				barriers2[index++] = { pFilteredIndexBuffer[graphicsFrameIdx][i], RESOURCE_STATE_UNORDERED_ACCESS };
			}
			barriers2[index++] = { pFilterIndirectMaterialBuffer[graphicsFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
			barriers2[index++] = { pLightClusters[graphicsFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
			barriers2[index++] = { pLightClustersCount[graphicsFrameIdx], RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(graphicsCmd, numBarriers, barriers2, 0, NULL, true);
		}
#endif
#endif

		if (gAppSettings.mEnablePaniniProjection)
		{
			cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "Panini Projection Pass");
			drawPaniniPostProcess(graphicsCmd, graphicsFrameIdx);
			cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);
		}

		cmdBeginGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler, "UI Pass");
		drawGUI(graphicsCmd, graphicsFrameIdx);
		cmdEndGpuTimestampQuery(graphicsCmd, pGraphicsGpuProfiler);

		barriers[0].mNewState = RESOURCE_STATE_PRESENT;
		cmdResourceBarrier(graphicsCmd, 0, NULL, 1, barriers, true);

		cmdEndGpuFrameProfile(graphicsCmd, pGraphicsGpuProfiler);

		endCmd(graphicsCmd);

		if (gAppSettings.mAsyncCompute)
		{
			// Submit all the work to the GPU and present
			Semaphore* pWaitSemaphores[] = { pImageAcquiredSemaphore, pComputeCompleteSemaphores[graphicsFrameIdx] };
			queueSubmit(pGraphicsQueue, 1, &graphicsCmd, pRenderCompleteFences[graphicsFrameIdx], 2, pWaitSemaphores, 1, &pRenderCompleteSemaphores[graphicsFrameIdx]);
		}
		else
		{
			queueSubmit(pGraphicsQueue, 1, &graphicsCmd, pRenderCompleteFences[graphicsFrameIdx], 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphores[graphicsFrameIdx]);
		}

		Semaphore* pWaitSemaphores[] = { pRenderCompleteSemaphores[graphicsFrameIdx] };
		queuePresent(pGraphicsQueue, pSwapChain, graphicsFrameIdx, 1, pWaitSemaphores);
	}
}

bool load()
{
	addSwapChain();
	addRenderTargets();
	return true;
}

void unload()
{
	removeRenderTargets();
	removeSwapChain();
}

#ifndef __APPLE__
int main(int argc, char **argv)
{
  UNREF_PARAM(argc);
	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	HiresTimer deltaTimer;

	RectDesc resoultion;
	getRecommendedResolution(&resoultion);

	gAppName = FileSystem::GetFileName(argv[0]);
	gWindow.windowedRect = resoultion;
	gWindow.fullscreenRect = resoultion;
	gWindow.fullScreen = false;
	gWindow.maximized = false;
	openWindow(gAppName, &gWindow);

	if (!initApp())
		return -1;

	registerWindowResizeEvent(onWindowResize);
#ifndef _DURANGO
	registerMouseMoveEvent(onMouseMoveHandler);
	registerMouseButtonEvent(onMouseButtonHandler);
	registerMouseWheelEvent(onMouseWheelHandler);
#endif

	while (isRunning())
	{
		float deltaTime = deltaTimer.GetSeconds(true);
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		handleMessages();
		update(deltaTime);
		drawFrame(deltaTime);
		++gFrameCount;
	}

	exitApp();
	closeWindow(&gWindow);

	return EXIT_SUCCESS;
}
#else
    
#import "MetalKitApplication.h"
    
// Timer used in the update function.
Timer deltaTimer;
float retinaScale = 1.0f;

// Metal application implementation.
@implementation MetalKitApplication {}
-(nonnull instancetype) initWithMetalDevice:(nonnull id<MTLDevice>)device
                  renderDestinationProvider:(nonnull id<RenderDestinationProvider>)renderDestinationProvider
                                       view:(nonnull MTKView*)view
                        retinaScalingFactor:(CGFloat)retinaScalingFactor
{
    self = [super init];
    if(self)
    {
        FileSystem::SetCurrentDir(FileSystem::GetProgramDir());
        
        retinaScale = retinaScalingFactor;
        
        RectDesc resolution;
        getRecommendedResolution(&resolution);
        
        gSwapchainWidth = resolution.right;
        gSwapchainHeight = resolution.bottom;
        
        gWindow.windowedRect = resolution;
        gWindow.fullscreenRect = resolution;
        gWindow.fullScreen = false;
        gWindow.maximized = false;
        gWindow.handle = (void*)CFBridgingRetain(view);
        
        @autoreleasepool {
            const char * appName = "Visibiliy Buffer";
            openWindow(appName, &gWindow);
            initApp();
        }
        
        registerMouseMoveEvent(onMouseMoveHandler);
        registerMouseButtonEvent(onMouseButtonHandler);
        registerMouseWheelEvent(onMouseWheelHandler);
    }
    
    return self;
}

- (void)drawRectResized:(CGSize)size
{
    waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences);
    gFrameCount = 0;
    
    gSwapchainWidth = size.width * retinaScale;
    gSwapchainHeight = size.height * retinaScale;
    
    unload();
    load();
}

- (void)update
{
    float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
    // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
    if (deltaTime > 0.15f)
        deltaTime = 0.05f;
    
    update(deltaTime);
    drawFrame(deltaTime);
    ++gFrameCount;
}
@end

#endif
