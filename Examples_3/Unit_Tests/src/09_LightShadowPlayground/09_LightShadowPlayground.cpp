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

#define SPHERE_EACH_ROW 5                                     //MUST MATCH with the same macro in all shaders
#define SPHERE_EACH_COL 5                                     //MUST MATCH with the same macro in all shaders
#define SPHERE_NUM (SPHERE_EACH_ROW * SPHERE_EACH_COL + 1)    // Must match with same macro in all shader.... +1 for plane
#define MAX_GAUSSIAN_WIDTH 31                                 //MUST MATCH with shade
#define MAX_GAUSSIAN_WEIGHTS_SIZE 64                          //MUST MATCH with shader = 32*2+1
#define MAX_SAMPLE_POINTS_NUM 16
#define DEBUG_OUTPUT 0    //exclusively used for texture data visulization, such as rendering depth, shadow map etc.

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
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
//GPU Profiler
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

//Generate sky box vertex buffer
const float gSkyboxPointArray[] = {
	10.0f,  -10.0f, -10.0f, 6.0f,    // -z
	-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
	-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

	-10.0f, -10.0f, 10.0f,  2.0f,    //-x
	-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
	-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

	10.0f,  -10.0f, -10.0f, 1.0f,    //+x
	10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
	10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

	-10.0f, -10.0f, 10.0f,  5.0f,    // +z
	-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
	10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

	-10.0f, 10.0f,  -10.0f, 3.0f,    //+y
	10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
	10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

	10.0f,  -10.0f, 10.0f,  4.0f,    //-y
	10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
	-10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
};

enum RenderOutput
{
	RENDER_OUTPUT_SCENE,    //render all scenes normally

	///@note all enum below will only be used when DEBUG_OUTPUT is enabled.
	RENDER_OUTPUT_SDF_MAP,     //render sdf shadows
	RENDER_OUTPUT_ALBEDO,      //render color only
	RENDER_OUTPUT_NORMAL,      //render normals in range[0,1]
	RENDER_OUTPUT_POSITION,    //render world position in RGBA32F
	RENDER_OUTPUT_DEPTH,       //render camera depth in range[0,1]
	RENDER_OUTPUT_ESM_MAP,     //render ESM shadow map, from light pov

	RENDER_OUTPUT_COUNT
};

enum ShadowType
{
	SHADOW_TYPE_NONE,
	SHADOW_TYPE_ESM,    //Exponential Shadow Map
	SHADOW_TYPE_SDF,    //Ray-Traced Signed Distance Fields Shadow

	SHADOW_TYPE_COUNT
};
typedef struct RenderSettingsUniformData
{
	vec4     mWindowDimension = { 1, 1, 0, 0 };      //only first two are used to represents window width and height, z and w are paddings
	uint32_t mRenderOutput = RENDER_OUTPUT_SCENE;    //Scene output, can change using GUI editor

#ifndef TARGET_IOS
	uint32_t mShadowType = SHADOW_TYPE_ESM;    //if not iOS, we start with ESM and can switch using GUI dropdown menu
#else
	uint32_t mShadowType = SHADOW_TYPE_SDF;    //if iOS, we only output SDF since GUI editor will not be aviable for iOS
#endif

} RenderSettingsUniformData;

typedef struct ObjectInfoStruct
{
	vec4 mColor;
	vec3 mTranslation;
	mat4 mTranslationMat;
	mat4 mScaleMat;
} ObjectInfoStruct;

typedef struct ObjectInfoUniformBlock
{
	mat4 mViewProject;
	mat4 mToWorldMat[SPHERE_NUM];
} ObjectInfoUniformBlock;

typedef struct SkyboxUniformBlock
{
	mat4 mViewProject;
} SkyboxUniformBlock;

typedef struct GaussianWeightsUniformBlock
{
	//use float4 for data alignment, but only x component is used
	float4 mWeights[MAX_GAUSSIAN_WEIGHTS_SIZE] = { { 0, 0, 0, 0 } };
} GaussianWeightsUniformBlock;

typedef struct LightUniformBlock
{
	mat4 mLightViewProj;
	vec4 mLightDirection = { -1, -1, -1, 0 };
	vec4 mLightColor = { 1, 0, 0, 1 };
} LightUniformBlock;

typedef struct CameraUniform
{
	vec4 mPosition;
} CameraUniform;

typedef struct ESMInputConstants
{
	float2 mWindowDimension = { 0, 0 };
	float2 mNearFarDist = { 1.f, 180.0f };
	float  mExponent = 240.0f;
	uint   mBlurWidth = 1U;
	int    mIfHorizontalBlur = 1;    //as boolean
	int    padding = 0;
} ESMInputConstants;

typedef struct SdfInputConstants
{
	mat4     mViewInverse;
	vec4     mCameraPosition = { 0, 0, 0, 0 };
	float    mShadowHardness = 6.0f;
	uint32_t mMaxIteration = 64U;
	float2   mWindowDimension = { 0, 0 };
	float    mSphereRadius;
	float    mRadsRot;
} SdfInputUniformBlock;

enum
{
	DEFERRED_RT_ALBEDO = 0,
	DEFERRED_RT_NORMAL,
	DEFERRED_RT_POSITION,

	DEFERRED_RT_COUNT
};

const uint32_t gImageCount = 3;

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pRenderTargetScreen;
RenderTarget* pRenderTargetDepth = NULL;
RenderTarget* pRenderTargetDeferredPass[DEFERRED_RT_COUNT] = { NULL };
RenderTarget* pRenderTargetShadowMap;
RenderTarget* pRenderTargetESMBlur[2];    //2: horizontal + vertical blur
RenderTarget* pRenderTargetSkybox;
RenderTarget* pRenderTargetSdfSimple;

/************************************************************************/
Buffer* pBufferSphereVertex = NULL;
/************************************************************************/
// Deferred Pass Shader pack
/************************************************************************/
Shader*           pShaderDeferredPass = NULL;
Pipeline*         pPipelineDeferredPass = NULL;
RootSignature*    pRootSignatureDeferredPass = NULL;
CommandSignature* pCmdSignatureDeferredPass = NULL;
/************************************************************************/
// Deferred Shade Shader pack
/************************************************************************/
Shader*        pShaderDeferredShade = NULL;
Pipeline*      pPipelineDeferredShadeSrgb = NULL;
RootSignature* pRootSignatureDeferredShade = NULL;
/************************************************************************/
// SDF Shader pack
/************************************************************************/
Shader*        pShaderSdfSimple = NULL;
Pipeline*      pPipelineSdfSimple = NULL;
RootSignature* pRootSignatureSdfSimple = NULL;
/************************************************************************/
// Skybox Shader Pack
/************************************************************************/
Shader*        pShaderSkybox = NULL;
Pipeline*      pPipelineSkybox = NULL;
RootSignature* pRootSignatureSkybox = NULL;
/************************************************************************/
// Shadow pass shader
/************************************************************************/
Shader*        pShaderShadowPass = NULL;
Pipeline*      pPipelineShadowPass = NULL;
RootSignature* pRootSignatureShadowPass = NULL;
//----------------- ESM Blur Shader------------------
Shader*        pShaderESMBlur = NULL;
Pipeline*      pPipelineESMBlur = NULL;
RootSignature* pRootSignatureESMBlur = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerPoint = NULL;
Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerShadow = NULL;
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerSkybox = NULL;

