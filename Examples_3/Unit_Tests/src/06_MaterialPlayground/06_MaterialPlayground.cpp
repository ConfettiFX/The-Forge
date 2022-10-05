/*
*
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

/********************************************************************************************************
*
* The Forge - MATERIALS UNIT TEST
*
* The purpose of this demo is to show the material workflow of The-Forge,
* featuring PBR materials and environment lighting.
*
*********************************************************************************************************/

//tiny stl
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/unordered_map.h"

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"	

//input
// Animations
#undef min
#undef max
#include "../../../../Common_3/Resources/AnimationSystem/Animation/SkeletonBatcher.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/AnimatedObject.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Animation.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Clip.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/ClipController.h"
#include "../../../../Common_3/Resources/AnimationSystem/Animation/Rig.h"

#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"    // Must be the last include in a cpp file

#include "Shaders/Shared.h"
#define HAIR_DEV_UI false
#define MAX_FILENAME_LENGTH 128

// when set, all the textures are a 2x2 white image
// and the BRDF shader won't sample those textures
#define SKIP_LOADING_TEXTURES 0

#define TEXTURE_RESOLUTION "2K"
//--------------------------------------------------------------------------------------------
// MATERIAL DEFINTIONS
//--------------------------------------------------------------------------------------------
typedef enum EMaterialTypes
{
	MATERIAL_METAL = 0,
	MATERIAL_WOOD,
	MATERIAL_HAIR,
	MATERIAL_BRDF_COUNT = MATERIAL_HAIR,
	MATERIAL_COUNT
} MaterialType;

typedef enum RenderMode
{
	RENDER_MODE_SHADED = 0,
	RENDER_MODE_ALBEDO,
	RENDER_MODE_NORMALS,
	RENDER_MODE_ROUGHNESS,
	RENDER_MODE_METALLIC,
	RENDER_MODE_AO,

	RENDER_MODE_COUNT
} RenderMode;

typedef enum HairType
{
	HAIR_TYPE_PONYTAIL,
	HAIR_TYPE_FEMALE_1,
	HAIR_TYPE_FEMALE_2,
	HAIR_TYPE_FEMALE_3,
	HAIR_TYPE_FEMALE_6,
	HAIR_TYPE_COUNT
} HairType;

typedef enum MeshResource
{
	MESH_MAT_BALL,
	MESH_CUBE,
	MESH_CAPSULE,
	MESH_COUNT,
} MeshResource;

typedef enum MaterialTexture
{
	MATERIAL_TEXTURE_ALBEDO,
	MATERIAL_TEXTURE_NORMAL,
	MATERIAL_TEXTURE_METALLIC,
	MATERIAL_TEXTURE_ROUGHNESS,
	MATERIAL_TEXTURE_OCCLUSION,
	MATERIAL_TEXTURE_COUNT
} MaterialTexture;

typedef enum HairColor
{
	HAIR_COLOR_BROWN,
	HAIR_COLOR_BLONDE,
	HAIR_COLOR_BLACK,
	HAIR_COLOR_RED,
	HAIR_COLOR_COUNT
} HairColor;

// testing a material made of raisins...
#define RAISINS 0

static const char* metalEnumNames[] = { "Aluminum", "Scratched Gold",
										"Copper",   "Tiled Metal",
#if RAISINS
										"Raisins",
#else
										"Old Iron",
#endif
										"Bronze",   NULL };
static const char* woodEnumNames[] = { "Wooden Plank 05", "Wooden Plank 06", "Wood #03", "Wood #08", "Wood #16", "Wood #18", NULL };

static const uint32_t MATERIAL_INSTANCE_COUNT = sizeof(metalEnumNames) / sizeof(metalEnumNames[0]) - 1;

const char* gHeadAttachmentJointName = "Bip01 HeadNub";
const char* gLeftShoulderJointName = "LeftShoulder";
const char* gRightShoulderJointName = "RightShoulder";

const uint32_t gImageCount = 3;
ProfileToken   gHairGpuProfileToken;
ProfileToken   gMetalWoodGpuProfileToken;
ProfileToken   gCurrentGpuProfileToken;
//--------------------------------------------------------------------------------------------
// STRUCT DEFINTIONS
//--------------------------------------------------------------------------------------------
struct UniformCamData
{
	CameraMatrix mProjectView;
    CameraMatrix mInvProjectView;

	vec3 mCamPos;

	float fAmbientLightIntensity = 0.0f;

	int   bUseEnvMap = 0;
	float fEnvironmentLightIntensity = 0.5f;
	float fAOIntensity = 0.01f;
	int   iRenderMode = 0;
	float fNormalMapIntensity = 1.0f;
};

struct UniformCamDataShadow
{
    mat4 mProjectView;
    mat4 mInvProjectView;

    vec3 mCamPos;
};

struct UniformObjData
{
	mat4   mWorldMat;
	float3 mAlbedo = float3(1, 1, 1);
	float  mRoughness = 0.04f;

	float2 tiling = float2(1, 1);
	float  mMetallic = 0.0f;

	int textureConfig = 0;
};
enum ETextureConfigFlags
{
	// specifies which textures are used for the material
	DIFFUSE = (1 << 0),
	NORMAL = (1 << 1),
	METALLIC = (1 << 2),
	ROUGHNESS = (1 << 3),
	AO = (1 << 4),

	TEXTURE_CONFIG_FLAGS_ALL = DIFFUSE | NORMAL | METALLIC | ROUGHNESS | AO,
	TEXTURE_CONFIG_FLAGS_NONE = 0,

	// specifies which diffuse reflection model to use
	OREN_NAYAR = (1 << 5),    // Lambert otherwise, we just check if this flag is set for now

	NUM_TEXTURE_CONFIG_FLAGS = 6
};

enum EDiffuseReflectionModels
{
	LAMBERT_REFLECTION = 0,
	OREN_NAYAR_REFLECTION,

	DIFFUSE_REFLECTION_MODEL_COUNT
};

struct PointLight
{
	float3 mPosition;
	float  mRadius;
	float3 mColor;
	float  mIntensity;
};

struct DirectionalLight
{
	float3 mDirection;
	int    mShadowMap;
	float3 mColor;
	float  mIntensity;
	float  mShadowRange;
	float2 _pad;
	int    mShadowMapDimensions;
	mat4   mViewProj;
};

struct UniformDataPointLights
{
	PointLight mPointLights[MAX_NUM_POINT_LIGHTS] = {};
	uint       mNumPointLights = 0;
};

struct UniformDataDirectionalLights
{
	DirectionalLight mDirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS] = {};
	uint             mNumDirectionalLights = 0;
};

struct Capsule
{
	float3 mCenter0;
	float  mRadius0;
	float3 mCenter1;
	float  mRadius1;
};

struct NamedCapsule
{
	const char* mName = NULL;
	Capsule     mCapsule = {};
	int         mAttachedBone = -1;
};

struct Transform
{
	vec3  mPosition;
	vec3  mOrientation;
	float mScale;
};

struct NamedTransform
{
	Transform   mTransform = {};
	const char* mName = NULL;
	int         mAttachedBone = -1;
};

struct UniformDataHairGlobal
{
	float4 mViewport;
	float4 mGravity;
	float4 mWind;
	float  mTimeStep;
};

struct UniformDataHairShading
{
	mat4  mTransform;
	uint  mRootColor;
	uint  mStrandColor;
	float mColorBias;
	float mKDiffuse;
	float mKSpecular1;
	float mKExponent1;
	float mKSpecular2;
	float mKExponent2;
	float mStrandRadius;
	float mStrandSpacing;
	uint  mNumVerticesPerStrand;
};

struct UniformDataHairSimulation
{
	mat4 mTransform;
	Quat mQuatRotation;
#if HAIR_MAX_CAPSULE_COUNT > 0
	Capsule mCapsules[HAIR_MAX_CAPSULE_COUNT];    // Hair local space capsules
	uint    mCapsuleCount;
#endif
	float mScale;
	uint  mNumStrandsPerThreadGroup;
	uint  mNumFollowHairsPerGuideHair;
	uint  mNumVerticesPerStrand;
	float mDamping;
	float mGlobalConstraintStiffness;
	float mGlobalConstraintRange;
	float mShockPropagationStrength;
	float mShockPropagationAccelerationThreshold;
	float mLocalStiffness;
	uint  mLocalConstraintIterations;
	uint  mLengthConstraintIterations;
	float mTipSeperationFactor;
};

struct HairBuffer
{
	Geometry*                 pGeom = NULL;
	Buffer*                   pBufferHairVertexPositions = NULL;
	Buffer*                   pBufferHairVertexTangents = NULL;
	Buffer*                   pBufferTriangleIndices = NULL;
	Buffer*                   pBufferHairRestLenghts = NULL;
	Buffer*                   pBufferHairGlobalRotations = NULL;
	Buffer*                   pBufferHairRefsInLocalFrame = NULL;
	Buffer*                   pBufferFollowHairRootOffsets = NULL;
	Buffer*                   pBufferHairThicknessCoefficients = NULL;
	Buffer*                   pBufferHairSimulationVertexPositions[3] = { NULL };
	Buffer*                   pUniformBufferHairShading[gImageCount] = { NULL };
	Buffer*                   pUniformBufferHairSimulation[gImageCount] = { NULL };
	UniformDataHairShading    mUniformDataHairShading = {};
	UniformDataHairSimulation mUniformDataHairSimulation = {};
	uint                      mIndexCountHair = 0;
	uint                      mTotalVertexCount = 0;
	uint                      mNumGuideStrands = 0;
	float                     mStrandRadius = 0.0f;
	float                     mStrandSpacing = 0.0f;
	uint                      mTransform = 0;    // Index into gTransforms
	bool                      mDisableRootColor = false;
#if HAIR_MAX_CAPSULE_COUNT > 0
	uint mCapsules[HAIR_MAX_CAPSULE_COUNT] = {};    // World space capsules
#endif
};

struct GlobalHairParameters
{
	float4 mGravity;    // Gravity direction * magnitude
	float4 mWind;       // Wind direction * magnitude
};

struct HairShadingParameters
{
	float4 mRootColor;      // Hair color near the root
	float4 mStrandColor;    // Hair color away from the root
	float  mKDiffuse;       // Diffuse light contribution
	float  mKSpecular1;     // Specular 1 light contribution
	float  mKExponent1;     // Specular 1 exponent
	float  mKSpecular2;     // Specular 2 light contribution
	float  mKExponent2;     // Specular 2 exponent
};

struct HairSectionShadingParameters
{
	float mColorBias;           // Bias between root and strand color
	float mStrandRadius;        // Strand width
	float mStrandSpacing;       // Strand density
	bool  mDisableRootColor;    // Stops the root color from being used.
};

struct HairSimulationParameters
{
	float mDamping;                                  // Dampens hair velocity over time
	float mGlobalConstraintStiffness;                // Force keeping the hair in its original position
	float mGlobalConstraintRange;                    // Range to apply global constraint to
	float mShockPropagationStrength;                 // Force propagating sudden changes to the rest of the strand
	float mShockPropagationAccelerationThreshold;    // Threshold at which to start shock propagation
	float mLocalConstraintStiffness;                 // Force keeping strands in the rest shape
	uint  mLocalConstraintIterations;                // Number of local constraint iterations
	uint  mLengthConstraintIterations;               // Number of length constraint iterations
	float mTipSeperationFactor;                      // Separates follow hairs from their guide hair
#if HAIR_MAX_CAPSULE_COUNT > 0
	uint mCapsuleCount;                        // Number of collision capsules
	uint mCapsules[HAIR_MAX_CAPSULE_COUNT];    // Index into gCapsules for collision capsules the hair will collide with
#endif
};

struct HairTypeInfo
{
	bool mInView;
	bool mPreWarm;
};

//--------------------------------------------------------------------------------------------
// RENDERING PIPELINE DATA
//--------------------------------------------------------------------------------------------
Renderer*  pRenderer = NULL;
Queue*     pGraphicsQueue = NULL;
CmdPool*   pCmdPools[gImageCount];
Cmd*       pCmds[gImageCount];
CmdPool*   pUICmdPools[gImageCount];
Cmd*       pUICmds[gImageCount];
SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
uint32_t   gFrameIndex = 0;

VertexLayout gVertexLayoutDefault = {};
//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------
ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;
FontDrawDesc       gFrameTimeDraw; // = TextDrawDesc(0, 0xff00ff00, 18);
FontDrawDesc       gErrMsgDrawDesc; // = TextDrawDesc(0, 0xff0000ee, 18);
UIComponent*      pGuiWindowMain = NULL;
UIComponent*      pGuiWindowHairSimulation = NULL;
UIComponent*      pGuiWindowMaterial = NULL;
LuaManager         gLuaManager;
ThreadSystem*      pIOThreads = NULL;

//--------------------------------------------------------------------------------------------
// SAMPLERS
//--------------------------------------------------------------------------------------------
Sampler* pSamplerBilinear = NULL;
Sampler* pSamplerBilinearClamped = NULL;
Sampler* pSamplerPoint = NULL;

//--------------------------------------------------------------------------------------------
// SHADERS
//--------------------------------------------------------------------------------------------
Shader* pShaderSkybox = NULL;
Shader* pShaderBRDF = NULL;
Shader* pShaderShadowPass = NULL;

Shader* pShaderHairClear = NULL;
Shader* pShaderHairDepthPeeling = NULL;
Shader* pShaderHairDepthResolve = NULL;
Shader* pShaderHairFillColors = NULL;
Shader* pShaderHairResolveColor = NULL;
Shader* pShaderHairIntegrate = NULL;
Shader* pShaderHairShockPropagation = NULL;
Shader* pShaderHairLocalConstraints = NULL;
Shader* pShaderHairLengthConstraints = NULL;
Shader* pShaderHairUpdateFollowHairs = NULL;
Shader* pShaderHairPreWarm = NULL;
Shader* pShaderShowCapsules = NULL;
Shader* pShaderSkeleton = NULL;
Shader* pShaderHairShadow = NULL;

//--------------------------------------------------------------------------------------------
// ROOT SIGNATURES
//--------------------------------------------------------------------------------------------
RootSignature* pRootSignatureSkybox = NULL;
RootSignature* pRootSignatureBRDF = NULL;
RootSignature* pRootSignatureShadowPass = NULL;

RootSignature* pRootSignatureHairClear = NULL;
RootSignature* pRootSignatureHairDepthPeeling = NULL;
RootSignature* pRootSignatureHairDepthResolve = NULL;
RootSignature* pRootSignatureHairFillColors = NULL;
RootSignature* pRootSignatureHairColorResolve = NULL;
RootSignature* pRootSignatureHairIntegrate = NULL;
RootSignature* pRootSignatureHairShockPropagation = NULL;
RootSignature* pRootSignatureHairLocalConstraints = NULL;
RootSignature* pRootSignatureHairLengthConstraints = NULL;
RootSignature* pRootSignatureHairUpdateFollowHairs = NULL;
RootSignature* pRootSignatureHairPreWarm = NULL;
RootSignature* pRootSignatureShowCapsules = NULL;
RootSignature* pRootSignatureSkeleton = NULL;
RootSignature* pRootSignatureHairShadow = NULL;

//--------------------------------------------------------------------------------------------
// DESCRIPTOR SET
//--------------------------------------------------------------------------------------------
DescriptorSet* pDescriptorSetShadow[2] = { NULL };
DescriptorSet* pDescriptorSetSkybox[2] = { NULL };
DescriptorSet* pDescriptorSetBRDF[3] = { NULL };

DescriptorSet* pDescriptorSetHairClear = { NULL };
DescriptorSet* pDescriptorSetHairPreWarm = { NULL };
DescriptorSet* pDescriptorSetHairIntegrate = { NULL };
DescriptorSet* pDescriptorSetHairShockPropagate = { NULL };
DescriptorSet* pDescriptorSetHairLocalConstraints = { NULL };
DescriptorSet* pDescriptorSetHairLengthConstraints = { NULL };
DescriptorSet* pDescriptorSetHairFollowHairs = { NULL };
DescriptorSet* pDescriptorSetHairShadow[2] = { NULL };
DescriptorSet* pDescriptorSetHairDepthPeeling[3] = { NULL };
DescriptorSet* pDescriptorSetHairDepthResolve = { NULL };
DescriptorSet* pDescriptorSetHairFillColors[4] = { NULL };
DescriptorSet* pDescriptorSetHairColorResolve = { NULL };
DescriptorSet* pDescriptorSetShowCapsule = { NULL };

uint32_t gHairDynamicDescriptorSetCount = 0;
//--------------------------------------------------------------------------------------------
// PIPELINES
//--------------------------------------------------------------------------------------------
Pipeline* pPipelineSkybox = NULL;
Pipeline* pPipelineBRDF = NULL;
Pipeline* pPipelineShadowPass = NULL;

Pipeline* pPipelineHairClear = NULL;
Pipeline* pPipelineHairDepthPeeling = NULL;
Pipeline* pPipelineHairDepthResolve = NULL;
Pipeline* pPipelineHairFillColors = NULL;
Pipeline* pPipelineHairColorResolve = NULL;
Pipeline* pPipelineHairIntegrate = NULL;
Pipeline* pPipelineHairShockPropagation = NULL;
Pipeline* pPipelineHairLocalConstraints = NULL;
Pipeline* pPipelineHairLengthConstraints = NULL;
Pipeline* pPipelineHairUpdateFollowHairs = NULL;
Pipeline* pPipelineHairPreWarm = NULL;
Pipeline* pPipelineShowCapsules = NULL;
Pipeline* pPipelineSkeleton = NULL;
Pipeline* pPipelineHairShadow = NULL;

//--------------------------------------------------------------------------------------------
// RENDER TARGETS
//--------------------------------------------------------------------------------------------
RenderTarget* pRenderTargetShadowMap = NULL;
RenderTarget* pRenderTargetDepth = NULL;
RenderTarget* pRenderTargetDepthPeeling = NULL;
RenderTarget* pRenderTargetFillColors = NULL;
RenderTarget* pRenderTargetHairShadows[HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS] = { { NULL } };
#ifndef METAL
Texture* pTextureHairDepth = NULL;
#else
// Metal does not support atomics on textures so we have to use a buffer
Buffer* pBufferHairDepth = NULL;
#endif

//--------------------------------------------------------------------------------------------
// VERTEX BUFFERS
//--------------------------------------------------------------------------------------------
Buffer*                   pVertexBufferSkybox = NULL;
eastl::vector<HairBuffer> gHair;
Buffer*                   pVertexBufferSkeletonJoint = NULL;
int                       gVertexCountSkeletonJoint = 0;
Buffer*                   pVertexBufferSkeletonBone = NULL;
int                       gVertexCountSkeletonBone = 0;

//--------------------------------------------------------------------------------------------
// INDEX BUFFERS
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// MESHES
//--------------------------------------------------------------------------------------------
eastl::vector<Geometry*> gMeshes;

//--------------------------------------------------------------------------------------------
// UNIFORM BUFFERS
//--------------------------------------------------------------------------------------------
Buffer* pUniformBufferCamera[gImageCount] = { NULL };
Buffer* pUniformBufferCameraShadowPass[gImageCount] = { NULL };
Buffer* pUniformBufferCameraSkybox[gImageCount] = { NULL };
Buffer* pUniformBufferCameraHairShadows[gImageCount][HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS] = {};
Buffer* pUniformBufferGroundPlane = NULL;
Buffer* pUniformBufferMatBall[gImageCount][MATERIAL_INSTANCE_COUNT];
Buffer* pUniformBufferNamePlates[MATERIAL_INSTANCE_COUNT];
Buffer* pUniformBufferPointLights = NULL;
Buffer* pUniformBufferDirectionalLights[gImageCount] = { NULL };
Buffer* pUniformBufferHairGlobal = NULL;

//--------------------------------------------------------------------------------------------
// TEXTURES
//--------------------------------------------------------------------------------------------
const int gMaterialTextureCount = MATERIAL_INSTANCE_COUNT * MATERIAL_TEXTURE_COUNT * MATERIAL_COUNT;

Texture*                pTextureSkybox = NULL;
Texture*                pTextureBRDFIntegrationMap = NULL;
eastl::vector<Texture*> gTextureMaterialMaps;          // objects
eastl::vector<Texture*> gTextureMaterialMapsGround;    // ground

Texture* pTextureIrradianceMap = NULL;
Texture* pTextureSpecularMap = NULL;

//--------------------------------------------------------------------------------------------
// UNIFORM DATA
//--------------------------------------------------------------------------------------------
UniformCamData               gUniformDataCamera;
UniformCamData               gUniformDataCameraSkybox;
UniformCamDataShadow         gUniformDataCameraHairShadows[HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS];
UniformDataPointLights       gUniformDataPointLights;
UniformObjData               gUniformDataMatBall[MATERIAL_INSTANCE_COUNT];
UniformDataDirectionalLights gUniformDataDirectionalLights;
UniformDataHairGlobal        gUniformDataHairGlobal;

//--------------------------------------------------------------------------------------------
// SKELETAL ANIMATION
//--------------------------------------------------------------------------------------------

Clip            gAnimationClipNeckCrack;
Clip            gAnimationClipStand;
ClipController  gAnimationClipControllerNeckCrack[HAIR_TYPE_COUNT];
ClipController  gAnimationClipControllerStand[HAIR_TYPE_COUNT];
Animation       gAnimation[HAIR_TYPE_COUNT];
Rig             gAnimationRig;
AnimatedObject  gAnimatedObject[HAIR_TYPE_COUNT];
SkeletonBatcher gSkeletonBatcher;

eastl::vector<NamedCapsule>   gCapsules;
eastl::vector<NamedTransform> gTransforms;
eastl::vector<Capsule>        gFinalCapsules[HAIR_TYPE_COUNT];    // Stores the capsule transformed by the bone matrix

//--------------------------------------------------------------------------------------------
// UI & OTHER
//--------------------------------------------------------------------------------------------
bool                gShowCapsules = false;
uint                gHairType = 0;
eastl::vector<uint> gHairTypeIndices[HAIR_TYPE_COUNT];
HairTypeInfo        gHairTypeInfo[HAIR_TYPE_COUNT];
bool                gEnvironmentLighting = true;
bool                gDrawSkybox = true;
uint32_t            gMaterialType = MATERIAL_METAL;
uint32_t            gDiffuseReflectionModel = LAMBERT_REFLECTION;
bool                gbLuaScriptingSystemLoadedSuccessfully = false;
bool                gbAnimateCamera = false;

eastl::unordered_map<EMaterialTypes, EDiffuseReflectionModels> gMaterialLightingModelMap;

FontDrawDesc gMaterialPropDraw; // = TextDrawDesc(0, 0xffaaaaaa, 32);

// light
const int gShadowMapDimensions = 2048;
float4    gDirectionalLightColor = unpackR8G8B8A8_SRGB(0xffb475ff);
float     gDirectionalLightIntensity = 10.0f;
float3    gDirectionalLightPosition = float3(-33.030f, 9.09f, 100.0f);
float     gAmbientLightIntensity = 0.01f;
float     gEnvironmentLightingIntensity = 0.35f;

// material
uint32_t gRenderMode = 0;
bool     gOverrideRoughnessTextures = false;
float    gRoughnessOverride = 0.04f;
bool     gDisableNormalMaps = false;
float    gNormalMapIntensity = 0.56f;
bool     gDisableAOMaps = false;
float    gAOIntensity = 1.00f;

uint32_t       gHairColor = HAIR_COLOR_BROWN;
uint32_t       gLastHairColor = gHairColor;
bool           gFirstHairSimulationFrame = true;
GPUPresetLevel gGPUPresetLevel;

CameraMatrix                gTextProjView;
eastl::vector<mat4> gTextWorldMats;

void ReloadScriptButtonCallback(void* pUserData) { gLuaManager.ReloadUpdatableScript(); }

bool gTakeScreenshot = false;

void takeScreenshot(void* pUserData)
{
	if (!gTakeScreenshot)
		gTakeScreenshot = true;
}

// Finds the vertex in the direction of the normal
vec3 AABBGetVertex(const AABB& b, const vec3& normal)
{
	vec3 p = b.minBounds;
	for (int i = 0; i < 3; ++i)
	{
		if (normal[i] >= 0.0f)
			p[i] = b.maxBounds[i];
	}
	return p;
}

bool AABBInFrustum(const AABB& b, vec4 frustumPlanes[6])
{
	for (int i = 0; i < 6; i++)
	{
		float distance = dot(AABBGetVertex(b, frustumPlanes[i].getXYZ()), frustumPlanes[i].getXYZ()) + frustumPlanes[i].getW();
		if (distance < 0.0f)
			return false;
	}
	return true;
}

struct GuiController
{
	static void AddGui();
	static void UpdateDynamicUI();
	static void Exit();

	struct HairDynamicWidgets
	{
		DynamicUIWidgets hairShading;
		DynamicUIWidgets hairSimulation;
	};
	static eastl::vector<HairDynamicWidgets> hairDynamicWidgets;

	static DynamicUIWidgets hairShadingDynamicWidgets;
	static DynamicUIWidgets hairSimulationDynamicWidgets;

	static DynamicUIWidgets materialDynamicWidgets;

	static MaterialType currentMaterialType;
	static uint         currentHairType;
};
eastl::vector<GuiController::HairDynamicWidgets> GuiController::hairDynamicWidgets;
DynamicUIWidgets                                 GuiController::hairShadingDynamicWidgets;
DynamicUIWidgets                                 GuiController::hairSimulationDynamicWidgets;
DynamicUIWidgets                                 GuiController::materialDynamicWidgets;
MaterialType                                     GuiController::currentMaterialType;
uint                                             GuiController::currentHairType = 0;

