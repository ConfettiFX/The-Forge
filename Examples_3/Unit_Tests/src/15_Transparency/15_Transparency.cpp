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

#define MAX_NUM_OBJECTS 64 // Should be less than 1023
#define MAX_NUM_PARTICLES 2048 // Per system
#define CUBES_EACH_ROW 5
#define CUBES_EACH_COL 5
#define CUBE_NUM (CUBES_EACH_ROW*CUBES_EACH_COL + 1)
#define DEBUG_OUTPUT 1//exclusively used for texture data visulization, such as rendering depth, shadow map etc.
#define AOIT_NODE_COUNT 4	// 2, 4 or 8. Higher numbers give better results at the cost of performance
#if AOIT_NODE_COUNT == 2
#define AOIT_RT_COUNT 1
#else
#define AOIT_RT_COUNT (AOIT_NODE_COUNT / 4)
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
	10.0f,  -10.0f, -10.0f,6.0f, // -z
	-10.0f, -10.0f, -10.0f,6.0f,
	-10.0f, 10.0f, -10.0f,6.0f,
	-10.0f, 10.0f, -10.0f,6.0f,
	10.0f,  10.0f, -10.0f,6.0f,
	10.0f,  -10.0f, -10.0f,6.0f,

	-10.0f, -10.0f,  10.0f,2.0f,  //-x
	-10.0f, -10.0f, -10.0f,2.0f,
	-10.0f,  10.0f, -10.0f,2.0f,
	-10.0f,  10.0f, -10.0f,2.0f,
	-10.0f,  10.0f,  10.0f,2.0f,
	-10.0f, -10.0f,  10.0f,2.0f,

	10.0f, -10.0f, -10.0f,1.0f, //+x
	10.0f, -10.0f,  10.0f,1.0f,
	10.0f,  10.0f,  10.0f,1.0f,
	10.0f,  10.0f,  10.0f,1.0f,
	10.0f,  10.0f, -10.0f,1.0f,
	10.0f, -10.0f, -10.0f,1.0f,

	-10.0f, -10.0f,  10.0f,5.0f,  // +z
	-10.0f,  10.0f,  10.0f,5.0f,
	10.0f,  10.0f,  10.0f,5.0f,
	10.0f,  10.0f,  10.0f,5.0f,
	10.0f, -10.0f,  10.0f,5.0f,
	-10.0f, -10.0f,  10.0f,5.0f,

	-10.0f,  10.0f, -10.0f, 3.0f,  //+y
	10.0f,  10.0f, -10.0f,3.0f,
	10.0f,  10.0f,  10.0f,3.0f,
	10.0f,  10.0f,  10.0f,3.0f,
	-10.0f,  10.0f,  10.0f,3.0f,
	-10.0f,  10.0f, -10.0f,3.0f,

	10.0f,  -10.0f, 10.0f, 4.0f,  //-y
	10.0f,  -10.0f, -10.0f,4.0f,
	-10.0f,  -10.0f,  -10.0f,4.0f,
	-10.0f,  -10.0f,  -10.0f,4.0f,
	-10.0f,  -10.0f,  10.0f,4.0f,
	10.0f,  -10.0f, 10.0f,4.0f,
};

typedef struct Vertex
{
	float3 mPosition;
	float3 mNormal;
} Vertex;

typedef struct Object
{
	vec3 mPosition;
	vec3 mScale;
	vec3 mOrientation;
	vec4 mColor;
} Object;

typedef struct ParticleSystem
{
	Buffer* pParticleBuffer;
	Object mObject;

	tinystl::vector<vec3> mParticlePositions;
	tinystl::vector<vec3> mParticleVelocities;
	tinystl::vector<float> mParticleLifetimes;
	size_t mLifeParticleCount;
} ParticleSystem;

typedef struct Scene
{
	tinystl::vector<Object> mObjects;
	tinystl::vector<ParticleSystem> mParticleSystems;
} Scene;

typedef struct ObjectInfoStruct
{
	vec4 mColor;
	mat4 mToWorldMat;
}ObjectInfoStruct;

typedef struct ObjectInfoUniformBlock
{
	mat4				mViewProject;
	ObjectInfoStruct	mObjectInfo[MAX_NUM_OBJECTS];
}ObjectInfoUniformBlock;

typedef struct SkyboxUniformBlock
{
	mat4 mViewProject;
}SkyboxUniformBlock;

typedef struct LightUniformBlock
{
	mat4 mLightViewProj;
	vec4 mLightDirection = {-1, -1, -1, 0};
	vec4 mLightColor = {1, 0, 0, 1};
}LightUniformBlock;

typedef struct CameraUniform
{
	vec4 mPosition;
}CameraUniform;

typedef struct AlphaBlendSettings
{
	bool mSortObjects = true;
	bool mSortParticles = true;
} AlphaBlendSettings;

typedef struct WBOITSettings
{
	float mColorResistance = 1.0f;	// Increase if low-coverage foreground transparents are affecting background transparent color.
	float mRangeAdjustment = 0.3f;	// Change to avoid saturating at the clamp bounds.
	float mDepthRange = 200.0f;		// Decrease if high-opacity surfaces seem “too transparent”, increase if distant transparents are blending together too much.
	float mOrderingStrength = 4.0f;	// Increase if background is showing through foreground too much.
	float mUnderflowLimit = 1e-2f;	// Increase to reduce underflow artifacts.
	float mOverflowLimit = 3e3f;		// Decrease to reduce overflow artifacts.
} WBOITSettings;

typedef struct WBOITVolitionSettings
{
	float mOpacitySensitivity = 3.0f;		// Should be greater than 1, so that we only downweight nearly transparent things. Otherwise, everything at the same depth should get equal weight. Can be artist controlled
	float mWeightBias = 5.0f;				// Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than near things, as well as preventing floating point underflow
	float mPrecisionScalar = 10000.0f;		// Adjusts where the weights fall in the floating point range, used to balance precision to combat both underflow and overflow
	float mMaximumWeight = 20.0f;			// Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower" effect of very near vs. very far things
	float mMaximumColorValue = 1000.0f;
	float mAdditiveSensitivity = 10.0f;		// How much we amplify the emissive when deciding whether to consider this additively blended
	float mEmissiveSensitivity = 0.5f;		// Artist controlled value between 0.01 and 1
} WBOITVolitionSettings;

typedef enum WBOITRenderTargets
{
	WBOIT_RT_ACCUMULATION,
	WBOIT_RT_REVEALAGE,
	WBOIT_RT_COUNT
} WBOITRenderTargets;

ImageFormat::Enum gWBOITRenderTargetFormats[WBOIT_RT_COUNT] =
{
	ImageFormat::RGBA16F, ImageFormat::RGBA8
};

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pRenderTargetScreen;
RenderTarget* pRenderTargetDepth = NULL;
RenderTarget* pRenderTargetSkybox;
RenderTarget* pRenderTargetWBOIT[WBOIT_RT_COUNT];

/************************************************************************/
Buffer* pBufferCubeVertex = NULL;
/************************************************************************/
// Opaque forward Pass Shader pack
/************************************************************************/
Shader* pShaderForwardShade = NULL;
Pipeline* pPipelineForwardShade = NULL;
RootSignature* pRootSignatureForwardShade = NULL;
/************************************************************************/
// Transparent forward Pass Shader pack
/************************************************************************/
Pipeline* pPipelineTransparentForwardShade = NULL;
/************************************************************************/
// Skybox Shader Pack
/************************************************************************/
Shader* pShaderSkybox = NULL;
Pipeline* pPipelineSkybox = NULL;
RootSignature* pRootSignatureSkybox = NULL;
/************************************************************************/
// WBOIT Shader Pack
/************************************************************************/
Shader* pShaderWBOITShade = NULL;
Pipeline* pPipelineWBOITShade = NULL;
RootSignature* pRootSignatureWBOITShade = NULL;
Shader* pShaderWBOITComposite = NULL;
Pipeline* pPipelineWBOITComposite = NULL;
RootSignature* pRootSignatureWBOITComposite = NULL;
/************************************************************************/
// WBOIT Volition Shader Pack
/************************************************************************/
Shader* pShaderWBOITVolitionShade = NULL;
Pipeline* pPipelineWBOITVolitionShade = NULL;
RootSignature* pRootSignatureWBOITVolitionShade = NULL;
Shader* pShaderWBOITVolitionComposite = NULL;
Pipeline* pPipelineWBOITVolitionComposite = NULL;
RootSignature* pRootSignatureWBOITVolitionComposite = NULL;
/************************************************************************/
// AOIT Shader Pack
/************************************************************************/
#if defined(DIRECT3D12) && !defined(_DURANGO)
Shader* pShaderAOITShade = NULL;
Pipeline* pPipelineAOITShade = NULL;
RootSignature* pRootSignatureAOITShade = NULL;
Shader* pShaderAOITComposite = NULL;
Pipeline* pPipelineAOITComposite = NULL;
RootSignature* pRootSignatureAOITComposite = NULL;
Shader* pShaderAOITClear = NULL;
Pipeline* pPipelineAOITClear = NULL;
RootSignature* pRootSignatureAOITClear = NULL;

Texture* pTextureAOITClearMask;
Buffer* pBufferAOITDepthData;
Buffer* pBufferAOITColorData;
#endif
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerPoint = NULL;
Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerSkybox = NULL;

/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullBack = NULL;
RasterizerState* pRasterizerStateCullFront = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;

/************************************************************************/
// Constant buffers
/************************************************************************/
Buffer* pBufferOpaqueObjectTransforms = NULL;
Buffer* pBufferTransparentObjectTransforms = NULL;
Buffer* pBufferParticleSystemTransforms = NULL;
Buffer* pBufferSkyboxUniform = NULL;
Buffer* pBufferLightUniform = NULL;
Buffer* pBufferSkyboxVertex = NULL;
Buffer* pBufferCameraUniform = NULL;
Buffer* pBufferWBOITSettings = NULL;

/************************************************************************/
// Depth State
/************************************************************************/
DepthState* pDepthStateEnable = NULL;
DepthState* pDepthStateDisable = NULL;
DepthState* pDepthStateNoWrite = NULL;

/************************************************************************/
// Blend State
/************************************************************************/
BlendState* pBlendStateAlphaBlend = NULL;
BlendState* pBlendStateWBOITShade = NULL;
BlendState* pBlendStateWBOITVolitionShade = NULL;
#if defined(DIRECT3D12) && !defined(_DURANGO)
BlendState* pBlendStateAOITShade = NULL;
#endif

/************************************************************************/
// Textures
/************************************************************************/
Texture*			pTextureSkybox[6];


typedef enum TransparencyType
{
	TRANSPARENCY_TYPE_ALPHA_BLEND,
	TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT,
	TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION,
#if defined(DIRECT3D12) && !defined(_DURANGO)
	TRANSPARENCY_TYPE_ADAPTIVE_OIT
#endif
} TransparencyType;