/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullBack = NULL;
RasterizerState* pRasterizerStateCullFront = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;

Buffer* pBufferSkyboxVertex = NULL;
/************************************************************************/
// Constant buffers
/************************************************************************/
Buffer* pBufferObjectTransforms[gImageCount] = { NULL };
Buffer* pBufferSkyboxUniform[gImageCount] = { NULL };
Buffer* pBufferLightUniform[gImageCount] = { NULL };
Buffer* pBufferESMBlurUniformH_Primary[gImageCount] = { NULL };
Buffer* pBufferESMBlurUniformV[gImageCount] = { NULL };
Buffer* pBufferESMGaussianWeights[gImageCount] = { NULL };
Buffer* pBufferRenderSettings[gImageCount] = { NULL };
Buffer* pBufferCameraUniform[gImageCount] = { NULL };
Buffer* pBufferSdfInputUniform[gImageCount] = { NULL };

/************************************************************************/
// Depth State
/************************************************************************/
DepthState* pDepthStateEnable = NULL;
DepthState* pDepthStateDisable = NULL;

/************************************************************************/
// Textures
/************************************************************************/
Texture* pTextureSkybox[6];
Texture* pTextureScene[2];
/************************************************************************/
// Render control variables
/************************************************************************/
struct
{
	uint32 mFilterWidth = 24U;
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

int                         gNumberOfSpherePoints;
ObjectInfoUniformBlock      gObjectInfoUniformData;
SkyboxUniformBlock          gSkyboxUniformData;
LightUniformBlock           gLightUniformData;
SdfInputUniformBlock        gSdfUniformData;
CameraUniform               gCameraUniformData;
ESMInputConstants           gESMBlurUniformDataH_Primary;
ESMInputConstants           gESMBlurUniformDataV;
GaussianWeightsUniformBlock gESMBlurGaussianWeights;

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

const char* pSkyboxImageFileNames[] = {
	"skybox/hw_sahara/sahara_rt.tga", "skybox/hw_sahara/sahara_lf.tga", "skybox/hw_sahara/sahara_up.tga",
	"skybox/hw_sahara/sahara_dn.tga", "skybox/hw_sahara/sahara_ft.tga", "skybox/hw_sahara/sahara_bk.tga",
};
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

static void calcGaussianWeights(GaussianWeightsUniformBlock* block, int gaussianWidth)
{
	for (int i = 0; i < MAX_GAUSSIAN_WEIGHTS_SIZE; i++)
	{
		block->mWeights[i] = { 0, 0, 0, 0 };
	}
	gaussianWidth = clamp(gaussianWidth, 0, MAX_GAUSSIAN_WIDTH);
	const int width = gaussianWidth;
	const int width2 = 2 * width;
	const int width2p1 = 2 * width + 1;
	block->mWeights[width] = 1;
	const float s = (float)(width) / 3.0f;
	float       totalWeight = block->mWeights[width].getX();
	//establish weights
	for (int i = 0; i < width; ++i)
	{
		float weight = exp(-((width - i) * (width - i)) / (2 * s * s));
		block->mWeights[i] = weight;
		block->mWeights[width2 - i] = weight;
		totalWeight += 2.0f * weight;
	}
	//normalize weights
	for (int i = 0; i < width2p1; ++i)
	{
		block->mWeights[i] /= totalWeight;
	}
}

/*!************************************************************************
 * @REMARK: Changing object transform here does NOT change SDF shadow since the latter is ray-marched.
 * One will also need to change the world setup in sdfSimple.frag
 *************************************************************************/
///@TODO: bake 3d texture of the world for sdf to use instead of a seperate setup
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
		gObjectInfoUniformData.mToWorldMat[i] = gObjectInfoData[i].mTranslationMat * gObjectInfoData[i].mScaleMat;
	}
}
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
struct GuiController
{
	static void addGui();
	static void updateDynamicUI();

	static DynamicUIControls esmDynamicWidgets;
	static DynamicUIControls sdfDynamicWidgets;

	static ShadowType currentlyShadowType;
};
ShadowType        GuiController::currentlyShadowType;
DynamicUIControls GuiController::esmDynamicWidgets;
DynamicUIControls GuiController::sdfDynamicWidgets;
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
class LightShadowPlayground: public IApp
{
	public:
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
		depthStateEnabledDesc.mDepthFunc = CMP_LEQUAL;
		depthStateEnabledDesc.mDepthWrite = true;
		depthStateEnabledDesc.mDepthTest = true;

		DepthStateDesc depthStateDisabledDesc = {};
		depthStateDisabledDesc.mDepthWrite = false;
		depthStateDisabledDesc.mDepthTest = false;

