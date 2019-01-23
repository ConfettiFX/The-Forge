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

#define MAX_NUM_OBJECTS 128
#define MAX_NUM_PARTICLES 2048    // Per system
#define CUBES_EACH_ROW 5
#define CUBES_EACH_COL 5
#define CUBE_NUM (CUBES_EACH_ROW * CUBES_EACH_COL + 1)
#define DEBUG_OUTPUT 1       //exclusively used for texture data visulization, such as rendering depth, shadow map etc.
#define AOIT_NODE_COUNT 4    // 2, 4 or 8. Higher numbers give better results at the cost of performance
#if AOIT_NODE_COUNT == 2
#define AOIT_RT_COUNT 1
#else
#define AOIT_RT_COUNT (AOIT_NODE_COUNT / 4)
#endif
#define USE_SHADOWS 1
#define PT_USE_REFRACTION 1
#define PT_USE_DIFFUSION 1
#define PT_USE_CAUSTICS (0 & USE_SHADOWS)

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
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"

//GPU Profiler
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

const uint32_t gImageCount = 3;

typedef struct Vertex
{
	float3 mPosition;
	float3 mNormal;
	float2 mUV;
} Vertex;

typedef struct Material
{
	float4 mColor;
	float4 mTransmission;
	float  mRefractionRatio;
	float  mCollimation;
	float2 mPadding;
	uint   mTextureFlags;
	uint   mAlbedoTexture;
	uint   mMetallicTexture;
	uint   mRoughnessTexture;
	uint   mEmissiveTexture;
	uint   mPadding2[3];
} Material;

typedef enum MeshResource
{
	MESH_CUBE,
	MESH_SPHERE,
	MESH_PLANE,
	MESH_LION,
	MESH_COUNT,
	/* vvv These meshes have different behaviour to the other meshes vvv */
	MESH_PARTICLE_SYSTEM
} MeshResource;

typedef struct MeshData
{
	Buffer* pVertexBuffer = NULL;
	uint    mVertexCount = 0;
	Buffer* pIndexBuffer = NULL;
	uint    mIndexCount = 0;
} MeshData;

typedef struct Object
{
	vec3         mPosition;
	vec3         mScale;
	vec3         mOrientation;
	MeshResource mMesh;
	Material     mMaterial;
} Object;

typedef struct ParticleSystem
{
	Buffer* pParticleBuffer;
	Object  mObject;

	tinystl::vector<vec3>  mParticlePositions;
	tinystl::vector<vec3>  mParticleVelocities;
	tinystl::vector<float> mParticleLifetimes;
	size_t                 mLifeParticleCount;
} ParticleSystem;

typedef struct Scene
{
	tinystl::vector<Object>         mObjects;
	tinystl::vector<ParticleSystem> mParticleSystems;
} Scene;

typedef struct DrawCall
{
	uint         mIndex;
	uint         mInstanceCount;
	uint         mInstanceOffset;
	MeshResource mMesh;
} DrawCall;

typedef struct ObjectInfoStruct
{
	mat4   mToWorldMat;
	mat4   mNormalMat;
	uint   mMaterialIndex;
	float3 mPadding;
} ObjectInfoStruct;

typedef struct MaterialUniformBlock
{
	Material mMaterials[MAX_NUM_OBJECTS];
} MaterialUniformBlock;

typedef struct ObjectInfoUniformBlock
{
	ObjectInfoStruct mObjectInfo[MAX_NUM_OBJECTS];
} ObjectInfoUniformBlock;

typedef struct SkyboxUniformBlock
{
	mat4 mViewProject;
} SkyboxUniformBlock;

typedef struct LightUniformBlock
{
	mat4 mLightViewProj;
	vec4 mLightDirection = { -1, -1, -1, 0 };
	vec4 mLightColor = { 1, 0, 0, 1 };
} LightUniformBlock;

typedef struct CameraUniform
{
	mat4 mViewProject;
	mat4 mViewMat;
	vec4 mClipInfo;
	vec4 mPosition;
} CameraUniform;

typedef struct AlphaBlendSettings
{
	bool mSortObjects = true;
	bool mSortParticles = true;
} AlphaBlendSettings;

typedef struct WBOITSettings
{
	float mColorResistance = 1.0f;    // Increase if low-coverage foreground transparents are affecting background transparent color.
	float mRangeAdjustment = 0.3f;    // Change to avoid saturating at the clamp bounds.
	float mDepthRange =
		200.0f;    // Decrease if high-opacity surfaces seem �too transparent�, increase if distant transparents are blending together too much.
	float mOrderingStrength = 4.0f;    // Increase if background is showing through foreground too much.
	float mUnderflowLimit = 1e-2f;     // Increase to reduce underflow artifacts.
	float mOverflowLimit = 3e3f;       // Decrease to reduce overflow artifacts.
} WBOITSettings;

typedef struct WBOITVolitionSettings
{
	float mOpacitySensitivity =
		3.0f;    // Should be greater than 1, so that we only downweight nearly transparent things. Otherwise, everything at the same depth should get equal weight. Can be artist controlled
	float mWeightBias =
		5.0f;    // Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than near things, as well as preventing floating point underflow
	float mPrecisionScalar =
		10000.0f;    // Adjusts where the weights fall in the floating point range, used to balance precision to combat both underflow and overflow
	float mMaximumWeight =
		20.0f;    // Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower" effect of very near vs. very far things
	float mMaximumColorValue = 1000.0f;
	float mAdditiveSensitivity = 10.0f;    // How much we amplify the emissive when deciding whether to consider this additively blended
	float mEmissiveSensitivity = 0.5f;     // Artist controlled value between 0.01 and 1
} WBOITVolitionSettings;

typedef enum WBOITRenderTargets
{
	WBOIT_RT_ACCUMULATION,
	WBOIT_RT_REVEALAGE,
	WBOIT_RT_COUNT
} WBOITRenderTargets;

ImageFormat::Enum gWBOITRenderTargetFormats[WBOIT_RT_COUNT] = { ImageFormat::RGBA16F, ImageFormat::RGBA8 };

typedef enum PTRenderTargets
{
	PT_RT_ACCUMULATION,    // Shared with WBOIT
	PT_RT_MODULATION,
#if PT_USE_REFRACTION != 0
	PT_RT_REFRACTION,
#endif
	PT_RT_COUNT
} PTRenderTargets;

ImageFormat::Enum gPTRenderTargetFormats[3] = { ImageFormat::RGBA16F, ImageFormat::RGBA8, ImageFormat::RG16F };

typedef enum TextureResource
{
	TEXTURE_SKYBOX_RIGHT,
	TEXTURE_SKYBOX_LEFT,
	TEXTURE_SKYBOX_UP,
	TEXTURE_SKYBOX_DOWN,
	TEXTURE_SKYBOX_FRONT,
	TEXTURE_SKYBOX_BACK,
	TEXTURE_MEASURING_GRID,
	TEXTURE_COUNT
} TextureResource;

/************************************************************************/
// Shaders
/************************************************************************/
Shader* pShaderSkybox = NULL;
#if USE_SHADOWS != 0
Shader* pShaderShadow = NULL;
Shader* pShaderGaussianBlur = NULL;
#if PT_USE_CAUSTICS != 0
Shader* pShaderPTShadow = NULL;
Shader* pShaderPTDownsample = NULL;
Shader* pShaderPTCopyShadowDepth = NULL;
#endif
#endif
Shader* pShaderForward = NULL;
Shader* pShaderWBOITShade = NULL;
Shader* pShaderWBOITComposite = NULL;
Shader* pShaderWBOITVShade = NULL;
Shader* pShaderWBOITVComposite = NULL;
Shader* pShaderPTShade = NULL;
Shader* pShaderPTComposite = NULL;
#if PT_USE_DIFFUSION != 0
Shader* pShaderPTCopyDepth = NULL;
Shader* pShaderPTGenMips = NULL;
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
Shader* pShaderAOITShade = NULL;
Shader* pShaderAOITComposite = NULL;
Shader* pShaderAOITClear = NULL;
#endif

/************************************************************************/
// Root signature
/************************************************************************/
RootSignature* pRootSignatureSkybox = NULL;
#if USE_SHADOWS != 0
RootSignature* pRootSignatureShadow = NULL;
RootSignature* pRootSignatureGaussianBlur = NULL;
#if PT_USE_CAUSTICS != 0
RootSignature* pRootSignaturePTShadow = NULL;
RootSignature* pRootSignaturePTDownsample = NULL;
RootSignature* pRootSignaturePTCopyShadowDepth = NULL;
#endif
#endif
RootSignature* pRootSignatureForward = NULL;
RootSignature* pRootSignatureWBOITShade = NULL;
RootSignature* pRootSignatureWBOITComposite = NULL;
RootSignature* pRootSignatureWBOITVShade = NULL;
RootSignature* pRootSignatureWBOITVComposite = NULL;
RootSignature* pRootSignaturePTShade = NULL;
RootSignature* pRootSignaturePTComposite = NULL;
#if PT_USE_DIFFUSION != 0
RootSignature* pRootSignaturePTCopyDepth = NULL;
RootSignature* pRootSignaturePTGenMips = NULL;
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
RootSignature* pRootSignatureAOITShade = NULL;
RootSignature* pRootSignatureAOITComposite = NULL;
RootSignature* pRootSignatureAOITClear = NULL;
#endif

/************************************************************************/
// Pipelines
/************************************************************************/
Pipeline* pPipelineSkybox = NULL;
#if USE_SHADOWS != 0
Pipeline* pPipelineShadow = NULL;
Pipeline* pPipelineGaussianBlur = NULL;
#if PT_USE_CAUSTICS != 0
Pipeline* pPipelinePTShadow = NULL;
Pipeline* pPipelinePTDownsample = NULL;
Pipeline* pPipelinePTCopyShadowDepth = NULL;
#endif
#endif
Pipeline* pPipelineForward = NULL;
Pipeline* pPipelineTransparentForward = NULL;
Pipeline* pPipelineWBOITShade = NULL;
Pipeline* pPipelineWBOITComposite = NULL;
Pipeline* pPipelineWBOITVShade = NULL;
Pipeline* pPipelineWBOITVComposite = NULL;
Pipeline* pPipelinePTShade = NULL;
Pipeline* pPipelinePTComposite = NULL;
#if PT_USE_DIFFUSION != 0
Pipeline* pPipelinePTCopyDepth = NULL;
Pipeline* pPipelinePTGenMips = NULL;
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
Pipeline* pPipelineAOITShade = NULL;
Pipeline* pPipelineAOITComposite = NULL;
Pipeline* pPipelineAOITClear = NULL;
#endif

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget* pRenderTargetScreen = NULL;
RenderTarget* pRenderTargetDepth = NULL;
#if PT_USE_DIFFUSION != 0
RenderTarget* pRenderTargetPTDepthCopy = NULL;
#endif
RenderTarget* pRenderTargetWBOIT[WBOIT_RT_COUNT] = {};
RenderTarget* pRenderTargetPT[PT_RT_COUNT] = {};
RenderTarget* pRenderTargetPTBackground = NULL;
#if USE_SHADOWS != 0
RenderTarget* pRenderTargetShadowVariance[2] = { NULL };
RenderTarget* pRenderTargetShadowDepth = NULL;
#if PT_USE_CAUSTICS != 0
RenderTarget* pRenderTargetPTShadowVariance[3] = { NULL };
RenderTarget* pRenderTargetPTShadowFinal[2][3] = { NULL };
#endif
#endif
/************************************************************************/
// AOIT Resources
/************************************************************************/
#if defined(DIRECT3D12) && !defined(_DURANGO)
Texture* pTextureAOITClearMask;
Buffer*  pBufferAOITDepthData;
Buffer*  pBufferAOITColorData;
#endif
/************************************************************************/
// Samplers
/************************************************************************/
Sampler* pSamplerPoint = NULL;
Sampler* pSamplerPointClamp = NULL;
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerTrilinearAniso = NULL;
Sampler* pSamplerSkybox = NULL;
Sampler* pSamplerShadow = NULL;    // Only created when USE_SHADOWS != 0

/************************************************************************/
// Rasterizer states
/************************************************************************/
RasterizerState* pRasterizerStateCullBack = NULL;
RasterizerState* pRasterizerStateCullFront = NULL;
RasterizerState* pRasterizerStateCullNone = NULL;

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
BlendState* pBlendStatePTShade = NULL;
BlendState* pBlendStatePTMinBlend = NULL;
#if defined(DIRECT3D12) && !defined(_DURANGO)
BlendState* pBlendStateAOITComposite = NULL;
#endif

/************************************************************************/
// Vertex layouts
/************************************************************************/
VertexLayout* pVertexLayoutSkybox = NULL;
VertexLayout* pVertexLayoutDefault = NULL;

/************************************************************************/
// Resources
/************************************************************************/
Buffer*   pBufferSkyboxVertex = NULL;
MeshData* pMeshes[MESH_COUNT] = {};
Texture*  pTextures[TEXTURE_COUNT] = {};

/************************************************************************/
// Uniform buffers
/************************************************************************/
Buffer* pBufferMaterials[gImageCount] = { NULL };
Buffer* pBufferOpaqueObjectTransforms[gImageCount] = { NULL };
Buffer* pBufferTransparentObjectTransforms[gImageCount] = { NULL };
Buffer* pBufferSkyboxUniform[gImageCount] = { NULL };
Buffer* pBufferLightUniform[gImageCount] = { NULL };
Buffer* pBufferCameraUniform[gImageCount] = { NULL };
Buffer* pBufferCameraLightUniform[gImageCount] = { NULL };
Buffer* pBufferWBOITSettings[gImageCount] = { NULL };

typedef enum TransparencyType
{
	TRANSPARENCY_TYPE_ALPHA_BLEND,
	TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT,
	TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION,
	TRANSPARENCY_TYPE_PHENOMENOLOGICAL,
#if defined(DIRECT3D12) && !defined(_DURANGO)
	TRANSPARENCY_TYPE_ADAPTIVE_OIT
#endif
} TransparencyType;

struct
{
	float3 mLightPosition = { 0, 10, 10 };    //light position, will be changed by GUI editor if not iOS
} gLightCpuSettings;

/************************************************************************/

#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

// Constants
uint32_t     gFrameIndex = 0;
GpuProfiler* pGpuProfiler = NULL;
float        gCurrentTime = 0.0f;

MaterialUniformBlock   gMaterialUniformData;
ObjectInfoUniformBlock gObjectInfoUniformData;
ObjectInfoUniformBlock gTransparentObjectInfoUniformData;
SkyboxUniformBlock     gSkyboxUniformData;
LightUniformBlock      gLightUniformData;
CameraUniform          gCameraUniformData;
CameraUniform          gCameraLightUniformData;
AlphaBlendSettings     gAlphaBlendSettings;
WBOITSettings          gWBOITSettingsData;
WBOITVolitionSettings  gWBOITVolitionSettingsData;

Scene                     gScene;
tinystl::vector<DrawCall> gOpaqueDrawCalls;
tinystl::vector<DrawCall> gTransparentDrawCalls;
vec3                      gObjectsCenter = { 0, 0, 0 };

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

/// UI
UIApp         gAppUI;
GuiComponent* pGuiWindow = NULL;
TextDrawDesc  gFrameTimeDraw = TextDrawDesc(0, 0xff00ffff, 18);
HiresTimer    gCpuTimer;

FileSystem gFileSystem;
LogManager gLogManager;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPool = NULL;
Cmd**    ppCmds = NULL;

SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };

uint32_t gTransparencyType = TRANSPARENCY_TYPE_PHENOMENOLOGICAL;

const char* pSkyboxImageFileNames[] = {
	"skybox/hw_sahara/sahara_rt.tga", "skybox/hw_sahara/sahara_lf.tga", "skybox/hw_sahara/sahara_up.tga",
	"skybox/hw_sahara/sahara_dn.tga", "skybox/hw_sahara/sahara_ft.tga", "skybox/hw_sahara/sahara_bk.tga",
};