struct
{
	float3 mLightPosition = { -5, 30, -5 };//light position, will be changed by GUI editor if not iOS
} gLightCpuSettings;

/************************************************************************/

#ifdef TARGET_IOS
VirtualJoystickUI	gVirtualJoystick;
#endif

// Constants
uint32_t				gFrameIndex = 0;
GpuProfiler*			pGpuProfiler = NULL;

ObjectInfoUniformBlock	gObjectInfoUniformData;
SkyboxUniformBlock		gSkyboxUniformData;
LightUniformBlock		gLightUniformData;
CameraUniform			gCameraUniformData;
AlphaBlendSettings		gAlphaBlendSettings;
WBOITSettings			gWBOITSettingsData;
WBOITVolitionSettings	gWBOITVolitionSettingsData;

Scene					gScene;
uint					gOpaqueObjectCount = 0;
uint					gTransparentObjectCount = 0;
vec3					gObjectsCenter = {0, 0, 0};

ICameraController*		pCameraController = NULL;
ICameraController*		pLightView = NULL;

/// UI
UIApp					gAppUI;
GuiComponent*			pGuiWindow = NULL;
TextDrawDesc			gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
HiresTimer				gCpuTimer;

FileSystem				gFileSystem;
LogManager				gLogManager;

const uint32_t			gImageCount = 3;

Renderer*				pRenderer = NULL;

Queue*					pGraphicsQueue = NULL;
CmdPool*				pCmdPool = NULL;
Cmd**					ppCmds = NULL;

SwapChain*				pSwapChain = NULL;
Fence*					pRenderCompleteFences[gImageCount] = {NULL};
Semaphore*				pImageAcquiredSemaphore = NULL;
Semaphore*				pRenderCompleteSemaphores[gImageCount] = {NULL};

uint32_t				gTransparencyType = TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT;

const char* pSkyboxImageFileNames[] =
{
	"skybox/hw_sahara/sahara_rt.tga",
	"skybox/hw_sahara/sahara_lf.tga",
	"skybox/hw_sahara/sahara_up.tga",
	"skybox/hw_sahara/sahara_dn.tga",
	"skybox/hw_sahara/sahara_ft.tga",
	"skybox/hw_sahara/sahara_bk.tga",
};


#if defined(DIRECT3D12) && !defined(_DURANGO) || defined(DIRECT3D11)
#define RESOURCE_DIR "PCDX12"
#elif defined(VULKAN)
#define RESOURCE_DIR "PCVulkan"
#elif defined(METAL)
#define RESOURCE_DIR "OSXMetal"
#elif defined(_DURANGO)
#define RESOURCE_DIR "PCDX12"
#else
#error PLATFORM NOT SUPPORTED
#endif

#ifdef _DURANGO
// Durango load assets from 'Layout\Image\Loose'
const char* pszRoots[] =
{
	"Shaders/Binary/",	// FSR_BinShaders
	"Shaders/",		// FSR_SrcShaders
	"Shaders/Binary/",			// FSR_BinShaders_Common
	"Shaders/",					// FSR_SrcShaders_Common
	"Textures/",						// FSR_Textures
	"Meshes/",						// FSR_Meshes
	"Fonts/",						// FSR_Builtin_Fonts
	"",															// FSR_OtherFiles
};
#else
//Example for using roots or will cause linker error with the extern root in FileSystem.cpp
const char* pszRoots[] =
{
	"../../../src/15_Transparency/" RESOURCE_DIR "/Binary/",	// FSR_BinShaders
	"../../../src/15_Transparency/" RESOURCE_DIR "/",			// FSR_SrcShaders
	"",															// FSR_BinShaders_Common
	"",															// FSR_SrcShaders_Common
	"../../../UnitTestResources/Textures/",						// FSR_Textures
	"../../../UnitTestResources/Meshes/",						// FSR_Meshes
	"../../../UnitTestResources/Fonts/",						// FSR_Builtin_Fonts
	"../../../src/15_Transparency/GPUCfg/",						// FSR_GpuConfig
	"",															// FSR_OtherFiles
};
#endif

void AddCube(vec3 position, vec4 color, vec3 scale = vec3(1.0f), vec3 orientation = vec3(0.0f))
{
	gScene.mObjects.push_back({ position, scale, orientation, color });
}

void AddParticleSystem(vec3 position, vec4 color, vec3 scale = vec3(1.0f), vec3 orientation = vec3(0.0f))
{
	Buffer* pParticleBuffer = NULL;
	BufferLoadDesc particleBufferDesc = {};
	particleBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	particleBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	particleBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	particleBufferDesc.mDesc.mSize = sizeof(Vertex) * 6 * MAX_NUM_PARTICLES;
	particleBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
	particleBufferDesc.ppBuffer = &pParticleBuffer;
	addResource(&particleBufferDesc);

	gScene.mParticleSystems.push_back({ pParticleBuffer, {position, scale, orientation, color}, MAX_NUM_PARTICLES, MAX_NUM_PARTICLES, MAX_NUM_PARTICLES, 0 });
}

static void CreateScene()
{
	// Set plane
	AddCube(vec3(0.0f), vec4(0.9f, 0.9f, 0.9f, 1.0f), vec3(100.0f, 1.0f, 100.0f));

	// Set cubes
	const float cubeDist = 3.0f;
	vec3 curTrans = { -cubeDist*(CUBES_EACH_ROW - 1)/2.f, 2.3f, -cubeDist*(CUBES_EACH_COL - 1)/2.f };

	for (int i = 0; i < CUBES_EACH_ROW; ++i)
	{		
		curTrans.setX(-cubeDist * (CUBES_EACH_ROW - 1) / 2.f);

		for (int j = 0; j < CUBES_EACH_COL; j++)
		{				
			AddCube(curTrans, vec4(float(i + 1) / CUBES_EACH_ROW, 1.0f - float(i + 1) / CUBES_EACH_ROW, 0.0f, float(j + 1) / CUBES_EACH_COL));
			curTrans.setX(curTrans.getX() + cubeDist);
		}
		
		curTrans.setZ(curTrans.getZ() + cubeDist);
	}

	AddCube(vec3(15.0f, 4.0f, 5.0f), vec4(1.0f, 0.0f, 0.0f, 0.9f), vec3(4.0f, 4.0f, 0.1f));
	AddCube(vec3(15.0f, 4.0f, 0.0f), vec4(0.0f, 1.0f, 0.0f, 0.9f), vec3(4.0f, 4.0f, 0.1f));
	AddCube(vec3(15.0f, 4.0f, -5.0f), vec4(0.0f, 0.0f, 1.0f, 0.9f), vec3(4.0f, 4.0f, 0.1f));

	AddCube(vec3(-15.0f, 4.0f, 5.0f), vec4(1.0f, 0.0f, 0.0f, 0.5f), vec3(4.0f, 4.0f, 0.1f));
	AddCube(vec3(-15.0f, 4.0f, 0.0f), vec4(0.0f, 1.0f, 0.0f, 0.5f), vec3(4.0f, 4.0f, 0.1f));
	AddCube(vec3(-15.0f, 4.0f, -5.0f), vec4(0.0f, 0.0f, 1.0f, 0.5f), vec3(4.0f, 4.0f, 0.1f));

	for (int i = 0; i < 25; ++i)
		AddCube(vec3(i * 2.0f - 25.0f, 4.0f, 25.0f), vec4(3.0f, 3.0f, 10.0f, 0.1f), vec3(0.1f, 4.0f, 4.0f));

	AddParticleSystem(vec3(30.0f, 5.0f, 20.0f), vec4(1.0f, 0.0f, 0.0f, 0.5f));
	AddParticleSystem(vec3(30.0f, 5.0f, 25.0f), vec4(1.0f, 1.0f, 0.0f, 0.5f));
}

/************************************************************************/
// Quicksort
/************************************************************************/
void QuickSortSwap(tinystl::pair<float, int>* a, tinystl::pair<float, int>* b)
{
	tinystl::pair<float, int> t = *a;
	*a = *b;
	*b = t;
}

int QuickSortPartition(tinystl::pair<float, int>* arr, int low, int high)
{
	tinystl::pair<float, int> pivot = arr[high];
	int i = (low - 1);

	for (int j = low; j <= high - 1; j++)
	{
		if (arr[j].first <= pivot.first)
		{
			i++;
			QuickSortSwap(&arr[i], &arr[j]);
		}
	}
	QuickSortSwap(&arr[i + 1], &arr[high]);
	return (i + 1);
}

void QuickSort(tinystl::pair<float, int>* arr, int low, int high)
{
	if (low < high)
	{
		int pi = QuickSortPartition(arr, low, high);
		QuickSort(arr, low, pi - 1);
		QuickSort(arr, pi + 1, high);
	}
}

