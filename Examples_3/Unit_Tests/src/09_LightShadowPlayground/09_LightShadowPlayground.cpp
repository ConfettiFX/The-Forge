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

#define SPHERE_EACH_ROW 5                                     //MUST MATCH with the same macro in all shaders
#define SPHERE_EACH_COL 5                                     //MUST MATCH with the same macro in all shaders
#define SPHERE_NUM (SPHERE_EACH_ROW * SPHERE_EACH_COL + 1)    // Must match with same macro in all shader.... +1 for plane

#if defined(METAL)
#define ESM_MSAA_SAMPLES 2
#define COPY_BUFFER_WORKGROUP 16
#define ESM_SHADOWMAP_RES 1024u
#else
#define ESM_MSAA_SAMPLES 8
#define COPY_BUFFER_WORKGROUP 32
#define ESM_SHADOWMAP_RES 2048u
#endif

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
//GPU Profiler
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//input
#include "../../../../Common_3/OS/Input/InputSystem.h"
#include "../../../../Common_3/OS/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

enum ShadowType
{
	SHADOW_TYPE_NONE,
	SHADOW_TYPE_ESM,    //Exponential Shadow Map
	SHADOW_TYPE_SDF,    //Ray-Traced Signed Distance Fields Shadow

	SHADOW_TYPE_COUNT
};
typedef struct RenderSettingsUniformData
{
	vec4 mWindowDimension = { 1, 1, 0, 0 };    //only first two are used to represents window width and height, z and w are paddings
	uint32_t mShadowType = SHADOW_TYPE_SDF;    //we start with ESM and can switch using GUI dropdown menu

} RenderSettingsUniformData;

enum Projections
{
	MAIN_CAMERA,    // primary view
	SHADOWMAP,

	PROJECTION_COUNT
};
typedef struct ObjectInfoStruct
{
	vec4 mColor;
	vec3 mTranslation;
	mat4 mTranslationMat;
	mat4 mScaleMat;
} ObjectInfoStruct;

typedef struct ObjectInfoUniformBlock
{
	mat4 mWorldViewProjMat[SPHERE_NUM];
	mat4 mWorldMat[SPHERE_NUM];
	vec4 mWorldBoundingSphereRadius[SPHERE_NUM];
} ObjectInfoUniformBlock;

typedef struct LightUniformBlock
{
	mat4  mLightViewProj;
	vec4  mLightPosition;
	vec4  mLightColor = { 1, 0, 0, 1 };
	vec4  mLightUpVec;
	float mLightRange;
	float padding[3];
} LightUniformBlock;

typedef struct CameraUniform
{
	mat4  mView;
	mat4  mProject;
	mat4  mViewProject;
	mat4  mInvView;
	mat4  mInvProj;
	mat4  mInvViewProject;
	vec4  NDCConversionConstants[2];
	float mNear;
	float mFarNearDiff;
	float mFarNear;
	float paddingForAlignment0;
} CameraUniform;

typedef struct ESMInputConstants
{
	float mNear = 1.f;
	float mFar = 180.f;
	float mFarNearDiff = mFar - mNear;
	float mNear_Over_FarNearDiff = mNear / mFarNearDiff;
	float mExponent = 346.0f;
	uint  mBlurWidth = 1U;
	int   padding[2];
} ESMInputConstants;

typedef struct SdfInputConstants
{
	float    mShadowHardness = 2.f;
	float    mShadowHardnessRcp = 1.f / mShadowHardness;
	uint32_t mMaxIteration = 64U;
	uint32_t padding;
} SdfInputUniformBlock;

constexpr uint32_t gImageCount = 3;

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pRenderTargetScreen;
RenderTarget* pRenderTargetDepth = NULL;
Texture*      pDepthCopyTexture = NULL;
RenderTarget* pRenderTargetShadowMap;
RenderTarget* pRenderTargetSdfSimple;
Texture*      pBlurredESM;

/************************************************************************/
Buffer* pBufferSphereVertex = NULL;
Buffer* pBufferBoxIndex = NULL;
// Warning these indices are not good indices for cubes that want correct normals
// (a.k.a. all vertices are shared)
const uint16_t gBoxIndices[36] = {
	0, 1, 4, 4, 1, 5,    //y-
	0, 4, 2, 2, 4, 6,    //x-
	0, 2, 1, 1, 2, 3,    //z-
	2, 6, 3, 3, 6, 7,    //y+
	1, 3, 5, 5, 3, 7,    //x+
	4, 5, 6, 6, 5, 7     //z+
};
/************************************************************************/
// Z-pass shader
/************************************************************************/
Shader* pShaderZPass = NULL;
/************************************************************************/
// Z-prepass Shader Pack
/************************************************************************/
Pipeline*         pPipelineZPrepass = NULL;
RootSignature*    pRootSignatureZprepass = NULL;
/************************************************************************/
// Forward Shade Shader pack
/************************************************************************/
Shader*           pShaderForwardPass = NULL;
Pipeline*         pPipelineForwardShadeSrgb = NULL;
RootSignature*    pRootSignatureForwardPass = NULL;
/************************************************************************/
// SDF Shader pack
/************************************************************************/
Shader*           pShaderSdfSphere = NULL;
Pipeline*         pPipelineSdfSphere = NULL;
RootSignature*    pRootSignatureSdfSphere = NULL;
/************************************************************************/
// Skybox Shader Pack
/************************************************************************/
Shader*           pShaderSkybox = NULL;
Pipeline*         pPipelineSkybox = NULL;
RootSignature*    pRootSignatureSkybox = NULL;
/************************************************************************/
// Shadow Shader Pack
/************************************************************************/
Pipeline*         pPipelineShadowPass = NULL;
RootSignature*    pRootSignatureShadowPass = NULL;
/************************************************************************/
// ESM Compute Blur Shader Pack
/************************************************************************/
Shader*           pShaderESMBlur = NULL;
Pipeline*         pPipelineESMBlur = NULL;
RootSignature*    pRootSignatureESMBlur = NULL;
/************************************************************************/
// Depth buffer copy compute shader
/************************************************************************/
Shader*           pShaderCopyBuffer = NULL;
Pipeline*         pPipelineCopyBuffer = NULL;
RootSignature*    pRootSignatureCopyBuffer = NULL;

/************************************************************************/
// Descriptor Binder
/************************************************************************/
DescriptorBinder* pDescriptorBinder = NULL;

/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerMiplessSampler = NULL;
Sampler* pSamplerTrilinearAniso = NULL;

/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullFront = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;

/************************************************************************/
// Blend states
/************************************************************************/
BlendState* pBlendStateSDF = NULL;

/************************************************************************/
// Constant buffers
/************************************************************************/
Buffer* pBufferObjectTransforms[PROJECTION_COUNT][gImageCount] = { { NULL }, { NULL } };
Buffer* pBufferLightUniform[gImageCount] = { NULL };
Buffer* pBufferESMBlurUniform[gImageCount] = { NULL };
Buffer* pBufferRenderSettings[gImageCount] = { NULL };
Buffer* pBufferCameraUniform[gImageCount] = { NULL };
Buffer* pBufferSdfInputUniform[gImageCount] = { NULL };

/************************************************************************/
// Shader Storage Buffers
/************************************************************************/
Buffer* pBufferESMBlurIntermediate = NULL;

/************************************************************************/
// Depth State
/************************************************************************/
DepthState* pDepthStateEnable = NULL;
DepthState* pDepthStateTestOnly = NULL;
DepthState* pDepthStateStencilShadow = NULL;

/************************************************************************/
// Textures
/************************************************************************/
Texture* pTextureSkybox;
Texture* pTextureScene[2];
/************************************************************************/
// Render control variables
/************************************************************************/
struct
{
	uint32 mFilterWidth = 2U;
} gEsmCpuSettings;

struct
{
	float3 mLightPosition = { -5, 30, -5 };    //light position, will be changed by GUI editor if not iOS
} gLightCpuSettings;

/************************************************************************/

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

// Constants
uint32_t                  gFrameIndex = 0;
GpuProfiler*              pGpuProfiler = NULL;
RenderSettingsUniformData gRenderSettings;

int                    gNumberOfSpherePoints;
ObjectInfoUniformBlock gObjectInfoUniformData[PROJECTION_COUNT];
LightUniformBlock      gLightUniformData;
SdfInputUniformBlock   gSdfUniformData;
CameraUniform          gCameraUniformData;
ESMInputConstants      gESMBlurUniformData;

ObjectInfoStruct gObjectInfoData[SPHERE_NUM];
vec3             gObjectsCenter = { 0, 0, 0 };

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