const char* pszBases[FSR_Count] = {
	"../../../src/15_Transparency/",        // FSR_BinShaders
	"../../../src/15_Transparency/",        // FSR_SrcShaders
	"../../../UnitTestResources/",          // FSR_Textures
	"../../../UnitTestResources/",          // FSR_Meshes
	"../../../UnitTestResources/",          // FSR_Builtin_Fonts
	"../../../src/15_Transparency/",        // FSR_GpuConfig
	"",                                     // FSR_Animation
	"",                                     // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",    // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",      // FSR_MIDDLEWARE_UI
};

void AddObject(
	MeshResource mesh, vec3 position, vec4 color, vec3 translucency = vec3(0.0f), float eta = 1.0f, float collimation = 0.0f,
	vec3 scale = vec3(1.0f), vec3 orientation = vec3(0.0f))
{
	gScene.mObjects.push_back(
		{ position, scale, orientation, mesh, { v4ToF4(color), float4(v3ToF3(translucency), 0.0f), eta, collimation } });
}

void AddObject(MeshResource mesh, vec3 position, TextureResource texture, vec3 scale = vec3(1.0f), vec3 orientation = vec3(0.0f))
{
	gScene.mObjects.push_back(
		{ position, scale, orientation, mesh, { float4(1.0f), float4(0.0f), 1.0f, 0.0f, float2(0.0f), 1, (uint)texture, 0, 0 } });
}

void AddParticleSystem(vec3 position, vec4 color, vec3 translucency = vec3(0.0f), vec3 scale = vec3(1.0f), vec3 orientation = vec3(0.0f))
{
	Buffer*        pParticleBuffer = NULL;
	BufferLoadDesc particleBufferDesc = {};
	particleBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
	particleBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
	particleBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
	particleBufferDesc.mDesc.mSize = sizeof(Vertex) * 6 * MAX_NUM_PARTICLES;
	particleBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
	particleBufferDesc.ppBuffer = &pParticleBuffer;
	addResource(&particleBufferDesc);

	gScene.mParticleSystems.push_back(
		{ pParticleBuffer,
		  { position, scale, orientation, MESH_PARTICLE_SYSTEM, { v4ToF4(color), float4(v3ToF3(translucency), 0.0f), 1.0f, 1.0f } },
		  MAX_NUM_PARTICLES,
		  MAX_NUM_PARTICLES,
		  MAX_NUM_PARTICLES,
		  0 });
}