const char* gTestScripts[] = { "Test_Metal.lua", "Test_Wood.lua",
#if !defined(METAL)
							   "Test_Hair.lua"
#endif
};

uint32_t gCurrentScriptIndex = 0;

uint32_t gFontID = 0; 

void RunScript(void* pUserData) 
{ 
	LuaScriptDesc runDesc = {};
	runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
	luaQueueScriptToRun(&runDesc);
}

class MaterialPlayground: public IApp
{
	public:
	MaterialPlayground() //-V832
	{
#ifdef TARGET_IOS
		mSettings.mContentScaleFactor = 1.f;
#endif
	}

	struct StagingData
	{
		struct TextureData
		{
			const char* pFileName;
			bool        mIsSrgb;
		};
		eastl::vector<char*>       mModelList;
		eastl::vector<TextureData> mMaterialNamesStorage;
		eastl::vector<TextureData> mGroundNamesStorage;
		float*                     pJointPoints;
		float*                     pBonePoints;
		~StagingData()
		{
			for (char* model : mModelList)
				tf_free(model);

			tf_free(pJointPoints);
			tf_free(pBonePoints);
		}
	};
	StagingData* pStagingData = NULL;

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_ANIMATIONS, "Animation");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");

		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mD3D11Supported = true;
		initRenderer(GetName(), &settings, &pRenderer);
		if (!pRenderer)
			return false; 
		
		gGPUPresetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);

			addCmdPool(pRenderer, &cmdPoolDesc, &pUICmdPools[i]);
			cmdDesc.pPool = pUICmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pUICmds[i]);
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initThreadSystem(&pIOThreads);

		// INITIALIZE CAMERA & INPUT
		//
		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ -0.21f, 12.2564745f, 59.3652649f };
		vec3                   lookAt{ 0, 0, 0 };

		pCameraController = initFpsCameraController(camPos, lookAt);
		pLightView = initGuiCameraController(f3Tov3(gDirectionalLightPosition), vec3(0, 0, 0));
		pCameraController->setMotionParameters(camParameters);

		// INITIALIZE SCRIPTING
		//
		luaDestroyCurrentManager(); 

		gLuaManager.Init();
		ICameraController* cameraLocalPtr = pCameraController;
		gLuaManager.SetFunction("GetCameraPosition", [cameraLocalPtr](ILuaStateWrap* state) -> int {
			vec3 pos = cameraLocalPtr->getViewPosition();
			state->PushResultNumber(pos.getX());
			state->PushResultNumber(pos.getY());
			state->PushResultNumber(pos.getZ());
			return 3;    // return amount of arguments
		});
		gLuaManager.SetFunction("SetCameraPosition", [cameraLocalPtr](ILuaStateWrap* state) -> int {
			float x = (float)state->GetNumberArg(1);    //in Lua indexing starts from 1!
			float y = (float)state->GetNumberArg(2);
			float z = (float)state->GetNumberArg(3);
			cameraLocalPtr->moveTo(vec3(x, y, z));
			return 0;    // return amount of arguments
		});
		gLuaManager.SetFunction("LookAtWorldOrigin", [cameraLocalPtr](ILuaStateWrap* state) -> int {
			cameraLocalPtr->lookAt(vec3(0, 0, 0));
			return 0;    // return amount of arguments
		});
		gLuaManager.SetFunction("GetIsCameraAnimated", [](ILuaStateWrap* state) -> int {
			state->PushResultInteger(gbAnimateCamera ? 1 : 0);
			return 1;    // return amount of arguments
		});
		gbLuaScriptingSystemLoadedSuccessfully = gLuaManager.SetUpdatableScript("updateCamera.lua", NULL, "Update", "Exit");

		// SET MATERIAL LIGHTING MODELS
		//
		gMaterialLightingModelMap[MATERIAL_METAL] = LAMBERT_REFLECTION;
		gMaterialLightingModelMap[MATERIAL_WOOD] = OREN_NAYAR_REFLECTION;
		// hair := custom shader. we still assign LAMBERT_REFLECTION to avoid branching logic when querying this map.
		gMaterialLightingModelMap[MATERIAL_HAIR] = LAMBERT_REFLECTION;

		// ... add more as new mateirals are introduced

		initAnimations();

		// INITIALIZE RESOURCE SYSTEMS
		//

		initResourceLoaderInterface(pRenderer);
		initScreenshotInterface(pRenderer, pGraphicsQueue);

		pStagingData = tf_new(StagingData);
		// CREATE RENDERING RESOURCES
		//
		ComputePBRMaps();

		LoadModelsAndTextures();

		addSamplers();

		addUniformBuffers();

		addResources();

		// Create skeleton batcher
		SkeletonRenderDesc skeletonRenderDesc = {};
		skeletonRenderDesc.mRenderer = pRenderer;
		skeletonRenderDesc.mSkeletonPipeline = pPipelineSkeleton;
		skeletonRenderDesc.mJointVertexBuffer = pVertexBufferSkeletonJoint;
		skeletonRenderDesc.mNumJointPoints = gVertexCountSkeletonJoint;
		skeletonRenderDesc.mDrawBones = true;
		skeletonRenderDesc.mBoneVertexBuffer = pVertexBufferSkeletonBone;
		skeletonRenderDesc.mNumBonePoints = gVertexCountSkeletonBone;
		skeletonRenderDesc.mBoneVertexStride = sizeof(float) * 6;
		skeletonRenderDesc.mJointVertexStride = sizeof(float) * 6;
		skeletonRenderDesc.mMaxAnimatedObjects = HAIR_TYPE_COUNT;

		gSkeletonBatcher.Initialize(skeletonRenderDesc);


		// Load skeleton batcher rigs
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			gSkeletonBatcher.AddAnimatedObject(&gAnimatedObject[hairType]);
		}

		waitThreadSystemIdle(pIOThreads);
		waitForAllResourceLoads();

		/* both buffers for file names are singular allocations */
		tf_free((char*)pStagingData->mMaterialNamesStorage[0].pFileName);
		tf_free((char*)pStagingData->mGroundNamesStorage[0].pFileName);
		tf_delete(pStagingData);

		InitializeUniformBuffers();

		luaAssignCustomManager(&gLuaManager);

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// INITIALIZE UI
		//
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
		LuaScriptDesc scriptDescs[numScripts] = {};
		for (uint32_t i = 0; i < numScripts; ++i)
			scriptDescs[i].pScriptFileName = gTestScripts[i];
		luaDefineScripts(scriptDescs, numScripts);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		gMetalWoodGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
		gHairGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
		gCurrentGpuProfileToken = gMetalWoodGpuProfileToken;

		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(5, 220.0f);
		guiDesc.mStartSize = vec2(450, 600);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindowMain);

		ButtonWidget screenshot;
		UIWidget*     pScreenshot = uiCreateComponentWidget(pGuiWindowMain, "Screenshot", &screenshot, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pScreenshot, nullptr, takeScreenshot);
		luaRegisterWidget(pScreenshot);

		//guiDesc.mStartPosition = vec2(300, 300.0f) / dpiScale;
		guiDesc.mStartPosition = vec2((float)mSettings.mWidth - 300.0f, 20.0f);
		uiCreateComponent("Material Properties", &guiDesc, &pGuiWindowMaterial);

		guiDesc.mStartPosition = vec2((float)mSettings.mWidth - 300.0f, 200.0f);
		guiDesc.mStartSize = vec2(450, 600);
		uiCreateComponent("Hair simulation", &guiDesc, &pGuiWindowHairSimulation);
		GuiController::AddGui();

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

		gFrameIndex = 0;

		return true;
	}

	void Exit()
	{
		exitAnimations();

		exitInputSystem();

		gLuaManager.Exit();

		exitThreadSystem(pIOThreads);
		exitCameraController(pCameraController);
		exitCameraController(pLightView);

		gTextWorldMats.set_capacity(0);

		gCapsules.set_capacity(0);
		gTransforms.set_capacity(0);
		for (uint32_t i = 0; i < HAIR_TYPE_COUNT; ++i)
		{
			gFinalCapsules[i].set_capacity(0);
			gHairTypeIndices[i].set_capacity(0);
		}

		gMaterialLightingModelMap.clear(true);

		gFirstHairSimulationFrame = true;
		for (uint32_t i = 0; i < HAIR_TYPE_COUNT; ++i)
			gHairTypeInfo[i] = {};
		gHairDynamicDescriptorSetCount = 0;

		removeGpuProfiler(gHairGpuProfileToken);
		removeGpuProfiler(gMetalWoodGpuProfileToken);

		exitProfiler();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		removeUniformBuffers();

		// Destroy skeleton batcher
		gSkeletonBatcher.Exit();

		removeResources();
		removeTextures();
		removeModels();
		removePBRMaps();

		removeSamplers();

		GuiController::Exit();
		exitUserInterface();

		exitFontSystem(); 

		// Remove commands and command pool&
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeCmd(pRenderer, pUICmds[i]);
			removeCmdPool(pRenderer, pUICmdPools[i]);

			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}

		removeQueue(pRenderer, pGraphicsQueue);

		// Remove resource loader and renderer
		exitResourceLoaderInterface(pRenderer);
		exitScreenshotInterface();

		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
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
		fontLoad.mDepthFormat = pRenderTargetDepth->mFormat;
		fontLoad.mDepthCompareMode = CompareMode::CMP_LEQUAL;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

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

		// UPDATE UI & CAMERA
		//
		GuiController::UpdateDynamicUI();

		if (gbLuaScriptingSystemLoadedSuccessfully)
		{
			gLuaManager.Update(deltaTime);
		}

		pCameraController->update(deltaTime);

		// calculate matrices
		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 3.0f;
		CameraMatrix projMat = CameraMatrix::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);

		// UPDATE UNIFORM BUFFERS
		//
		// cameras
        gTextProjView = projMat * viewMat;
		gUniformDataCamera.mProjectView = projMat * viewMat;
		gUniformDataCamera.mInvProjectView = CameraMatrix::inverse(gUniformDataCamera.mProjectView);
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();
		gUniformDataCamera.fAmbientLightIntensity = gAmbientLightIntensity;
		gUniformDataCamera.bUseEnvMap = gEnvironmentLighting;
		gUniformDataCamera.fAOIntensity = gAOIntensity;
		gUniformDataCamera.iRenderMode = gRenderMode;
		gUniformDataCamera.fNormalMapIntensity = gNormalMapIntensity;
		gUniformDataCamera.fEnvironmentLightIntensity = gEnvironmentLightingIntensity;

		vec4 frustumPlanes[6];
        CameraMatrix::extractFrustumClipPlanes(
			gUniformDataCamera.mProjectView, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2], frustumPlanes[3], frustumPlanes[4],
			frustumPlanes[5], true);

		viewMat.setTranslation(vec3(0));
		gUniformDataCameraSkybox = gUniformDataCamera;
		gUniformDataCameraSkybox.mProjectView = projMat * viewMat;

		// Ensure we have the correct light view in case the position changes (right now it can change through the UI)
		pLightView->moveTo(f3Tov3(gDirectionalLightPosition));
		pLightView->lookAt(vec3(0.f));
		viewMat = pLightView->getViewMatrix();

		// lights
		gUniformDataDirectionalLights.mDirectionalLights[0].mDirection = v3ToF3(normalize(f3Tov3(gDirectionalLightPosition)));
		gUniformDataDirectionalLights.mDirectionalLights[0].mShadowMap = 0;
		gUniformDataDirectionalLights.mDirectionalLights[0].mIntensity = gDirectionalLightIntensity;
		gUniformDataDirectionalLights.mDirectionalLights[0].mColor = gDirectionalLightColor.getXYZ();
		gUniformDataDirectionalLights.mDirectionalLights[0].mViewProj = projMat.getPrimaryMatrix() * viewMat;
		gUniformDataDirectionalLights.mDirectionalLights[0].mShadowMapDimensions = gShadowMapDimensions;
		gUniformDataDirectionalLights.mNumDirectionalLights = 1;

		gUniformDataPointLights.mNumPointLights = 0;    // short out point lights for now

		// update the texture config (position and all other variables are
		// set during initialization and they dont change during Update()).
		//
		for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			UniformObjData& objUniform = gUniformDataMatBall[i];

			// Add the Oren-Nayar diffuse model to the texture config.
			objUniform.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL;
			if (gDiffuseReflectionModel == OREN_NAYAR_REFLECTION)
				objUniform.textureConfig |= ETextureConfigFlags::OREN_NAYAR;

			// Update material properties
			if (gOverrideRoughnessTextures)
			{
				objUniform.textureConfig = objUniform.textureConfig & ~ETextureConfigFlags::ROUGHNESS;
				objUniform.mRoughness = gRoughnessOverride;
			}
			if (gDisableNormalMaps)
			{
				objUniform.textureConfig = objUniform.textureConfig & ~ETextureConfigFlags::NORMAL;
			}
			if (gDisableAOMaps)
			{
				objUniform.textureConfig = objUniform.textureConfig & ~ETextureConfigFlags::AO;
			}
			if (SKIP_LOADING_TEXTURES != 0)
			{
				objUniform.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_NONE;
			}
		}

		if (gMaterialType == MATERIAL_HAIR)
		{
			if (gHairColor != gLastHairColor)
			{
				for (size_t i = 0; i < gHair.size(); ++i)
					SetHairColor(&gHair[i], (HairColor)gHairColor);
				gLastHairColor = gHairColor;
			}

			if (gUniformDataDirectionalLights.mNumDirectionalLights > 0)
				gSkeletonBatcher.SetSharedUniforms(
					gUniformDataCamera.mProjectView, f3Tov3(gUniformDataDirectionalLights.mDirectionalLights[0].mDirection),
					f3Tov3(gUniformDataDirectionalLights.mDirectionalLights[0].mColor));
			else
				gSkeletonBatcher.SetSharedUniforms(gUniformDataCamera.mProjectView, vec3(0.0f, 10.0f, 2.0f), vec3(1.0f, 1.0f, 1.0f));

			// Update animated objects
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				vec3 skeletonPosition = vec3(20.0f - hairType * 10.0f, -5.5f, 10.0f);
				AABB boundingBox;
				boundingBox.minBounds = skeletonPosition + vec3(-2.0f, 0.0f, -2.0f);
				boundingBox.maxBounds = skeletonPosition + vec3(2.0f, 9.0f, 2.0f);

				if (AABBInFrustum(boundingBox, frustumPlanes))
				{
					if (!gHairTypeInfo[hairType].mInView)
					{
						gHairTypeInfo[hairType].mInView = true;
						gHairTypeInfo[hairType].mPreWarm = true;
					}

					gAnimatedObject[hairType].mRootTransform = mat4::translation(vec3(20.0f - hairType * 10.0f, -5.5f, 10.0f)) * mat4::scale(vec3(5.0f));
					if (!gAnimatedObject[hairType].Update(min(deltaTime, 1.0f / 60.0f)))
						LOGF(eINFO, "Animation NOT Updating!");
					gAnimatedObject[hairType].ComputePose(gAnimatedObject[hairType].mRootTransform);
				}
				else
					gHairTypeInfo[hairType].mInView = false;
			}

			// Update final capsules
			mat4 boneMatrix = mat4::identity();
			mat3 boneRotation = mat3::identity();
			gUniformDataHairGlobal.mTimeStep = 0.01f;

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				gFinalCapsules[hairType].resize(gCapsules.size());
				for (size_t i = 0; i < gCapsules.size(); ++i)
				{
					Capsule capsule = gCapsules[i].mCapsule;
					if (gCapsules[i].mAttachedBone != -1)
					{
						GetCorrectedBoneTranformation(hairType, gCapsules[i].mAttachedBone, &boneMatrix, &boneRotation);
						vec4 p0 = vec4(f3Tov3(capsule.mCenter0), 1.0f);
						vec4 p1 = vec4(f3Tov3(capsule.mCenter1), 1.0f);
						capsule.mCenter0 = v3ToF3((boneMatrix * p0).getXYZ());
						capsule.mCenter1 = v3ToF3((boneMatrix * p1).getXYZ());
					}
					gFinalCapsules[hairType][i] = capsule;
				}

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint           k = gHairTypeIndices[hairType][i];
					HairBuffer&    hair = gHair[k];
					NamedTransform namedTransform = gTransforms[gHair[k].mTransform];
					Transform      transform = namedTransform.mTransform;

					boneMatrix = mat4::identity();
					boneRotation = mat3::identity();

					if (namedTransform.mAttachedBone != -1)
						GetCorrectedBoneTranformation(hairType, namedTransform.mAttachedBone, &boneMatrix, &boneRotation);

					hair.mUniformDataHairShading.mTransform = mat4::identity();
					hair.mUniformDataHairShading.mStrandRadius = hair.mStrandRadius * transform.mScale;
					hair.mUniformDataHairShading.mStrandSpacing = hair.mStrandSpacing * transform.mScale;

					// Transform the hair to be centered around the origin in hair local space. Then transform it to follow the head.
					hair.mUniformDataHairSimulation.mTransform = boneMatrix * mat4::rotationZYX(transform.mOrientation) *
																 mat4::translation(transform.mPosition) *
																 mat4::scale(vec3(transform.mScale));
					hair.mUniformDataHairSimulation.mQuatRotation = Quat(boneRotation) * Quat(mat3::rotationZYX(transform.mOrientation));
					hair.mUniformDataHairSimulation.mScale = transform.mScale;
					for (uint j = 0; j < hair.mUniformDataHairSimulation.mCapsuleCount; ++j)
						hair.mUniformDataHairSimulation.mCapsules[j] = gFinalCapsules[hairType][hair.mCapsules[j]];
				}

				// Find head transform
				boneMatrix = mat4::identity();
				for (uint i = 0; i < (uint)gTransforms.size(); ++i)
				{
					if (strcmp(gTransforms[i].mName, "Head") == 0)
					{
						GetCorrectedBoneTranformation(hairType, gTransforms[i].mAttachedBone, &boneMatrix, &boneRotation);
						break;
					}
				}
				vec4        headPos = boneMatrix * vec4(-1.0f, 0.0f, 0.0f, 1.0f);
				const float shadowRange = 3.0f;
				mat4        orto = mat4::orthographic(-shadowRange, shadowRange, -shadowRange, shadowRange, -shadowRange, shadowRange);

				// Update hair shadow cameras
				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				{
					gUniformDataDirectionalLights.mDirectionalLights[i].mShadowMap = i;
					mat4 lookAt = mat4::lookAt(
						Point3(headPos.getXYZ()),
						Point3(headPos.getXYZ() + normalize(f3Tov3(gUniformDataDirectionalLights.mDirectionalLights[i].mDirection))),
						vec3(0.0f, 1.0f, 0.0f));
					gUniformDataCameraHairShadows[hairType][i].mProjectView = orto * lookAt;
					gUniformDataCameraHairShadows[hairType][i].mCamPos =
						headPos.getXYZ() - normalize(f3Tov3(gUniformDataDirectionalLights.mDirectionalLights[i].mDirection)) * 1000.0f;
					gUniformDataDirectionalLights.mDirectionalLights[i].mShadowRange = shadowRange * 2.0f;
				}
			}
		}
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		// FRAME SYNC
		//
		// This will acquire the next swapchain image
		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);
		resetCmdPool(pRenderer, pUICmdPools[gFrameIndex]);

		// SET CONSTANT BUFFERS
		//
		for (size_t totalBuf = 0; totalBuf < MATERIAL_INSTANCE_COUNT; ++totalBuf)
		{
			BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[gFrameIndex][totalBuf] };
			beginUpdateResource(&objBuffUpdateDesc);
			*(UniformObjData*)objBuffUpdateDesc.pMappedData = gUniformDataMatBall[totalBuf];
			endUpdateResource(&objBuffUpdateDesc, NULL);
		}

		// using the existing buffer for the shadow pass: &gUniformDataCamera -------------------------------------+
		// this will work as long as projView matrix is the first piece of data in &gUniformDataCamera             v
		//BufferUpdateDesc shadowMapCamBuffUpdatedesc = { pUniformBufferCameraShadowPass[gFrameIndex], &gUniformDataCamera };
		BufferUpdateDesc shadowMapCamBuffUpdatedesc = { pUniformBufferCameraShadowPass[gFrameIndex] };
		beginUpdateResource(&shadowMapCamBuffUpdatedesc);
		*(mat4*)shadowMapCamBuffUpdatedesc.pMappedData = gUniformDataDirectionalLights.mDirectionalLights[0].mViewProj;
		endUpdateResource(&shadowMapCamBuffUpdatedesc, NULL);

		BufferUpdateDesc camBuffUpdateDesc = { pUniformBufferCamera[gFrameIndex] };
		beginUpdateResource(&camBuffUpdateDesc);
		*(UniformCamData*)camBuffUpdateDesc.pMappedData = gUniformDataCamera;
		endUpdateResource(&camBuffUpdateDesc, NULL);

		BufferUpdateDesc skyboxViewProjCbv = { pUniformBufferCameraSkybox[gFrameIndex] };
		beginUpdateResource(&skyboxViewProjCbv);
		*(UniformCamData*)skyboxViewProjCbv.pMappedData = gUniformDataCameraSkybox;
		endUpdateResource(&skyboxViewProjCbv, NULL);

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			if (!gHairTypeInfo[hairType].mInView)
				continue;

			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
			{
				BufferUpdateDesc hairShadowBuffUpdateDesc = { pUniformBufferCameraHairShadows[gFrameIndex][hairType][i] };
				beginUpdateResource(&hairShadowBuffUpdateDesc);
				*(UniformCamDataShadow*)hairShadowBuffUpdateDesc.pMappedData = gUniformDataCameraHairShadows[hairType][i];
				endUpdateResource(&hairShadowBuffUpdateDesc, NULL);
			}
		}

		BufferUpdateDesc directionalLightsBufferUpdateDesc = { pUniformBufferDirectionalLights[gFrameIndex] };
		beginUpdateResource(&directionalLightsBufferUpdateDesc);
		*(UniformDataDirectionalLights*)directionalLightsBufferUpdateDesc.pMappedData = gUniformDataDirectionalLights;
		endUpdateResource(&directionalLightsBufferUpdateDesc, NULL);

		BufferUpdateDesc hairGlobalBufferUpdateDesc = { pUniformBufferHairGlobal };
		beginUpdateResource(&hairGlobalBufferUpdateDesc);
		*(UniformDataHairGlobal*)hairGlobalBufferUpdateDesc.pMappedData = gUniformDataHairGlobal;
		endUpdateResource(&hairGlobalBufferUpdateDesc, NULL);

		for (size_t i = 0; i < gHair.size(); ++i)
		{
			BufferUpdateDesc hairShadingBufferUpdateDesc = { gHair[i].pUniformBufferHairShading[gFrameIndex] };
			beginUpdateResource(&hairShadingBufferUpdateDesc);
			*(UniformDataHairShading*)hairShadingBufferUpdateDesc.pMappedData = gHair[i].mUniformDataHairShading;
			endUpdateResource(&hairShadingBufferUpdateDesc, NULL);

			BufferUpdateDesc hairSimulationBufferUpdateDesc = { gHair[i].pUniformBufferHairSimulation[gFrameIndex] };
			beginUpdateResource(&hairSimulationBufferUpdateDesc);
			*(UniformDataHairSimulation*)hairSimulationBufferUpdateDesc.pMappedData = gHair[i].mUniformDataHairSimulation;
			endUpdateResource(&hairSimulationBufferUpdateDesc, NULL);
		}

		if (gMaterialType == MATERIAL_HAIR)
			gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		// Draw
		eastl::vector<Cmd*> allCmds;
		Cmd*                cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		gCurrentGpuProfileToken = gMaterialType == MATERIAL_HAIR ? gHairGpuProfileToken : gMetalWoodGpuProfileToken;

		cmdBeginGpuFrameProfile(cmd, gCurrentGpuProfileToken);

		RenderTargetBarrier barriers[] = { { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
										   { pRenderTargetShadowMap, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE } };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, barriers);

		// DRAW DIRECTIONAL SHADOW MAP
		//
		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Shadow Pass");

		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 1.0f;
		loadActions.mClearDepth.stencil = 0;

		cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetShadowMap, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadowMap->mWidth, (float)pRenderTargetShadowMap->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTargetShadowMap->mWidth, pRenderTargetShadowMap->mHeight);
		cmdBindPipeline(cmd, pPipelineShadowPass);

		if (gMaterialType != MATERIAL_HAIR)
		{
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetShadow[0]);

			// DRAW THE GROUND
			//
			cmdBindVertexBuffer(cmd, 1, &gMeshes[MESH_CUBE]->pVertexBuffers[0], &gMeshes[MESH_CUBE]->mVertexStrides[0], NULL);
			cmdBindIndexBuffer(cmd, gMeshes[MESH_CUBE]->pIndexBuffer, gMeshes[MESH_CUBE]->mIndexType, 0);

			cmdBindDescriptorSet(cmd, 0, pDescriptorSetShadow[1]);
			cmdDrawIndexed(cmd, gMeshes[MESH_CUBE]->mIndexCount, 0, 0);

			// DRAW THE LABEL PLATES
			//
			for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			{
				cmdBindDescriptorSet(cmd, 1 + j, pDescriptorSetShadow[1]);
				cmdDrawIndexed(cmd, gMeshes[MESH_CUBE]->mIndexCount, 0, 0);
			}

			// DRAW THE MATERIAL BALLS
			//
			cmdBindVertexBuffer(cmd, 1, &gMeshes[MESH_MAT_BALL]->pVertexBuffers[0], &gMeshes[MESH_MAT_BALL]->mVertexStrides[0], NULL);
			cmdBindIndexBuffer(cmd, gMeshes[MESH_MAT_BALL]->pIndexBuffer, gMeshes[MESH_MAT_BALL]->mIndexType, 0);
			for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				cmdBindDescriptorSet(
					cmd, 1 + MATERIAL_INSTANCE_COUNT + (gFrameIndex * MATERIAL_INSTANCE_COUNT + i), pDescriptorSetShadow[1]);
				cmdDrawIndexed(cmd, gMeshes[MESH_MAT_BALL]->mIndexCount, 0, 0);
			}
		}

		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);    // Shadow Pass

		// DRAW SKYBOX
		//
		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Skybox Pass");
		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 1.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		if (gDrawSkybox)    // TODO: do we need this condition?
		{
			const uint32_t skyboxStride = sizeof(float) * 4;
			cmdBindPipeline(cmd, pPipelineSkybox);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetSkybox[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetSkybox[1]);
			cmdBindVertexBuffer(cmd, 1, &pVertexBufferSkybox, &skyboxStride, NULL);
			cmdDraw(cmd, 36, 0);
		}
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);    // Skybox Pass

		// DRAW THE OBJECTS W/ MATERIALS
		//
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

		RenderTargetBarrier shadowTexBarrier = { pRenderTargetShadowMap, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &shadowTexBarrier);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
		cmdBindPipeline(cmd, pPipelineBRDF);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetBRDF[0]);
		cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetBRDF[1]);

		// DRAW THE GROUND PLANE
		//
		cmdBindVertexBuffer(cmd, 1, &gMeshes[MESH_CUBE]->pVertexBuffers[0], &gMeshes[MESH_CUBE]->mVertexStrides[0], NULL);
		cmdBindIndexBuffer(cmd, gMeshes[MESH_CUBE]->pIndexBuffer, gMeshes[MESH_CUBE]->mIndexType, 0);
		cmdBindDescriptorSet(cmd, 0, pDescriptorSetBRDF[2]);
		cmdDrawIndexed(cmd, gMeshes[MESH_CUBE]->mIndexCount, 0, 0);

		// DRAW THE OBJECTS W/ MATERIALS
		//
		if (gMaterialType != MATERIAL_HAIR)
		{
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Lighting Pass");

			// DRAW THE LABEL PLATES
			//
			for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			{
				cmdBindDescriptorSet(cmd, 1 + j, pDescriptorSetBRDF[2]);
				cmdDrawIndexed(cmd, gMeshes[MESH_CUBE]->mIndexCount, 0, 0);
			}

			cmdBindVertexBuffer(cmd, 1, &gMeshes[MESH_MAT_BALL]->pVertexBuffers[0], &gMeshes[MESH_MAT_BALL]->mVertexStrides[0], NULL);
			cmdBindIndexBuffer(cmd, gMeshes[MESH_MAT_BALL]->pIndexBuffer, gMeshes[MESH_MAT_BALL]->mIndexType, 0);

			// DRAW THE MATERIAL BALLS
			//
#if 1    // toggle for rendering objects
			for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				const uint32_t index = 1 + MATERIAL_INSTANCE_COUNT + (gFrameIndex * MATERIAL_BRDF_COUNT * MATERIAL_INSTANCE_COUNT) +
									   (GuiController::currentMaterialType * MATERIAL_INSTANCE_COUNT) + i;
				cmdBindDescriptorSet(cmd, index, pDescriptorSetBRDF[2]);
				cmdDrawIndexed(cmd, gMeshes[MESH_MAT_BALL]->mIndexCount, 0, 0);
			}