void SwapParticles(ParticleSystem* pParticleSystem, size_t a, size_t b)
{
	vec3 pos = pParticleSystem->mParticlePositions[a];
	vec3 vel = pParticleSystem->mParticleVelocities[a];
	float life = pParticleSystem->mParticleLifetimes[a];

	pParticleSystem->mParticlePositions[a] = pParticleSystem->mParticlePositions[b];
	pParticleSystem->mParticleVelocities[a] = pParticleSystem->mParticleVelocities[b];
	pParticleSystem->mParticleLifetimes[a] = pParticleSystem->mParticleLifetimes[b];

	pParticleSystem->mParticlePositions[b] = pos;
	pParticleSystem->mParticleVelocities[b] = vel;
	pParticleSystem->mParticleLifetimes[b] = life;
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
struct GuiController
{
	static void AddGui();
	static void UpdateDynamicUI();

	static DynamicUIControls alphaBlendDynamicWidgets;
	static DynamicUIControls weightedBlendedOitDynamicWidgets;
	static DynamicUIControls weightedBlendedOitVolitionDynamicWidgets;
	
	static TransparencyType currentTransparencyType;
};
DynamicUIControls	GuiController::alphaBlendDynamicWidgets;
DynamicUIControls	GuiController::weightedBlendedOitDynamicWidgets;
DynamicUIControls	GuiController::weightedBlendedOitVolitionDynamicWidgets;
TransparencyType	GuiController::currentTransparencyType;
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
class Transparency : public IApp
{
public:
	bool Init() override
	{
		RendererDesc settings = {NULL};
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
		depthStateDisabledDesc.mDepthTest  = false;

		DepthStateDesc depthStateNoWriteDesc = {};
		depthStateNoWriteDesc.mDepthFunc = CMP_LEQUAL;
		depthStateNoWriteDesc.mDepthWrite = false;
		depthStateNoWriteDesc.mDepthTest = true;
		
		addDepthState(pRenderer, &depthStateEnabledDesc, &pDepthStateEnable);
		addDepthState(pRenderer, &depthStateDisabledDesc, &pDepthStateDisable);
		addDepthState(pRenderer, &depthStateNoWriteDesc, &pDepthStateNoWrite);

		BlendStateDesc blendStateAlphaDesc = {};
		blendStateAlphaDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateAlphaDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateAlphaDesc.mBlendModes[0] = BM_ADD;
		blendStateAlphaDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateAlphaDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateAlphaDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateAlphaDesc.mMasks[0] = ALL;
		blendStateAlphaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateAlphaDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStateAlphaDesc, &pBlendStateAlphaBlend);

		BlendStateDesc blendStateWBOITShadeDesc = {};
		blendStateWBOITShadeDesc.mSrcFactors[0] = BC_ONE;
		blendStateWBOITShadeDesc.mDstFactors[0] = BC_ONE;
		blendStateWBOITShadeDesc.mBlendModes[0] = BM_ADD;
		blendStateWBOITShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateWBOITShadeDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateWBOITShadeDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateWBOITShadeDesc.mMasks[0] = ALL;
		blendStateWBOITShadeDesc.mSrcFactors[1] = BC_ZERO;
		blendStateWBOITShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
		blendStateWBOITShadeDesc.mBlendModes[1] = BM_ADD;
		blendStateWBOITShadeDesc.mSrcAlphaFactors[1] = BC_ZERO;
		blendStateWBOITShadeDesc.mDstAlphaFactors[1] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateWBOITShadeDesc.mBlendAlphaModes[1] = BM_ADD;
		blendStateWBOITShadeDesc.mMasks[1] = RED;
		blendStateWBOITShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
		blendStateWBOITShadeDesc.mIndependentBlend = true;
		addBlendState(pRenderer, &blendStateWBOITShadeDesc, &pBlendStateWBOITShade);

		BlendStateDesc blendStateWBOITVolitionShadeDesc = {};
		blendStateWBOITVolitionShadeDesc.mSrcFactors[0] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mDstFactors[0] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mBlendModes[0] = BM_ADD;
		blendStateWBOITVolitionShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateWBOITVolitionShadeDesc.mMasks[0] = ALL;
		blendStateWBOITVolitionShadeDesc.mSrcFactors[1] = BC_ZERO;
		blendStateWBOITVolitionShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
		blendStateWBOITVolitionShadeDesc.mBlendModes[1] = BM_ADD;
		blendStateWBOITVolitionShadeDesc.mSrcAlphaFactors[1] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mDstAlphaFactors[1] = BC_ONE;
		blendStateWBOITVolitionShadeDesc.mBlendAlphaModes[1] = BM_ADD;
		blendStateWBOITVolitionShadeDesc.mMasks[1] = RED | ALPHA;
		blendStateWBOITVolitionShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
		blendStateWBOITVolitionShadeDesc.mIndependentBlend = true;
		addBlendState(pRenderer, &blendStateWBOITVolitionShadeDesc, &pBlendStateWBOITVolitionShade);

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			BlendStateDesc blendStateAOITShadeaDesc = {};
			blendStateAOITShadeaDesc.mSrcFactors[0] = BC_ONE;
			blendStateAOITShadeaDesc.mDstFactors[0] = BC_SRC_ALPHA;
			blendStateAOITShadeaDesc.mBlendModes[0] = BM_ADD;
			blendStateAOITShadeaDesc.mSrcAlphaFactors[0] = BC_ONE;
			blendStateAOITShadeaDesc.mDstAlphaFactors[0] = BC_SRC_ALPHA;
			blendStateAOITShadeaDesc.mBlendAlphaModes[0] = BM_ADD;
			blendStateAOITShadeaDesc.mMasks[0] = ALL;
			blendStateAOITShadeaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
			blendStateAOITShadeaDesc.mIndependentBlend = false;
			addBlendState(pRenderer, &blendStateAOITShadeaDesc, &pBlendStateAOITShade);
		}
#endif

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);


		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);

		for (int i = 0; i < 6; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = pSkyboxImageFileNames[i];
			textureDesc.ppTexture = &pTextureSkybox[i];
			addResource(&textureDesc, true);
		} 

		/************************************************************************/
		// Geometry data for the scene
		/************************************************************************/
		Vertex cubeVertices[] = 
		{
			{float3(-1.0f,-1.0f,-1.0f), float3(-1.0f, 0.0f, 0.0f)},  // -X side
			{float3(-1.0f,-1.0f, 1.0f), float3(-1.0f, 0.0f, 0.0f)},
			{float3(-1.0f, 1.0f, 1.0f), float3(-1.0f, 0.0f, 0.0f)},
			{float3(-1.0f, 1.0f, 1.0f), float3(-1.0f, 0.0f, 0.0f)},
			{float3(-1.0f, 1.0f,-1.0f), float3(-1.0f, 0.0f, 0.0f)},
			{float3(-1.0f,-1.0f,-1.0f), float3(-1.0f, 0.0f, 0.0f)},

			{float3(-1.0f,-1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},  // -Z side
			{float3(1.0f, 1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},
			{float3(1.0f,-1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},
			{float3(-1.0f,-1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},
			{float3(-1.0f, 1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},
			{float3(1.0f, 1.0f,-1.0f), float3(0.0f, 0.0f, -1.0f)},

			{float3(-1.0f,-1.0f,-1.0f), float3(0.0f, -1.0f, 0.0f)},  // -Y side
			{float3(1.0f,-1.0f,-1.0f), float3(0.0f, -1.0f, 0.0f)},
			{float3(1.0f,-1.0f, 1.0f), float3(0.0f, -1.0f, 0.0f)},
			{float3(-1.0f,-1.0f,-1.0f), float3(0.0f, -1.0f, 0.0f)},
			{float3(1.0f,-1.0f, 1.0f), float3(0.0f, -1.0f, 0.0f)},
			{float3(-1.0f,-1.0f, 1.0f), float3(0.0f, -1.0f, 0.0f)},

			{float3(-1.0f, 1.0f,-1.0f), float3(0.0f, 1.0f, 0.0f)},  // +Y side
			{float3(-1.0f, 1.0f, 1.0f), float3(0.0f, 1.0f, 0.0f)},
			{float3(1.0f, 1.0f, 1.0f), float3(0.0f, 1.0f, 0.0f)},
			{float3(-1.0f, 1.0f,-1.0f), float3(0.0f, 1.0f, 0.0f)},
			{float3(1.0f, 1.0f, 1.0f), float3(0.0f, 1.0f, 0.0f)},
			{float3(1.0f, 1.0f,-1.0f), float3(0.0f, 1.0f, 0.0f)},

			{float3(1.0f, 1.0f,-1.0f), float3(1.0f, 0.0f, 0.0f)},  // +X side
			{float3(1.0f, 1.0f, 1.0f), float3(1.0f, 0.0f, 0.0f)},
			{float3(1.0f,-1.0f, 1.0f), float3(1.0f, 0.0f, 0.0f)},
			{float3(1.0f,-1.0f, 1.0f), float3(1.0f, 0.0f, 0.0f)},
			{float3(1.0f,-1.0f,-1.0f), float3(1.0f, 0.0f, 0.0f)},
			{float3(1.0f, 1.0f,-1.0f), float3(1.0f, 0.0f, 0.0f)},

			{float3(-1.0f, 1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)},  // +Z side
			{float3(-1.0f,-1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)},
			{float3(1.0f, 1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)},
			{float3(-1.0f,-1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)},
			{float3(1.0f,-1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)},
			{float3(1.0f, 1.0f, 1.0f), float3(0.0f, 0.0f, 1.0f)}
		};

		BufferLoadDesc cubeVbDesc = {};
		cubeVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		cubeVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		cubeVbDesc.mDesc.mSize = sizeof(cubeVertices);
		cubeVbDesc.mDesc.mVertexStride = sizeof(Vertex);
		cubeVbDesc.pData = cubeVertices;
		cubeVbDesc.ppBuffer = &pBufferCubeVertex;
		addResource(&cubeVbDesc);
		//------------------------Skybox--------------------------
		uint64_t skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
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
		ubDesc.ppBuffer = &pBufferOpaqueObjectTransforms;
		addResource(&ubDesc);
		ubDesc.ppBuffer = &pBufferTransparentObjectTransforms;
		addResource(&ubDesc);
		ubDesc.ppBuffer = &pBufferParticleSystemTransforms;
		addResource(&ubDesc);


		BufferLoadDesc skyboxDesc = {};
		skyboxDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		skyboxDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		skyboxDesc.mDesc.mSize = sizeof(SkyboxUniformBlock);
		skyboxDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		skyboxDesc.pData = NULL;
		skyboxDesc.ppBuffer = &pBufferSkyboxUniform;
		addResource(&skyboxDesc);


		BufferLoadDesc camUniDesc = {};
		camUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		camUniDesc.mDesc.mSize = sizeof(CameraUniform);
		camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		camUniDesc.pData = &gCameraUniformData;
		camUniDesc.ppBuffer = &pBufferCameraUniform;
		addResource(&camUniDesc);


		BufferLoadDesc lightUniformDesc = {};
		lightUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightUniformDesc.mDesc.mSize = sizeof(LightUniformBlock);
		lightUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightUniformDesc.pData = NULL;
		lightUniformDesc.ppBuffer = &pBufferLightUniform;
		addResource(&lightUniformDesc);

		BufferLoadDesc wboitSettingsDesc = {};
		wboitSettingsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		wboitSettingsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		wboitSettingsDesc.mDesc.mSize = max(sizeof(WBOITSettings), sizeof(WBOITVolitionSettings));
		wboitSettingsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		wboitSettingsDesc.pData = NULL;
		wboitSettingsDesc.ppBuffer = &pBufferWBOITSettings;
		addResource(&wboitSettingsDesc);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Absolute))
			return false;
#endif

		ShaderMacro maxNumObjectsMacro = {"MAX_NUM_OBJECTS", tinystl::string::format("%i", MAX_NUM_OBJECTS) };

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc forwardShadeShaderDesc = {};
		forwardShadeShaderDesc.mStages[0] = { "forward.vert", &maxNumObjectsMacro, 1, FSR_SrcShaders };
		forwardShadeShaderDesc.mStages[1] = { "forward.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc wboitShadeShaderDesc = {};
		wboitShadeShaderDesc.mStages[0] = { "forward.vert", &maxNumObjectsMacro, 1, FSR_SrcShaders };
		wboitShadeShaderDesc.mStages[1] = { "weightedBlendedOIT.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc wboitCompositeShaderDesc = {};
		wboitCompositeShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, FSR_SrcShaders };
		wboitCompositeShaderDesc.mStages[1] = { "weightedBlendedOITComposite.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc wboitVolitionShadeShaderDesc = {};
		wboitVolitionShadeShaderDesc.mStages[0] = { "forward.vert", &maxNumObjectsMacro, 1, FSR_SrcShaders };
		wboitVolitionShadeShaderDesc.mStages[1] = { "weightedBlendedOITVolition.frag", NULL, 0, FSR_SrcShaders };
		ShaderLoadDesc wboitVolitionCompositeShaderDesc = {};
		wboitVolitionCompositeShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, FSR_SrcShaders };
		wboitVolitionCompositeShaderDesc.mStages[1] = { "weightedBlendedOITVolitionComposite.frag", NULL, 0, FSR_SrcShaders };
		/************************************************************************/
		// Add shaders
		/************************************************************************/
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);
		addShader(pRenderer, &forwardShadeShaderDesc, &pShaderForwardShade);
		addShader(pRenderer, &wboitShadeShaderDesc, &pShaderWBOITShade);
		addShader(pRenderer, &wboitCompositeShaderDesc, &pShaderWBOITComposite);
		addShader(pRenderer, &wboitVolitionShadeShaderDesc, &pShaderWBOITVolitionShade);
		addShader(pRenderer, &wboitVolitionCompositeShaderDesc, &pShaderWBOITVolitionComposite);

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			ShaderMacro aoitNodeCountMacro = { "AOIT_NODE_COUNT", tinystl::string::format("%i", AOIT_NODE_COUNT) };

			ShaderLoadDesc aoitShadeShaderDesc = {};
			aoitShadeShaderDesc.mStages[0] = { "forward.vert", &maxNumObjectsMacro, 1, FSR_SrcShaders };
			aoitShadeShaderDesc.mStages[1] = { "adaptiveOIT.frag", &aoitNodeCountMacro, 1, FSR_SrcShaders };
			ShaderLoadDesc aoitCompositeShaderDesc = {};
			aoitCompositeShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, FSR_SrcShaders };
			aoitCompositeShaderDesc.mStages[1] = { "adaptiveOITComposite.frag", &aoitNodeCountMacro, 1, FSR_SrcShaders };
			ShaderLoadDesc aoitClearShaderDesc = {};
			aoitClearShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, FSR_SrcShaders };
			aoitClearShaderDesc.mStages[1] = { "adaptiveOITClear.frag", &aoitNodeCountMacro, 1, FSR_SrcShaders };
			addShader(pRenderer, &aoitShadeShaderDesc, &pShaderAOITShade);
			addShader(pRenderer, &aoitCompositeShaderDesc, &pShaderAOITComposite);
			addShader(pRenderer, &aoitClearShaderDesc, &pShaderAOITClear);
		}