static void CreateScene()
{
	// Set plane
	AddObject(MESH_CUBE, vec3(0.0f), vec4(1.0f), vec3(0.0f), 1.0f, 1.0f, vec3(100.0f, 0.5f, 100.0f));

	// Set cubes
	const float cubeDist = 3.0f;
	vec3        curTrans = { -cubeDist * (CUBES_EACH_ROW - 1) / 2.f, 2.3f, -cubeDist * (CUBES_EACH_COL - 1) / 2.f };

	for (int i = 0; i < CUBES_EACH_ROW; ++i)
	{
		curTrans.setX(-cubeDist * (CUBES_EACH_ROW - 1) / 2.f);

		for (int j = 0; j < CUBES_EACH_COL; j++)
		{
			AddObject(
				MESH_CUBE, curTrans,
				vec4(float(i + 1) / CUBES_EACH_ROW, 1.0f - float(i + 1) / CUBES_EACH_ROW, 0.0f, float(j + 1) / CUBES_EACH_COL), vec3(0.0f),
				1.0f, 1.0f, vec3(1.0f));
			curTrans.setX(curTrans.getX() + cubeDist);
		}

		curTrans.setZ(curTrans.getZ() + cubeDist);
	}

	AddObject(MESH_CUBE, vec3(15.0f, 4.0f, 5.0f), vec4(1.0f, 0.0f, 0.0f, 0.9f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));
	AddObject(MESH_CUBE, vec3(15.0f, 4.0f, 0.0f), vec4(0.0f, 1.0f, 0.0f, 0.9f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));
	AddObject(MESH_CUBE, vec3(15.0f, 4.0f, -5.0f), vec4(0.0f, 0.0f, 1.0f, 0.9f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));

	AddObject(MESH_CUBE, vec3(-15.0f, 4.0f, 5.0f), vec4(1.0f, 0.0f, 0.0f, 0.5f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));
	AddObject(MESH_CUBE, vec3(-15.0f, 4.0f, 0.0f), vec4(0.0f, 1.0f, 0.0f, 0.5f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));
	AddObject(MESH_CUBE, vec3(-15.0f, 4.0f, -5.0f), vec4(0.0f, 0.0f, 1.0f, 0.5f), vec3(0.0f), 1.0f, 1.0f, vec3(4.0f, 4.0f, 0.1f));

	for (int i = 0; i < 25; ++i)
		AddObject(
			MESH_CUBE, vec3(i * 2.0f - 25.0f, 4.0f, 25.0f), vec4(3.0f, 3.0f, 10.0f, 0.1f), vec3(0.0f), 1.0f, 1.0f, vec3(0.1f, 4.0f, 4.0f));

	AddObject(MESH_CUBE, vec3(1.0f, 5.0f, -22.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(0.5f, 0.5f, 0.01f));
	AddObject(MESH_CUBE, vec3(-1.0f, 5.0f, -35.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(1.0f, 1.0f, 0.005f));
	AddObject(MESH_SPHERE, vec3(0.0f, 5.0f, -25.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.5f, 0.0f, vec3(4.0f));

	AddObject(MESH_LION, vec3(10.0f, 0.0f, -25.0f), vec4(1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(0.25f), vec3(0.0f, PI, 0.0f));
	AddObject(
		MESH_CUBE, vec3(7.0f, 5.0f, -22.0f), vec4(1.0f, 0.3f, 0.3f, 0.9f), vec3(1.0f, 0.3f, 0.3f), 1.0f, 0.0f, vec3(1.5f, 4.0f, 0.005f));
	AddObject(
		MESH_CUBE, vec3(10.0f, 5.0f, -22.0f), vec4(0.3f, 1.0f, 0.3f, 0.9f), vec3(0.3f, 1.0f, 0.3f), 1.0f, 0.5f, vec3(1.5f, 4.0f, 0.005f));
	AddObject(
		MESH_CUBE, vec3(13.0f, 5.0f, -22.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.0f, 0.9f, vec3(1.5f, 4.0f, 0.005f));

	AddParticleSystem(vec3(30.0f, 5.0f, 20.0f), vec4(1.0f, 0.0f, 0.0f, 0.5f));
	AddParticleSystem(vec3(30.0f, 5.0f, 25.0f), vec4(1.0f, 1.0f, 0.0f, 0.5f));

	AddObject(
		MESH_PLANE, vec3(-15.0f - 5.0f, 10.0f, -25.0f), TEXTURE_MEASURING_GRID, vec3(10.0f, 1.0f, 10.0f),
		vec3(-90.0f * (PI / 180.0f), PI, 0.0f));
	AddObject(
		MESH_SPHERE, vec3(-17.5f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.001f, 1.0f, vec3(1.0f));
	AddObject(MESH_SPHERE, vec3(-15.0f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.3f, 1.0f, vec3(1.0f));
	AddObject(MESH_SPHERE, vec3(-12.5f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.5f, 1.0f, vec3(1.0f));
}

int DistanceCompare(const float3& a, const float3& b)
{
	if (a.getX() < b.getX())
		return -1;
	else if (a.getX() > b.getX())
		return 1;
	if (a.getY() < b.getY())
		return -1;
	else if (a.getY() > b.getY())
		return 1;

	return 0;
}

int MeshCompare(const float2& a, const float2& b)
{
	if (a.getX() < b.getX())
		return -1;
	if (a.getX() == b.getX())
		return 0;

	return 1;
}

void SwapParticles(ParticleSystem* pParticleSystem, size_t a, size_t b)
{
	vec3  pos = pParticleSystem->mParticlePositions[a];
	vec3  vel = pParticleSystem->mParticleVelocities[a];
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
DynamicUIControls GuiController::alphaBlendDynamicWidgets;
DynamicUIControls GuiController::weightedBlendedOitDynamicWidgets;
DynamicUIControls GuiController::weightedBlendedOitVolitionDynamicWidgets;
TransparencyType  GuiController::currentTransparencyType;
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
class Transparency: public IApp
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

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Absolute))
			return false;
#endif

		CreateSamplers();
		CreateRasterizerStates();
		CreateDepthStates();
		CreateBlendStates();
		CreateShaders();
		CreateRootSignatures();
		CreateResources();
		CreateUniformBuffers();

		/************************************************************************/
		// Add GPU profiler
		/************************************************************************/
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		CreateScene();
		finishResourceLoading();

		if (!gAppUI.Init(pRenderer))
			return false;
		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartPosition = vec2(5, 200.0f) / dpiScale;
		;
		guiDesc.mStartSize = vec2(450, 600) / dpiScale;
		;
		pGuiWindow = gAppUI.AddGuiComponent(GetName(), &guiDesc);
		GuiController::AddGui();

		CameraMotionParameters cmp{ 16.0f, 60.0f, 20.0f };
		vec3                   camPos{ 0, 5, -15 };
		vec3                   lookAt{ 0, 5, 0 };

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

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		DestroySamplers();
		DestroyRasterizerStates();
		DestroyDepthStates();
		DestroyBlendStates();
		DestroyShaders();
		DestroyRootSignatures();
		DestroyResources();
		DestroyUniformBuffers();

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
		if (!CreateRenderTargetsAndSwapChain())
			return false;
		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;
#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], ImageFormat::Enum::NONE))
			return false;
#endif

		CreatePipelines();

		return true;
	}

	void Unload() override
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		gAppUI.Unload();

		DestroyPipelines();

		DestroyRenderTargetsAndSwapChian();
	}

	void Update(float deltaTime) override
	{
		gCpuTimer.Reset();

		gCurrentTime += deltaTime;

		if (getKeyDown(KEY_BUTTON_X))
			RecenterCameraView(170.0f);

		// Dynamic UI elements
		GuiController::UpdateDynamicUI();

		/************************************************************************/
		// Camera Update
		/************************************************************************/
		const float zNear = 1.0f;
		const float zFar = 4000.0f;
		pCameraController->update(deltaTime);
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, zNear, zFar);    //view matrix
		vec3        camPos = pCameraController->getViewPosition();
		mat4        vpMatrix = projMat * viewMat;

		/************************************************************************/
		// Light Update
		/************************************************************************/
		const float lightZNear = -100.0f;
		const float lightZFar = 100.0f;
		vec3 lightPos = vec3(gLightCpuSettings.mLightPosition.x, gLightCpuSettings.mLightPosition.y, gLightCpuSettings.mLightPosition.z);
		vec3 lightDir = normalize(gObjectsCenter - lightPos);
		pLightView->moveTo(lightDir * lightZNear);
		pLightView->lookAt(gObjectsCenter);
		mat4 lightViewMat = pLightView->getViewMatrix();
		mat4 lightProjMat = mat4::orthographic(-50.0f, 50.0f, -50.0f, 50.0f, 0.0f, lightZFar - lightZNear);
		mat4 lightVPMatrix = lightProjMat * lightViewMat;

		/************************************************************************/
		// Scene Update
		/************************************************************************/
		UpdateScene(deltaTime, viewMat, camPos);
		/************************************************************************/
		// Update Cameras
		/************************************************************************/
		gCameraUniformData.mViewProject = vpMatrix;
		gCameraUniformData.mViewMat = viewMat;
		gCameraUniformData.mClipInfo = vec4(zNear * zFar, zNear - zFar, zFar, 0.0f);
		gCameraUniformData.mPosition = vec4(pCameraController->getViewPosition(), 1);

		gCameraLightUniformData.mViewProject = lightVPMatrix;
		gCameraLightUniformData.mViewMat = lightViewMat;
		gCameraLightUniformData.mClipInfo = vec4(lightZNear * lightZFar, lightZNear - lightZFar, lightZFar, 0.0f);
		gCameraLightUniformData.mPosition = vec4(lightPos, 1);

		/************************************************************************/
		// Update Skybox
		/************************************************************************/
		viewMat.setTranslation(vec3(0, 0, 0));
		gSkyboxUniformData.mViewProject = projMat * viewMat;
		/************************************************************************/
		// Light Matrix Update
		/************************************************************************/
		gLightUniformData.mLightDirection = vec4(lightDir, 0);
		gLightUniformData.mLightViewProj = lightVPMatrix;
		gLightUniformData.mLightColor = vec4(1, 1, 1, 1);
		/************************************************************************/
		////////////////////////////////////////////////////////////////
		gAppUI.Update(deltaTime);
	}

	void UpdateParticleSystems(float deltaTime, mat4 viewMat, vec3 camPos)
	{
		tinystl::vector<Vertex> tempVertexBuffer(6 * MAX_NUM_PARTICLES);
		const float             particleSize = 0.2f;
		const vec3              camRight = vec3(viewMat[0][0], viewMat[1][0], viewMat[2][0]) * particleSize;
		const vec3              camUp = vec3(viewMat[0][1], viewMat[1][1], viewMat[2][1]) * particleSize;

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
				pParticleSystem->mParticleVelocities[pi] = normalize(
					vec3(sin(gCurrentTime + pi) * 0.97f, cos(gCurrentTime * gCurrentTime + pi), sin(gCurrentTime * pi)) *
					cos(gCurrentTime + deltaTime * pi));
				pParticleSystem->mParticlePositions[pi] = pParticleSystem->mParticleVelocities[pi];
				pParticleSystem->mParticleLifetimes[pi] = (sin(gCurrentTime + pi) + 1.0f) * 3.0f + 10.0f;
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
				tinystl::vector<float2> sortedArray;

				for (size_t j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
					sortedArray.push_back({ (float)distSqr(Point3(camPos), Point3(pParticleSystem->mParticlePositions[j])), (float)j });

				sortedArray.sort(MeshCompare);

				for (uint j = 0; j < sortedArray.size(); ++j)
				{
					vec3 pos = pParticleSystem->mParticlePositions[(int)sortedArray[sortedArray.size() - j - 1][1]];
					tempVertexBuffer[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 1] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 1.0f) };
					tempVertexBuffer[j * 6 + 2] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 3] = { v3ToF3(pos + camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 1.0f) };
					tempVertexBuffer[j * 6 + 4] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 1.0f) };
				}
			}
			else
			{
				for (uint j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
				{
					vec3 pos = pParticleSystem->mParticlePositions[j];
					tempVertexBuffer[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 1] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 1.0f) };
					tempVertexBuffer[j * 6 + 2] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 3] = { v3ToF3(pos + camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 1.0f) };
					tempVertexBuffer[j * 6 + 4] = { v3ToF3(pos - camUp + camRight), float3(0.0f, 1.0f, 0.0f), float2(1.0f, 0.0f) };
					tempVertexBuffer[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), float3(0.0f, 1.0f, 0.0f), float2(0.0f, 1.0f) };
				}
			}

			BufferUpdateDesc particleBufferUpdateDesc = { pParticleSystem->pParticleBuffer, tempVertexBuffer.data() };
			particleBufferUpdateDesc.mSize = sizeof(Vertex) * 6 * pParticleSystem->mLifeParticleCount;
			updateResource(&particleBufferUpdateDesc);
		}
	}

	void CreateDrawCalls(
		float* pSortedObjects, uint objectCount, uint sizeOfObject, ObjectInfoUniformBlock* pObjectUniformBlock,
		MaterialUniformBlock* pMaterialUniformBlock, uint* pMaterialCount, tinystl::vector<DrawCall>* pDrawCalls)
	{
		const uint meshIndexOffset = sizeOfObject - 2;
		const uint objectIndexOffset = sizeOfObject - 1;

		uint         instanceCount = 0;
		uint         instanceOffset = 0;
		MeshResource prevMesh = (MeshResource)0xFFFFFFFF;
		for (uint i = 0; i < objectCount; ++i)
		{
			uint          sortedObjectIndex = (objectCount - i - 1) * sizeOfObject;
			const Object* pObj = NULL;
			MeshResource  mesh = (MeshResource)(int)pSortedObjects[sortedObjectIndex + meshIndexOffset];
			int           index = (int)pSortedObjects[sortedObjectIndex + objectIndexOffset];
			if (mesh < MESH_COUNT)
				pObj = &gScene.mObjects[index];
			else
				pObj = &gScene.mParticleSystems[index].mObject;

			pObjectUniformBlock->mObjectInfo[i].mToWorldMat =
				mat4::translation(pObj->mPosition) * mat4::rotationZYX(pObj->mOrientation) * mat4::scale(pObj->mScale);
			pObjectUniformBlock->mObjectInfo[i].mNormalMat = mat4::rotationZYX(pObj->mOrientation);
			pObjectUniformBlock->mObjectInfo[i].mMaterialIndex = *pMaterialCount;
			pMaterialUniformBlock->mMaterials[*pMaterialCount] = pObj->mMaterial;
			++(*pMaterialCount);
			++instanceCount;

			if (mesh == MESH_PARTICLE_SYSTEM)
			{
				if (instanceCount > 1)
				{
					pDrawCalls->push_back({ 0, instanceCount - 1, instanceOffset, prevMesh });
					instanceOffset += instanceCount - 1;
					instanceCount = 1;
				}

				pDrawCalls->push_back({ (uint)index, instanceCount, instanceOffset, MESH_PARTICLE_SYSTEM });
				instanceOffset += instanceCount;
				instanceCount = 0;
			}
			else if (mesh != prevMesh && instanceCount > 1)
			{
				pDrawCalls->push_back({ 0, instanceCount - 1, instanceOffset, prevMesh });
				instanceOffset += instanceCount - 1;
				instanceCount = 1;
			}

			prevMesh = mesh;
		}

		if (instanceCount > 0)
			pDrawCalls->push_back({ 0, instanceCount, instanceOffset, prevMesh });
	}

	void UpdateScene(float deltaTime, mat4 viewMat, vec3 camPos)
	{
		uint materialCount = 0;

		UpdateParticleSystems(deltaTime, viewMat, camPos);

		// Create list of opaque objects
		gOpaqueDrawCalls.clear();
		uint opaqueObjectCount = 0;
		{
			tinystl::vector<float2> sortedArray = {};

			for (size_t i = 0; i < gScene.mObjects.size(); ++i)
			{
				const Object* pObj = &gScene.mObjects[i];
				if (pObj->mMaterial.mColor.getW() == 1.0f)
					sortedArray.push_back({ (float)pObj->mMesh, (float)i });
			}
			for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[i].mObject;
				if (pObj->mMaterial.mColor.getW() == 1.0f)
					sortedArray.push_back({ (float)pObj->mMesh, (float)i });
			}

			opaqueObjectCount = (int)sortedArray.size();
			ASSERT(opaqueObjectCount < MAX_NUM_OBJECTS);
			sortedArray.sort(MeshCompare);    // Sorts by mesh

			CreateDrawCalls(
				(float*)sortedArray.data(), opaqueObjectCount, sizeof(sortedArray[0]) / sizeof(float), &gObjectInfoUniformData,
				&gMaterialUniformData, &materialCount, &gOpaqueDrawCalls);
		}

		// Create list of transparent objects
		gTransparentDrawCalls.clear();
		uint transparentObjectCount = 0;
		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortObjects)
		{
			tinystl::vector<float3> sortedArray = {};

			for (size_t i = 0; i < gScene.mObjects.size(); ++i)
			{
				const Object* pObj = &gScene.mObjects[i];
				if (pObj->mMaterial.mColor.getW() < 1.0f)
					sortedArray.push_back({ (float)distSqr(Point3(camPos), Point3(pObj->mPosition)) - (float)pow(maxElem(pObj->mScale), 2),
											(float)pObj->mMesh, (float)i });
			}
			for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[i].mObject;
				if (pObj->mMaterial.mColor.getW() < 1.0f)
					sortedArray.push_back({ (float)distSqr(Point3(camPos), Point3(pObj->mPosition)) - (float)pow(maxElem(pObj->mScale), 2),
											(float)pObj->mMesh, (float)i });
			}

			transparentObjectCount = (int)sortedArray.size();
			ASSERT(transparentObjectCount < MAX_NUM_OBJECTS);
			sortedArray.sort(DistanceCompare);    // Sorts by distance first, then by mesh

			CreateDrawCalls(
				(float*)sortedArray.data(), transparentObjectCount, sizeof(sortedArray[0]) / sizeof(float),
				&gTransparentObjectInfoUniformData, &gMaterialUniformData, &materialCount, &gTransparentDrawCalls);
		}
		else
		{
			tinystl::vector<float2> sortedArray = {};

			for (size_t i = 0; i < gScene.mObjects.size(); ++i)
			{
				const Object* pObj = &gScene.mObjects[i];
				if (pObj->mMaterial.mColor.getW() < 1.0f)
					sortedArray.push_back({ (float)pObj->mMesh, (float)i });
			}
			for (size_t i = 0; i < gScene.mParticleSystems.size(); ++i)
			{
				const Object* pObj = &gScene.mParticleSystems[i].mObject;
				if (pObj->mMaterial.mColor.getW() < 1.0f)
					sortedArray.push_back({ (float)pObj->mMesh, (float)i });
			}

			transparentObjectCount = (int)sortedArray.size();
			ASSERT(transparentObjectCount < MAX_NUM_OBJECTS);
			sortedArray.sort(MeshCompare);    // Sorts by mesh

			CreateDrawCalls(
				(float*)sortedArray.data(), transparentObjectCount, sizeof(sortedArray[0]) / sizeof(float),
				&gTransparentObjectInfoUniformData, &gMaterialUniformData, &materialCount, &gTransparentDrawCalls);
		}
	}

	void DrawSkybox(Cmd* pCmd)
	{
		RenderTarget* rt = pRenderTargetScreen;
		if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
		{
			rt = pRenderTargetPTBackground;
			TextureBarrier barrier = { rt->pTexture, RESOURCE_STATE_RENDER_TARGET };
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, false);
		}

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mClearColorValues[0] = rt->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		cmdBeginDebugMarker(pCmd, 0, 0, 1, "Draw skybox");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Draw Skybox", true);

		cmdBindRenderTargets(pCmd, 1, &rt, NULL, &loadActions, NULL, NULL, -1, -1);

		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)rt->mDesc.mWidth, (float)rt->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, rt->mDesc.mWidth, rt->mDesc.mHeight);

		cmdBindPipeline(pCmd, pPipelineSkybox);

		DescriptorData params[7] = {};
		params[0].pName = "SkyboxUniformBlock";
		params[0].ppBuffers = &pBufferSkyboxUniform[gFrameIndex];
		params[1].pName = "RightText";
		params[1].ppTextures = &pTextures[TEXTURE_SKYBOX_RIGHT];
		params[2].pName = "LeftText";
		params[2].ppTextures = &pTextures[TEXTURE_SKYBOX_LEFT];
		params[3].pName = "TopText";
		params[3].ppTextures = &pTextures[TEXTURE_SKYBOX_UP];
		params[4].pName = "BotText";
		params[4].ppTextures = &pTextures[TEXTURE_SKYBOX_DOWN];
		params[5].pName = "FrontText";
		params[5].ppTextures = &pTextures[TEXTURE_SKYBOX_FRONT];
		params[6].pName = "BackText";
		params[6].ppTextures = &pTextures[TEXTURE_SKYBOX_BACK];
		cmdBindDescriptors(pCmd, pRootSignatureSkybox, 7, params);
		cmdBindVertexBuffer(pCmd, 1, &pBufferSkyboxVertex, NULL);
		cmdDraw(pCmd, 36, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

	void ShadowPass(Cmd* pCmd)
	{
#if USE_SHADOWS != 0
		TextureBarrier barriers[2] = {};
		barriers[0].pTexture = pRenderTargetShadowVariance[0]->pTexture;
		barriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
		barriers[1].pTexture = pRenderTargetShadowDepth->pTexture;
		barriers[1].mNewState = RESOURCE_STATE_DEPTH_WRITE;
		cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mClearColorValues[0] = pRenderTargetShadowVariance[0]->mDesc.mClearValue;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetShadowDepth->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetShadowVariance[0], pRenderTargetShadowDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			pCmd, 0.0f, 0.0f, (float)pRenderTargetShadowVariance[0]->mDesc.mWidth, (float)pRenderTargetShadowVariance[0]->mDesc.mHeight,
			0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetShadowVariance[0]->mDesc.mWidth, pRenderTargetShadowVariance[0]->mDesc.mHeight);

		// Draw the opaque objects.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw shadow map");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render shadow map", true);

		cmdBindPipeline(pCmd, pPipelineShadow);

		DrawObjects(
			pCmd, &gOpaqueDrawCalls, pRootSignatureShadow, pBufferOpaqueObjectTransforms[gFrameIndex],
			pBufferCameraLightUniform[gFrameIndex], false, false);
		cmdEndDebugMarker(pCmd);

		// Blur shadow map
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Blur shadow map");
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		for (int i = 0; i < 1; ++i)
		{
			cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			barriers[0].pTexture = pRenderTargetShadowVariance[0]->pTexture;
			barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			barriers[1].pTexture = pRenderTargetShadowVariance[1]->pTexture;
			barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

			cmdBindRenderTargets(pCmd, 1, &pRenderTargetShadowVariance[1], NULL, &loadActions, NULL, NULL, -1, -1);

			cmdBindPipeline(pCmd, pPipelineGaussianBlur);

			DescriptorData params[2] = {};
			float          axis = 0.0f;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &axis;
			params[1].pName = "Source";
			params[1].pRootConstant = &pRenderTargetShadowVariance[0]->pTexture;
			cmdBindDescriptors(pCmd, pRootSignatureGaussianBlur, 2, params);

			cmdDraw(pCmd, 3, 0);

			cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			barriers[0].pTexture = pRenderTargetShadowVariance[1]->pTexture;
			barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			barriers[1].pTexture = pRenderTargetShadowVariance[0]->pTexture;
			barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

			cmdBindRenderTargets(pCmd, 1, &pRenderTargetShadowVariance[0], NULL, &loadActions, NULL, NULL, -1, -1);
			cmdBindPipeline(pCmd, pPipelineGaussianBlur);

			axis = 1.0f;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &axis;
			params[1].pName = "Source";
			params[1].pRootConstant = &pRenderTargetShadowVariance[1]->pTexture;
			cmdBindDescriptors(pCmd, pRootSignatureGaussianBlur, 2, params);

			cmdDraw(pCmd, 3, 0);
		}

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		barriers[0].pTexture = pRenderTargetShadowVariance[0]->pTexture;
		barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		cmdResourceBarrier(pCmd, 0, NULL, 1, barriers, false);
#endif
	}

	void StochasticShadowPass(Cmd* pCmd)
	{
#if PT_USE_CAUSTICS != 0
		TextureBarrier barriers[3] = {};
		for (int i = 0; i < 3; ++i)
		{
			barriers[i].pTexture = pRenderTargetPTShadowVariance[i]->pTexture;
			barriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
		}
		cmdResourceBarrier(pCmd, 0, NULL, 3, barriers, false);

		LoadActionsDesc loadActions = {};
		for (int i = 0; i < 3; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetPTShadowVariance[i]->mDesc.mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		// Copy depth buffer to shadow maps
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render stochastic shadow map", true);
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Copy shadow map");

		for (int w = 0; w < 3; ++w)
		{
			cmdBindRenderTargets(pCmd, 1, &pRenderTargetPTShadowVariance[w], NULL, &loadActions, NULL, NULL, -1, -1);
			cmdBindPipeline(pCmd, pPipelinePTCopyShadowDepth);
			cmdSetViewport(
				pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowVariance[0]->mDesc.mWidth,
				(float)pRenderTargetPTShadowVariance[0]->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowVariance[0]->mDesc.mWidth, pRenderTargetPTShadowVariance[0]->mDesc.mHeight);

			DescriptorData param = {};
			param.pName = "Source";
			param.ppTextures = &pRenderTargetShadowVariance[0]->pTexture;
			cmdBindDescriptors(pCmd, pRootSignaturePTCopyShadowDepth, 1, &param);

			cmdDraw(pCmd, 3, 0);
		}
		cmdEndDebugMarker(pCmd);

		// Start render pass and apply load actions
		for (int i = 0; i < 3; ++i)
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(pCmd, 3, pRenderTargetPTShadowVariance, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowVariance[0]->mDesc.mWidth, (float)pRenderTargetPTShadowVariance[0]->mDesc.mHeight,
			0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowVariance[0]->mDesc.mWidth, pRenderTargetPTShadowVariance[0]->mDesc.mHeight);

		// Draw the opaque objects.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw stochastic shadow map");

		cmdBindPipeline(pCmd, pPipelinePTShadow);

		DrawObjects(
			pCmd, &gTransparentDrawCalls, pRootSignaturePTShadow, pBufferTransparentObjectTransforms[gFrameIndex],
			pBufferCameraLightUniform[gFrameIndex], true, false);
		cmdEndDebugMarker(pCmd);

		// Downsample shadow map
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Downsample shadow map");
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		for (int w = 0; w < 3; ++w)
		{
			cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			barriers[0].pTexture = pRenderTargetPTShadowVariance[w]->pTexture;
			barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			barriers[1].pTexture = pRenderTargetPTShadowFinal[0][w]->pTexture;
			barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

			cmdBindRenderTargets(pCmd, 1, &pRenderTargetPTShadowFinal[0][w], NULL, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(
				pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowFinal[0][w]->mDesc.mWidth,
				(float)pRenderTargetPTShadowFinal[0][w]->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowFinal[0][w]->mDesc.mWidth, pRenderTargetPTShadowFinal[0][w]->mDesc.mHeight);

			cmdBindPipeline(pCmd, pPipelinePTDownsample);

			DescriptorData param = {};
			param.pName = "Source";
			param.pRootConstant = &pRenderTargetPTShadowVariance[w]->pTexture;
			cmdBindDescriptors(pCmd, pRootSignaturePTDownsample, 1, &param);

			cmdDraw(pCmd, 3, 0);
		}
		cmdEndDebugMarker(pCmd);

		// Blur shadow map
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Blur shadow map");
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		for (int i = 0; i < 1; ++i)
		{
			for (int w = 0; w < 3; ++w)
			{
				cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

				barriers[0].pTexture = pRenderTargetPTShadowFinal[0][w]->pTexture;
				barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
				barriers[1].pTexture = pRenderTargetPTShadowFinal[1][w]->pTexture;
				barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
				cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

				cmdBindRenderTargets(pCmd, 1, &pRenderTargetPTShadowFinal[1][w], NULL, &loadActions, NULL, NULL, -1, -1);

				cmdBindPipeline(pCmd, pPipelineGaussianBlur);

				DescriptorData params[2] = {};
				float          axis = 0.0f;
				params[0].pName = "RootConstant";
				params[0].pRootConstant = &axis;
				params[1].pName = "Source";
				params[1].ppTextures = &pRenderTargetPTShadowFinal[0][w]->pTexture;
				cmdBindDescriptors(pCmd, pRootSignatureGaussianBlur, 2, params);

				cmdDraw(pCmd, 3, 0);

				cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

				barriers[0].pTexture = pRenderTargetPTShadowFinal[1][w]->pTexture;
				barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
				barriers[1].pTexture = pRenderTargetPTShadowFinal[0][w]->pTexture;
				barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
				cmdResourceBarrier(pCmd, 0, NULL, 2, barriers, false);

				cmdBindRenderTargets(pCmd, 1, &pRenderTargetPTShadowFinal[0][w], NULL, &loadActions, NULL, NULL, -1, -1);
				cmdBindPipeline(pCmd, pPipelineGaussianBlur);

				axis = 1.0f;
				params[0].pName = "RootConstant";
				params[0].pRootConstant = &axis;
				params[1].pName = "Source";
				params[1].ppTextures = &pRenderTargetPTShadowFinal[1][w]->pTexture;
				cmdBindDescriptors(pCmd, pRootSignatureGaussianBlur, 2, params);

				cmdDraw(pCmd, 3, 0);
			}
		}

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		for (int w = 0; w < 3; ++w)
		{
			barriers[0].pTexture = pRenderTargetPTShadowFinal[0][w]->pTexture;
			barriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(pCmd, 0, NULL, 1, barriers, false);
		}

		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
#endif
	}

	void DrawObjects(
		Cmd* pCmd, tinystl::vector<DrawCall>* pDrawCalls, RootSignature* pRootSignature, Buffer* pObjectTransforms, Buffer* cameraBuffer,
		bool bindMaterials = true, bool bindLights = true)
	{
		static MeshResource boundMesh = (MeshResource)0xFFFFFFFF;
		static uint         vertexCount = 0;
		static uint         indexCount = 0;

		uint           descriptorCount = 2;
		DescriptorData params[9] = {};
		params[0].pName = "ObjectUniformBlock";
		params[0].ppBuffers = &pObjectTransforms;
		params[1].pName = "CameraUniform";
		params[1].ppBuffers = &cameraBuffer;
		if (bindMaterials)
		{
			params[descriptorCount + 0].pName = "MaterialUniform";
			params[descriptorCount + 0].ppBuffers = &pBufferMaterials[gFrameIndex];
			params[descriptorCount + 1].pName = "MaterialTextures";
			params[descriptorCount + 1].ppTextures = pTextures;
			params[descriptorCount + 1].mCount = TEXTURE_COUNT;
			descriptorCount += 2;
		}
		if (bindLights)
		{
			params[descriptorCount].pName = "LightUniformBlock";
			params[descriptorCount].ppBuffers = &pBufferLightUniform[gFrameIndex];
			++descriptorCount;
#if USE_SHADOWS != 0
			params[descriptorCount].pName = "VSM";
			params[descriptorCount].ppTextures = &pRenderTargetShadowVariance[0]->pTexture;
			++descriptorCount;
#if PT_USE_CAUSTICS != 0
			params[descriptorCount].pName = "VSMRed";
			params[descriptorCount].ppTextures = &pRenderTargetPTShadowFinal[0][0]->pTexture;
			params[descriptorCount + 1].pName = "VSMGreen";
			params[descriptorCount + 1].ppTextures = &pRenderTargetPTShadowFinal[0][1]->pTexture;
			params[descriptorCount + 2].pName = "VSMBlue";
			params[descriptorCount + 2].ppTextures = &pRenderTargetPTShadowFinal[0][2]->pTexture;
			descriptorCount += 3;
#endif
#endif
		}

		cmdBindDescriptors(pCmd, pRootSignature, descriptorCount, params);

		for (size_t i = 0; i < pDrawCalls->size(); ++i)
		{
			DrawCall* dc = &(*pDrawCalls)[i];
			params[0].pName = "DrawInfoRootConstant";
			params[0].pRootConstant = &dc->mInstanceOffset;
			cmdBindDescriptors(pCmd, pRootSignature, 1, params);

			if (dc->mMesh != boundMesh || dc->mMesh > MESH_COUNT)
			{
				if (dc->mMesh == MESH_PARTICLE_SYSTEM)
				{
					cmdBindVertexBuffer(pCmd, 1, &gScene.mParticleSystems[dc->mIndex].pParticleBuffer, NULL);
					vertexCount = (uint)gScene.mParticleSystems[dc->mIndex].mLifeParticleCount * 6;
					indexCount = 0;
					boundMesh = MESH_PARTICLE_SYSTEM;
				}
				else
				{
					cmdBindVertexBuffer(pCmd, 1, &pMeshes[dc->mMesh]->pVertexBuffer, NULL);
					if (pMeshes[dc->mMesh]->pIndexBuffer)
						cmdBindIndexBuffer(pCmd, pMeshes[dc->mMesh]->pIndexBuffer, NULL);
					vertexCount = pMeshes[dc->mMesh]->mVertexCount;
					indexCount = pMeshes[dc->mMesh]->mIndexCount;
				}
			}

			if (indexCount > 0)
				cmdDrawIndexedInstanced(pCmd, indexCount, 0, dc->mInstanceCount, 0, 0);
			else
				cmdDrawInstanced(pCmd, vertexCount, 0, dc->mInstanceCount, 0);
		}
	}

	void OpaquePass(Cmd* pCmd)
	{
		RenderTarget* rt = pRenderTargetScreen;
		if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
			rt = pRenderTargetPTBackground;

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &rt, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)rt->mDesc.mWidth, (float)rt->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, rt->mDesc.mWidth, rt->mDesc.mHeight);

		// Draw the opaque objects.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw opaque geometry");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render opaque geometry", true);

		cmdBindPipeline(pCmd, pPipelineForward);

		DrawObjects(
			pCmd, &gOpaqueDrawCalls, pRootSignatureForward, pBufferOpaqueObjectTransforms[gFrameIndex], pBufferCameraUniform[gFrameIndex]);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

#if PT_USE_DIFFUSION != 0
		if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
		{
			TextureBarrier barrier = {};
			barrier.pTexture = rt->pTexture;
			barrier.mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, false);

			uint mipSizeX = 1 << (uint)ceil(log2((float)rt->mDesc.mWidth));
			uint mipSizeY = 1 << (uint)ceil(log2((float)rt->mDesc.mHeight));
			cmdBindPipeline(pCmd, pPipelinePTGenMips);
			for (uint i = 1; i < rt->mDesc.mMipLevels; ++i)
			{
				mipSizeX >>= 1;
				mipSizeY >>= 1;
				uint mipSize[2] = { mipSizeX, mipSizeY };

				DescriptorData params[3] = {};
				params[0].pName = "Source";
				params[0].ppTextures = &rt->pTexture;
				params[0].mUAVMipSlice = i - 1;
				params[1].pName = "Destination";
				params[1].ppTextures = &rt->pTexture;
				params[1].mUAVMipSlice = i;
				params[2].pName = "RootConstant";
				params[2].pRootConstant = mipSize;
				cmdBindDescriptors(pCmd, pRootSignaturePTGenMips, 3, params);

				uint groupCountX = mipSizeX / 16;
				uint groupCountY = mipSizeY / 16;
				if (groupCountX == 0)
					groupCountX = 1;
				if (groupCountY == 0)
					groupCountY = 1;
				cmdDispatch(pCmd, groupCountX, groupCountY, 1);
			}

			barrier.pTexture = rt->pTexture;
			barrier.mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(pCmd, 0, NULL, 1, &barrier, false);
		}