#endif

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);    // Lighting Pass
		}

		// Draw hair
		else    // gMaterialType == MATERIAL_HAIR
		{
			//// draw the skeleton of the rig
			gSkeletonBatcher.Draw(cmd, gFrameIndex);

			uint32_t descriptorSetIndex = gFrameIndex * gHairDynamicDescriptorSetCount;

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			// Hair simulation
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair simulation");
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
				{
					descriptorSetIndex += (uint32_t)gHairTypeIndices[hairType].size();
					continue;
				}

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint        k = gHairTypeIndices[hairType][i];
					HairBuffer& hair = gHair[k];

					uint dispatchGroupCountPerVertex =
						hair.mTotalVertexCount / 64 / (hair.mUniformDataHairSimulation.mNumFollowHairsPerGuideHair + 1);
					uint dispatchGroupCountPerStrand = hair.mNumGuideStrands / 64;

					BufferBarrier bufferBarriers[3] = {};
					for (int j = 0; j < 3; ++j)
					{
						bufferBarriers[j].pBuffer = hair.pBufferHairSimulationVertexPositions[j];
						bufferBarriers[j].mCurrentState = bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
					}
					cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);

					if (gFirstHairSimulationFrame || gHairTypeInfo[hairType].mPreWarm)
					{
						cmdBindPipeline(cmd, pPipelineHairPreWarm);
						cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairPreWarm);

						cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = hair.pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mCurrentState = bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						}
						cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);
					}

					cmdBindPipeline(cmd, pPipelineHairIntegrate);
					cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairIntegrate);
					cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

					for (int j = 0; j < 3; ++j)
					{
						bufferBarriers[j].pBuffer = hair.pBufferHairSimulationVertexPositions[j];
						bufferBarriers[j].mCurrentState = bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
					}
					cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);

					if (hair.mUniformDataHairSimulation.mShockPropagationStrength > 0.0f)
					{
						cmdBindPipeline(cmd, pPipelineHairShockPropagation);
						cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairShockPropagate);
						cmdDispatch(cmd, dispatchGroupCountPerStrand, 1, 1);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = hair.pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mCurrentState = bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						}
						cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);
					}

					if (hair.mUniformDataHairSimulation.mLocalConstraintIterations > 0 &&
						hair.mUniformDataHairSimulation.mLocalStiffness > 0.0f)
					{
						cmdBindPipeline(cmd, pPipelineHairLocalConstraints);
						cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairLocalConstraints);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = hair.pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mCurrentState = bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						}

						for (int j = 0; j < (int)hair.mUniformDataHairSimulation.mLocalConstraintIterations; ++j)
						{
							cmdDispatch(cmd, dispatchGroupCountPerStrand, 1, 1);
							cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, 0, NULL);
						}
					}

					cmdBindPipeline(cmd, pPipelineHairLengthConstraints);

					bufferBarriers[0].pBuffer = hair.pBufferHairVertexTangents;
					bufferBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
					bufferBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
					cmdResourceBarrier(cmd, 1, bufferBarriers, 0, NULL, 0, NULL);

					cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairLengthConstraints);
					cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

					bufferBarriers[0].pBuffer = hair.pBufferHairSimulationVertexPositions[0];
					bufferBarriers[0].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
					bufferBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
					bufferBarriers[1].pBuffer = hair.pBufferHairVertexTangents;
					bufferBarriers[1].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
					bufferBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
					cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);

					// Update follow hairs
					if (hair.mUniformDataHairSimulation.mNumFollowHairsPerGuideHair > 0)
					{
						cmdBindPipeline(cmd, pPipelineHairUpdateFollowHairs);

						bufferBarriers[0].pBuffer = hair.pBufferHairVertexTangents;
						bufferBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
						bufferBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						cmdResourceBarrier(cmd, 1, bufferBarriers, 0, NULL, 0, NULL);

						cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairFollowHairs);
						cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

						bufferBarriers[0].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[0];
						bufferBarriers[0].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
						bufferBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						bufferBarriers[1].pBuffer = gHair[k].pBufferHairVertexTangents;
						bufferBarriers[1].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
						bufferBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
						cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, 0, NULL);
					}

					++descriptorSetIndex;
				}

				gHairTypeInfo[hairType].mPreWarm = false;
			}
			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			// Draw hair - shadow map
			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair rendering");

			uint32_t            shadowDescriptorSetIndex[2] = { gFrameIndex * MAX_NUM_DIRECTIONAL_LIGHTS * HAIR_TYPE_COUNT,
                                                     gFrameIndex * gHairDynamicDescriptorSetCount * MAX_NUM_DIRECTIONAL_LIGHTS };
			RenderTargetBarrier rtBarriers[2] = {};
#if defined(METAL)
			BufferBarrier bufferBarrier[1] = {};
#endif

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair shadow");

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
				{
					shadowDescriptorSetIndex[0] += MAX_NUM_DIRECTIONAL_LIGHTS;
					shadowDescriptorSetIndex[1] += MAX_NUM_DIRECTIONAL_LIGHTS * (uint32_t)gHairTypeIndices[hairType].size();
					continue;
				}

				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				{
					cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
					rtBarriers[0].pRenderTarget = pRenderTargetHairShadows[hairType][i];
					rtBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
					rtBarriers[0].mNewState = RESOURCE_STATE_DEPTH_WRITE;
					cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);

					loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
					loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
					loadActions.mClearDepth = pRenderTargetHairShadows[hairType][i]->mClearValue;

					cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetHairShadows[hairType][i], &loadActions, NULL, NULL, -1, -1);
					cmdSetViewport(
						cmd, 0.0f, 0.0f, (float)pRenderTargetHairShadows[hairType][i]->mWidth,
						(float)pRenderTargetHairShadows[hairType][i]->mHeight, 0.0f, 1.0f);
					cmdSetScissor(cmd, 0, 0, pRenderTargetHairShadows[hairType][i]->mWidth, pRenderTargetHairShadows[hairType][i]->mHeight);

					cmdBindPipeline(cmd, pPipelineHairShadow);
					cmdBindDescriptorSet(cmd, shadowDescriptorSetIndex[0], pDescriptorSetHairShadow[0]);

					for (size_t j = 0; j < gHairTypeIndices[hairType].size(); ++j)
					{
						uint k = gHairTypeIndices[hairType][j];

						cmdBindDescriptorSet(cmd, shadowDescriptorSetIndex[1], pDescriptorSetHairShadow[1]);
						cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, gHair[k].pGeom->mIndexType, 0);
						cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);

						++shadowDescriptorSetIndex[1];
					}

					++shadowDescriptorSetIndex[0];
				}
			}

			// Draw hair - clear hair depths texture
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

#ifndef METAL
            TextureBarrier textureBarriers[2] = {};
			textureBarriers[0].pTexture = pTextureHairDepth;
			textureBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
			textureBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);
#else
			bufferBarrier[0].pBuffer = pBufferHairDepth;
			bufferBarrier[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
			bufferBarrier[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			cmdResourceBarrier(cmd, 1, bufferBarrier, 0, NULL, 0, NULL);
#endif

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair clear");

			loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
			cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBindPipeline(cmd, pPipelineHairClear);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetHairClear);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair depth peeling");

			// Draw hair - depth peeling and alpha accumulaiton
			rtBarriers[0].pRenderTarget = pRenderTargetDepthPeeling;
			rtBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
			rtBarriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);

			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = pRenderTargetDepthPeeling->mClearValue;
			loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;    //-V1048

			cmdBindRenderTargets(cmd, 1, &pRenderTargetDepthPeeling, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(
				cmd, 0.0f, 0.0f, (float)pRenderTargetDepthPeeling->mWidth, (float)pRenderTargetDepthPeeling->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetDepthPeeling->mWidth, pRenderTargetDepthPeeling->mHeight);

			cmdBindPipeline(cmd, pPipelineHairDepthPeeling);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetHairDepthPeeling[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetHairDepthPeeling[1]);

			descriptorSetIndex = gFrameIndex * gHairDynamicDescriptorSetCount;

			for (uint32_t hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
				{
					descriptorSetIndex += (uint32_t)gHairTypeIndices[hairType].size();
					continue;
				}

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint32_t k = gHairTypeIndices[hairType][i];
					cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairDepthPeeling[2]);
					cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, gHair[k].pGeom->mIndexType, 0);
					cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);

					++descriptorSetIndex;
				}
			}

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair depth resolve");

			// Draw hair - depth resolve
#ifndef METAL
			textureBarriers[0].pTexture = pTextureHairDepth;
			textureBarriers[0].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
			textureBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, 0, NULL);
#else
			bufferBarrier[0].pBuffer = pBufferHairDepth;
			bufferBarrier[0].mCurrentState = RESOURCE_STATE_UNORDERED_ACCESS;
			bufferBarrier[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 1, bufferBarrier, 0, NULL, 0, NULL);
#endif

			cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdBindPipeline(cmd, pPipelineHairDepthResolve);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetHairDepthResolve);
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair fill colors");

			// Draw hair - fill colors
			rtBarriers[0].pRenderTarget = pRenderTargetFillColors;
			rtBarriers[0].mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
			rtBarriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, rtBarriers);

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				{
					rtBarriers[i].pRenderTarget = pRenderTargetHairShadows[hairType][i];
					rtBarriers[i].mCurrentState = RESOURCE_STATE_DEPTH_WRITE;
					rtBarriers[i].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
				}
				cmdResourceBarrier(cmd, 0, NULL, 0, NULL, MAX_NUM_DIRECTIONAL_LIGHTS, rtBarriers);
			}

			loadActions.mClearColorValues[0] = pRenderTargetFillColors->mClearValue;

			cmdBindRenderTargets(cmd, 1, &pRenderTargetFillColors, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetFillColors->mWidth, (float)pRenderTargetFillColors->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetFillColors->mWidth, pRenderTargetFillColors->mHeight);

			cmdBindPipeline(cmd, pPipelineHairFillColors);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetHairFillColors[0]);
			cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetHairFillColors[1]);

			descriptorSetIndex = gFrameIndex * gHairDynamicDescriptorSetCount;

			for (uint32_t hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
				{
					descriptorSetIndex += (uint32_t)gHairTypeIndices[hairType].size();
					continue;
				}

				cmdBindDescriptorSet(cmd, gFrameIndex * HAIR_TYPE_COUNT + hairType, pDescriptorSetHairFillColors[2]);

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint32_t k = gHairTypeIndices[hairType][i];

					cmdBindDescriptorSet(cmd, descriptorSetIndex, pDescriptorSetHairFillColors[3]);
					cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, gHair[k].pGeom->mIndexType, 0);
					cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);

					++descriptorSetIndex;
				}
			}

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Hair resolve colors");

			// Draw hair - color resolve
			rtBarriers[0].pRenderTarget = pRenderTargetFillColors;
			rtBarriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
			rtBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			rtBarriers[1].pRenderTarget = pRenderTargetDepthPeeling;
			rtBarriers[1].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
			rtBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 2, rtBarriers);

			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

			cmdBindPipeline(cmd, pPipelineHairColorResolve);
			cmdBindDescriptorSet(cmd, 0, pDescriptorSetHairColorResolve);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

			cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);

#if HAIR_MAX_CAPSULE_COUNT > 0
			if (gShowCapsules)
			{
				cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

				cmdBindPipeline(cmd, pPipelineShowCapsules);
				cmdBindVertexBuffer(cmd, 1, &gMeshes[MESH_CAPSULE]->pVertexBuffers[0], &gMeshes[MESH_CAPSULE]->mVertexStrides[0], NULL);
				cmdBindIndexBuffer(cmd, gMeshes[MESH_CAPSULE]->pIndexBuffer, gMeshes[MESH_CAPSULE]->mIndexType, 0);
				cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetShowCapsule);

				uint32_t capsuleRootConstantIndex = getDescriptorIndexFromName(pRootSignatureShowCapsules, "CapsuleRootConstant");
				for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
				{
					for (size_t i = 0; i < gCapsules.size(); ++i)
					{
						cmdBindPushConstants(cmd, pRootSignatureShowCapsules, capsuleRootConstantIndex, &gFinalCapsules[hairType][i]);
						cmdDrawIndexed(cmd, gMeshes[MESH_CAPSULE]->mIndexCount, 0, 0);
					}
				}
			}