#endif

		/************************************************************************/
		// Add GPU profiler
		/************************************************************************/
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		/************************************************************************/
		// Add samplers
		/************************************************************************/
		SamplerDesc samplerPointDesc= {};
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
		RootSignatureDesc forwardShadeRootDesc = {};
		forwardShadeRootDesc.ppShaders = &pShaderForwardShade;
		forwardShadeRootDesc.mShaderCount = 1;

		RootSignatureDesc skyboxRootDesc = {};
		skyboxRootDesc.ppShaders = &pShaderSkybox;
		skyboxRootDesc.mShaderCount = 1;
		skyboxRootDesc.ppStaticSamplers = &pSamplerSkybox;
		skyboxRootDesc.mStaticSamplerCount = 1;
		const char* skyboxSamplersNames[] = { "SkySampler" };
		skyboxRootDesc.ppStaticSamplerNames = skyboxSamplersNames;

		RootSignatureDesc wboitShadeRootDesc = {};
		wboitShadeRootDesc.ppShaders = &pShaderWBOITShade;
		wboitShadeRootDesc.mShaderCount = 1;

		RootSignatureDesc wboitCompositeRootDesc = {};
		wboitCompositeRootDesc.ppShaders = &pShaderWBOITComposite;
		wboitCompositeRootDesc.mShaderCount = 1;
		wboitCompositeRootDesc.ppStaticSamplers = &pSamplerPoint;
		wboitCompositeRootDesc.mStaticSamplerCount = 1;
		const char* wboitCompositeSamplerNames[] = { "PointSampler" };
		wboitCompositeRootDesc.ppStaticSamplerNames = wboitCompositeSamplerNames;

		RootSignatureDesc wboitVolitionShadeRootDesc = {};
		wboitVolitionShadeRootDesc.ppShaders = &pShaderWBOITVolitionShade;
		wboitVolitionShadeRootDesc.mShaderCount = 1;

		RootSignatureDesc wboitVolitionCompositeRootDesc = {};
		wboitVolitionCompositeRootDesc.ppShaders = &pShaderWBOITVolitionComposite;
		wboitVolitionCompositeRootDesc.mShaderCount = 1;
		wboitVolitionCompositeRootDesc.ppStaticSamplers = &pSamplerPoint;
		wboitVolitionCompositeRootDesc.mStaticSamplerCount = 1;
		const char* wboitVolitionCompositeSamplerNames[] = { "PointSampler" };
		wboitVolitionCompositeRootDesc.ppStaticSamplerNames = wboitVolitionCompositeSamplerNames;

		addRootSignature(pRenderer, &forwardShadeRootDesc, &pRootSignatureForwardShade);
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);
		addRootSignature(pRenderer, &wboitShadeRootDesc, &pRootSignatureWBOITShade);
		addRootSignature(pRenderer, &wboitCompositeRootDesc, &pRootSignatureWBOITComposite);
		addRootSignature(pRenderer, &wboitVolitionShadeRootDesc, &pRootSignatureWBOITVolitionShade);
		addRootSignature(pRenderer, &wboitVolitionCompositeRootDesc, &pRootSignatureWBOITVolitionComposite);

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			RootSignatureDesc aoitShadeRootDesc = {};
			aoitShadeRootDesc.ppShaders = &pShaderAOITShade;
			aoitShadeRootDesc.mShaderCount = 1;

			RootSignatureDesc aoitCompositeRootDesc = {};
			aoitCompositeRootDesc.ppShaders = &pShaderAOITComposite;
			aoitCompositeRootDesc.mShaderCount = 1;

			RootSignatureDesc aoitClearRootDesc = {};
			aoitClearRootDesc.ppShaders = &pShaderAOITClear;
			aoitClearRootDesc.mShaderCount = 1;

			addRootSignature(pRenderer, &aoitShadeRootDesc, &pRootSignatureAOITShade);
			addRootSignature(pRenderer, &aoitCompositeRootDesc, &pRootSignatureAOITComposite);
			addRootSignature(pRenderer, &aoitClearRootDesc, &pRootSignatureAOITClear);
		}
#endif

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

		CreateScene();

		/************************************************************************/
		finishResourceLoading();
		/************************************************************************/
		////////////////////////////////////////////////

		/*************************************************/
		//                      UI 
		/*************************************************/
		if (!gAppUI.Init(pRenderer))
			return false;
		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.ttf", FSR_Builtin_Fonts);
		
		GuiDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(5, 200.0f);
		guiDesc.mStartSize = vec2(450, 600);
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		GuiController::AddGui();
		

		CameraMotionParameters cmp{16.0f, 60.0f, 20.0f};
		vec3 camPos{0, 5, -15};
		vec3 lookAt{0, 5, 0};

		pLightView = createGuiCameraController(camPos, lookAt);
		pCameraController = createFpsCameraController(camPos, lookAt);
		requestMouseCapture(true);

		pCameraController->setMotionParameters(cmp);
		
		InputSystem::RegisterInputEvent(cameraInputEvent);

		return true;
	}
	void RemoveRenderTargetsAndSwapChian()
	{
		removeRenderTarget(pRenderer, pRenderTargetDepth);
		removeRenderTarget(pRenderer, pRenderTargetSkybox);
		for(int i = 0; i < WBOIT_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetWBOIT[i]);
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

		for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			removeResource(gScene.mParticleSystems[i].pParticleBuffer);

		removeResource(pBufferOpaqueObjectTransforms);
		removeResource(pBufferTransparentObjectTransforms);
		removeResource(pBufferParticleSystemTransforms);
		removeResource(pBufferLightUniform);
		removeResource(pBufferCubeVertex);
		removeResource(pBufferSkyboxVertex);
		removeResource(pBufferSkyboxUniform);
		removeResource(pBufferCameraUniform);
		removeResource(pBufferWBOITSettings);

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerPointClamp);
		removeSampler(pRenderer, pSamplerSkybox);
		removeSampler(pRenderer, pSamplerPoint);

		removeShader(pRenderer, pShaderForwardShade);
		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderWBOITShade);
		removeShader(pRenderer, pShaderWBOITComposite);
		removeShader(pRenderer, pShaderWBOITVolitionShade);
		removeShader(pRenderer, pShaderWBOITVolitionComposite);

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeShader(pRenderer, pShaderAOITShade);
			removeShader(pRenderer, pShaderAOITComposite);
			removeShader(pRenderer, pShaderAOITClear);
		}
#endif


		removeRootSignature(pRenderer, pRootSignatureForwardShade);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureWBOITShade);
		removeRootSignature(pRenderer, pRootSignatureWBOITComposite);
		removeRootSignature(pRenderer, pRootSignatureWBOITVolitionShade);
		removeRootSignature(pRenderer, pRootSignatureWBOITVolitionComposite);
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeRootSignature(pRenderer, pRootSignatureAOITShade);
			removeRootSignature(pRenderer, pRootSignatureAOITComposite);
			removeRootSignature(pRenderer, pRootSignatureAOITClear);
		}
#endif


		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateDisable);
		removeDepthState(pDepthStateNoWrite);

		removeBlendState(pBlendStateAlphaBlend);
		removeBlendState(pBlendStateWBOITShade);
		removeBlendState(pBlendStateWBOITVolitionShade);
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeBlendState(pBlendStateAOITShade);
		}