#endif

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

		cmdBindPipeline(pCmd, pPipelineTransparentForward);

		DrawObjects(
			pCmd, &gTransparentDrawCalls, pRootSignatureForward, pBufferTransparentObjectTransforms[gFrameIndex],
			pBufferCameraUniform[gFrameIndex]);

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);
	}

	void WeightedBlendedOrderIndependentTransparencyPass(Cmd* pCmd, bool volition)
	{
		Pipeline*      pShadePipeline = volition ? pPipelineWBOITVShade : pPipelineWBOITShade;
		Pipeline*      pCompositePipeline = volition ? pPipelineWBOITVComposite : pPipelineWBOITComposite;
		RootSignature* pShadeRootSignature = volition ? pRootSignatureWBOITVShade : pRootSignatureWBOITShade;
		RootSignature* pCompositeRootSignature = volition ? pRootSignatureWBOITVComposite : pRootSignatureWBOITComposite;

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
		cmdSetViewport(
			pCmd, 0.0f, 0.0f, (float)pRenderTargetWBOIT[0]->mDesc.mWidth, (float)pRenderTargetWBOIT[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetWBOIT[0]->mDesc.mWidth, pRenderTargetWBOIT[0]->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (WBOIT)");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry (WBOIT)", true);

		cmdBindPipeline(pCmd, pShadePipeline);

		DescriptorData shadeParam = {};
		shadeParam.pName = "WBOITSettings";
		shadeParam.ppBuffers = &pBufferWBOITSettings[gFrameIndex];
		cmdBindDescriptors(pCmd, pShadeRootSignature, 1, &shadeParam);

		DrawObjects(
			pCmd, &gTransparentDrawCalls, pShadeRootSignature, pBufferTransparentObjectTransforms[gFrameIndex],
			pBufferCameraUniform[gFrameIndex]);

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

	void PhenomenologicalTransparencyPass(Cmd* pCmd)
	{
		TextureBarrier  textureBarriers[PT_RT_COUNT + 1] = {};
		LoadActionsDesc loadActions = {};

#if PT_USE_DIFFUSION != 0
		// Copy depth buffer
		textureBarriers[0].pTexture = pRenderTargetDepth->pTexture;
		textureBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		textureBarriers[1].pTexture = pRenderTargetPTDepthCopy->pTexture;
		textureBarriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
		cmdResourceBarrier(pCmd, 0, nullptr, 2, textureBarriers, false);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mClearColorValues[0] = pRenderTargetPTDepthCopy->pTexture->mDesc.mClearValue;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetPTDepthCopy, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(
			pCmd, 0.0f, 0.0f, (float)pRenderTargetPTDepthCopy->mDesc.mWidth, (float)pRenderTargetPTDepthCopy->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetPTDepthCopy->mDesc.mWidth, pRenderTargetPTDepthCopy->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "PT Copy depth buffer");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "PT Copy depth buffer", true);

		cmdBindPipeline(pCmd, pPipelinePTCopyDepth);

		DescriptorData copyParam = {};
		copyParam.pName = "Source";
		copyParam.ppTextures = &pRenderTargetDepth->pTexture;
		cmdBindDescriptors(pCmd, pRootSignaturePTCopyDepth, 1, &copyParam);

		cmdDraw(pCmd, 3, 0);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		textureBarriers[0].pTexture = pRenderTargetDepth->pTexture;
		textureBarriers[0].mNewState = RESOURCE_STATE_DEPTH_WRITE;
		textureBarriers[1].pTexture = pRenderTargetPTDepthCopy->pTexture;
		textureBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		cmdResourceBarrier(pCmd, 0, nullptr, 2, textureBarriers, false);
#endif

		for (int i = 0; i < PT_RT_COUNT; ++i)
		{
			textureBarriers[i].pTexture = pRenderTargetPT[i]->pTexture;
			textureBarriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
		}
		cmdResourceBarrier(pCmd, 0, NULL, PT_RT_COUNT, textureBarriers, false);

		loadActions = {};
		for (int i = 0; i < PT_RT_COUNT; ++i)
		{
			loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[i] = pRenderTargetPT[i]->mDesc.mClearValue;
		}
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, PT_RT_COUNT, pRenderTargetPT, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPT[0]->mDesc.mWidth, (float)pRenderTargetPT[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetPT[0]->mDesc.mWidth, pRenderTargetPT[0]->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (PT)");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry (PT)", true);

		cmdBindPipeline(pCmd, pPipelinePTShade);

#if PT_USE_DIFFUSION != 0
		DescriptorData shadeParam = {};
		shadeParam.pName = "DepthTexture";
		shadeParam.ppTextures = &pRenderTargetPTDepthCopy->pTexture;
		cmdBindDescriptors(pCmd, pRootSignaturePTShade, 1, &shadeParam);
#endif

		DrawObjects(
			pCmd, &gTransparentDrawCalls, pRootSignaturePTShade, pBufferTransparentObjectTransforms[gFrameIndex],
			pBufferCameraUniform[gFrameIndex]);

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		// Composite PT buffers
		for (int i = 0; i < PT_RT_COUNT; ++i)
		{
			textureBarriers[i].pTexture = pRenderTargetPT[i]->pTexture;
			textureBarriers[i].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		}
		textureBarriers[PT_RT_COUNT].pTexture = pRenderTargetPTBackground->pTexture;
		textureBarriers[PT_RT_COUNT].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
		cmdResourceBarrier(pCmd, 0, nullptr, PT_RT_COUNT + 1, textureBarriers, false);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		// Start render pass and apply load actions
		cmdBindRenderTargets(pCmd, 1, &pRenderTargetScreen, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mDesc.mWidth, (float)pRenderTargetScreen->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mDesc.mWidth, pRenderTargetScreen->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite PT buffers");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Composite PT buffers", true);

		cmdBindPipeline(pCmd, pPipelinePTComposite);

		uint           compositeParamCount = 3;
		DescriptorData compositeParams[4] = {};
		compositeParams[0].pName = "AccumulationTexture";
		compositeParams[0].ppTextures = &pRenderTargetPT[PT_RT_ACCUMULATION]->pTexture;
		compositeParams[1].pName = "ModulationTexture";
		compositeParams[1].ppTextures = &pRenderTargetPT[PT_RT_MODULATION]->pTexture;
		compositeParams[2].pName = "BackgroundTexture";
		compositeParams[2].ppTextures = &pRenderTargetPTBackground->pTexture;
#if PT_USE_REFRACTION != 0
		compositeParamCount = 4;
		compositeParams[3].pName = "RefractionTexture";
		compositeParams[3].ppTextures = &pRenderTargetPT[PT_RT_REFRACTION]->pTexture;
#endif
		cmdBindDescriptors(pCmd, pRootSignaturePTComposite, compositeParamCount, compositeParams);
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
		cmdSetViewport(
			pCmd, 0.0f, 0.0f, (float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth,
			(float)pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(
			pCmd, 0, 0, pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mWidth, pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mHeight);

		// Draw the transparent geometry.
		cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (AOIT)");
		cmdBeginGpuTimestampQuery(pCmd, pGpuProfiler, "Render transparent geometry (AOIT)", true);

		cmdBindPipeline(pCmd, pPipelineAOITShade);

		int            shadeParamsCount = 2;
		DescriptorData shadeParams[3] = {};
		shadeParams[0].pName = "AOITClearMaskUAV";
		shadeParams[0].ppTextures = &pTextureAOITClearMask;
		shadeParams[1].pName = "AOITColorDataUAV";
		shadeParams[1].ppBuffers = &pBufferAOITColorData;
#if AOIT_NODE_COUNT != 2
		shadeParams[2].pName = "AOITDepthDataUAV";
		shadeParams[2].ppBuffers = &pBufferAOITDepthData;
		shadeParamsCount = 3;
#endif
		cmdBindDescriptors(pCmd, pRootSignatureAOITShade, shadeParamsCount, shadeParams);

		DrawObjects(
			pCmd, &gTransparentDrawCalls, pRootSignatureAOITShade, pBufferTransparentObjectTransforms[gFrameIndex],
			pBufferCameraUniform[gFrameIndex]);

		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(pCmd, pGpuProfiler);
		cmdEndDebugMarker(pCmd);

		// Composite AOIT buffers
		int           bufferBarrierCount = 1;
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

		gCpuTimer.GetUSec(true);
		/************************************************************************/
		// Update uniform buffers
		/************************************************************************/
		BufferUpdateDesc materialBufferUpdateDesc = { pBufferMaterials[gFrameIndex], &gMaterialUniformData };
		updateResource(&materialBufferUpdateDesc);
		BufferUpdateDesc opaqueBufferUpdateDesc = { pBufferOpaqueObjectTransforms[gFrameIndex], &gObjectInfoUniformData };
		updateResource(&opaqueBufferUpdateDesc);
		BufferUpdateDesc transparentBufferUpdateDesc = { pBufferTransparentObjectTransforms[gFrameIndex],
														 &gTransparentObjectInfoUniformData };
		updateResource(&transparentBufferUpdateDesc);

		BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex], &gCameraUniformData };
		updateResource(&cameraCbv);

		BufferUpdateDesc cameraLightBufferCbv = { pBufferCameraLightUniform[gFrameIndex], &gCameraLightUniformData };
		updateResource(&cameraLightBufferCbv);

		BufferUpdateDesc skyboxViewProjCbv = { pBufferSkyboxUniform[gFrameIndex], &gSkyboxUniformData };
		updateResource(&skyboxViewProjCbv);

		BufferUpdateDesc lightBufferCbv = { pBufferLightUniform[gFrameIndex], &gLightUniformData };
		updateResource(&lightBufferCbv);
		/************************************************************************/
		// Update transparency settings
		/************************************************************************/
		if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
		{
			BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings[gFrameIndex], &gWBOITSettingsData };
			wboitSettingsUpdateDesc.mSize = sizeof(WBOITSettings);
			updateResource(&wboitSettingsUpdateDesc);
		}
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
		{
			BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings[gFrameIndex], &gWBOITVolitionSettingsData };
			wboitSettingsUpdateDesc.mSize = sizeof(WBOITVolitionSettings);
			updateResource(&wboitSettingsUpdateDesc);
		}
		/************************************************************************/
		// Rendering
		/************************************************************************/
		// Get command list to store rendering commands for this frame
		Cmd* pCmd = ppCmds[gFrameIndex];

		pRenderTargetScreen = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		beginCmd(pCmd);
		cmdBeginGpuFrameProfile(pCmd, pGpuProfiler);
		TextureBarrier barriers1[] = {
			{ pRenderTargetScreen->pTexture, RESOURCE_STATE_RENDER_TARGET },
			{ pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE },
		};
		cmdResourceBarrier(pCmd, 0, NULL, 2, barriers1, false);

		cmdFlushBarriers(pCmd);

		DrawSkybox(pCmd);
		ShadowPass(pCmd);
		StochasticShadowPass(pCmd);
		OpaquePass(pCmd);

		if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
			AlphaBlendTransparentPass(pCmd);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
			WeightedBlendedOrderIndependentTransparencyPass(pCmd, false);
		else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
			WeightedBlendedOrderIndependentTransparencyPass(pCmd, true);
		else if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
			PhenomenologicalTransparencyPass(pCmd);
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
		drawDebugText(
			pCmd, 8.0f, 40.0f, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugText(pCmd, 8.0f, 65.0f, tinystl::string::format("Frame Time: %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);

		drawDebugGpuProfile(pCmd, 8.0f, 90.0f, pGpuProfiler, NULL);
#else
		gVirtualJoystick.Draw(pCmd, pCameraController, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		gAppUI.Gui(pGuiWindow);
		gAppUI.Draw(pCmd);
		cmdBindRenderTargets(pCmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		cmdEndDebugMarker(pCmd);
		////////////////////////////////////////////////////////

		barriers1[0] = { pRenderTargetScreen->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(pCmd, 0, NULL, 1, barriers1, true);

		cmdEndGpuFrameProfile(pCmd, pGpuProfiler);
		endCmd(pCmd);

		queueSubmit(pGraphicsQueue, 1, &pCmd, pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1, &pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() override { return "15_Transparency"; }

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

	/************************************************************************/
	// Init and Exit functions
	/************************************************************************/
	void CreateSamplers()
	{
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

#if USE_SHADOWS != 0
		SamplerDesc samplerShadowDesc = {};
		samplerShadowDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerShadowDesc.mMinFilter = FILTER_LINEAR;
		samplerShadowDesc.mMagFilter = FILTER_LINEAR;
		samplerShadowDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		addSampler(pRenderer, &samplerShadowDesc, &pSamplerShadow);
#endif
	}

	void DestroySamplers()
	{
		removeSampler(pRenderer, pSamplerTrilinearAniso);
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerPointClamp);
		removeSampler(pRenderer, pSamplerSkybox);
		removeSampler(pRenderer, pSamplerPoint);
#if USE_SHADOWS != 0
		removeSampler(pRenderer, pSamplerShadow);
#endif
	}

	void CreateRasterizerStates()
	{
		RasterizerStateDesc rasterStateDesc = {};
		rasterStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullBack);

		rasterStateDesc.mCullMode = CULL_MODE_FRONT;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullFront);

		rasterStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterStateDesc, &pRasterizerStateCullNone);
	}

	void DestroyRasterizerStates()
	{
		removeRasterizerState(pRasterizerStateCullBack);
		removeRasterizerState(pRasterizerStateCullFront);
		removeRasterizerState(pRasterizerStateCullNone);
	}

	void CreateDepthStates()
	{
		DepthStateDesc depthStateEnabledDesc = {};
		depthStateEnabledDesc.mDepthFunc = CMP_LEQUAL;
		depthStateEnabledDesc.mDepthWrite = true;
		depthStateEnabledDesc.mDepthTest = true;
		addDepthState(pRenderer, &depthStateEnabledDesc, &pDepthStateEnable);

		DepthStateDesc depthStateDisabledDesc = {};
		depthStateDisabledDesc.mDepthWrite = false;
		depthStateDisabledDesc.mDepthTest = false;
		addDepthState(pRenderer, &depthStateDisabledDesc, &pDepthStateDisable);

		DepthStateDesc depthStateNoWriteDesc = {};
		depthStateNoWriteDesc.mDepthFunc = CMP_LEQUAL;
		depthStateNoWriteDesc.mDepthWrite = false;
		depthStateNoWriteDesc.mDepthTest = true;
		addDepthState(pRenderer, &depthStateNoWriteDesc, &pDepthStateNoWrite);
	}

	void DestroyDepthStates()
	{
		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateDisable);
		removeDepthState(pDepthStateNoWrite);
	}

	void CreateBlendStates()
	{
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

		BlendStateDesc blendStatePTShadeDesc = {};
		blendStatePTShadeDesc.mSrcFactors[0] = BC_ONE;
		blendStatePTShadeDesc.mDstFactors[0] = BC_ONE;
		blendStatePTShadeDesc.mBlendModes[0] = BM_ADD;
		blendStatePTShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStatePTShadeDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStatePTShadeDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStatePTShadeDesc.mMasks[0] = ALL;
		blendStatePTShadeDesc.mSrcFactors[1] = BC_ZERO;
		blendStatePTShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
		blendStatePTShadeDesc.mBlendModes[1] = BM_ADD;
		blendStatePTShadeDesc.mSrcAlphaFactors[1] = BC_ONE;
		blendStatePTShadeDesc.mDstAlphaFactors[1] = BC_ONE;
		blendStatePTShadeDesc.mBlendAlphaModes[1] = BM_ADD;
		blendStatePTShadeDesc.mMasks[1] = ALL;
#if PT_USE_REFRACTION != 0
		blendStatePTShadeDesc.mSrcFactors[2] = BC_ONE;
		blendStatePTShadeDesc.mDstFactors[2] = BC_ONE;
		blendStatePTShadeDesc.mBlendModes[2] = BM_ADD;
		blendStatePTShadeDesc.mSrcAlphaFactors[2] = BC_ONE;
		blendStatePTShadeDesc.mDstAlphaFactors[2] = BC_ONE;
		blendStatePTShadeDesc.mBlendAlphaModes[2] = BM_ADD;
		blendStatePTShadeDesc.mMasks[2] = RED | GREEN;
		blendStatePTShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_2;
#endif
		blendStatePTShadeDesc.mRenderTargetMask |= BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
		blendStatePTShadeDesc.mIndependentBlend = true;
		addBlendState(pRenderer, &blendStatePTShadeDesc, &pBlendStatePTShade);

		BlendStateDesc blendStatePTMinDesc = {};
		blendStatePTMinDesc.mSrcFactors[0] = BC_ONE;
		blendStatePTMinDesc.mDstFactors[0] = BC_ONE;
		blendStatePTMinDesc.mBlendModes[0] = BM_MIN;
		blendStatePTMinDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStatePTMinDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStatePTMinDesc.mBlendAlphaModes[0] = BM_MIN;
		blendStatePTMinDesc.mMasks[0] = RED | GREEN;
		blendStatePTMinDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1 | BLEND_STATE_TARGET_2;
		blendStatePTMinDesc.mIndependentBlend = false;
		addBlendState(pRenderer, &blendStatePTMinDesc, &pBlendStatePTMinBlend);

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
			addBlendState(pRenderer, &blendStateAOITShadeaDesc, &pBlendStateAOITComposite);
		}
#endif
	}

	void DestroyBlendStates()
	{
		removeBlendState(pBlendStateAlphaBlend);
		removeBlendState(pBlendStateWBOITShade);
		removeBlendState(pBlendStateWBOITVolitionShade);
		removeBlendState(pBlendStatePTShade);
		removeBlendState(pBlendStatePTMinBlend);
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeBlendState(pBlendStateAOITComposite);
		}
#endif
	}

	void CreateShaders()
	{
		// Define shader macros
		ShaderMacro maxNumObjectsMacro = { "MAX_NUM_OBJECTS", tinystl::string::format("%i", MAX_NUM_OBJECTS) };
		ShaderMacro maxNumTexturesMacro = { "MAX_NUM_TEXTURES", tinystl::string::format("%i", TEXTURE_COUNT) };
		ShaderMacro aoitNodeCountMacro = { "AOIT_NODE_COUNT", tinystl::string::format("%i", AOIT_NODE_COUNT) };
		ShaderMacro useShadowsMacro = { "USE_SHADOWS", tinystl::string::format("%i", USE_SHADOWS) };
		ShaderMacro useRefractionMacro = { "PT_USE_REFRACTION", tinystl::string::format("%i", PT_USE_REFRACTION) };
		ShaderMacro useDiffusionMacro = { "PT_USE_DIFFUSION", tinystl::string::format("%i", PT_USE_DIFFUSION) };
		ShaderMacro useCausticsMacro = { "PT_USE_CAUSTICS", tinystl::string::format("%i", PT_USE_CAUSTICS) };

		ShaderMacro shaderMacros[] = { maxNumObjectsMacro, maxNumTexturesMacro, aoitNodeCountMacro, useShadowsMacro,
									   useRefractionMacro, useDiffusionMacro,   useCausticsMacro };
		const uint  numShaderMacros = sizeof(shaderMacros) / sizeof(shaderMacros[0]);

		// Skybox shader
		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

#if USE_SHADOWS != 0
		// Shadow mapping shader
		ShaderLoadDesc shadowShaderDesc = {};
		shadowShaderDesc.mStages[0] = { "shadow.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		shadowShaderDesc.mStages[1] = { "shadow.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &shadowShaderDesc, &pShaderShadow);

		// Gaussian blur shader
		ShaderLoadDesc blurShaderDesc = {};
		blurShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		blurShaderDesc.mStages[1] = { "gaussianBlur.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &blurShaderDesc, &pShaderGaussianBlur);

#if PT_USE_CAUSTICS != 0
		// Stochastic shadow mapping shader
		ShaderLoadDesc stochasticShadowShaderDesc = {};
		stochasticShadowShaderDesc.mStages[0] = { "forward.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		stochasticShadowShaderDesc.mStages[1] = { "stochasticShadow.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &stochasticShadowShaderDesc, &pShaderPTShadow);

		// Downsample shader
		ShaderLoadDesc downsampleShaderDesc = {};
		downsampleShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		downsampleShaderDesc.mStages[1] = { "downsample.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &downsampleShaderDesc, &pShaderPTDownsample);

		// Shadow map copy shader
		ShaderLoadDesc copyShadowDepthShaderDesc = {};
		copyShadowDepthShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		copyShadowDepthShaderDesc.mStages[1] = { "copy.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &copyShadowDepthShaderDesc, &pShaderPTCopyShadowDepth);
#endif
#endif

		// Forward shading shader
		ShaderLoadDesc forwardShaderDesc = {};
		forwardShaderDesc.mStages[0] = { "forward.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		forwardShaderDesc.mStages[1] = { "forward.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &forwardShaderDesc, &pShaderForward);

		// WBOIT shade shader
		ShaderLoadDesc wboitShadeShaderDesc = {};
		wboitShadeShaderDesc.mStages[0] = { "forward.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		wboitShadeShaderDesc.mStages[1] = { "weightedBlendedOIT.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &wboitShadeShaderDesc, &pShaderWBOITShade);

		// WBOIT composite shader
		ShaderLoadDesc wboitCompositeShaderDesc = {};
		wboitCompositeShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		wboitCompositeShaderDesc.mStages[1] = { "weightedBlendedOITComposite.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &wboitCompositeShaderDesc, &pShaderWBOITComposite);

		// WBOIT Volition shade shader
		ShaderLoadDesc wboitVolitionShadeShaderDesc = {};
		wboitVolitionShadeShaderDesc.mStages[0] = { "forward.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		wboitVolitionShadeShaderDesc.mStages[1] = { "weightedBlendedOITVolition.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &wboitVolitionShadeShaderDesc, &pShaderWBOITVShade);

		// WBOIT Volition composite shader
		ShaderLoadDesc wboitVolitionCompositeShaderDesc = {};
		wboitVolitionCompositeShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		wboitVolitionCompositeShaderDesc.mStages[1] = { "weightedBlendedOITVolitionComposite.frag", shaderMacros, numShaderMacros,
														FSR_SrcShaders };
		addShader(pRenderer, &wboitVolitionCompositeShaderDesc, &pShaderWBOITVComposite);

		// PT shade shader
		ShaderMacro ptShaderMacros[numShaderMacros + 1];
		for (int i = 0; i < numShaderMacros; ++i)
			ptShaderMacros[i] = shaderMacros[i];
		ptShaderMacros[numShaderMacros] = { "PHENOMENOLOGICAL_TRANSPARENCY", "" };
		ShaderLoadDesc ptShadeShaderDesc = {};
		ptShadeShaderDesc.mStages[0] = { "forward.vert", ptShaderMacros, numShaderMacros + 1, FSR_SrcShaders };
		ptShadeShaderDesc.mStages[1] = { "phenomenologicalTransparency.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &ptShadeShaderDesc, &pShaderPTShade);

		// PT composite shader
		ShaderLoadDesc ptCompositeShaderDesc = {};
		ptCompositeShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		ptCompositeShaderDesc.mStages[1] = { "phenomenologicalTransparencyComposite.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &ptCompositeShaderDesc, &pShaderPTComposite);

#if PT_USE_DIFFUSION != 0
		// PT copy depth shader
		ShaderLoadDesc ptCopyShaderDesc = {};
		ptCopyShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
		ptCopyShaderDesc.mStages[1] = { "copy.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &ptCopyShaderDesc, &pShaderPTCopyDepth);

		// PT generate mips shader
		ShaderLoadDesc ptGenMipsShaderDesc = {};
		ptGenMipsShaderDesc.mStages[0] = { "generateMips.comp", shaderMacros, numShaderMacros, FSR_SrcShaders };
		addShader(pRenderer, &ptGenMipsShaderDesc, &pShaderPTGenMips);
#endif

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			// AOIT shade shader
			ShaderLoadDesc aoitShadeShaderDesc = {};
			aoitShadeShaderDesc.mStages[0] = { "forward.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
			aoitShadeShaderDesc.mStages[1] = { "adaptiveOIT.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
			addShader(pRenderer, &aoitShadeShaderDesc, &pShaderAOITShade);

			// AOIT composite shader
			ShaderLoadDesc aoitCompositeShaderDesc = {};
			aoitCompositeShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
			aoitCompositeShaderDesc.mStages[1] = { "adaptiveOITComposite.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
			addShader(pRenderer, &aoitCompositeShaderDesc, &pShaderAOITComposite);

			// AOIT clear shader
			ShaderLoadDesc aoitClearShaderDesc = {};
			aoitClearShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, numShaderMacros, FSR_SrcShaders };
			aoitClearShaderDesc.mStages[1] = { "adaptiveOITClear.frag", shaderMacros, numShaderMacros, FSR_SrcShaders };
			addShader(pRenderer, &aoitClearShaderDesc, &pShaderAOITClear);
		}
#endif
	}

	void DestroyShaders()
	{
		removeShader(pRenderer, pShaderSkybox);
#if USE_SHADOWS != 0
		removeShader(pRenderer, pShaderShadow);
		removeShader(pRenderer, pShaderGaussianBlur);
#if PT_USE_CAUSTICS != 0
		removeShader(pRenderer, pShaderPTShadow);
		removeShader(pRenderer, pShaderPTDownsample);
		removeShader(pRenderer, pShaderPTCopyShadowDepth);
#endif
#endif
		removeShader(pRenderer, pShaderForward);
		removeShader(pRenderer, pShaderWBOITShade);
		removeShader(pRenderer, pShaderWBOITComposite);
		removeShader(pRenderer, pShaderWBOITVShade);
		removeShader(pRenderer, pShaderWBOITVComposite);
		removeShader(pRenderer, pShaderPTShade);
		removeShader(pRenderer, pShaderPTComposite);
#if PT_USE_DIFFUSION != 0
		removeShader(pRenderer, pShaderPTCopyDepth);
		removeShader(pRenderer, pShaderPTGenMips);
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
		removeShader(pRenderer, pShaderAOITShade);
		removeShader(pRenderer, pShaderAOITComposite);
		removeShader(pRenderer, pShaderAOITClear);
#endif
	}

	void CreateRootSignatures()
	{
		// Define static samplers
		const char* skyboxSamplerName = "SkySampler";
		const char* pointSamplerName = "PointSampler";
		const char* linearSamplerName = "LinearSampler";
		const char* shadowSamplerName = USE_SHADOWS ? "VSMSampler" : 0;

		Sampler*    staticSamplers[] = { pSamplerSkybox, pSamplerPoint, pSamplerBilinear, pSamplerShadow };
		const char* staticSamplerNames[] = { skyboxSamplerName, pointSamplerName, linearSamplerName, shadowSamplerName };
		const uint  numStaticSamplers = sizeof(staticSamplers) / sizeof(staticSamplers[0]);

		// Skybox root signature
		RootSignatureDesc skyboxRootSignatureDesc = {};
		skyboxRootSignatureDesc.ppShaders = &pShaderSkybox;
		skyboxRootSignatureDesc.mShaderCount = 1;
		skyboxRootSignatureDesc.ppStaticSamplers = staticSamplers;
		skyboxRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		skyboxRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		skyboxRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &skyboxRootSignatureDesc, &pRootSignatureSkybox);

#if USE_SHADOWS != 0
		// Shadow mapping root signature
		RootSignatureDesc shadowRootSignatureDesc = {};
		shadowRootSignatureDesc.ppShaders = &pShaderShadow;
		shadowRootSignatureDesc.mShaderCount = 1;
		shadowRootSignatureDesc.ppStaticSamplers = staticSamplers;
		shadowRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		shadowRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		shadowRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &shadowRootSignatureDesc, &pRootSignatureShadow);

		// Shadow mapping root signature
		RootSignatureDesc blurRootSignatureDesc = {};
		blurRootSignatureDesc.ppShaders = &pShaderGaussianBlur;
		blurRootSignatureDesc.mShaderCount = 1;
		blurRootSignatureDesc.ppStaticSamplers = staticSamplers;
		blurRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		blurRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		blurRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &blurRootSignatureDesc, &pRootSignatureGaussianBlur);

#if PT_USE_CAUSTICS != 0
		// Shadow mapping root signature
		RootSignatureDesc stochasticShadowRootSignatureDesc = {};
		stochasticShadowRootSignatureDesc.ppShaders = &pShaderPTShadow;
		stochasticShadowRootSignatureDesc.mShaderCount = 1;
		stochasticShadowRootSignatureDesc.ppStaticSamplers = staticSamplers;
		stochasticShadowRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		stochasticShadowRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		stochasticShadowRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &stochasticShadowRootSignatureDesc, &pRootSignaturePTShadow);

		// Shadow downsample root signature
		RootSignatureDesc downsampleRootSignatureDesc = {};
		downsampleRootSignatureDesc.ppShaders = &pShaderPTDownsample;
		downsampleRootSignatureDesc.mShaderCount = 1;
		downsampleRootSignatureDesc.ppStaticSamplers = staticSamplers;
		downsampleRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		downsampleRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		downsampleRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &downsampleRootSignatureDesc, &pRootSignaturePTDownsample);

		// Copy shadow root signature
		RootSignatureDesc copyShadowDepthRootSignatureDesc = {};
		copyShadowDepthRootSignatureDesc.ppShaders = &pShaderPTCopyShadowDepth;
		copyShadowDepthRootSignatureDesc.mShaderCount = 1;
		copyShadowDepthRootSignatureDesc.ppStaticSamplers = staticSamplers;
		copyShadowDepthRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		copyShadowDepthRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		copyShadowDepthRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &copyShadowDepthRootSignatureDesc, &pRootSignaturePTCopyShadowDepth);
#endif
#endif

		// Forward shading root signature
		RootSignatureDesc forwardRootSignatureDesc = {};
		forwardRootSignatureDesc.ppShaders = &pShaderForward;
		forwardRootSignatureDesc.mShaderCount = 1;
		forwardRootSignatureDesc.ppStaticSamplers = staticSamplers;
		forwardRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		forwardRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		forwardRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &forwardRootSignatureDesc, &pRootSignatureForward);

		// WBOIT shade root signature
		RootSignatureDesc wboitShadeRootSignatureDesc = {};
		wboitShadeRootSignatureDesc.ppShaders = &pShaderWBOITShade;
		wboitShadeRootSignatureDesc.mShaderCount = 1;
		wboitShadeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		wboitShadeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		wboitShadeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		wboitShadeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &wboitShadeRootSignatureDesc, &pRootSignatureWBOITShade);

		// WBOIT composite root signature
		RootSignatureDesc wboitCompositeRootSignatureDesc = {};
		wboitCompositeRootSignatureDesc.ppShaders = &pShaderWBOITComposite;
		wboitCompositeRootSignatureDesc.mShaderCount = 1;
		wboitCompositeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		wboitCompositeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		wboitCompositeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		wboitCompositeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &wboitCompositeRootSignatureDesc, &pRootSignatureWBOITComposite);

		// WBOIT Volition shade root signature
		RootSignatureDesc wboitVolitionShadeRootSignatureDesc = {};
		wboitVolitionShadeRootSignatureDesc.ppShaders = &pShaderWBOITVShade;
		wboitVolitionShadeRootSignatureDesc.mShaderCount = 1;
		wboitVolitionShadeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		wboitVolitionShadeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		wboitVolitionShadeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		wboitVolitionShadeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &wboitVolitionShadeRootSignatureDesc, &pRootSignatureWBOITVShade);

		// WBOIT Volition composite root signature
		RootSignatureDesc wboitVolitionCompositeRootSignatureDesc = {};
		wboitVolitionCompositeRootSignatureDesc.ppShaders = &pShaderWBOITVComposite;
		wboitVolitionCompositeRootSignatureDesc.mShaderCount = 1;
		wboitVolitionCompositeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		wboitVolitionCompositeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		wboitVolitionCompositeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		wboitVolitionCompositeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &wboitVolitionCompositeRootSignatureDesc, &pRootSignatureWBOITVComposite);

		// PT shade root signature
		RootSignatureDesc ptShadeRootSignatureDesc = {};
		ptShadeRootSignatureDesc.ppShaders = &pShaderPTShade;
		ptShadeRootSignatureDesc.mShaderCount = 1;
		ptShadeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		ptShadeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		ptShadeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		ptShadeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &ptShadeRootSignatureDesc, &pRootSignaturePTShade);

		// PT composite root signature
		RootSignatureDesc ptCompositeRootSignatureDesc = {};
		ptCompositeRootSignatureDesc.ppShaders = &pShaderPTComposite;
		ptCompositeRootSignatureDesc.mShaderCount = 1;
		ptCompositeRootSignatureDesc.ppStaticSamplers = staticSamplers;
		ptCompositeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		ptCompositeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		ptCompositeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &ptCompositeRootSignatureDesc, &pRootSignaturePTComposite);