#endif
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);

			gFirstHairSimulationFrame = false;
		}

		endCmd(cmd);
		allCmds.push_back(cmd);

		// SET UP DRAW COMMANDS (UI)
		//
		cmd = pUICmds[gFrameIndex];
		beginCmd(cmd);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;    //-V1048
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

		// draw world-space text
		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "Text");
		const char** ppMaterialNames = NULL;
		switch (GuiController::currentMaterialType)
		{
			case MATERIAL_METAL: ppMaterialNames = metalEnumNames; break;
			case MATERIAL_WOOD:
				ppMaterialNames = woodEnumNames;
				break;

				// we don't use name plates for hair material, hairEnumNames are removed.
				//
				//case MATERIAL_HAIR:
				//	ppMaterialNames = hairEnumNames;
				//	break;

			default: ppMaterialNames = metalEnumNames; break;
		}

		if (GuiController::currentMaterialType != MATERIAL_HAIR)
		{
			for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				gMaterialPropDraw.pText = ppMaterialNames[i];
				gMaterialPropDraw.mFontColor = 0xffaaaaaa;
				gMaterialPropDraw.mFontSize = 32.0f; 
				cmdDrawWorldSpaceTextWithFont(cmd, &gTextWorldMats[i], &gTextProjView, &gMaterialPropDraw);
			}
		}

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);

		// draw HUD text
		float2 screenCoords = float2(8, 15);

		gFrameTimeDraw.mFontColor = 0xff00ff00;
		gFrameTimeDraw.mFontSize = 18.0f;
		float2 txtSize = cmdDrawCpuProfile(cmd, screenCoords, &gFrameTimeDraw);

		screenCoords = float2(8.0f, txtSize.y + 75.f);
		cmdDrawGpuProfile(cmd, screenCoords, gCurrentGpuProfileToken, &gFrameTimeDraw);

		if (!gbLuaScriptingSystemLoadedSuccessfully)
		{
			gErrMsgDrawDesc.pText = "Error loading LUA scripts!";
			gErrMsgDrawDesc.mFontColor = 0xff0000ee;
			gErrMsgDrawDesc.mFontSize = 18.0f;
			cmdDrawTextWithFont(cmd, screenCoords, &gErrMsgDrawDesc);
		}
		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);    // HUD Text

		cmdBeginGpuTimestampQuery(cmd, gCurrentGpuProfileToken, "UI");

		cmdDrawUserInterface(cmd);
		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, gCurrentGpuProfileToken);    // UI

		// PRESENT THE GFX QUEUE
		//
		// Transition our texture to present state
		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
		cmdEndGpuFrameProfile(cmd, gCurrentGpuProfileToken);
		endCmd(cmd);
		allCmds.push_back(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = (uint32_t)allCmds.size();
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = allCmds.data();
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
        getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.mSubmitDone = true;

		// Capture screenshot before presentation.
		if (gTakeScreenshot)
		{
			// Metal platforms need one renderpass to prepare the swapchain textures for copy.
			if (prepareScreenshot(pSwapChain))
			{
				captureScreenshot(pSwapChain, swapchainImageIndex, RESOURCE_STATE_PRESENT, "06_Material_Screenshot.png");
				gTakeScreenshot = false;
			}
		}

		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "06_MaterialPlayground"; }

	void GetCorrectedBoneTranformation(uint rigIndex, uint boneIndex, mat4* boneMatrix, mat3* boneRotation)
	{
		(*boneMatrix) = gAnimatedObject[rigIndex].mJointWorldMats[boneIndex];

		// Get skeleton scale. Assumes uniform scaling.
		float boneScale = length(((*boneMatrix) * vec4(1.0f, 0.0f, 0.0f, 0.0f)).getXYZ());

		// Get bone position
		vec3 bonePosition = ((*boneMatrix) * vec4(0.0f, 0.0f, 0.0f, 1.0f)).getXYZ();

		// Get bone rotation
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
				(*boneRotation)[i][j] = (*boneMatrix)[i][j];
		}

		// Take scale out of rotation matrix
		*boneRotation = (1.0f / boneScale) * (*boneRotation);

		// Create new bone matrix without scale, with fixed rotations
		(*boneMatrix) = mat4((*boneRotation), bonePosition);
	}

	//--------------------------------------------------------------------------------------------
	// INIT FUNCTIONS
	//--------------------------------------------------------------------------------------------
	void addSamplers()
	{
		SamplerDesc bilinearSamplerDesc = {};
		bilinearSamplerDesc.mMinFilter = FILTER_LINEAR;
		bilinearSamplerDesc.mMagFilter = FILTER_LINEAR;
		bilinearSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		bilinearSamplerDesc.mAddressU = ADDRESS_MODE_REPEAT;
		bilinearSamplerDesc.mAddressV = ADDRESS_MODE_REPEAT;
		bilinearSamplerDesc.mAddressW = ADDRESS_MODE_REPEAT;
		addSampler(pRenderer, &bilinearSamplerDesc, &pSamplerBilinear);

		SamplerDesc bilinearClampedSamplerDesc = {};
		bilinearClampedSamplerDesc.mMinFilter = FILTER_LINEAR;
		bilinearClampedSamplerDesc.mMagFilter = FILTER_LINEAR;
		bilinearClampedSamplerDesc.mMipMapMode = MIPMAP_MODE_LINEAR;
		bilinearClampedSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_EDGE;
		bilinearClampedSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_EDGE;
		bilinearClampedSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_EDGE;
		addSampler(pRenderer, &bilinearClampedSamplerDesc, &pSamplerBilinearClamped);

		SamplerDesc pointSamplerDesc = {};
		pointSamplerDesc.mMinFilter = FILTER_NEAREST;
		pointSamplerDesc.mMagFilter = FILTER_NEAREST;
		pointSamplerDesc.mMipMapMode = MIPMAP_MODE_NEAREST;
		pointSamplerDesc.mAddressU = ADDRESS_MODE_CLAMP_TO_BORDER;
		pointSamplerDesc.mAddressV = ADDRESS_MODE_CLAMP_TO_BORDER;
		pointSamplerDesc.mAddressW = ADDRESS_MODE_CLAMP_TO_BORDER;
		addSampler(pRenderer, &pointSamplerDesc, &pSamplerPoint);
	}

	void removeSamplers()
	{
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerBilinearClamped);
		removeSampler(pRenderer, pSamplerPoint);
	}

	void addShaders()
	{
		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

		ShaderLoadDesc shadowPassShaderDesc = {};
		shadowPassShaderDesc.mStages[0] = { "renderSceneShadows.vert", 0 };
		shadowPassShaderDesc.mStages[1] = { "renderSceneShadows.frag", 0 };
		addShader(pRenderer, &shadowPassShaderDesc, &pShaderShadowPass);

		ShaderLoadDesc hairClearShaderDesc = {};
		hairClearShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		hairClearShaderDesc.mStages[1] = { "hair_short_cut_clear.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &hairClearShaderDesc, &pShaderHairClear);

		ShaderLoadDesc hairDepthPeelingShaderDesc = {};
		hairDepthPeelingShaderDesc.mStages[0] = { "hair.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		hairDepthPeelingShaderDesc.mStages[1] = { "hair_short_cut_depth_peeling.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &hairDepthPeelingShaderDesc, &pShaderHairDepthPeeling);

		ShaderLoadDesc hairDepthResolveShaderDesc = {};
		hairDepthResolveShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		hairDepthResolveShaderDesc.mStages[1] = { "hair_short_cut_resolve_depth.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &hairDepthResolveShaderDesc, &pShaderHairDepthResolve);

		ShaderLoadDesc hairFillColorShaderDesc = {};
		hairFillColorShaderDesc.mStages[0] = { "hair.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		hairFillColorShaderDesc.mStages[1] = { "hair_short_cut_fill_color.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &hairFillColorShaderDesc, &pShaderHairFillColors);

		ShaderLoadDesc hairColorResolveShaderDesc = {};
		hairColorResolveShaderDesc.mStages[0] = { "fullscreen.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		hairColorResolveShaderDesc.mStages[1] = { "hair_short_cut_resolve_color.frag", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &hairColorResolveShaderDesc, &pShaderHairResolveColor);

		ShaderLoadDesc hairShadowShaderDesc = {};
		hairShadowShaderDesc.mStages[0] = { "hair_shadow.vert", NULL, 0 };
		hairShadowShaderDesc.mStages[1] = { "hair_shadow.frag", NULL, 0 };
		addShader(pRenderer, &hairShadowShaderDesc, &pShaderHairShadow);

		ShaderLoadDesc hairIntegrateShaderDesc = {};
		hairIntegrateShaderDesc.mStages[0] = { "hair_integrate.comp", NULL, 0 };
		addShader(pRenderer, &hairIntegrateShaderDesc, &pShaderHairIntegrate);

		ShaderLoadDesc hairShockPropagationShaderDesc = {};
		hairShockPropagationShaderDesc.mStages[0] = { "hair_shock_propagation.comp", NULL, 0 };
		addShader(pRenderer, &hairShockPropagationShaderDesc, &pShaderHairShockPropagation);

		ShaderLoadDesc hairLocalConstraintsShaderDesc = {};
		hairLocalConstraintsShaderDesc.mStages[0] = { "hair_local_constraints.comp", NULL, 0 };
		addShader(pRenderer, &hairLocalConstraintsShaderDesc, &pShaderHairLocalConstraints);

		ShaderLoadDesc hairLengthConstraintsShaderDesc = {};
		hairLengthConstraintsShaderDesc.mStages[0] = { "hair_length_constraints.comp", NULL, 0 };
		addShader(pRenderer, &hairLengthConstraintsShaderDesc, &pShaderHairLengthConstraints);

		ShaderLoadDesc hairUpdateFollowHairsShaderDesc = {};
		hairUpdateFollowHairsShaderDesc.mStages[0] = { "hair_update_follow_hairs.comp", NULL, 0 };
		addShader(pRenderer, &hairUpdateFollowHairsShaderDesc, &pShaderHairUpdateFollowHairs);

		ShaderLoadDesc hairPreWarmShaderDesc = {};
		hairPreWarmShaderDesc.mStages[0] = { "hair_pre_warm.comp", NULL, 0 };
		addShader(pRenderer, &hairPreWarmShaderDesc, &pShaderHairPreWarm);

		ShaderLoadDesc showCapsulesShaderDesc = {};
		showCapsulesShaderDesc.mStages[0] = { "showCapsules.vert", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		showCapsulesShaderDesc.mStages[1] = { "showCapsules.frag", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &showCapsulesShaderDesc, &pShaderShowCapsules);

		ShaderLoadDesc skeletonShaderDesc = {};
		skeletonShaderDesc.mStages[0] = { "skeleton.vert", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skeletonShaderDesc.mStages[1] = { "skeleton.frag", 0, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		addShader(pRenderer, &skeletonShaderDesc, &pShaderSkeleton);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pShaderSkybox);
		removeShader(pRenderer, pShaderShadowPass);

		removeShader(pRenderer, pShaderHairClear);
		removeShader(pRenderer, pShaderHairDepthPeeling);
		removeShader(pRenderer, pShaderHairDepthResolve);
		removeShader(pRenderer, pShaderHairFillColors);
		removeShader(pRenderer, pShaderHairResolveColor);
		removeShader(pRenderer, pShaderHairIntegrate);
		removeShader(pRenderer, pShaderHairShockPropagation);
		removeShader(pRenderer, pShaderHairLocalConstraints);
		removeShader(pRenderer, pShaderHairLengthConstraints);
		removeShader(pRenderer, pShaderHairUpdateFollowHairs);
		removeShader(pRenderer, pShaderHairPreWarm);
		removeShader(pRenderer, pShaderShowCapsules);
		removeShader(pRenderer, pShaderSkeleton);
		removeShader(pRenderer, pShaderHairShadow);
	}

	void addRootSignatures()
	{
		const char* pStaticSamplerNames[] = { "bilinearSampler", "bilinearClampedSampler", "skyboxSampler", "PointSampler" };
		Sampler*    pStaticSamplers[] = { pSamplerBilinear, pSamplerBilinearClamped, pSamplerBilinear, pSamplerPoint };
		uint        numStaticSamplers = sizeof(pStaticSamplerNames) / sizeof(pStaticSamplerNames[0]);

		RootSignatureDesc skyboxRootDesc = { &pShaderSkybox, 1 };
		skyboxRootDesc.mStaticSamplerCount = numStaticSamplers;
		skyboxRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		skyboxRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &skyboxRootDesc, &pRootSignatureSkybox);

		RootSignatureDesc brdfRootDesc = { &pShaderBRDF, 1 };
		brdfRootDesc.mStaticSamplerCount = numStaticSamplers;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = pStaticSamplers;
		addRootSignature(pRenderer, &brdfRootDesc, &pRootSignatureBRDF);

		RootSignatureDesc shadowPassRootDesc = { &pShaderShadowPass, 1 };
		shadowPassRootDesc.mStaticSamplerCount = 0;
		addRootSignature(pRenderer, &shadowPassRootDesc, &pRootSignatureShadowPass);

		RootSignatureDesc hairClearRootSignatureDesc = {};
		hairClearRootSignatureDesc.ppShaders = &pShaderHairClear;
		hairClearRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairClearRootSignatureDesc, &pRootSignatureHairClear);

		RootSignatureDesc hairDepthPeelingRootSignatureDesc = {};
		hairDepthPeelingRootSignatureDesc.mMaxBindlessTextures = MAX_NUM_DIRECTIONAL_LIGHTS;
		hairDepthPeelingRootSignatureDesc.ppShaders = &pShaderHairDepthPeeling;
		hairDepthPeelingRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairDepthPeelingRootSignatureDesc, &pRootSignatureHairDepthPeeling);

		RootSignatureDesc hairResolveDepthRootSignatureDesc = {};
		hairResolveDepthRootSignatureDesc.ppShaders = &pShaderHairDepthResolve;
		hairResolveDepthRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairResolveDepthRootSignatureDesc, &pRootSignatureHairDepthResolve);

		RootSignatureDesc hairFillColorsRootSignatureDesc = {};
		hairFillColorsRootSignatureDesc.mMaxBindlessTextures = MAX_NUM_DIRECTIONAL_LIGHTS;
		hairFillColorsRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
		hairFillColorsRootSignatureDesc.ppStaticSamplerNames = pStaticSamplerNames;
		hairFillColorsRootSignatureDesc.ppStaticSamplers = pStaticSamplers;
		hairFillColorsRootSignatureDesc.ppShaders = &pShaderHairFillColors;
		hairFillColorsRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairFillColorsRootSignatureDesc, &pRootSignatureHairFillColors);

		RootSignatureDesc hairColorResolveRootSignatureDesc = {};
		hairColorResolveRootSignatureDesc.ppShaders = &pShaderHairResolveColor;
		hairColorResolveRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairColorResolveRootSignatureDesc, &pRootSignatureHairColorResolve);

		RootSignatureDesc hairIntegrateRootSignatureDesc = {};
		hairIntegrateRootSignatureDesc.ppShaders = &pShaderHairIntegrate;
		hairIntegrateRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairIntegrateRootSignatureDesc, &pRootSignatureHairIntegrate);

		RootSignatureDesc hairShockPropagationRootSignatureDesc = {};
		hairShockPropagationRootSignatureDesc.ppShaders = &pShaderHairShockPropagation;
		hairShockPropagationRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairShockPropagationRootSignatureDesc, &pRootSignatureHairShockPropagation);

		RootSignatureDesc hairLocalConstraintsRootSignatureDesc = {};
		hairLocalConstraintsRootSignatureDesc.ppShaders = &pShaderHairLocalConstraints;
		hairLocalConstraintsRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairLocalConstraintsRootSignatureDesc, &pRootSignatureHairLocalConstraints);

		RootSignatureDesc hairLengthConstraintsRootSignatureDesc = {};
		hairLengthConstraintsRootSignatureDesc.ppShaders = &pShaderHairLengthConstraints;
		hairLengthConstraintsRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairLengthConstraintsRootSignatureDesc, &pRootSignatureHairLengthConstraints);

		RootSignatureDesc hairUpdateFollowHairsRootSignatureDesc = {};
		hairUpdateFollowHairsRootSignatureDesc.ppShaders = &pShaderHairUpdateFollowHairs;
		hairUpdateFollowHairsRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairUpdateFollowHairsRootSignatureDesc, &pRootSignatureHairUpdateFollowHairs);

		RootSignatureDesc hairPreWarmRootSignatureDesc = {};
		hairPreWarmRootSignatureDesc.ppShaders = &pShaderHairPreWarm;
		hairPreWarmRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairPreWarmRootSignatureDesc, &pRootSignatureHairPreWarm);

		RootSignatureDesc showCapsulesRootSignatureDesc = {};
		showCapsulesRootSignatureDesc.ppShaders = &pShaderShowCapsules;
		showCapsulesRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &showCapsulesRootSignatureDesc, &pRootSignatureShowCapsules);

		RootSignatureDesc skeletonRootSignatureDesc = {};
		skeletonRootSignatureDesc.ppShaders = &pShaderSkeleton;
		skeletonRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &skeletonRootSignatureDesc, &pRootSignatureSkeleton);

		RootSignatureDesc hairShadowRootSignatureDesc = {};
		hairShadowRootSignatureDesc.mMaxBindlessTextures = MAX_NUM_DIRECTIONAL_LIGHTS;
		hairShadowRootSignatureDesc.ppShaders = &pShaderHairShadow;
		hairShadowRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairShadowRootSignatureDesc, &pRootSignatureHairShadow);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureBRDF);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
		removeRootSignature(pRenderer, pRootSignatureShadowPass);

		removeRootSignature(pRenderer, pRootSignatureHairClear);
		removeRootSignature(pRenderer, pRootSignatureHairDepthPeeling);
		removeRootSignature(pRenderer, pRootSignatureHairDepthResolve);
		removeRootSignature(pRenderer, pRootSignatureHairFillColors);
		removeRootSignature(pRenderer, pRootSignatureHairColorResolve);
		removeRootSignature(pRenderer, pRootSignatureHairIntegrate);
		removeRootSignature(pRenderer, pRootSignatureHairShockPropagation);
		removeRootSignature(pRenderer, pRootSignatureHairLocalConstraints);
		removeRootSignature(pRenderer, pRootSignatureHairLengthConstraints);
		removeRootSignature(pRenderer, pRootSignatureHairUpdateFollowHairs);
		removeRootSignature(pRenderer, pRootSignatureHairPreWarm);
		removeRootSignature(pRenderer, pRootSignatureShowCapsules);
		removeRootSignature(pRenderer, pRootSignatureSkeleton);
		removeRootSignature(pRenderer, pRootSignatureHairShadow);
	}

	void addDescriptorSets()
	{
		gSkeletonBatcher.AddDescriptorSets(pRootSignatureSkeleton);

		for (uint32_t i = 0; i < HAIR_TYPE_COUNT; i++)
			gHairDynamicDescriptorSetCount += (uint32_t)gHairTypeIndices[i].size();

		DescriptorSetDesc setDesc = { pRootSignatureShadowPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetShadow[0]);
		setDesc = { pRootSignatureShadowPass, DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
					(1 + MATERIAL_INSTANCE_COUNT) + (gImageCount * MATERIAL_INSTANCE_COUNT) };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetShadow[1]);
		setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
		setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
		setDesc = { pRootSignatureBRDF, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[0]);
		setDesc = { pRootSignatureBRDF, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[1]);
		setDesc = { pRootSignatureBRDF, DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
					(1 + MATERIAL_INSTANCE_COUNT) + (gImageCount * MATERIAL_BRDF_COUNT * MATERIAL_INSTANCE_COUNT) };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF[2]);

		// Hair Simulation
		setDesc = { pRootSignatureHairClear, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairClear);
		setDesc = { pRootSignatureHairPreWarm, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairPreWarm);
		setDesc = { pRootSignatureHairIntegrate, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairIntegrate);
		setDesc = { pRootSignatureHairShockPropagation, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairShockPropagate);
		setDesc = { pRootSignatureHairLocalConstraints, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairLocalConstraints);
		setDesc = { pRootSignatureHairLengthConstraints, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairLengthConstraints);
		setDesc = { pRootSignatureHairUpdateFollowHairs, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairFollowHairs);
		// Hair Shadow
		setDesc = { pRootSignatureHairShadow, DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
					HAIR_TYPE_COUNT * MAX_NUM_DIRECTIONAL_LIGHTS * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairShadow[0]);
		setDesc = { pRootSignatureHairShadow, DESCRIPTOR_UPDATE_FREQ_PER_DRAW,
					gHairDynamicDescriptorSetCount * MAX_NUM_DIRECTIONAL_LIGHTS * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairShadow[1]);
		// Depth Peeling
		setDesc = { pRootSignatureHairDepthPeeling, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairDepthPeeling[0]);
		setDesc = { pRootSignatureHairDepthPeeling, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairDepthPeeling[1]);
		setDesc = { pRootSignatureHairDepthPeeling, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairDepthPeeling[2]);
		setDesc = { pRootSignatureHairDepthResolve, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairDepthResolve);
		// Fill Colors
		setDesc = { pRootSignatureHairFillColors, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairFillColors[0]);
		setDesc = { pRootSignatureHairFillColors, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairFillColors[1]);
		setDesc = { pRootSignatureHairFillColors, DESCRIPTOR_UPDATE_FREQ_PER_BATCH, HAIR_TYPE_COUNT * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairFillColors[2]);
		setDesc = { pRootSignatureHairFillColors, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gHairDynamicDescriptorSetCount * gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairFillColors[3]);
		setDesc = { pRootSignatureHairColorResolve, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetHairColorResolve);
		// Debug
		setDesc = { pRootSignatureShowCapsules, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetShowCapsule);
	}

	void removeDescriptorSets()
	{
		gSkeletonBatcher.RemoveDescriptorSets();

		gHairDynamicDescriptorSetCount = 0;
		removeDescriptorSet(pRenderer, pDescriptorSetShadow[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetShadow[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetBRDF[2]);

		removeDescriptorSet(pRenderer, pDescriptorSetHairClear);
		removeDescriptorSet(pRenderer, pDescriptorSetHairPreWarm);
		removeDescriptorSet(pRenderer, pDescriptorSetHairIntegrate);
		removeDescriptorSet(pRenderer, pDescriptorSetHairShockPropagate);
		removeDescriptorSet(pRenderer, pDescriptorSetHairLocalConstraints);
		removeDescriptorSet(pRenderer, pDescriptorSetHairLengthConstraints);
		removeDescriptorSet(pRenderer, pDescriptorSetHairFollowHairs);
		removeDescriptorSet(pRenderer, pDescriptorSetHairShadow[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairShadow[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairDepthPeeling[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairDepthPeeling[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairDepthPeeling[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairDepthResolve);
		removeDescriptorSet(pRenderer, pDescriptorSetHairFillColors[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairFillColors[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairFillColors[2]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairFillColors[3]);
		removeDescriptorSet(pRenderer, pDescriptorSetHairColorResolve);
		removeDescriptorSet(pRenderer, pDescriptorSetShowCapsule);
	}

	// Bake as many descriptor sets upfront as possible to avoid updates during runtime
	void prepareDescriptorSets()
	{
		gSkeletonBatcher.PrepareDescriptorSets();

		// Shadow pass
		{
			DescriptorData shadowParams[1] = {};
			shadowParams[0].pName = "cbCamera";
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				shadowParams[0].ppBuffers = &pUniformBufferCameraShadowPass[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetShadow[0], 1, shadowParams);
			}

			shadowParams[0].pName = "cbObject";
			shadowParams[0].ppBuffers = &pUniformBufferGroundPlane;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetShadow[1], 1, shadowParams);
			for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			{
				shadowParams[0].ppBuffers = &pUniformBufferNamePlates[j];
				updateDescriptorSet(pRenderer, 1 + j, pDescriptorSetShadow[1], 1, shadowParams);
				for (uint32_t i = 0; i < gImageCount; ++i)
				{
					shadowParams[0].ppBuffers = &pUniformBufferMatBall[i][j];
					updateDescriptorSet(
						pRenderer, 1 + MATERIAL_INSTANCE_COUNT + (i * MATERIAL_INSTANCE_COUNT + j), pDescriptorSetShadow[1], 1,
						shadowParams);
				}
			}
		}
		// Skybox
		{
			DescriptorData skyParams[1] = {};
			skyParams[0].pName = "skyboxTex";
			skyParams[0].ppTextures = &pTextureSkybox;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 1, skyParams);

			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				skyParams[0].pName = "uniformBlock";
				skyParams[0].ppBuffers = &pUniformBufferCameraSkybox[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, skyParams);
			}
		}
		// Materials
		{
			const char* pTextureName[5] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

			DescriptorData params[6] = {};
			params[0].pName = "cbPointLights";
			params[0].ppBuffers = &pUniformBufferPointLights;
			params[1].pName = "brdfIntegrationMap";
			params[1].ppTextures = &pTextureBRDFIntegrationMap;
			params[2].pName = "irradianceMap";
			params[2].ppTextures = &pTextureIrradianceMap;
			params[3].pName = "specularMap";
			params[3].ppTextures = &pTextureSpecularMap;
			params[4].pName = "shadowMap";
			params[4].ppTextures = &pRenderTargetShadowMap->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetBRDF[0], 5, params);

			// Per Frame
			for (uint32_t f = 0; f < gImageCount; ++f)
			{
				params[0].pName = "cbCamera";
				params[0].ppBuffers = &pUniformBufferCamera[f];
				params[1].pName = "cbDirectionalLights";
				params[1].ppBuffers = &pUniformBufferDirectionalLights[f];
				updateDescriptorSet(pRenderer, f, pDescriptorSetBRDF[1], 2, params);

				for (uint32_t m = 0; m < MATERIAL_BRDF_COUNT; ++m)
				{
					uint32_t matTypeId = m * MATERIAL_TEXTURE_COUNT * MATERIAL_INSTANCE_COUNT;

					for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
					{
						//binding pbr material textures
						for (uint32_t j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
						{
							uint32_t index = j + MATERIAL_TEXTURE_COUNT * i;
							uint32_t textureIndex = matTypeId + index;
							params[j].pName = pTextureName[j];
							if (textureIndex >= gMaterialTextureCount)
							{
								LOGF(LogLevel::eERROR, "texture index greater than array size, setting it to default texture");
								textureIndex = matTypeId + j;
							}
							params[j].ppTextures = &gTextureMaterialMaps[textureIndex];
						}

						params[MATERIAL_TEXTURE_COUNT].pName = "cbObject";
						params[MATERIAL_TEXTURE_COUNT].ppBuffers = &pUniformBufferMatBall[f][i];
						const uint32_t index = 1 + MATERIAL_INSTANCE_COUNT + (f * MATERIAL_BRDF_COUNT * MATERIAL_INSTANCE_COUNT) +
											   (m * MATERIAL_INSTANCE_COUNT) + i;
						updateDescriptorSet(pRenderer, index, pDescriptorSetBRDF[2], MATERIAL_TEXTURE_COUNT + 1, params);
					}
				}
			}

			// Per Draw
			for (uint32_t j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
			{
				params[j].pName = pTextureName[j];
				params[j].ppTextures = &gTextureMaterialMapsGround[j];
			}
			params[MATERIAL_TEXTURE_COUNT].pName = "cbObject";
			params[MATERIAL_TEXTURE_COUNT].ppBuffers = &pUniformBufferGroundPlane;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetBRDF[2], MATERIAL_TEXTURE_COUNT + 1, params);

			for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			{
				for (uint32_t j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
				{
					params[j].pName = pTextureName[j];
					params[j].ppTextures = &gTextureMaterialMaps[j + 4];
				}
				params[MATERIAL_TEXTURE_COUNT].pName = "cbObject";
				params[MATERIAL_TEXTURE_COUNT].ppBuffers = &pUniformBufferNamePlates[j];
				updateDescriptorSet(pRenderer, 1 + j, pDescriptorSetBRDF[2], MATERIAL_TEXTURE_COUNT + 1, params);
			}
		}
		// Hair

		{
			DescriptorData hairParams[7] = {};
			hairParams[0].pName = "DepthsTexture";
#ifndef METAL
			hairParams[0].ppTextures = &pTextureHairDepth;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairClear, 1, hairParams);
#else
			hairParams[0].ppBuffers = &pBufferHairDepth;
			hairParams[1].pName = "cbHairGlobal";
			hairParams[1].ppBuffers = &pUniformBufferHairGlobal;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairClear, 2, hairParams);
#endif

			uint32_t descriptorSetIndex = 0;
			uint32_t shadowDescriptorSetIndex[2] = { 0 };
			for (uint32_t f = 0; f < gImageCount; ++f)
			{
				for (uint32_t hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
				{
					for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
					{
						uint32_t k = gHairTypeIndices[hairType][i];

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexPositionsPrev";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						hairParams[3].pName = "HairVertexPositionsPrevPrev";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[2];
						hairParams[4].pName = "HairRestPositions";
						hairParams[4].ppBuffers = &gHair[k].pBufferHairVertexPositions;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairPreWarm, 5, hairParams);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexPositionsPrev";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						hairParams[3].pName = "HairVertexPositionsPrevPrev";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[2];
						hairParams[4].pName = "HairRestPositions";
						hairParams[4].ppBuffers = &gHair[k].pBufferHairVertexPositions;
						hairParams[5].pName = "cbHairGlobal";
						hairParams[5].ppBuffers = &pUniformBufferHairGlobal;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairIntegrate, 6, hairParams);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexPositionsPrev";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						hairParams[3].pName = "HairVertexPositionsPrevPrev";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[2];
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairShockPropagate, 4, hairParams);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairGlobalRotations";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairGlobalRotations;
						hairParams[3].pName = "HairRefsInLocalFrame";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairRefsInLocalFrame;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairLocalConstraints, 4, hairParams);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "HairRestLengths";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairRestLenghts;
						hairParams[4].pName = "cbHairGlobal";
						hairParams[4].ppBuffers = &pUniformBufferHairGlobal;
#if HAIR_MAX_CAPSULE_COUNT > 0
						hairParams[5].pName = "HairVertexPositionsPrev";
						hairParams[5].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairLengthConstraints, 6, hairParams);
#else
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairLengthConstraints, 5, hairParams);
#endif

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[f];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "FollowHairRootOffsets";
						hairParams[3].ppBuffers = &gHair[k].pBufferFollowHairRootOffsets;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairFollowHairs, 4, hairParams);

						hairParams[0].pName = "cbHair";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[f];
						hairParams[1].pName = "GuideHairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "GuideHairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "HairThicknessCoefficients";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairDepthPeeling[2], 4, hairParams);

						hairParams[0].pName = "cbHair";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[f];
						hairParams[1].pName = "GuideHairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "GuideHairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "HairThicknessCoefficients";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
						updateDescriptorSet(pRenderer, descriptorSetIndex, pDescriptorSetHairFillColors[3], 4, hairParams);

						++descriptorSetIndex;
					}

					for (uint32_t i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
					{
						hairParams[0].pName = "cbCamera";
						hairParams[0].ppBuffers = &pUniformBufferCameraHairShadows[f][hairType][i];
						updateDescriptorSet(pRenderer, shadowDescriptorSetIndex[0], pDescriptorSetHairShadow[0], 1, hairParams);

						for (size_t j = 0; j < gHairTypeIndices[hairType].size(); ++j)
						{
							uint32_t k = gHairTypeIndices[hairType][j];

							hairParams[0].pName = "cbHair";
							hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[f];
							hairParams[1].pName = "GuideHairVertexPositions";
							hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
							hairParams[2].pName = "GuideHairVertexTangents";
							hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
							hairParams[3].pName = "HairThicknessCoefficients";
							hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
							updateDescriptorSet(pRenderer, shadowDescriptorSetIndex[1], pDescriptorSetHairShadow[1], 4, hairParams);

							++shadowDescriptorSetIndex[1];
						}

						++shadowDescriptorSetIndex[0];
					}

					Texture* ppShadowMaps[MAX_NUM_DIRECTIONAL_LIGHTS] = {};
					for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
						ppShadowMaps[i] = pRenderTargetHairShadows[hairType][i]->pTexture;

					hairParams[0].pName = "DirectionalLightShadowMaps";
					hairParams[0].ppTextures = ppShadowMaps;    //-V507
					hairParams[0].mCount = MAX_NUM_DIRECTIONAL_LIGHTS;
					hairParams[1].pName = "cbDirectionalLightShadowCameras";
					hairParams[1].ppBuffers = pUniformBufferCameraHairShadows[f][hairType];
					hairParams[1].mCount = MAX_NUM_DIRECTIONAL_LIGHTS;
					updateDescriptorSet(pRenderer, f * HAIR_TYPE_COUNT + hairType, pDescriptorSetHairFillColors[2], 2, hairParams);
					hairParams[0] = {};
					hairParams[1] = {};
				}

				hairParams[0].pName = "cbCamera";
				hairParams[0].ppBuffers = &pUniformBufferCamera[f];
				updateDescriptorSet(pRenderer, f, pDescriptorSetHairDepthPeeling[1], 1, hairParams);

				hairParams[1].pName = "cbDirectionalLights";
				hairParams[1].ppBuffers = &pUniformBufferDirectionalLights[f];
				updateDescriptorSet(pRenderer, f, pDescriptorSetHairFillColors[1], 2, hairParams);
			}

			hairParams[0].pName = "DepthsTexture";
#ifndef METAL
			hairParams[0].ppTextures = &pTextureHairDepth;
#else
			hairParams[0].ppBuffers = &pBufferHairDepth;
#endif
			hairParams[1].pName = "cbHairGlobal";
			hairParams[1].ppBuffers = &pUniformBufferHairGlobal;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairDepthPeeling[0], 2, hairParams);