/// UI
UIApp         gAppUI;
GuiComponent* pGuiWindow = NULL;
TextDrawDesc  gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);

FileSystem gFileSystem;
LogManager gLogManager;

const int   gSphereResolution = 120;    // Increase for higher resolution spheres
const float gSphereRadius = 1.33f;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

const char* pSceneFileNames[] = {
	"Warehouse-with-lights.tga",
	"rect.tga",
};

const char* pszBases[FSR_Count] = {
	"../../../src/09_LightShadowPlayground/",    // FSR_BinShaders
	"../../../src/09_LightShadowPlayground/",    // FSR_SrcShaders
	"../../../UnitTestResources/",               // FSR_Textures
	"../../../UnitTestResources/",               // FSR_Meshes
	"../../../UnitTestResources/",               // FSR_Builtin_Fonts
	"../../../src/09_LightShadowPlayground/",    // FSR_GpuConfig
	"",                                          // FSR_Animation
	"",                                          // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",         // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",           // FSR_MIDDLEWARE_UI
};

static void createScene()
{
	/************************************************************************/
	// Initialize Objects
	/************************************************************************/
	const float sphereRadius = gSphereRadius;
	const float sphereDist = 3.0f * sphereRadius;
	int         sphereIndex = 0;

	vec3 curTrans = { -sphereDist * (SPHERE_EACH_ROW - 1) / 2.f, sphereRadius * 2.3f, -sphereDist * (SPHERE_EACH_COL - 1) / 2.f };

	for (int i = 0; i < SPHERE_EACH_ROW; ++i)
	{
		curTrans.setX(-sphereDist * (SPHERE_EACH_ROW - 1) / 2.f);

		for (int j = 0; j < SPHERE_EACH_COL; j++)
		{
			gObjectInfoData[sphereIndex].mTranslation = curTrans;
			gObjectInfoData[sphereIndex].mScaleMat = mat4::scale(vec3(sphereRadius));
			gObjectInfoData[sphereIndex].mColor = vec4(1, 1, 1, 1);
			sphereIndex++;

			curTrans.setX(curTrans.getX() + sphereDist);
		}

		curTrans.setZ(curTrans.getZ() + sphereDist);
	}

	gObjectsCenter = vec3(0, 0, 0);

	gObjectInfoData[SPHERE_NUM - 1].mTranslation = { 0.f, 0.f, 0.f };
	gObjectInfoData[SPHERE_NUM - 1].mScaleMat =
		mat4::scale(vec3(sphereDist * SPHERE_EACH_ROW / 0.9f, 1.f, sphereDist * SPHERE_EACH_COL / 0.9f));
	gObjectInfoData[SPHERE_NUM - 1].mColor = vec4(1, 1, 1, 1);

	for (int i = 0; i < SPHERE_NUM; i++)
	{
		gObjectInfoData[i].mTranslationMat = gObjectInfoData[i].mTranslationMat.translation(gObjectInfoData[i].mTranslation);
	}
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
struct GuiController
{
	static void addGui();
	static void updateDynamicUI();

	static DynamicUIWidgets esmDynamicWidgets;
	static DynamicUIWidgets sdfDynamicWidgets;

	static ShadowType currentlyShadowType;
};
ShadowType        GuiController::currentlyShadowType;
DynamicUIWidgets GuiController::esmDynamicWidgets;
DynamicUIWidgets GuiController::sdfDynamicWidgets;
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
class LightShadowPlayground: public IApp
{
	public:
	LightShadowPlayground()
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}
	
	bool Init() override
	{
		RendererDesc settings = { NULL };
		initRenderer(GetName(), &settings, &pRenderer);

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		DepthStateDesc depthStateEnabledDesc = {};
		depthStateEnabledDesc.mDepthFunc = CMP_GEQUAL;
		depthStateEnabledDesc.mDepthWrite = true;
		depthStateEnabledDesc.mDepthTest = true;

		DepthStateDesc depthStateTestOnlyDesc = {};
		depthStateTestOnlyDesc.mDepthFunc = CMP_EQUAL;
		depthStateTestOnlyDesc.mDepthWrite = false;
		depthStateTestOnlyDesc.mDepthTest = true;

		DepthStateDesc depthStateStencilShadow = {};
		depthStateStencilShadow.mDepthFunc = CMP_LESS;
		depthStateStencilShadow.mDepthWrite = false;
		depthStateStencilShadow.mDepthTest = true;

		#ifdef STENCIL_SDF_OPTIMIZATION
		depthStateStencilShadow.mStencilTest = true;
		/* @REMARK: The discard kills our early-Z but we use early_fragment_tests to make sure that the shader won't even start executing
		 * for all pixels covering the light volume (empty space in mid-air), let alone try to write out its results via a blend in the ROP.
		 *
		 * It uses a nasty vertex shader trick... the front layer of polygons of the bounding shape are guaranteed to be drawn first.
		 * Only feasible with simple bounding geometries such as spheres and boxes.
		 *
		 * If anyone knows a better way to draw stencil shadows from multiple overlapping instanced volumes (without breaking the instancing),
		 * feel free to correct this.
		 */
		depthStateStencilShadow.mStencilReadMask = 0xffu;
		depthStateStencilShadow.mStencilWriteMask = 0xffu;
		depthStateStencilShadow.mStencilFrontFunc = CMP_EQUAL;
		depthStateStencilShadow.mStencilFrontPass = STENCIL_OP_INCR;
		depthStateStencilShadow.mStencilBackFunc = CMP_EQUAL;
		depthStateStencilShadow.mStencilBackFail = STENCIL_OP_REPLACE;
		#else
		depthStateStencilShadow.mStencilTest = false;
		#endif

		addDepthState(pRenderer, &depthStateEnabledDesc, &pDepthStateEnable);
		addDepthState(pRenderer, &depthStateTestOnlyDesc, &pDepthStateTestOnly);
		addDepthState(pRenderer, &depthStateStencilShadow, &pDepthStateStencilShadow);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer);

		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = false;
			textureDesc.pFilename = "skybox/hw_sahara/sahara_cubemap.dds";
			textureDesc.ppTexture = &pTextureSkybox;
			addResource(&textureDesc, true);
		}
		for (int i = 0; i < 2; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSceneFileNames[i];
			textureDesc.ppTexture = &pTextureScene[i];
			addResource(&textureDesc, true);
		}

		/************************************************************************/
		// Geometry data for the scene
		/************************************************************************/
		float* pSpherePoints;
		generateSpherePoints(&pSpherePoints, &gNumberOfSpherePoints, gSphereResolution, gSphereRadius);

		uint64_t       sphereDataSize = gNumberOfSpherePoints * sizeof(float);
		BufferLoadDesc sphereVbDesc = {};
		sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		sphereVbDesc.mDesc.mSize = sphereDataSize;
		// despite mVertexStride being specified, it doesn't have the intended effect, see the FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA ifdefs
		sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		sphereVbDesc.pData = pSpherePoints;
		sphereVbDesc.ppBuffer = &pBufferSphereVertex;
		addResource(&sphereVbDesc);
		// Need to free memory;
		conf_free(pSpherePoints);

		BufferLoadDesc boxIbDesc = {};
		boxIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		boxIbDesc.mDesc.mIndexType = INDEX_TYPE_UINT16;
		boxIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boxIbDesc.mDesc.mSize = sizeof(gBoxIndices);
		boxIbDesc.pData = gBoxIndices;
		boxIbDesc.ppBuffer = &pBufferBoxIndex;
		addResource(&boxIbDesc);
		/************************************************************************/
		// Setup constant buffer data
		/************************************************************************/
		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(ObjectInfoUniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t j = 0; j < PROJECTION_COUNT; j++)
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				ubDesc.ppBuffer = &pBufferObjectTransforms[j][i];
				addResource(&ubDesc);
			}
		BufferLoadDesc ubEsmBlurDesc = {};
		ubEsmBlurDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubEsmBlurDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubEsmBlurDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubEsmBlurDesc.mDesc.mSize = sizeof(ESMInputConstants);
		ubEsmBlurDesc.pData = &gESMBlurUniformData;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubEsmBlurDesc.ppBuffer = &pBufferESMBlurUniform[i];
			addResource(&ubEsmBlurDesc);
		}

		BufferLoadDesc sdfDesc = {};
		sdfDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		sdfDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		sdfDesc.mDesc.mSize = sizeof(SdfInputUniformBlock);
		sdfDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		sdfDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			sdfDesc.ppBuffer = &pBufferSdfInputUniform[i];
			addResource(&sdfDesc);
		}

		BufferLoadDesc camUniDesc = {};
		camUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		camUniDesc.mDesc.mSize = sizeof(CameraUniform);
		camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		camUniDesc.pData = &gCameraUniformData;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			camUniDesc.ppBuffer = &pBufferCameraUniform[i];
			addResource(&camUniDesc);
		}
		BufferLoadDesc renderSettingsDesc = {};
		renderSettingsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		renderSettingsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		renderSettingsDesc.mDesc.mSize = sizeof(RenderSettingsUniformData);
		renderSettingsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		renderSettingsDesc.pData = &gRenderSettings;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			renderSettingsDesc.ppBuffer = &pBufferRenderSettings[i];
			addResource(&renderSettingsDesc);
		}

		BufferLoadDesc lightUniformDesc = {};
		lightUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightUniformDesc.mDesc.mSize = sizeof(LightUniformBlock);
		lightUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightUniformDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			lightUniformDesc.ppBuffer = &pBufferLightUniform[i];
			addResource(&lightUniformDesc);
		}

		/************************************************************************/
		// Setup shader storage buffers
		/************************************************************************/
		BufferLoadDesc esmImdtSSBDesc = {};
		esmImdtSSBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER;
		esmImdtSSBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		esmImdtSSBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		esmImdtSSBDesc.mDesc.mStructStride = sizeof(float);
		esmImdtSSBDesc.mDesc.mElementCount = ESM_SHADOWMAP_RES * ESM_SHADOWMAP_RES;
		esmImdtSSBDesc.mDesc.mSize = esmImdtSSBDesc.mDesc.mStructStride * esmImdtSSBDesc.mDesc.mElementCount;
		esmImdtSSBDesc.ppBuffer = &pBufferESMBlurIntermediate;
		addResource(&esmImdtSSBDesc);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		constexpr size_t GlobalMacroCount = 6u;
		ShaderMacro      macros[GlobalMacroCount];
		macros[0].definition = "SPHERE_EACH_ROW";
		macros[0].value = tinystl::string::format("%d", SPHERE_EACH_ROW);
		macros[1].definition = "SPHERE_EACH_COL";
		macros[1].value = tinystl::string::format("%d", SPHERE_EACH_COL);
		macros[2].definition = "SPHERE_NUM";
		macros[2].value = tinystl::string::format("%d", SPHERE_EACH_ROW * SPHERE_EACH_COL + 1);
		macros[3].definition = "ESM_SHADOWMAP_RES";
		macros[3].value = tinystl::string::format("%d", ESM_SHADOWMAP_RES);
		macros[4].definition = "ESM_MSAA_SAMPLES";
		macros[4].value = tinystl::string::format("%d", ESM_MSAA_SAMPLES);
		macros[5].definition = "WORKGROUP_SIZE";
		macros[5].value = tinystl::string::format("%d", COPY_BUFFER_WORKGROUP);

		ShaderLoadDesc forwardPassShaderDesc = {};
		forwardPassShaderDesc.mStages[0] = { "forwardSphere.vert", macros, GlobalMacroCount, FSR_SrcShaders };
		forwardPassShaderDesc.mStages[1] = { "forwardSphere.frag", macros, GlobalMacroCount, FSR_SrcShaders };
		ShaderLoadDesc zPassShaderDesc = {};
		zPassShaderDesc.mStages[0] = { "zPass.vert", macros, GlobalMacroCount, FSR_SrcShaders };
		//zPassShaderDesc.mStages[1] = { "zPass.frag", macros, GlobalMacroCount, FSR_SrcShaders }; // no need for a z-pass pixel shader if there is no alpha testing in the scene

		ShaderLoadDesc esmBlurShaderDesc = {};
		esmBlurShaderDesc.mStages[0] = { "blurESM.comp", macros, GlobalMacroCount, FSR_SrcShaders };
		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "fsTriangle.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", macros, GlobalMacroCount, FSR_SrcShaders };
		ShaderLoadDesc sdfSphereShaderDesc = {};
		sdfSphereShaderDesc.mStages[0] = { "sdfBBox.vert", macros, GlobalMacroCount, FSR_SrcShaders };
		sdfSphereShaderDesc.mStages[1] = { "sdfSphere.frag", macros, GlobalMacroCount, FSR_SrcShaders };
		ShaderLoadDesc bufferCopyShaderDesc = {};
		bufferCopyShaderDesc.mStages[0] = { "copyBuffer.comp", macros, GlobalMacroCount, FSR_SrcShaders };
		/************************************************************************/
		// Add shaders
		/************************************************************************/
		addShader(pRenderer, &esmBlurShaderDesc, &pShaderESMBlur);
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
		addShader(pRenderer, &zPassShaderDesc, &pShaderZPass);
		addShader(pRenderer, &forwardPassShaderDesc, &pShaderForwardPass);
		addShader(pRenderer, &sdfSphereShaderDesc, &pShaderSdfSphere);
		addShader(pRenderer, &bufferCopyShaderDesc, &pShaderCopyBuffer);

		/************************************************************************/
		// Add GPU profiler
		/************************************************************************/
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		/************************************************************************/
		// Add samplers
		/************************************************************************/
		SamplerDesc clampMiplessSamplerDesc = {};
		clampMiplessSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		clampMiplessSamplerDesc.mMinFilter = FILTER_LINEAR;
		clampMiplessSamplerDesc.mMagFilter = FILTER_LINEAR;
		clampMiplessSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		clampMiplessSamplerDesc.mMipLosBias = 0.0f;
		clampMiplessSamplerDesc.mMaxAnisotropy = 0.0f;
		addSampler(pRenderer, &clampMiplessSamplerDesc, &pSamplerMiplessSampler);

		SamplerDesc samplerTrilinearAnisoDesc = {};
		samplerTrilinearAnisoDesc.mAddressU = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressV = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mAddressW = ADDRESS_MODE_REPEAT;
		samplerTrilinearAnisoDesc.mMinFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMagFilter = FILTER_LINEAR;
		samplerTrilinearAnisoDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		samplerTrilinearAnisoDesc.mMipLosBias = 0.0f;
		samplerTrilinearAnisoDesc.mMaxAnisotropy = 8.0f;
		addSampler(pRenderer, &samplerTrilinearAnisoDesc, &pSamplerTrilinearAniso);

		/************************************************************************/
		// setup root signitures
		/************************************************************************/
		RootSignatureDesc zPrepassRootDesc = {};
		zPrepassRootDesc.ppShaders = &pShaderZPass;
		zPrepassRootDesc.mShaderCount = 1;
		zPrepassRootDesc.mStaticSamplerCount = 0;

		RootSignatureDesc shadowMapPassRootDesc = {};
		shadowMapPassRootDesc.ppShaders = &pShaderZPass;
		shadowMapPassRootDesc.mShaderCount = 1;
		shadowMapPassRootDesc.mStaticSamplerCount = 0;

		RootSignatureDesc forwardPassRootDesc = {};
		forwardPassRootDesc.ppShaders = &pShaderForwardPass;
		forwardPassRootDesc.mShaderCount = 1;
		Sampler* forwardShadeSamplers[] = { pSamplerTrilinearAniso, pSamplerMiplessSampler };
		forwardPassRootDesc.ppStaticSamplers = forwardShadeSamplers;
		forwardPassRootDesc.mStaticSamplerCount = 2;
		const char* forwardPassSamplersNames[] = { "textureSampler", "clampMiplessSampler" };
		forwardPassRootDesc.ppStaticSamplerNames = forwardPassSamplersNames;

		RootSignatureDesc skyboxRootDesc = {};
		skyboxRootDesc.ppShaders = &pShaderSkybox;
		skyboxRootDesc.mShaderCount = 1;
		skyboxRootDesc.ppStaticSamplers = &pSamplerMiplessSampler;
		skyboxRootDesc.mStaticSamplerCount = 1;
		const char* skyboxSamplersNames[] = { "skySampler" };
		skyboxRootDesc.ppStaticSamplerNames = skyboxSamplersNames;

		RootSignatureDesc sdfSphereRootDesc = {};
		sdfSphereRootDesc.ppShaders = &pShaderSdfSphere;
		sdfSphereRootDesc.mShaderCount = 1;
		sdfSphereRootDesc.ppStaticSamplers = &pSamplerMiplessSampler;
		sdfSphereRootDesc.mStaticSamplerCount = 1;
		const char* sdfSimpleRootSamplerNames[] = { "clampMiplessSampler" };
		sdfSphereRootDesc.ppStaticSamplerNames = sdfSimpleRootSamplerNames;

		addRootSignature(pRenderer, &zPrepassRootDesc, &pRootSignatureZprepass);
		addRootSignature(pRenderer, &forwardPassRootDesc, &pRootSignatureForwardPass);
		addRootSignature(pRenderer, &shadowMapPassRootDesc, &pRootSignatureShadowPass);
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);
		addRootSignature(pRenderer, &sdfSphereRootDesc, &pRootSignatureSdfSphere);

		RootSignatureDesc esmBlurShaderRootDesc = {};
		esmBlurShaderRootDesc.ppShaders = &pShaderESMBlur;
		esmBlurShaderRootDesc.mShaderCount = 1;
		esmBlurShaderRootDesc.ppStaticSamplers = &pSamplerMiplessSampler;
		esmBlurShaderRootDesc.mStaticSamplerCount = 1;
		const char* esmblurShaderRootSamplerNames[] = {
			"clampMiplessSampler",
		};
		esmBlurShaderRootDesc.ppStaticSamplerNames = esmblurShaderRootSamplerNames;
		addRootSignature(pRenderer, &esmBlurShaderRootDesc, &pRootSignatureESMBlur);

		RootSignatureDesc bufferCopyShaderRootDesc = {};
		bufferCopyShaderRootDesc.ppShaders = &pShaderCopyBuffer;
		bufferCopyShaderRootDesc.mShaderCount = 1;
		bufferCopyShaderRootDesc.ppStaticSamplers = &pSamplerMiplessSampler;
		bufferCopyShaderRootDesc.mStaticSamplerCount = 1;
		const char* bufferCopyShaderRootSamplerNames[] = {
			"clampMiplessSampler",
		};
		bufferCopyShaderRootDesc.ppStaticSamplerNames = bufferCopyShaderRootSamplerNames;
		addRootSignature(pRenderer, &bufferCopyShaderRootDesc, &pRootSignatureCopyBuffer);
		
		DescriptorBinderDesc descriptorBinderDesc[] = {
			{ pRootSignatureZprepass, 0, 1 },
			{ pRootSignatureForwardPass, 0, 1 },
			{ pRootSignatureShadowPass, 0, 1 },
			{ pRootSignatureSkybox, 0, 1 },
			{ pRootSignatureSdfSphere, 1, 1 },
			{ pRootSignatureESMBlur, 0, 2 },
			{ pRootSignatureCopyBuffer, 0, 1 }
		};
		const uint32_t descBinderSize = sizeof(descriptorBinderDesc) / sizeof(*descriptorBinderDesc);
		addDescriptorBinder(pRenderer, 0, descBinderSize, descriptorBinderDesc, &pDescriptorBinder);
		
		/************************************************************************/
		// setup Rasterizer State
		/************************************************************************/
		RasterizerStateDesc rasterStateDesc = {};
		rasterStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullFront);

		rasterStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullNone);

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcFactors[0] = BC_ZERO;
		blendStateDesc.mDstFactors[0] = BC_SRC_COLOR;
		blendStateDesc.mBlendModes[0] = BM_ADD;    //tried min, multiplicative looks better
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mMasks[0] = RED;
		addBlendState(pRenderer, &blendStateDesc, &pBlendStateSDF);

		/************************************************************************/
		finishResourceLoading();
		/************************************************************************/
		////////////////////////////////////////////////

		/************************************************************************/
		// Initialize Resources
		/************************************************************************/

		gESMBlurUniformData.mBlurWidth = gEsmCpuSettings.mFilterWidth;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			BufferUpdateDesc esmBlurBufferCbv = { pBufferESMBlurUniform[i], &gESMBlurUniformData };
			updateResource(&esmBlurBufferCbv);
		}
		createScene();

		/*************************************************/
		//					UI
		/*************************************************/
		if (!gAppUI.Init(pRenderer))
			return false;
		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartPosition = vec2(5, 200.0f) / dpiScale;
		;
		guiDesc.mStartSize = vec2(450, 600) / dpiScale;
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		GuiController::addGui();

		CameraMotionParameters cmp{ 16.0f, 60.0f, 20.0f };
		vec3                   camPos{ 12, 13, -15 };
		vec3                   lookAt{ 0 };

		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);

		InputSystem::RegisterInputEvent(cameraInputEvent);

		return true;
	}
	void RemoveRenderTargetsAndSwapChian()
	{
		removeResource(pDepthCopyTexture);
		removeRenderTarget(pRenderer, pRenderTargetDepth);
		removeResource(pBlurredESM);
		removeRenderTarget(pRenderer, pRenderTargetShadowMap);
		removeRenderTarget(pRenderer, pRenderTargetSdfSimple);
		removeSwapChain(pRenderer, pSwapChain);
	}
	void Exit() override
	{
		waitQueueIdle(pGraphicsQueue);
		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

		gAppUI.Exit();
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			for (uint32_t j = 0; j < PROJECTION_COUNT; ++j)
				removeResource(pBufferObjectTransforms[j][i]);
			removeResource(pBufferLightUniform[i]);
			removeResource(pBufferESMBlurUniform[i]);
			removeResource(pBufferRenderSettings[i]);
			removeResource(pBufferCameraUniform[i]);
			removeResource(pBufferSdfInputUniform[i]);
		}
		removeResource(pBufferESMBlurIntermediate);
		removeResource(pBufferBoxIndex);
		removeResource(pBufferSphereVertex);