#if PT_USE_DIFFUSION != 0
		// PT copy depth root signature
		RootSignatureDesc ptCopyDepthRootSignatureDesc = {};
		ptCopyDepthRootSignatureDesc.ppShaders = &pShaderPTCopyDepth;
		ptCopyDepthRootSignatureDesc.mShaderCount = 1;
		ptCopyDepthRootSignatureDesc.ppStaticSamplers = staticSamplers;
		ptCopyDepthRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		ptCopyDepthRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		ptCopyDepthRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &ptCopyDepthRootSignatureDesc, &pRootSignaturePTCopyDepth);

		// PT generate mips root signature
		RootSignatureDesc ptGenMipsRootSignatureDesc = {};
		ptGenMipsRootSignatureDesc.ppShaders = &pShaderPTGenMips;
		ptGenMipsRootSignatureDesc.mShaderCount = 1;
		ptGenMipsRootSignatureDesc.ppStaticSamplers = staticSamplers;
		ptGenMipsRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		ptGenMipsRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
		ptGenMipsRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
		addRootSignature(pRenderer, &ptGenMipsRootSignatureDesc, &pRootSignaturePTGenMips);
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			// AOIT shade root signature
			RootSignatureDesc aoitShadeRootSignatureDesc = {};
			aoitShadeRootSignatureDesc.ppShaders = &pShaderAOITShade;
			aoitShadeRootSignatureDesc.mShaderCount = 1;
			aoitShadeRootSignatureDesc.ppStaticSamplers = staticSamplers;
			aoitShadeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
			aoitShadeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
			aoitShadeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
			addRootSignature(pRenderer, &aoitShadeRootSignatureDesc, &pRootSignatureAOITShade);

			// AOIT composite root signature
			RootSignatureDesc aoitCompositeRootSignatureDesc = {};
			aoitCompositeRootSignatureDesc.ppShaders = &pShaderAOITComposite;
			aoitCompositeRootSignatureDesc.mShaderCount = 1;
			aoitCompositeRootSignatureDesc.ppStaticSamplers = staticSamplers;
			aoitCompositeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
			aoitCompositeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
			aoitCompositeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
			addRootSignature(pRenderer, &aoitCompositeRootSignatureDesc, &pRootSignatureAOITComposite);

			// AOIT clear signature
			RootSignatureDesc aoitClearRootSignatureDesc = {};
			aoitClearRootSignatureDesc.ppShaders = &pShaderAOITClear;
			aoitClearRootSignatureDesc.mShaderCount = 1;
			aoitClearRootSignatureDesc.ppStaticSamplers = staticSamplers;
			aoitClearRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
			aoitClearRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
			aoitClearRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
			addRootSignature(pRenderer, &aoitClearRootSignatureDesc, &pRootSignatureAOITClear);
		}