#ifndef METAL
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairDepthResolve, 1, hairParams);
#else
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairDepthResolve, 2, hairParams);
#endif

			hairParams[0].pName = "cbPointLights";
			hairParams[0].ppBuffers = &pUniformBufferPointLights;
			hairParams[1].pName = "cbHairGlobal";
			hairParams[1].ppBuffers = &pUniformBufferHairGlobal;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairFillColors[0], 2, hairParams);

			hairParams[0].pName = "ColorsTexture";
			hairParams[0].ppTextures = &pRenderTargetFillColors->pTexture;
			hairParams[1].pName = "InvAlphaTexture";
			hairParams[1].ppTextures = &pRenderTargetDepthPeeling->pTexture;
			updateDescriptorSet(pRenderer, 0, pDescriptorSetHairColorResolve, 2, hairParams);
		}
		// Debug
		{
			DescriptorData params[1] = {};
			for (uint32_t i = 0; i < gImageCount; ++i)
			{
				params[0].pName = "cbCamera";
				params[0].ppBuffers = &pUniformBufferCamera[i];
				updateDescriptorSet(pRenderer, i, pDescriptorSetShowCapsule, 1, params);
			}
		}
	}

	void removePBRMaps()
	{
		removeResource(pTextureSpecularMap);
		removeResource(pTextureIrradianceMap);
		removeResource(pTextureSkybox);
		removeResource(pTextureBRDFIntegrationMap);
	}

	void LoadModelsAndTextures()
	{
		gVertexLayoutDefault.mAttribCount = 3;
		gVertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		gVertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[0].mBinding = 0;
		gVertexLayoutDefault.mAttribs[0].mLocation = 0;
		gVertexLayoutDefault.mAttribs[0].mOffset = 0;
		gVertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		gVertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		gVertexLayoutDefault.mAttribs[1].mLocation = 1;
		gVertexLayoutDefault.mAttribs[1].mBinding = 0;
		gVertexLayoutDefault.mAttribs[1].mOffset = 3 * sizeof(float);
		gVertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		gVertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
		gVertexLayoutDefault.mAttribs[2].mLocation = 2;
		gVertexLayoutDefault.mAttribs[2].mBinding = 0;
		gVertexLayoutDefault.mAttribs[2].mOffset = 6 * sizeof(float);

		bool modelsAreLoaded = false;
		gLuaManager.SetFunction("LoadModel", [this](ILuaStateWrap* state) -> int {
			const char* filename;
			state->GetStringArg(1, &filename);    //indexing in Lua starts from 1 (NOT 0) !!
			size_t len = strlen(filename) + 1;
			char* copy = (char*)tf_malloc(len);
			memcpy(copy, filename, len);
			pStagingData->mModelList.push_back(copy);
			//this->LoadModel(filename);
			return 0;    //return amount of arguments that we want to send back to script
		});

		gLuaManager.AddAsyncScript("loadModels.lua", NULL, [&modelsAreLoaded](ScriptState state) { modelsAreLoaded = true; });

		while (!modelsAreLoaded)    //-V776 //-V712
			threadSleep(0);

		uintptr_t meshCount = pStagingData->mModelList.size();
		gMeshes.resize(meshCount);

		if (!isResourceLoaderSingleThreaded())
		{
			addThreadSystemRangeTask(pIOThreads, &memberTaskFunc<MaterialPlayground, &MaterialPlayground::addModel>, this, meshCount);
		}
		else
		{
			for (uint32_t i = 0; i < meshCount; ++i)
			{
				addModel(i);
			}
		}

		bool texturesAreLoaded = false;
		gLuaManager.SetFunction("GetTextureResolution", [](ILuaStateWrap* state) -> int {
			state->PushResultString(TEXTURE_RESOLUTION);
			return 1;
		});
		gLuaManager.SetFunction("GetSkipLoadingTexturesFlag", [](ILuaStateWrap* state) -> int {
			state->PushResultInteger(SKIP_LOADING_TEXTURES);
			return 1;
		});
		gLuaManager.SetFunction("LoadTextureMaps", [this](ILuaStateWrap* state) -> int {
			int arrSize = 0;
			state->GetStringArrayArg(1, NULL, &arrSize);
			if (arrSize > 0)
			{
				const char** texturesNames = (const char**)tf_malloc(sizeof(const char*)*arrSize);
				size_t namesBufferSize = 0;
				state->GetStringArrayArg(1, texturesNames, &arrSize);
				for (int i = 0; i < arrSize; ++i)
					namesBufferSize += strlen(texturesNames[i]) + 1; 
				
				char* namesBuffer = (char*)tf_malloc(namesBufferSize);
				char* pBegin = namesBuffer;
				char* pEnd = namesBuffer + namesBufferSize;
				for (int i = 0; i < arrSize; ++i)
				{
					// Albedo textures use gamma corrected values
					bool isAlbedo = strstr(texturesNames[i], "Albedo") != NULL;
					this->pStagingData->mMaterialNamesStorage.push_back({ pBegin, isAlbedo});
					/* strcpy doesn't return number of written characters :(*/
					pBegin += snprintf(pBegin, pEnd - pBegin,  "%s", texturesNames[i]) + 1;
				}
				tf_free(texturesNames);
			}

			return 0;
		});

		gLuaManager.AddAsyncScript("loadTextures.lua", NULL, [&texturesAreLoaded](ScriptState state) { texturesAreLoaded = true; });

		while (!texturesAreLoaded)    //-V776 //-V712
			threadSleep(0);

		uintptr_t materialTextureCount = pStagingData->mMaterialNamesStorage.size();
		gTextureMaterialMaps.resize(materialTextureCount);

		if (!isResourceLoaderSingleThreaded())
		{
			addThreadSystemRangeTask(pIOThreads, LoadMaterialTexturesTask, pStagingData, materialTextureCount);
		}
		else
		{
			for (uint32_t i = 0; i < materialTextureCount; ++i)
			{
				LoadMaterialTexturesTask(pStagingData, i);
			}
		}

		bool groundTexturesAreLoaded = false;
		//This is how we can replace function in runtime.
		gLuaManager.SetFunction("LoadTextureMaps", [this](ILuaStateWrap* state) -> int {
			int arrSize = 0;
			state->GetStringArrayArg(1, NULL, &arrSize);
			if (arrSize > 0)
			{
				const char** texturesNames = (const char**)tf_malloc(sizeof(const char*)*arrSize);
				size_t namesBufferSize = 0;
				state->GetStringArrayArg(1, texturesNames, &arrSize);
				for (int i = 0; i < arrSize; ++i)
					namesBufferSize += strlen(texturesNames[i]) + 1; 
				
				char* namesBuffer = (char*)tf_malloc(namesBufferSize);
				char* pBegin = namesBuffer;
				char* pEnd = namesBuffer + namesBufferSize;
				for (int i = 0; i < arrSize; ++i)
				{
					// Albedo textures use gamma corrected values
					bool isAlbedo = strstr(texturesNames[i], "Albedo") != NULL;
					this->pStagingData->mGroundNamesStorage.push_back({ pBegin, isAlbedo});
					/* strcpy doesn't return number of written characters, so have to use sprintf*/
					pBegin += snprintf(pBegin, pEnd - pBegin, "%s", texturesNames[i]) + 1;
				}
				tf_free(texturesNames);
			}

			return 0;
		});

		gLuaManager.AddAsyncScript(
			"loadGroundTextures.lua", NULL, [&groundTexturesAreLoaded](ScriptState state) { groundTexturesAreLoaded = true; });

		while (!groundTexturesAreLoaded)    //-V776 //-V712
			threadSleep(0);

		uintptr_t groundTextureCount = pStagingData->mGroundNamesStorage.size();
		gTextureMaterialMapsGround.resize(groundTextureCount);

		if (!isResourceLoaderSingleThreaded())
		{
			addThreadSystemRangeTask(pIOThreads, &LoadGroundTexturesTask, pStagingData, groundTextureCount);
		}
		else
		{
			for (uint32_t i = 0; i < groundTextureCount; ++i)
			{
				LoadGroundTexturesTask(pStagingData, i);
			}
		}
	}

	static void LoadMaterialTexturesTask(void* data, uintptr_t i)
	{
		StagingData*    pTaskData = (StagingData*)data;
		TextureLoadDesc desc = {};
		desc.pFileName = pTaskData->mMaterialNamesStorage[i].pFileName;
		desc.ppTexture = &gTextureMaterialMaps[i];
		if (pTaskData->mMaterialNamesStorage[i].mIsSrgb)
		{
			// Textures representing color should be stored in SRGB or HDR format
			desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		}
		addResource(&desc, NULL);
	}

	static void LoadGroundTexturesTask(void* data, uintptr_t i)
	{
		StagingData*    pTaskData = (StagingData*)data;
		TextureLoadDesc desc = {};
		desc.pFileName = pTaskData->mGroundNamesStorage[i].pFileName;
		desc.ppTexture = &gTextureMaterialMapsGround[i];
		if (pTaskData->mGroundNamesStorage[i].mIsSrgb)
		{
			// Textures representing color should be stored in SRGB or HDR format
			desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
		}
		addResource(&desc, NULL);
	}

	void ComputePBRMaps()
	{
		Texture*       pPanoSkybox = NULL;
		Shader*        pPanoToCubeShader = NULL;
		RootSignature* pPanoToCubeRootSignature = NULL;
		Pipeline*      pPanoToCubePipeline = NULL;
		Shader*        pBRDFIntegrationShader = NULL;
		RootSignature* pBRDFIntegrationRootSignature = NULL;
		Pipeline*      pBRDFIntegrationPipeline = NULL;
		Shader*        pIrradianceShader = NULL;
		RootSignature* pIrradianceRootSignature = NULL;
		Pipeline*      pIrradiancePipeline = NULL;
		Shader*        pSpecularShader = NULL;
		RootSignature* pSpecularRootSignature = NULL;
		Pipeline*      pSpecularPipeline = NULL;
		Sampler*       pSkyboxSampler = NULL;
		DescriptorSet* pDescriptorSetBRDF = { NULL };
		DescriptorSet* pDescriptorSetPanoToCube[2] = { NULL };
		DescriptorSet* pDescriptorSetIrradiance = { NULL };
		DescriptorSet* pDescriptorSetSpecular[2] = { NULL };

		static const int skyboxIndex = 0;
		const char*      skyboxNames[] = {
            "LA_Helipad",
		};
		// PBR Texture values (these values are mirrored on the shaders).
		static const uint32_t gBRDFIntegrationSize = 512;
		static const uint32_t gSkyboxSize = 1024;
		static const uint32_t gSkyboxMips = (uint)log2(gSkyboxSize) + 1;
		static const uint32_t gIrradianceSize = 32;
		static const uint32_t gSpecularSize = 128;
		static const uint32_t gSpecularMips = (uint)log2(gSpecularSize) + 1;

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, false, 0.0f, 0.0f, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		SyncToken       token = {};
		TextureLoadDesc panoDesc = {};
		panoDesc.pFileName = skyboxNames[skyboxIndex];
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc, &token);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pName = "skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pTextureSkybox;
		addResource(&skyboxLoadDesc, &token);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pName = "irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pTextureIrradianceMap;
		addResource(&irrLoadDesc, &token);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pName = "specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pTextureSpecularMap;
		addResource(&specImgLoadDesc, &token);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc     brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = TinyImageFormat_R32G32_SFLOAT;
		brdfIntegrationDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pTextureBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc, &token);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", 0 };

		GPUPresetLevel presetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;

		const char* brdfIntegrationShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"BRDFIntegration_SAMPLES_0.comp",
			"BRDFIntegration_SAMPLES_0.comp",
			"BRDFIntegration_SAMPLES_64.comp",
			"BRDFIntegration_SAMPLES_128.comp",
			"BRDFIntegration_SAMPLES_256.comp",
			"BRDFIntegration_SAMPLES_1024.comp"
		};

		const char* irradianceShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp",
			"computeIrradianceMap_SAMPLE_DELTA_0025.comp"
		};

		const char* specularShaders[GPUPresetLevel::GPU_PRESET_COUNT] = {
			"computeSpecularMap_SAMPLES_0.comp",
			"computeSpecularMap_SAMPLES_0.comp",
			"computeSpecularMap_SAMPLES_64.comp",
			"computeSpecularMap_SAMPLES_128.comp",
			"computeSpecularMap_SAMPLES_256.comp",
			"computeSpecularMap_SAMPLES_1024.comp"
		};

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { brdfIntegrationShaders[presetLevel], NULL, 0 };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { irradianceShaders[presetLevel], NULL, 0 };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { specularShaders[presetLevel], NULL, 0 };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);

		const char*       pStaticSamplerNames[] = { "skyboxSampler" };
		RootSignatureDesc panoRootDesc = { &pPanoToCubeShader, 1 };
		panoRootDesc.mStaticSamplerCount = 1;
		panoRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		panoRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc brdfRootDesc = { &pBRDFIntegrationShader, 1 };
		brdfRootDesc.mStaticSamplerCount = 1;
		brdfRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		brdfRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc irradianceRootDesc = { &pIrradianceShader, 1 };
		irradianceRootDesc.mStaticSamplerCount = 1;
		irradianceRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		irradianceRootDesc.ppStaticSamplers = &pSkyboxSampler;
		RootSignatureDesc specularRootDesc = { &pSpecularShader, 1 };
		specularRootDesc.mStaticSamplerCount = 1;
		specularRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		specularRootDesc.ppStaticSamplers = &pSkyboxSampler;
		addRootSignature(pRenderer, &panoRootDesc, &pPanoToCubeRootSignature);
		addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
		addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);
		addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);

		DescriptorSetDesc setDesc = { pBRDFIntegrationRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetBRDF);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[0]);
		setDesc = { pPanoToCubeRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPanoToCube[1]);
		setDesc = { pIrradianceRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetIrradiance);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[0]);
		setDesc = { pSpecularRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, gSkyboxMips };
		addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSpecular[1]);

		PipelineDesc desc = {};
		desc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addPipeline(pRenderer, &desc, &pPanoToCubePipeline);
		pipelineSettings.pShaderProgram = pIrradianceShader;
		pipelineSettings.pRootSignature = pIrradianceRootSignature;
		addPipeline(pRenderer, &desc, &pIrradiancePipeline);
		pipelineSettings.pShaderProgram = pSpecularShader;
		pipelineSettings.pRootSignature = pSpecularRootSignature;
		addPipeline(pRenderer, &desc, &pSpecularPipeline);
		pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
		pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
		addPipeline(pRenderer, &desc, &pBRDFIntegrationPipeline);

		waitForToken(&token);

		Cmd* pCmd = pCmds[0];

		// Compute the BRDF Integration map.
		beginCmd(pCmd);

		cmdBindPipeline(pCmd, pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pTextureBRDFIntegrationMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetBRDF, 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetBRDF);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] = { { pTextureBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };

		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarrier, 0, NULL);

		// Store the panorama texture inside a cubemap.
		cmdBindPipeline(pCmd, pPanoToCubePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pPanoSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetPanoToCube[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPanoToCube[0]);

		struct
		{
			uint32_t mip;
			uint32_t textureSize;
		} rootConstantData = { 0, gSkyboxSize };

		uint32_t rootConstantIndex = getDescriptorIndexFromName(pPanoToCubeRootSignature, "RootConstant");

		for (uint32_t i = 0; i < gSkyboxMips; ++i)
		{
			rootConstantData.mip = i;
			cmdBindPushConstants(pCmd, pPanoToCubeRootSignature, rootConstantIndex, &rootConstantData);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pTextureSkybox;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetPanoToCube[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetPanoToCube[1]);

			pThreadGroupSize = pPanoToCubeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				pCmd, max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(rootConstantData.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pTextureSkybox, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 1, srvBarriers, 0, NULL);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(pCmd, pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pTextureIrradianceMap;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetIrradiance, 2, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetIrradiance);
		pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(pCmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);
		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(pCmd, pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetSpecular[0], 1, params);
		cmdBindDescriptorSet(pCmd, 0, pDescriptorSetSpecular[0]);

		struct PrecomputeSkySpecularData
		{
			uint  mipSize;
			float roughness;
		};

		rootConstantIndex = getDescriptorIndexFromName(pSpecularRootSignature, "RootConstant");

		for (uint32_t i = 0; i < gSpecularMips; i++)
		{
			PrecomputeSkySpecularData data = {};
			data.roughness = (float)i / (float)(gSpecularMips - 1);
			data.mipSize = gSpecularSize >> i;
			cmdBindPushConstants(pCmd, pSpecularRootSignature, rootConstantIndex, &data);
			params[0].pName = "dstTexture";
			params[0].ppTextures = &pTextureSpecularMap;
			params[0].mUAVMipSlice = i;
			updateDescriptorSet(pRenderer, i, pDescriptorSetSpecular[1], 1, params);
			cmdBindDescriptorSet(pCmd, i, pDescriptorSetSpecular[1]);
			pThreadGroupSize = pIrradianceShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(pCmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] = { { pTextureIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
										   { pTextureSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(pCmd, 0, NULL, 2, srvBarriers2, 0, NULL);

		endCmd(pCmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.ppCmds = &pCmd;
		submitDesc.pSignalFence = pRenderCompleteFences[0];
		submitDesc.mSubmitDone = true;
		queueSubmit(pGraphicsQueue, &submitDesc);
		waitQueueIdle(pGraphicsQueue);

		removeDescriptorSet(pRenderer, pDescriptorSetBRDF);

		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetPanoToCube[1]);
		removeDescriptorSet(pRenderer, pDescriptorSetIrradiance);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[0]);
		removeDescriptorSet(pRenderer, pDescriptorSetSpecular[1]);
		removePipeline(pRenderer, pSpecularPipeline);
		removeRootSignature(pRenderer, pSpecularRootSignature);
		removeShader(pRenderer, pSpecularShader);
		removePipeline(pRenderer, pIrradiancePipeline);
		removeRootSignature(pRenderer, pIrradianceRootSignature);
		removeShader(pRenderer, pIrradianceShader);
		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removePipeline(pRenderer, pBRDFIntegrationPipeline);
		removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
		removeShader(pRenderer, pBRDFIntegrationShader);
		removeResource(pPanoSkybox);
		removeSampler(pRenderer, pSkyboxSampler);
	}

	void addModel(uintptr_t i)
	{
		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = pStagingData->mModelList[i];
		loadDesc.ppGeometry = &gMeshes[i];
		loadDesc.pVertexLayout = &gVertexLayoutDefault;
		loadDesc.mOptimizationFlags = MESH_OPTIMIZATION_FLAG_OVERDRAW;
		addResource(&loadDesc, NULL);
	}

	void removeModels()
	{
		for (size_t i = 0; i < gMeshes.size(); ++i)
			removeResource(gMeshes[i]);

		gMeshes.set_capacity(0);
	}

	void removeTextures()
	{
		for (size_t i = 0; i < gTextureMaterialMaps.size(); ++i)
			removeResource(gTextureMaterialMaps[i]);
		gTextureMaterialMaps.set_capacity(0);

		for (size_t i = 0; i < gTextureMaterialMapsGround.size(); ++i)
			removeResource(gTextureMaterialMapsGround[i]);
		gTextureMaterialMapsGround.set_capacity(0);
	}

	void addResources()
	{
		//Generate skybox vertex buffer
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
		skyboxVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pVertexBufferSkybox;
		addResource(&skyboxVbDesc, NULL);

		// Load hair models
		gUniformDataHairGlobal.mGravity = float4(0.0f, -9.81f, 0.0f, 0.0f);
		gUniformDataHairGlobal.mWind = float4(0.0f);

		gCapsules.clear();

		NamedCapsule headCapsule = {};
		headCapsule.mName = "Head";
		headCapsule.mCapsule.mCenter0 = float3(-0.41f, 0.0f, 0.0f);
		headCapsule.mCapsule.mRadius0 = 0.5f;
		headCapsule.mCapsule.mCenter1 = float3(-0.83f, 0.0f, 0.0f);
		headCapsule.mCapsule.mRadius1 = 0.5f;
		headCapsule.mAttachedBone = gAnimationRig.FindJoint(gHeadAttachmentJointName);
		gCapsules.push_back(headCapsule);

#if HAIR_MAX_CAPSULE_COUNT >= 3
		NamedCapsule leftShoulderCapsule = {};
		leftShoulderCapsule.mName = "LeftShoulder";
		leftShoulderCapsule.mCapsule.mCenter0 = float3(-0.17f, 0.0f, 0.0f);
		leftShoulderCapsule.mCapsule.mRadius0 = 0.25f;
		leftShoulderCapsule.mCapsule.mCenter1 = float3(0.74f, 0.0f, 0.0f);
		leftShoulderCapsule.mCapsule.mRadius1 = 0.21f;
		leftShoulderCapsule.mAttachedBone = gAnimationRig.FindJoint(gLeftShoulderJointName);
		gCapsules.push_back(leftShoulderCapsule);

		NamedCapsule rightShoulderCapsule = {};
		rightShoulderCapsule.mName = "RightShoulder";
		rightShoulderCapsule.mCapsule.mCenter0 = float3(-0.17f, 0.0f, 0.0f);
		rightShoulderCapsule.mCapsule.mRadius0 = 0.25f;
		rightShoulderCapsule.mCapsule.mCenter1 = float3(0.74f, 0.0f, 0.0f);
		rightShoulderCapsule.mCapsule.mRadius1 = 0.21f;
		rightShoulderCapsule.mAttachedBone = gAnimationRig.FindJoint(gRightShoulderJointName);
		gCapsules.push_back(rightShoulderCapsule);
#endif

		gTransforms.clear();

		NamedTransform headTransform = {};
		headTransform.mName = "Head";
		headTransform.mTransform.mPosition = vec3(0.0f, -2.2f, 0.0f);
		headTransform.mTransform.mOrientation = vec3(0.0f, PI * 0.5f, -PI * 0.5f);
		headTransform.mTransform.mScale = 0.02f;
		headTransform.mAttachedBone = gAnimationRig.FindJoint(gHeadAttachmentJointName);
		gTransforms.push_back(headTransform);

		float gpuPresetScore = (float)((uint)gGPUPresetLevel - GPU_PRESET_LOW) / (float)(GPU_PRESET_ULTRA - GPU_PRESET_LOW);

		// Load all hair meshes
		HairSectionShadingParameters ponytailHairShadingParameters = {};
		ponytailHairShadingParameters.mColorBias = 10.0f;
		ponytailHairShadingParameters.mStrandRadius = 0.04f * gpuPresetScore + 0.21f * (1.0f - gpuPresetScore);
		ponytailHairShadingParameters.mStrandSpacing = 0.5f * gpuPresetScore + 0.2f * (1.0f - gpuPresetScore);
		ponytailHairShadingParameters.mDisableRootColor = true;

		HairSectionShadingParameters hairShadingParameters = {};
		hairShadingParameters.mColorBias = 5.0f;
		hairShadingParameters.mStrandRadius = 0.04f * gpuPresetScore + 0.21f * (1.0f - gpuPresetScore);
		hairShadingParameters.mStrandSpacing = 0.5f * gpuPresetScore + 0.2f * (1.0f - gpuPresetScore);
		hairShadingParameters.mDisableRootColor = false;

		HairSimulationParameters ponytailHairSimulationParameters = {};
		ponytailHairSimulationParameters.mDamping = 0.04f;
		ponytailHairSimulationParameters.mGlobalConstraintStiffness = 0.06f;
		ponytailHairSimulationParameters.mGlobalConstraintRange = 0.55f;
		ponytailHairSimulationParameters.mShockPropagationStrength = 0.0f;
		ponytailHairSimulationParameters.mShockPropagationAccelerationThreshold = 10.0f;
		ponytailHairSimulationParameters.mLocalConstraintStiffness = 0.04f;
		ponytailHairSimulationParameters.mLocalConstraintIterations = 2;
		ponytailHairSimulationParameters.mLengthConstraintIterations = 2;
		ponytailHairSimulationParameters.mTipSeperationFactor = 2.0f;
		ponytailHairSimulationParameters.mCapsuleCount = 1;
		ponytailHairSimulationParameters.mCapsules[0] = 0;
#if HAIR_MAX_CAPSULE_COUNT >= 3
		ponytailHairSimulationParameters.mCapsuleCount = 3;
		ponytailHairSimulationParameters.mCapsules[1] = 1;
		ponytailHairSimulationParameters.mCapsules[2] = 2;
#endif

		HairSimulationParameters hairSimulationParameters = {};
		hairSimulationParameters.mDamping = 0.1f;
		hairSimulationParameters.mGlobalConstraintStiffness = 0.06f;
		hairSimulationParameters.mGlobalConstraintRange = 0.55f;
		hairSimulationParameters.mShockPropagationStrength = 0.0f;
		hairSimulationParameters.mShockPropagationAccelerationThreshold = 10.0f;
		hairSimulationParameters.mLocalConstraintStiffness = 0.26f;
		hairSimulationParameters.mLocalConstraintIterations = 2;
		hairSimulationParameters.mLengthConstraintIterations = 2;
		hairSimulationParameters.mTipSeperationFactor = 2.0f;
		hairSimulationParameters.mCapsuleCount = 1;
		hairSimulationParameters.mCapsules[0] = 0;
#if HAIR_MAX_CAPSULE_COUNT >= 3
		hairSimulationParameters.mCapsuleCount = 3;
		hairSimulationParameters.mCapsules[1] = 1;
		hairSimulationParameters.mCapsules[2] = 2;
#endif

		HairSimulationParameters stiffHairSimulationParameters = hairSimulationParameters;
		stiffHairSimulationParameters.mGlobalConstraintRange = 0.8f;
		stiffHairSimulationParameters.mLocalConstraintStiffness = 0.7f;

		HairSimulationParameters staticHairSimulationParameters = hairSimulationParameters;
		staticHairSimulationParameters.mGlobalConstraintRange = 1.0f;
		staticHairSimulationParameters.mGlobalConstraintStiffness = 1.0f;

		for (uint32_t i = 0; i < HAIR_TYPE_COUNT; ++i)
			gHairTypeIndices[i].clear();

		addHairMesh(
			HAIR_TYPE_PONYTAIL, "ponytail", "Hair/tail.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &ponytailHairShadingParameters,
			&ponytailHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_PONYTAIL, "top", "Hair/front_top.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_PONYTAIL, "side", "Hair/side.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_PONYTAIL, "back", "Hair/back.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&staticHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_FEMALE_1, "Female hair 1", "Hair/female_hair_1.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&hairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_FEMALE_2, "Female hair 2", "Hair/female_hair_2.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&hairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_FEMALE_3, "Female hair 3", "Hair/female_hair_3.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_FEMALE_6, "female hair 6 top", "Hair/female_hair_6_top.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0,
			&hairShadingParameters, &staticHairSimulationParameters);
		addHairMesh(
			HAIR_TYPE_FEMALE_6, "female hair 6 tail", "Hair/female_hair_6_tail.gltf", (uint)(5 * gpuPresetScore), 0.5f, 0,
			&ponytailHairShadingParameters, &ponytailHairSimulationParameters);

		// Create skeleton buffers
		const int   sphereResolution = 30;                  // Increase for higher resolution joint spheres
		const float boneWidthRatio = 0.2f;                  // Determines how far along the bone to put the max width [0,1]
		const float jointRadius = boneWidthRatio * 0.5f;    // set to replicate Ozz skeleton

		// Generate joint vertex buffer
		generateSpherePoints(&pStagingData->pJointPoints, &gVertexCountSkeletonJoint, sphereResolution, jointRadius);

		uint64_t       jointDataSize = gVertexCountSkeletonJoint * sizeof(float);
		BufferLoadDesc jointVbDesc = {};
		jointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		jointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		jointVbDesc.mDesc.mSize = jointDataSize;
		jointVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		jointVbDesc.pData = pStagingData->pJointPoints;
		jointVbDesc.ppBuffer = &pVertexBufferSkeletonJoint;
		addResource(&jointVbDesc, NULL);

		// Generate bone vertex buffer
		generateBonePoints(&pStagingData->pBonePoints, &gVertexCountSkeletonBone, boneWidthRatio);

		uint64_t       boneDataSize = gVertexCountSkeletonBone * sizeof(float);
		BufferLoadDesc boneVbDesc = {};
		boneVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		boneVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boneVbDesc.mDesc.mSize = boneDataSize;
		boneVbDesc.mDesc.mStartState = RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		boneVbDesc.pData = pStagingData->pBonePoints;
		boneVbDesc.ppBuffer = &pVertexBufferSkeletonBone;
		addResource(&boneVbDesc, NULL);
	}

	void removeResources()
	{
		removeResource(pVertexBufferSkybox);
		removeHairMeshes();
		removeResource(pVertexBufferSkeletonJoint);
		removeResource(pVertexBufferSkeletonBone);
	}

	void addUniformBuffers()
	{
		// Ground plane uniform buffer
		BufferLoadDesc surfaceUBDesc = {};
		surfaceUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		surfaceUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		surfaceUBDesc.mDesc.mSize = sizeof(UniformObjData);
		surfaceUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		surfaceUBDesc.pData = NULL;
		surfaceUBDesc.ppBuffer = &pUniformBufferGroundPlane;
		addResource(&surfaceUBDesc, NULL);

		// Nameplate uniform buffers
		BufferLoadDesc nameplateUBDesc = {};
		nameplateUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		nameplateUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		nameplateUBDesc.mDesc.mSize = sizeof(UniformObjData);
		nameplateUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		nameplateUBDesc.pData = NULL;
		for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			nameplateUBDesc.ppBuffer = &pUniformBufferNamePlates[i];
			addResource(&nameplateUBDesc, NULL);
		}

		// Create a uniform buffer per mat ball
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				BufferLoadDesc matBallUBDesc = {};
				matBallUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				matBallUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				matBallUBDesc.mDesc.mSize = sizeof(UniformObjData);
				matBallUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				matBallUBDesc.pData = NULL;
				matBallUBDesc.ppBuffer = &pUniformBufferMatBall[frameIdx][i];
				addResource(&matBallUBDesc, NULL);
			}
		}

		// Uniform buffer for camera data
		BufferLoadDesc cameraUBDesc = {};
		cameraUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		cameraUBDesc.mDesc.mSize = sizeof(UniformCamData);
		cameraUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		cameraUBDesc.pData = NULL;
		for (uint i = 0; i < gImageCount; ++i)
		{
			cameraUBDesc.ppBuffer = &pUniformBufferCamera[i];
			addResource(&cameraUBDesc, NULL);
			cameraUBDesc.ppBuffer = &pUniformBufferCameraSkybox[i];
			addResource(&cameraUBDesc, NULL);
			cameraUBDesc.ppBuffer = &pUniformBufferCameraShadowPass[i];
			addResource(&cameraUBDesc, NULL);

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				for (int j = 0; j < MAX_NUM_DIRECTIONAL_LIGHTS; ++j)
				{
					cameraUBDesc.ppBuffer = &pUniformBufferCameraHairShadows[i][hairType][j];
					addResource(&cameraUBDesc, NULL);
				}
			}
		}

		// Uniform buffer for directional light data
		BufferLoadDesc directionalLightBufferDesc = {};
		directionalLightBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		directionalLightBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		directionalLightBufferDesc.mDesc.mSize = sizeof(UniformDataDirectionalLights);
		directionalLightBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		directionalLightBufferDesc.pData = NULL;
		for (uint i = 0; i < gImageCount; ++i)
		{
			directionalLightBufferDesc.ppBuffer = &pUniformBufferDirectionalLights[i];
			addResource(&directionalLightBufferDesc, NULL);
		}

		// Uniform buffer for light data
		BufferLoadDesc lightsUBDesc = {};
		lightsUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightsUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightsUBDesc.mDesc.mSize = sizeof(UniformDataPointLights);
		lightsUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightsUBDesc.pData = NULL;
		lightsUBDesc.ppBuffer = &pUniformBufferPointLights;
		addResource(&lightsUBDesc, NULL);

		// Uniform buffer for hair data
		BufferLoadDesc hairGlobalBufferDesc = {};
		hairGlobalBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairGlobalBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairGlobalBufferDesc.mDesc.mSize = sizeof(UniformDataHairGlobal);
		hairGlobalBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairGlobalBufferDesc.pData = NULL;
		hairGlobalBufferDesc.ppBuffer = &pUniformBufferHairGlobal;
		addResource(&hairGlobalBufferDesc, NULL);
	}

	void removeUniformBuffers()
	{
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBufferCameraSkybox[i]);
			removeResource(pUniformBufferCamera[i]);
			removeResource(pUniformBufferCameraShadowPass[i]);
			removeResource(pUniformBufferDirectionalLights[i]);
			for (uint32_t hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				for (uint32_t j = 0; j < MAX_NUM_DIRECTIONAL_LIGHTS; ++j)
					removeResource(pUniformBufferCameraHairShadows[i][hairType][j]);
			}
			for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
				removeResource(pUniformBufferMatBall[i][j]);
		}

		removeResource(pUniformBufferGroundPlane);
		for (uint32_t j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			removeResource(pUniformBufferNamePlates[j]);

		removeResource(pUniformBufferPointLights);

		removeResource(pUniformBufferHairGlobal);
	}

	void InitializeUniformBuffers()
	{
		// Update the uniform buffer for the objects
		float baseX = 22.0f;
		float baseY = -1.8f;
		float baseZ = 12.0f;
		float offsetX = 8.0f;
		float scaleVal = 4.0f;
		float roughDelta = 1.0f;
		float materialPlateOffset = 4.0f;

		gTextWorldMats.clear();

		for (uint32_t i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			mat4 modelmat =
				mat4::translation(vec3(baseX - i - offsetX * i, baseY, baseZ)) * mat4::scale(vec3(scaleVal)) * mat4::rotationY(PI);

			gUniformDataMatBall[i] = {};
			gUniformDataMatBall[i].mWorldMat = modelmat;
			gUniformDataMatBall[i].mMetallic = i / (float)MATERIAL_INSTANCE_COUNT;
			gUniformDataMatBall[i].mRoughness = 0.04f + roughDelta;
			gUniformDataMatBall[i].textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL;
			//if not enough materials specified then set pbrMaterials to -1

			BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[gFrameIndex][i] };
			beginUpdateResource(&objBuffUpdateDesc);
			*(UniformObjData*)objBuffUpdateDesc.pMappedData = gUniformDataMatBall[i];
			endUpdateResource(&objBuffUpdateDesc, NULL);
			roughDelta -= .25f;

			{
				//plates
				modelmat = mat4::translation(vec3(baseX - i - offsetX * i, -5.8f, baseZ + materialPlateOffset)) *
						   mat4::rotationX(3.1415f * 0.2f) * mat4::scale(vec3(3.0f, 0.1f, 1.0f));
				UniformObjData plateUniform = {};
				plateUniform.mWorldMat = modelmat;
				plateUniform.mMetallic = 1.0f;
				plateUniform.mRoughness = 0.4f;
				plateUniform.mAlbedo = float3(0.04f);
				plateUniform.textureConfig = 0;
				BufferUpdateDesc objBuffUpdateDesc1 = { pUniformBufferNamePlates[i] };
				beginUpdateResource(&objBuffUpdateDesc1);
				*(UniformObjData*)objBuffUpdateDesc1.pMappedData = plateUniform;
				endUpdateResource(&objBuffUpdateDesc1, NULL);

				//text
				const float ANGLE_OFFSET = 0.6f;    // angle offset to tilt the text shown on the plates for materials
				gTextWorldMats.push_back(
					mat4::translation(vec3(baseX - i - offsetX * i, -6.2f, baseZ + materialPlateOffset - 0.65f)) *
					mat4::rotationX(-PI * 0.5f + ANGLE_OFFSET) * mat4::scale(vec3(16.0f, 10.0f, 1.0f)));
			}
		}

		// ground plane
		UniformObjData groundUniform = {};
		vec3           groundScale = vec3(30.0f, 0.2f, 20.0f);
		mat4           modelmat = mat4::translation(vec3(0.0f, -6.0f, 5.0f)) * mat4::scale(groundScale);
		groundUniform.mWorldMat = modelmat;
		groundUniform.mMetallic = 0;
		groundUniform.mRoughness = 0.74f;
		groundUniform.mAlbedo = float3(0.3f, 0.3f, 0.3f);
		groundUniform.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL;
		groundUniform.tiling = float2(groundScale.getX() / groundScale.getZ(), 1.0f);
		//gUniformDataObject.textureConfig = ETextureConfigFlags::NORMAL | ETextureConfigFlags::METALLIC | ETextureConfigFlags::AO | ETextureConfigFlags::ROUGHNESS;
		BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferGroundPlane };
		beginUpdateResource(&objBuffUpdateDesc);
		*(UniformObjData*)objBuffUpdateDesc.pMappedData = groundUniform;
		endUpdateResource(&objBuffUpdateDesc, NULL);

		// Directional light
		gUniformDataDirectionalLights.mDirectionalLights[0].mDirection = v3ToF3(normalize(f3Tov3(gDirectionalLightPosition)));
		gUniformDataDirectionalLights.mDirectionalLights[0].mShadowMap = 0;

		gUniformDataDirectionalLights.mDirectionalLights[0].mColor = float3(255.0f, 180.0f, 117.0f) / 255.0f;
		//gUniformDataDirectionalLights.mDirectionalLights[0].mColor = float3(236.222f, 178.504f, 119.650f) / 255.0f;
		//gUniformDataDirectionalLights.mDirectionalLights[0].mColor = float3(255.0f, 0.5f, 0.5f) / 255.0f;
		gUniformDataDirectionalLights.mDirectionalLights[0].mIntensity = 10.0f;
		gUniformDataDirectionalLights.mNumDirectionalLights = 1;
		BufferUpdateDesc directionalLightsBufferUpdateDesc = { pUniformBufferDirectionalLights[0] };
		beginUpdateResource(&directionalLightsBufferUpdateDesc);
		*(UniformDataDirectionalLights*)directionalLightsBufferUpdateDesc.pMappedData = gUniformDataDirectionalLights;
		endUpdateResource(&directionalLightsBufferUpdateDesc, NULL);

		// Point lights (currently none)
		gUniformDataPointLights.mNumPointLights = 0;
		BufferUpdateDesc pointLightBufferUpdateDesc = { pUniformBufferPointLights };
		beginUpdateResource(&pointLightBufferUpdateDesc);
		*(UniformDataPointLights*)pointLightBufferUpdateDesc.pMappedData = gUniformDataPointLights;
		endUpdateResource(&pointLightBufferUpdateDesc, NULL);
	}

	static void addHairMesh(
		HairType type, const char* name, const char* tfxFile, uint numFollowHairs, float maxRadiusAroundGuideHair, uint transform,
		HairSectionShadingParameters* shadingParameters, HairSimulationParameters* simulationParameters)
	{
		HairBuffer hairBuffer = {};

		VertexLayout layout = {};
		layout.mAttribCount = 7;
		layout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		layout.mAttribs[0].mBinding = 0;
		layout.mAttribs[1].mSemantic = SEMANTIC_TANGENT;
		layout.mAttribs[1].mBinding = 1;
		layout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		layout.mAttribs[2].mBinding = 2;
		layout.mAttribs[3].mSemantic = SEMANTIC_TEXCOORD2;
		layout.mAttribs[3].mBinding = 4;
		layout.mAttribs[4].mSemantic = SEMANTIC_TEXCOORD3;
		layout.mAttribs[4].mBinding = 5;
		layout.mAttribs[5].mSemantic = SEMANTIC_TEXCOORD6;
		layout.mAttribs[5].mBinding = 8;
		layout.mAttribs[6].mSemantic = SEMANTIC_TEXCOORD7;
		layout.mAttribs[6].mBinding = 9;

		SyncToken        token = {};
		GeometryLoadDesc loadDesc = {};
		loadDesc.pFileName = tfxFile;
		loadDesc.pVertexLayout = &layout;
		loadDesc.mFlags = GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS;
		loadDesc.ppGeometry = &hairBuffer.pGeom;
		addResource(&loadDesc, &token);
		waitForToken(&token);

		hairBuffer.pBufferTriangleIndices = hairBuffer.pGeom->pIndexBuffer;
		hairBuffer.pBufferHairVertexPositions = hairBuffer.pGeom->pVertexBuffers[0];
		hairBuffer.pBufferHairVertexTangents = hairBuffer.pGeom->pVertexBuffers[1];
		hairBuffer.pBufferHairGlobalRotations = hairBuffer.pGeom->pVertexBuffers[2];
		hairBuffer.pBufferHairRefsInLocalFrame = hairBuffer.pGeom->pVertexBuffers[3];
		hairBuffer.pBufferFollowHairRootOffsets = hairBuffer.pGeom->pVertexBuffers[4];
		hairBuffer.pBufferHairThicknessCoefficients = hairBuffer.pGeom->pVertexBuffers[5];
		hairBuffer.pBufferHairRestLenghts = hairBuffer.pGeom->pVertexBuffers[6];

		BufferLoadDesc vertexPositionsBufferDesc = {};
		vertexPositionsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		vertexPositionsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vertexPositionsBufferDesc.mDesc.mElementCount = hairBuffer.pGeom->mVertexCount;
		vertexPositionsBufferDesc.mDesc.mStructStride = sizeof(float4);
		vertexPositionsBufferDesc.mDesc.mFormat = TinyImageFormat_UNDEFINED;
		vertexPositionsBufferDesc.mDesc.mSize =
			vertexPositionsBufferDesc.mDesc.mElementCount * vertexPositionsBufferDesc.mDesc.mStructStride;
		vertexPositionsBufferDesc.mDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		vertexPositionsBufferDesc.mDesc.pName = "Hair vertex positions";

		for (int i = 0; i < 3; ++i)
		{
			vertexPositionsBufferDesc.mDesc.mDescriptors =
				(DescriptorType)(DESCRIPTOR_TYPE_RW_BUFFER | (i == 0 ? DESCRIPTOR_TYPE_BUFFER : 0));
			vertexPositionsBufferDesc.mDesc.pName = "Hair simulation vertex positions";
			vertexPositionsBufferDesc.ppBuffer = &hairBuffer.pBufferHairSimulationVertexPositions[i];
			addResource(&vertexPositionsBufferDesc, NULL);
		}

		BufferLoadDesc hairShadingBufferDesc = {};
		hairShadingBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairShadingBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairShadingBufferDesc.mDesc.mSize = sizeof(UniformDataHairShading);
		hairShadingBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairShadingBufferDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			hairShadingBufferDesc.ppBuffer = &hairBuffer.pUniformBufferHairShading[i];
			addResource(&hairShadingBufferDesc, NULL);
		}

		BufferLoadDesc hairSimulationBufferDesc = {};
		hairSimulationBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairSimulationBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairSimulationBufferDesc.mDesc.mSize = sizeof(UniformDataHairSimulation);
		hairSimulationBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairSimulationBufferDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			hairSimulationBufferDesc.ppBuffer = &hairBuffer.pUniformBufferHairSimulation[i];
			addResource(&hairSimulationBufferDesc, NULL);
		}

		hairBuffer.mUniformDataHairShading.mColorBias = shadingParameters->mColorBias;
		hairBuffer.mUniformDataHairShading.mStrandRadius = shadingParameters->mStrandRadius;
		hairBuffer.mUniformDataHairShading.mStrandSpacing = shadingParameters->mStrandSpacing;
		hairBuffer.mUniformDataHairShading.mNumVerticesPerStrand = hairBuffer.pGeom->mHair.mVertexCountPerStrand;

		hairBuffer.mUniformDataHairSimulation.mNumStrandsPerThreadGroup = 64 / hairBuffer.pGeom->mHair.mVertexCountPerStrand;
		hairBuffer.mUniformDataHairSimulation.mNumFollowHairsPerGuideHair = numFollowHairs;
		hairBuffer.mUniformDataHairSimulation.mDamping = clamp(simulationParameters->mDamping, 0.0f, 0.1f);
		hairBuffer.mUniformDataHairSimulation.mGlobalConstraintStiffness =
			clamp(simulationParameters->mGlobalConstraintStiffness, 0.0f, 1.0f);
		hairBuffer.mUniformDataHairSimulation.mGlobalConstraintRange = clamp(simulationParameters->mGlobalConstraintRange, 0.0f, 1.0f);
		hairBuffer.mUniformDataHairSimulation.mShockPropagationStrength =
			clamp(simulationParameters->mShockPropagationStrength, 0.0f, 1.0f);
		hairBuffer.mUniformDataHairSimulation.mShockPropagationAccelerationThreshold =
			max(0.0f, simulationParameters->mShockPropagationAccelerationThreshold);
		hairBuffer.mUniformDataHairSimulation.mLocalStiffness = clamp(simulationParameters->mLocalConstraintStiffness, 0.0f, 1.0f);
		hairBuffer.mUniformDataHairSimulation.mLocalConstraintIterations = simulationParameters->mLocalConstraintIterations;
		hairBuffer.mUniformDataHairSimulation.mLengthConstraintIterations = simulationParameters->mLengthConstraintIterations;
		hairBuffer.mUniformDataHairSimulation.mTipSeperationFactor = simulationParameters->mTipSeperationFactor;
		hairBuffer.mUniformDataHairSimulation.mNumVerticesPerStrand = hairBuffer.pGeom->mHair.mVertexCountPerStrand;