		addDepthState(pRenderer, &depthStateEnabledDesc, &pDepthStateEnable);
		addDepthState(pRenderer, &depthStateDisabledDesc, &pDepthStateDisable);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSkyboxImageFileNames[i];
			textureDesc.ppTexture = &pTextureSkybox[i];
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
		sphereVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		sphereVbDesc.pData = pSpherePoints;
		sphereVbDesc.ppBuffer = &pBufferSphereVertex;
		addResource(&sphereVbDesc);
		// Need to free memory;
		conf_free(pSpherePoints);
		//------------------------Skybox--------------------------
		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = gSkyboxPointArray;
		skyboxVbDesc.ppBuffer = &pBufferSkyboxVertex;
		addResource(&skyboxVbDesc);
		/************************************************************************/
		// Setup constant buffer data
		/************************************************************************/
		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(ObjectInfoUniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pBufferObjectTransforms[i];
			addResource(&ubDesc);
		}
		BufferLoadDesc ubEsmBlurDescH = {};
		ubEsmBlurDescH.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubEsmBlurDescH.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubEsmBlurDescH.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubEsmBlurDescH.mDesc.mSize = sizeof(ESMInputConstants);
		ubEsmBlurDescH.pData = &gESMBlurUniformDataH_Primary;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubEsmBlurDescH.ppBuffer = &pBufferESMBlurUniformH_Primary[i];
			addResource(&ubEsmBlurDescH);
		}

		BufferLoadDesc ubEsmBlurDescV = {};
		ubEsmBlurDescV.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubEsmBlurDescV.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubEsmBlurDescV.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubEsmBlurDescV.mDesc.mSize = sizeof(ESMInputConstants);
		ubEsmBlurDescV.pData = &gESMBlurUniformDataV;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubEsmBlurDescV.ppBuffer = &pBufferESMBlurUniformV[i];
			addResource(&ubEsmBlurDescV);
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
		calcGaussianWeights(&gESMBlurGaussianWeights, gEsmCpuSettings.mFilterWidth);
		BufferLoadDesc esmgwUbDesc = {};
		esmgwUbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		esmgwUbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		esmgwUbDesc.mDesc.mSize = sizeof(GaussianWeightsUniformBlock);
		esmgwUbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		esmgwUbDesc.pData = &gESMBlurGaussianWeights;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			esmgwUbDesc.ppBuffer = &pBufferESMGaussianWeights[i];
			addResource(&esmgwUbDesc);
		}

		BufferLoadDesc skyboxDesc = {};
		skyboxDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		skyboxDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		skyboxDesc.mDesc.mSize = sizeof(SkyboxUniformBlock);
		skyboxDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		skyboxDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			skyboxDesc.ppBuffer = &pBufferSkyboxUniform[i];
			addResource(&skyboxDesc);
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
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif

		ShaderLoadDesc deferredPassShaderDesc = {};
		deferredPassShaderDesc.mStages[0] = { "gbuffer.vert", NULL, 0, FSR_SrcShaders };
		deferredPassShaderDesc.mStages[1] = { "gbuffer.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc deferredShadeShaderDesc = {};
		deferredShadeShaderDesc.mStages[0] = { "finalPass.vert", NULL, 0, FSR_SrcShaders };
		deferredShadeShaderDesc.mStages[1] = { "finalPass.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc shadowPassShaderDesc = {};
		shadowPassShaderDesc.mStages[0] = { "shadowMapPass.vert", NULL, 0, FSR_SrcShaders };
		shadowPassShaderDesc.mStages[1] = { "shadowMapPass.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc esmBlurShaderDesc = {};
		esmBlurShaderDesc.mStages[0] = { "blurESM.vert", NULL, 0, FSR_SrcShaders };
		esmBlurShaderDesc.mStages[1] = { "blurESM.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc sdfShaderDesc = {};
		sdfShaderDesc.mStages[0] = { "sdfSimple.vert", NULL, 0, FSR_SrcShaders };
		sdfShaderDesc.mStages[1] = { "sdfSimple.frag", NULL, 0, FSR_SrcShaders };
		/************************************************************************/
		// Add shaders
		/************************************************************************/
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
		addShader(pRenderer, &shadowPassShaderDesc, &pShaderShadowPass);
		addShader(pRenderer, &deferredPassShaderDesc, &pShaderDeferredPass);
		addShader(pRenderer, &deferredShadeShaderDesc, &pShaderDeferredShade);
		addShader(pRenderer, &esmBlurShaderDesc, &pShaderESMBlur);
		addShader(pRenderer, &sdfShaderDesc, &pShaderSdfSimple);

		/************************************************************************/
		// Add GPU profiler
		/************************************************************************/
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		/************************************************************************/
		// Add samplers
		/************************************************************************/
		SamplerDesc samplerPointDesc = {};
		addSampler(pRenderer, &samplerPointDesc, &pSamplerPoint);

		SamplerDesc samplerPointClampDesc = {};
		samplerPointClampDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerPointClampDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerPointClampDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerPointClampDesc.mMinFilter = FILTER_NEAREST;
		samplerPointClampDesc.mMagFilter = FILTER_NEAREST;
		samplerPointClampDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		addSampler(pRenderer, &samplerPointClampDesc, &pSamplerPointClamp);

		SamplerDesc samplerBiliniearDesc = {};
		samplerBiliniearDesc.mAddressU = ADDRESS_MODE_REPEAT;
		samplerBiliniearDesc.mAddressV = ADDRESS_MODE_REPEAT;
		samplerBiliniearDesc.mAddressW = ADDRESS_MODE_REPEAT;
		samplerBiliniearDesc.mMinFilter = FILTER_LINEAR;
		samplerBiliniearDesc.mMagFilter = FILTER_LINEAR;
		samplerBiliniearDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		addSampler(pRenderer, &samplerBiliniearDesc, &pSamplerBilinear);

		SamplerDesc samplerShadowDesc = {};
		samplerShadowDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mMinFilter = FILTER_LINEAR;
		samplerShadowDesc.mMagFilter = FILTER_LINEAR;
		samplerShadowDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		samplerShadowDesc.mMipLosBias = 0.0f;
		samplerShadowDesc.mMaxAnisotropy = 8.0f;
		addSampler(pRenderer, &samplerShadowDesc, &pSamplerShadow);

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

		SamplerDesc samplerpSamplerSkyboxDesc = {};
		samplerpSamplerSkyboxDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerpSamplerSkyboxDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerpSamplerSkyboxDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerpSamplerSkyboxDesc.mMinFilter = FILTER_LINEAR;
		samplerpSamplerSkyboxDesc.mMagFilter = FILTER_LINEAR;
		samplerpSamplerSkyboxDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		addSampler(pRenderer, &samplerpSamplerSkyboxDesc, &pSamplerSkybox);

		/************************************************************************/
		// setup root signitures
		/************************************************************************/
		RootSignatureDesc shadowMapPassRootDesc = {};
		shadowMapPassRootDesc.ppShaders = &pShaderShadowPass;
		shadowMapPassRootDesc.mShaderCount = 1;
		shadowMapPassRootDesc.mStaticSamplerCount = 0;

		RootSignatureDesc deferredPassRootDesc = {};
		deferredPassRootDesc.ppShaders = &pShaderDeferredPass;
		deferredPassRootDesc.mShaderCount = 1;
		deferredPassRootDesc.ppStaticSamplers = &pSamplerTrilinearAniso;
		deferredPassRootDesc.mStaticSamplerCount = 1;
		const char* deferredPassSamplersNames[] = { "textureSampler" };
		deferredPassRootDesc.ppStaticSamplerNames = deferredPassSamplersNames;

		RootSignatureDesc esmBlurShaderRootDesc = {};
		esmBlurShaderRootDesc.ppShaders = &pShaderESMBlur;
		esmBlurShaderRootDesc.mShaderCount = 1;
		esmBlurShaderRootDesc.ppStaticSamplers = &pSamplerShadow;
		esmBlurShaderRootDesc.mStaticSamplerCount = 1;
		const char* esmblurShaderRootSamplerNames[] = {
			"blurSampler",
		};
		esmBlurShaderRootDesc.ppStaticSamplerNames = esmblurShaderRootSamplerNames;

		RootSignatureDesc deferredShadeRootDesc = {};
		deferredShadeRootDesc.ppShaders = &pShaderDeferredShade;
		deferredShadeRootDesc.mShaderCount = 1;
		Sampler* deferredShadeSamplers[] = { pSamplerShadow, pSamplerTrilinearAniso };
		deferredShadeRootDesc.ppStaticSamplers = deferredShadeSamplers;
		deferredShadeRootDesc.mStaticSamplerCount = 2;
		const char* deferredShadeSamplerNames[] = { "depthSampler", "textureSampler" };
		deferredShadeRootDesc.ppStaticSamplerNames = deferredShadeSamplerNames;

		RootSignatureDesc skyboxRootDesc = {};
		skyboxRootDesc.ppShaders = &pShaderSkybox;
		skyboxRootDesc.mShaderCount = 1;
		skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
		skyboxRootDesc.mStaticSamplerCount = 1;
		const char* skyboxSamplersNames[] = { "skySampler" };
		skyboxRootDesc.ppStaticSamplerNames = skyboxSamplersNames;

		RootSignatureDesc sdfSimpleRootDesc = {};
		sdfSimpleRootDesc.ppShaders = &pShaderSdfSimple;
		sdfSimpleRootDesc.mShaderCount = 1;

		addRootSignature(pRenderer, &deferredPassRootDesc, &pRootSignatureDeferredPass);
		addRootSignature(pRenderer, &shadowMapPassRootDesc, &pRootSignatureShadowPass);
		addRootSignature(pRenderer, &esmBlurShaderRootDesc, &pRootSignatureESMBlur);
		addRootSignature(pRenderer, &deferredShadeRootDesc, &pRootSignatureDeferredShade);
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);
		addRootSignature(pRenderer, &sdfSimpleRootDesc, &pRootSignatureSdfSimple);

		/************************************************************************/
		// setup Rasterizer State
		/************************************************************************/
		RasterizerStateDesc rasterStateDesc = {};
		rasterStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullBack);

		rasterStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullFront);

		rasterStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullNone);

		/************************************************************************/
		finishResourceLoading();
		/************************************************************************/
		////////////////////////////////////////////////

		/************************************************************************/
		// Initialize Resources
		/************************************************************************/

		gESMBlurUniformDataH_Primary.mIfHorizontalBlur = 1;
		gESMBlurUniformDataH_Primary.mBlurWidth = gEsmCpuSettings.mFilterWidth;
		gESMBlurUniformDataH_Primary.mWindowDimension.x = (float)(mSettings.mWidth);
		gESMBlurUniformDataH_Primary.mWindowDimension.y = (float)(mSettings.mHeight);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			BufferUpdateDesc esmBlurBufferCbvH = { pBufferESMBlurUniformH_Primary[i], &gESMBlurUniformDataH_Primary };
			updateResource(&esmBlurBufferCbvH);
		}
		gESMBlurUniformDataV.mIfHorizontalBlur = 0;
		gESMBlurUniformDataV.mBlurWidth = gEsmCpuSettings.mFilterWidth;
		gESMBlurUniformDataV.mWindowDimension.x = (float)(mSettings.mWidth);
		gESMBlurUniformDataV.mWindowDimension.y = (float)(mSettings.mHeight);
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			BufferUpdateDesc esmBlurBufferCbvV = { pBufferESMBlurUniformV[i], &gESMBlurUniformDataV };
			updateResource(&esmBlurBufferCbvV);
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
		removeRenderTarget(pRenderer, pRenderTargetDepth);
		for (int i = 0; i < 2; i++)
		{
			removeRenderTarget(pRenderer, pRenderTargetESMBlur[i]);
		}
		removeRenderTarget(pRenderer, pRenderTargetShadowMap);
		removeRenderTarget(pRenderer, pRenderTargetSkybox);
		removeRenderTarget(pRenderer, pRenderTargetSdfSimple);

		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			removeRenderTarget(pRenderer, pRenderTargetDeferredPass[i]);
		}
		removeSwapChain(pRenderer, pSwapChain);
	}
	void Exit() override
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
		destroyCameraController(pCameraController);
		destroyCameraController(pLightView);

		gAppUI.Exit();
		removeDebugRendererInterface();
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferObjectTransforms[i]);
			removeResource(pBufferLightUniform[i]);
			removeResource(pBufferESMBlurUniformH_Primary[i]);
			removeResource(pBufferESMBlurUniformV[i]);
			removeResource(pBufferESMGaussianWeights[i]);
			removeResource(pBufferRenderSettings[i]);
			removeResource(pBufferSkyboxUniform[i]);
			removeResource(pBufferCameraUniform[i]);
			removeResource(pBufferSdfInputUniform[i]);
		}
		removeResource(pBufferSkyboxVertex);
		removeResource(pBufferSphereVertex);