#endif
	}

	void DestroyRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureSkybox);
#if USE_SHADOWS != 0
		removeRootSignature(pRenderer, pRootSignatureShadow);
		removeRootSignature(pRenderer, pRootSignatureGaussianBlur);
#if PT_USE_CAUSTICS != 0
		removeRootSignature(pRenderer, pRootSignaturePTShadow);
		removeRootSignature(pRenderer, pRootSignaturePTDownsample);
		removeRootSignature(pRenderer, pRootSignaturePTCopyShadowDepth);
#endif
#endif
		removeRootSignature(pRenderer, pRootSignatureForward);
		removeRootSignature(pRenderer, pRootSignatureWBOITShade);
		removeRootSignature(pRenderer, pRootSignatureWBOITComposite);
		removeRootSignature(pRenderer, pRootSignatureWBOITVShade);
		removeRootSignature(pRenderer, pRootSignatureWBOITVComposite);
		removeRootSignature(pRenderer, pRootSignaturePTShade);
		removeRootSignature(pRenderer, pRootSignaturePTComposite);
#if PT_USE_DIFFUSION != 0
		removeRootSignature(pRenderer, pRootSignaturePTCopyDepth);
		removeRootSignature(pRenderer, pRootSignaturePTGenMips);
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeRootSignature(pRenderer, pRootSignatureAOITShade);
			removeRootSignature(pRenderer, pRootSignatureAOITComposite);
			removeRootSignature(pRenderer, pRootSignatureAOITClear);
		}
#endif
	}

	void CreateResources()
	{
		LoadModels();
		LoadTextures();

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

		uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
		BufferLoadDesc skyboxVbDesc = {};
		skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = gSkyboxPointArray;
		skyboxVbDesc.ppBuffer = &pBufferSkyboxVertex;
		addResource(&skyboxVbDesc);

#if USE_SHADOWS != 0
		const uint shadowMapResolution = 1024;

		RenderTargetDesc renderTargetDesc = {};
		renderTargetDesc.mArraySize = 1;
		renderTargetDesc.mClearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
		renderTargetDesc.mDepth = 1;
		renderTargetDesc.mFormat = ImageFormat::RG16;
		renderTargetDesc.mWidth = shadowMapResolution;
		renderTargetDesc.mHeight = shadowMapResolution;
		renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		renderTargetDesc.mSampleQuality = 0;
		renderTargetDesc.pDebugName = L"Shadow variance RT";
		for (int i = 0; i < 2; ++i)
			addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetShadowVariance[i]);

		RenderTargetDesc shadowRT = {};
		shadowRT.mArraySize = 1;
		shadowRT.mClearValue = { 1.0f, 0.0f };
		shadowRT.mDepth = 1;
		shadowRT.mFormat = ImageFormat::D16;
		shadowRT.mWidth = shadowMapResolution;
		shadowRT.mHeight = shadowMapResolution;
		shadowRT.mSampleCount = SAMPLE_COUNT_1;
		shadowRT.mSampleQuality = 0;
		shadowRT.pDebugName = L"Shadow depth RT";
		addRenderTarget(pRenderer, &shadowRT, &pRenderTargetShadowDepth);

#if PT_USE_CAUSTICS != 0
		const uint ptShadowMapResolution = 4096;
		renderTargetDesc = {};
		renderTargetDesc.mArraySize = 1;
		renderTargetDesc.mClearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
		renderTargetDesc.mDepth = 1;
		renderTargetDesc.mFormat = ImageFormat::RG16;
		renderTargetDesc.mWidth = ptShadowMapResolution;
		renderTargetDesc.mHeight = ptShadowMapResolution;
		renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		renderTargetDesc.mSampleQuality = 0;
		renderTargetDesc.pDebugName = L"PT shadow variance RT";
		for (int w = 0; w < 3; ++w)
			addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTShadowVariance[w]);

		renderTargetDesc = {};
		renderTargetDesc.mArraySize = 1;
		renderTargetDesc.mClearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
		renderTargetDesc.mDepth = 1;
		renderTargetDesc.mFormat = ImageFormat::RG16;
		renderTargetDesc.mWidth = ptShadowMapResolution / 4;
		renderTargetDesc.mHeight = ptShadowMapResolution / 4;
		renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		renderTargetDesc.mSampleQuality = 0;
		renderTargetDesc.pDebugName = L"PT shadow final RT";
		for (int w = 0; w < 3; ++w)
		{
			for (int i = 0; i < 2; ++i)
				addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTShadowFinal[i][w]);
		}