#if HAIR_MAX_CAPSULE_COUNT > 0
		hairBuffer.mUniformDataHairSimulation.mCapsuleCount = simulationParameters->mCapsuleCount;
#endif

		hairBuffer.mIndexCountHair = (uint)(hairBuffer.pGeom->mIndexCount);
		hairBuffer.mTotalVertexCount = (uint)hairBuffer.pGeom->mVertexCount;
		hairBuffer.mNumGuideStrands = (uint)hairBuffer.pGeom->mHair.mGuideCountPerStrand;
		hairBuffer.mStrandRadius = shadingParameters->mStrandRadius;
		hairBuffer.mStrandSpacing = shadingParameters->mStrandSpacing;
		hairBuffer.mTransform = transform;
		hairBuffer.mDisableRootColor = shadingParameters->mDisableRootColor;

#if HAIR_MAX_CAPSULE_COUNT > 0
		for (uint i = 0; i < simulationParameters->mCapsuleCount; ++i)
			hairBuffer.mCapsules[i] = simulationParameters->mCapsules[i];
#endif

		SetHairColor(&hairBuffer, (HairColor)gHairColor);

		gHair.push_back(hairBuffer);
		gHairTypeIndices[type].push_back((uint)gHair.size() - 1);
	}

	static void removeHairMeshes()
	{
		for (size_t i = 0; i < gHair.size(); ++i)
		{
			removeResource(gHair[i].pGeom);
			for (int j = 0; j < 3; ++j)
				removeResource(gHair[i].pBufferHairSimulationVertexPositions[j]);
			for (uint32_t j = 0; j < gImageCount; ++j)
			{
				removeResource(gHair[i].pUniformBufferHairShading[j]);
				removeResource(gHair[i].pUniformBufferHairSimulation[j]);
			}
		}
		gHair.set_capacity(0);
	}

	static void SetHairColor(HairBuffer* hairBuffer, HairColor hairColor)
	{
		// Fill these variables for each hair color
		float4 rootColor = float4(0.06f, 0.02f, 0.0f, 1.0f);
		float4 strandColor = float4(0.41f, 0.3f, 0.26f, 1.0f);
		float  kDiffuse = 0.14f;
		float  kSpecular1 = 0.03f;
		float  kExponent1 = 12.0f;
		float  kSpecular2 = 0.02f;
		float  kExponent2 = 20.0f;

		// Fill variables
		if (hairColor == HAIR_COLOR_BROWN)
		{
			rootColor = float4(0.06f, 0.02f, 0.0f, 1.0f);
			strandColor = float4(0.41f, 0.3f, 0.26f, 1.0f);
			kDiffuse = 0.14f;
			kSpecular1 = 0.03f;
			kExponent1 = 12.0f;
			kSpecular2 = 0.02f;
			kExponent2 = 20.0f;
		}
		else if (hairColor == HAIR_COLOR_BLONDE)
		{
			rootColor = float4(0.2f, 0.08f, 0.03f, 1.0f);
			strandColor = float4(0.9f, 0.78f, 0.66f, 1.0f);
			kDiffuse = 0.14f;
			kSpecular1 = 0.03f;
			kExponent1 = 12.0f;
			kSpecular2 = 0.02f;
			kExponent2 = 20.0f;
		}
		else if (hairColor == HAIR_COLOR_BLACK)
		{
			rootColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
			strandColor = float4(0.04f, 0.03f, 0.02f, 1.0f);
			kDiffuse = 0.14f;
			kSpecular1 = 0.03f;
			kExponent1 = 12.0f;
			kSpecular2 = 0.02f;
			kExponent2 = 20.0f;
		}
		else if (hairColor == HAIR_COLOR_RED)
		{
			rootColor = float4(0.15f, 0.0f, 0.0f, 1.0f);
			strandColor = float4(0.55f, 0.29f, 0.26f, 1.0f);
			kDiffuse = 0.14f;
			kSpecular1 = 0.03f;
			kExponent1 = 12.0f;
			kSpecular2 = 0.02f;
			kExponent2 = 20.0f;
		}

		if (hairBuffer->mDisableRootColor)
			rootColor = strandColor;

		// Set variables of uniform buffer
		rootColor[0] = clamp(rootColor[0], 0.0f, 1.0f);
		rootColor[1] = clamp(rootColor[1], 0.0f, 1.0f);
		rootColor[2] = clamp(rootColor[2], 0.0f, 1.0f);
		rootColor[3] = clamp(rootColor[3], 0.0f, 1.0f);
		hairBuffer->mUniformDataHairShading.mRootColor = uint(rootColor.getX() * 255) << 24 | uint(rootColor.getY() * 255) << 16 |
														 uint(rootColor.getZ() * 255) << 8 | uint(rootColor.getW() * 255);
		strandColor[0] = clamp(strandColor[0], 0.0f, 1.0f);
		strandColor[1] = clamp(strandColor[1], 0.0f, 1.0f);
		strandColor[2] = clamp(strandColor[2], 0.0f, 1.0f);
		strandColor[3] = clamp(strandColor[3], 0.0f, 1.0f);
		hairBuffer->mUniformDataHairShading.mStrandColor = uint(strandColor.getX() * 255) << 24 | uint(strandColor.getY() * 255) << 16 |
														   uint(strandColor.getZ() * 255) << 8 | uint(strandColor.getW() * 255);
		hairBuffer->mUniformDataHairShading.mKDiffuse = kDiffuse;
		hairBuffer->mUniformDataHairShading.mKSpecular1 = kSpecular1;
		hairBuffer->mUniformDataHairShading.mKExponent1 = kExponent1;
		hairBuffer->mUniformDataHairShading.mKSpecular2 = kSpecular2;
		hairBuffer->mUniformDataHairShading.mKExponent2 = kExponent2;
	}

	void initAnimations()
	{
		// Load rigs
		gAnimationRig.Initialize(RD_ANIMATIONS, "stickFigure/skeleton.ozz", NULL);
		
		// Load clips
		gAnimationClipNeckCrack.Initialize(RD_ANIMATIONS, "stickFigure/animations/neckCrack.ozz", NULL, &gAnimationRig);

		gAnimationClipStand.Initialize(RD_ANIMATIONS, "stickFigure/animations/stand.ozz", NULL, &gAnimationRig);

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			// Create clip controllers
			gAnimationClipControllerNeckCrack[hairType].Initialize(gAnimationClipNeckCrack.GetDuration(), NULL);
			gAnimationClipControllerStand[hairType].Initialize(gAnimationClipStand.GetDuration(), NULL);

			/*gAnimationClipControllerNeckCrack[hairType].SetPlay(false);
			gAnimationClipControllerStand[hairType].SetPlay(false);*/

			// Create animations
			AnimationDesc animationDesc{};
			animationDesc.mRig = &gAnimationRig;
			animationDesc.mNumLayers = 2;
			animationDesc.mLayerProperties[0].mClip = &gAnimationClipStand;
			animationDesc.mLayerProperties[0].mClipController = &gAnimationClipControllerStand[hairType];
			animationDesc.mLayerProperties[0].mAdditive = false;
			animationDesc.mLayerProperties[1].mClip = &gAnimationClipNeckCrack;
			animationDesc.mLayerProperties[1].mClipController = &gAnimationClipControllerNeckCrack[hairType];
			animationDesc.mLayerProperties[1].mAdditive = true;
			animationDesc.mBlendType = BlendType::EQUAL;
			gAnimation[hairType].Initialize(animationDesc);

			// Create animated object
			gAnimatedObject[hairType].Initialize(&gAnimationRig, &gAnimation[hairType]);
		}
	}

	void exitAnimations()
	{
		// Destroy clips
		gAnimationClipNeckCrack.Exit();
		gAnimationClipStand.Exit();

		// Destroy rigs, animations and animated objects
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			gAnimation[hairType].Exit();
			gAnimatedObject[hairType].Exit();
			gAnimationClipControllerNeckCrack[hairType].Reset();
			gAnimationClipControllerStand[hairType].Reset();
		}

		gAnimationRig.Exit();
	}

	//--------------------------------------------------------------------------------------------
	// LOAD FUNCTIONS
	//--------------------------------------------------------------------------------------------
	void addPipelines()
	{
		// Create vertex layouts
		VertexLayout skyboxVertexLayout = {};
		skyboxVertexLayout.mAttribCount = 1;
		skyboxVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		skyboxVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
		skyboxVertexLayout.mAttribs[0].mBinding = 0;
		skyboxVertexLayout.mAttribs[0].mLocation = 0;
		skyboxVertexLayout.mAttribs[0].mOffset = 0;

		VertexLayout skeletonVertexLayout = {};
		skeletonVertexLayout.mAttribCount = 2;
		skeletonVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		skeletonVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		skeletonVertexLayout.mAttribs[0].mBinding = 0;
		skeletonVertexLayout.mAttribs[0].mLocation = 0;
		skeletonVertexLayout.mAttribs[0].mOffset = 0;
		skeletonVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		skeletonVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		skeletonVertexLayout.mAttribs[1].mLocation = 1;
		skeletonVertexLayout.mAttribs[1].mBinding = 0;
		skeletonVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		// Create pipelines
		PipelineDesc graphicsPipelineDesc = {};
		graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;

		RasterizerStateDesc rasterizerStateCullNoneDesc = {};
		rasterizerStateCullNoneDesc.mCullMode = CULL_MODE_NONE;

		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;

		DepthStateDesc depthStateDisableDesc = {};
		depthStateDisableDesc.mDepthTest = false;
		depthStateDisableDesc.mDepthWrite = false;
		depthStateDisableDesc.mDepthFunc = CMP_LEQUAL;

		DepthStateDesc depthStateNoWriteDesc = {};
		depthStateNoWriteDesc.mDepthTest = true;
		depthStateNoWriteDesc.mDepthWrite = false;
		depthStateNoWriteDesc.mDepthFunc = CMP_LEQUAL;

		DepthStateDesc depthStateDepthResolveDesc = {};
		depthStateDepthResolveDesc.mDepthTest = true;
		depthStateDepthResolveDesc.mDepthWrite = true;
		depthStateDepthResolveDesc.mDepthFunc = CMP_LEQUAL;

		BlendStateDesc blendStateDesc = {};
		blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
		blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
		blendStateDesc.mBlendModes[0] = BM_ADD;
		blendStateDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateDesc.mMasks[0] = ALL;
		blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDesc.mIndependentBlend = false;

		BlendStateDesc blendStateDepthPeelingDesc = {};
		blendStateDepthPeelingDesc.mSrcFactors[0] = BC_ZERO;
		blendStateDepthPeelingDesc.mDstFactors[0] = BC_SRC_COLOR;
		blendStateDepthPeelingDesc.mBlendModes[0] = BM_ADD;
		blendStateDepthPeelingDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateDepthPeelingDesc.mDstAlphaFactors[0] = BC_SRC_ALPHA;
		blendStateDepthPeelingDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateDepthPeelingDesc.mMasks[0] = RED;
		blendStateDepthPeelingDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateDepthPeelingDesc.mIndependentBlend = false;

		BlendStateDesc blendStateAddDesc = {};
		blendStateAddDesc.mSrcFactors[0] = BC_ONE;
		blendStateAddDesc.mDstFactors[0] = BC_ONE;
		blendStateAddDesc.mBlendModes[0] = BM_ADD;
		blendStateAddDesc.mSrcAlphaFactors[0] = BC_ONE;
		blendStateAddDesc.mDstAlphaFactors[0] = BC_ONE;
		blendStateAddDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateAddDesc.mMasks[0] = ALL;
		blendStateAddDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateAddDesc.mIndependentBlend = false;

		BlendStateDesc blendStateColorResolveDesc = {};
		blendStateColorResolveDesc.mSrcFactors[0] = BC_ONE;
		blendStateColorResolveDesc.mDstFactors[0] = BC_SRC_ALPHA;
		blendStateColorResolveDesc.mBlendModes[0] = BM_ADD;
		blendStateColorResolveDesc.mSrcAlphaFactors[0] = BC_ZERO;
		blendStateColorResolveDesc.mDstAlphaFactors[0] = BC_ZERO;
		blendStateColorResolveDesc.mBlendAlphaModes[0] = BM_ADD;
		blendStateColorResolveDesc.mMasks[0] = ALL;
		blendStateColorResolveDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
		blendStateColorResolveDesc.mIndependentBlend = false;

		// skybox
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
		pipelineSettings.pRootSignature = pRootSignatureSkybox;
		pipelineSettings.pShaderProgram = pShaderSkybox;
		pipelineSettings.pVertexLayout = &skyboxVertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineSkybox);

		// shadow pass
		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = NULL;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetShadowMap->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureShadowPass;
		pipelineSettings.pShaderProgram = pShaderShadowPass;
		pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineShadowPass);

		// brdf
		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineBRDF);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = &depthStateDisableDesc;
		pipelineSettings.pColorFormats = NULL;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairClear;
		pipelineSettings.pShaderProgram = pShaderHairClear;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		pipelineSettings.pBlendState = NULL;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairClear);

		TinyImageFormat depthPeelingFormat = TinyImageFormat_R16_SFLOAT;

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateNoWriteDesc;
		pipelineSettings.pColorFormats = &depthPeelingFormat;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairDepthPeeling;
		pipelineSettings.pShaderProgram = pShaderHairDepthPeeling;
		pipelineSettings.pRasterizerState = &rasterizerStateDesc;
		pipelineSettings.pBlendState = &blendStateDepthPeelingDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairDepthPeeling);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = &depthStateDepthResolveDesc;
		pipelineSettings.pColorFormats = NULL;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairDepthResolve;
		pipelineSettings.pShaderProgram = pShaderHairDepthResolve;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairDepthResolve);

		TinyImageFormat fillColorsFormat = TinyImageFormat_R16G16B16A16_SFLOAT;

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateNoWriteDesc;
		pipelineSettings.pColorFormats = &fillColorsFormat;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairFillColors;
		pipelineSettings.pShaderProgram = pShaderHairFillColors;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		pipelineSettings.pBlendState = &blendStateAddDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairFillColors);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDisableDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairColorResolve;
		pipelineSettings.pShaderProgram = pShaderHairResolveColor;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		pipelineSettings.pBlendState = &blendStateColorResolveDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairColorResolve);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetHairShadows[0][0]->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairShadow;
		pipelineSettings.pShaderProgram = pShaderHairShadow;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineHairShadow);

		PipelineDesc computeDesc = {};
		computeDesc.mType = PIPELINE_TYPE_COMPUTE;
		ComputePipelineDesc& computePipelineDesc = computeDesc.mComputeDesc;
		computePipelineDesc.pRootSignature = pRootSignatureHairIntegrate;
		computePipelineDesc.pShaderProgram = pShaderHairIntegrate;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairIntegrate);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairShockPropagation;
		computePipelineDesc.pShaderProgram = pShaderHairShockPropagation;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairShockPropagation);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairLocalConstraints;
		computePipelineDesc.pShaderProgram = pShaderHairLocalConstraints;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairLocalConstraints);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairLengthConstraints;
		computePipelineDesc.pShaderProgram = pShaderHairLengthConstraints;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairLengthConstraints);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairUpdateFollowHairs;
		computePipelineDesc.pShaderProgram = pShaderHairUpdateFollowHairs;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairUpdateFollowHairs);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairPreWarm;
		computePipelineDesc.pShaderProgram = pShaderHairPreWarm;
		addPipeline(pRenderer, &computeDesc, &pPipelineHairPreWarm);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateNoWriteDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureShowCapsules;
		pipelineSettings.pShaderProgram = pShaderShowCapsules;
		pipelineSettings.pVertexLayout = &gVertexLayoutDefault;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		pipelineSettings.pBlendState = &blendStateDesc;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineShowCapsules);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = &depthStateDesc;
		pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mFormat;
		pipelineSettings.pRootSignature = pRootSignatureSkeleton;
		pipelineSettings.pShaderProgram = pShaderSkeleton;
		pipelineSettings.pVertexLayout = &skeletonVertexLayout;
		pipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
		pipelineSettings.pBlendState = NULL;
		addPipeline(pRenderer, &graphicsPipelineDesc, &pPipelineSkeleton);
		gSkeletonBatcher.LoadPipeline(pPipelineSkeleton);

		gUniformDataHairGlobal.mViewport = float4(0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineBRDF);
		removePipeline(pRenderer, pPipelineShadowPass);

		removePipeline(pRenderer, pPipelineHairClear);
		removePipeline(pRenderer, pPipelineHairDepthPeeling);
		removePipeline(pRenderer, pPipelineHairDepthResolve);
		removePipeline(pRenderer, pPipelineHairFillColors);
		removePipeline(pRenderer, pPipelineHairColorResolve);
		removePipeline(pRenderer, pPipelineHairIntegrate);
		removePipeline(pRenderer, pPipelineHairShockPropagation);
		removePipeline(pRenderer, pPipelineHairLocalConstraints);
		removePipeline(pRenderer, pPipelineHairLengthConstraints);
		removePipeline(pRenderer, pPipelineHairUpdateFollowHairs);
		removePipeline(pRenderer, pPipelineHairPreWarm);
		removePipeline(pRenderer, pPipelineShowCapsules);
		removePipeline(pRenderer, pPipelineSkeleton);
		removePipeline(pRenderer, pPipelineHairShadow);
	}

	void addRenderTargets()
	{
		RenderTargetDesc depthPeelingRenderTargetDesc = {};
		depthPeelingRenderTargetDesc.mWidth = mSettings.mWidth;
		depthPeelingRenderTargetDesc.mHeight = mSettings.mHeight;
		depthPeelingRenderTargetDesc.mDepth = 1;
		depthPeelingRenderTargetDesc.mArraySize = 1;
		depthPeelingRenderTargetDesc.mMipLevels = 1;
		depthPeelingRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		depthPeelingRenderTargetDesc.mFormat = TinyImageFormat_R16_SFLOAT;
		depthPeelingRenderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		depthPeelingRenderTargetDesc.mClearValue.r = 1.0f;
		depthPeelingRenderTargetDesc.mClearValue.g = 1.0f;
		depthPeelingRenderTargetDesc.mClearValue.b = 1.0f;
		depthPeelingRenderTargetDesc.mClearValue.a = 1.0f;
		depthPeelingRenderTargetDesc.mSampleQuality = 0;
		depthPeelingRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		depthPeelingRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		depthPeelingRenderTargetDesc.pName = "Depth peeling RT";
		addRenderTarget(pRenderer, &depthPeelingRenderTargetDesc, &pRenderTargetDepthPeeling);

#ifndef METAL
		TextureDesc hairDepthsTextureDesc = {};
		hairDepthsTextureDesc.mWidth = mSettings.mWidth;
		hairDepthsTextureDesc.mHeight = mSettings.mHeight;
		hairDepthsTextureDesc.mDepth = 1;
		hairDepthsTextureDesc.mArraySize = 3;
		hairDepthsTextureDesc.mMipLevels = 1;
		hairDepthsTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		hairDepthsTextureDesc.mFormat = TinyImageFormat_R32_UINT;
		hairDepthsTextureDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		hairDepthsTextureDesc.mClearValue.r = 1.0f;
		hairDepthsTextureDesc.mClearValue.g = 1.0f;
		hairDepthsTextureDesc.mClearValue.b = 1.0f;
		hairDepthsTextureDesc.mClearValue.a = 1.0f;
		hairDepthsTextureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        hairDepthsTextureDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		hairDepthsTextureDesc.pName = "Hair depths texture";

		TextureLoadDesc hairDepthsTextureLoadDesc = {};
		hairDepthsTextureLoadDesc.pDesc = &hairDepthsTextureDesc;
		hairDepthsTextureLoadDesc.ppTexture = &pTextureHairDepth;
		addResource(&hairDepthsTextureLoadDesc, NULL);
#else
		BufferLoadDesc hairDepthsBufferLoadDesc = {};
		hairDepthsBufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		hairDepthsBufferLoadDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight * 3;
		hairDepthsBufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		hairDepthsBufferLoadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		hairDepthsBufferLoadDesc.mDesc.mStructStride = sizeof(uint);
		hairDepthsBufferLoadDesc.mDesc.mSize = hairDepthsBufferLoadDesc.mDesc.mElementCount * hairDepthsBufferLoadDesc.mDesc.mStructStride;
		hairDepthsBufferLoadDesc.mDesc.pName = "Hair depths buffer";
		hairDepthsBufferLoadDesc.ppBuffer = &pBufferHairDepth;
		addResource(&hairDepthsBufferLoadDesc, NULL);
#endif

		RenderTargetDesc fillColorsRenderTargetDesc = {};
		fillColorsRenderTargetDesc.mWidth = mSettings.mWidth;
		fillColorsRenderTargetDesc.mHeight = mSettings.mHeight;
		fillColorsRenderTargetDesc.mDepth = 1;
		fillColorsRenderTargetDesc.mArraySize = 1;
		fillColorsRenderTargetDesc.mMipLevels = 1;
		fillColorsRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		fillColorsRenderTargetDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
		fillColorsRenderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		fillColorsRenderTargetDesc.mClearValue.r = 0.0f;
		fillColorsRenderTargetDesc.mClearValue.g = 0.0f;
		fillColorsRenderTargetDesc.mClearValue.b = 0.0f;
		fillColorsRenderTargetDesc.mClearValue.a = 0.0f;
		fillColorsRenderTargetDesc.mSampleQuality = 0;
		fillColorsRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		fillColorsRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		fillColorsRenderTargetDesc.pName = "Fill colors RT";
		addRenderTarget(pRenderer, &fillColorsRenderTargetDesc, &pRenderTargetFillColors);

		RenderTargetDesc hairShadowRenderTargetDesc = {};
		hairShadowRenderTargetDesc.mWidth = 1024;
		hairShadowRenderTargetDesc.mHeight = 1024;
		hairShadowRenderTargetDesc.mDepth = 1;
		hairShadowRenderTargetDesc.mArraySize = 1;
		hairShadowRenderTargetDesc.mMipLevels = 1;
		hairShadowRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		hairShadowRenderTargetDesc.mFormat = TinyImageFormat_D16_UNORM;
		hairShadowRenderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		hairShadowRenderTargetDesc.mClearValue.depth = 1.0f;
		hairShadowRenderTargetDesc.mClearValue.stencil = 0;
		hairShadowRenderTargetDesc.mSampleQuality = 0;
		hairShadowRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		hairShadowRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		hairShadowRenderTargetDesc.pName = "Hair shadow RT";
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
			{
				addRenderTarget(pRenderer, &hairShadowRenderTargetDesc, &pRenderTargetHairShadows[hairType][i]);
			}
		}

		RenderTargetDesc depthRenderTargetDesc = {};
		depthRenderTargetDesc.mArraySize = 1;
		depthRenderTargetDesc.mClearValue.depth = 1.0f;
		depthRenderTargetDesc.mClearValue.stencil = 0;
		depthRenderTargetDesc.mDepth = 1;
		depthRenderTargetDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRenderTargetDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRenderTargetDesc.mHeight = mSettings.mHeight;
		depthRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		depthRenderTargetDesc.mSampleQuality = 0;
		depthRenderTargetDesc.mWidth = mSettings.mWidth;
		depthRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		depthRenderTargetDesc.pName = "Depth buffer";
		addRenderTarget(pRenderer, &depthRenderTargetDesc, &pRenderTargetDepth);

		RenderTargetDesc shadowPassRenderTargetDesc = {};
		shadowPassRenderTargetDesc.mArraySize = 1;
		shadowPassRenderTargetDesc.mClearValue.depth = 1.0f;
		shadowPassRenderTargetDesc.mClearValue.stencil = 0;
		shadowPassRenderTargetDesc.mDepth = 1;
		shadowPassRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		shadowPassRenderTargetDesc.mFormat = TinyImageFormat_D32_SFLOAT;
		shadowPassRenderTargetDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
		shadowPassRenderTargetDesc.mHeight = gShadowMapDimensions;
		shadowPassRenderTargetDesc.mWidth = gShadowMapDimensions;
		shadowPassRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		shadowPassRenderTargetDesc.mSampleQuality = 0;
		shadowPassRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		shadowPassRenderTargetDesc.pName = "Shadow Map Render Target";
		addRenderTarget(pRenderer, &shadowPassRenderTargetDesc, &pRenderTargetShadowMap);
	}

	void removeRenderTargets()
	{
		removeRenderTarget(pRenderer, pRenderTargetShadowMap);
		removeRenderTarget(pRenderer, pRenderTargetDepth);

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				removeRenderTarget(pRenderer, pRenderTargetHairShadows[hairType][i]);
		}

		removeRenderTarget(pRenderer, pRenderTargetFillColors);