#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerMiplessSampler);

		removeShader(pRenderer, pShaderForwardPass);
		removeShader(pRenderer, pShaderESMBlur);
		removeShader(pRenderer, pShaderZPass);
		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderSdfSphere);
		removeShader(pRenderer, pShaderCopyBuffer);

		removeRootSignature(pRenderer, pRootSignatureZprepass);
		removeRootSignature(pRenderer, pRootSignatureForwardPass);
		removeRootSignature(pRenderer, pRootSignatureESMBlur);
		removeRootSignature(pRenderer, pRootSignatureShadowPass);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureSdfSphere);
		removeRootSignature(pRenderer, pRootSignatureCopyBuffer);

		removeDescriptorBinder(pRenderer, pDescriptorBinder);

		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateTestOnly);
		removeDepthState(pDepthStateStencilShadow);

		removeBlendState(pBlendStateSDF);

		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);

		removeResource(pTextureSkybox);

		for (uint i = 0; i < 2; ++i)
			removeResource(pTextureScene[i]);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);

		removeResourceLoaderInterface(pRenderer);
		removeQueue(pGraphicsQueue);
		removeRenderer(pRenderer);
	}

	bool Load() override
	{
		if (!AddRenderTargetsAndSwapChain())
			return false;
		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif

		/************************************************************************/
		// Setup vertex layout for all shaders
		/************************************************************************/
		//layout and pipeline for sphere draw
		VertexLayout vertexLayoutRegular = {};
		vertexLayoutRegular.mAttribCount = 2;
		vertexLayoutRegular.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutRegular.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutRegular.mAttribs[0].mBinding = 0;
		vertexLayoutRegular.mAttribs[0].mLocation = 0;
		vertexLayoutRegular.mAttribs[0].mOffset = 0;
		vertexLayoutRegular.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutRegular.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutRegular.mAttribs[1].mBinding = 0;
		vertexLayoutRegular.mAttribs[1].mLocation = 1;
		vertexLayoutRegular.mAttribs[1].mOffset = 3 * sizeof(float);
#if FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA
		VertexLayout vertexLayoutZPass = {};
		vertexLayoutZPass.mAttribCount = 1;
		vertexLayoutZPass.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutZPass.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutZPass.mAttribs[0].mBinding = 0;
		vertexLayoutZPass.mAttribs[0].mLocation = 0;
		vertexLayoutZPass.mAttribs[0].mOffset = 0;
#endif
		/************************************************************************/
		// Setup the resources needed for z-prepass
		/************************************************************************/
		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& zPrepassPipelineSettings = desc.mGraphicsDesc;
		zPrepassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		zPrepassPipelineSettings.mRenderTargetCount = 0;
		zPrepassPipelineSettings.pDepthState = pDepthStateEnable;
		zPrepassPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		zPrepassPipelineSettings.mSampleCount = pRenderTargetDepth->mDesc.mSampleCount;
		zPrepassPipelineSettings.mSampleQuality = pRenderTargetDepth->mDesc.mSampleQuality;
		zPrepassPipelineSettings.pRootSignature = pRootSignatureZprepass;
		zPrepassPipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		zPrepassPipelineSettings.pShaderProgram = pShaderZPass;
#if FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA
		zPrepassPipelineSettings.pVertexLayout = &vertexLayoutZPass;
#else
		zPrepassPipelineSettings.pVertexLayout = &vertexLayoutRegular;
#endif
		addPipeline(pRenderer, &desc, &pPipelineZPrepass);
		/************************************************************************/
		// Setup the resources needed for the Forward Shade Pipeline
		/******************************/
		PipelineDesc forwardGraphicsDesc = {};
		forwardGraphicsDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& forwardShadePipelineSettings = forwardGraphicsDesc.mGraphicsDesc;
		forwardShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		forwardShadePipelineSettings.mRenderTargetCount = 1;
		forwardShadePipelineSettings.pDepthState = pDepthStateTestOnly;
		forwardShadePipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		forwardShadePipelineSettings.pRootSignature = pRootSignatureForwardPass;
		forwardShadePipelineSettings.pShaderProgram = pShaderForwardPass;
		forwardShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		forwardShadePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		forwardShadePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		forwardShadePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		forwardShadePipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		forwardShadePipelineSettings.pVertexLayout = &vertexLayoutRegular;
		addPipeline(pRenderer, &forwardGraphicsDesc, &pPipelineForwardShadeSrgb);

		/************************************************************************/
		// Setup the resources needed for shadow map
		/************************************************************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& shadowMapPipelineSettings = desc.mGraphicsDesc;
		shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowMapPipelineSettings.mRenderTargetCount = 0;
		shadowMapPipelineSettings.pDepthState = pDepthStateEnable;
		shadowMapPipelineSettings.mDepthStencilFormat = pRenderTargetShadowMap->mDesc.mFormat;
		shadowMapPipelineSettings.mSampleCount = pRenderTargetShadowMap->mDesc.mSampleCount;
		shadowMapPipelineSettings.mSampleQuality = pRenderTargetShadowMap->mDesc.mSampleQuality;
		shadowMapPipelineSettings.pRootSignature = pRootSignatureShadowPass;
		shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		shadowMapPipelineSettings.pShaderProgram = pShaderZPass;
#if FORGE_ALLOWS_NOT_TIGHTLY_PACKED_VERTEX_DATA
		shadowMapPipelineSettings.pVertexLayout = &vertexLayoutZPass;
#else
		shadowMapPipelineSettings.pVertexLayout = &vertexLayoutRegular;
#endif
		addPipeline(pRenderer, &desc, &pPipelineShadowPass);
		/*-----------------------------------------------------------*/
		// Setup the resources needed for the ESM Blur Compute Pipeline
		/*-----------------------------------------------------------*/
		desc.mType = PIPELINE_TYPE_COMPUTE;
		desc.mComputeDesc = {};
		ComputePipelineDesc& esmBlurPipelineSettings = desc.mComputeDesc;
		esmBlurPipelineSettings.pRootSignature = pRootSignatureESMBlur;
		esmBlurPipelineSettings.pShaderProgram = pShaderESMBlur;
		addPipeline(pRenderer, &desc, &pPipelineESMBlur);
		/*-----------------------------------------------------------*/
		// Setup the resources needed for the buffer copy pipeline
		/*-----------------------------------------------------------*/
		desc.mComputeDesc = {};
		ComputePipelineDesc& bufferCopyPipelineSettings = desc.mComputeDesc;
		bufferCopyPipelineSettings.pRootSignature = pRootSignatureCopyBuffer;
		bufferCopyPipelineSettings.pShaderProgram = pShaderCopyBuffer;
		addPipeline(pRenderer, &desc, &pPipelineCopyBuffer);

		/************************************************************************/
		// Setup the resources needed for Skybox
		/************************************************************************/
		desc.mType = PIPELINE_TYPE_GRAPHICS;
		desc.mGraphicsDesc = forwardGraphicsDesc.mGraphicsDesc;
		GraphicsPipelineDesc& skyboxPipelineSettings = desc.mGraphicsDesc;
		skyboxPipelineSettings.pDepthState = pDepthStateTestOnly;
		skyboxPipelineSettings.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineSettings.pShaderProgram = pShaderSkybox;
		skyboxPipelineSettings.pVertexLayout = NULL;
		addPipeline(pRenderer, &desc, &pPipelineSkybox);

		/************************************************************************/
		// Setup the resources needed for Sdf box
		/************************************************************************/
		desc.mGraphicsDesc = {};
		GraphicsPipelineDesc& sdfSpherePipelineSettings = desc.mGraphicsDesc;
		sdfSpherePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		sdfSpherePipelineSettings.mRenderTargetCount = 1;
		sdfSpherePipelineSettings.pDepthState = pDepthStateStencilShadow;
		sdfSpherePipelineSettings.pColorFormats = &pRenderTargetSdfSimple->mDesc.mFormat;
		sdfSpherePipelineSettings.pSrgbValues = &pRenderTargetSdfSimple->mDesc.mSrgb;
		sdfSpherePipelineSettings.mSampleCount = pRenderTargetSdfSimple->mDesc.mSampleCount;
		sdfSpherePipelineSettings.mSampleQuality = pRenderTargetSdfSimple->mDesc.mSampleQuality;
		sdfSpherePipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		sdfSpherePipelineSettings.pRootSignature = pRootSignatureSdfSphere;
		sdfSpherePipelineSettings.pShaderProgram = pShaderSdfSphere;
		sdfSpherePipelineSettings.pBlendState = pBlendStateSDF;
		sdfSpherePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &desc, &pPipelineSdfSphere);

		return true;
	}

	void Unload() override
	{
		waitQueueIdle(pGraphicsQueue);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineZPrepass);
		removePipeline(pRenderer, pPipelineForwardShadeSrgb);
		removePipeline(pRenderer, pPipelineESMBlur);
		removePipeline(pRenderer, pPipelineShadowPass);
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineSdfSphere);
		removePipeline(pRenderer, pPipelineCopyBuffer);

		RemoveRenderTargetsAndSwapChian();
	}

	void Update(float deltaTime) override
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		if (InputSystem::GetBoolInput(KEY_BUTTON_X_TRIGGERED))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		// Dynamic UI elements
		GuiController::updateDynamicUI();

		/************************************************************************/
		// Scene Render Settings
		/************************************************************************/
		gRenderSettings.mWindowDimension.setX((float)mSettings.mWidth);
		gRenderSettings.mWindowDimension.setY((float)mSettings.mHeight);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;
    //if(currentTime < 1000.f)
      currentTime += deltaTime * 1000.0f;

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();
		/************************************************************************/
		// Update Camera
		/************************************************************************/
		const float     aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		constexpr float horizontal_fov = PI / 2.0f;
		constexpr float nearValue = 1.0f;
		constexpr float farValue = 4000.f;
		mat4            projMat = mat4::perspectiveReverseZ(horizontal_fov, aspectInverse, nearValue, farValue);    //view matrix

		gCameraUniformData.mView = viewMat;
		gCameraUniformData.mProject = projMat;
		gCameraUniformData.mViewProject = projMat * viewMat;
		gCameraUniformData.mInvProj = inverse(projMat);
		gCameraUniformData.mInvView = inverse(viewMat);
		gCameraUniformData.mInvViewProject = inverse(gCameraUniformData.mViewProject);
		gCameraUniformData.mNear = nearValue;
		gCameraUniformData.mFarNearDiff = farValue - nearValue;    // if OpenGL convention was used this would be 2x the value
		gCameraUniformData.mFarNear = nearValue * farValue;
		gCameraUniformData.NDCConversionConstants[0] = vec4(-1.f, 1.f, projMat[2][2], 1.f);
		gCameraUniformData.NDCConversionConstants[1] = gCameraUniformData.mInvViewProject[2] * projMat[3][2];
		/************************************************************************/
		// Light Matrix Update - for shadow map
		/************************************************************************/
		vec3 lightPos = vec3(gLightCpuSettings.mLightPosition.x, gLightCpuSettings.mLightPosition.y, gLightCpuSettings.mLightPosition.z);
		pLightView->moveTo(lightPos);
		pLightView->lookAt(gObjectsCenter);

		mat4 lightView = pLightView->getViewMatrix();
		//perspective as spotlight, for future use. TODO: Use a frustum fitting algorithm to maximise effective resolution!
		mat4 lightProjMat = mat4::perspectiveReverseZ(PI / 1.2f, 1.f, gESMBlurUniformData.mNear, gESMBlurUniformData.mFar);
		gLightUniformData.mLightPosition = vec4(lightPos, 0);
		gLightUniformData.mLightViewProj = lightProjMat * lightView;
		gLightUniformData.mLightColor = vec4(1, 1, 1, 1);
		gLightUniformData.mLightUpVec = transpose(lightView)[1];
		gLightUniformData.mLightRange = gESMBlurUniformData.mFar;
		/************************************************************************/
		// Update SDF
		/************************************************************************/
		gSdfUniformData.mShadowHardnessRcp = 1.f / gSdfUniformData.mShadowHardness;
		/************************************************************************/
		// Update ESM
		/************************************************************************/
		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
			gESMBlurUniformData.mBlurWidth = gEsmCpuSettings.mFilterWidth;

		float const rads = 0.0001f * currentTime;

		// Rotate spheres
		for (int i = 0; i < SPHERE_NUM; i++)
		{
			mat4 worldMat = mat4::rotationY(rads) * gObjectInfoData[i].mTranslationMat * gObjectInfoData[i].mScaleMat;
			gObjectInfoUniformData[MAIN_CAMERA].mWorldViewProjMat[i] = gCameraUniformData.mViewProject * worldMat;
			gObjectInfoUniformData[MAIN_CAMERA].mWorldMat[i] = worldMat;

			/*
             * @REMARK: In normal scenes we would extract the scale from the world transform matrix and find the maximum extent
                vec3 scale = gObjectInfoData[i].mScaleMat.getScale();
                float maxScale = max(scale.x,scale.y,scale.z);
            */
			float maxScale = gSphereRadius;
			gObjectInfoUniformData[MAIN_CAMERA].mWorldBoundingSphereRadius[i] =
				vec4(1.f, 1.f + gSdfUniformData.mShadowHardnessRcp, 0.0, 0.0);
			gObjectInfoUniformData[MAIN_CAMERA].mWorldBoundingSphereRadius[i] *= maxScale * gSphereRadius;

			gObjectInfoUniformData[SHADOWMAP].mWorldViewProjMat[i] = gLightUniformData.mLightViewProj * worldMat;
			//gObjectInfoUniformData[SHADOWMAP].mWorldMat[i] = //redundant
			//gObjectInfoUniformData[MAIN_CAMERA].mWorldBoundingSphereRadius[i] = // redundant
		}
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	static void setRenderTarget(
		Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions)
	{
		if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		else
		{
			cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
			// sets the rectangles to match with first attachment, I know that it's not very portable.
			RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pSizeTarget->mDesc.mWidth, (float)pSizeTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
		}
	}

	static void drawZPrepass(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0u, NULL, pRenderTargetDepth, &loadActions);
		/* cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetDepth->mDesc.mWidth,
          (float)pRenderTargetDepth->mDesc.mHeight, 0.0f, 1.0f);*/

		// Draw the Z-prepass
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render Z-Prepass", true);

		cmdBindVertexBuffer(cmd, 1, &pBufferSphereVertex, NULL);

		cmdBindPipeline(cmd, pPipelineZPrepass);

		DescriptorData params[1] = {};
		params[0].pName = "objectUniformBlock";
		params[0].ppBuffers = &pBufferObjectTransforms[MAIN_CAMERA][gFrameIndex];

		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureZprepass, sizeof(params) / sizeof(DescriptorData), params);
		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, SPHERE_NUM, 0);
		
		setRenderTarget(cmd, 0, NULL, NULL, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	static void drawForwardPass(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
		// could do don't care but this is a combined depth-stencil format (same op, best perf)
		loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetScreen->mDesc.mClearValue;
		setRenderTarget(cmd, 1, &pRenderTargetScreen, pRenderTargetDepth, &loadActions);
		/*cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetDepth->mDesc.mWidth,
          (float)pRenderTargetDepth->mDesc.mHeight, 0.0f, 1.0f);*/

		// Draw the objects with lighting
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Final Render Pass", true);

		cmdBindVertexBuffer(cmd, 1, &pBufferSphereVertex, NULL);

		DescriptorData params[8] = {};
		params[0].pName = "SphereTex";
		params[0].ppTextures = &pTextureScene[0];
		params[1].pName = "PlaneTex";
		params[1].ppTextures = &pTextureScene[1];
		params[2].pName = "objectUniformBlock";
		params[2].ppBuffers = &pBufferObjectTransforms[MAIN_CAMERA][gFrameIndex];
		params[3].pName = "lightUniformBlock";
		params[3].ppBuffers = &pBufferLightUniform[gFrameIndex];
		params[4].pName = "renderSettingUniformBlock";
		params[4].ppBuffers = &pBufferRenderSettings[gFrameIndex];
		params[5].pName = "ESMInputConstants";
		params[5].ppBuffers = &pBufferESMBlurUniform[gFrameIndex];
		params[6].pName = "shadowTexture";
		switch (gRenderSettings.mShadowType)
		{
			case SHADOW_TYPE_ESM: params[6].ppTextures = &pBlurredESM; break;
			case SHADOW_TYPE_SDF: params[6].ppTextures = &pRenderTargetSdfSimple->pTexture; break;
			case SHADOW_TYPE_NONE:
			default:
				params[6].ppTextures = &pTextureScene[1];    // just set something so the debug layers don't complain
				break;
		}
		params[7].pName = "cameraUniformBlock";
		params[7].ppBuffers = &pBufferCameraUniform[gFrameIndex];
		cmdBindPipeline(cmd, pPipelineForwardShadeSrgb);
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureForwardPass, sizeof(params) / sizeof(DescriptorData), params);

		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, SPHERE_NUM, 0);

		//extra
		drawSkybox(cmd);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	static void blurEsmMap(Cmd* cmd, uint32_t rtId)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, rtId == 0 ? "Blur ESM Pass H" : "Blur ESM Pass V", true);

		DescriptorData params[5] = {};
		params[0].pName = "ESMInputConstants";
		params[0].ppBuffers = &pBufferESMBlurUniform[gFrameIndex];
		params[1].pName = "shadowExpMap";
		params[1].ppTextures = &pRenderTargetShadowMap->pTexture;
		params[2].pName = "blurredESM";
		params[2].ppTextures = &pBlurredESM;
		params[3].pName = "IntermediateResult";
		params[3].ppBuffers = &pBufferESMBlurIntermediate;
		params[4].pName = "RootConstants";
		params[4].pRootConstant = &rtId;
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureESMBlur, sizeof(params) / sizeof(DescriptorData), params);
		cmdBindPipeline(cmd, pPipelineESMBlur);
		cmdDispatch(cmd, 1u, ESM_SHADOWMAP_RES, 1u);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	static void drawEsmShadowMap(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadowMap->mDesc.mClearValue;

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw ESM Shadow Map", true);
		// Start render pass and apply load actions
		setRenderTarget(cmd, 0, NULL, pRenderTargetShadowMap, &loadActions);
		/*cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadowMap->mDesc.mWidth,
          (float)pRenderTargetShadowMap->mDesc.mHeight, 1.0f, 0.0f);*/

		cmdBindVertexBuffer(cmd, 1, &pBufferSphereVertex, NULL);

		cmdBindPipeline(cmd, pPipelineShadowPass);
		DescriptorData params[1] = {};
		params[0].pName = "objectUniformBlock";
		params[0].ppBuffers = &pBufferObjectTransforms[SHADOWMAP][gFrameIndex];
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureShadowPass, sizeof(params) / sizeof(DescriptorData), params);

		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, SPHERE_NUM, 0);
		setRenderTarget(cmd, 0, NULL, NULL, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
	static void drawSDFShadow(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
		// could do don't care but this should be a combined depth-stencil format (same op, best perf)
		loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetSdfSimple->mDesc.mClearValue;
		setRenderTarget(cmd, 1, &pRenderTargetSdfSimple, pRenderTargetDepth, &loadActions);

		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw SDF", true);

		cmdBindPipeline(cmd, pPipelineSdfSphere);

		DescriptorData params[6] = {};
		params[0].pName = "renderSettingUniformBlock";
		params[0].ppBuffers = &pBufferRenderSettings[gFrameIndex];
		params[1].pName = "cameraUniformBlock";
		params[1].ppBuffers = &pBufferCameraUniform[gFrameIndex];
		params[2].pName = "lightUniformBlock";
		params[2].ppBuffers = &pBufferLightUniform[gFrameIndex];
		params[3].pName = "objectUniformBlock";
		params[3].ppBuffers = &pBufferObjectTransforms[MAIN_CAMERA][gFrameIndex];
		params[4].pName = "DepthBufferCopy";
		params[4].ppTextures = &pDepthCopyTexture;
		params[5].pName = "sdfUniformBlock";
		params[5].ppBuffers = &pBufferSdfInputUniform[gFrameIndex];
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureSdfSphere, sizeof(params) / sizeof(DescriptorData), params);
		cmdBindIndexBuffer(cmd, pBufferBoxIndex, NULL);
		cmdDrawIndexedInstanced(cmd, 36, 0, SPHERE_NUM - 1, 0, 0);

		setRenderTarget(cmd, 0, NULL, NULL, NULL);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}
	static void drawSkybox(Cmd* cmd)
	{
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Skybox", true);

		cmdBindPipeline(cmd, pPipelineSkybox);

		DescriptorData params[3] = {};
		params[0].pName = "renderSettingUniformBlock";
		params[0].ppBuffers = &pBufferRenderSettings[gFrameIndex];
		params[1].pName = "cameraUniformBlock";
		params[1].ppBuffers = &pBufferCameraUniform[gFrameIndex];
		params[2].pName = "Skybox";
		params[2].ppTextures = &pTextureSkybox;
		cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureSkybox, sizeof(params) / sizeof(DescriptorData), params);
		cmdDraw(cmd, 3, 0);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
	}

	void Draw() override
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*     pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);
		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/
		BufferUpdateDesc renderSettingCbv = { pBufferRenderSettings[gFrameIndex], &gRenderSettings };
		updateResource(&renderSettingCbv);

		for (uint32_t j = 0; j < PROJECTION_COUNT; j++)
		{
			BufferUpdateDesc viewProjCbv = { pBufferObjectTransforms[j][gFrameIndex], gObjectInfoUniformData + j };
			updateResource(&viewProjCbv);
		}

		BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex], &gCameraUniformData };
		updateResource(&cameraCbv);

		BufferUpdateDesc lightBufferCbv = { pBufferLightUniform[gFrameIndex], &gLightUniformData };
		updateResource(&lightBufferCbv);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			BufferUpdateDesc esmBlurCbv = { pBufferESMBlurUniform[gFrameIndex], &gESMBlurUniformData };
			updateResource(&esmBlurCbv);
		}

		BufferUpdateDesc sdfInputUniformCbv = { pBufferSdfInputUniform[gFrameIndex], &gSdfUniformData };
		updateResource(&sdfInputUniformCbv);
		/************************************************************************/
		// Rendering
		/************************************************************************/
		// Get command list to store rendering commands for this frame
		Cmd* cmd = ppCmds[gFrameIndex];

		pRenderTargetScreen = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		beginCmd(cmd);
		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		tinystl::vector<TextureBarrier> barriers(8);

		////////////////////////////////////////////////////////
		///// Draw Z-Prepass if needed
		barriers.clear();
		barriers.emplace_back(TextureBarrier{ pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE });
		// always do Z-Prepass because even basic shadowmapping could Z-pyramid to fit shadowmaps better
		// plus even if Forward Shading is used nothing stops us from using Forward+
		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
		cmdFlushBarriers(cmd);
		drawZPrepass(cmd);

		////////////////////////////////////////////////////////
		///// Draw Shadows
		barriers.clear();
		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
			barriers.emplace_back(TextureBarrier{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_DEPTH_WRITE });
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			barriers.emplace_back(TextureBarrier{ pRenderTargetDepth->pTexture, RESOURCE_STATE_COPY_SOURCE });
			barriers.emplace_back(TextureBarrier{ pDepthCopyTexture, RESOURCE_STATE_COPY_DEST });    // bad Vulkan usage both aspects
			barriers.emplace_back(TextureBarrier{ pRenderTargetSdfSimple->pTexture, RESOURCE_STATE_RENDER_TARGET });
		}
		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
		cmdFlushBarriers(cmd);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			drawEsmShadowMap(cmd);

			barriers.clear();
			barriers.emplace_back(TextureBarrier{ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
			barriers.emplace_back(TextureBarrier{ pBlurredESM, RESOURCE_STATE_UNORDERED_ACCESS });
			cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
			cmdFlushBarriers(cmd);
			////////////////////////////////////////////////////////
			///// Blur ESM Map
			blurEsmMap(cmd, 0);
			BufferBarrier blurIntermediateBarrier = { pBufferESMBlurIntermediate, RESOURCE_STATE_UNORDERED_ACCESS };
			cmdResourceBarrier(cmd, 1, &blurIntermediateBarrier, 0, NULL, true);
			cmdFlushBarriers(cmd);
			blurEsmMap(cmd, 1);
			TextureBarrier blurEsmBarrier = { pBlurredESM, RESOURCE_STATE_SHADER_RESOURCE };
			cmdResourceBarrier(cmd, 0, NULL, 1, &blurEsmBarrier, true);
			cmdFlushBarriers(cmd);
		}
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			{
				cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Copy buffer", true);
				
#define USE_VK_COPY 0
#if USE_VK_COPY
#ifdef VULKAN
				// this would have been a stopgap measure
				// under Vulkan the SDF raytracer should use depth as a subpass input, if possible to do so while still z-testing
				VkImageCopy copyDesc = {};
				copyDesc.srcSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
				copyDesc.dstOffset = { 0, 0, 0 };
				copyDesc.dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
				copyDesc.srcOffset = { 0, 0, 0 };
				copyDesc.extent = { pRenderTargetDepth->mDesc.mWidth, pRenderTargetDepth->mDesc.mHeight, 1 };
				vkCmdCopyImage(
					cmd->pVkCmdBuf, pRenderTargetDepth->pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					pDepthCopyTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyDesc);
#elif defined(DIRECT3D12)
				D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
				srcLoc.pResource = pRenderTargetDepth->pTexture->pDxResource;
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
				dstLoc.pResource = pDepthCopyTexture->pDxResource;
				dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				D3D12_BOX region = {};
				region.right = pRenderTargetDepth->mDesc.mWidth;
				region.top = pRenderTargetDepth->mDesc.mHeight;
				cmd->pDxCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &region);
#endif
#else
				TextureBarrier preCopyBarriers[] = { { pDepthCopyTexture, RESOURCE_STATE_UNORDERED_ACCESS },
													 { pRenderTargetDepth->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
				cmdResourceBarrier(cmd, 0, NULL, 2, preCopyBarriers, false);

				DescriptorData params[2] = {};
				params[0].pName = "srcImg";
				params[0].ppTextures = &pRenderTargetDepth->pTexture;
				params[1].pName = "dstImg";
				params[1].ppTextures = &pDepthCopyTexture;
				cmdBindDescriptors(cmd, pDescriptorBinder, pRootSignatureCopyBuffer, 2, params);
				cmdBindPipeline(cmd, pPipelineCopyBuffer);
				cmdDispatch(
					cmd, pRenderTargetDepth->mDesc.mWidth / COPY_BUFFER_WORKGROUP,
					(uint)ceilf((float)pRenderTargetDepth->mDesc.mHeight / COPY_BUFFER_WORKGROUP), 1u);
#endif

				cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
			}

			barriers.clear();
			barriers.emplace_back(TextureBarrier{ pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE });
			barriers.emplace_back(TextureBarrier{ pDepthCopyTexture, RESOURCE_STATE_SHADER_RESOURCE });
			cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
			cmdFlushBarriers(cmd);

			drawSDFShadow(cmd);
		}

		// Draw To Screen
		barriers.clear();
		barriers.emplace_back(TextureBarrier{ pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET });
		barriers.emplace_back(TextureBarrier{ pRenderTargetSdfSimple->pTexture, RESOURCE_STATE_SHADER_RESOURCE });
		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
		cmdFlushBarriers(cmd);

		drawForwardPass(cmd);

		////////////////////////////////////////////////////////
		//  Draw UIs
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw UI", true);
		LoadActionsDesc loadActions;
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
		loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
		setRenderTarget(cmd, 1, &pRenderTargetScreen, NULL, &loadActions);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

		gAppUI.DrawText(cmd, float2(8.0f, 15.0f), tinystl::string::format("CPU Time: %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
#ifndef TARGET_IOS
		gAppUI.DrawText(cmd, float2(8, 40), tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);

		gAppUI.DrawDebugGpuProfile(cmd, float2(8, 65), pGpuProfiler, NULL);
#else
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pGuiWindow);
		gAppUI.Draw(cmd);
		setRenderTarget(cmd, 0, NULL, NULL, NULL);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		////////////////////////////////////////////////////////

		barriers.clear();
		barriers.emplace_back(TextureBarrier{ pRenderTargetScreen->pTexture, RESOURCE_STATE_PRESENT });
		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)barriers.size(), barriers.data(), true);
		cmdFlushBarriers(cmd);

		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);

		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() override { return "09_LightShadowPlayground"; }

	bool addSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;
		SwapChainDesc  swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = width;
		swapChainDesc.mHeight = height;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = ImageFormat::BGRA8;
		swapChainDesc.mColorClearValue = { 1, 1, 1, 1 };
		swapChainDesc.mSrgb = false;

		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		return pSwapChain != NULL;
	}

	bool AddRenderTargetsAndSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		const ClearValue depthStencilClear = { 0.0f, 0 };
		const ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };
		const ClearValue colorClearWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

		/************************************************************************/
		// Main depth buffer
		/************************************************************************/
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = depthStencilClear;
		depthRT.mDepth = 1;
		#define STENCIL_SDF_OPTIMIZATION
		#ifdef STENCIL_SDF_OPTIMIZATION
		// When using stencil optimization, use a packed depth-stencil format.
		// However mobile doesn't support D32S8, while AMD does not support D24S8 under Vulkan, etc.
		if (isImageFormatSupported(ImageFormat::D32S8))
			depthRT.mFormat = ImageFormat::D32S8;
		else if (isImageFormatSupported(ImageFormat::D24S8))
			depthRT.mFormat = ImageFormat::D24S8;
		else if (isImageFormatSupported(ImageFormat::X8D24PAX32))
			depthRT.mFormat = ImageFormat::X8D24PAX32;
		else if (isImageFormatSupported(ImageFormat::D16S8))
			depthRT.mFormat = ImageFormat::D16S8;
		else
			ASSERT(false); // no supported packed depth stencil format to use
		#else
		depthRT.mFormat = ImageFormat::D32F;
		#endif
		depthRT.mWidth = width;
		depthRT.mHeight = height;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.pDebugName = L"Depth RT";