#endif
#endif
	}

	void DestroyResources()
	{
		removeResource(pBufferSkyboxVertex);
#if USE_SHADOWS != 0
		for (int i = 0; i < 2; ++i)
			removeRenderTarget(pRenderer, pRenderTargetShadowVariance[i]);
		removeRenderTarget(pRenderer, pRenderTargetShadowDepth);
#if PT_USE_CAUSTICS != 0
		for (int w = 0; w < 3; ++w)
		{
			removeRenderTarget(pRenderer, pRenderTargetPTShadowVariance[w]);
			for (int i = 0; i < 2; ++i)
				removeRenderTarget(pRenderer, pRenderTargetPTShadowFinal[i][w]);
		}
#endif
#endif

		DestroyTextures();
		DestroyModels();
	}

	void LoadModels()
	{
		AssimpImporter          importer;
		tinystl::vector<Vertex> vertices = {};
		tinystl::vector<uint>   indices = {};

		const char* modelNames[MESH_COUNT] = { "cube.obj", "sphere.obj", "plane.obj", "lion.obj" };

		for (int m = 0; m < MESH_COUNT; ++m)
		{
			AssimpImporter::Model model;
			if (importer.ImportModel(FileSystem::FixPath(modelNames[m], FSR_Meshes).c_str(), &model))
			{
				vertices.clear();
				indices.clear();

				for (size_t i = 0; i < model.mMeshArray.size(); ++i)
				{
					AssimpImporter::Mesh* mesh = &model.mMeshArray[i];
					vertices.reserve(vertices.size() + mesh->mPositions.size());
					indices.reserve(indices.size() + mesh->mIndices.size());

					for (size_t v = 0; v < mesh->mPositions.size(); ++v)
					{
						Vertex vertex = { float3(0.0f), float3(0.0f, 1.0f, 0.0f) };
						vertex.mPosition = mesh->mPositions[v];
						vertex.mNormal = mesh->mNormals[v];
						vertex.mUV = mesh->mUvs[v];
						vertices.push_back(vertex);
					}

					for (size_t j = 0; j < mesh->mIndices.size(); ++j)
						indices.push_back(mesh->mIndices[j]);
				}

				MeshData* meshData = conf_placement_new<MeshData>(conf_malloc(sizeof(MeshData)));
				meshData->mVertexCount = (uint)vertices.size();
				meshData->mIndexCount = (uint)indices.size();

				BufferLoadDesc vertexBufferDesc = {};
				vertexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
				vertexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
				vertexBufferDesc.mDesc.mSize = sizeof(Vertex) * meshData->mVertexCount;
				vertexBufferDesc.mDesc.mVertexStride = sizeof(Vertex);
				vertexBufferDesc.pData = vertices.data();
				vertexBufferDesc.ppBuffer = &meshData->pVertexBuffer;
				addResource(&vertexBufferDesc);

				if (meshData->mIndexCount > 0)
				{
					BufferLoadDesc indexBufferDesc = {};
					indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
					indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
					indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
					indexBufferDesc.mDesc.mSize = sizeof(uint) * meshData->mIndexCount;
					indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
					indexBufferDesc.pData = indices.data();
					indexBufferDesc.ppBuffer = &meshData->pIndexBuffer;
					addResource(&indexBufferDesc);
				}

				pMeshes[m] = meshData;
			}
			else
				ErrorMsg("Failed to load model.");
		}
	}

	void DestroyModels()
	{
		for (int i = 0; i < MESH_COUNT; ++i)
		{
			removeResource(pMeshes[i]->pVertexBuffer);
			if (pMeshes[i]->pIndexBuffer)
				removeResource(pMeshes[i]->pIndexBuffer);
			conf_free(pMeshes[i]);
		}
	}

	void LoadTextures()
	{
		const char* textureNames[TEXTURE_COUNT] = {
			"skybox/hw_sahara/sahara_rt.tga",
			"skybox/hw_sahara/sahara_lf.tga",
			"skybox/hw_sahara/sahara_up.tga",
			"skybox/hw_sahara/sahara_dn.tga",
			"skybox/hw_sahara/sahara_ft.tga",
			"skybox/hw_sahara/sahara_bk.tga",
			"grid.jpg",
		};

		for (int i = 0; i < TEXTURE_COUNT; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_Textures;
			textureDesc.mUseMipmaps = true;
			textureDesc.pFilename = textureNames[i];
			textureDesc.ppTexture = &pTextures[i];
			addResource(&textureDesc, true);
		}
	}

	void DestroyTextures()
	{
		for (uint i = 0; i < TEXTURE_COUNT; ++i)
			removeResource(pTextures[i]);
	}

	void CreateUniformBuffers()
	{
		BufferLoadDesc materialUBDesc = {};
		materialUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		materialUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		materialUBDesc.mDesc.mSize = sizeof(MaterialUniformBlock);
		materialUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		materialUBDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			materialUBDesc.ppBuffer = &pBufferMaterials[i];
			addResource(&materialUBDesc);
		}

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(ObjectInfoUniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pBufferOpaqueObjectTransforms[i];
			addResource(&ubDesc);
		}
		for (int i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pBufferTransparentObjectTransforms[i];
			addResource(&ubDesc);
		}

		BufferLoadDesc skyboxDesc = {};
		skyboxDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		skyboxDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		skyboxDesc.mDesc.mSize = sizeof(SkyboxUniformBlock);
		skyboxDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		skyboxDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
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
		for (int i = 0; i < gImageCount; ++i)
		{
			camUniDesc.ppBuffer = &pBufferCameraUniform[i];
			addResource(&camUniDesc);
		}

		BufferLoadDesc camLightUniDesc = {};
		camLightUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		camLightUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		camLightUniDesc.mDesc.mSize = sizeof(CameraUniform);
		camLightUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		camLightUniDesc.pData = &gCameraLightUniformData;
		for (int i = 0; i < gImageCount; ++i)
		{
			camLightUniDesc.ppBuffer = &pBufferCameraLightUniform[i];
			addResource(&camLightUniDesc);
		}

		BufferLoadDesc lightUniformDesc = {};
		lightUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightUniformDesc.mDesc.mSize = sizeof(LightUniformBlock);
		lightUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightUniformDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			lightUniformDesc.ppBuffer = &pBufferLightUniform[i];
			addResource(&lightUniformDesc);
		}
		BufferLoadDesc wboitSettingsDesc = {};
		wboitSettingsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		wboitSettingsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		wboitSettingsDesc.mDesc.mSize = max(sizeof(WBOITSettings), sizeof(WBOITVolitionSettings));
		wboitSettingsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		wboitSettingsDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			wboitSettingsDesc.ppBuffer = &pBufferWBOITSettings[i];
			addResource(&wboitSettingsDesc);
		}
	}

	void DestroyUniformBuffers()
	{
		for (int i = 0; i < gImageCount; ++i)
		{
			removeResource(pBufferMaterials[i]);
			removeResource(pBufferOpaqueObjectTransforms[i]);
			removeResource(pBufferTransparentObjectTransforms[i]);
			removeResource(pBufferLightUniform[i]);
			removeResource(pBufferSkyboxUniform[i]);
			removeResource(pBufferCameraUniform[i]);
			removeResource(pBufferCameraLightUniform[i]);
			removeResource(pBufferWBOITSettings[i]);
		}
	}

	/************************************************************************/
	// Load and Unload functions
	/************************************************************************/
	bool CreateRenderTargetsAndSwapChain() const
	{
		const uint32_t width = mSettings.mWidth;
		const uint32_t height = mSettings.mHeight;

		const ClearValue depthClear = { 1.0f, 0 };
		const ClearValue colorClearBlack = { 0.0f, 0.0f, 0.0f, 0.0f };
		const ClearValue colorClearWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
		const ClearValue colorClearTransparentWhite = { 1.0f, 1.0f, 1.0f, 0.0f };

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
		depthRT.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);
#if PT_USE_DIFFUSION != 0
		depthRT.mFormat = ImageFormat::R32F;
		depthRT.pDebugName = L"Depth RT PT";
		addRenderTarget(pRenderer, &depthRT, &pRenderTargetPTDepthCopy);
#endif
		/************************************************************************/
		// Swapchain
		/************************************************************************/
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
			swapChainDesc.mColorClearValue = { 1, 0, 1, 1 };
			swapChainDesc.mSrgb = false;

			swapChainDesc.mEnableVsync = false;
			::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

			if (pSwapChain == NULL)
				return false;
		}

		/************************************************************************/
		// WBOIT render targets
		/************************************************************************/
		ClearValue     wboitClearValues[] = { colorClearBlack, colorClearWhite };
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

		/************************************************************************/
		// PT render targets
		/************************************************************************/
		ClearValue     ptClearValues[] = { colorClearBlack, colorClearTransparentWhite, colorClearBlack };
		const wchar_t* ptNames[] = { L"Accumulation RT", L"Modulation RT", L"Refraction RT" };
		for (int i = 0; i < PT_RT_COUNT; ++i)
		{
			if (i == PT_RT_ACCUMULATION)
			{
				// PT shares the accumulation buffer with WBOIT
				pRenderTargetPT[PT_RT_ACCUMULATION] = pRenderTargetWBOIT[WBOIT_RT_ACCUMULATION];
				continue;
			}
			RenderTargetDesc renderTargetDesc = {};
			renderTargetDesc.mArraySize = 1;
			renderTargetDesc.mClearValue = ptClearValues[i];
			renderTargetDesc.mDepth = 1;
			renderTargetDesc.mFormat = gPTRenderTargetFormats[i];
			renderTargetDesc.mWidth = width;
			renderTargetDesc.mHeight = height;
			renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
			renderTargetDesc.mSampleQuality = 0;
			renderTargetDesc.pDebugName = ptNames[i];
			addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPT[i]);
		}

		{
			RenderTargetDesc renderTargetDesc = {};
			renderTargetDesc.mArraySize = 1;
			renderTargetDesc.mClearValue = pSwapChain->mDesc.mColorClearValue;
			renderTargetDesc.mDepth = 1;
			renderTargetDesc.mFormat = pSwapChain->mDesc.mColorFormat;
			renderTargetDesc.mWidth = width;
			renderTargetDesc.mHeight = height;
			renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
			renderTargetDesc.mSampleQuality = 0;
			renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE;
			renderTargetDesc.mMipLevels = (uint)log2(width);
			renderTargetDesc.pDebugName = L"PT Background RT";
			renderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
			addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTBackground);
		}

#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
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

	void DestroyRenderTargetsAndSwapChian()
	{
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			removeResource(pTextureAOITClearMask);
#if AOIT_NODE_COUNT != 2
			removeResource(pBufferAOITDepthData);
#endif
			removeResource(pBufferAOITColorData);
		}
#endif

		removeRenderTarget(pRenderer, pRenderTargetDepth);
#if PT_USE_DIFFUSION != 0
		removeRenderTarget(pRenderer, pRenderTargetPTDepthCopy);
#endif
		for (int i = 0; i < WBOIT_RT_COUNT; ++i)
			removeRenderTarget(pRenderer, pRenderTargetWBOIT[i]);
		for (int i = 0; i < PT_RT_COUNT; ++i)
		{
			if (i == PT_RT_ACCUMULATION)
				continue;    // Acculuation RT is shared with WBOIT and has already been removed
			removeRenderTarget(pRenderer, pRenderTargetPT[i]);
		}
		removeRenderTarget(pRenderer, pRenderTargetPTBackground);
		removeSwapChain(pRenderer, pSwapChain);
	}

	void CreatePipelines()
	{
		// Define vertex layouts
		VertexLayout vertexLayoutSkybox = {};
		vertexLayoutSkybox.mAttribCount = 1;
		vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutSkybox.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		vertexLayoutSkybox.mAttribs[0].mBinding = 0;
		vertexLayoutSkybox.mAttribs[0].mLocation = 0;
		vertexLayoutSkybox.mAttribs[0].mOffset = 0;

		VertexLayout vertexLayoutDefault = {};
		vertexLayoutDefault.mAttribCount = 3;
		vertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		vertexLayoutDefault.mAttribs[0].mFormat = ImageFormat::RGB32F;
		vertexLayoutDefault.mAttribs[0].mBinding = 0;
		vertexLayoutDefault.mAttribs[0].mLocation = 0;
		vertexLayoutDefault.mAttribs[0].mOffset = 0;
		vertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		vertexLayoutDefault.mAttribs[1].mFormat = ImageFormat::RGB32F;
		vertexLayoutDefault.mAttribs[1].mBinding = 0;
		vertexLayoutDefault.mAttribs[1].mLocation = 1;
		vertexLayoutDefault.mAttribs[1].mOffset = 3 * sizeof(float);
		vertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		vertexLayoutDefault.mAttribs[2].mFormat = ImageFormat::RG32F;
		vertexLayoutDefault.mAttribs[2].mBinding = 0;
		vertexLayoutDefault.mAttribs[2].mLocation = 2;
		vertexLayoutDefault.mAttribs[2].mOffset = 6 * sizeof(float);

		bool srgbEnabled[] = { true, true, true, true, true, true, true, true };
		bool srgbDisabled[] = { false, false, false, false, false, false, false, false };

		// Skybox pipeline
		GraphicsPipelineDesc skyboxPipelineDesc = {};
		skyboxPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		skyboxPipelineDesc.pShaderProgram = pShaderSkybox;
		skyboxPipelineDesc.pRootSignature = pRootSignatureSkybox;
		skyboxPipelineDesc.mRenderTargetCount = 1;
		skyboxPipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		skyboxPipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		skyboxPipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		skyboxPipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		skyboxPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		skyboxPipelineDesc.pVertexLayout = &vertexLayoutSkybox;
		skyboxPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		skyboxPipelineDesc.pDepthState = pDepthStateDisable;
		skyboxPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &skyboxPipelineDesc, &pPipelineSkybox);

#if USE_SHADOWS != 0
		// Shadow pipeline
		GraphicsPipelineDesc shadowPipelineDesc = {};
		shadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		shadowPipelineDesc.pShaderProgram = pShaderShadow;
		shadowPipelineDesc.pRootSignature = pRootSignatureShadow;
		shadowPipelineDesc.mRenderTargetCount = 1;
		shadowPipelineDesc.pColorFormats = &pRenderTargetShadowVariance[0]->mDesc.mFormat;
		shadowPipelineDesc.pSrgbValues = &pRenderTargetShadowVariance[0]->mDesc.mSrgb;
		shadowPipelineDesc.mSampleCount = pRenderTargetShadowVariance[0]->mDesc.mSampleCount;
		shadowPipelineDesc.mSampleQuality = pRenderTargetShadowVariance[0]->mDesc.mSampleQuality;
		shadowPipelineDesc.mDepthStencilFormat = ImageFormat::D16;
		shadowPipelineDesc.pVertexLayout = &vertexLayoutDefault;
		shadowPipelineDesc.pRasterizerState = pRasterizerStateCullFront;
		shadowPipelineDesc.pDepthState = pDepthStateEnable;
		shadowPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &shadowPipelineDesc, &pPipelineShadow);

		// Gaussian blur pipeline
		GraphicsPipelineDesc blurPipelineDesc = {};
		blurPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		blurPipelineDesc.pShaderProgram = pShaderGaussianBlur;
		blurPipelineDesc.pRootSignature = pRootSignatureGaussianBlur;
		blurPipelineDesc.mRenderTargetCount = 1;
		blurPipelineDesc.pColorFormats = &pRenderTargetShadowVariance[0]->mDesc.mFormat;
		blurPipelineDesc.pSrgbValues = &pRenderTargetShadowVariance[0]->mDesc.mSrgb;
		blurPipelineDesc.mSampleCount = pRenderTargetShadowVariance[0]->mDesc.mSampleCount;
		blurPipelineDesc.mSampleQuality = pRenderTargetShadowVariance[0]->mDesc.mSampleQuality;
		blurPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		blurPipelineDesc.pVertexLayout = NULL;
		blurPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		blurPipelineDesc.pDepthState = pDepthStateDisable;
		blurPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &blurPipelineDesc, &pPipelineGaussianBlur);

#if PT_USE_CAUSTICS != 0
		ImageFormat::Enum stochasticShadowColorFormats[] = { pRenderTargetPTShadowVariance[0]->mDesc.mFormat,
															 pRenderTargetPTShadowVariance[1]->mDesc.mFormat,
															 pRenderTargetPTShadowVariance[2]->mDesc.mFormat };

		// Stochastic shadow pipeline
		GraphicsPipelineDesc stochasticShadowPipelineDesc = {};
		stochasticShadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		stochasticShadowPipelineDesc.pShaderProgram = pShaderPTShadow;
		stochasticShadowPipelineDesc.pRootSignature = pRootSignaturePTShadow;
		stochasticShadowPipelineDesc.mRenderTargetCount = 3;
		stochasticShadowPipelineDesc.pColorFormats = stochasticShadowColorFormats;
		stochasticShadowPipelineDesc.pSrgbValues = srgbDisabled;
		stochasticShadowPipelineDesc.mSampleCount = pRenderTargetPTShadowVariance[0]->mDesc.mSampleCount;
		stochasticShadowPipelineDesc.mSampleQuality = pRenderTargetPTShadowVariance[0]->mDesc.mSampleQuality;
		stochasticShadowPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		stochasticShadowPipelineDesc.pVertexLayout = &vertexLayoutDefault;
		stochasticShadowPipelineDesc.pRasterizerState = pRasterizerStateCullFront;
		stochasticShadowPipelineDesc.pDepthState = pDepthStateDisable;
		stochasticShadowPipelineDesc.pBlendState = pBlendStatePTMinBlend;
		addPipeline(pRenderer, &stochasticShadowPipelineDesc, &pPipelinePTShadow);

		// Downsample shadow pipeline
		GraphicsPipelineDesc downsampleShadowPipelineDesc = {};
		downsampleShadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		downsampleShadowPipelineDesc.pShaderProgram = pShaderPTDownsample;
		downsampleShadowPipelineDesc.pRootSignature = pRootSignaturePTDownsample;
		downsampleShadowPipelineDesc.mRenderTargetCount = 1;
		downsampleShadowPipelineDesc.pColorFormats = &pRenderTargetPTShadowFinal[0][0]->mDesc.mFormat;
		downsampleShadowPipelineDesc.pSrgbValues = &pRenderTargetPTShadowFinal[0][0]->mDesc.mSrgb;
		downsampleShadowPipelineDesc.mSampleCount = pRenderTargetPTShadowFinal[0][0]->mDesc.mSampleCount;
		downsampleShadowPipelineDesc.mSampleQuality = pRenderTargetPTShadowFinal[0][0]->mDesc.mSampleQuality;
		downsampleShadowPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		downsampleShadowPipelineDesc.pVertexLayout = NULL;
		downsampleShadowPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		downsampleShadowPipelineDesc.pDepthState = pDepthStateDisable;
		downsampleShadowPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &downsampleShadowPipelineDesc, &pPipelinePTDownsample);

		// Copy shadow map pipeline
		GraphicsPipelineDesc copyShadowDepthPipelineDesc = {};
		copyShadowDepthPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		copyShadowDepthPipelineDesc.pShaderProgram = pShaderPTCopyShadowDepth;
		copyShadowDepthPipelineDesc.pRootSignature = pRootSignaturePTCopyShadowDepth;
		copyShadowDepthPipelineDesc.mRenderTargetCount = 1;
		copyShadowDepthPipelineDesc.pColorFormats = &pRenderTargetPTShadowVariance[0]->mDesc.mFormat;
		copyShadowDepthPipelineDesc.pSrgbValues = &pRenderTargetPTShadowVariance[0]->mDesc.mSrgb;
		copyShadowDepthPipelineDesc.mSampleCount = pRenderTargetPTShadowVariance[0]->mDesc.mSampleCount;
		copyShadowDepthPipelineDesc.mSampleQuality = pRenderTargetPTShadowVariance[0]->mDesc.mSampleQuality;
		copyShadowDepthPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		copyShadowDepthPipelineDesc.pVertexLayout = NULL;
		copyShadowDepthPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		copyShadowDepthPipelineDesc.pDepthState = pDepthStateDisable;
		copyShadowDepthPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &copyShadowDepthPipelineDesc, &pPipelinePTCopyShadowDepth);