#ifndef METAL
		removeResource(pTextureHairDepth);
#else
		removeResource(pBufferHairDepth);
#endif

		removeRenderTarget(pRenderer, pRenderTargetDepthPeeling);
	}

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
		swapChainDesc.mColorClearValue.r = 0.0f;
		swapChainDesc.mColorClearValue.g = 0.0f;
		swapChainDesc.mColorClearValue.b = 0.0f;
		swapChainDesc.mColorClearValue.a = 0.0f;
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}
};

//--------------------------------------------------------------------------------------------
// UI
//--------------------------------------------------------------------------------------------
void GuiController::UpdateDynamicUI()
{
	if (gMaterialType != GuiController::currentMaterialType)
	{
		gDiffuseReflectionModel = gMaterialLightingModelMap[(MaterialType)gMaterialType];

		if (gMaterialType != MATERIAL_HAIR)
		{
			uiHideDynamicWidgets(&GuiController::hairShadingDynamicWidgets, pGuiWindowMain);
			uiHideDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, pGuiWindowHairSimulation);

			uiHideDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairShading, pGuiWindowMain);
			uiHideDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairSimulation, pGuiWindowHairSimulation);
		}

		if (gMaterialType == MATERIAL_HAIR)
		{
			uiShowDynamicWidgets(&GuiController::hairShadingDynamicWidgets, pGuiWindowMain);
			uiShowDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, pGuiWindowHairSimulation);

			uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairShading, pGuiWindowMain);
			uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairSimulation, pGuiWindowHairSimulation);
			gFirstHairSimulationFrame = true;
		}

		if (gMaterialType == MATERIAL_WOOD)
			uiShowDynamicWidgets(&GuiController::materialDynamicWidgets, pGuiWindowMaterial);
		else
			uiHideDynamicWidgets(&GuiController::materialDynamicWidgets, pGuiWindowMaterial);

		GuiController::currentMaterialType = (MaterialType)gMaterialType;
	}

	if (gHairType != GuiController::currentHairType)
	{
		uiHideDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairShading, pGuiWindowMain);
		uiHideDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairSimulation, pGuiWindowHairSimulation);

		gHairType = GuiController::currentHairType;
		uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairShading, pGuiWindowMain);
		uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairSimulation, pGuiWindowHairSimulation);
	}
}