#ifdef METAL
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
#endif
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);
		TextureDesc depthCopyTextureDesc = {};
		depthCopyTextureDesc.mArraySize = 1;
		depthCopyTextureDesc.mDepth = 1;
		depthCopyTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE
#if !USE_VK_COPY
		| DESCRIPTOR_TYPE_RW_TEXTURE
#endif
			;
		depthCopyTextureDesc.mFormat = ImageFormat::R32F;
		depthCopyTextureDesc.mWidth = depthRT.mWidth;
		depthCopyTextureDesc.mHeight = depthRT.mHeight;
		depthCopyTextureDesc.mMipLevels = 1;
		depthCopyTextureDesc.mSampleCount = depthRT.mSampleCount;
		depthCopyTextureDesc.mSampleQuality = depthRT.mSampleQuality;
		depthCopyTextureDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		TextureLoadDesc depthCopyTextureLoadDesc = {};
		depthCopyTextureLoadDesc.pDesc = &depthCopyTextureDesc;
		depthCopyTextureLoadDesc.ppTexture = &pDepthCopyTexture;
		addResource(&depthCopyTextureLoadDesc);
		/* TextureView for depth only aspect would be useful
        TextureDesc depthTextureViewDesc= {};
        depthTextureViewDesc.mArraySize = 1;
        depthTextureViewDesc.mDepth = 1;
        depthTextureViewDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthTextureViewDesc.mFormat = depthRT.mFormat;
        depthTextureViewDesc.mWidth = depthRT.mWidth;
        depthTextureViewDesc.mHeight = depthRT.mHeight;
        depthTextureViewDesc.mMipLevels = 1;
        depthTextureViewDesc.mSampleCount = depthRT.mSampleCount;
        depthTextureViewDesc.mSampleQuality = depthRT.mSampleQuality;
        depthTextureViewDesc.*/
		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/
		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue.depth = depthStencilClear.depth;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = ImageFormat::D32F;
		shadowRTDesc.mWidth = ESM_SHADOWMAP_RES;
		shadowRTDesc.mHeight = ESM_SHADOWMAP_RES;
		shadowRTDesc.mSampleCount = (SampleCount)ESM_MSAA_SAMPLES;
		shadowRTDesc.mSampleQuality = 0;    // don't need higher quality sample patterns as the texture will be blurred heavily
		shadowRTDesc.pDebugName = L"Shadow Map RT";

		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadowMap);

		/************************************************************************/
		// Sdf render target
		/************************************************************************/
		RenderTargetDesc sdfSimpleRTDesc = {};
		sdfSimpleRTDesc.mArraySize = 1;
		sdfSimpleRTDesc.mClearValue = colorClearWhite;
		sdfSimpleRTDesc.mDepth = 1;
		sdfSimpleRTDesc.mFormat = ImageFormat::R8;
		sdfSimpleRTDesc.mWidth = width;
		sdfSimpleRTDesc.mHeight = height;
		sdfSimpleRTDesc.mSampleCount = SAMPLE_COUNT_1;
		sdfSimpleRTDesc.mSampleQuality = 0;
		sdfSimpleRTDesc.pDebugName = L"Sdf RT";
		addRenderTarget(pRenderer, &sdfSimpleRTDesc, &pRenderTargetSdfSimple);

		/************************************************************************/
		// ESM Blurred texture
		/************************************************************************/
		TextureDesc blurredESMTextureDesc = {};
		blurredESMTextureDesc.mArraySize = 1;
		blurredESMTextureDesc.mDepth = 1;
		blurredESMTextureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		blurredESMTextureDesc.mFormat = ImageFormat::R32F;
		blurredESMTextureDesc.mWidth = shadowRTDesc.mWidth;
		blurredESMTextureDesc.mHeight = shadowRTDesc.mHeight;
		blurredESMTextureDesc.mMipLevels = 1;
		blurredESMTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		blurredESMTextureDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		TextureLoadDesc blurredESMTextureLoadDesc = {};
		blurredESMTextureLoadDesc.pDesc = &blurredESMTextureDesc;
		blurredESMTextureLoadDesc.ppTexture = &pBlurredESM;
		addResource(&blurredESMTextureLoadDesc);

		return addSwapChain();
	}

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0)) const
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > maxDistance * maxDistance)
		{
			d *= maxDistance / sqrtf(lenSqr);
		}

		p = d + lookAt;
		pCameraController->moveTo(p);
		pCameraController->lookAt(lookAt);
	}

	static bool cameraInputEvent(const ButtonData* data)
	{
		pCameraController->onInputEvent(data);
		return true;
	}
};