#endif

		removeRasterizerState(pRasterizerStateCullBack);
		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);

		for (uint i = 0; i < 6; ++i)
			removeResource(pTextureSkybox[i]);


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
		//layout and pipeline for cube draw
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
		// Setup pipelines for opaque forward pass shader
		/*------------------------------------------*/
		GraphicsPipelineDesc pipelineSettings = { NULL };
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureForwardShade;
		pipelineSettings.pVertexLayout = &vertexLayoutRegular;
		pipelineSettings.pShaderProgram = pShaderForwardShade;
		pipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineForwardShade);
		/*------------------------------------------*/
		// Setup pipelines for transparent forward pass shader
		pipelineSettings.pDepthState = pDepthStateNoWrite;
		pipelineSettings.pShaderProgram = pShaderForwardShade;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineTransparentForwardShade);

		/************************************************************************/
		// Setup the resources needed for Skybox
		/************************************************************************/
		GraphicsPipelineDesc skyboxPipelineSettings = {};
		skyboxPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		skyboxPipelineSettings.mRenderTargetCount = 1;
		skyboxPipelineSettings.pDepthState = pDepthStateDisable;
		skyboxPipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		skyboxPipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		skyboxPipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		skyboxPipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		skyboxPipelineSettings.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineSettings.pShaderProgram = pShaderSkybox;
		skyboxPipelineSettings.pVertexLayout = &vertexLayoutSkybox;
		skyboxPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &skyboxPipelineSettings, &pPipelineSkybox);

		/************************************************************************/
		// Setup pipelines for weighted blended order independent transparency
		/************************************************************************/
		GraphicsPipelineDesc wboitShadePipelineSettings = {};
		wboitShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitShadePipelineSettings.mRenderTargetCount = 2;
		wboitShadePipelineSettings.pDepthState = pDepthStateNoWrite;
		wboitShadePipelineSettings.pColorFormats = gWBOITRenderTargetFormats;
		bool wboitShaderSrgbValues[] = { false, false };
		wboitShadePipelineSettings.pSrgbValues = wboitShaderSrgbValues;
		wboitShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		wboitShadePipelineSettings.mSampleQuality = 0;
		wboitShadePipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		wboitShadePipelineSettings.pRootSignature = pRootSignatureWBOITShade;
		wboitShadePipelineSettings.pShaderProgram = pShaderWBOITShade;
		wboitShadePipelineSettings.pVertexLayout = &vertexLayoutRegular;
		wboitShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		wboitShadePipelineSettings.pBlendState = pBlendStateWBOITShade;
		addPipeline(pRenderer, &wboitShadePipelineSettings, &pPipelineWBOITShade);

		GraphicsPipelineDesc wboitCompositePipelineSettings = {};
		wboitCompositePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitCompositePipelineSettings.mRenderTargetCount = 1;
		wboitCompositePipelineSettings.pDepthState = pDepthStateDisable;
		wboitCompositePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		wboitCompositePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		wboitCompositePipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		wboitCompositePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		wboitCompositePipelineSettings.pRootSignature = pRootSignatureWBOITComposite;
		wboitCompositePipelineSettings.pShaderProgram = pShaderWBOITComposite;
		wboitCompositePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		wboitCompositePipelineSettings.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &wboitCompositePipelineSettings, &pPipelineWBOITComposite);

		/************************************************************************/
		// Setup pipelines for weighted blended order independent transparency - Volition
		/************************************************************************/
		GraphicsPipelineDesc wboitVolitionShadePipelineSettings = {};
		wboitVolitionShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitVolitionShadePipelineSettings.mRenderTargetCount = 2;
		wboitVolitionShadePipelineSettings.pDepthState = pDepthStateNoWrite;
		wboitVolitionShadePipelineSettings.pColorFormats = gWBOITRenderTargetFormats;
		wboitVolitionShadePipelineSettings.pSrgbValues = wboitShaderSrgbValues;
		wboitVolitionShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		wboitVolitionShadePipelineSettings.mSampleQuality = 0;
		wboitVolitionShadePipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		wboitVolitionShadePipelineSettings.pRootSignature = pRootSignatureWBOITVolitionShade;
		wboitVolitionShadePipelineSettings.pShaderProgram = pShaderWBOITVolitionShade;
		wboitVolitionShadePipelineSettings.pVertexLayout = &vertexLayoutRegular;
		wboitVolitionShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		wboitVolitionShadePipelineSettings.pBlendState = pBlendStateWBOITVolitionShade;
		addPipeline(pRenderer, &wboitVolitionShadePipelineSettings, &pPipelineWBOITVolitionShade);

		GraphicsPipelineDesc wboitVolitionCompositePipelineSettings = {};
		wboitVolitionCompositePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitVolitionCompositePipelineSettings.mRenderTargetCount = 1;
		wboitVolitionCompositePipelineSettings.pDepthState = pDepthStateDisable;
		wboitVolitionCompositePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		wboitVolitionCompositePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		wboitVolitionCompositePipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		wboitVolitionCompositePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		wboitVolitionCompositePipelineSettings.pRootSignature = pRootSignatureWBOITVolitionComposite;
		wboitVolitionCompositePipelineSettings.pShaderProgram = pShaderWBOITVolitionComposite;
		wboitVolitionCompositePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		wboitVolitionCompositePipelineSettings.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &wboitVolitionCompositePipelineSettings, &pPipelineWBOITVolitionComposite);

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			/************************************************************************/
			// Setup pipelines for addaptive order independent transparency
			/************************************************************************/
			GraphicsPipelineDesc aoitShadePipelineSettings = {};
			aoitShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitShadePipelineSettings.mRenderTargetCount = 0;
			aoitShadePipelineSettings.pDepthState = pDepthStateNoWrite;
			aoitShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
			aoitShadePipelineSettings.mSampleQuality = 0;
			aoitShadePipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
			aoitShadePipelineSettings.pRootSignature = pRootSignatureAOITShade;
			aoitShadePipelineSettings.pShaderProgram = pShaderAOITShade;
			aoitShadePipelineSettings.pVertexLayout = &vertexLayoutRegular;
			aoitShadePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
			addPipeline(pRenderer, &aoitShadePipelineSettings, &pPipelineAOITShade);

			GraphicsPipelineDesc aoitCompositePipelineSettings = {};
			aoitCompositePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitCompositePipelineSettings.mRenderTargetCount = 1;
			aoitCompositePipelineSettings.pDepthState = pDepthStateDisable;
			aoitCompositePipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
			aoitCompositePipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
			aoitCompositePipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
			aoitCompositePipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
			aoitCompositePipelineSettings.pRootSignature = pRootSignatureAOITComposite;
			aoitCompositePipelineSettings.pShaderProgram = pShaderAOITComposite;
			aoitCompositePipelineSettings.pRasterizerState = pRasterizerStateCullNone;
			aoitCompositePipelineSettings.pBlendState = pBlendStateAOITShade;
			addPipeline(pRenderer, &aoitCompositePipelineSettings, &pPipelineAOITComposite);

			GraphicsPipelineDesc aoitClearPipelineSettings = {};
			aoitClearPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitClearPipelineSettings.mRenderTargetCount = 0;
			aoitClearPipelineSettings.pDepthState = pDepthStateDisable;
			aoitClearPipelineSettings.mSampleCount = SAMPLE_COUNT_1;
			aoitClearPipelineSettings.mSampleQuality = 0;
			aoitClearPipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
			aoitClearPipelineSettings.pRootSignature = pRootSignatureAOITClear;
			aoitClearPipelineSettings.pShaderProgram = pShaderAOITClear;
			aoitClearPipelineSettings.pRasterizerState = pRasterizerStateCullNone;
			addPipeline(pRenderer, &aoitClearPipelineSettings, &pPipelineAOITClear);

			// Create AOIT resources
			TextureDesc aoitClearMaskTextureDesc = {};
			aoitClearMaskTextureDesc.mFormat = ImageFormat::R32UI;
			aoitClearMaskTextureDesc.mWidth = pSwapChain->mDesc.mWidth;
			aoitClearMaskTextureDesc.mHeight = pSwapChain->mDesc.mHeight;
			aoitClearMaskTextureDesc.mDepth = 1;
			aoitClearMaskTextureDesc.mArraySize = 1;
			aoitClearMaskTextureDesc.mSampleCount = SAMPLE_COUNT_1;
			aoitClearMaskTextureDesc.mSampleQuality = 0;
			aoitClearMaskTextureDesc.mMipLevels = 1;
			aoitClearMaskTextureDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
			aoitClearMaskTextureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
			aoitClearMaskTextureDesc.pDebugName = L"AOIT Clear Mask";

			TextureLoadDesc aoitClearMaskTextureLoadDesc = {};
			aoitClearMaskTextureLoadDesc.pDesc = &aoitClearMaskTextureDesc;
			aoitClearMaskTextureLoadDesc.ppTexture = &pTextureAOITClearMask;
			addResource(&aoitClearMaskTextureLoadDesc);

#if AOIT_NODE_COUNT != 2
			BufferLoadDesc aoitDepthDataLoadDesc = {};
			aoitDepthDataLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			aoitDepthDataLoadDesc.mDesc.mFormat = ImageFormat::NONE;
			aoitDepthDataLoadDesc.mDesc.mElementCount = pSwapChain->mDesc.mWidth * pSwapChain->mDesc.mHeight;
			aoitDepthDataLoadDesc.mDesc.mStructStride = sizeof(uint32_t) * 4 * AOIT_RT_COUNT;
			aoitDepthDataLoadDesc.mDesc.mSize = aoitDepthDataLoadDesc.mDesc.mElementCount * aoitDepthDataLoadDesc.mDesc.mStructStride;
			aoitDepthDataLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			aoitDepthDataLoadDesc.mDesc.pDebugName = L"AOIT Depth Data";
			aoitDepthDataLoadDesc.ppBuffer = &pBufferAOITDepthData;
			addResource(&aoitDepthDataLoadDesc);
#endif

			BufferLoadDesc aoitColorDataLoadDesc = {};
			aoitColorDataLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
			aoitColorDataLoadDesc.mDesc.mFormat = ImageFormat::NONE;
			aoitColorDataLoadDesc.mDesc.mElementCount = pSwapChain->mDesc.mWidth * pSwapChain->mDesc.mHeight;
			aoitColorDataLoadDesc.mDesc.mStructStride = sizeof(uint32_t) * 4 * AOIT_RT_COUNT;
			aoitColorDataLoadDesc.mDesc.mSize = aoitColorDataLoadDesc.mDesc.mElementCount * aoitColorDataLoadDesc.mDesc.mStructStride;
			aoitColorDataLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
			aoitColorDataLoadDesc.mDesc.pDebugName = L"AOIT Color Data";
			aoitColorDataLoadDesc.ppBuffer = &pBufferAOITColorData;
			addResource(&aoitColorDataLoadDesc);
		}
#endif

		return true;
	}

	void Unload() override
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		removePipeline(pRenderer, pPipelineForwardShade);
		removePipeline(pRenderer, pPipelineTransparentForwardShade);
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineWBOITShade);
		removePipeline(pRenderer, pPipelineWBOITComposite);
		removePipeline(pRenderer, pPipelineWBOITVolitionShade);
		removePipeline(pRenderer, pPipelineWBOITVolitionComposite);
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removePipeline(pRenderer, pPipelineAOITShade);
			removePipeline(pRenderer, pPipelineAOITComposite);
			removePipeline(pRenderer, pPipelineAOITClear);
			removeResource(pTextureAOITClearMask);
#if AOIT_NODE_COUNT != 2
			removeResource(pBufferAOITDepthData);
#endif
			removeResource(pBufferAOITColorData);
		}