void GuiController::AddGui()
{
	// Dropdown structs
	static const char* materialTypeNames[MATERIAL_COUNT] = {
		"Metals",
		"Wood",
		"Hair",
	};

	static const char*    diffuseReflectionNames[] = { "Lambert", "Oren-Nayar"};

	static const char*    renderModeNames[] = { "Shaded", "Albedo", "Normals", "Roughness", "Metallic", "AO"};

	// SCENE GUI
	CheckboxWidget     checkbox;
	SliderFloatWidget  sliderFloat;
	SliderFloat3Widget sliderFloat3;
#if HAIR_DEV_UI
	SliderUintWidget sliderUint;
	SliderIntWidget  sliderInt;
#endif

	DropdownWidget ddMatType = {};
	ddMatType.pData = &gMaterialType;
	ddMatType.pNames = materialTypeNames;
	ddMatType.mCount = sizeof(materialTypeNames) / sizeof(materialTypeNames[0]);
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMain, "Material Type", &ddMatType, WIDGET_TYPE_DROPDOWN));

	checkbox.pData = &gbAnimateCamera;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMain, "Animate Camera", &checkbox, WIDGET_TYPE_CHECKBOX));

	ButtonWidget ReloadScriptButton;
	UIWidget*     pReloadScript = uiCreateComponentWidget(pGuiWindowMain, "Reload script", &ReloadScriptButton, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnDeactivatedAfterEditCallback(pReloadScript, nullptr, ReloadScriptButtonCallback);
	luaRegisterWidget(pReloadScript);

	checkbox.pData = &gDrawSkybox;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMain, "Skybox", &checkbox, WIDGET_TYPE_CHECKBOX));

	enum
	{
		SUN_LIGHT_ENVIRONMENT_CHECKBOX,
		SUN_LIGHT_ENVIRONMENT_INTENSITY,
		SUN_LIGHT_AMBIENT_INTENSITY,
		SUN_LIGHT_DIRECTIONAL_INTENSITY,
		SUN_LIGHT_POSITION,
		SUN_LIGHT_COLOR,

		SUN_LIGHT_MAX
	};

	UIWidget* sunLightWidgets[SUN_LIGHT_MAX];
	UIWidget sunLightWidgetBases[SUN_LIGHT_MAX];

	for (int i = 0; i < SUN_LIGHT_MAX; ++i)
	{
		sunLightWidgets[i] = &sunLightWidgetBases[i];
		sunLightWidgetBases[i] = UIWidget{};
	}

	CheckboxWidget environmentLighting;
	environmentLighting.pData = &gEnvironmentLighting;

	sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_CHECKBOX].mType = WIDGET_TYPE_CHECKBOX;
	strcpy(sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_CHECKBOX].mLabel, "Environment Lighting");
	sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_CHECKBOX].pWidget = &environmentLighting;

	SliderFloatWidget environmentLightingIntensity;

	environmentLightingIntensity.pData = &gEnvironmentLightingIntensity;
	environmentLightingIntensity.mMin = 0.0f;
	environmentLightingIntensity.mMax = 1.0f;
	environmentLightingIntensity.mStep = 0.005f;

	sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_INTENSITY].mType = WIDGET_TYPE_SLIDER_FLOAT;
	strcpy(sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_INTENSITY].mLabel, "Environment Light Intensity");
	sunLightWidgetBases[SUN_LIGHT_ENVIRONMENT_INTENSITY].pWidget = &environmentLightingIntensity;

	SliderFloatWidget ambientLightIntensity;

	ambientLightIntensity.pData = &gAmbientLightIntensity;
	ambientLightIntensity.mMin = 0.0f;
	ambientLightIntensity.mMax = 1.0f;
	ambientLightIntensity.mStep = 0.005f;

	sunLightWidgetBases[SUN_LIGHT_AMBIENT_INTENSITY].mType = WIDGET_TYPE_SLIDER_FLOAT;
	strcpy(sunLightWidgetBases[SUN_LIGHT_AMBIENT_INTENSITY].mLabel, "Ambient Light Intensity");
	sunLightWidgetBases[SUN_LIGHT_AMBIENT_INTENSITY].pWidget = &ambientLightIntensity;


	SliderFloatWidget directionalLightIntensity;

	directionalLightIntensity.pData = &gDirectionalLightIntensity;
	directionalLightIntensity.mMin = 0.0f;
	directionalLightIntensity.mMax = 150.0f;
	directionalLightIntensity.mStep = 0.1f;

	sunLightWidgetBases[SUN_LIGHT_DIRECTIONAL_INTENSITY].mType = WIDGET_TYPE_SLIDER_FLOAT;
	strcpy(sunLightWidgetBases[SUN_LIGHT_DIRECTIONAL_INTENSITY].mLabel, "Directional Light Intensity");
	sunLightWidgetBases[SUN_LIGHT_DIRECTIONAL_INTENSITY].pWidget = &directionalLightIntensity;

	SliderFloat3Widget lightPosition = {};
	lightPosition.pData = &gDirectionalLightPosition;
	lightPosition.mMin = float3(-100.0f);
	lightPosition.mMax = float3(100.0f);

	sunLightWidgetBases[SUN_LIGHT_POSITION].mType = WIDGET_TYPE_SLIDER_FLOAT3;
	strcpy(sunLightWidgetBases[SUN_LIGHT_POSITION].mLabel, "Light Position");
	sunLightWidgetBases[SUN_LIGHT_POSITION].pWidget = &lightPosition;

	ColorPickerWidget colorPicker;
	colorPicker.pData = &gDirectionalLightColor;

	sunLightWidgetBases[SUN_LIGHT_COLOR].mType = WIDGET_TYPE_COLOR_PICKER;
	strcpy(sunLightWidgetBases[SUN_LIGHT_COLOR].mLabel, "Light Color");
	sunLightWidgetBases[SUN_LIGHT_COLOR].pWidget = &colorPicker;

	CollapsingHeaderWidget sunLightHeader = {};
	sunLightHeader.pGroupedWidgets = sunLightWidgets;
	sunLightHeader.mWidgetsCount = SUN_LIGHT_MAX;

	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMain, "Lighting Options", &sunLightHeader, WIDGET_TYPE_COLLAPSING_HEADER));

	// MATERIAL PROPERTIES GUI
	DropdownWidget ddReflModel = { NULL, NULL, 0 };
	ddReflModel.pData = &gDiffuseReflectionModel;
	ddReflModel.pNames = diffuseReflectionNames;
	ddReflModel.mCount = sizeof(diffuseReflectionNames) / sizeof(diffuseReflectionNames[0]);

	uiCreateDynamicWidgets(&GuiController::materialDynamicWidgets, "Diffuse Reflection Model", &ddReflModel, WIDGET_TYPE_DROPDOWN);

	DropdownWidget ddRenderMode = { NULL, NULL, 0 };
	ddRenderMode.pData = &gRenderMode;
	ddRenderMode.pNames = renderModeNames;
	ddRenderMode.mCount = sizeof(renderModeNames) / sizeof(renderModeNames[0]);

	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Render Mode", &ddRenderMode, WIDGET_TYPE_DROPDOWN));
	

	checkbox.pData = &gOverrideRoughnessTextures;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Override Roughness", &checkbox, WIDGET_TYPE_CHECKBOX));

	sliderFloat.pData = &gRoughnessOverride;
	sliderFloat.mMin = 0.04f;
	sliderFloat.mMax = 1.0f;
	sliderFloat.mStep = 0.01f;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Roughness", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

	checkbox.pData = &gDisableNormalMaps;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Disable Normal Maps", &checkbox, WIDGET_TYPE_CHECKBOX));

	sliderFloat.pData = &gNormalMapIntensity;
	sliderFloat.mMin = 0.0f;
	sliderFloat.mMax = 1.0f;
	sliderFloat.mStep = 0.01f;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Normal Map Intensity", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

	checkbox.pData = &gDisableAOMaps;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "Disable AO Maps", &checkbox, WIDGET_TYPE_CHECKBOX));

	sliderFloat.pData = &gAOIntensity;
	sliderFloat.mMin = 0.0f;
	sliderFloat.mMax = 1.0f;
	sliderFloat.mStep = 0.001f;
	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMaterial, "AO Intensity", &sliderFloat, WIDGET_TYPE_SLIDER_FLOAT));

	// HAIR GUI
	{
		GuiController::hairDynamicWidgets.resize(HAIR_TYPE_COUNT);

		static const char* hairColorNames[HAIR_COLOR_COUNT] = { "Brown", "Blonde", "Black", "Red" };

		// Hair shading widgets
		LabelWidget hairLabel;
		uiCreateDynamicWidgets(&GuiController::hairShadingDynamicWidgets, "Hair Shading", &hairLabel, WIDGET_TYPE_LABEL);

		DropdownWidget ddHairColor;
		ddHairColor.pData = &gHairColor;
		ddHairColor.pNames = hairColorNames;
		ddHairColor.mCount = HAIR_COLOR_COUNT;
		uiCreateDynamicWidgets(&GuiController::hairShadingDynamicWidgets, "Hair Color", &ddHairColor, WIDGET_TYPE_DROPDOWN);

#if HAIR_DEV_UI
		static const char* hairNames[HAIR_TYPE_COUNT] = { "Ponytail", "Female hair 1", "Female hair 2", "Female hair 3", "Female hair 6" };


		DropdownWidget ddHairType;
		ddHairType.pData = &GuiController::currentHairType;
		ddHairType.pNames = hairNames;
		ddHairType.mCount = HAIR_TYPE_COUNT;
		uiCreateDynamicWidgets(&GuiController::hairShadingDynamicWidgets, "Hair type", &ddHairType, WIDGET_TYPE_DROPDOWN);

		for (uint i = 0; i < HAIR_TYPE_COUNT; ++i)
		{
			for (size_t j = 0; j < gHairTypeIndices[i].size(); ++j)
			{
				uint                    k = gHairTypeIndices[i][j];
				UniformDataHairShading* hair = &gHair[k].mUniformDataHairShading;

				enum
				{
					HAIR_WIDGET_ROOT_COLOR,
					HAIR_WIDGET_STRAND_COLOR,
					HAIR_WIDGET_COLOR_BIAS,
					HAIR_WIDGET_K_DIFFUSE,
					HAIR_WIDGET_K_SPECULAR_1,
					HAIR_WIDGET_K_EXPONENT_1,
					HAIR_WIDGET_K_SPECULAR_2,
					HAIR_WIDGET_K_EXPONENT_2,
					HAIR_WIDGET_STRAND_RADIUS,
					HAIR_WIDGET_STRAND_SPACING,


					HAIR_WIDGET_MAX
				};

				UIWidget headerBases[HAIR_WIDGET_MAX] = {};

				ColorSliderWidget rootColor;
				rootColor.pData = &hair->mRootColor;

				headerBases[HAIR_WIDGET_ROOT_COLOR].mType = WIDGET_TYPE_COLOR_SLIDER;
				strcpy(headerBases[HAIR_WIDGET_ROOT_COLOR].mLabel, "Root Color");
				headerBases[HAIR_WIDGET_ROOT_COLOR].pWidget = &rootColor;

				ColorSliderWidget strandColor;
				strandColor.pData = &hair->mStrandColor;

				headerBases[HAIR_WIDGET_STRAND_COLOR].mType = WIDGET_TYPE_COLOR_SLIDER;
				strcpy(headerBases[HAIR_WIDGET_STRAND_COLOR].mLabel, "Strand Color");
				headerBases[HAIR_WIDGET_STRAND_COLOR].pWidget = &strandColor;

				SliderFloatWidget colorBias;
				colorBias.pData = &hair->mColorBias;
				colorBias.mMin = 0.0f;
				colorBias.mMax = 10.0f;
				colorBias.mStep = 0.01f;

				headerBases[HAIR_WIDGET_COLOR_BIAS].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_COLOR_BIAS].mLabel, "Color Bias");
				headerBases[HAIR_WIDGET_COLOR_BIAS].pWidget = &colorBias;

				SliderFloatWidget kDiffuse = {};
				kDiffuse.pData = &hair->mKDiffuse;
				kDiffuse.mMin = 0.0f;
				kDiffuse.mMax = 1.0f;

				headerBases[HAIR_WIDGET_K_DIFFUSE].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_K_DIFFUSE].mLabel, "Kd");
				headerBases[HAIR_WIDGET_K_DIFFUSE].pWidget = &kDiffuse;


				SliderFloatWidget kSpecular1;
				kSpecular1.pData = &hair->mKSpecular1;
				kSpecular1.mMin = 0.0f;
				kSpecular1.mMax = 0.1f;
				kSpecular1.mStep = 0.001f;

				headerBases[HAIR_WIDGET_K_SPECULAR_1].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_K_SPECULAR_1].mLabel, "Ks1");
				headerBases[HAIR_WIDGET_K_SPECULAR_1].pWidget = &kSpecular1;


				SliderFloatWidget kExponent1;
				kExponent1.pData = &hair->mKExponent1;
				kExponent1.mMin = 0.0f;
				kExponent1.mMax = 128.0f;
				kExponent1.mStep = 0.01f;

				headerBases[HAIR_WIDGET_K_EXPONENT_1].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_K_EXPONENT_1].mLabel, "Ex1");
				headerBases[HAIR_WIDGET_K_EXPONENT_1].pWidget = &kExponent1;

				SliderFloatWidget kSpecular2;
				kSpecular2.pData = &hair->mKSpecular2;
				kSpecular2.mMin = 0.0f;
				kSpecular2.mMax = 0.1f;
				kSpecular2.mStep = 0.001f;

				headerBases[HAIR_WIDGET_K_SPECULAR_2].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_K_SPECULAR_2].mLabel, "Ks2");
				headerBases[HAIR_WIDGET_K_SPECULAR_2].pWidget = &kSpecular2;

				SliderFloatWidget kExponent2;
				kExponent2.pData = &hair->mKExponent2;
				kExponent2.mMin = 0.0f;
				kExponent2.mMax = 128.0f;
				kExponent2.mStep = 0.01f;

				headerBases[HAIR_WIDGET_K_EXPONENT_2].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_K_EXPONENT_2].mLabel, "Ex2");
				headerBases[HAIR_WIDGET_K_EXPONENT_2].pWidget = &kExponent2;

				SliderFloatWidget strandRadius = {};
				strandRadius.pData = &gHair[k].mStrandRadius;
				strandRadius.mMin = 0.0f;
				strandRadius.mMax = 1.0f;

				headerBases[HAIR_WIDGET_STRAND_RADIUS].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_STRAND_RADIUS].mLabel, "Strand Radius");
				headerBases[HAIR_WIDGET_STRAND_RADIUS].pWidget = &strandRadius;

				SliderFloatWidget strandSpacing = {};
				strandSpacing.pData = &gHair[k].mStrandSpacing;
				strandSpacing.mMin = 0.0f;
				strandSpacing.mMax = 1.0f;

				headerBases[HAIR_WIDGET_STRAND_SPACING].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(headerBases[HAIR_WIDGET_STRAND_SPACING].mLabel, "Strand Spacing");
				headerBases[HAIR_WIDGET_STRAND_SPACING].pWidget = &strandSpacing;

				CollapsingHeaderWidget header = {};
				UIWidget* headerWidgets[HAIR_WIDGET_MAX];
				for (int i = 0; i < HAIR_WIDGET_MAX; ++i)
					headerWidgets[i] = &headerBases[i];

				header.pGroupedWidgets = headerWidgets;
				header.mWidgetsCount = HAIR_WIDGET_MAX;
				char name[32];
				snprintf(name, 32, "Head %zu color", j);

				uiCreateDynamicWidgets(
					&GuiController::hairDynamicWidgets[i].hairShading, name, &header, WIDGET_TYPE_COLLAPSING_HEADER);
			}
		}
#endif

		// Hair simulation widgets
		LabelWidget hairSim;
		uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Hair Simulation", &hairSim, WIDGET_TYPE_LABEL);

		sliderFloat3.pData = (float3*)&gUniformDataHairGlobal.mGravity;    //-V641 //-V1027
		sliderFloat3.mMin = float3(-10.0f);
		sliderFloat3.mMax = float3(10.0f);
		uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Gravity", &sliderFloat3, WIDGET_TYPE_SLIDER_FLOAT3);

		sliderFloat3.pData = (float3*)&gUniformDataHairGlobal.mWind;    //-V641 //-V1027
		sliderFloat3.mMin = float3(-1024.0f);
		sliderFloat3.mMax = float3(1024.0f);
		uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Wind", &sliderFloat3, WIDGET_TYPE_SLIDER_FLOAT3);

#if HAIR_MAX_CAPSULE_COUNT > 0
		checkbox.pData = &gShowCapsules;
		uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Show Collision Capsules", &checkbox, WIDGET_TYPE_CHECKBOX);
#endif

#if HAIR_DEV_UI

		{
			enum
			{
				HAIR_TRANSFORM_POSITION,
				HAIR_TRANSFORM_ORIENTATION,
				HAIR_TRANSFORM_SCALE,
				HAIR_TRANSFORM_ATTACHMENT,

				HAIR_TRANSFORM_MAX
			};

			static const uint32_t maxTransforms = 32;

			ASSERT(gTransforms.size() <= maxTransforms);

			UIWidget subHeaderBases[maxTransforms];
			typedef UIWidget* SubWidgets[HAIR_TRANSFORM_MAX];

			SubWidgets subWidgets[maxTransforms];
			UIWidget subWidgetBases[maxTransforms][HAIR_TRANSFORM_MAX];

			CollapsingHeaderWidget	subHeaders[maxTransforms];
			SliderFloat3Widget		positions[maxTransforms];
			SliderFloat3Widget		orientations[maxTransforms];
			SliderFloatWidget		scales[maxTransforms];
			SliderIntWidget			attachments[maxTransforms];

			for (size_t i = 0; i < gTransforms.size(); ++i)
			{
				Transform*	transform = &gTransforms[i].mTransform;
				UIWidget*	pBase = &subWidgetBases[i][0];

				SliderFloat3Widget* pPos = &positions[i];
				*pPos = SliderFloat3Widget{};
				*pBase = UIWidget{};
				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT3;
				strcpy(pBase->mLabel, "Position");
				pBase->pWidget = pPos;

				pPos->pData = (float3*)&transform->mPosition;
				pPos->mMin = float3(-10.0f);
				pPos->mMax = float3(10.0f);

				++pBase;


				SliderFloat3Widget* pRot = &orientations[i];
				*pRot = SliderFloat3Widget{};
				*pBase = UIWidget{};
				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT3;
				strcpy(pBase->mLabel, "Orientation");
				pBase->pWidget = pRot;

				pRot->pData = (float3*)&transform->mOrientation;
				pRot->mMin = float3(-PI * 2.0f);
				pRot->mMax = float3(PI * 2.0f);

				++pBase;

				SliderFloatWidget* pScale = &scales[i];
				*pScale = SliderFloatWidget{};
				*pBase = UIWidget{};
				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(pBase->mLabel, "Scale");
				pBase->pWidget = pScale;

				pScale->pData = &transform->mScale;
				pScale->mMin = 0.001f;
				pScale->mMax = 1.0f;
				pScale->mStep = 0.001f;

				++pBase;

				SliderIntWidget* pAttachment = &attachments[i];
				*pAttachment = SliderIntWidget{};
				*pBase = UIWidget{};
				pBase->mType = WIDGET_TYPE_SLIDER_INT;
				strcpy(pBase->mLabel, "Attached To Bone");
				pBase->pWidget = pScale;

				pAttachment->pData = &gTransforms[i].mAttachedBone;
				pAttachment->mMin = -1;
				pAttachment->mMax = gAnimationRig[0].GetNumJoints() - 1;


				for (int j = 0; j < HAIR_TRANSFORM_MAX; ++j)
					subWidgets[i][j] = &subWidgetBases[i][j];

				pBase = &subHeaderBases[i];
				CollapsingHeaderWidget* pHeader = &subHeaders[i];

				*pBase = UIWidget{};
				*pHeader = CollapsingHeaderWidget{};
				pBase->mType = WIDGET_TYPE_COLLAPSING_HEADER;
				pBase->pWidget = pHeader;
				snprintf(pBase->mLabel, sizeof(pBase->mLabel), "Transform %zu", i);

				pHeader->pGroupedWidgets = subWidgets[i];
				pHeader->mWidgetsCount = HAIR_TRANSFORM_MAX;
			}

			UIWidget* headerWidgets[maxTransforms];
			for (uint32_t i = 0; i < (uint32_t)gTransforms.size(); ++i)
				headerWidgets[i] = &subHeaderBases[i];

			CollapsingHeaderWidget transformHeader = {};
			transformHeader.pGroupedWidgets = headerWidgets;
			transformHeader.mWidgetsCount = (uint32_t)gTransforms.size();

			uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Transforms", &transformHeader, WIDGET_TYPE_COLLAPSING_HEADER);
		}

#if HAIR_MAX_CAPSULE_COUNT > 0

		{
			ASSERT(gCapsules.size() <= HAIR_MAX_CAPSULE_COUNT);

			enum
			{
				CAPSULE_CENTER_0,
				CAPSULE_RADIUS_0,
				CAPSULE_CENTER_1,
				CAPSULE_RADIUS_1,
				CAPSULE_RADIUS_ATTACHMENT,

				CAPSULE_WIDGET_COUNT
			};
			CollapsingHeaderWidget subHeaders[HAIR_MAX_CAPSULE_COUNT] = {};
			UIWidget subHeaderBases[HAIR_MAX_CAPSULE_COUNT] = {};

			UIWidget widgetBases[HAIR_MAX_CAPSULE_COUNT][CAPSULE_WIDGET_COUNT] = {};
			UIWidget* widgetGroups[HAIR_MAX_CAPSULE_COUNT][CAPSULE_WIDGET_COUNT] = {};
			SliderFloat3Widget centers0[HAIR_MAX_CAPSULE_COUNT] = {};
			SliderFloatWidget radii0[HAIR_MAX_CAPSULE_COUNT] = {};
			SliderFloat3Widget centers1[HAIR_MAX_CAPSULE_COUNT] = {};
			SliderFloatWidget radii1[HAIR_MAX_CAPSULE_COUNT] = {};
			SliderIntWidget   attachments[HAIR_MAX_CAPSULE_COUNT] = {};

			for (size_t i = 0; i < gCapsules.size(); ++i)
			{
				Capsule*	capsule = &gCapsules[i].mCapsule;
				UIWidget*	pBase = &widgetBases[i][0];

				centers0[i].pData = &capsule->mCenter0;
				centers0[i].mMin = float3(-10.0f);
				centers0[i].mMax = float3(10.0f);

				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT3;
				strcpy(pBase->mLabel, "Center0");
				pBase->pWidget = &centers0[i];

				++pBase;

				radii0[i].pData = &capsule->mRadius0;
				radii0[i].mMin = 0.0f;
				radii0[i].mMax = 10.0f;
				radii0[i].mStep = 0.01f;

				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(pBase->mLabel, "Radius0");
				pBase->pWidget = &radii0[i];

				++pBase;

				centers1[i].pData = &capsule->mCenter1;
				centers1[i].mMin = float3(-10.0f);
				centers1[i].mMax = float3(10.0f);

				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT3;
				strcpy(pBase->mLabel, "Center1");
				pBase->pWidget = &centers1[i];

				++pBase;

				radii1[i].pData = &capsule->mRadius0;
				radii1[i].mMin = 0.0f;
				radii1[i].mMax = 10.0f;
				radii0[i].mStep = 0.01f;

				pBase->mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(pBase->mLabel, "Radius1");
				pBase->pWidget = &radii1[i];

				++pBase;

				attachments[i].pData = &gCapsules[i].mAttachedBone;
				attachments[i].mMin = -1;
				attachments[i].mMax = gAnimationRig[0].GetNumJoints() - 1;

				pBase->mType = WIDGET_TYPE_SLIDER_INT;
				strcpy(pBase->mLabel, "Attached To Bone");
				pBase->pWidget = &attachments[i];

				for (int j = 0; j < CAPSULE_WIDGET_COUNT; ++j)
					widgetGroups[i][j] = &widgetBases[i][j];

				subHeaders[i].pGroupedWidgets = widgetGroups[i];
				subHeaders[i].mWidgetsCount = CAPSULE_WIDGET_COUNT;

				subHeaderBases[i].pWidget = &subHeaders[i];
				subHeaderBases[i].mType = WIDGET_TYPE_COLLAPSING_HEADER;
				snprintf(subHeaderBases[i].mLabel, sizeof(subHeaderBases[i].mLabel), "Capsule %zu", i);
			}

			UIWidget* capsuleWidgets[HAIR_MAX_CAPSULE_COUNT];
			for (size_t i = 0; i < gCapsules.size(); ++i)
				capsuleWidgets[i] = &subHeaderBases[i];

			CollapsingHeaderWidget capsuleHeader = {};
			capsuleHeader.pGroupedWidgets = capsuleWidgets;
			capsuleHeader.mWidgetsCount = (uint32_t)gCapsules.size();

			uiCreateDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, "Capsules", &capsuleHeader, WIDGET_TYPE_COLLAPSING_HEADER);
		}
#endif

		for (uint i = 0; i < HAIR_TYPE_COUNT; ++i)
		{
			for (size_t j = 0; j < gHairTypeIndices[i].size(); ++j)
			{
				uint                       k = gHairTypeIndices[i][j];
				UniformDataHairSimulation* hair = &gHair[k].mUniformDataHairSimulation;


				enum
				{
					WIDGET_DAMPING,
					WIDGET_STIFFNESS,
					WIDGET_RANGE,
					WIDGET_SHOCK_STRENGTH,
					WIDGET_ACCELERATION_THRESHOLD,
					WIDGET_LOCAL_STIFFNESS,
					WIDGET_LOCAL_ITERATIONS,
					WIDGET_LENGTH_ITERATIONS,

					WIDGET_MAX
				};

				UIWidget widgetBases[WIDGET_MAX] = {};


				SliderFloatWidget damping = {};
				damping.pData = &hair->mDamping;
				damping.mMin = 0.0f;
				damping.mMax = 0.1f;
				damping.mStep = 0.001f;
				
				widgetBases[WIDGET_DAMPING].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_DAMPING].mLabel, "Damping");
				widgetBases[WIDGET_DAMPING].pWidget = &damping;

				SliderFloatWidget globalStiffness = {};
				globalStiffness.pData = &hair->mGlobalConstraintStiffness;
				globalStiffness.mMin = 0.0f;
				globalStiffness.mMax = 0.1f;
				globalStiffness.mStep = 0.001f;

				widgetBases[WIDGET_STIFFNESS].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_STIFFNESS].mLabel, "Global Constraint Stiffness");
				widgetBases[WIDGET_STIFFNESS].pWidget = &globalStiffness;

				SliderFloatWidget globalRange = {};
				globalRange.pData = &hair->mGlobalConstraintRange;
				globalRange.mMin = 0.0f;
				globalRange.mMax = 1.0f;
				globalRange.mStep = 0.01f;

				widgetBases[WIDGET_RANGE].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_RANGE].mLabel, "Global Constraint Range");
				widgetBases[WIDGET_RANGE].pWidget = &globalRange;

				SliderFloatWidget shockStrength = {};
				shockStrength.pData = &hair->mShockPropagationStrength;
				shockStrength.mMin = 0.0f;
				shockStrength.mMax = 1.0f;
				shockStrength.mStep = 0.01f;

				widgetBases[WIDGET_SHOCK_STRENGTH].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_SHOCK_STRENGTH].mLabel, "Shock Propagation Strength");
				widgetBases[WIDGET_SHOCK_STRENGTH].pWidget = &shockStrength;

				SliderFloatWidget accelerationThreshold = {};
				accelerationThreshold.pData = &hair->mShockPropagationAccelerationThreshold;
				accelerationThreshold.mMin = 0.0f;
				accelerationThreshold.mMax = 10.0f;
				accelerationThreshold.mStep = 0.01f;

				widgetBases[WIDGET_ACCELERATION_THRESHOLD].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_ACCELERATION_THRESHOLD].mLabel, "Shock Propagation Acceleration Threshold");
				widgetBases[WIDGET_ACCELERATION_THRESHOLD].pWidget = &accelerationThreshold;

				SliderFloatWidget localStiffness = {};
				localStiffness.pData = &hair->mLocalStiffness;
				localStiffness.mMin = 0.0f;
				localStiffness.mMax = 1.0f;
				localStiffness.mStep = 0.01f;

				widgetBases[WIDGET_LOCAL_STIFFNESS].mType = WIDGET_TYPE_SLIDER_FLOAT;
				strcpy(widgetBases[WIDGET_LOCAL_STIFFNESS].mLabel, "Local Stiffness");
				widgetBases[WIDGET_LOCAL_STIFFNESS].pWidget = &localStiffness;

				SliderUintWidget localIterations = {};
				localIterations.pData = &hair->mLocalConstraintIterations;
				localIterations.mMin = 0;
				localIterations.mMax = 32;
				localIterations.mStep = 1;

				widgetBases[WIDGET_LOCAL_ITERATIONS].mType = WIDGET_TYPE_SLIDER_UINT;
				strcpy(widgetBases[WIDGET_LOCAL_ITERATIONS].mLabel, "Local Constraint Iterations");
				widgetBases[WIDGET_LOCAL_ITERATIONS].pWidget = &localIterations;

				SliderUintWidget lengthIterations = {};
				lengthIterations.pData = &hair->mLengthConstraintIterations;
				lengthIterations.mMin = 1;
				lengthIterations.mMax = 32;

				widgetBases[WIDGET_LENGTH_ITERATIONS].mType = WIDGET_TYPE_SLIDER_UINT;
				strcpy(widgetBases[WIDGET_LENGTH_ITERATIONS].mLabel, "Length Constraint Iterations");
				widgetBases[WIDGET_LENGTH_ITERATIONS].pWidget = &localIterations;


				UIWidget* widgets[WIDGET_MAX];
				for (int i = 0; i < WIDGET_MAX; ++i)
					widgets[i] = &widgetBases[i];

				CollapsingHeaderWidget header = {};
				header.pGroupedWidgets = widgets;
				header.mWidgetsCount = WIDGET_MAX;
				char headerLabel[32];
				snprintf(headerLabel, sizeof(headerLabel), "Hair %u", k);

				uiCreateDynamicWidgets(
					&GuiController::hairDynamicWidgets[i].hairSimulation, headerLabel /*gHair[k].mName*/, &header, WIDGET_TYPE_COLLAPSING_HEADER);
			}
		}
#endif
	}

	if (gMaterialType == MaterialType::MATERIAL_HAIR)
	{
		GuiController::currentMaterialType = MaterialType::MATERIAL_HAIR;
		uiShowDynamicWidgets(&GuiController::hairShadingDynamicWidgets, pGuiWindowMain);
		uiShowDynamicWidgets(&GuiController::hairSimulationDynamicWidgets, pGuiWindowHairSimulation);
		uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairShading, pGuiWindowMain);
		uiShowDynamicWidgets(&GuiController::hairDynamicWidgets[gHairType].hairSimulation, pGuiWindowHairSimulation);
	}

	DropdownWidget ddTestScripts;
	ddTestScripts.pData = &gCurrentScriptIndex;
	ddTestScripts.pNames = gTestScripts;
	ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);

	luaRegisterWidget(uiCreateComponentWidget(pGuiWindowMain, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

	ButtonWidget bRunScript;
	UIWidget*     pRunScript = uiCreateComponentWidget(pGuiWindowMain, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
	uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
	luaRegisterWidget(pRunScript);
}

void GuiController::Exit()
{
	uiDestroyDynamicWidgets(&hairShadingDynamicWidgets);
	uiDestroyDynamicWidgets(&hairSimulationDynamicWidgets);
	uiDestroyDynamicWidgets(&materialDynamicWidgets);
	for (HairDynamicWidgets& w : hairDynamicWidgets)
	{
		uiDestroyDynamicWidgets(&w.hairShading);
		uiDestroyDynamicWidgets(&w.hairSimulation);
	}
	hairDynamicWidgets.set_capacity(0);
}

DEFINE_APPLICATION_MAIN(MaterialPlayground)