#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerShadow);
		removeSampler(pRenderer, pSamplerPointClamp);
		removeSampler(pRenderer, pSamplerSkybox);
		removeSampler(pRenderer, pSamplerPoint);

		removeShader(pRenderer, pShaderDeferredPass);
		removeShader(pRenderer, pShaderESMBlur);
		removeShader(pRenderer, pShaderDeferredShade);
		removeShader(pRenderer, pShaderShadowPass);
		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderSdfSimple);

		removeRootSignature(pRenderer, pRootSignatureDeferredPass);
		removeRootSignature(pRenderer, pRootSignatureDeferredShade);
		removeRootSignature(pRenderer, pRootSignatureESMBlur);
		removeRootSignature(pRenderer, pRootSignatureShadowPass);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureSdfSimple);

		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateDisable);

		removeRasterizerState(pRasterizerStateCullBack);
		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);

		for (uint i = 0; i < 6; ++i)
			removeResource(pTextureSkybox[i]);

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
		// Setup the resources needed for the Deferred Pass Pipeline
		/************************************************************************/
		ImageFormat::Enum deferredFormats[DEFERRED_RT_COUNT] = {};
		bool              deferredSrgb[DEFERRED_RT_COUNT] = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			deferredFormats[i] = pRenderTargetDeferredPass[i]->mDesc.mFormat;
			deferredSrgb[i] = pRenderTargetDeferredPass[i]->mDesc.mSrgb;
		}

		//layout and pipeline for skybox draw
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;
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
		/*------------------------------------------*/
		// Setup pipelines for deferred pass shader
		/*------------------------------------------*/
		GraphicsPipelineDesc pipelineSettings = { NULL };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = DEFERRED_RT_COUNT;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.pColorFormats = deferredFormats;
		pipelineSettings.pSrgbValues = deferredSrgb;
		pipelineSettings.mSampleCount = pRenderTargetDepth->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pRenderTargetDepth->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureDeferredPass;
		pipelineSettings.pVertexLayout = &vertexLayoutRegular;
		pipelineSettings.pShaderProgram = pShaderDeferredPass;
		pipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineDeferredPass);
		/************************************************************************/
		// Setup the resources needed for the Deferred Shade Pipeline
		/******************************/
		GraphicsPipelineDesc deferredShadePipelineSettings = {};
		deferredShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		deferredShadePipelineSettings.mRenderTargetCount = 1;
		deferredShadePipelineSettings.pDepthState = pDepthStateDisable;
		deferredShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		deferredShadePipelineSettings.pRootSignature = pRootSignatureDeferredShade;
		deferredShadePipelineSettings.pShaderProgram = pShaderDeferredShade;
		deferredShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		deferredShadePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		deferredShadePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		deferredShadePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		addPipeline(pRenderer, &deferredShadePipelineSettings, &pPipelineDeferredShadeSrgb);

		/************************************************************************/
		// Setup the resources needed for shadow map
		/************************************************************************/
		GraphicsPipelineDesc shadowMapPipelineSettings = {};
		shadowMapPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowMapPipelineSettings.mRenderTargetCount = 1;
		shadowMapPipelineSettings.pDepthState = pDepthStateEnable;
		shadowMapPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		shadowMapPipelineSettings.pColorFormats = &pRenderTargetShadowMap->mDesc.mFormat;
		shadowMapPipelineSettings.pSrgbValues = &pRenderTargetShadowMap->mDesc.mSrgb;
		shadowMapPipelineSettings.mSampleCount = pRenderTargetShadowMap->mDesc.mSampleCount;
		shadowMapPipelineSettings.mSampleQuality = pRenderTargetShadowMap->mDesc.mSampleQuality;
		shadowMapPipelineSettings.pRootSignature = pRootSignatureShadowPass;
		shadowMapPipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		shadowMapPipelineSettings.pShaderProgram = pShaderShadowPass;
		shadowMapPipelineSettings.pVertexLayout = &vertexLayoutRegular;
		addPipeline(pRenderer, &shadowMapPipelineSettings, &pPipelineShadowPass);
		/*-----------------------------------------------------------*/
		// Setup the resources needed for the ESM Blur Shade Pipeline
		/*-----------------------------------------------------------*/
		GraphicsPipelineDesc esmBlurPipelineSettings = {};
		esmBlurPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		esmBlurPipelineSettings.mRenderTargetCount = 1;
		esmBlurPipelineSettings.pDepthState = pDepthStateDisable;
		esmBlurPipelineSettings.pColorFormats = &pRenderTargetESMBlur[0]->mDesc.mFormat;
		esmBlurPipelineSettings.pSrgbValues = &pRenderTargetESMBlur[0]->mDesc.mSrgb;
		esmBlurPipelineSettings.mSampleCount = pRenderTargetESMBlur[0]->mDesc.mSampleCount;
		esmBlurPipelineSettings.mSampleQuality = pRenderTargetESMBlur[0]->mDesc.mSampleQuality;
		esmBlurPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		esmBlurPipelineSettings.pRootSignature = pRootSignatureESMBlur;
		esmBlurPipelineSettings.pShaderProgram = pShaderESMBlur;
		addPipeline(pRenderer, &esmBlurPipelineSettings, &pPipelineESMBlur);

		/************************************************************************/
		// Setup the resources needed for Skybox
		/************************************************************************/
		GraphicsPipelineDesc skyboxPipelineSettings = {};
		skyboxPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		skyboxPipelineSettings.mRenderTargetCount = 1;
		skyboxPipelineSettings.pDepthState = pDepthStateEnable;
		skyboxPipelineSettings.pColorFormats = &pRenderTargetSkybox->mDesc.mFormat;
		skyboxPipelineSettings.pSrgbValues = &pRenderTargetSkybox->mDesc.mSrgb;
		skyboxPipelineSettings.mSampleCount = pRenderTargetSkybox->mDesc.mSampleCount;
		skyboxPipelineSettings.mSampleQuality = pRenderTargetSkybox->mDesc.mSampleQuality;
		skyboxPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		skyboxPipelineSettings.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineSettings.pShaderProgram = pShaderSkybox;
		skyboxPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		skyboxPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &skyboxPipelineSettings, &pPipelineSkybox);

		/************************************************************************/
		// Setup the resources needed for Sdf Simple
		/************************************************************************/
		GraphicsPipelineDesc sdfSimplePipelineSettings = {};
		sdfSimplePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		sdfSimplePipelineSettings.mRenderTargetCount = 1;
		sdfSimplePipelineSettings.pDepthState = pDepthStateDisable;
		sdfSimplePipelineSettings.pColorFormats = &pRenderTargetSdfSimple->mDesc.mFormat;
		sdfSimplePipelineSettings.pSrgbValues = &pRenderTargetSdfSimple->mDesc.mSrgb;
		sdfSimplePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		sdfSimplePipelineSettings.mSampleQuality = pRenderTargetSdfSimple->mDesc.mSampleQuality;
		sdfSimplePipelineSettings.pRootSignature = pRootSignatureSdfSimple;
		sdfSimplePipelineSettings.pShaderProgram = pShaderSdfSimple;
		sdfSimplePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &sdfSimplePipelineSettings, &pPipelineSdfSimple);

		return true;
	}

	void Unload() override
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineDeferredPass);
		removePipeline(pRenderer, pPipelineESMBlur);
		removePipeline(pRenderer, pPipelineDeferredShadeSrgb);
		removePipeline(pRenderer, pPipelineShadowPass);
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineSdfSimple);

		RemoveRenderTargetsAndSwapChian();
	}

	void Update(float deltaTime) override
	{
		/************************************************************************/
		// Input
		/************************************************************************/
		if (getKeyDown(KEY_BUTTON_X))
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
		currentTime += deltaTime * 1000.0f;

		// update camera with time
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 1.0f, 4000.0f);    //view matrix

		gObjectInfoUniformData.mViewProject = projMat * viewMat;
		/************************************************************************/
		// Update Camera
		/************************************************************************/
		gCameraUniformData.mPosition = vec4(pCameraController->getViewPosition(), 1);
		/************************************************************************/
		// Update Skybox
		/************************************************************************/
		viewMat.setTranslation(vec3(0, 0, 0));
		gSkyboxUniformData.mViewProject = projMat * viewMat;
		/************************************************************************/
		// Light Matrix Update - for shadow map
		/************************************************************************/
		vec3 lightPos = vec3(gLightCpuSettings.mLightPosition.x, gLightCpuSettings.mLightPosition.y, gLightCpuSettings.mLightPosition.z);
		pLightView->moveTo(lightPos);
		pLightView->lookAt(gObjectsCenter);

		mat4 lightViewMat = pLightView->getViewMatrix();
		//perspective as spotlight, for future use
		mat4 lightProjMat = mat4::perspective(
			PI / 1.2f, aspectInverse, gESMBlurUniformDataH_Primary.mNearFarDist.x, gESMBlurUniformDataH_Primary.mNearFarDist.y);
		gLightUniformData.mLightDirection = vec4(gObjectsCenter - lightPos, 0);
		gLightUniformData.mLightViewProj = lightProjMat * lightViewMat;
		gLightUniformData.mLightColor = vec4(1, 1, 1, 1);
		/************************************************************************/
		// Update ESM
		/************************************************************************/
		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			gESMBlurUniformDataH_Primary.mWindowDimension.x = (float)(mSettings.mWidth);
			gESMBlurUniformDataH_Primary.mWindowDimension.y = (float)(mSettings.mHeight);
			gESMBlurUniformDataV.mWindowDimension.x = (float)(mSettings.mWidth);
			gESMBlurUniformDataV.mWindowDimension.y = (float)(mSettings.mHeight);
			gESMBlurUniformDataH_Primary.mBlurWidth = gEsmCpuSettings.mFilterWidth;
			gESMBlurUniformDataV.mBlurWidth = gEsmCpuSettings.mFilterWidth;
			calcGaussianWeights(&gESMBlurGaussianWeights, gEsmCpuSettings.mFilterWidth);
		}
		/************************************************************************/
		// Update SDF Settings
		/************************************************************************/

		float const rads = 0.0001f * currentTime;
		gSdfUniformData.mCameraPosition = gCameraUniformData.mPosition;
		gSdfUniformData.mViewInverse = transpose(pCameraController->getViewMatrix());    //transpose to invert
		gSdfUniformData.mWindowDimension.x = (float)(mSettings.mWidth);
		gSdfUniformData.mWindowDimension.y = (float)(mSettings.mHeight);
		gSdfUniformData.mSphereRadius = gSphereRadius;
		gSdfUniformData.mRadsRot = rads;

		// Rotate spheres
		for (int i = 0; i < SPHERE_NUM; i++)
		{
			gObjectInfoUniformData.mToWorldMat[i] =
				mat4::rotationY(rads) * gObjectInfoData[i].mTranslationMat * gObjectInfoData[i].mScaleMat;
		}
		/************************************************************************/
		gAppUI.Update(deltaTime);
	}

	static void drawDeferredPass(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		for (uint32_t i = 0; i < DEFERRED_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetDeferredPass[i]->mDesc.mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, DEFERRED_RT_COUNT, pRenderTargetDeferredPass, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);

		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetDeferredPass[0]->mDesc.mWidth, (float)pRenderTargetDeferredPass[0]->mDesc.mHeight, 0.0f,
			1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetDeferredPass[0]->mDesc.mWidth, pRenderTargetDeferredPass[0]->mDesc.mHeight);

		// Draw the skybox.
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw GBuffers");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Render G-Buffer", true);

		cmdBindVertexBuffer(cmd, 1, &pBufferSphereVertex, NULL);

		cmdBindPipeline(cmd, pPipelineDeferredPass);

		DescriptorData params[3] = {};
		params[0].pName = "objectUniformBlock";
		params[0].ppBuffers = &pBufferObjectTransforms[gFrameIndex];
		params[1].pName = "SphereTex";
		params[1].ppTextures = &pTextureScene[0];
		params[2].pName = "PlaneTex";
		params[2].ppTextures = &pTextureScene[1];

		cmdBindDescriptors(cmd, pRootSignatureDeferredPass, 3, params);
		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, SPHERE_NUM, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}

	static void drawDeferredShade(Cmd* cmd)
	{
		RenderTarget* pDestinationRenderTarget = pRenderTargetScreen;

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetScreen->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pDestinationRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pDestinationRenderTarget->mDesc.mWidth, (float)pDestinationRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pDestinationRenderTarget->mDesc.mWidth, pDestinationRenderTarget->mDesc.mHeight);

		// Draw the skybox.
		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Final Render Pass");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Final Render Pass", true);

		cmdBindPipeline(cmd, pPipelineDeferredShadeSrgb);

		DescriptorData params[11] = {};
		params[0].pName = "gBufferColor";
		params[0].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture;
		params[1].pName = "gBufferNormal";
		params[1].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture;
		params[2].pName = "gBufferPosition";
		params[2].ppTextures = &pRenderTargetDeferredPass[DEFERRED_RT_POSITION]->pTexture;
		params[3].pName = "gBufferDepth";
		params[3].ppTextures = &pRenderTargetDepth->pTexture;
		params[4].pName = "lightUniformBlock";
		params[4].ppBuffers = &pBufferLightUniform[gFrameIndex];
		params[5].pName = "renderSettingUniformBlock";
		params[5].ppBuffers = &pBufferRenderSettings[gFrameIndex];
		params[6].pName = "ESMInputConstants";
		params[6].ppBuffers = &pBufferESMBlurUniformH_Primary[gFrameIndex];
		params[7].pName = "shadowMap";
		params[7].ppTextures = &pRenderTargetESMBlur[1]->pTexture;
		params[8].pName = "skyboxTex";
		params[8].ppTextures = &pRenderTargetSkybox->pTexture;
		params[9].pName = "sdfScene";
		params[9].ppTextures = &pRenderTargetSdfSimple->pTexture;
		params[10].pName = "cameraUniform";
		params[10].ppBuffers = &pBufferCameraUniform[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignatureDeferredShade, 11, params);

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}
	void drawSdfShadow(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetSdfSimple->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTargetSdfSimple, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetSdfSimple->mDesc.mWidth, (float)pRenderTargetSdfSimple->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetSdfSimple->mDesc.mWidth, pRenderTargetSdfSimple->mDesc.mHeight);

		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw SDF Shadow");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw SDF Shadow", true);
		cmdBindPipeline(cmd, pPipelineSdfSimple);

		DescriptorData params[2] = {};
		params[0].pName = "lightUniformBlock";
		params[0].ppBuffers = &pBufferLightUniform[gFrameIndex];
		params[1].pName = "sdfUniformBlock";
		params[1].ppBuffers = &pBufferSdfInputUniform[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignatureSdfSimple, 2, params);

		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}

	static void blurEsmMap(Cmd* cmd, uint32_t rtId)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetESMBlur[rtId]->mDesc.mClearValue;
		cmdBeginDebugMarker(cmd, 1, 0, 1, rtId == 0 ? "Blur ESM Pass H" : "Blur ESM Pass V");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, rtId == 0 ? "Blur ESM Pass H" : "Blur ESM Pass V");
		cmdBindRenderTargets(cmd, 1, &pRenderTargetESMBlur[rtId], NULL, &loadActions, NULL, NULL, -1, -1);

		DescriptorData params[3] = {};
		params[0].pName = "shadowExpMap";
		params[0].ppTextures = rtId == 0 ? &pRenderTargetShadowMap->pTexture : &pRenderTargetESMBlur[0]->pTexture;
		params[1].pName = "ESMInputConstants";
		params[1].ppBuffers = rtId == 0 ? &pBufferESMBlurUniformH_Primary[gFrameIndex] : &pBufferESMBlurUniformV[gFrameIndex];
		params[2].pName = "GaussianWeightsBuffer";
		params[2].ppBuffers = &pBufferESMGaussianWeights[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignatureESMBlur, 3, params);
		cmdBindPipeline(cmd, pPipelineESMBlur);
		// A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
		cmdDraw(cmd, 3, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}

	static void drawEsmShadowMap(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetShadowMap->mDesc.mClearValue;

		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		cmdBeginDebugMarker(cmd, 1, 0, 1, "Draw Shadow Map");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw ESM Shadow Map");
		// Start render pass and apply load actions
		cmdBindRenderTargets(cmd, 1, &pRenderTargetShadowMap, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			cmd, 0.0f, 0.0f, (float)pRenderTargetShadowMap->mDesc.mWidth, (float)pRenderTargetShadowMap->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetShadowMap->mDesc.mWidth, pRenderTargetShadowMap->mDesc.mHeight);

		cmdBindVertexBuffer(cmd, 1, &pBufferSphereVertex, NULL);

		cmdBindPipeline(cmd, pPipelineShadowPass);
		DescriptorData params[3] = {};
		params[0].pName = "objectUniformBlock";
		params[0].ppBuffers = &pBufferObjectTransforms[gFrameIndex];
		params[1].pName = "lightUniformBlock";
		params[1].ppBuffers = &pBufferLightUniform[gFrameIndex];
		params[2].pName = "ESMInputConstants";
		params[2].ppBuffers = &pBufferESMBlurUniformH_Primary[gFrameIndex];
		cmdBindDescriptors(cmd, pRootSignatureShadowPass, 3, params);

		cmdDrawInstanced(cmd, gNumberOfSpherePoints / 6, 0, SPHERE_NUM, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}
	static void drawSkybox(Cmd* cmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetSkybox->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		cmdBeginDebugMarker(cmd, 0, 0, 1, "Draw skybox");
		cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Draw Skybox", true);

		cmdBindRenderTargets(cmd, 1, &pRenderTargetSkybox, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);

		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetSkybox->mDesc.mWidth, (float)pRenderTargetSkybox->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetSkybox->mDesc.mWidth, pRenderTargetSkybox->mDesc.mHeight);

		cmdBindPipeline(cmd, pPipelineSkybox);

		DescriptorData params[7] = {};
		params[0].pName = "skyboxUniformBlock";
		params[0].ppBuffers = &pBufferSkyboxUniform[gFrameIndex];
		params[1].pName = "RightText";
		params[1].ppTextures = &pTextureSkybox[0];
		params[2].pName = "LeftText";
		params[2].ppTextures = &pTextureSkybox[1];
		params[3].pName = "TopText";
		params[3].ppTextures = &pTextureSkybox[2];
		params[4].pName = "BotText";
		params[4].ppTextures = &pTextureSkybox[3];
		params[5].pName = "FrontText";
		params[5].ppTextures = &pTextureSkybox[4];
		params[6].pName = "BackText";
		params[6].ppTextures = &pTextureSkybox[5];
		cmdBindDescriptors(cmd, pRootSignatureSkybox, 7, params);
		cmdBindVertexBuffer(cmd, 1, &pBufferSkyboxVertex, NULL);
		cmdDraw(cmd, 36, 0);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
		cmdEndDebugMarker(cmd);
	}

	static void renderTargetBarriers(Cmd* cmd)
	{
		tinystl::vector<TextureBarrier> rtBarrier;
		//skybox
		rtBarrier.push_back({ pRenderTargetSkybox->pTexture, RESOURCE_STATE_RENDER_TARGET });
		//G-buffer
		rtBarrier.push_back({ pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture, RESOURCE_STATE_RENDER_TARGET });
		rtBarrier.push_back({ pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture, RESOURCE_STATE_RENDER_TARGET });
		rtBarrier.push_back({ pRenderTargetDeferredPass[DEFERRED_RT_POSITION]->pTexture, RESOURCE_STATE_RENDER_TARGET });

		//ESM
		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			rtBarrier.push_back({ pRenderTargetShadowMap->pTexture, RESOURCE_STATE_RENDER_TARGET });
			rtBarrier.push_back({ pRenderTargetESMBlur[0]->pTexture, RESOURCE_STATE_RENDER_TARGET });
			rtBarrier.push_back({ pRenderTargetESMBlur[1]->pTexture, RESOURCE_STATE_RENDER_TARGET });
		}
		//SDF
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			rtBarrier.push_back({ pRenderTargetSdfSimple->pTexture, RESOURCE_STATE_RENDER_TARGET });
		}
		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)rtBarrier.size(), &rtBarrier.front(), true);
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
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);
		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/
		BufferUpdateDesc renderSettingCbv = { pBufferRenderSettings[gFrameIndex], &gRenderSettings };
		updateResource(&renderSettingCbv);

		BufferUpdateDesc viewProjCbv = { pBufferObjectTransforms[gFrameIndex], &gObjectInfoUniformData };
		updateResource(&viewProjCbv);

		BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex], &gCameraUniformData };
		updateResource(&cameraCbv);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferSkyboxUniform[gFrameIndex], &gSkyboxUniformData };
		updateResource(&skyboxViewProjCbv);

		BufferUpdateDesc lightBufferCbv = { pBufferLightUniform[gFrameIndex], &gLightUniformData };
		updateResource(&lightBufferCbv);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			BufferUpdateDesc esmBlurCbvH = { pBufferESMBlurUniformH_Primary[gFrameIndex], &gESMBlurUniformDataH_Primary };
			BufferUpdateDesc esmBlurCbvV = { pBufferESMBlurUniformV[gFrameIndex], &gESMBlurUniformDataV };
			BufferUpdateDesc esmBlurWeights = { pBufferESMGaussianWeights[gFrameIndex], &gESMBlurGaussianWeights };
			updateResource(&esmBlurCbvH);
			updateResource(&esmBlurCbvV);
			updateResource(&esmBlurWeights);
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
		renderTargetBarriers(cmd);
		TextureBarrier barriers1[] = {
			{ pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers1, false);

		cmdFlushBarriers(cmd);

		////////////////////////////////////////////////////////
		//  Draw Skybox
		drawSkybox(cmd);
		TextureBarrier barriersSky[] = {
			{ pRenderTargetSkybox->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
		};
		cmdResourceBarrier(cmd, 0, NULL, 1, barriersSky, true);
		cmdFlushBarriers(cmd);

		////////////////////////////////////////////////////////
		///// Draw ESM Map
		if (gRenderSettings.mRenderOutput == RENDER_OUTPUT_ESM_MAP || gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			drawEsmShadowMap(cmd);

			TextureBarrier drawShadowMapBarrier[] = { { pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
			cmdResourceBarrier(cmd, 0, NULL, 1, drawShadowMapBarrier, false);
			cmdFlushBarriers(cmd);
			////////////////////////////////////////////////////////
			///// Blur ESM Map
			for (int i = 0; i < 2; i++)
			{
				blurEsmMap(cmd, i);
				TextureBarrier blurEsmBarrier = { pRenderTargetESMBlur[i]->pTexture, RESOURCE_STATE_SHADER_RESOURCE };
				cmdResourceBarrier(cmd, 0, NULL, 1, &blurEsmBarrier, false);
				cmdFlushBarriers(cmd);
			}
		}
		else if (gRenderSettings.mRenderOutput == RENDER_OUTPUT_SDF_MAP || gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			drawSdfShadow(cmd);
		}

		////////////////////////////////////////////////////////
		///// Fill GBuffer
		drawDeferredPass(cmd);

		TextureBarrier barriers2[] = { { pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									   { pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									   { pRenderTargetDeferredPass[DEFERRED_RT_POSITION]->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									   { pRenderTargetDepth->pTexture, RESOURCE_STATE_SHADER_RESOURCE },
									   { pRenderTargetShadowMap->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, DEFERRED_RT_COUNT + 2, barriers2, true);
		cmdFlushBarriers(cmd);

		////////////////////////////////////////////////////////
		///// Last Step: Combine Everything
		TextureBarrier drawSdfBarrier[] = { { pRenderTargetSdfSimple->pTexture, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, drawSdfBarrier, false);
		drawDeferredShade(cmd);
		////////////////////////////////////////////////////////

		////////////////////////////////////////////////////////
		//  Draw UIs
		cmdBeginDebugMarker(cmd, 0, 1, 0, "Draw UI");
		//cmdBeginRender(cmd, 1, &pRenderTargetScreen, NULL, NULL);
		cmdBindRenderTargets(cmd, 1, &pRenderTargetScreen, NULL, NULL, NULL, NULL, -1, -1);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifndef TARGET_IOS
		drawDebugText(cmd, 8.0f, 15.0f, tinystl::string::format("CPU Time: %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);

		drawDebugGpuProfile(cmd, 8, 65, pGpuProfiler, NULL);
#else
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pGuiWindow);
		gAppUI.Draw(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndDebugMarker(cmd);
		////////////////////////////////////////////////////////

		barriers1[0] = { pRenderTargetScreen->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers1, true);

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

		const ClearValue depthClear = { 1.0f, 0 };
		const ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };
		const ClearValue colorClearWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

		/************************************************************************/
		// Main depth buffer
		/************************************************************************/
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue = depthClear;
		depthRT.mDepth = 1;
		depthRT.mFormat = ImageFormat::D32F;
		depthRT.mWidth = width;
		depthRT.mHeight = height;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.pDebugName = L"Depth RT";
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);
		/************************************************************************/
		// Shadow Map Render Target
		/************************************************************************/
		RenderTargetDesc shadowRTDesc = {};
		shadowRTDesc.mArraySize = 1;
		shadowRTDesc.mClearValue = colorClearWhite;
		shadowRTDesc.mDepth = 1;
		shadowRTDesc.mFormat = ImageFormat::R32F;    //ESM needs full width
		shadowRTDesc.mWidth = width;
		shadowRTDesc.mHeight = height;
		shadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
		shadowRTDesc.mSampleQuality = 0;
		shadowRTDesc.pDebugName = L"Shadow Map RT";

		addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadowMap);
		/************************************************************************/
		// Deferred pass render targets
		/************************************************************************/
		RenderTargetDesc deferredRTDesc = {};
		deferredRTDesc.mArraySize = 1;
		deferredRTDesc.mClearValue = colorClearBlack;
		deferredRTDesc.mDepth = 1;
		deferredRTDesc.mFormat = ImageFormat::RGBA8;
		deferredRTDesc.mWidth = width;
		deferredRTDesc.mHeight = height;
		deferredRTDesc.mSampleCount = SAMPLE_COUNT_1;
		deferredRTDesc.mSampleQuality = 0;
		deferredRTDesc.pDebugName = L"G-Buffer RTs";
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[DEFERRED_RT_ALBEDO]);
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[DEFERRED_RT_NORMAL]);

		deferredRTDesc.mFormat = ImageFormat::RGBA32F;    // use 32-bit float for world scale position
		addRenderTarget(pRenderer, &deferredRTDesc, &pRenderTargetDeferredPass[DEFERRED_RT_POSITION]);

		/************************************************************************/
		// Skybox render target
		/************************************************************************/
		RenderTargetDesc skyboxRTDesc = {};
		skyboxRTDesc.mArraySize = 1;
		skyboxRTDesc.mClearValue = colorClearBlack;
		skyboxRTDesc.mDepth = 1;
		skyboxRTDesc.mFormat = ImageFormat::RGBA8;
		skyboxRTDesc.mWidth = width;
		skyboxRTDesc.mHeight = height;
		skyboxRTDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxRTDesc.mSampleQuality = 0;
		skyboxRTDesc.pDebugName = L"Skybox RT";
		addRenderTarget(pRenderer, &skyboxRTDesc, &pRenderTargetSkybox);

		/************************************************************************/
		// Sdf render target
		/************************************************************************/
		RenderTargetDesc sdfSimpleRTDesc = {};
		sdfSimpleRTDesc.mArraySize = 1;
		sdfSimpleRTDesc.mClearValue = colorClearBlack;
		sdfSimpleRTDesc.mDepth = 1;
		sdfSimpleRTDesc.mFormat = ImageFormat::RGBA8;    //TODO: use R8 for as shadow factor
		sdfSimpleRTDesc.mWidth = width;
		sdfSimpleRTDesc.mHeight = height;
		sdfSimpleRTDesc.mSampleCount = SAMPLE_COUNT_1;
		sdfSimpleRTDesc.mSampleQuality = 0;
		sdfSimpleRTDesc.pDebugName = L"Sdf RT";
		addRenderTarget(pRenderer, &sdfSimpleRTDesc, &pRenderTargetSdfSimple);

		/************************************************************************/
		// ESM Blur render target
		/************************************************************************/
		RenderTargetDesc esmBlurRTDesc = {};
		esmBlurRTDesc.mArraySize = 1;
		esmBlurRTDesc.mClearValue = colorClearWhite;
		esmBlurRTDesc.mDepth = 1;
		esmBlurRTDesc.mFormat = ImageFormat::R32F;
		esmBlurRTDesc.mWidth = width;
		esmBlurRTDesc.mHeight = height;
		esmBlurRTDesc.mSampleCount = SAMPLE_COUNT_1;
		esmBlurRTDesc.mSampleQuality = 0;
		esmBlurRTDesc.pDebugName = L"ESM Blur RT H";
		addRenderTarget(pRenderer, &esmBlurRTDesc, &pRenderTargetESMBlur[0]);
		esmBlurRTDesc.pDebugName = L"ESM Blur RT V";
		addRenderTarget(pRenderer, &esmBlurRTDesc, &pRenderTargetESMBlur[1]);

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
			GuiController::esmDynamicWidgets.HideDynamicProperties(pGuiWindow);
		else if (GuiController::currentlyShadowType == SHADOW_TYPE_SDF)
			GuiController::sdfDynamicWidgets.HideDynamicProperties(pGuiWindow);

		if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
		{
			GuiController::esmDynamicWidgets.ShowDynamicProperties(pGuiWindow);
		}
		else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
		{
			GuiController::sdfDynamicWidgets.ShowDynamicProperties(pGuiWindow);
		}

		GuiController::currentlyShadowType = (ShadowType)gRenderSettings.mShadowType;
	}
}

void GuiController::addGui()
{
#if DEBUG_OUTPUT
	static const char* renderModeNames[] = {
		"Scene", "SDF Soft Shadow", "Albedo", "World Normal [0,1]", "World Position", "Camera Depth", "ESM Map",
		NULL    //needed for unix
	};
	static const uint32_t renderModeValues[] = {
		RENDER_OUTPUT_SCENE,
		RENDER_OUTPUT_SDF_MAP,
		RENDER_OUTPUT_ALBEDO,
		RENDER_OUTPUT_NORMAL,
		RENDER_OUTPUT_POSITION,
		RENDER_OUTPUT_DEPTH,
		RENDER_OUTPUT_ESM_MAP,
		0    //needed for unix
	};

	pGuiWindow->AddWidget(DropdownWidget("Render Output", &gRenderSettings.mRenderOutput, renderModeNames, renderModeValues, 7));
#endif
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
		static SliderUintWidget esmSoftness("ESM Softness", &gEsmCpuSettings.mFilterWidth, 0u, MAX_GAUSSIAN_WIDTH);
		GuiController::esmDynamicWidgets.mDynamicProperties.emplace_back(&esmSoftness);

		static SliderFloatWidget esmDarkness("ESM Darkness", &gESMBlurUniformDataH_Primary.mExponent, 0.0f, 240.0f, 0.1f);
		GuiController::esmDynamicWidgets.mDynamicProperties.emplace_back(&esmDarkness);
	}
	// SDF dynamic widgets
	{
		static SliderFloatWidget sdfHardness("SDF Shadow Hardness", &gSdfUniformData.mShadowHardness, 3, 20, 0.01f);
		GuiController::sdfDynamicWidgets.mDynamicProperties.emplace_back(&sdfHardness);

		static SliderUintWidget sdfMaxIter("SDF Max Iteration", &gSdfUniformData.mMaxIteration, 32u, 1024u, 1u);
		GuiController::sdfDynamicWidgets.mDynamicProperties.emplace_back(&sdfMaxIter);
	}

	if (gRenderSettings.mShadowType == SHADOW_TYPE_ESM)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_ESM;
		GuiController::esmDynamicWidgets.ShowDynamicProperties(pGuiWindow);
	}
	else if (gRenderSettings.mShadowType == SHADOW_TYPE_SDF)
	{
		GuiController::currentlyShadowType = SHADOW_TYPE_SDF;
		GuiController::sdfDynamicWidgets.ShowDynamicProperties(pGuiWindow);
	}
	else
		GuiController::currentlyShadowType = SHADOW_TYPE_NONE;
}

DEFINE_APPLICATION_MAIN(LightShadowPlayground)