#endif
#endif

		// Forward shading pipeline
		GraphicsPipelineDesc forwardPipelineDesc = {};
		forwardPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		forwardPipelineDesc.pShaderProgram = pShaderForward;
		forwardPipelineDesc.pRootSignature = pRootSignatureForward;
		forwardPipelineDesc.mRenderTargetCount = 1;
		forwardPipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		forwardPipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		forwardPipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		forwardPipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		forwardPipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
		forwardPipelineDesc.pVertexLayout = &vertexLayoutDefault;
		forwardPipelineDesc.pRasterizerState = pRasterizerStateCullFront;
		forwardPipelineDesc.pDepthState = pDepthStateEnable;
		forwardPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &forwardPipelineDesc, &pPipelineForward);

		// Transparent forward shading pipeline
		GraphicsPipelineDesc transparentForwardPipelineDesc = {};
		transparentForwardPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		transparentForwardPipelineDesc.pShaderProgram = pShaderForward;
		transparentForwardPipelineDesc.pRootSignature = pRootSignatureForward;
		transparentForwardPipelineDesc.mRenderTargetCount = 1;
		transparentForwardPipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		transparentForwardPipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		transparentForwardPipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		transparentForwardPipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		transparentForwardPipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
		transparentForwardPipelineDesc.pVertexLayout = &vertexLayoutDefault;
		transparentForwardPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		transparentForwardPipelineDesc.pDepthState = pDepthStateNoWrite;
		transparentForwardPipelineDesc.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &transparentForwardPipelineDesc, &pPipelineTransparentForward);

		// WBOIT shading pipeline
		GraphicsPipelineDesc wboitShadePipelineDesc = {};
		wboitShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitShadePipelineDesc.pShaderProgram = pShaderWBOITShade;
		wboitShadePipelineDesc.pRootSignature = pRootSignatureWBOITShade;
		wboitShadePipelineDesc.mRenderTargetCount = WBOIT_RT_COUNT;
		wboitShadePipelineDesc.pColorFormats = gWBOITRenderTargetFormats;
		wboitShadePipelineDesc.pSrgbValues = srgbDisabled;
		wboitShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
		wboitShadePipelineDesc.mSampleQuality = 0;
		wboitShadePipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
		wboitShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
		wboitShadePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		wboitShadePipelineDesc.pDepthState = pDepthStateNoWrite;
		wboitShadePipelineDesc.pBlendState = pBlendStateWBOITShade;
		addPipeline(pRenderer, &wboitShadePipelineDesc, &pPipelineWBOITShade);

		// WBOIT composite pipeline
		GraphicsPipelineDesc wboitCompositePipelineDesc = {};
		wboitCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitCompositePipelineDesc.pShaderProgram = pShaderWBOITComposite;
		wboitCompositePipelineDesc.pRootSignature = pRootSignatureWBOITComposite;
		wboitCompositePipelineDesc.mRenderTargetCount = 1;
		wboitCompositePipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		wboitCompositePipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		wboitCompositePipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		wboitCompositePipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		wboitCompositePipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		wboitCompositePipelineDesc.pVertexLayout = NULL;
		wboitCompositePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		wboitCompositePipelineDesc.pDepthState = pDepthStateDisable;
		wboitCompositePipelineDesc.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &wboitCompositePipelineDesc, &pPipelineWBOITComposite);

		// WBOIT Volition shading pipeline
		GraphicsPipelineDesc wboitVolitionShadePipelineDesc = {};
		wboitVolitionShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitVolitionShadePipelineDesc.pShaderProgram = pShaderWBOITVShade;
		wboitVolitionShadePipelineDesc.pRootSignature = pRootSignatureWBOITVShade;
		wboitVolitionShadePipelineDesc.mRenderTargetCount = WBOIT_RT_COUNT;
		wboitVolitionShadePipelineDesc.pColorFormats = gWBOITRenderTargetFormats;
		wboitVolitionShadePipelineDesc.pSrgbValues = srgbDisabled;
		wboitVolitionShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
		wboitVolitionShadePipelineDesc.mSampleQuality = 0;
		wboitVolitionShadePipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
		wboitVolitionShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
		wboitVolitionShadePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		wboitVolitionShadePipelineDesc.pDepthState = pDepthStateNoWrite;
		wboitVolitionShadePipelineDesc.pBlendState = pBlendStateWBOITVolitionShade;
		addPipeline(pRenderer, &wboitVolitionShadePipelineDesc, &pPipelineWBOITVShade);

		// WBOIT Volition composite pipeline
		GraphicsPipelineDesc wboitVolitionCompositePipelineDesc = {};
		wboitVolitionCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		wboitVolitionCompositePipelineDesc.pShaderProgram = pShaderWBOITVComposite;
		wboitVolitionCompositePipelineDesc.pRootSignature = pRootSignatureWBOITVComposite;
		wboitVolitionCompositePipelineDesc.mRenderTargetCount = 1;
		wboitVolitionCompositePipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		wboitVolitionCompositePipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		wboitVolitionCompositePipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		wboitVolitionCompositePipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		wboitVolitionCompositePipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		wboitVolitionCompositePipelineDesc.pVertexLayout = NULL;
		wboitVolitionCompositePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		wboitVolitionCompositePipelineDesc.pDepthState = pDepthStateDisable;
		wboitVolitionCompositePipelineDesc.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &wboitVolitionCompositePipelineDesc, &pPipelineWBOITVComposite);

		// PT shading pipeline
		GraphicsPipelineDesc ptShadePipelineDesc = {};
		ptShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ptShadePipelineDesc.pShaderProgram = pShaderPTShade;
		ptShadePipelineDesc.pRootSignature = pRootSignaturePTShade;
		ptShadePipelineDesc.mRenderTargetCount = PT_RT_COUNT;
		ptShadePipelineDesc.pColorFormats = gPTRenderTargetFormats;
		ptShadePipelineDesc.pSrgbValues = srgbDisabled;
		ptShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
		ptShadePipelineDesc.mSampleQuality = 0;
		ptShadePipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
		ptShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
		ptShadePipelineDesc.pRasterizerState = pRasterizerStateCullFront;
		ptShadePipelineDesc.pDepthState = pDepthStateNoWrite;
		ptShadePipelineDesc.pBlendState = pBlendStatePTShade;
		addPipeline(pRenderer, &ptShadePipelineDesc, &pPipelinePTShade);

		// PT composite pipeline
		GraphicsPipelineDesc ptCompositePipelineDesc = {};
		ptCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ptCompositePipelineDesc.pShaderProgram = pShaderPTComposite;
		ptCompositePipelineDesc.pRootSignature = pRootSignaturePTComposite;
		ptCompositePipelineDesc.mRenderTargetCount = 1;
		ptCompositePipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
		ptCompositePipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
		ptCompositePipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
		ptCompositePipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
		ptCompositePipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		ptCompositePipelineDesc.pVertexLayout = NULL;
		ptCompositePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		ptCompositePipelineDesc.pDepthState = pDepthStateDisable;
		ptCompositePipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &ptCompositePipelineDesc, &pPipelinePTComposite);

#if PT_USE_DIFFUSION != 0
		ImageFormat::Enum ptCopyDepthFormat = ImageFormat::R32F;

		// PT copy depth pipeline
		GraphicsPipelineDesc ptCopyDepthPipelineDesc = {};
		ptCopyDepthPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		ptCopyDepthPipelineDesc.pShaderProgram = pShaderPTCopyDepth;
		ptCopyDepthPipelineDesc.pRootSignature = pRootSignaturePTCopyDepth;
		ptCopyDepthPipelineDesc.mRenderTargetCount = 1;
		ptCopyDepthPipelineDesc.pColorFormats = &ptCopyDepthFormat;
		ptCopyDepthPipelineDesc.pSrgbValues = srgbDisabled;
		ptCopyDepthPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
		ptCopyDepthPipelineDesc.mSampleQuality = 0;
		ptCopyDepthPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
		ptCopyDepthPipelineDesc.pVertexLayout = NULL;
		ptCopyDepthPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
		ptCopyDepthPipelineDesc.pDepthState = pDepthStateDisable;
		ptCopyDepthPipelineDesc.pBlendState = NULL;
		addPipeline(pRenderer, &ptCopyDepthPipelineDesc, &pPipelinePTCopyDepth);

		// PT generate mips pipeline
		ComputePipelineDesc ptGenMipsPipelineDesc = {};
		ptGenMipsPipelineDesc.pShaderProgram = pShaderPTGenMips;
		ptGenMipsPipelineDesc.pRootSignature = pRootSignaturePTGenMips;
		addComputePipeline(pRenderer, &ptGenMipsPipelineDesc, &pPipelinePTGenMips);
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
		if (pRenderer->pActiveGpuSettings->mROVsSupported)
		{
			// AOIT shading pipeline
			GraphicsPipelineDesc aoitShadePipelineDesc = {};
			aoitShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitShadePipelineDesc.pShaderProgram = pShaderAOITShade;
			aoitShadePipelineDesc.pRootSignature = pRootSignatureAOITShade;
			aoitShadePipelineDesc.mRenderTargetCount = 0;
			aoitShadePipelineDesc.pColorFormats = NULL;
			aoitShadePipelineDesc.pSrgbValues = NULL;
			aoitShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
			aoitShadePipelineDesc.mSampleQuality = 0;
			aoitShadePipelineDesc.mDepthStencilFormat = ImageFormat::D32F;
			aoitShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
			aoitShadePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
			aoitShadePipelineDesc.pDepthState = pDepthStateNoWrite;
			aoitShadePipelineDesc.pBlendState = NULL;
			addPipeline(pRenderer, &aoitShadePipelineDesc, &pPipelineAOITShade);

			// AOIT composite pipeline
			GraphicsPipelineDesc aoitCompositePipelineDesc = {};
			aoitCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitCompositePipelineDesc.pShaderProgram = pShaderAOITComposite;
			aoitCompositePipelineDesc.pRootSignature = pRootSignatureAOITComposite;
			aoitCompositePipelineDesc.mRenderTargetCount = 1;
			aoitCompositePipelineDesc.pColorFormats = &pSwapChain->mDesc.mColorFormat;
			aoitCompositePipelineDesc.pSrgbValues = &pSwapChain->mDesc.mSrgb;
			aoitCompositePipelineDesc.mSampleCount = pSwapChain->mDesc.mSampleCount;
			aoitCompositePipelineDesc.mSampleQuality = pSwapChain->mDesc.mSampleQuality;
			aoitCompositePipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
			aoitCompositePipelineDesc.pVertexLayout = NULL;
			aoitCompositePipelineDesc.pRasterizerState = pRasterizerStateCullNone;
			aoitCompositePipelineDesc.pDepthState = pDepthStateDisable;
			aoitCompositePipelineDesc.pBlendState = pBlendStateAOITComposite;
			addPipeline(pRenderer, &aoitCompositePipelineDesc, &pPipelineAOITComposite);

			// AOIT clear pipeline
			GraphicsPipelineDesc aoitClearPipelineDesc = {};
			aoitClearPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
			aoitClearPipelineDesc.pShaderProgram = pShaderAOITClear;
			aoitClearPipelineDesc.pRootSignature = pRootSignatureAOITClear;
			aoitClearPipelineDesc.mRenderTargetCount = 0;
			aoitClearPipelineDesc.pColorFormats = NULL;
			aoitClearPipelineDesc.pSrgbValues = NULL;
			aoitClearPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
			aoitClearPipelineDesc.mSampleQuality = 0;
			aoitClearPipelineDesc.mDepthStencilFormat = ImageFormat::NONE;
			aoitClearPipelineDesc.pVertexLayout = NULL;
			aoitClearPipelineDesc.pRasterizerState = pRasterizerStateCullNone;
			aoitClearPipelineDesc.pDepthState = pDepthStateDisable;
			aoitClearPipelineDesc.pBlendState = NULL;
			addPipeline(pRenderer, &aoitClearPipelineDesc, &pPipelineAOITClear);
		}
#endif
	}

	void DestroyPipelines()
	{
		removePipeline(pRenderer, pPipelineSkybox);
#if USE_SHADOWS != 0
		removePipeline(pRenderer, pPipelineShadow);
		removePipeline(pRenderer, pPipelineGaussianBlur);
#if PT_USE_CAUSTICS != 0
		removePipeline(pRenderer, pPipelinePTShadow);
		removePipeline(pRenderer, pPipelinePTDownsample);
		removePipeline(pRenderer, pPipelinePTCopyShadowDepth);
#endif
#endif
		removePipeline(pRenderer, pPipelineForward);
		removePipeline(pRenderer, pPipelineTransparentForward);
		removePipeline(pRenderer, pPipelineWBOITShade);
		removePipeline(pRenderer, pPipelineWBOITComposite);
		removePipeline(pRenderer, pPipelineWBOITVShade);
		removePipeline(pRenderer, pPipelineWBOITVComposite);
		removePipeline(pRenderer, pPipelinePTShade);
		removePipeline(pRenderer, pPipelinePTComposite);
#if PT_USE_DIFFUSION != 0
		removePipeline(pRenderer, pPipelinePTCopyDepth);
		removePipeline(pRenderer, pPipelinePTGenMips);
#endif
#if defined(DIRECT3D12) && !defined(_DURANGO)
		removePipeline(pRenderer, pPipelineAOITShade);
		removePipeline(pRenderer, pPipelineAOITComposite);
		removePipeline(pRenderer, pPipelineAOITClear);
#endif
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
	static const char* transparencyTypeNames[] = {
		"Alpha blended",
		"(WBOIT) Weighted blended order independent transparency",
		"(WBOIT) Weighted blended order independent transparency - Volition",
		"(PT) Phenomenological transparency",
#if defined(DIRECT3D12) && !defined(_DURANGO)
		"(AOIT) Adaptive order independent transparency",
#endif
		NULL    //needed for unix
	};

	static const uint32_t transparencyTypeValues[] = {
		TRANSPARENCY_TYPE_ALPHA_BLEND,
		TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT,
		TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION,
		TRANSPARENCY_TYPE_PHENOMENOLOGICAL,
#if defined(DIRECT3D12) && !defined(_DURANGO)
		TRANSPARENCY_TYPE_ADAPTIVE_OIT,
#endif
		0    //needed for unix
	};

	uint32_t dropDownCount = 4;
#if defined(DIRECT3D12) && !defined(_DURANGO)
	if (pRenderer->pActiveGpuSettings->mROVsSupported)
		dropDownCount = 5;
#endif

	pGuiWindow->AddWidget(
		DropdownWidget("Transparency Type", &gTransparencyType, transparencyTypeNames, transparencyTypeValues, dropDownCount));

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
	else if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_PHENOMENOLOGICAL;
	}
#if defined(DIRECT3D12) && !defined(_DURANGO)
	else if (gTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT && pRenderer->pActiveGpuSettings->mROVsSupported)
	{
		GuiController::currentTransparencyType = TRANSPARENCY_TYPE_ADAPTIVE_OIT;
	}
#endif
}

DEFINE_APPLICATION_MAIN(Transparency)