#endif

		RemoveRenderTargetsAndSwapChian();
	}

	void Update(float deltaTime) override
	{
		gCpuTimer.Reset();

		/************************************************************************/
		// Input
		/************************************************************************/
		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}

		pCameraController->update(deltaTime);

		// Dynamic UI elements
		GuiController::UpdateDynamicUI();

		/************************************************************************/
		// Camera Update
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime;

		// update camera with time 
		mat4 viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4 projMat = mat4::perspective(horizontal_fov, aspectInverse, 1.0f, 4000.0f);//view matrix
		vec3 camPos = pCameraController->getViewPosition();

		gObjectInfoUniformData.mViewProject = projMat * viewMat;



		/************************************************************************/
		// Update particle systems
		/************************************************************************/
		tinystl::vector<Vertex> tempVertexBuffer(6 * MAX_NUM_PARTICLES);
		const float particleSize = 0.2f;
		const vec3 camRight = vec3(viewMat[0][0], viewMat[1][0], viewMat[2][0]) * particleSize;
		const vec3 camUp = vec3(viewMat[0][1], viewMat[1][1], viewMat[2][1]) * particleSize;

		for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
		{
			ParticleSystem* pParticleSystem = &gScene.mParticleSystems[i];

			// Remove dead particles
			for (size_t j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
			{
				float* pLifetime = &pParticleSystem->mParticleLifetimes[j];
				*pLifetime -= deltaTime;

				if (*pLifetime < 0.0f)
				{
					--pParticleSystem->mLifeParticleCount;
					if (j != pParticleSystem->mLifeParticleCount)
						SwapParticles(pParticleSystem, j, pParticleSystem->mLifeParticleCount);
					--j;
				}
			}

			// Spawn new particles
			size_t newParticleCount = (size_t)max(deltaTime * 25.0f, 1.0f);
			for (size_t j = 0; j < newParticleCount && pParticleSystem->mLifeParticleCount < MAX_NUM_PARTICLES; ++j)
			{
				size_t pi = pParticleSystem->mLifeParticleCount;
				pParticleSystem->mParticleVelocities[pi] = normalize(vec3(sin(currentTime + pi) * 0.97f, cos(currentTime * currentTime + pi), sin(currentTime * pi)) * cos(currentTime + deltaTime * pi));
				pParticleSystem->mParticlePositions[pi] = pParticleSystem->mParticleVelocities[pi];
				pParticleSystem->mParticleLifetimes[pi] = (sin(currentTime + pi) + 1.0f) * 3.0f + 10.0f;
				++pParticleSystem->mLifeParticleCount;
			}

			// Update particles
			for (size_t j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
			{
				pParticleSystem->mParticlePositions[j] += pParticleSystem->mParticleVelocities[j] * deltaTime;
				pParticleSystem->mParticleVelocities[j] *= 1.0f - 0.2f * deltaTime;
			}

			// Update vertex buffers
			if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortParticles)
			{
				tinystl::vector<tinystl::pair<float, int>> sortedArray;

				for (size_t j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
					sortedArray.push_back(tinystl::make_pair((float)distSqr(Point3(camPos), Point3(pParticleSystem->mParticlePositions[j])), (int)j));

				QuickSort(sortedArray.data(), 0, (int)sortedArray.size() - 1);

				for (uint j = 0; j < sortedArray.size(); ++j)
				{
					vec3 pos = pParticleSystem->mParticlePositions[sortedArray[sortedArray.size() - j - 1].second];
					tempVertexBuffer[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 1] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 2] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 3] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 4] = { v3ToF3(pos + camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
				}
			}
			else
			{
				for (uint j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
				{
					vec3 pos = pParticleSystem->mParticlePositions[j];
					tempVertexBuffer[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 1] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 2] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 3] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 4] = { v3ToF3(pos + camUp + camRight), float3(0.0f, 1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f) };
				}
			}

			BufferUpdateDesc particleBufferUpdateDesc = { pParticleSystem->pParticleBuffer, tempVertexBuffer.data() };
			particleBufferUpdateDesc.mSize = sizeof(Vertex) * 6 * pParticleSystem->mLifeParticleCount;
			updateResource(&particleBufferUpdateDesc);
		}

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		gOpaqueObjectCount = 0;
		for (size_t i = 0; i < gScene.mObjects.size(); ++i)
		{
			const Object* pObj = &gScene.mObjects[i];
			if (pObj->mColor.getW() == 1.0f)
			{
				ASSERT(gOpaqueObjectCount < MAX_NUM_OBJECTS);
				gObjectInfoUniformData.mObjectInfo[gOpaqueObjectCount].mToWorldMat = mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
				gObjectInfoUniformData.mObjectInfo[gOpaqueObjectCount].mColor = pObj->mColor;
				++gOpaqueObjectCount;
			}
		}

		BufferUpdateDesc opaqueBufferUpdateDesc = {pBufferOpaqueObjectTransforms, &gObjectInfoUniformData};
		updateResource(&opaqueBufferUpdateDesc);		

		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortObjects)
		{
			tinystl::vector<tinystl::pair<float, int>> sortedArray;

			gTransparentObjectCount = 0;
			for (size_t i = 0; i < gScene.mObjects.size(); ++i)
			{
				const Object* pObj = &gScene.mObjects[i];
				if (pObj->mColor.getW() < 1.0f)
					sortedArray.push_back(tinystl::make_pair((float)distSqr(Point3(camPos), Point3(pObj->mPosition)) - (float)pow(maxElem(pObj->mScale), 2), (int)i));
			}

			gTransparentObjectCount = (int)sortedArray.size();
			QuickSort(sortedArray.data(), 0, gTransparentObjectCount - 1);

			for (uint i = 0; i < gTransparentObjectCount; ++i)
			{
				const Object* pObj = &gScene.mObjects[sortedArray[gTransparentObjectCount - i - 1].second];
				gObjectInfoUniformData.mObjectInfo[i].mToWorldMat = mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
				gObjectInfoUniformData.mObjectInfo[i].mColor = pObj->mColor;
			}
		}
		else
		{
			gTransparentObjectCount = 0;
			for (size_t i = 0; i < gScene.mObjects.size(); ++i)
			{
				const Object* pObj = &gScene.mObjects[i];
				if (pObj->mColor.getW() < 1.0f)
				{
					ASSERT(gTransparentObjectCount < MAX_NUM_OBJECTS);
					gObjectInfoUniformData.mObjectInfo[gTransparentObjectCount].mToWorldMat = mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
					gObjectInfoUniformData.mObjectInfo[gTransparentObjectCount].mColor = pObj->mColor;
					++gTransparentObjectCount;
				}
			}
		}

		BufferUpdateDesc transparentBufferUpdateDesc = { pBufferTransparentObjectTransforms, &gObjectInfoUniformData };
		updateResource(&transparentBufferUpdateDesc);

		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortObjects)
		{
			tinystl::vector<tinystl::pair<float, int>> sortedArray;

			for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[i].mObject;
				sortedArray.push_back(tinystl::make_pair((float)distSqr(Point3(camPos), Point3(pObj->mPosition)), (int)i));
			}

			QuickSort(sortedArray.data(), 0, (int)sortedArray.size() - 1);

			for (uint i = 0; i < sortedArray.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[sortedArray[(int)sortedArray.size() - i - 1].second].mObject;
				gObjectInfoUniformData.mObjectInfo[i].mToWorldMat = mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
				gObjectInfoUniformData.mObjectInfo[i].mColor = pObj->mColor;
			}
		}
		else
		{
			for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[i].mObject;
				gObjectInfoUniformData.mObjectInfo[i].mToWorldMat = mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
				gObjectInfoUniformData.mObjectInfo[i].mColor = pObj->mColor;
			}
		}

		BufferUpdateDesc particleBufferUpdateDesc = { pBufferParticleSystemTransforms, &gObjectInfoUniformData };
		updateResource(&particleBufferUpdateDesc);
		/************************************************************************/
		// Update Camera
		/************************************************************************/
		BufferUpdateDesc cameraCbv = { pBufferCameraUniform, &gCameraUniformData };
		gCameraUniformData.mPosition = vec4(pCameraController->getViewPosition(), 1);
		updateResource(&cameraCbv);

		/************************************************************************/
		// Update Skybox
		/************************************************************************/
		BufferUpdateDesc skyboxViewProjCbv = { pBufferSkyboxUniform, &gSkyboxUniformData };
		viewMat.setTranslation(vec3(0,0,0));
		gSkyboxUniformData.mViewProject = projMat * viewMat;
		updateResource(&skyboxViewProjCbv);

		/************************************************************************/
		// Light Matrix Update
		/************************************************************************/
		BufferUpdateDesc lightBufferCbv = {pBufferLightUniform, &gLightUniformData};

		vec3 lightPos = vec3(gLightCpuSettings.mLightPosition.x, gLightCpuSettings.mLightPosition.y, gLightCpuSettings.mLightPosition.z);
		pLightView->moveTo(lightPos);
		pLightView->lookAt(gObjectsCenter);

		mat4 lightViewMat = pLightView->getViewMatrix();
		//perspective as spotlight, for future use
		mat4 lightProjMat = mat4::perspective(PI / 1.2f, aspectInverse, 1.f, 180);
		gLightUniformData.mLightDirection = vec4(gObjectsCenter - lightPos, 0);
		gLightUniformData.mLightViewProj = lightProjMat * lightViewMat;
		gLightUniformData.mLightColor = vec4(1, 1, 1, 1);

		updateResource(&lightBufferCbv);

		/************************************************************************/
		// Update transparency settings
		/************************************************************************/
		if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
		{
			BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings, &gWBOITSettingsData };
			wboitSettingsUpdateDesc.mSize = sizeof(WBOITSettings);
			updateResource(&wboitSettingsUpdateDesc);

		}
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
		{
			BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings, &gWBOITVolitionSettingsData };
			wboitSettingsUpdateDesc.mSize = sizeof(WBOITVolitionSettings);
			updateResource(&wboitSettingsUpdateDesc);
		}

		/************************************************************************/
		////////////////////////////////////////////////////////////////
		gAppUI.Update(deltaTime);
	}

	static void DrawSkybox(Cmd* pCmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mClearColorValues[0] = pRenderTargetScreen->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		cmdBeginDebugMarker(pCmd, 0, 0, 1, "Draw skybox");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Draw Skybox", true);

		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, NULL, &loadActions, NULL, NULL, -1, -1);

		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);
		
		cmdBindPipeline(pCmd, pPipelineSkybox);

		DescriptorData params[7] = {};
		params[0].pName = "SkyboxUniformBlock";
		params[0].ppBuffers = &pBufferSkyboxUniform;
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
		cmdBindDescriptors(pCmd, pRootSignatureSkybox, 7, params);
		cmdBindVertexBuffer(pCmd, 1, &pBufferSkyboxVertex, NULL);
		cmdDraw(pCmd, 36, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

	void OpaquePass(Cmd* pCmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw the opaque objects.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw opaque geometry");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render opaque geometry", true);

		cmdBindVertexBuffer(pCmd, 1, &pBufferCubeVertex, NULL);

		cmdBindPipeline(pCmd, pPipelineForwardShade);

		DescriptorData params[4] = {};
		params[0].pName = "ObjectUniformBlock";
		params[0].ppBuffers = &pBufferOpaqueObjectTransforms;
		params[1].pName = "LightUniformBlock";
		params[1].ppBuffers = &pBufferLightUniform;
		params[2].pName = "CameraUniform";
		params[2].ppBuffers = &pBufferCameraUniform;
		uint drawInfoRootConstant = 0;
		params[3].pName = "DrawInfoRootConstant";
		params[3].pRootConstant = &drawInfoRootConstant;

		cmdBindDescriptors(pCmd, pRootSignatureForwardShade, 4, params);
		cmdDrawInstanced(pCmd, 36, 0, gOpaqueObjectCount, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

	void AlphaBlendTransparentPass(Cmd* pCmd)
	{
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry", true);

		cmdBindVertexBuffer(pCmd, 1, &pBufferCubeVertex, NULL);

		cmdBindPipeline(pCmd, pPipelineTransparentForwardShade);

		DescriptorData params[4] = {};
		params[0].pName = "ObjectUniformBlock";
		params[0].ppBuffers = &pBufferTransparentObjectTransforms;
		params[1].pName = "LightUniformBlock";
		params[1].ppBuffers = &pBufferLightUniform;
		params[2].pName = "CameraUniform";
		params[2].ppBuffers = &pBufferCameraUniform;
		uint drawInfoRootConstant = 0;
		params[3].pName = "DrawInfoRootConstant";
		params[3].pRootConstant = &drawInfoRootConstant;

		cmdBindDescriptors(pCmd, pRootSignatureForwardShade, 4, params);
		cmdDrawInstanced(pCmd, 36, 0, gTransparentObjectCount, 0);

		// Draw particles
		params[0].ppBuffers = &pBufferParticleSystemTransforms;

		for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
		{
			params[1].pName = "DrawInfoRootConstant";
			params[1].pRootConstant = &i;

			cmdBindDescriptors(pCmd, pRootSignatureForwardShade, 2, params);

			ParticleSystem* pParticleSystem = &gScene.mParticleSystems[i];
			cmdBindVertexBuffer(pCmd, 1, &pParticleSystem->pParticleBuffer, NULL);
			cmdDraw(pCmd, (uint)pParticleSystem->mLifeParticleCount * 6, 0);
		}

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

	void WeightedBlendedOrderIndependentTransparencyPass(Cmd* pCmd, bool volition)
	{
		Pipeline* pShadePipeline = volition ? pPipelineWBOITVolitionShade : pPipelineWBOITShade;
		Pipeline* pCompositePipeline = volition ? pPipelineWBOITVolitionComposite : pPipelineWBOITComposite;
		RootSignature* pShadeRootSignature = volition ? pRootSignatureWBOITVolitionShade : pRootSignatureWBOITShade;
		RootSignature* pCompositeRootSignature = volition ? pRootSignatureWBOITVolitionComposite : pRootSignatureWBOITComposite;

		TextureBarrier textureBarriers[WBOIT_RT_COUNT] = {};
		for (int i = 0; i < WBOIT_RT_COUNT; ++i)
		{
			textureBarriers[i].pTexture = pRenderTargetWBOIT[i]->pTexture;
			textureBarriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
		}
		cmdResourceBarrier(pCmd, 0, nullptr, WBOIT_RT_COUNT, textureBarriers, false);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetWBOIT[WBOIT_RT_ACCUMULATION]->mDesc.mClearValue;
		loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[1] = pRenderTargetWBOIT[WBOIT_RT_REVEALAGE]->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, WBOIT_RT_COUNT, pRenderTargetWBOIT, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetWBOIT[0]->mDesc.mWidth, (float)pRenderTargetWBOIT[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetWBOIT[0]->mDesc.mWidth, pRenderTargetWBOIT[0]->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (WBOIT)");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry (WBOIT)", true);

		cmdBindVertexBuffer(pCmd, 1, &pBufferCubeVertex, NULL);

		cmdBindPipeline(pCmd, pShadePipeline);

		DescriptorData shadeParams[5] = {};
		shadeParams[0].pName = "ObjectUniformBlock";
		shadeParams[0].ppBuffers = &pBufferTransparentObjectTransforms;
		shadeParams[1].pName = "LightUniformBlock";
		shadeParams[1].ppBuffers = &pBufferLightUniform;
		shadeParams[2].pName = "CameraUniform";
		shadeParams[2].ppBuffers = &pBufferCameraUniform;
		shadeParams[3].pName = "WBOITSettings";
		shadeParams[3].ppBuffers = &pBufferWBOITSettings;
		uint drawInfoRootConstant = 0;
		shadeParams[4].pName = "DrawInfoRootConstant";
		shadeParams[4].pRootConstant = &drawInfoRootConstant;

		cmdBindDescriptors(pCmd, pShadeRootSignature, 5, shadeParams);
		cmdDrawInstanced(pCmd, 36, 0, gTransparentObjectCount, 0);

		// Draw particles
		shadeParams[0].ppBuffers = &pBufferParticleSystemTransforms;

		for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
		{
			shadeParams[1].pName = "DrawInfoRootConstant";
			shadeParams[1].pRootConstant = &i;
			cmdBindDescriptors(pCmd, pShadeRootSignature, 2, shadeParams);

			ParticleSystem* pParticleSystem = &gScene.mParticleSystems[i];
			cmdBindVertexBuffer(pCmd, 1, &pParticleSystem->pParticleBuffer, NULL);
			cmdDraw(pCmd, (uint)pParticleSystem->mLifeParticleCount * 6, 0);
		}

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		// Composite WBOIT buffers
		for (int i = 0; i < WBOIT_RT_COUNT; ++i)
		{
			textureBarriers[i].pTexture = pRenderTargetWBOIT[i]->pTexture;
			textureBarriers[i].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		}
		cmdResourceBarrier(pCmd, 0, nullptr, WBOIT_RT_COUNT, textureBarriers, false);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, nullptr, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite WBOIT buffers");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Composite WBOIT buffers", true);

		cmdBindPipeline(pCmd, pCompositePipeline);

		DescriptorData compositeParams[2] = {};
		compositeParams[0].pName = "AccumulationTexture";
		compositeParams[0].ppTextures = &pRenderTargetWBOIT[WBOIT_RT_ACCUMULATION]->pTexture;
		compositeParams[1].pName = "RevealageTexture";
		compositeParams[1].ppTextures = &pRenderTargetWBOIT[WBOIT_RT_REVEALAGE]->pTexture;

		cmdBindDescriptors(pCmd, pCompositeRootSignature, 2, compositeParams);
		cmdDraw(pCmd, 3, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

#if defined(DIRECT3D12) && !defined(_DURANGO)
	void AdaptiveOrderIndependentTransparency(Cmd* pCmd)
	{
		// Clear AOIT buffers
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw fullscreen quad.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Clear AOIT buffers");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Clear AOIT buffers", true);

		cmdBindPipeline(pCmd, pPipelineAOITClear);

		DescriptorData clearParams[1] = {};
		clearParams[0].pName = "AOITClearMaskUAV";
		clearParams[0].ppTextures = &pTextureAOITClearMask;

		cmdBindDescriptors(pCmd, pRootSignatureAOITClear, 1, clearParams);
		cmdDraw(pCmd, 3, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		TextureBarrier textureBarrier = {};
		textureBarrier.pTexture = pTextureAOITClearMask;

		cmdResourceBarrier(pCmd, 0, NULL, 1, &textureBarrier, false);

		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth, (float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth, pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (AOIT)");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry (AOIT)", true);

		cmdBindVertexBuffer(pCmd, 1, &pBufferCubeVertex, NULL);

		cmdBindPipeline(pCmd, pPipelineAOITShade);

		int shadeParamsCount = 5;
		DescriptorData shadeParams[7] = {};
		shadeParams[0].pName = "ObjectUniformBlock";
		shadeParams[0].ppBuffers = &pBufferTransparentObjectTransforms;
		shadeParams[1].pName = "LightUniformBlock";
		shadeParams[1].ppBuffers = &pBufferLightUniform;
		shadeParams[2].pName = "CameraUniform";
		shadeParams[2].ppBuffers = &pBufferCameraUniform;
		shadeParams[3].pName = "AOITClearMaskUAV";
		shadeParams[3].ppTextures = &pTextureAOITClearMask;
		shadeParams[4].pName = "AOITColorDataUAV";
		shadeParams[4].ppBuffers = &pBufferAOITColorData;
#if AOIT_NODE_COUNT != 2
		shadeParams[5].pName = "AOITDepthDataUAV";
		shadeParams[5].ppBuffers = &pBufferAOITDepthData;
		shadeParamsCount = 6;
#endif

		cmdBindDescriptors(pCmd, pRootSignatureAOITShade, shadeParamsCount, shadeParams);
		cmdDrawInstanced(pCmd, 36, 0, gTransparentObjectCount, 0);

		// Draw particles
		shadeParams[0].ppBuffers = &pBufferParticleSystemTransforms;

		for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
		{
			shadeParams[shadeParamsCount].pName = "DrawInfoRootConstant";
			shadeParams[shadeParamsCount].pRootConstant = &i;
			cmdBindDescriptors(pCmd, pRootSignatureAOITShade, shadeParamsCount + 1, shadeParams);

			ParticleSystem* pParticleSystem = &gScene.mParticleSystems[i];
			cmdBindVertexBuffer(pCmd, 1, &pParticleSystem->pParticleBuffer, NULL);
			cmdDrawInstanced(pCmd, (uint)pParticleSystem->mLifeParticleCount * 6, 0, 1, (uint)i);
		}

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		// Composite AOIT buffers
		int bufferBarrierCount = 1;
		BufferBarrier bufferBarriers[2] = {};
		bufferBarriers[0].pBuffer = pBufferAOITColorData;
#if AOIT_NODE_COUNT != 2
		bufferBarriers[1].pBuffer = pBufferAOITDepthData;
		bufferBarrierCount = 2;
#endif
		cmdResourceBarrier(pCmd, bufferBarrierCount, bufferBarriers, 1, &textureBarrier, false);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, nullptr, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw fullscreen quad.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite AOIT buffers");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Composite AOIT buffers", true);

		cmdBindPipeline(pCmd, pPipelineAOITComposite);

		DescriptorData compositeParams[2] = {};
		compositeParams[0].pName = "AOITClearMaskSRV";
		compositeParams[0].ppTextures = &pTextureAOITClearMask;
		compositeParams[1].pName = "AOITColorDataSRV";
		compositeParams[1].ppBuffers = &pBufferAOITColorData;

		cmdBindDescriptors(pCmd, pRootSignatureAOITComposite, 2, compositeParams);
		cmdDraw(pCmd, 3, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}
#endif

	static void RenderTargetBarriers(Cmd* cmd)
	{
		tinystl::vector<TextureBarrier> rtBarrier;
		//skybox
		rtBarrier.push_back({ pRenderTargetSkybox->pTexture, RESOURCE_STATE_RENDER_TARGET });

		cmdResourceBarrier(cmd, 0, NULL, (uint32_t)rtBarrier.size(), &rtBarrier.front(), true);
	}

	void Draw() override
	{
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		gCpuTimer.GetUSec(true);

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		Fence* pNextFence = pRenderCompleteFences[gFrameIndex];
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pNextFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pNextFence, false);

		Semaphore* pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence* pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Get command list to store rendering commands for this frame
		Cmd* pCmd = ppCmds[gFrameIndex];

		pRenderTargetScreen = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		beginCmd(pCmd);
		cmdBeginGpuFrameProfile(pCmd, pGpuProfiler);
		RenderTargetBarriers(pCmd);
		TextureBarrier barriers1[] = {
			{pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET},
			{pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE},
		};
		cmdResourceBarrier(pCmd, 0, NULL, 2, barriers1, false);
		
		cmdFlushBarriers(pCmd);

		DrawSkybox(pCmd);
		OpaquePass(pCmd);

		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
			AlphaBlendTransparentPass(pCmd);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
			WeightedBlendedOrderIndependentTransparencyPass(pCmd, false);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
			WeightedBlendedOrderIndependentTransparencyPass(pCmd, true);
#if defined(DIRECT3D12) && !defined(_DURANGO)
		else if (gTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT)
			AdaptiveOrderIndependentTransparency(pCmd);
#endif
		else
			ASSERT(false && "Not implemented.");

		////////////////////////////////////////////////////////
		//  Draw UIs
		cmdBeginDebugMarker(pCmd, 0, 1, 0, "Draw UI");
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, NULL, NULL, NULL, NULL, -1, -1);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifndef TARGET_IOS
		drawDebugText(pCmd, 8.0f, 15.0f, tinystl::string::format("CPU Time: %f ms", gCpuTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
		drawDebugText(pCmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugText(pCmd, 8.0f, 65.0f, tinystl::string::format("Frame Time: %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

		drawDebugGpuProfile(pCmd, 8.0f, 90.0f, pGpuProfiler, NULL);
#else
		gVirtualJoystick.Draw(pCmd, pCameraController, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif
		
#ifndef TARGET_IOS
		gAppUI.Gui(pGuiWindow);
#endif
		gAppUI.Draw(pCmd);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndDebugMarker(pCmd);
		////////////////////////////////////////////////////////

		barriers1[0] = {pRenderTargetScreen->pTexture, RESOURCE_STATE_PRESENT};
		cmdResourceBarrier(pCmd, 0, NULL, 1, barriers1, true);

		cmdEndGpuFrameProfile(pCmd, pGpuProfiler);
		endCmd(pCmd);
		
		queueSubmit(pGraphicsQueue, 1, &pCmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
					&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);


	}

	tinystl::string GetName() override
	{
		return "15_Transparency";
	}

	bool AddSwapChain() const
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
		swapChainDesc.mColorFormat = ImageFormat::BGRA8;
		swapChainDesc.mColorClearValue = { 1, 0, 1, 1 };
		swapChainDesc.mSrgb = false;

		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
		return pSwapChain != NULL;
	}

	bool AddRenderTargetsAndSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		const ClearValue depthClear = {1.0f, 0};
		const ClearValue colorClearBlack = {0.0f, 0.0f, 0.0f, 0.0f};
		const ClearValue colorClearWhite = {1.0f, 1.0f, 1.0f, 1.0f};

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
		// WBOIT render targets
		/************************************************************************/
		ClearValue wboitClearValues[] = { colorClearBlack, colorClearWhite };
		const wchar_t* wboitNames[] = { L"Accumulation RT", L"Revealage RT" };
		for (int i = 0; i < WBOIT_RT_COUNT; ++i)
		{
			RenderTargetDesc renderTargetDesc = {};
			renderTargetDesc.mArraySize = 1;
			renderTargetDesc.mClearValue = wboitClearValues[i];
			renderTargetDesc.mDepth = 1;
			renderTargetDesc.mFormat = gWBOITRenderTargetFormats[i];
			renderTargetDesc.mWidth = width;
			renderTargetDesc.mHeight = height;
			renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
			renderTargetDesc.mSampleQuality = 0;
			renderTargetDesc.pDebugName = wboitNames[i];
			addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetWBOIT[i]);
		}

		return AddSwapChain();
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

void GuiController::UpdateDynamicUI()
{
	if (gTransparencyType != GuiController::currentTransparencyType)
	{
		if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
			GuiController::alphaBlendDynamicWidgets.HideDynamicProperties(pGuiWindow);
		else if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
			GuiController::weightedBlendedOitDynamicWidgets.HideDynamicProperties(pGuiWindow);
		else if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
			GuiController::weightedBlendedOitVolitionDynamicWidgets.HideDynamicProperties(pGuiWindow);

		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
			GuiController::alphaBlendDynamicWidgets.ShowDynamicProperties(pGuiWindow);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
			GuiController::weightedBlendedOitDynamicWidgets.ShowDynamicProperties(pGuiWindow);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
			GuiController::weightedBlendedOitVolitionDynamicWidgets.ShowDynamicProperties(pGuiWindow);

		GuiController::currentTransparencyType = (TransparencyType)gTransparencyType;
	}
}

void GuiController::AddGui()
{	
	static const char* transparencyTypeNames[] = 
	{
		"Alpha blended",
		"(WBOIT) Weighted blended order independent transparency",
		"(WBOIT) Weighted blended order independent transparency - Volition",
#if defined(DIRECT3D12) && !defined(_DURANGO)
		"(AOIT) Adaptive order independent transparency",
#endif
		NULL//needed for unix
	};

	static const uint32_t transparencyTypeValues[] = 
	{
		TRANSPARENCY_TYPE_ALPHA_BLEND,
		TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT,
		TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION,
#if defined(DIRECT3D12) && !defined(_DURANGO)
		TRANSPARENCY_TYPE_ADAPTIVE_OIT,
#endif
		0//needed for unix
	};

	uint32_t dropDownCount = 3;
#if defined(DIRECT3D12) && !defined(_DURANGO)
	if (pRenderer->pActiveGpuSettings->mROVsSupported)
		dropDownCount = 4;
#endif

	pGuiWindow->AddWidget(DropdownWidget("Transparency Type", &gTransparencyType, transparencyTypeNames, transparencyTypeValues, dropDownCount));
	
	// TRANSPARENCY_TYPE_ALPHA_BLEND Widgets
	{
		static LabelWidget blendSettings("Blend Settings");		
		GuiController::alphaBlendDynamicWidgets.mDynamicProperties.emplace_back(&blendSettings);

		static CheckboxWidget sortObjects("Sort Objects", &gAlphaBlendSettings.mSortObjects);
		GuiController::alphaBlendDynamicWidgets.mDynamicProperties.emplace_back(&sortObjects);

		static CheckboxWidget sortParticles("Sort Particles", &gAlphaBlendSettings.mSortParticles);		
		GuiController::alphaBlendDynamicWidgets.mDynamicProperties.emplace_back(&sortParticles);
	}
	// TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT Widgets
	{
		static LabelWidget blendSettings("Blend Settings");
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&blendSettings);

		static SliderFloatWidget colorResistance("Color Resistance", &gWBOITSettingsData.mColorResistance, 1.0f, 25.0f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&colorResistance);

		static SliderFloatWidget rangeAdjustment("Range Adjustment", &gWBOITSettingsData.mRangeAdjustment, 0.0f, 1.0f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&rangeAdjustment);

		static SliderFloatWidget depthRange("Depth Range", &gWBOITSettingsData.mDepthRange, 0.1f, 500.0f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&depthRange);

		static SliderFloatWidget orderingStrength("Ordering Strength", &gWBOITSettingsData.mOrderingStrength, 0.1f, 25.0f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&orderingStrength);

		static SliderFloatWidget underflowLimit("Underflow Limit", &gWBOITSettingsData.mUnderflowLimit, 1e-4f, 1e-1f, 1e-4f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&underflowLimit);

		static SliderFloatWidget overflowLimit("Overflow Limit", &gWBOITSettingsData.mOverflowLimit, 3e1f, 3e4f);
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&overflowLimit);

		static ButtonWidget resetButton("Reset");
		resetButton.pOnDeactivatedAfterEdit = ([]() { gWBOITSettingsData = WBOITSettings(); });
		GuiController::weightedBlendedOitDynamicWidgets.mDynamicProperties.emplace_back(&resetButton);
	}
	// TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION Widgets
	{
		static LabelWidget blendSettings("Blend Settings");
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&blendSettings);

		static SliderFloatWidget opacitySensitivity("Opacity Sensitivity", &gWBOITVolitionSettingsData.mOpacitySensitivity, 1.0f, 25.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&opacitySensitivity);

		static SliderFloatWidget weightBias("Weight Bias", &gWBOITVolitionSettingsData.mWeightBias, 0.0f, 25.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&weightBias);

		static SliderFloatWidget precisionScalar("Precision Scalar", &gWBOITVolitionSettingsData.mPrecisionScalar, 100.0f, 100000.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&precisionScalar);

		static SliderFloatWidget maximumWeight("Maximum Weight", &gWBOITVolitionSettingsData.mMaximumWeight, 0.1f, 100.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&maximumWeight);

		static SliderFloatWidget maximumColorValue("Maximum Color Value", &gWBOITVolitionSettingsData.mMaximumColorValue, 100.0f, 10000.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&maximumColorValue);

		static SliderFloatWidget additiveSensitivity("Additive Sensitivity", &gWBOITVolitionSettingsData.mAdditiveSensitivity, 0.1f, 25.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&additiveSensitivity);

		static SliderFloatWidget emissiveSensitivity("Emissive Sensitivity", &gWBOITVolitionSettingsData.mEmissiveSensitivity, 0.01f, 1.0f);
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&emissiveSensitivity);

		static ButtonWidget resetButton("Reset");
		resetButton.pOnDeactivatedAfterEdit = ([]() { gWBOITVolitionSettingsData = WBOITVolitionSettings(); });
		GuiController::weightedBlendedOitVolitionDynamicWidgets.mDynamicProperties.emplace_back(&resetButton);
	}
		
	pGuiWindow->AddWidget(LabelWidget("Light Settings"));

	const float lightPosBound = 10.0f;	
	pGuiWindow->AddWidget(SliderFloat3Widget("Light Position", &gLightCpuSettings.mLightPosition, -lightPosBound, lightPosBound, 0.1f));


	if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_ALPHA_BLEND;
		GuiController::alphaBlendDynamicWidgets.ShowDynamicProperties(pGuiWindow);
	}
	else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT;
		GuiController::weightedBlendedOitDynamicWidgets.ShowDynamicProperties(pGuiWindow);
	}
	else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION;
		GuiController::weightedBlendedOitVolitionDynamicWidgets.ShowDynamicProperties(pGuiWindow);
	}
#if defined(DIRECT3D12) && !defined(_DURANGO)	
	else if (gTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT && pRenderer->pActiveGpuSettings->mROVsSupported)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_ADAPTIVE_OIT;
	}
#endif
}


DEFINE_APPLICATION_MAIN(Transparency)