void GuiController::updateDynamicUI()
{
	if (gRenderSettings.mShadowType != GuiController::currentlyShadowType)
	{
		if (GuiController::currentlyShadowType == SHADOW_TYPE_ESM)
			GuiController::esmDynamicWidgets.HideWidgets(pGuiWindow);
		else if (GuiController::currentlyShadowType == SHADOW_TYPE_SDF)
			GuiController::sdfDynamicWidgets.HideWidgets(pGuiWindow);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			GuiController::esmDynamicWidgets.ShowWidgets(pGuiWindow);
		}
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			GuiController::sdfDynamicWidgets.ShowWidgets(pGuiWindow);
		}

		GuiController::currentlyShadowType = (ShadowType)gRenderSettings.mShadowType;
	}
}

void GuiController::addGui()
{
	const float lightPosBound = 10.0f;
	pGuiWindow->AddWidget(SliderFloat3Widget(
		"Light Position", &gLightCpuSettings.mLightPosition, float3(-lightPosBound, 5.0f, -lightPosBound),
		float3(lightPosBound, 30.0f, lightPosBound), float3(0.1f, 0.1f, 0.1f)));

	static const char* shadowTypeNames[] = {
		"No Shadow", "(ESM) Exponential Shadow Mapping", "(SDF) Ray-Traced Soft Shadow",
		NULL    //needed for unix
	};
	static const uint32_t shadowTypeValues[] = {
		SHADOW_TYPE_NONE, SHADOW_TYPE_ESM, SHADOW_TYPE_SDF,
		0    //needed for unix
	};

	pGuiWindow->AddWidget(DropdownWidget("Shadow Type", &gRenderSettings.mShadowType, shadowTypeNames, shadowTypeValues, 3));

	// ESM dynamic widgets
	{
		GuiController::esmDynamicWidgets.AddWidget(SliderUintWidget("ESM Softness", &gEsmCpuSettings.mFilterWidth, 0u, 16u));
		GuiController::esmDynamicWidgets.AddWidget(SliderFloatWidget("ESM Darkness", &gESMBlurUniformData.mExponent, 0.0f, 346.0f, 0.1f));
	}
	// SDF dynamic widgets
	{
		GuiController::sdfDynamicWidgets.AddWidget(SliderFloatWidget("SDF Shadow Hardness", &gSdfUniformData.mShadowHardness, 0.1f, 8.f, 0.01f));
		GuiController::sdfDynamicWidgets.AddWidget(SliderUintWidget("SDF Max Iteration", &gSdfUniformData.mMaxIteration, 8u, 128u, 1u));
	}

	if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_ESM;
		GuiController::esmDynamicWidgets.ShowWidgets(pGuiWindow);
	}
	else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_SDF;
		GuiController::sdfDynamicWidgets.ShowWidgets(pGuiWindow);
	}
	else
		GuiController::currentlyShadowType = SHADOW_TYPE_NONE;
}

DEFINE_APPLICATION_MAIN(LightShadowPlayground)
