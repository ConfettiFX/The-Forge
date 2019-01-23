/*
*
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

/********************************************************************************************************
*
* The Forge - MATERIALS UNIT TEST
*
* The purpose of this demo is to show the material workflow of The-Forge,
* featuring PBR materials and environment lighting.
*
*********************************************************************************************************/

//asimp importer
#include "../../../../Common_3/Tools/AssimpImporter/AssimpImporter.h"
#include "../../../../Common_3/Tools/TFXImporter/TFXImporter.h"

//tiny stl
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../../../Common_3/ThirdParty/OpenSource/TinySTL/string.h"

//Interfaces
#include "../../../../Common_3/OS/Interfaces/ICameraController.h"
#include "../../../../Common_3/OS/Interfaces/ILogManager.h"
#include "../../../../Common_3/OS/Interfaces/IFileSystem.h"
#include "../../../../Common_3/OS/Interfaces/ITimeManager.h"
#include "../../../../Middleware_3/UI/AppUI.h"
#include "../../../../Common_3/OS/Core/DebugRenderer.h"
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/Renderer/ResourceLoader.h"
#include "../../../../Common_3/OS/Interfaces/IApp.h"

//Renderer
#include "../../../../Common_3/Renderer/GpuProfiler.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"

//input
#include "../../../../Middleware_3/Input/InputSystem.h"
#include "../../../../Middleware_3/Input/InputMappings.h"

// Animations
#undef min
#undef max
#include "../../../../Middleware_3/Animation/SkeletonBatcher.h"
#include "../../../../Middleware_3/Animation/AnimatedObject.h"
#include "../../../../Middleware_3/Animation/Animation.h"
#include "../../../../Middleware_3/Animation/Clip.h"
#include "../../../../Middleware_3/Animation/ClipController.h"
#include "../../../../Middleware_3/Animation/Rig.h"

#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"

//LUA
#include "../../../../Middleware_3/LUA/LuaManager.h"

//mmgr
#include "../../../../Common_3/OS/Interfaces/IMemoryManager.h"    // Must be the last include in a cpp file

// define folders for resources
const char* pszBases[FSR_Count] = {
	"../../../src/06_MaterialPlayground/",    // FSR_BinShaders
	"../../../src/06_MaterialPlayground/",    // FSR_SrcShaders
	"../../../UnitTestResources/",            // FSR_Textures
	"../../../UnitTestResources/",            // FSR_Meshes
	"../../../UnitTestResources/",            // FSR_Builtin_Fonts
	"../../../src/06_MaterialPlayground/",    // FSR_GpuConfig
	"../../../UnitTestResources/",            // FSR_Animtion
	"../../../../../Art/",                    // FSR_OtherFiles
	"../../../../../Middleware_3/Text/",      // FSR_MIDDLEWARE_TEXT
	"../../../../../Middleware_3/UI/",        // FSR_MIDDLEWARE_UI
#if defined(TARGET_IOS) || defined(_DURANGO)
	"",    // FSR_Middleware2 //used to load lua scripts
#else
	"../../../src/06_MaterialPlayground/",    // FSR_Middleware2 //used to load lua scripts
#endif
};

// quick way to skip loading the assets
#define MAX_NUM_POINT_LIGHTS 8          // >= 1
#define MAX_NUM_DIRECTIONAL_LIGHTS 1    // >= 1
#define HAIR_MAX_CAPSULE_COUNT 3
#define HAIR_DEV_UI false

#define ENABLE_DIFFUSE_REFLECTION_DROPDOWN 1

#if defined(TARGET_IOS) || defined(ANDROID)
#define TEXTURE_RESOLUTION "1K"
#else
#define TEXTURE_RESOLUTION "2K"
#endif

//--------------------------------------------------------------------------------------------
// MATERIAL DEFINTIONS
//--------------------------------------------------------------------------------------------

typedef enum MaterialTypes
{
	MATERIAL_METAL = 0,
	MATERIAL_WOOD,
	MATERIAL_HAIR,
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

static const int MATERIAL_INSTANCE_COUNT = sizeof(metalEnumNames) / sizeof(metalEnumNames[0]) - 1;

const uint32_t gImageCount = 3;

//--------------------------------------------------------------------------------------------
// STRUCT DEFINTIONS
//--------------------------------------------------------------------------------------------
struct Vertex
{
	float3 mPos;
	float3 mNormal;
	float2 mUv;
};

typedef struct MeshData
{
	Buffer* pVertexBuffer = NULL;
	uint    mVertexCount = 0;
	Buffer* pIndexBuffer = NULL;
	uint    mIndexCount = 0;
} MeshData;

struct UniformCamData
{
	mat4  mProjectView;
	mat4  mInvProjectView;
	vec3  mCamPos;
	int   bUseEnvMap = 0;
	float fAOIntensity = 0.01f;
	int   iRenderMode;
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

	// specifies which diffuse reflection model to use
	OREN_NAYAR = (1 << 5),    // Lambert otherwise, we just check if this flag is set for now

	NUM_TEXTURE_CONFIG_FLAGS = 6
};

#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
enum EDiffuseReflectionModels
{
	LAMBERT_REFLECTION = 0,
	OREN_NAYAR_REFLECTION,

	DIFFUSE_REFLECTION_MODEL_COUNT
};
#endif

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
	float3 _pad;
};

struct UniformDataPointLights
{
	PointLight mPointLights[MAX_NUM_POINT_LIGHTS];
	uint       mNumPointLights = 0;
};

struct UniformDataDirectionalLights
{
	DirectionalLight mDirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
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
	tinystl::string mName;
	Capsule         mCapsule;
	int             mAttachedBone = -1;
};

struct Transform
{
	vec3  mPosition;
	vec3  mOrientation;
	float mScale;
};

struct NamedTransform
{
	tinystl::string mName;
	Transform       mTransform;
	int             mAttachedBone = -1;
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
	tinystl::string           mName = NULL;
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
	UniformDataHairShading    mUniformDataHairShading;
	UniformDataHairSimulation mUniformDataHairSimulation;
	uint                      mIndexCountHair = 0;
	uint                      mTotalVertexCount = 0;
	uint                      mNumGuideStrands = 0;
	float                     mStrandRadius;
	float                     mStrandSpacing;
	uint                      mTransform;    // Index into gTransforms
	bool                      mDisableRootColor;
#if HAIR_MAX_CAPSULE_COUNT > 0
	uint mCapsules[HAIR_MAX_CAPSULE_COUNT];    // World space capsules
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
	float  mKDiffuse;       // Diffude light contribution
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
	float mShockPropagationStrength;                 // Force propgating sudden changes to the rest of the strand
	float mShockPropagationAccelerationThreshold;    // Threshold at which to start shock propagation
	float mLocalConstraintStiffness;                 // Force keeping strands in the rest shape
	uint  mLocalConstraintIterations;                // Number of local constraint iterations
	uint  mLengthConstraintIterations;               // Number of length constraint iterations
	float mTipSeperationFactor;                      // Seperates follow hairs from their guide hair
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
CmdPool*   pCmdPool = NULL;
Cmd**      ppCmds = NULL;
CmdPool*   pUICmdPool = NULL;
Cmd**      ppUICmds = NULL;
SwapChain* pSwapChain = NULL;
Fence*     pRenderCompleteFences[gImageCount] = { NULL };
Semaphore* pImageAcquiredSemaphore = NULL;
Semaphore* pRenderCompleteSemaphores[gImageCount] = { NULL };
uint32_t   gFrameIndex = 0;

//--------------------------------------------------------------------------------------------
// THE FORGE OBJECTS
//--------------------------------------------------------------------------------------------
LogManager         gLogManager;
UIApp              gAppUI;
ICameraController* pCameraController = NULL;
TextDrawDesc       gFrameTimeDraw = TextDrawDesc(0, 0xff00ff00, 18);
TextDrawDesc       gErrMsgDrawDesc = TextDrawDesc(0, 0xff0000ee, 18);
GpuProfiler*       pGpuProfiler = NULL;
GuiComponent*      pGuiWindowMain = NULL;
GuiComponent*      pGuiWindowHairSimulation = NULL;
GuiComponent*      pGuiWindowMaterial = NULL;
LuaManager         gLuaManager;
#ifdef TARGET_IOS
VirtualJoystickUI gVirtualJoystick;
#endif

//--------------------------------------------------------------------------------------------
// RASTERIZER STATES
//--------------------------------------------------------------------------------------------
RasterizerState* pRasterizerStateCullNone = NULL;
RasterizerState* pRasterizerStateCullFront = NULL;

//--------------------------------------------------------------------------------------------
// DEPTH STATES
//--------------------------------------------------------------------------------------------
DepthState* pDepthStateEnable = NULL;
DepthState* pDepthStateDisable = NULL;
DepthState* pDepthStateNoWrite = NULL;
DepthState* pDepthStateDepthResolve = NULL;

//--------------------------------------------------------------------------------------------
// BLEND STATES
//--------------------------------------------------------------------------------------------
BlendState* pBlendStateAlphaBlend = NULL;
BlendState* pBlendStateDepthPeeling = NULL;
BlendState* pBlendStateAdd = NULL;
BlendState* pBlendStateColorResolve = NULL;

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
#ifndef DIRECT3D11
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
#endif

//--------------------------------------------------------------------------------------------
// ROOT SIGNATURES
//--------------------------------------------------------------------------------------------
RootSignature* pRootSignatureSkybox = NULL;
RootSignature* pRootSignatureBRDF = NULL;
#ifndef DIRECT3D11
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
#endif

//--------------------------------------------------------------------------------------------
// PIPELINES
//--------------------------------------------------------------------------------------------
Pipeline* pPipelineSkybox = NULL;
Pipeline* pPipelineBRDF = NULL;
#ifndef DIRECT3D11
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
#endif

//--------------------------------------------------------------------------------------------
// RENDER TARGETS
//--------------------------------------------------------------------------------------------
RenderTarget* pRenderTargetDepth = NULL;
RenderTarget* pRenderTargetDepthPeeling = NULL;
RenderTarget* pRenderTargetFillColors = NULL;
RenderTarget* pRenderTargetHairShadows[HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS] = { NULL };
#ifndef METAL
Texture* pTextureHairDepth = NULL;
#else
Buffer* pBufferHairDepth = NULL;
#endif

//--------------------------------------------------------------------------------------------
// VERTEX BUFFERS
//--------------------------------------------------------------------------------------------
Buffer*                     pVertexBufferSkybox = NULL;
tinystl::vector<HairBuffer> gHair;
Buffer*                     pVertexBufferSkeletonJoint = NULL;
int                         gVertexCountSkeletonJoint = 0;
Buffer*                     pVertexBufferSkeletonBone = NULL;
int                         gVertexCountSkeletonBone = 0;

//--------------------------------------------------------------------------------------------
// INDEX BUFFERS
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// MESHES
//--------------------------------------------------------------------------------------------
tinystl::vector<MeshData*> pMeshes;

//--------------------------------------------------------------------------------------------
// UNIFORM BUFFERS
//--------------------------------------------------------------------------------------------
Buffer* pUniformBufferCamera[gImageCount] = { NULL };
Buffer* pUniformBufferCameraSkybox[gImageCount] = { NULL };
Buffer* pUniformBufferCameraHairShadows[gImageCount][HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS] = {};
Buffer* pUniformBufferGroundPlane = NULL;
Buffer* pUniformBufferMatBall[gImageCount][MATERIAL_INSTANCE_COUNT];
Buffer* pUniformBufferNamePlates[MATERIAL_INSTANCE_COUNT];
Buffer* pUniformBufferPointLights = NULL;
Buffer* pUniformBufferDirectionalLights = NULL;
Buffer* pUniformBufferHairGlobal = NULL;

//--------------------------------------------------------------------------------------------
// TEXTURES
//--------------------------------------------------------------------------------------------
const int gMaterialTextureCount = MATERIAL_INSTANCE_COUNT * MATERIAL_TEXTURE_COUNT * MATERIAL_COUNT;

Texture*                  pTextureSkybox = NULL;
Texture*                  pTextureBRDFIntegrationMap = NULL;
tinystl::vector<Texture*> pTextureMaterialMaps;          // objects
tinystl::vector<Texture*> pTextureMaterialMapsGround;    // ground

Texture* pTextureIrradianceMap = NULL;
Texture* pTextureSpecularMap = NULL;

//--------------------------------------------------------------------------------------------
// UNIFORM DATA
//--------------------------------------------------------------------------------------------
UniformCamData               gUniformDataCamera;
UniformCamData               gUniformDataCameraSkybox;
UniformCamData               gUniformDataCameraHairShadows[HAIR_TYPE_COUNT][MAX_NUM_DIRECTIONAL_LIGHTS];
UniformDataPointLights       gUniformDataPointLights;
UniformObjData               gUniformDataObject;
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
Rig             gAnimationRig[HAIR_TYPE_COUNT];
AnimatedObject  gAnimatedObject[HAIR_TYPE_COUNT];
SkeletonBatcher gSkeletonBatcher;

tinystl::vector<NamedCapsule>   gCapsules;
tinystl::vector<NamedTransform> gTransforms;
tinystl::vector<Capsule>        gFinalCapsules[HAIR_TYPE_COUNT];    // Stores the capsule transformed by the bone matrix

//--------------------------------------------------------------------------------------------
// UI & OTHER
//--------------------------------------------------------------------------------------------
bool                  gVSyncEnabled = false;
bool                  gShowCapsules = false;
uint                  gHairType = 0;
tinystl::vector<uint> gHairTypeIndices[HAIR_TYPE_COUNT];
HairTypeInfo          gHairTypeInfo[HAIR_TYPE_COUNT];
bool                  gEnvironmentLighting = false;
bool                  gDrawSkybox = true;
uint32_t              gMaterialType = MATERIAL_METAL;
#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
uint32_t gDiffuseReflectionModel = LAMBERT_REFLECTION;
#endif
bool gbLuaScriptingSystemLoadedSuccessfully = false;
bool gbAnimateCamera = true;

const int    gSphereResolution = 30;    // Increase for higher resolution spheres
const float  gSphereDiameter = 0.5f;
TextDrawDesc gMaterialPropDraw = TextDrawDesc(0, 0xffaaaaaa, 32);
float3       gDirectionalLightPosition = float3(0.0f, 10.0f, 10.0f);

uint32_t gRenderMode = 0;
bool     gOverrideRoughnessTextures = false;
float    gRoughnessOverride = 0.04f;
bool     gDisableNormalMaps = false;
bool     gDisableAOMaps = false;
float    gAOIntensity = 0.01f;

const char* pTextureName[] = { "albedoMap", "normalMap", "metallicMap", "roughnessMap", "aoMap" };

uint32_t       gHairColor = HAIR_COLOR_BROWN;
uint32_t       gLastHairColor = gHairColor;
bool           gFirstHairSimulationFrame = true;
GPUPresetLevel gGPUPresetLevel;

mat4                  gTextProjView;
tinystl::vector<mat4> gTextWorldMats;

void ReloadScriptButtonCallback() { gLuaManager.ReloadUpdatableScript(); }

// Generates an array of vertices and normals for a sphere
void createSpherePoints(Vertex** ppPoints, int* pNumberOfPoints, int numberOfDivisions, float radius = 1.0f)
{
	tinystl::vector<Vector3> vertices;
	tinystl::vector<Vector3> normals;
	tinystl::vector<Vector3> uvs;

	float numStacks = (float)numberOfDivisions;
	float numSlices = (float)numberOfDivisions;

	for (int i = 0; i < numberOfDivisions; i++)
	{
		for (int j = 0; j < numberOfDivisions; j++)
		{
			// Sectioned into quads, utilizing two triangles
			Vector3 topLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)),
									 (float)(-cos(PI * (j + 1.0f) / numSlices)),
									 (float)(sin(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)) };
			Vector3 topRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)),
									  (float)(-cos(PI * (j + 1.0) / numSlices)),
									  (float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)) };
			Vector3 botLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)), (float)(-cos(PI * j / numSlices)),
									 (float)(sin(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)) };
			Vector3 botRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)),
									  (float)(-cos(PI * j / numSlices)),
									  (float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)) };

			// Top right triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botRightPoint);
			vertices.push_back(radius * topRightPoint);

			normals.push_back(normalize(topLeftPoint));
			float   theta = atan2f(normalize(topLeftPoint).getY(), normalize(topLeftPoint).getX());
			float   phi = acosf(normalize(topLeftPoint).getZ());
			Vector3 textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(), normalize(botRightPoint).getX());
			phi = acosf(normalize(botRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(topRightPoint));
			theta = atan2f(normalize(topRightPoint).getY(), normalize(topRightPoint).getX());
			phi = acosf(normalize(topRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);

			// Bot left triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botLeftPoint);
			vertices.push_back(radius * botRightPoint);

			normals.push_back(normalize(topLeftPoint));
			theta = atan2f(normalize(topLeftPoint).getY(), normalize(topLeftPoint).getX());
			phi = acosf(normalize(topLeftPoint).getZ());
			textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botLeftPoint));
			theta = atan2f(normalize(botLeftPoint).getY(), normalize(botLeftPoint).getX());
			phi = acosf(normalize(botLeftPoint).getZ());
			textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);

			normals.push_back(normalize(botRightPoint));
			theta = atan2f(normalize(botRightPoint).getY(), normalize(botRightPoint).getX());
			phi = acosf(normalize(botRightPoint).getZ());
			textcoord1 = { (theta / (2 * PI)), (phi / PI), 0.0f };
			uvs.push_back(textcoord1);
		}
	}

	*pNumberOfPoints = (uint32_t)vertices.size();
	(*ppPoints) = (Vertex*)conf_malloc(sizeof(Vertex) * (*pNumberOfPoints));

	for (uint32_t i = 0; i < (uint32_t)vertices.size(); i++)
	{
		Vertex vertex;
		vertex.mPos = float3(vertices[i].getX(), vertices[i].getY(), vertices[i].getZ());
		vertex.mNormal = float3(normals[i].getX(), normals[i].getY(), normals[i].getZ());

		float theta = atan2f(normals[i].getY(), normals[i].getX());
		float phi = acosf(normals[i].getZ());

		vertex.mUv.x = (theta / (2 * PI));
		vertex.mUv.y = (phi / PI);

		(*ppPoints)[i] = vertex;
	}
}

// Finds the vertex in the direction of the normal
vec3 AABBGetVertex(AABB b, vec3 normal)
{
	vec3 p = b.minBounds;
	for (int i = 0; i < 3; ++i)
	{
		if (normal[i] >= 0.0f)
			p[i] = b.maxBounds[i];
	}
	return p;
}

bool AABBInFrustum(AABB b, vec4 frustumPlanes[6])
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
		DynamicUIControls         hairShading;
		DynamicUIControls         hairSimulation;
		tinystl::vector<IWidget*> hairWidgets;
	};
	static tinystl::vector<HairDynamicWidgets> hairDynamicWidgets;

	static DynamicUIControls hairShadingDynamicWidgets;
	static DynamicUIControls hairSimulationDynamicWidgets;

	static MaterialType currentMaterialType;
	static uint         currentHairType;
};
tinystl::vector<GuiController::HairDynamicWidgets> GuiController::hairDynamicWidgets;
DynamicUIControls                                  GuiController::hairShadingDynamicWidgets;
DynamicUIControls                                  GuiController::hairSimulationDynamicWidgets;
MaterialType                                       GuiController::currentMaterialType;
uint                                               GuiController::currentHairType = 0;

class MaterialPlayground: public IApp
{
	public:
	bool Init()
	{
		// INITIALIZE RENDERER, COMMAND BUFFERS
		//
		RendererDesc settings = { 0 };
		initRenderer(GetName(), &settings, &pRenderer);
		if (!pRenderer)
			return false;

		gGPUPresetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;

		QueueDesc queueDesc = {};
		queueDesc.mType = CMD_POOL_DIRECT;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);
		// Create command pool and create a cmd buffer for each swapchain image
		addCmdPool(pRenderer, pGraphicsQueue, false, &pCmdPool);
		addCmd_n(pCmdPool, false, gImageCount, &ppCmds);

		addCmdPool(pRenderer, pGraphicsQueue, false, &pUICmdPool);
		addCmd_n(pUICmdPool, false, gImageCount, &ppUICmds);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		// INITIALIZE SCRIPTING & RESOURCE SYSTEMS
		//
		gLuaManager.Init();
		initResourceLoaderInterface(pRenderer, DEFAULT_MEMORY_BUDGET, true);
		initDebugRendererInterface(pRenderer, "TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Init(pRenderer, "circlepad.png", FSR_Textures))
			return false;
#endif
		addGpuProfiler(pRenderer, pGraphicsQueue, &pGpuProfiler);

		// CREATE RENDERING RESOURCES
		//
		CreateRasterizerStates();
		CreateDepthStates();
		CreateBlendStates();
		CreateSamplers();

		CreateShaders();
		CreateRootSignatures();

		CreatePBRMaps();
		LoadModelsAndTextures();

		CreateResources();
		LoadAnimations();
		CreateUniformBuffers();

		finishResourceLoading();

		InitializeUniformBuffers();

		// INITIALIZE UI
		//
		if (!gAppUI.Init(pRenderer))
			return false;

		gAppUI.LoadFont("TitilliumText/TitilliumText-Bold.otf", FSR_Builtin_Fonts);

		GuiDesc guiDesc = {};
		float   dpiScale = getDpiScale().x;
		guiDesc.mStartPosition = vec2(5, 200.0f) / dpiScale;
		guiDesc.mStartSize = vec2(450, 600) / dpiScale;
		pGuiWindowMain = gAppUI.AddGuiComponent(GetName(), &guiDesc);

		guiDesc.mStartPosition = vec2(300, 200.0f) / dpiScale;
		pGuiWindowMaterial = gAppUI.AddGuiComponent("Material Properties", &guiDesc);

		guiDesc.mStartPosition = vec2((float)mSettings.mWidth - 300.0f * dpiScale, 200.0f) / dpiScale;
		guiDesc.mStartSize = vec2(450, 600) / dpiScale;
		pGuiWindowHairSimulation = gAppUI.AddGuiComponent("Hair simulation", &guiDesc);
		GuiController::AddGui();

		// INITIALIZE CAMERA & INPUT
		//
		CameraMotionParameters camParameters{ 100.0f, 150.0f, 300.0f };
		vec3                   camPos{ -6.12865686f, 12.2564745f, 59.3652649f };

		vec3 lookAt{ -6.10978842f, 0, 0 };

		pCameraController = createFpsCameraController(camPos, lookAt);

#if defined(TARGET_IOS) || defined(__ANDROID__)
		gVirtualJoystick.InitLRSticks();
		pCameraController->setVirtualJoystick(&gVirtualJoystick);
#endif
		requestMouseCapture(true);
		pCameraController->setMotionParameters(camParameters);
		InputSystem::RegisterInputEvent(cameraInputEvent);
		InputSystem::RegisterInputEvent(pFnInputEvent);

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
		gLuaManager.SetFunction("GetIsCameraAnimated", [cameraLocalPtr](ILuaStateWrap* state) -> int {
			state->PushResultInteger(gbAnimateCamera ? 1 : 0);
			return 1;    // return amount of arguments
		});
		tinystl::string updateCameraFilename = FileSystem::FixPath("updateCamera.lua", FSR_Middleware2);
		gbLuaScriptingSystemLoadedSuccessfully = gLuaManager.SetUpdatableScript(updateCameraFilename.c_str(), "Update", "Exit");

		return true;
	}

	void Exit()
	{
		gLuaManager.Exit();

		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		destroyCameraController(pCameraController);

		removeDebugRendererInterface();

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		DestroyUniformBuffers();
		DestroyAnimations();
		DestroyResources();
		DestroyTextures();
		DestroyModels();
		DestroyPBRMaps();

		DestroyRootSignatures();
		DestroyShaders();

		DestroySamplers();
		DestroyBlendStates();
		DestroyDepthStates();
		DestroyRasterizerStates();

#ifdef TARGET_IOS
		gVirtualJoystick.Exit();
#endif
		removeGpuProfiler(pRenderer, pGpuProfiler);

		GuiController::Exit();
		gAppUI.Exit();

		// Remove commands and command pool&
		removeCmd_n(pUICmdPool, gImageCount, ppUICmds);
		removeCmdPool(pRenderer, pUICmdPool);

		removeCmd_n(pCmdPool, gImageCount, ppCmds);
		removeCmdPool(pRenderer, pCmdPool);
		removeQueue(pGraphicsQueue);

		// Remove resource loader and renderer
		removeResourceLoaderInterface(pRenderer);
		removeRenderer(pRenderer);
	}

	bool Load()
	{
		CreateRenderTargets();
		CreatePipelines();

		if (!gAppUI.Load(pSwapChain->ppSwapchainRenderTargets))
			return false;

#ifdef TARGET_IOS
		if (!gVirtualJoystick.Load(pSwapChain->ppSwapchainRenderTargets[0], pRenderTargetDepth->mDesc.mFormat))
			return false;
#endif

		return true;
	}

	void Unload()
	{
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFences[gFrameIndex], true);

		gAppUI.Unload();

#ifdef TARGET_IOS
		gVirtualJoystick.Unload();
#endif

		DestroyPipelines();
		DestroyRenderTargets();
	}

	void Update(float deltaTime)
	{
		// HANDLE INPUT
		//
		if (getKeyDown(KEY_BUTTON_X))
		{
			RecenterCameraView(170.0f);
		}
#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
		if (getKeyUp(KEY_LEFT_TRIGGER))    // KEY_LEFT_TRIGGER = spacebar
		{
			gDiffuseReflectionModel = (gDiffuseReflectionModel + 1) == DIFFUSE_REFLECTION_MODEL_COUNT ? 0 : gDiffuseReflectionModel + 1;
		}
#endif
		// rest of the input callbacks are in 'static bool pFnInputEvent(const ButtonData* data)'

		// UPDATE UI & CAMERA
		//
		gAppUI.Update(deltaTime);
		GuiController::UpdateDynamicUI();

		pCameraController->update(deltaTime);
		if (gbLuaScriptingSystemLoadedSuccessfully)
		{
			gLuaManager.Update(deltaTime);
		}

		mat4        viewMat = pCameraController->getViewMatrix();
		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 3.0f;
		mat4        projMat = mat4::perspective(horizontal_fov, aspectInverse, 0.1f, 1000.0f);
		gUniformDataCamera.mProjectView = projMat * viewMat;
		gUniformDataCamera.mInvProjectView = inverse(gUniformDataCamera.mProjectView);
		gTextProjView = gUniformDataCamera.mProjectView;
		gUniformDataCamera.mCamPos = pCameraController->getViewPosition();

		vec4 frustumPlanes[6];
		mat4::extractFrustumClipPlanes(
			gUniformDataCamera.mProjectView, frustumPlanes[0], frustumPlanes[1], frustumPlanes[2], frustumPlanes[3], frustumPlanes[4],
			frustumPlanes[5], true);

		viewMat.setTranslation(vec3(0));
		gUniformDataCameraSkybox = gUniformDataCamera;
		gUniformDataCameraSkybox.mProjectView = projMat * viewMat;

		// UPDATE UNIFORM BUFFERS
		//
		gUniformDataDirectionalLights.mDirectionalLights[0].mDirection = v3ToF3(normalize(f3Tov3(gDirectionalLightPosition)));
		gUniformDataDirectionalLights.mDirectionalLights[0].mShadowMap = 0;
		gUniformDataDirectionalLights.mNumDirectionalLights = 1;

		gUniformDataPointLights.mNumPointLights = 0;    // short out point lights for now

		gUniformDataCamera.bUseEnvMap = gEnvironmentLighting;
		gUniformDataCamera.fAOIntensity = gAOIntensity;
		gUniformDataCamera.iRenderMode = gRenderMode;

		// update the texture config (position and all other variables are set during initialization
		// and they dont change during Update()).
		for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			gUniformDataObject = gUniformDataMatBall[i];

			// Add the Oren-Nayar diffuse model for wood material to the texture config.
			if (true    //GuiController::currentMaterialType == MATERIAL_WOOD
#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
				&& gDiffuseReflectionModel == OREN_NAYAR_REFLECTION
#endif
			)
				gUniformDataObject.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL | ETextureConfigFlags::OREN_NAYAR;
			else
				gUniformDataObject.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL;

			// Override roughness value if the checkbox is ticked.
			if (gOverrideRoughnessTextures)
			{
				gUniformDataObject.textureConfig = gUniformDataObject.textureConfig & ~ETextureConfigFlags::ROUGHNESS;
				gUniformDataObject.mRoughness = gRoughnessOverride;
			}
			if (gDisableNormalMaps)
			{
				gUniformDataObject.textureConfig = gUniformDataObject.textureConfig & ~ETextureConfigFlags::NORMAL;
			}
			if (gDisableAOMaps)
			{
				gUniformDataObject.textureConfig = gUniformDataObject.textureConfig & ~ETextureConfigFlags::AO;
			}

			gUniformDataMatBall[i] = gUniformDataObject;
			for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
			{
				BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[frameIdx][i], &gUniformDataObject };
				updateResource(&objBuffUpdateDesc);
			}
		}
#ifndef DIRECT3D11
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

					gAnimatedObject[hairType].SetRootTransform(
						mat4::translation(vec3(20.0f - hairType * 10.0f, -5.5f, 10.0f)) * mat4::scale(vec3(5.0f)));
					if (!gAnimatedObject[hairType].Update(min(deltaTime, 1.0f / 60.0f)))
						InfoMsg("Animation NOT Updating!");
					gAnimatedObject[hairType].PoseRig();
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
					NamedTransform namedTransform = gTransforms[gHair[k].mTransform];
					Transform      transform = namedTransform.mTransform;

					boneMatrix = mat4::identity();
					boneRotation = mat3::identity();

					if (namedTransform.mAttachedBone != -1)
						GetCorrectedBoneTranformation(hairType, namedTransform.mAttachedBone, &boneMatrix, &boneRotation);

					gHair[k].mUniformDataHairShading.mTransform = mat4::identity();
					gHair[k].mUniformDataHairShading.mStrandRadius = gHair[k].mStrandRadius * transform.mScale;
					gHair[k].mUniformDataHairShading.mStrandSpacing = gHair[k].mStrandSpacing * transform.mScale;

					// Transform the hair to be centered around the origin in hair local space. Then transform it to follow the head.
					gHair[k].mUniformDataHairSimulation.mTransform = boneMatrix * mat4::rotationZYX(transform.mOrientation) *
																	 mat4::translation(transform.mPosition) *
																	 mat4::scale(vec3(transform.mScale));
					gHair[k].mUniformDataHairSimulation.mQuatRotation =
						Quat(boneRotation) * Quat(mat3::rotationZYX(transform.mOrientation));
					gHair[k].mUniformDataHairSimulation.mScale = transform.mScale;
					for (uint j = 0; j < gHair[k].mUniformDataHairSimulation.mCapsuleCount; ++j)
						gHair[k].mUniformDataHairSimulation.mCapsules[j] = gFinalCapsules[hairType][gHair[k].mCapsules[j]];
				}

				// Find head transform
				boneMatrix = mat4::identity();
				for (uint i = 0; i < (uint)gTransforms.size(); ++i)
				{
					if (gTransforms[i].mName == "Head")
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
#endif
	}

	void Draw()
	{
		// FRAME SYNC
		//
		// This will acquire the next swapchain image
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &gFrameIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppSwapchainRenderTargets[gFrameIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		// SET CONSTANT BUFFERS
		//
		for (size_t totalBuf = 0; totalBuf < MATERIAL_INSTANCE_COUNT; ++totalBuf)
		{
			BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[gFrameIndex][totalBuf], &gUniformDataMatBall[totalBuf] };
			updateResource(&objBuffUpdateDesc);
		}

		BufferUpdateDesc camBuffUpdateDesc = { pUniformBufferCamera[gFrameIndex], &gUniformDataCamera };
		updateResource(&camBuffUpdateDesc);

		BufferUpdateDesc skyboxViewProjCbv = { pUniformBufferCameraSkybox[gFrameIndex], &gUniformDataCameraSkybox };
		updateResource(&skyboxViewProjCbv);

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			if (!gHairTypeInfo[hairType].mInView)
				continue;

			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
			{
				BufferUpdateDesc hairShadowBuffUpdateDesc = { pUniformBufferCameraHairShadows[gFrameIndex][hairType][i],
															  &gUniformDataCameraHairShadows[hairType][i] };
				updateResource(&hairShadowBuffUpdateDesc);
			}
		}

		BufferUpdateDesc directionalLightsBufferUpdateDesc = { pUniformBufferDirectionalLights, &gUniformDataDirectionalLights };
		updateResource(&directionalLightsBufferUpdateDesc);

		BufferUpdateDesc hairGlobalBufferUpdateDesc = { pUniformBufferHairGlobal, &gUniformDataHairGlobal };
		updateResource(&hairGlobalBufferUpdateDesc);

		for (size_t i = 0; i < gHair.size(); ++i)
		{
			BufferUpdateDesc hairShadingBufferUpdateDesc = { gHair[i].pUniformBufferHairShading[gFrameIndex],
															 &gHair[i].mUniformDataHairShading };
			updateResource(&hairShadingBufferUpdateDesc);

			BufferUpdateDesc hairSimulationBufferUpdateDesc = { gHair[i].pUniformBufferHairSimulation[gFrameIndex],
																&gHair[i].mUniformDataHairSimulation };
			updateResource(&hairSimulationBufferUpdateDesc);
		}

		if (gMaterialType == MATERIAL_HAIR)
			gSkeletonBatcher.SetPerInstanceUniforms(gFrameIndex);

		// Draw
		tinystl::vector<Cmd*> allCmds;
		Cmd*                  cmd = ppCmds[gFrameIndex];
		beginCmd(cmd);

		cmdBeginGpuFrameProfile(cmd, pGpuProfiler);

		TextureBarrier barriers[] = { { pRenderTarget->pTexture, RESOURCE_STATE_RENDER_TARGET },
									  { pRenderTargetDepth->pTexture, RESOURCE_STATE_DEPTH_WRITE } };
		cmdResourceBarrier(cmd, 0, NULL, 2, barriers, false);

		// DRAW SKYBOX
		//
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		if (gDrawSkybox)    // TODO: do we need this condition?
		{
			cmdBindPipeline(cmd, pPipelineSkybox);
			DescriptorData skyParams[2] = {};
			skyParams[0].pName = "uniformBlock";
			skyParams[0].ppBuffers = &pUniformBufferCameraSkybox[gFrameIndex];
			skyParams[1].pName = "skyboxTex";
			skyParams[1].ppTextures = &pTextureSkybox;
			cmdBindDescriptors(cmd, pRootSignatureSkybox, 2, skyParams);
			cmdBindVertexBuffer(cmd, 1, &pVertexBufferSkybox, NULL);
			cmdDraw(cmd, 36, 0);
		}

		// DRAW THE OBJECTS W/ MATERIALS
		//
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth = pRenderTargetDepth->mDesc.mClearValue;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);
		cmdBindPipeline(cmd, pPipelineBRDF);

		DescriptorData params[6] = {};
		params[0].pName = "cbCamera";
		params[0].ppBuffers = &pUniformBufferCamera[gFrameIndex];
		params[1].pName = "cbPointLights";
		params[1].ppBuffers = &pUniformBufferPointLights;
		params[2].pName = "cbDirectionalLights";
		params[2].ppBuffers = &pUniformBufferDirectionalLights;
		params[3].pName = "brdfIntegrationMap";
		params[3].ppTextures = &pTextureBRDFIntegrationMap;
		params[4].pName = "irradianceMap";
		params[4].ppTextures = &pTextureIrradianceMap;
		params[5].pName = "specularMap";
		params[5].ppTextures = &pTextureSpecularMap;
		cmdBindDescriptors(cmd, pRootSignatureBRDF, 6, params);

		int matTypeId = GuiController::currentMaterialType * MATERIAL_TEXTURE_COUNT * MATERIAL_INSTANCE_COUNT;
		int textureIndex = 0;

		// DRAW THE GROUND PLANE
		//
		cmdBindVertexBuffer(cmd, 1, &pMeshes[MESH_CUBE]->pVertexBuffer, NULL);
		cmdBindIndexBuffer(cmd, pMeshes[MESH_CUBE]->pIndexBuffer, NULL);

		params[0].pName = "cbObject";
		params[0].ppBuffers = &pUniformBufferGroundPlane;
		cmdBindDescriptors(cmd, pRootSignatureBRDF, 1, params);

		for (int j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
		{
			params[j].pName = pTextureName[j];
			params[j].ppTextures = &pTextureMaterialMapsGround[j];
		}

		cmdBindDescriptors(cmd, pRootSignatureBRDF, MATERIAL_TEXTURE_COUNT, params);
		cmdDrawIndexed(cmd, pMeshes[MESH_CUBE]->mIndexCount, 0, 0);

		// DRAW THE OBJECTS W/ MATERIALS
		//
		if (gMaterialType != MATERIAL_HAIR)
		{
			// DRAW THE LABEL PLATES
			//
			for (int j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
			{
				params[j].pName = pTextureName[j];
				params[j].ppTextures = &pTextureMaterialMaps[j + 4];
			}
			cmdBindDescriptors(cmd, pRootSignatureBRDF, MATERIAL_TEXTURE_COUNT, params);

			for (int j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			{
				params[0].pName = "cbObject";
				params[0].ppBuffers = &pUniformBufferNamePlates[j];
				cmdBindDescriptors(cmd, pRootSignatureBRDF, 1, params);
				cmdDrawIndexed(cmd, pMeshes[MESH_CUBE]->mIndexCount, 0, 0);
			}

			cmdBindVertexBuffer(cmd, 1, &pMeshes[MESH_MAT_BALL]->pVertexBuffer, NULL);
			cmdBindIndexBuffer(cmd, pMeshes[MESH_MAT_BALL]->pIndexBuffer, NULL);

			// DRAW THE MATERIAL BALLS
			//
#if 1    // toggle for rendering objects
			for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				params[0].pName = "cbObject";
				params[0].ppBuffers = &pUniformBufferMatBall[gFrameIndex][i];
				cmdBindDescriptors(cmd, pRootSignatureBRDF, 1, params);

				//binding pbr material textures
				for (int j = 0; j < MATERIAL_TEXTURE_COUNT; ++j)
				{
					int index = j + MATERIAL_TEXTURE_COUNT * i;
					textureIndex = matTypeId + index;
					params[j].pName = pTextureName[j];
					if (textureIndex >= gMaterialTextureCount)
					{
						LOGERROR("texture index greater than array size, setting it to default texture");
						textureIndex = matTypeId + j;
					}
					params[j].ppTextures = &pTextureMaterialMaps[textureIndex];
				}

				cmdBindDescriptors(cmd, pRootSignatureBRDF, MATERIAL_TEXTURE_COUNT, params);
				cmdDrawIndexed(cmd, pMeshes[MESH_MAT_BALL]->mIndexCount, 0, 0);
			}
#endif
		}
#ifndef DIRECT3D11
		// Draw hair
		else if (gMaterialType == MATERIAL_HAIR)
		{
			//// draw the skeleton of the rig
			gSkeletonBatcher.Draw(cmd, gFrameIndex);

			DescriptorData hairParams[11] = {};

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			// Hair simulation
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Hair simulation", true);
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint k = gHairTypeIndices[hairType][i];

					uint dispatchGroupCountPerVertex =
						gHair[k].mTotalVertexCount / 64 / (gHair[k].mUniformDataHairSimulation.mNumFollowHairsPerGuideHair + 1);
					uint dispatchGroupCountPerStrand = gHair[k].mNumGuideStrands / 64;

					BufferBarrier bufferBarriers[3] = {};
					for (int j = 0; j < 3; ++j)
					{
						bufferBarriers[j].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[j];
						bufferBarriers[j].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
					}
					cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, false);

					if (gFirstHairSimulationFrame || gHairTypeInfo[hairType].mPreWarm)
					{
						cmdBindPipeline(cmd, pPipelineHairPreWarm);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexPositionsPrev";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						hairParams[3].pName = "HairVertexPositionsPrevPrev";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[2];
						hairParams[4].pName = "HairRestPositions";
						hairParams[4].ppBuffers = &gHair[k].pBufferHairVertexPositions;
						cmdBindDescriptors(cmd, pRootSignatureHairPreWarm, 5, hairParams);

						cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mNewState = gHair[k].pBufferHairSimulationVertexPositions[j]->mCurrentState;
						}
						cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, false);
					}

					cmdBindPipeline(cmd, pPipelineHairIntegrate);

					hairParams[0].pName = "cbSimulation";
					hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
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
					cmdBindDescriptors(cmd, pRootSignatureHairIntegrate, 6, hairParams);

					cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

					for (int j = 0; j < 3; ++j)
					{
						bufferBarriers[j].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[j];
						bufferBarriers[j].mNewState = gHair[k].pBufferHairSimulationVertexPositions[j]->mCurrentState;
					}
					cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, false);

					if (gHair[k].mUniformDataHairSimulation.mShockPropagationStrength > 0.0f)
					{
						cmdBindPipeline(cmd, pPipelineHairShockPropagation);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexPositionsPrev";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[1];
						hairParams[3].pName = "HairVertexPositionsPrevPrev";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[2];
						cmdBindDescriptors(cmd, pRootSignatureHairShockPropagation, 4, hairParams);

						cmdDispatch(cmd, dispatchGroupCountPerStrand, 1, 1);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mNewState = gHair[k].pBufferHairSimulationVertexPositions[j]->mCurrentState;
						}
						cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, false);
					}

					if (gHair[k].mUniformDataHairSimulation.mLocalConstraintIterations > 0 &&
						gHair[k].mUniformDataHairSimulation.mLocalStiffness > 0.0f)
					{
						cmdBindPipeline(cmd, pPipelineHairLocalConstraints);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairGlobalRotations";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairGlobalRotations;
						hairParams[3].pName = "HairRefsInLocalFrame";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairRefsInLocalFrame;
						cmdBindDescriptors(cmd, pRootSignatureHairLocalConstraints, 4, hairParams);

						for (int j = 0; j < 3; ++j)
						{
							bufferBarriers[j].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[j];
							bufferBarriers[j].mNewState = gHair[k].pBufferHairSimulationVertexPositions[j]->mCurrentState;
						}

						for (int j = 0; j < (int)gHair[k].mUniformDataHairSimulation.mLocalConstraintIterations; ++j)
						{
							cmdDispatch(cmd, dispatchGroupCountPerStrand, 1, 1);
							cmdResourceBarrier(cmd, 3, bufferBarriers, 0, NULL, false);
						}
					}

					cmdBindPipeline(cmd, pPipelineHairLengthConstraints);

					bufferBarriers[0].pBuffer = gHair[k].pBufferHairVertexTangents;
					bufferBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
					cmdResourceBarrier(cmd, 1, bufferBarriers, 0, NULL, false);

					hairParams[0].pName = "cbSimulation";
					hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
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
					cmdBindDescriptors(cmd, pRootSignatureHairLengthConstraints, 6, hairParams);
#else
					cmdBindDescriptors(cmd, pRootSignatureHairLengthConstraints, 5, hairParams);
#endif

					cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

					bufferBarriers[0].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[0];
					bufferBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
					bufferBarriers[1].pBuffer = gHair[k].pBufferHairVertexTangents;
					bufferBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
					cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, false);

					// Update follow hairs
					if (gHair[k].mUniformDataHairSimulation.mNumFollowHairsPerGuideHair > 0)
					{
						cmdBindPipeline(cmd, pPipelineHairUpdateFollowHairs);

						bufferBarriers[0].pBuffer = gHair[k].pBufferHairVertexTangents;
						bufferBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
						cmdResourceBarrier(cmd, 1, bufferBarriers, 0, NULL, false);

						hairParams[0].pName = "cbSimulation";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairSimulation[gFrameIndex];
						hairParams[1].pName = "HairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "HairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "FollowHairRootOffsets";
						hairParams[3].ppBuffers = &gHair[k].pBufferFollowHairRootOffsets;
						cmdBindDescriptors(cmd, pRootSignatureHairUpdateFollowHairs, 4, hairParams);

						cmdDispatch(cmd, dispatchGroupCountPerVertex, 1, 1);

						bufferBarriers[0].pBuffer = gHair[k].pBufferHairSimulationVertexPositions[0];
						bufferBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
						bufferBarriers[1].pBuffer = gHair[k].pBufferHairVertexTangents;
						bufferBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
						cmdResourceBarrier(cmd, 2, bufferBarriers, 0, NULL, false);
					}
				}

				gHairTypeInfo[hairType].mPreWarm = false;
			}
			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

			// Draw hair - shadow map
			cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Hair rendering", true);

			TextureBarrier textureBarriers[2] = {};
			BufferBarrier  bufferBarrier[1] = {};
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				{
					cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);
					textureBarriers[0].pTexture = pRenderTargetHairShadows[hairType][i]->pTexture;
					textureBarriers[0].mNewState = RESOURCE_STATE_DEPTH_WRITE;
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);

					loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
					loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
					loadActions.mClearDepth = pRenderTargetHairShadows[hairType][i]->mDesc.mClearValue;

					cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetHairShadows[hairType][i], &loadActions, NULL, NULL, 0, 0);
					cmdSetViewport(
						cmd, 0.0f, 0.0f, (float)pRenderTargetHairShadows[hairType][i]->mDesc.mWidth,
						(float)pRenderTargetHairShadows[hairType][i]->mDesc.mHeight, 0.0f, 1.0f);
					cmdSetScissor(
						cmd, 0, 0, pRenderTargetHairShadows[hairType][i]->mDesc.mWidth,
						pRenderTargetHairShadows[hairType][i]->mDesc.mHeight);

					cmdBindPipeline(cmd, pPipelineHairShadow);
					hairParams[0].pName = "cbCamera";
					hairParams[0].ppBuffers = &pUniformBufferCameraHairShadows[gFrameIndex][hairType][i];
					cmdBindDescriptors(cmd, pRootSignatureHairShadow, 1, hairParams);

					for (size_t j = 0; j < gHairTypeIndices[hairType].size(); ++j)
					{
						uint k = gHairTypeIndices[hairType][j];

						hairParams[0].pName = "cbHair";
						hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[gFrameIndex];
						hairParams[1].pName = "GuideHairVertexPositions";
						hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
						hairParams[2].pName = "GuideHairVertexTangents";
						hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
						hairParams[3].pName = "HairThicknessCoefficients";
						hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
						cmdBindDescriptors(cmd, pRootSignatureHairShadow, 4, hairParams);

						cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, 0);
						cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);
					}
				}
			}

			// Draw hair - clear hair depths texture
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

#ifndef METAL
			textureBarriers[0].pTexture = pTextureHairDepth;
			textureBarriers[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);
#else
			bufferBarrier[0].pBuffer = pBufferHairDepth;
			bufferBarrier[0].mNewState = RESOURCE_STATE_UNORDERED_ACCESS;
			cmdResourceBarrier(cmd, 1, bufferBarrier, 0, NULL, false);
#endif

			loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
			cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, 0, 0);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pPipelineHairClear);
			hairParams[0].pName = "DepthsTexture";
#ifndef METAL
			hairParams[0].ppTextures = &pTextureHairDepth;
			cmdBindDescriptors(cmd, pRootSignatureHairClear, 1, hairParams);
#else
			hairParams[0].ppBuffers = &pBufferHairDepth;
			hairParams[1].pName = "cbHairGlobal";
			hairParams[1].ppBuffers = &pUniformBufferHairGlobal;
			cmdBindDescriptors(cmd, pRootSignatureHairClear, 2, hairParams);
#endif

			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			// Draw hair - depth peeling and alpha accumulaiton
			textureBarriers[0].pTexture = pRenderTargetDepthPeeling->pTexture;
			textureBarriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);

			loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
			loadActions.mClearColorValues[0] = pRenderTargetDepthPeeling->mDesc.mClearValue;
			loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTargetDepthPeeling, pRenderTargetDepth, &loadActions, NULL, NULL, 0, 0);
			cmdSetViewport(
				cmd, 0.0f, 0.0f, (float)pRenderTargetDepthPeeling->mDesc.mWidth, (float)pRenderTargetDepthPeeling->mDesc.mHeight, 0.0f,
				1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetDepthPeeling->mDesc.mWidth, pRenderTargetDepthPeeling->mDesc.mHeight);

			cmdBindPipeline(cmd, pPipelineHairDepthPeeling);
			hairParams[0].pName = "cbCamera";
			hairParams[0].ppBuffers = &pUniformBufferCamera[gFrameIndex];
			hairParams[1].pName = "DepthsTexture";
#ifndef METAL
			hairParams[1].ppTextures = &pTextureHairDepth;
#else
			hairParams[1].ppBuffers = &pBufferHairDepth;
#endif
			hairParams[2].pName = "cbHairGlobal";
			hairParams[2].ppBuffers = &pUniformBufferHairGlobal;
			cmdBindDescriptors(cmd, pRootSignatureHairDepthPeeling, 3, hairParams);

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint k = gHairTypeIndices[hairType][i];

					hairParams[0].pName = "cbHair";
					hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[gFrameIndex];
					hairParams[1].pName = "GuideHairVertexPositions";
					hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
					hairParams[2].pName = "GuideHairVertexTangents";
					hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
					hairParams[3].pName = "HairThicknessCoefficients";
					hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
					cmdBindDescriptors(cmd, pRootSignatureHairDepthPeeling, 4, hairParams);

					cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, 0);
					cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);
				}
			}

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			// Draw hair - depth resolve
#ifndef METAL
			textureBarriers[0].pTexture = pTextureHairDepth;
			textureBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);
#else
			bufferBarrier[0].pBuffer = pBufferHairDepth;
			bufferBarrier[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 1, bufferBarrier, 0, NULL, false);
#endif

			cmdBindRenderTargets(cmd, 0, NULL, pRenderTargetDepth, &loadActions, NULL, NULL, 0, 0);
			cmdBindPipeline(cmd, pPipelineHairDepthResolve);
			hairParams[0].pName = "DepthsTexture";
#ifndef METAL
			hairParams[0].ppTextures = &pTextureHairDepth;
			cmdBindDescriptors(cmd, pRootSignatureHairDepthResolve, 1, hairParams);
#else
			hairParams[0].ppBuffers = &pBufferHairDepth;
			hairParams[1].pName = "cbHairGlobal";
			hairParams[1].ppBuffers = &pUniformBufferHairGlobal;
			cmdBindDescriptors(cmd, pRootSignatureHairDepthResolve, 2, hairParams);
#endif
			cmdDraw(cmd, 3, 0);

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			// Draw hair - fill colors
			textureBarriers[0].pTexture = pRenderTargetFillColors->pTexture;
			textureBarriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
			cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				{
					textureBarriers[0].pTexture = pRenderTargetHairShadows[hairType][i]->pTexture;
					textureBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
					cmdResourceBarrier(cmd, 0, NULL, 1, textureBarriers, false);
				}
			}

			loadActions.mClearColorValues[0] = pRenderTargetFillColors->mDesc.mClearValue;

			cmdBindRenderTargets(cmd, 1, &pRenderTargetFillColors, pRenderTargetDepth, &loadActions, NULL, NULL, 0, 0);
			cmdSetViewport(
				cmd, 0.0f, 0.0f, (float)pRenderTargetFillColors->mDesc.mWidth, (float)pRenderTargetFillColors->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTargetFillColors->mDesc.mWidth, pRenderTargetFillColors->mDesc.mHeight);

			cmdBindPipeline(cmd, pPipelineHairFillColors);
			hairParams[0].pName = "cbCamera";
			hairParams[0].ppBuffers = &pUniformBufferCamera[gFrameIndex];
			hairParams[1].pName = "cbPointLights";
			hairParams[1].ppBuffers = &pUniformBufferPointLights;
			hairParams[2].pName = "cbDirectionalLights";
			hairParams[2].ppBuffers = &pUniformBufferDirectionalLights;
			hairParams[3].pName = "cbHairGlobal";
			hairParams[3].ppBuffers = &pUniformBufferHairGlobal;
			cmdBindDescriptors(cmd, pRootSignatureHairFillColors, 4, hairParams);

			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				if (!gHairTypeInfo[hairType].mInView)
					continue;

				Texture* ppShadowMaps[MAX_NUM_DIRECTIONAL_LIGHTS] = {};
				for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
					ppShadowMaps[i] = pRenderTargetHairShadows[hairType][i]->pTexture;

				hairParams[0].pName = "DirectionalLightShadowMaps";
				hairParams[0].ppTextures = ppShadowMaps;
				hairParams[0].mCount = MAX_NUM_DIRECTIONAL_LIGHTS;
				hairParams[1].pName = "cbDirectionalLightShadowCameras";
				hairParams[1].ppBuffers = pUniformBufferCameraHairShadows[gFrameIndex][hairType];
				hairParams[1].mCount = MAX_NUM_DIRECTIONAL_LIGHTS;
				cmdBindDescriptors(cmd, pRootSignatureHairFillColors, 2, hairParams);

				for (size_t i = 0; i < gHairTypeIndices[hairType].size(); ++i)
				{
					uint k = gHairTypeIndices[hairType][i];

					hairParams[0].pName = "cbHair";
					hairParams[0].ppBuffers = &gHair[k].pUniformBufferHairShading[gFrameIndex];
					hairParams[1].pName = "GuideHairVertexPositions";
					hairParams[1].ppBuffers = &gHair[k].pBufferHairSimulationVertexPositions[0];
					hairParams[2].pName = "GuideHairVertexTangents";
					hairParams[2].ppBuffers = &gHair[k].pBufferHairVertexTangents;
					hairParams[3].pName = "HairThicknessCoefficients";
					hairParams[3].ppBuffers = &gHair[k].pBufferHairThicknessCoefficients;
					cmdBindDescriptors(cmd, pRootSignatureHairFillColors, 4, hairParams);

					cmdBindIndexBuffer(cmd, gHair[k].pBufferTriangleIndices, 0);
					cmdDrawIndexed(cmd, gHair[k].mIndexCountHair, 0, 0);
				}
			}

			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			// Draw hair - color resolve
			textureBarriers[0].pTexture = pRenderTargetFillColors->pTexture;
			textureBarriers[0].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			textureBarriers[1].pTexture = pRenderTargetDepthPeeling->pTexture;
			textureBarriers[1].mNewState = RESOURCE_STATE_SHADER_RESOURCE;
			cmdResourceBarrier(cmd, 0, NULL, 2, textureBarriers, false);

			loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;

			cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
			cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

			cmdBindPipeline(cmd, pPipelineHairColorResolve);
			hairParams[0].pName = "ColorsTexture";
			hairParams[0].ppTextures = &pRenderTargetFillColors->pTexture;
			hairParams[1].pName = "InvAlphaTexture";
			hairParams[1].ppTextures = &pRenderTargetDepthPeeling->pTexture;
			cmdBindDescriptors(cmd, pRootSignatureHairColorResolve, 2, hairParams);
			cmdDraw(cmd, 3, 0);

			cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

#if HAIR_MAX_CAPSULE_COUNT > 0
			if (gShowCapsules)
			{
				cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
				cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
				cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

				cmdBindVertexBuffer(cmd, 1, &pMeshes[MESH_CAPSULE]->pVertexBuffer, NULL);
				cmdBindIndexBuffer(cmd, pMeshes[MESH_CAPSULE]->pIndexBuffer, NULL);

				cmdBindPipeline(cmd, pPipelineShowCapsules);
				for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
				{
					for (size_t i = 0; i < gCapsules.size(); ++i)
					{
						hairParams[0].pName = "CapsuleRootConstant";
						hairParams[0].pRootConstant = &gFinalCapsules[hairType][i];
						hairParams[1].pName = "cbCamera";
						hairParams[1].ppBuffers = &pUniformBufferCamera[gFrameIndex];
						cmdBindDescriptors(cmd, pRootSignatureShowCapsules, 2, hairParams);
						cmdDrawIndexed(cmd, pMeshes[MESH_CAPSULE]->mIndexCount, 0, 0);
					}
				}
			}
#endif
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

			gFirstHairSimulationFrame = false;
		}
#endif

		endCmd(cmd);
		allCmds.push_back(cmd);

		// SET UP DRAW COMMANDS (UI)
		//
		cmd = ppUICmds[gFrameIndex];
		beginCmd(cmd);

		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;

		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pRenderTargetDepth, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mDesc.mWidth, (float)pRenderTarget->mDesc.mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mDesc.mWidth, pRenderTarget->mDesc.mHeight);

		static HiresTimer gTimer;
		gTimer.GetUSec(true);

#ifdef TARGET_IOS
		gVirtualJoystick.Draw(cmd, { 1.0f, 1.0f, 1.0f, 1.0f });
#endif

		// draw world-space text
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
			for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				gAppUI.DrawTextInWorldSpace(cmd, ppMaterialNames[i], gMaterialPropDraw, gTextWorldMats[i], gTextProjView);
			}
		}

		// draw HUD text
		drawDebugText(cmd, 8, 15, tinystl::string::format("CPU %f ms", gTimer.GetUSecAverage() / 1000.0f), &gFrameTimeDraw);
#ifndef METAL    // Metal doesn't support GPU profilers
		drawDebugText(cmd, 8, 40, tinystl::string::format("GPU %f ms", (float)pGpuProfiler->mCumulativeTime * 1000.0f), &gFrameTimeDraw);
		drawDebugGpuProfile(cmd, 8.0f, 90.0f, pGpuProfiler, NULL);
#endif

		if (!gbLuaScriptingSystemLoadedSuccessfully)
		{
			drawDebugText(cmd, 8, 75, "Error loading LUA scripts!", &gErrMsgDrawDesc);
		}

		gAppUI.Gui(pGuiWindowMain);
		if (GuiController::currentMaterialType == MATERIAL_HAIR)
			gAppUI.Gui(pGuiWindowHairSimulation);
		else
			gAppUI.Gui(pGuiWindowMaterial);

		gAppUI.Draw(cmd);

		// PRESENT THE GFX QUEUE
		//
		// Transition our texture to present state
		barriers[0] = { pRenderTarget->pTexture, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 1, barriers, true);
		cmdEndGpuFrameProfile(cmd, pGpuProfiler);
		endCmd(cmd);
		allCmds.push_back(cmd);

		queueSubmit(
			pGraphicsQueue, (uint32_t)allCmds.size(), allCmds.data(), pRenderCompleteFence, 1, &pImageAcquiredSemaphore, 1,
			&pRenderCompleteSemaphore);
		queuePresent(pGraphicsQueue, pSwapChain, gFrameIndex, 1, &pRenderCompleteSemaphore);
	}

	tinystl::string GetName() { return "06_MaterialPlayground"; }

	void RecenterCameraView(float maxDistance, vec3 lookAt = vec3(0))
	{
		vec3 p = pCameraController->getViewPosition();
		vec3 d = p - lookAt;

		float lenSqr = lengthSqr(d);
		if (lenSqr > (maxDistance * maxDistance))
		{
			d *= (maxDistance / sqrtf(lenSqr));
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
	static bool pFnInputEvent(const ButtonData* data)
	{
		// Handle Keyboard Events
		if (data->mActiveDevicesMask == GAINPUT_KEYBOARD && data->mIsTriggered)
		{
			switch (data->mCharacter)
			{
				case '1': gMaterialType = MATERIAL_METAL; break;
				case '2': gMaterialType = MATERIAL_WOOD; break;
				case '3': gMaterialType = MATERIAL_HAIR; break;
				case 'c': gbAnimateCamera = !gbAnimateCamera; break;
				case 'v': gDrawSkybox = !gDrawSkybox; break;
				case 'b': gEnvironmentLighting = !gEnvironmentLighting; break;

				// shift + number combinations
				case '!': gRenderMode = 0; break;    // Key 1
				case '@': gRenderMode = 1; break;    // Key 2
				case '#': gRenderMode = 2; break;    // Key 3
				case '$': gRenderMode = 3; break;    // Key 4
				case '%': gRenderMode = 4; break;    // Key 5
				case '^': gRenderMode = 5; break;    // Key 6
				default: break;
			}
		}
		return true;
	}

	void GetCorrectedBoneTranformation(uint rigIndex, uint boneIndex, mat4* boneMatrix, mat3* boneRotation)
	{
		(*boneMatrix) = gAnimationRig[rigIndex].GetJointWorldMat(boneIndex);

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
	void CreateRasterizerStates()
	{
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullNone);

		rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_BACK;
		addRasterizerState(pRenderer, &rasterizerStateDesc, &pRasterizerStateCullFront);
	}

	void DestroyRasterizerStates()
	{
		removeRasterizerState(pRasterizerStateCullNone);
		removeRasterizerState(pRasterizerStateCullFront);
	}

	void CreateDepthStates()
	{
		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDesc, &pDepthStateEnable);

		DepthStateDesc depthStateDisableDesc = {};
		depthStateDisableDesc.mDepthTest = false;
		depthStateDisableDesc.mDepthWrite = false;
		depthStateDisableDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDisableDesc, &pDepthStateDisable);

		DepthStateDesc depthStateNoWriteDesc = {};
		depthStateNoWriteDesc.mDepthTest = true;
		depthStateNoWriteDesc.mDepthWrite = false;
		depthStateNoWriteDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateNoWriteDesc, &pDepthStateNoWrite);

		DepthStateDesc depthStateDepthResolveDesc = {};
		depthStateDepthResolveDesc.mDepthTest = true;
		depthStateDepthResolveDesc.mDepthWrite = true;
		depthStateDepthResolveDesc.mDepthFunc = CMP_LEQUAL;
		addDepthState(pRenderer, &depthStateDepthResolveDesc, &pDepthStateDepthResolve);
	}

	void DestroyDepthStates()
	{
		removeDepthState(pDepthStateEnable);
		removeDepthState(pDepthStateDisable);
		removeDepthState(pDepthStateNoWrite);
		removeDepthState(pDepthStateDepthResolve);
	}

	void CreateBlendStates()
	{
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
		addBlendState(pRenderer, &blendStateDesc, &pBlendStateAlphaBlend);

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
		addBlendState(pRenderer, &blendStateDepthPeelingDesc, &pBlendStateDepthPeeling);

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
		addBlendState(pRenderer, &blendStateAddDesc, &pBlendStateAdd);

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
		addBlendState(pRenderer, &blendStateColorResolveDesc, &pBlendStateColorResolve);
	}

	void DestroyBlendStates()
	{
		removeBlendState(pBlendStateAlphaBlend);
		removeBlendState(pBlendStateDepthPeeling);
		removeBlendState(pBlendStateAdd);
		removeBlendState(pBlendStateColorResolve);
	}

	void CreateSamplers()
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

	void DestroySamplers()
	{
		removeSampler(pRenderer, pSamplerBilinear);
		removeSampler(pRenderer, pSamplerBilinearClamped);
		removeSampler(pRenderer, pSamplerPoint);
	}

	void CreateShaders()
	{
		ShaderMacro pointLightsShaderMacro = { "MAX_NUM_POINT_LIGHTS", tinystl::string::format("%i", MAX_NUM_POINT_LIGHTS) };
		ShaderMacro directionalLightsShaderMacro = { "MAX_NUM_DIRECTIONAL_LIGHTS",
													 tinystl::string::format("%i", MAX_NUM_DIRECTIONAL_LIGHTS) };
		ShaderMacro lightMacros[] = { pointLightsShaderMacro, directionalLightsShaderMacro };

		ShaderLoadDesc skyboxShaderDesc = {};
		skyboxShaderDesc.mStages[0] = { "skybox.vert", NULL, 0, FSR_SrcShaders };
		skyboxShaderDesc.mStages[1] = { "skybox.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

		ShaderLoadDesc brdfRenderSceneShaderDesc = {};
		brdfRenderSceneShaderDesc.mStages[0] = { "renderSceneBRDF.vert", lightMacros, 2, FSR_SrcShaders };
		brdfRenderSceneShaderDesc.mStages[1] = { "renderSceneBRDF.frag", lightMacros, 2, FSR_SrcShaders };
		addShader(pRenderer, &brdfRenderSceneShaderDesc, &pShaderBRDF);

#ifndef DIRECT3D11
		const uint  macroCount = 4;
		ShaderMacro shaderMacros[macroCount] = { { "HAIR_MAX_CAPSULE_COUNT", tinystl::string::format("%i", HAIR_MAX_CAPSULE_COUNT) } };
		shaderMacros[1] = pointLightsShaderMacro;
		shaderMacros[2] = directionalLightsShaderMacro;
		shaderMacros[3] = { "SHORT_CUT_CLEAR", "" };
		ShaderLoadDesc hairClearShaderDesc = {};
		hairClearShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairClearShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairClearShaderDesc, &pShaderHairClear);

		shaderMacros[3] = { "SHORT_CUT_DEPTH_PEELING", "" };
		ShaderLoadDesc hairDepthPeelingShaderDesc = {};
		hairDepthPeelingShaderDesc.mStages[0] = { "hair.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairDepthPeelingShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairDepthPeelingShaderDesc, &pShaderHairDepthPeeling);

		shaderMacros[3] = { "SHORT_CUT_RESOLVE_DEPTH", "" };
		ShaderLoadDesc hairDepthResolveShaderDesc = {};
		hairDepthResolveShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairDepthResolveShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairDepthResolveShaderDesc, &pShaderHairDepthResolve);

		shaderMacros[3] = { "SHORT_CUT_FILL_COLOR", "" };
		ShaderLoadDesc hairFillColorShaderDesc = {};
		hairFillColorShaderDesc.mStages[0] = { "hair.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairFillColorShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairFillColorShaderDesc, &pShaderHairFillColors);

		shaderMacros[3] = { "SHORT_CUT_RESOLVE_COLOR", "" };
		ShaderLoadDesc hairColorResolveShaderDesc = {};
		hairColorResolveShaderDesc.mStages[0] = { "fullscreen.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairColorResolveShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairColorResolveShaderDesc, &pShaderHairResolveColor);

		shaderMacros[3] = { "HAIR_SHADOW", "" };
		ShaderLoadDesc hairShadowShaderDesc = {};
		hairShadowShaderDesc.mStages[0] = { "hair.vert", shaderMacros, macroCount, FSR_SrcShaders };
		hairShadowShaderDesc.mStages[1] = { "hair.frag", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairShadowShaderDesc, &pShaderHairShadow);

		shaderMacros[3] = { "HAIR_INTEGRATE", "" };
		ShaderLoadDesc hairIntegrateShaderDesc = {};
		hairIntegrateShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairIntegrateShaderDesc, &pShaderHairIntegrate);

		shaderMacros[3] = { "HAIR_SHOCK_PROPAGATION", "" };
		ShaderLoadDesc hairShockPropagationShaderDesc = {};
		hairShockPropagationShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairShockPropagationShaderDesc, &pShaderHairShockPropagation);

		shaderMacros[3] = { "HAIR_LOCAL_CONSTRAINTS", "" };
		ShaderLoadDesc hairLocalConstraintsShaderDesc = {};
		hairLocalConstraintsShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairLocalConstraintsShaderDesc, &pShaderHairLocalConstraints);

		shaderMacros[3] = { "HAIR_LENGTH_CONSTRAINTS", "" };
		ShaderLoadDesc hairLengthConstraintsShaderDesc = {};
		hairLengthConstraintsShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairLengthConstraintsShaderDesc, &pShaderHairLengthConstraints);

		shaderMacros[3] = { "HAIR_UPDATE_FOLLOW_HAIRS", "" };
		ShaderLoadDesc hairUpdateFollowHairsShaderDesc = {};
		hairUpdateFollowHairsShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairUpdateFollowHairsShaderDesc, &pShaderHairUpdateFollowHairs);

		shaderMacros[3] = { "HAIR_PRE_WARM", "" };
		ShaderLoadDesc hairPreWarmShaderDesc = {};
		hairPreWarmShaderDesc.mStages[0] = { "hair.comp", shaderMacros, macroCount, FSR_SrcShaders };
		addShader(pRenderer, &hairPreWarmShaderDesc, &pShaderHairPreWarm);

		ShaderLoadDesc showCapsulesShaderDesc = {};
		showCapsulesShaderDesc.mStages[0] = { "showCapsules.vert", NULL, 0, FSR_SrcShaders };
		showCapsulesShaderDesc.mStages[1] = { "showCapsules.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &showCapsulesShaderDesc, &pShaderShowCapsules);

		ShaderLoadDesc skeletonShaderDesc = {};
		skeletonShaderDesc.mStages[0] = { "skeleton.vert", NULL, 0, FSR_SrcShaders };
		skeletonShaderDesc.mStages[1] = { "skeleton.frag", NULL, 0, FSR_SrcShaders };
		addShader(pRenderer, &skeletonShaderDesc, &pShaderSkeleton);
#endif
	}

	void DestroyShaders()
	{
		removeShader(pRenderer, pShaderBRDF);
		removeShader(pRenderer, pShaderSkybox);
#ifndef DIRECT3D11
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
#endif
	}

	void CreateRootSignatures()
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

#ifndef DIRECT3D11
		RootSignatureDesc hairClearRootSignatureDesc = {};
		hairClearRootSignatureDesc.ppShaders = &pShaderHairClear;
		hairClearRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairClearRootSignatureDesc, &pRootSignatureHairClear);

		RootSignatureDesc hairDepthPeelingRootSignatureDesc = {};
		hairDepthPeelingRootSignatureDesc.ppShaders = &pShaderHairDepthPeeling;
		hairDepthPeelingRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairDepthPeelingRootSignatureDesc, &pRootSignatureHairDepthPeeling);

		RootSignatureDesc hairResolveDepthRootSignatureDesc = {};
		hairResolveDepthRootSignatureDesc.ppShaders = &pShaderHairDepthResolve;
		hairResolveDepthRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairResolveDepthRootSignatureDesc, &pRootSignatureHairDepthResolve);

		RootSignatureDesc hairFillColorsRootSignatureDesc = {};
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
		hairShadowRootSignatureDesc.ppShaders = &pShaderHairShadow;
		hairShadowRootSignatureDesc.mShaderCount = 1;
		addRootSignature(pRenderer, &hairShadowRootSignatureDesc, &pRootSignatureHairShadow);
#endif
	}

	void DestroyRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignatureBRDF);
		removeRootSignature(pRenderer, pRootSignatureSkybox);
#ifndef DIRECT3D11
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
#endif
	}

	void CreatePBRMaps()
	{
		// PBR Texture values (these values are mirrored on the shaders).
		const uint32_t gBRDFIntegrationSize = 512;
		const uint32_t gSkyboxSize = 1024;
		const uint32_t gSkyboxMips = 11;
		const uint32_t gIrradianceSize = 32;
		const uint32_t gSpecularSize = 128;
		const uint32_t gSpecularMips = (uint)log2(gSpecularSize) + 1;

		// Temporary resources that will be loaded on PBR preprocessing.
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

		SamplerDesc samplerDesc = {
			FILTER_LINEAR, FILTER_LINEAR, MIPMAP_MODE_LINEAR, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, 0, 16
		};
		addSampler(pRenderer, &samplerDesc, &pSkyboxSampler);

		// Load the skybox panorama texture.
		TextureLoadDesc panoDesc = {};
		panoDesc.mRoot = FSR_Textures;
		panoDesc.mUseMipmaps = true;
		panoDesc.pFilename = "LA_Helipad.hdr";
		panoDesc.ppTexture = &pPanoSkybox;
		addResource(&panoDesc);

		TextureDesc skyboxImgDesc = {};
		skyboxImgDesc.mArraySize = 6;
		skyboxImgDesc.mDepth = 1;
		skyboxImgDesc.mFormat = ImageFormat::RGBA32F;
		skyboxImgDesc.mHeight = gSkyboxSize;
		skyboxImgDesc.mWidth = gSkyboxSize;
		skyboxImgDesc.mMipLevels = gSkyboxMips;
		skyboxImgDesc.mSampleCount = SAMPLE_COUNT_1;
		skyboxImgDesc.mSrgb = false;
		skyboxImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		skyboxImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		skyboxImgDesc.pDebugName = L"skyboxImgBuff";

		TextureLoadDesc skyboxLoadDesc = {};
		skyboxLoadDesc.pDesc = &skyboxImgDesc;
		skyboxLoadDesc.ppTexture = &pTextureSkybox;
		addResource(&skyboxLoadDesc);

		TextureDesc irrImgDesc = {};
		irrImgDesc.mArraySize = 6;
		irrImgDesc.mDepth = 1;
		irrImgDesc.mFormat = ImageFormat::RGBA32F;
		irrImgDesc.mHeight = gIrradianceSize;
		irrImgDesc.mWidth = gIrradianceSize;
		irrImgDesc.mMipLevels = 1;
		irrImgDesc.mSampleCount = SAMPLE_COUNT_1;
		irrImgDesc.mSrgb = false;
		irrImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		irrImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		irrImgDesc.pDebugName = L"irrImgBuff";

		TextureLoadDesc irrLoadDesc = {};
		irrLoadDesc.pDesc = &irrImgDesc;
		irrLoadDesc.ppTexture = &pTextureIrradianceMap;
		addResource(&irrLoadDesc);

		TextureDesc specImgDesc = {};
		specImgDesc.mArraySize = 6;
		specImgDesc.mDepth = 1;
		specImgDesc.mFormat = ImageFormat::RGBA32F;
		specImgDesc.mHeight = gSpecularSize;
		specImgDesc.mWidth = gSpecularSize;
		specImgDesc.mMipLevels = gSpecularMips;
		specImgDesc.mSampleCount = SAMPLE_COUNT_1;
		specImgDesc.mSrgb = false;
		specImgDesc.mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
		specImgDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RW_TEXTURE;
		specImgDesc.pDebugName = L"specImgBuff";

		TextureLoadDesc specImgLoadDesc = {};
		specImgLoadDesc.pDesc = &specImgDesc;
		specImgLoadDesc.ppTexture = &pTextureSpecularMap;
		addResource(&specImgLoadDesc);

		// Create empty texture for BRDF integration map.
		TextureLoadDesc brdfIntegrationLoadDesc = {};
		TextureDesc     brdfIntegrationDesc = {};
		brdfIntegrationDesc.mWidth = gBRDFIntegrationSize;
		brdfIntegrationDesc.mHeight = gBRDFIntegrationSize;
		brdfIntegrationDesc.mDepth = 1;
		brdfIntegrationDesc.mArraySize = 1;
		brdfIntegrationDesc.mMipLevels = 1;
		brdfIntegrationDesc.mFormat = ImageFormat::RG32F;
		brdfIntegrationDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
		brdfIntegrationDesc.mSampleCount = SAMPLE_COUNT_1;
		brdfIntegrationDesc.mHostVisible = false;
		brdfIntegrationLoadDesc.pDesc = &brdfIntegrationDesc;
		brdfIntegrationLoadDesc.ppTexture = &pTextureBRDFIntegrationMap;
		addResource(&brdfIntegrationLoadDesc);

		// Load pre-processing shaders.
		ShaderLoadDesc panoToCubeShaderDesc = {};
		panoToCubeShaderDesc.mStages[0] = { "panoToCube.comp", NULL, 0, FSR_SrcShaders };

		GPUPresetLevel presetLevel = pRenderer->pActiveGpuSettings->mGpuVendorPreset.mPresetLevel;
		uint32_t       importanceSampleCounts[GPUPresetLevel::GPU_PRESET_COUNT] = { 0, 0, 64, 128, 256, 1024 };
		uint32_t       importanceSampleCount = importanceSampleCounts[presetLevel];
		ShaderMacro    importanceSampleMacro = { "IMPORTANCE_SAMPLE_COUNT", tinystl::string::format("%u", importanceSampleCount) };

		ShaderLoadDesc brdfIntegrationShaderDesc = {};
		brdfIntegrationShaderDesc.mStages[0] = { "BRDFIntegration.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		ShaderLoadDesc irradianceShaderDesc = {};
		irradianceShaderDesc.mStages[0] = { "computeIrradianceMap.comp", NULL, 0, FSR_SrcShaders };

		ShaderLoadDesc specularShaderDesc = {};
		specularShaderDesc.mStages[0] = { "computeSpecularMap.comp", &importanceSampleMacro, 1, FSR_SrcShaders };

		addShader(pRenderer, &panoToCubeShaderDesc, &pPanoToCubeShader);
		addShader(pRenderer, &brdfIntegrationShaderDesc, &pBRDFIntegrationShader);
		addShader(pRenderer, &irradianceShaderDesc, &pIrradianceShader);
		addShader(pRenderer, &specularShaderDesc, &pSpecularShader);

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
		addRootSignature(pRenderer, &brdfRootDesc, &pBRDFIntegrationRootSignature);
		addRootSignature(pRenderer, &irradianceRootDesc, &pIrradianceRootSignature);
		addRootSignature(pRenderer, &specularRootDesc, &pSpecularRootSignature);

		ComputePipelineDesc pipelineSettings = { 0 };
		pipelineSettings.pShaderProgram = pPanoToCubeShader;
		pipelineSettings.pRootSignature = pPanoToCubeRootSignature;
		addComputePipeline(pRenderer, &pipelineSettings, &pPanoToCubePipeline);
		pipelineSettings.pShaderProgram = pBRDFIntegrationShader;
		pipelineSettings.pRootSignature = pBRDFIntegrationRootSignature;
		addComputePipeline(pRenderer, &pipelineSettings, &pBRDFIntegrationPipeline);
		pipelineSettings.pShaderProgram = pIrradianceShader;
		pipelineSettings.pRootSignature = pIrradianceRootSignature;
		addComputePipeline(pRenderer, &pipelineSettings, &pIrradiancePipeline);
		pipelineSettings.pShaderProgram = pSpecularShader;
		pipelineSettings.pRootSignature = pSpecularRootSignature;
		addComputePipeline(pRenderer, &pipelineSettings, &pSpecularPipeline);

		// Since this happens on iniatilization, use the first cmd/fence pair available.
		Cmd*   cmd = ppCmds[0];
		Fence* pRenderCompleteFence = pRenderCompleteFences[0];

		// Compute the BRDF Integration map.
		beginCmd(cmd);

		TextureBarrier uavBarriers[4] = {
			{ pTextureSkybox, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureIrradianceMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureSpecularMap, RESOURCE_STATE_UNORDERED_ACCESS },
			{ pTextureBRDFIntegrationMap, RESOURCE_STATE_UNORDERED_ACCESS },
		};
		cmdResourceBarrier(cmd, 0, NULL, 4, uavBarriers, false);

		cmdBindPipeline(cmd, pBRDFIntegrationPipeline);
		DescriptorData params[2] = {};
		params[0].pName = "dstTexture";
		params[0].ppTextures = &pTextureBRDFIntegrationMap;
		cmdBindDescriptors(cmd, pBRDFIntegrationRootSignature, 1, params);
		const uint32_t* pThreadGroupSize = pBRDFIntegrationShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, gBRDFIntegrationSize / pThreadGroupSize[0], gBRDFIntegrationSize / pThreadGroupSize[1], pThreadGroupSize[2]);

		TextureBarrier srvBarrier[1] = { { pTextureBRDFIntegrationMap, RESOURCE_STATE_SHADER_RESOURCE } };

		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarrier, true);

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
			params[1].ppTextures = &pTextureSkybox;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pPanoToCubeRootSignature, 2, params);

			pThreadGroupSize = pPanoToCubeShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(
				cmd, max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[0]),
				max(1u, (uint32_t)(data.textureSize >> i) / pThreadGroupSize[1]), 6);
		}

		TextureBarrier srvBarriers[1] = { { pTextureSkybox, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 1, srvBarriers, false);
		/************************************************************************/
		// Compute sky irradiance
		/************************************************************************/
		params[0] = {};
		params[1] = {};
		cmdBindPipeline(cmd, pIrradiancePipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		params[1].pName = "dstTexture";
		params[1].ppTextures = &pTextureIrradianceMap;
		cmdBindDescriptors(cmd, pIrradianceRootSignature, 2, params);
		pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
		cmdDispatch(cmd, gIrradianceSize / pThreadGroupSize[0], gIrradianceSize / pThreadGroupSize[1], 6);
		/************************************************************************/
		// Compute specular sky
		/************************************************************************/
		cmdBindPipeline(cmd, pSpecularPipeline);
		params[0].pName = "srcTexture";
		params[0].ppTextures = &pTextureSkybox;
		cmdBindDescriptors(cmd, pSpecularRootSignature, 1, params);

		struct PrecomputeSkySpecularData
		{
			uint  mipSize;
			float roughness;
		};

		for (uint i = 0; i < gSpecularMips; i++)
		{
			PrecomputeSkySpecularData data = {};
			data.roughness = (float)i / (float)(gSpecularMips - 1);
			data.mipSize = gSpecularSize >> i;
			params[0].pName = "RootConstant";
			params[0].pRootConstant = &data;
			params[1].pName = "dstTexture";
			params[1].ppTextures = &pTextureSpecularMap;
			params[1].mUAVMipSlice = i;
			cmdBindDescriptors(cmd, pSpecularRootSignature, 2, params);
			pThreadGroupSize = pIrradianceShader->mReflection.mStageReflections[0].mNumThreadsPerGroup;
			cmdDispatch(cmd, max(1u, (gSpecularSize >> i) / pThreadGroupSize[0]), max(1u, (gSpecularSize >> i) / pThreadGroupSize[1]), 6);
		}
		/************************************************************************/
		/************************************************************************/
		TextureBarrier srvBarriers2[2] = { { pTextureIrradianceMap, RESOURCE_STATE_SHADER_RESOURCE },
										   { pTextureSpecularMap, RESOURCE_STATE_SHADER_RESOURCE } };
		cmdResourceBarrier(cmd, 0, NULL, 2, srvBarriers2, false);

		endCmd(cmd);
		queueSubmit(pGraphicsQueue, 1, &cmd, pRenderCompleteFence, 0, 0, 0, 0);
		waitForFences(pGraphicsQueue, 1, &pRenderCompleteFence, false);

		// Remove temporary resources.
		removePipeline(pRenderer, pSpecularPipeline);
		removeRootSignature(pRenderer, pSpecularRootSignature);
		removeShader(pRenderer, pSpecularShader);

		removePipeline(pRenderer, pIrradiancePipeline);
		removeRootSignature(pRenderer, pIrradianceRootSignature);
		removeShader(pRenderer, pIrradianceShader);

		removePipeline(pRenderer, pBRDFIntegrationPipeline);
		removeRootSignature(pRenderer, pBRDFIntegrationRootSignature);
		removeShader(pRenderer, pBRDFIntegrationShader);

		removePipeline(pRenderer, pPanoToCubePipeline);
		removeRootSignature(pRenderer, pPanoToCubeRootSignature);
		removeShader(pRenderer, pPanoToCubeShader);

		removeResource(pPanoSkybox);

		removeSampler(pRenderer, pSkyboxSampler);
	}

	void DestroyPBRMaps()
	{
		removeResource(pTextureSpecularMap);
		removeResource(pTextureIrradianceMap);
		removeResource(pTextureSkybox);
		removeResource(pTextureBRDFIntegrationMap);
	}

	void LoadModelsAndTextures()
	{
		bool modelsAreLoaded = false;
		gLuaManager.SetFunction("LoadModel", [this](ILuaStateWrap* state) -> int {
			tinystl::string filename = state->GetStringArg(1);    //indexing in Lua starts from 1 (NOT 0) !!
			this->LoadModel(filename);
			return 0;    //return amount of arguments that we want to send back to script
		});

		tinystl::string loadModelsFilename = FileSystem::FixPath("loadModels.lua", FSR_Middleware2);
		gLuaManager.AddAsyncScript(loadModelsFilename.c_str(), [&modelsAreLoaded](ScriptState state) { modelsAreLoaded = true; });

		while (!modelsAreLoaded)
			Thread::Sleep(0);

		bool texturesAreLoaded = false;
		gLuaManager.SetFunction("GetTextureResolution", [this](ILuaStateWrap* state) -> int {
			state->PushResultString(TEXTURE_RESOLUTION);
			return 1;
		});
		gLuaManager.SetFunction("LoadTextureMaps", [this](ILuaStateWrap* state) -> int {
			tinystl::vector<const char*> texturesNames;
			state->GetStringArrayArg(1, texturesNames);
			bool generateMips = state->GetIntegerArg(2) != 0;    //bool for Lua is integer
			this->LoadTextureMaps(pTextureMaterialMaps, texturesNames.data(), (int)texturesNames.size(), generateMips);
			return 0;
		});
		tinystl::string loadTexturesFilename = FileSystem::FixPath("loadTextures.lua", FSR_Middleware2);
		gLuaManager.AddAsyncScript(loadTexturesFilename.c_str(), [&texturesAreLoaded](ScriptState state) { texturesAreLoaded = true; });

		while (!texturesAreLoaded)
			Thread::Sleep(0);

		bool groundTexturesAreLoaded = false;
		//This is how we can replace function in runtime.
		gLuaManager.SetFunction("LoadTextureMaps", [this](ILuaStateWrap* state) -> int {
			tinystl::vector<const char*> texturesNames;
			state->GetStringArrayArg(1, texturesNames);
			bool generateMips = state->GetIntegerArg(2) != 0;    //bool for Lua is integer
			this->LoadTextureMaps(pTextureMaterialMapsGround, texturesNames.data(), (int)texturesNames.size(), generateMips);
			return 0;
		});
		tinystl::string loadGroundTexturesFilename = FileSystem::FixPath("loadGroundTextures.lua", FSR_Middleware2);
		gLuaManager.AddAsyncScript(
			loadGroundTexturesFilename.c_str(), [&groundTexturesAreLoaded](ScriptState state) { groundTexturesAreLoaded = true; });

		while (!groundTexturesAreLoaded)
			Thread::Sleep(0);
	}

	bool LoadModel(const char* model_name)
	{
		tinystl::vector<Vertex> vertices = {};
		tinystl::vector<uint>   indices = {};
		AssimpImporter          importer;

		AssimpImporter::Model model;
		if (importer.ImportModel(FileSystem::FixPath(model_name, FSR_Meshes).c_str(), &model))
		{
			for (size_t i = 0; i < model.mMeshArray.size(); ++i)
			{
				AssimpImporter::Mesh* mesh = &model.mMeshArray[i];
				vertices.reserve(vertices.size() + mesh->mPositions.size());
				indices.reserve(indices.size() + mesh->mIndices.size());

				for (size_t v = 0; v < mesh->mPositions.size(); ++v)
				{
					Vertex vertex = { float3(0.0f), float3(0.0f, 1.0f, 0.0f) };
					vertex.mPos = mesh->mPositions[v];
					vertex.mNormal = mesh->mNormals[v];
					vertex.mUv = mesh->mUvs[v];
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

			pMeshes.insert(pMeshes.end(), meshData);
		}
		else
			return false;

		return true;
	}

	void DestroyModels()
	{
		for (int i = 0; i < pMeshes.size(); ++i)
		{
			removeResource(pMeshes[i]->pVertexBuffer);
			if (pMeshes[i]->pIndexBuffer)
				removeResource(pMeshes[i]->pIndexBuffer);
			conf_free(pMeshes[i]);
		}
		pMeshes.clear();
	}

	void LoadTextureMaps(tinystl::vector<Texture*>& textures, const char* mapsNames[], int mapsCount, bool generateMips)
	{
		textures.resize(mapsCount);
		for (int i = 0; i < mapsCount; ++i)
		{
			TextureLoadDesc textureDesc = {};
			textureDesc.mRoot = FSR_OtherFiles;
			textureDesc.ppTexture = &textures[i];
			textureDesc.mUseMipmaps = generateMips;
			textureDesc.pFilename = mapsNames[i];
			addResource(&textureDesc, true);
		}
	}

	void DestroyTextures()
	{
		for (uint i = 0; i < pTextureMaterialMaps.size(); ++i)
			removeResource(pTextureMaterialMaps[i]);
		pTextureMaterialMaps.clear();

		for (int i = 0; i < pTextureMaterialMapsGround.size(); ++i)
			removeResource(pTextureMaterialMapsGround[i]);
		pTextureMaterialMapsGround.clear();
	}

	void CreateResources()
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
		skyboxVbDesc.mDesc.mVertexStride = sizeof(float) * 4;
		skyboxVbDesc.pData = skyBoxPoints;
		skyboxVbDesc.ppBuffer = &pVertexBufferSkybox;
		addResource(&skyboxVbDesc);

		// Load hair models
		gUniformDataHairGlobal.mGravity = float4(0.0f, -9.81f, 0.0f, 0.0f);
		gUniformDataHairGlobal.mWind = float4(0.0f);

		NamedCapsule headCapsule = {};
		headCapsule.mName = "Head";
		headCapsule.mCapsule.mCenter0 = float3(-0.41f, 0.0f, 0.0f);
		headCapsule.mCapsule.mRadius0 = 0.5f;
		headCapsule.mCapsule.mCenter1 = float3(-0.83f, 0.0f, 0.0f);
		headCapsule.mCapsule.mRadius1 = 0.5f;
		headCapsule.mAttachedBone = 12;
		gCapsules.push_back(headCapsule);

#if HAIR_MAX_CAPSULE_COUNT >= 3
		NamedCapsule leftShoulderCapsule = {};
		leftShoulderCapsule.mName = "LeftShoulder";
		leftShoulderCapsule.mCapsule.mCenter0 = float3(-0.17f, 0.0f, 0.0f);
		leftShoulderCapsule.mCapsule.mRadius0 = 0.25f;
		leftShoulderCapsule.mCapsule.mCenter1 = float3(0.74f, 0.0f, 0.0f);
		leftShoulderCapsule.mCapsule.mRadius1 = 0.21f;
		leftShoulderCapsule.mAttachedBone = 10;
		gCapsules.push_back(leftShoulderCapsule);

		NamedCapsule rightShoulderCapsule = {};
		rightShoulderCapsule.mName = "RightShoulder";
		rightShoulderCapsule.mCapsule.mCenter0 = float3(-0.17f, 0.0f, 0.0f);
		rightShoulderCapsule.mCapsule.mRadius0 = 0.25f;
		rightShoulderCapsule.mCapsule.mCenter1 = float3(0.74f, 0.0f, 0.0f);
		rightShoulderCapsule.mCapsule.mRadius1 = 0.21f;
		rightShoulderCapsule.mAttachedBone = 11;
		gCapsules.push_back(rightShoulderCapsule);
#endif

		NamedTransform headTransform = {};
		headTransform.mName = "Head";
		headTransform.mTransform.mPosition = vec3(0.0f, -2.2f, 0.0f);
		headTransform.mTransform.mOrientation = vec3(0.0f, PI * 0.5f, -PI * 0.5f);
		headTransform.mTransform.mScale = 0.02f;
		headTransform.mAttachedBone = 12;
		gTransforms.push_back(headTransform);

#ifndef DIRECT3D11
		float gpuPresetScore = (float)((uint)gGPUPresetLevel - GPU_PRESET_LOW) / (float)(GPU_PRESET_ULTRA - GPU_PRESET_LOW);

		// Load all hair meshes
		HairSectionShadingParameters ponytailHairShadingParameters = {};
		ponytailHairShadingParameters.mColorBias = 10.0f;
		ponytailHairShadingParameters.mStrandRadius = 0.04f * gpuPresetScore + 0.21f * (1.0f - gpuPresetScore);
		ponytailHairShadingParameters.mStrandSpacing = 1.0f * gpuPresetScore + 0.2f * (1.0f - gpuPresetScore);
		ponytailHairShadingParameters.mDisableRootColor = true;

		HairSectionShadingParameters hairShadingParameters = {};
		hairShadingParameters.mColorBias = 5.0f;
		hairShadingParameters.mStrandRadius = 0.04f * gpuPresetScore + 0.21f * (1.0f - gpuPresetScore);
		hairShadingParameters.mStrandSpacing = 1.0f * gpuPresetScore + 0.2f * (1.0f - gpuPresetScore);
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

		AddHairMesh(
			HAIR_TYPE_PONYTAIL, "ponytail", "Hair/tail.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &ponytailHairShadingParameters,
			&ponytailHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_PONYTAIL, "top", "Hair/front_top.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_PONYTAIL, "side", "Hair/side.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_PONYTAIL, "back", "Hair/back.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&staticHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_FEMALE_1, "Female hair 1", "Hair/female_hair_1.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&hairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_FEMALE_2, "Female hair 2", "Hair/female_hair_2.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&hairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_FEMALE_3, "Female hair 3", "Hair/female_hair_3.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0, &hairShadingParameters,
			&stiffHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_FEMALE_6, "female hair 6 top", "Hair/female_hair_6_top.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0,
			&hairShadingParameters, &staticHairSimulationParameters);
		AddHairMesh(
			HAIR_TYPE_FEMALE_6, "female hair 6 tail", "Hair/female_hair_6_tail.tfx", (uint)(5 * gpuPresetScore), 0.5f, 0,
			&ponytailHairShadingParameters, &ponytailHairSimulationParameters);
#endif

		// Create skeleton buffers
		const int   sphereResolution = 30;                  // Increase for higher resolution joint spheres
		const float boneWidthRatio = 0.2f;                  // Determines how far along the bone to put the max width [0,1]
		const float jointRadius = boneWidthRatio * 0.5f;    // set to replicate Ozz skeleton

		// Generate joint vertex buffer
		float* pJointPoints;
		generateSpherePoints(&pJointPoints, &gVertexCountSkeletonJoint, sphereResolution, jointRadius);

		uint64_t       jointDataSize = gVertexCountSkeletonJoint * sizeof(float);
		BufferLoadDesc jointVbDesc = {};
		jointVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		jointVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		jointVbDesc.mDesc.mSize = jointDataSize;
		jointVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		jointVbDesc.pData = pJointPoints;
		jointVbDesc.ppBuffer = &pVertexBufferSkeletonJoint;
		addResource(&jointVbDesc);

		// Need to free memory;
		conf_free(pJointPoints);

		// Generate bone vertex buffer
		float* pBonePoints;
		generateBonePoints(&pBonePoints, &gVertexCountSkeletonBone, boneWidthRatio);

		uint64_t       boneDataSize = gVertexCountSkeletonBone * sizeof(float);
		BufferLoadDesc boneVbDesc = {};
		boneVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		boneVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		boneVbDesc.mDesc.mSize = boneDataSize;
		boneVbDesc.mDesc.mVertexStride = sizeof(float) * 6;
		boneVbDesc.pData = pBonePoints;
		boneVbDesc.ppBuffer = &pVertexBufferSkeletonBone;
		addResource(&boneVbDesc);

		// Need to free memory;
		conf_free(pBonePoints);
	}

	void DestroyResources()
	{
		removeResource(pVertexBufferSkybox);
		DestroyHairMeshes();
		removeResource(pVertexBufferSkeletonJoint);
		removeResource(pVertexBufferSkeletonBone);
	}

	void CreateUniformBuffers()
	{
		// Ground plane uniform buffer
		BufferLoadDesc surfaceUBDesc = {};
		surfaceUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		surfaceUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		surfaceUBDesc.mDesc.mSize = sizeof(UniformObjData);
		surfaceUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		surfaceUBDesc.pData = NULL;
		surfaceUBDesc.ppBuffer = &pUniformBufferGroundPlane;
		addResource(&surfaceUBDesc);

		// Nameplate uniform buffers
		BufferLoadDesc nameplateUBDesc = {};
		nameplateUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		nameplateUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		nameplateUBDesc.mDesc.mSize = sizeof(UniformObjData);
		nameplateUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		nameplateUBDesc.pData = NULL;
		for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			nameplateUBDesc.ppBuffer = &pUniformBufferNamePlates[i];
			addResource(&nameplateUBDesc);
		}

		// Create a uniform buffer per mat ball
		for (uint32_t frameIdx = 0; frameIdx < gImageCount; ++frameIdx)
		{
			for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
			{
				BufferLoadDesc matBallUBDesc = {};
				matBallUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				matBallUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
				matBallUBDesc.mDesc.mSize = sizeof(UniformObjData);
				matBallUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
				matBallUBDesc.pData = NULL;
				matBallUBDesc.ppBuffer = &pUniformBufferMatBall[frameIdx][i];
				addResource(&matBallUBDesc);
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
			addResource(&cameraUBDesc);
			cameraUBDesc.ppBuffer = &pUniformBufferCameraSkybox[i];
			addResource(&cameraUBDesc);
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				for (int j = 0; j < MAX_NUM_DIRECTIONAL_LIGHTS; ++j)
				{
					cameraUBDesc.ppBuffer = &pUniformBufferCameraHairShadows[i][hairType][j];
					addResource(&cameraUBDesc);
				}
			}
		}

		// Uniform buffer for light data
		BufferLoadDesc lightsUBDesc = {};
		lightsUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightsUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		lightsUBDesc.mDesc.mSize = sizeof(UniformDataPointLights);
		lightsUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		lightsUBDesc.pData = NULL;
		lightsUBDesc.ppBuffer = &pUniformBufferPointLights;
		addResource(&lightsUBDesc);

		// Uniform buffer for directional light data
		BufferLoadDesc directionalLightBufferDesc = {};
		directionalLightBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		directionalLightBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		directionalLightBufferDesc.mDesc.mSize = sizeof(UniformDataDirectionalLights);
		directionalLightBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		directionalLightBufferDesc.pData = NULL;
		directionalLightBufferDesc.ppBuffer = &pUniformBufferDirectionalLights;
		addResource(&directionalLightBufferDesc);

		// Uniform buffer for hair data
		BufferLoadDesc hairGlobalBufferDesc = {};
		hairGlobalBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairGlobalBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairGlobalBufferDesc.mDesc.mSize = sizeof(UniformDataHairGlobal);
		hairGlobalBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairGlobalBufferDesc.pData = NULL;
		hairGlobalBufferDesc.ppBuffer = &pUniformBufferHairGlobal;
		addResource(&hairGlobalBufferDesc);
	}

	void DestroyUniformBuffers()
	{
		for (uint i = 0; i < gImageCount; ++i)
		{
			removeResource(pUniformBufferCameraSkybox[i]);
			removeResource(pUniformBufferCamera[i]);
			for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
			{
				for (int j = 0; j < MAX_NUM_DIRECTIONAL_LIGHTS; ++j)
					removeResource(pUniformBufferCameraHairShadows[i][hairType][j]);
			}
			for (int j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
				removeResource(pUniformBufferMatBall[i][j]);
		}

		removeResource(pUniformBufferGroundPlane);
		for (int j = 0; j < MATERIAL_INSTANCE_COUNT; ++j)
			removeResource(pUniformBufferNamePlates[j]);

		removeResource(pUniformBufferPointLights);
		removeResource(pUniformBufferDirectionalLights);

		removeResource(pUniformBufferHairGlobal);
	}

	void InitializeUniformBuffers()
	{
		// Update the uniform buffer for the objects
		float baseX = 4.5f;
		float baseY = -1.8f;
		float baseZ = 12.0f;
		float offsetX = 0.1f;
		float offsetZ = 10.0f;
		float scaleVal = 1.0f;
		float roughDelta = 1.0f;
		float materialPlateOffset = 4.0f;

		baseX = 22.0f;
		offsetX = 8.0f;
		scaleVal = 4.0f;

		for (int i = 0; i < MATERIAL_INSTANCE_COUNT; ++i)
		{
			mat4 modelmat =
				mat4::translation(vec3(baseX - i - offsetX * i, baseY, baseZ)) * mat4::scale(vec3(scaleVal)) * mat4::rotationY(PI);

			gUniformDataObject.mWorldMat = modelmat;
			gUniformDataObject.mMetallic = i / (float)MATERIAL_INSTANCE_COUNT;
			gUniformDataObject.mRoughness = 0.04f + roughDelta;
			//gUniformDataObject.textureConfig = 0;
			gUniformDataObject.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL | ETextureConfigFlags::OREN_NAYAR;
			//if not enough materials specified then set pbrMaterials to -1

			gUniformDataMatBall[i] = gUniformDataObject;
			BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferMatBall[gFrameIndex][i], &gUniformDataObject };
			updateResource(&objBuffUpdateDesc);
			roughDelta -= .25f;

			{
				//plates
				modelmat = mat4::translation(vec3(baseX - i - offsetX * i, -5.8f, baseZ + materialPlateOffset)) *
						   mat4::rotationX(3.1415f * 0.2f) * mat4::scale(vec3(3.0f, 0.1f, 1.0f));
				gUniformDataObject.mWorldMat = modelmat;
				gUniformDataObject.mMetallic = 1.0f;
				gUniformDataObject.mRoughness = 0.4f;
				gUniformDataObject.mAlbedo = float3(0.04f);
				gUniformDataObject.textureConfig = 0;
				BufferUpdateDesc objBuffUpdateDesc1 = { pUniformBufferNamePlates[i], &gUniformDataObject };
				updateResource(&objBuffUpdateDesc1);

				//text
				const float ANGLE_OFFSET = 0.6f;    // angle offset to tilt the text shown on the plates for materials
				gTextWorldMats.push_back(
					mat4::translation(vec3(baseX - i - offsetX * i, -6.2f, baseZ + materialPlateOffset - 0.65f)) *
					mat4::rotationX(-PI * 0.5f + ANGLE_OFFSET) * mat4::scale(vec3(16.0f, 10.0f, 1.0f)));
			}
		}

		// ground plane
		vec3 groundScale = vec3(40.0f, 0.2f, 20.0f);
		mat4 modelmat = mat4::translation(vec3(-5.0f, -6.0f, 5.0f)) * mat4::scale(groundScale);
		gUniformDataObject.mWorldMat = modelmat;
		gUniformDataObject.mMetallic = 0;
		gUniformDataObject.mRoughness = 0.74f;
		gUniformDataObject.mAlbedo = float3(0.3f, 0.3f, 0.3f);
		gUniformDataObject.textureConfig = ETextureConfigFlags::TEXTURE_CONFIG_FLAGS_ALL;
		gUniformDataObject.tiling = float2(groundScale.getX() / groundScale.getZ(), 1.0f);
		//gUniformDataObject.textureConfig = ETextureConfigFlags::NORMAL | ETextureConfigFlags::METALLIC | ETextureConfigFlags::AO | ETextureConfigFlags::ROUGHNESS;
		BufferUpdateDesc objBuffUpdateDesc = { pUniformBufferGroundPlane, &gUniformDataObject };
		updateResource(&objBuffUpdateDesc);

		// Directional light
		gUniformDataDirectionalLights.mDirectionalLights[0].mDirection = v3ToF3(normalize(f3Tov3(gDirectionalLightPosition)));
		gUniformDataDirectionalLights.mDirectionalLights[0].mShadowMap = 0;
		gUniformDataDirectionalLights.mDirectionalLights[0].mColor = float3(255.0f, 180.0f, 117.0f) / 255.0f;
		gUniformDataDirectionalLights.mDirectionalLights[0].mIntensity = 10.0f;
		gUniformDataDirectionalLights.mNumDirectionalLights = 1;
		BufferUpdateDesc directionalLightsBufferUpdateDesc = { pUniformBufferDirectionalLights, &gUniformDataDirectionalLights };
		updateResource(&directionalLightsBufferUpdateDesc);

		// Point lights (currently none)
		gUniformDataPointLights.mNumPointLights = 0;
		BufferUpdateDesc pointLightBufferUpdateDesc = { pUniformBufferPointLights, &gUniformDataPointLights };
		updateResource(&pointLightBufferUpdateDesc);
	}

	static void AddHairMesh(
		HairType type, const char* name, const char* tfxFile, uint numFollowHairs, float maxRadiusAroundGuideHair, uint transform,
		HairSectionShadingParameters* shadingParameters, HairSimulationParameters* simulationParameters)
	{
		TFXAsset tfxAsset = {};
		if (!TFXImporter::ImportTFX(
				tfxFile, FSRoot::FSR_OtherFiles, numFollowHairs, simulationParameters->mTipSeperationFactor, maxRadiusAroundGuideHair,
				&tfxAsset))
			return;

		HairBuffer hairBuffer = {};

		hairBuffer.mName = name;

		BufferLoadDesc vertexPositionsBufferDesc = {};
		vertexPositionsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		vertexPositionsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vertexPositionsBufferDesc.mDesc.mElementCount = tfxAsset.mPositions.size();
		vertexPositionsBufferDesc.mDesc.mStructStride = sizeof(float4);
		vertexPositionsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		vertexPositionsBufferDesc.mDesc.mSize =
			vertexPositionsBufferDesc.mDesc.mElementCount * vertexPositionsBufferDesc.mDesc.mStructStride;
		vertexPositionsBufferDesc.pData = tfxAsset.mPositions.data();
		vertexPositionsBufferDesc.ppBuffer = &hairBuffer.pBufferHairVertexPositions;
		vertexPositionsBufferDesc.mDesc.pDebugName = L"Hair vertex positions";
		addResource(&vertexPositionsBufferDesc);

		for (int i = 0; i < 3; ++i)
		{
			vertexPositionsBufferDesc.mDesc.mDescriptors =
				(DescriptorType)(DESCRIPTOR_TYPE_RW_BUFFER | (i == 0 ? DESCRIPTOR_TYPE_BUFFER : 0));
			vertexPositionsBufferDesc.mDesc.pDebugName = L"Hair simulation vertex positions";
			vertexPositionsBufferDesc.ppBuffer = &hairBuffer.pBufferHairSimulationVertexPositions[i];
			addResource(&vertexPositionsBufferDesc);
		}

		BufferLoadDesc vertexTangentsBufferDesc = {};
		vertexTangentsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
		vertexTangentsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		vertexTangentsBufferDesc.mDesc.mElementCount = tfxAsset.mTangents.size();
		vertexTangentsBufferDesc.mDesc.mStructStride = sizeof(float4);
		vertexTangentsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		vertexTangentsBufferDesc.mDesc.mSize = vertexTangentsBufferDesc.mDesc.mElementCount * vertexTangentsBufferDesc.mDesc.mStructStride;
		vertexTangentsBufferDesc.pData = tfxAsset.mTangents.data();
		vertexTangentsBufferDesc.ppBuffer = &hairBuffer.pBufferHairVertexTangents;
		vertexTangentsBufferDesc.mDesc.pDebugName = L"Hair vertex tangents";
		addResource(&vertexTangentsBufferDesc);

		BufferLoadDesc indexBufferDesc = {};
		indexBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
		indexBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		indexBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		indexBufferDesc.mDesc.mIndexType = INDEX_TYPE_UINT32;
		indexBufferDesc.mDesc.mElementCount = tfxAsset.mTriangleIndices.size();
		indexBufferDesc.mDesc.mStructStride = sizeof(uint);
		indexBufferDesc.mDesc.mSize = indexBufferDesc.mDesc.mElementCount * indexBufferDesc.mDesc.mStructStride;
		indexBufferDesc.pData = tfxAsset.mTriangleIndices.data();
		indexBufferDesc.ppBuffer = &hairBuffer.pBufferTriangleIndices;
		indexBufferDesc.mDesc.pDebugName = L"Index buffer hair";
		addResource(&indexBufferDesc);

		BufferLoadDesc restLengthsBufferDesc = {};
		restLengthsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		restLengthsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		restLengthsBufferDesc.mDesc.mElementCount = tfxAsset.mRestLengths.size();
		restLengthsBufferDesc.mDesc.mStructStride = sizeof(float);
		restLengthsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		restLengthsBufferDesc.mDesc.mSize = restLengthsBufferDesc.mDesc.mElementCount * restLengthsBufferDesc.mDesc.mStructStride;
		restLengthsBufferDesc.pData = tfxAsset.mRestLengths.data();
		restLengthsBufferDesc.ppBuffer = &hairBuffer.pBufferHairRestLenghts;
		restLengthsBufferDesc.mDesc.pDebugName = L"Hair rest lenghts";
		addResource(&restLengthsBufferDesc);

		BufferLoadDesc globalRotationsBufferDesc = {};
		globalRotationsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		globalRotationsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		globalRotationsBufferDesc.mDesc.mElementCount = tfxAsset.mGlobalRotations.size();
		globalRotationsBufferDesc.mDesc.mStructStride = sizeof(float4);
		globalRotationsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		globalRotationsBufferDesc.mDesc.mSize =
			globalRotationsBufferDesc.mDesc.mElementCount * globalRotationsBufferDesc.mDesc.mStructStride;
		globalRotationsBufferDesc.pData = tfxAsset.mGlobalRotations.data();
		globalRotationsBufferDesc.ppBuffer = &hairBuffer.pBufferHairGlobalRotations;
		globalRotationsBufferDesc.mDesc.pDebugName = L"Hair global rotations";
		addResource(&globalRotationsBufferDesc);

		BufferLoadDesc refVecsInLocalFrameBufferDesc = {};
		refVecsInLocalFrameBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		refVecsInLocalFrameBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		refVecsInLocalFrameBufferDesc.mDesc.mElementCount = tfxAsset.mRefVectors.size();
		refVecsInLocalFrameBufferDesc.mDesc.mStructStride = sizeof(float4);
		refVecsInLocalFrameBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		refVecsInLocalFrameBufferDesc.mDesc.mSize =
			refVecsInLocalFrameBufferDesc.mDesc.mElementCount * refVecsInLocalFrameBufferDesc.mDesc.mStructStride;
		refVecsInLocalFrameBufferDesc.pData = tfxAsset.mRefVectors.data();
		refVecsInLocalFrameBufferDesc.ppBuffer = &hairBuffer.pBufferHairRefsInLocalFrame;
		refVecsInLocalFrameBufferDesc.mDesc.pDebugName = L"Hair refs in local frame";
		addResource(&refVecsInLocalFrameBufferDesc);

		BufferLoadDesc followHairRootOffsetsBufferDesc = {};
		followHairRootOffsetsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		followHairRootOffsetsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		followHairRootOffsetsBufferDesc.mDesc.mElementCount = tfxAsset.mFollowRootOffsets.size();
		followHairRootOffsetsBufferDesc.mDesc.mStructStride = sizeof(float4);
		followHairRootOffsetsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		followHairRootOffsetsBufferDesc.mDesc.mSize =
			followHairRootOffsetsBufferDesc.mDesc.mElementCount * followHairRootOffsetsBufferDesc.mDesc.mStructStride;
		followHairRootOffsetsBufferDesc.pData = tfxAsset.mFollowRootOffsets.data();
		followHairRootOffsetsBufferDesc.ppBuffer = &hairBuffer.pBufferFollowHairRootOffsets;
		followHairRootOffsetsBufferDesc.mDesc.pDebugName = L"Follow hair root offsets";
		addResource(&followHairRootOffsetsBufferDesc);

		BufferLoadDesc thicknessCoefficientsBufferDesc = {};
		thicknessCoefficientsBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
		thicknessCoefficientsBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		thicknessCoefficientsBufferDesc.mDesc.mElementCount = tfxAsset.mThicknessCoeffs.size();
		thicknessCoefficientsBufferDesc.mDesc.mStructStride = sizeof(float);
		thicknessCoefficientsBufferDesc.mDesc.mFormat = ImageFormat::NONE;
		thicknessCoefficientsBufferDesc.mDesc.mSize =
			thicknessCoefficientsBufferDesc.mDesc.mElementCount * thicknessCoefficientsBufferDesc.mDesc.mStructStride;
		thicknessCoefficientsBufferDesc.pData = tfxAsset.mThicknessCoeffs.data();
		thicknessCoefficientsBufferDesc.ppBuffer = &hairBuffer.pBufferHairThicknessCoefficients;
		thicknessCoefficientsBufferDesc.mDesc.pDebugName = L"Hair thickness coefficients";
		addResource(&thicknessCoefficientsBufferDesc);

		BufferLoadDesc hairShadingBufferDesc = {};
		hairShadingBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairShadingBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairShadingBufferDesc.mDesc.mSize = sizeof(UniformDataHairShading);
		hairShadingBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairShadingBufferDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			hairShadingBufferDesc.ppBuffer = &hairBuffer.pUniformBufferHairShading[i];
			addResource(&hairShadingBufferDesc);
		}

		BufferLoadDesc hairSimulationBufferDesc = {};
		hairSimulationBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		hairSimulationBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		hairSimulationBufferDesc.mDesc.mSize = sizeof(UniformDataHairSimulation);
		hairSimulationBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		hairSimulationBufferDesc.pData = NULL;
		for (int i = 0; i < gImageCount; ++i)
		{
			hairSimulationBufferDesc.ppBuffer = &hairBuffer.pUniformBufferHairSimulation[i];
			addResource(&hairSimulationBufferDesc);
		}

		hairBuffer.mUniformDataHairShading.mColorBias = shadingParameters->mColorBias;
		hairBuffer.mUniformDataHairShading.mStrandRadius = shadingParameters->mStrandRadius;
		hairBuffer.mUniformDataHairShading.mStrandSpacing = shadingParameters->mStrandSpacing;
		hairBuffer.mUniformDataHairShading.mNumVerticesPerStrand = tfxAsset.mNumVerticesPerStrand;

		hairBuffer.mUniformDataHairSimulation.mNumStrandsPerThreadGroup = 64 / tfxAsset.mNumVerticesPerStrand;
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
		hairBuffer.mUniformDataHairSimulation.mNumVerticesPerStrand = tfxAsset.mNumVerticesPerStrand;
#if HAIR_MAX_CAPSULE_COUNT > 0
		hairBuffer.mUniformDataHairSimulation.mCapsuleCount = simulationParameters->mCapsuleCount;
#endif

		hairBuffer.mIndexCountHair = (uint)(tfxAsset.mTriangleIndices.size());
		hairBuffer.mTotalVertexCount = (uint)tfxAsset.mPositions.size();
		hairBuffer.mNumGuideStrands = (uint)tfxAsset.mNumGuideStrands;
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

	static void DestroyHairMeshes()
	{
		for (size_t i = 0; i < gHair.size(); ++i)
		{
			removeResource(gHair[i].pBufferHairVertexPositions);
			for (int j = 0; j < 3; ++j)
				removeResource(gHair[i].pBufferHairSimulationVertexPositions[j]);
			removeResource(gHair[i].pBufferHairVertexTangents);
			removeResource(gHair[i].pBufferTriangleIndices);
			removeResource(gHair[i].pBufferHairRestLenghts);
			removeResource(gHair[i].pBufferHairGlobalRotations);
			removeResource(gHair[i].pBufferHairRefsInLocalFrame);
			removeResource(gHair[i].pBufferFollowHairRootOffsets);
			removeResource(gHair[i].pBufferHairThicknessCoefficients);
			for (int j = 0; j < gImageCount; ++j)
			{
				removeResource(gHair[i].pUniformBufferHairShading[j]);
				removeResource(gHair[i].pUniformBufferHairSimulation[j]);
			}
		}
		gHair.clear();
	}

	static void SetHairColor(HairBuffer* hairBuffer, HairColor hairColor)
	{
		// Fill these variables for each hair color
		float4 rootColor = float4(0.06f, 0.02f, 0.0f, 1.0f);
		float4 strandColor = float4(0.41f, 0.3f, 0.26f, 1.0f);
		float  kDiffuse = 0.14f;
		float  kSpecular1 = 0.03f;
		;
		float kExponent1 = 12.0f;
		;
		float kSpecular2 = 0.02f;
		float kExponent2 = 20.0f;

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

	void LoadAnimations()
	{
#ifndef DIRECT3D11
		// Create skeleton batcher
		SkeletonRenderDesc skeletonRenderDesc = {};
		skeletonRenderDesc.mSkeletonPipeline = pPipelineSkeleton;
		skeletonRenderDesc.mRootSignature = pRootSignatureSkeleton;
		skeletonRenderDesc.mJointVertexBuffer = pVertexBufferSkeletonJoint;
		skeletonRenderDesc.mNumJointPoints = gVertexCountSkeletonJoint;
		skeletonRenderDesc.mDrawBones = true;
		skeletonRenderDesc.mBoneVertexBuffer = pVertexBufferSkeletonBone;
		skeletonRenderDesc.mNumBonePoints = gVertexCountSkeletonBone;

		gSkeletonBatcher.Initialize(skeletonRenderDesc);

		// Load rigs
		tinystl::string rigPath = FileSystem::FixPath("stickFigure/skeleton.ozz", FSR_Animation);
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			gAnimationRig[hairType].Initialize(rigPath.c_str());
			gSkeletonBatcher.AddRig(&gAnimationRig[hairType]);
		}

		// Load clips
		gAnimationClipNeckCrack.Initialize(
			FileSystem::FixPath("stickFigure/animations/neckCrack.ozz", FSR_Animation).c_str(), &gAnimationRig[0]);
		gAnimationClipStand.Initialize(FileSystem::FixPath("stickFigure/animations/stand.ozz", FSR_Animation).c_str(), &gAnimationRig[0]);

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			// Create clip controllers
			gAnimationClipControllerNeckCrack[hairType].Initialize(gAnimationClipNeckCrack.GetDuration(), NULL);
			gAnimationClipControllerStand[hairType].Initialize(gAnimationClipStand.GetDuration(), NULL);

			/*gAnimationClipControllerNeckCrack[hairType].SetPlay(false);
			gAnimationClipControllerStand[hairType].SetPlay(false);*/

			// Create animations
			AnimationDesc animationDesc{};
			animationDesc.mRig = &gAnimationRig[hairType];
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
			gAnimatedObject[hairType].Initialize(&gAnimationRig[hairType], &gAnimation[hairType]);
		}
#endif
	}

	void DestroyAnimations()
	{
#ifndef DIRECT3D11
		// Destroy clips
		gAnimationClipNeckCrack.Destroy();
		gAnimationClipStand.Destroy();

		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			// Destroy rigs
			gAnimationRig[hairType].Destroy();

			// Destroy animations
			gAnimation[hairType].Destroy();

			// Destroy animated objects
			gAnimatedObject[hairType].Destroy();
		}

		// Destroy skeleton batcher
		gSkeletonBatcher.Destroy();
#endif
	}

	//--------------------------------------------------------------------------------------------
	// LOAD FUNCTIONS
	//--------------------------------------------------------------------------------------------
	void CreatePipelines()
	{
		// Create vertex layouts
		VertexLayout skyboxVertexLayout = {};
		skyboxVertexLayout.mAttribCount = 1;
		skyboxVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		skyboxVertexLayout.mAttribs[0].mFormat = ImageFormat::RGBA32F;
		skyboxVertexLayout.mAttribs[0].mBinding = 0;
		skyboxVertexLayout.mAttribs[0].mLocation = 0;
		skyboxVertexLayout.mAttribs[0].mOffset = 0;

		VertexLayout defaultVertexLayout = {};
		defaultVertexLayout.mAttribCount = 3;
		defaultVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		defaultVertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		defaultVertexLayout.mAttribs[0].mBinding = 0;
		defaultVertexLayout.mAttribs[0].mLocation = 0;
		defaultVertexLayout.mAttribs[0].mOffset = 0;
		defaultVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		defaultVertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
		defaultVertexLayout.mAttribs[1].mLocation = 1;
		defaultVertexLayout.mAttribs[1].mBinding = 0;
		defaultVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
		defaultVertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
		defaultVertexLayout.mAttribs[2].mFormat = ImageFormat::RG32F;
		defaultVertexLayout.mAttribs[2].mLocation = 2;
		defaultVertexLayout.mAttribs[2].mBinding = 0;
		defaultVertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

		VertexLayout skeletonVertexLayout = {};
		skeletonVertexLayout.mAttribCount = 2;
		skeletonVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		skeletonVertexLayout.mAttribs[0].mFormat = ImageFormat::RGB32F;
		skeletonVertexLayout.mAttribs[0].mBinding = 0;
		skeletonVertexLayout.mAttribs[0].mLocation = 0;
		skeletonVertexLayout.mAttribs[0].mOffset = 0;
		skeletonVertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
		skeletonVertexLayout.mAttribs[1].mFormat = ImageFormat::RGB32F;
		skeletonVertexLayout.mAttribs[1].mLocation = 1;
		skeletonVertexLayout.mAttribs[1].mBinding = 0;
		skeletonVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		// Create pipelines
		GraphicsPipelineDesc pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = NULL;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = ImageFormat::NONE;
		pipelineSettings.pRootSignature = pRootSignatureSkybox;
		pipelineSettings.pShaderProgram = pShaderSkybox;
		pipelineSettings.pVertexLayout = &skyboxVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineSkybox);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureBRDF;
		pipelineSettings.pShaderProgram = pShaderBRDF;
		pipelineSettings.pVertexLayout = &defaultVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineBRDF);

#ifndef DIRECT3D11
		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = pDepthStateDisable;
		pipelineSettings.pColorFormats = NULL;
		pipelineSettings.pSrgbValues = NULL;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairClear;
		pipelineSettings.pShaderProgram = pShaderHairClear;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = NULL;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairClear);

		ImageFormat::Enum depthPeelingFormat = ImageFormat::R16F;
		bool              srgb = false;

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateNoWrite;
		pipelineSettings.pColorFormats = &depthPeelingFormat;
		pipelineSettings.pSrgbValues = &srgb;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairDepthPeeling;
		pipelineSettings.pShaderProgram = pShaderHairDepthPeeling;
		pipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		pipelineSettings.pBlendState = pBlendStateDepthPeeling;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairDepthPeeling);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = pDepthStateDepthResolve;
		pipelineSettings.pColorFormats = NULL;
		pipelineSettings.pSrgbValues = NULL;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairDepthResolve;
		pipelineSettings.pShaderProgram = pShaderHairDepthResolve;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairDepthResolve);

		ImageFormat::Enum fillColorsFormat = ImageFormat::RGBA16F;

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateNoWrite;
		pipelineSettings.pColorFormats = &fillColorsFormat;
		pipelineSettings.pSrgbValues = &srgb;
		pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
		pipelineSettings.mSampleQuality = 0;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairFillColors;
		pipelineSettings.pShaderProgram = pShaderHairFillColors;
		pipelineSettings.pRasterizerState = pRasterizerStateCullFront;
		pipelineSettings.pBlendState = pBlendStateAdd;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairFillColors);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateDisable;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairColorResolve;
		pipelineSettings.pShaderProgram = pShaderHairResolveColor;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = pBlendStateColorResolve;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairColorResolve);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 0;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetHairShadows[0][0]->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureHairShadow;
		pipelineSettings.pShaderProgram = pShaderHairShadow;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineHairShadow);

		ComputePipelineDesc computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairIntegrate;
		computePipelineDesc.pShaderProgram = pShaderHairIntegrate;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairIntegrate);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairShockPropagation;
		computePipelineDesc.pShaderProgram = pShaderHairShockPropagation;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairShockPropagation);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairLocalConstraints;
		computePipelineDesc.pShaderProgram = pShaderHairLocalConstraints;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairLocalConstraints);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairLengthConstraints;
		computePipelineDesc.pShaderProgram = pShaderHairLengthConstraints;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairLengthConstraints);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairUpdateFollowHairs;
		computePipelineDesc.pShaderProgram = pShaderHairUpdateFollowHairs;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairUpdateFollowHairs);

		computePipelineDesc = {};
		computePipelineDesc.pRootSignature = pRootSignatureHairPreWarm;
		computePipelineDesc.pShaderProgram = pShaderHairPreWarm;
		addComputePipeline(pRenderer, &computePipelineDesc, &pPipelineHairPreWarm);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateNoWrite;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureShowCapsules;
		pipelineSettings.pShaderProgram = pShaderShowCapsules;
		pipelineSettings.pVertexLayout = &defaultVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = pBlendStateAlphaBlend;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineShowCapsules);

		pipelineSettings = {};
		pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		pipelineSettings.mRenderTargetCount = 1;
		pipelineSettings.pDepthState = pDepthStateEnable;
		pipelineSettings.pColorFormats = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mFormat;
		pipelineSettings.pSrgbValues = &pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSrgb;
		pipelineSettings.mSampleCount = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleCount;
		pipelineSettings.mSampleQuality = pSwapChain->ppSwapchainRenderTargets[0]->mDesc.mSampleQuality;
		pipelineSettings.mDepthStencilFormat = pRenderTargetDepth->mDesc.mFormat;
		pipelineSettings.pRootSignature = pRootSignatureSkeleton;
		pipelineSettings.pShaderProgram = pShaderSkeleton;
		pipelineSettings.pVertexLayout = &skeletonVertexLayout;
		pipelineSettings.pRasterizerState = pRasterizerStateCullNone;
		pipelineSettings.pBlendState = NULL;
		addPipeline(pRenderer, &pipelineSettings, &pPipelineSkeleton);
		gSkeletonBatcher.LoadPipeline(pPipelineSkeleton);

		gUniformDataHairGlobal.mViewport = float4(0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight);
#endif
	}

	void DestroyPipelines()
	{
		removePipeline(pRenderer, pPipelineSkybox);
		removePipeline(pRenderer, pPipelineBRDF);
#ifndef DIRECT3D11
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
#endif
	}

	void CreateRenderTargets()
	{
		RenderTargetDesc depthPeelingRenderTargetDesc = {};
		depthPeelingRenderTargetDesc.mWidth = mSettings.mWidth;
		depthPeelingRenderTargetDesc.mHeight = mSettings.mHeight;
		depthPeelingRenderTargetDesc.mDepth = 1;
		depthPeelingRenderTargetDesc.mArraySize = 1;
		depthPeelingRenderTargetDesc.mMipLevels = 1;
		depthPeelingRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		depthPeelingRenderTargetDesc.mFormat = ImageFormat::R16F;
		depthPeelingRenderTargetDesc.mClearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
		depthPeelingRenderTargetDesc.mSampleQuality = 0;
		depthPeelingRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		depthPeelingRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		depthPeelingRenderTargetDesc.pDebugName = L"Depth peeling RT";
		addRenderTarget(pRenderer, &depthPeelingRenderTargetDesc, &pRenderTargetDepthPeeling);

#ifndef METAL
		TextureDesc hairDepthsTextureDesc = {};
		hairDepthsTextureDesc.mWidth = mSettings.mWidth;
		hairDepthsTextureDesc.mHeight = mSettings.mHeight;
		hairDepthsTextureDesc.mDepth = 1;
		hairDepthsTextureDesc.mArraySize = 3;
		hairDepthsTextureDesc.mMipLevels = 1;
		hairDepthsTextureDesc.mSampleCount = SAMPLE_COUNT_1;
		hairDepthsTextureDesc.mFormat = ImageFormat::R32UI;
		hairDepthsTextureDesc.mClearValue = { 1.0f, 1.0f, 1.0f, 1.0f };
		hairDepthsTextureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
		hairDepthsTextureDesc.pDebugName = L"Hair depths texture";

		TextureLoadDesc hairDepthsTextureLoadDesc = {};
		hairDepthsTextureLoadDesc.pDesc = &hairDepthsTextureDesc;
		hairDepthsTextureLoadDesc.ppTexture = &pTextureHairDepth;
		addResource(&hairDepthsTextureLoadDesc);
#else
		BufferLoadDesc hairDepthsBufferLoadDesc = {};
		hairDepthsBufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
		hairDepthsBufferLoadDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight * 3;
		hairDepthsBufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		hairDepthsBufferLoadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		hairDepthsBufferLoadDesc.mDesc.mStructStride = sizeof(uint);
		hairDepthsBufferLoadDesc.mDesc.mSize = hairDepthsBufferLoadDesc.mDesc.mElementCount * hairDepthsBufferLoadDesc.mDesc.mStructStride;
		hairDepthsBufferLoadDesc.mDesc.pDebugName = L"Hair depths buffer";
		hairDepthsBufferLoadDesc.ppBuffer = &pBufferHairDepth;
		addResource(&hairDepthsBufferLoadDesc);
#endif

		RenderTargetDesc fillColorsRenderTargetDesc = {};
		fillColorsRenderTargetDesc.mWidth = mSettings.mWidth;
		fillColorsRenderTargetDesc.mHeight = mSettings.mHeight;
		fillColorsRenderTargetDesc.mDepth = 1;
		fillColorsRenderTargetDesc.mArraySize = 1;
		fillColorsRenderTargetDesc.mMipLevels = 1;
		fillColorsRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		fillColorsRenderTargetDesc.mFormat = ImageFormat::RGBA16F;
		fillColorsRenderTargetDesc.mClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		fillColorsRenderTargetDesc.mSampleQuality = 0;
		fillColorsRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		fillColorsRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		fillColorsRenderTargetDesc.pDebugName = L"Fill colors RT";
		addRenderTarget(pRenderer, &fillColorsRenderTargetDesc, &pRenderTargetFillColors);

		RenderTargetDesc hairShadowRenderTargetDesc = {};
		hairShadowRenderTargetDesc.mWidth = 1024;
		hairShadowRenderTargetDesc.mHeight = 1024;
		hairShadowRenderTargetDesc.mDepth = 1;
		hairShadowRenderTargetDesc.mArraySize = 1;
		hairShadowRenderTargetDesc.mMipLevels = 1;
		hairShadowRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		hairShadowRenderTargetDesc.mFormat = ImageFormat::D16;
		hairShadowRenderTargetDesc.mClearValue = { 1.0f, 0 };
		hairShadowRenderTargetDesc.mSampleQuality = 0;
		hairShadowRenderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
		hairShadowRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		hairShadowRenderTargetDesc.pDebugName = L"Hair shadow RT";
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				addRenderTarget(pRenderer, &hairShadowRenderTargetDesc, &pRenderTargetHairShadows[hairType][i]);
		}

		RenderTargetDesc depthRenderTargetDesc = {};
		depthRenderTargetDesc.mArraySize = 1;
		depthRenderTargetDesc.mClearValue = { 1.0f, 0 };
		depthRenderTargetDesc.mDepth = 1;
		depthRenderTargetDesc.mFormat = ImageFormat::D32F;
		depthRenderTargetDesc.mHeight = mSettings.mHeight;
		depthRenderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
		depthRenderTargetDesc.mSampleQuality = 0;
		depthRenderTargetDesc.mWidth = mSettings.mWidth;
		depthRenderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		depthRenderTargetDesc.pDebugName = L"Depth buffer";

		addRenderTarget(pRenderer, &depthRenderTargetDesc, &pRenderTargetDepth);

		SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = pWindow;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true);
		swapChainDesc.mColorClearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
		swapChainDesc.mSrgb = false;
		swapChainDesc.mEnableVsync = false;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
	}

	void DestroyRenderTargets()
	{
		removeRenderTarget(pRenderer, pRenderTargetDepthPeeling);
#ifndef METAL
		removeResource(pTextureHairDepth);
#else
		removeResource(pBufferHairDepth);
#endif
		removeRenderTarget(pRenderer, pRenderTargetFillColors);
		for (uint hairType = 0; hairType < HAIR_TYPE_COUNT; ++hairType)
		{
			for (int i = 0; i < MAX_NUM_DIRECTIONAL_LIGHTS; ++i)
				removeRenderTarget(pRenderer, pRenderTargetHairShadows[hairType][0]);
		}
		removeRenderTarget(pRenderer, pRenderTargetDepth);
		removeSwapChain(pRenderer, pSwapChain);
	}
};

//--------------------------------------------------------------------------------------------
// UI
//--------------------------------------------------------------------------------------------
void GuiController::UpdateDynamicUI()
{
	if (gMaterialType != GuiController::currentMaterialType)
	{
		if (gMaterialType != MATERIAL_HAIR)
		{
			GuiController::hairShadingDynamicWidgets.HideDynamicProperties(pGuiWindowMain);
			GuiController::hairSimulationDynamicWidgets.HideDynamicProperties(pGuiWindowHairSimulation);

			GuiController::hairDynamicWidgets[gHairType].hairShading.HideDynamicProperties(pGuiWindowMain);
			GuiController::hairDynamicWidgets[gHairType].hairSimulation.HideDynamicProperties(pGuiWindowHairSimulation);
		}

		if (gMaterialType == MATERIAL_HAIR)
		{
			GuiController::hairShadingDynamicWidgets.ShowDynamicProperties(pGuiWindowMain);
			GuiController::hairSimulationDynamicWidgets.ShowDynamicProperties(pGuiWindowHairSimulation);

			GuiController::hairDynamicWidgets[gHairType].hairShading.ShowDynamicProperties(pGuiWindowMain);
			GuiController::hairDynamicWidgets[gHairType].hairSimulation.ShowDynamicProperties(pGuiWindowHairSimulation);
			gFirstHairSimulationFrame = true;
		}

		GuiController::currentMaterialType = (MaterialType)gMaterialType;
	}

	if (gHairType != GuiController::currentHairType)
	{
		GuiController::hairDynamicWidgets[gHairType].hairShading.HideDynamicProperties(pGuiWindowMain);
		GuiController::hairDynamicWidgets[gHairType].hairSimulation.HideDynamicProperties(pGuiWindowHairSimulation);

		gHairType = GuiController::currentHairType;
		GuiController::hairDynamicWidgets[gHairType].hairShading.ShowDynamicProperties(pGuiWindowMain);
		GuiController::hairDynamicWidgets[gHairType].hairSimulation.ShowDynamicProperties(pGuiWindowHairSimulation);
	}

#if !defined(TARGET_IOS) && !defined(_DURANGO)
	if (pSwapChain->mDesc.mEnableVsync != gVSyncEnabled)
	{
		waitForFences(pGraphicsQueue, gImageCount, pRenderCompleteFences, true);
		::toggleVSync(pRenderer, &pSwapChain);
	}
#endif
}

void GuiController::AddGui()
{
	// Dropdown structs
	static const char* materialTypeNames[MATERIAL_COUNT] = {
		"Metals",
		"Wood",
		"Hair",
	};
	static const uint32_t materialTypeValues[MATERIAL_COUNT] = {
		MATERIAL_METAL,
		MATERIAL_WOOD,
		MATERIAL_HAIR,
	};
	uint32_t dropDownCount = (sizeof(materialTypeNames) / sizeof(materialTypeNames[0]));
#ifdef DIRECT3D11
	--dropDownCount;
#endif

#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
	static const char*    diffuseReflectionNames[] = { "Lambert", "Oren-Nayar", NULL };
	static const uint32_t diffuseReflectionValues[] = {
		LAMBERT_REFLECTION, OREN_NAYAR_REFLECTION, MATERIAL_HAIR,
		0    //needed for unix
	};
	const uint32_t dropDownCount2 = (sizeof(diffuseReflectionNames) / sizeof(diffuseReflectionNames[0])) - 1;
#endif

	static const char*    renderModeNames[] = { "Shaded", "Albedo", "Normals", "Roughness", "Metallic", "AO", NULL };
	static const uint32_t renderModeVals[] = {
		RENDER_MODE_SHADED, RENDER_MODE_ALBEDO, RENDER_MODE_NORMALS, RENDER_MODE_ROUGHNESS, RENDER_MODE_METALLIC, RENDER_MODE_AO, NULL
	};
	const uint32_t dropDownCount3 = (sizeof(renderModeNames) / sizeof(renderModeNames[0])) - 1;

	// MAIN GUI
#if !defined(TARGET_IOS) && !defined(_DURANGO)
	pGuiWindowMain->AddWidget(CheckboxWidget("V-Sync", &gVSyncEnabled));
#endif
	pGuiWindowMain->AddWidget(DropdownWidget("Material Type", &gMaterialType, materialTypeNames, materialTypeValues, dropDownCount));
	pGuiWindowMain->AddWidget(CheckboxWidget("Animate Camera", &gbAnimateCamera));
	pGuiWindowMain->AddWidget(CheckboxWidget("Environment Lighting", &gEnvironmentLighting));
	pGuiWindowMain->AddWidget(CheckboxWidget("Skybox", &gDrawSkybox));
	pGuiWindowMain->AddWidget(SliderFloat3Widget("Light Position", &gDirectionalLightPosition, float3(-10.0f), float3(10.0f)));
	ButtonWidget ReloadScriptButton("Reload script");
	ReloadScriptButton.pOnDeactivatedAfterEdit = ReloadScriptButtonCallback;
	pGuiWindowMain->AddWidget(ReloadScriptButton);

	// MATERIAL PROPS GUI

	pGuiWindowMaterial->AddWidget(DropdownWidget("Render Mode", &gRenderMode, renderModeNames, renderModeVals, dropDownCount3));
#if ENABLE_DIFFUSE_REFLECTION_DROPDOWN
	pGuiWindowMaterial->AddWidget(DropdownWidget(
		"Diffuse Reflection Model", &gDiffuseReflectionModel, diffuseReflectionNames, diffuseReflectionValues, dropDownCount2));
#endif
	pGuiWindowMaterial->AddWidget(CheckboxWidget("Override Roughness", &gOverrideRoughnessTextures));
	pGuiWindowMaterial->AddWidget(SliderFloatWidget("Roughness", &gRoughnessOverride, 0.04f, 1.0f));
	pGuiWindowMaterial->AddWidget(CheckboxWidget("Disable Normal Maps", &gDisableNormalMaps));
	pGuiWindowMaterial->AddWidget(CheckboxWidget("Disable AO Maps", &gDisableAOMaps));
	pGuiWindowMaterial->AddWidget(SliderFloatWidget("AO Intensity", &gAOIntensity, 0.0f, 1.0f, 0.001f));

	// HAIR GUI
	{
		GuiController::hairDynamicWidgets.resize(HAIR_TYPE_COUNT);

		static const char* hairNames[HAIR_TYPE_COUNT] = { "Ponytail", "Female hair 1", "Female hair 2", "Female hair 3", "Female hair 6" };

		static const uint32_t hairTypeValues[HAIR_TYPE_COUNT] = { HAIR_TYPE_PONYTAIL, HAIR_TYPE_FEMALE_1, HAIR_TYPE_FEMALE_2,
																  HAIR_TYPE_FEMALE_3, HAIR_TYPE_FEMALE_6 };

		static const char* hairColorNames[HAIR_COLOR_COUNT] = { "Brown", "Blonde", "Black", "Red" };

		static const uint32_t hairColorValues[HAIR_COLOR_COUNT] = { HAIR_COLOR_BROWN, HAIR_COLOR_BLONDE, HAIR_COLOR_BLACK, HAIR_COLOR_RED };

		// Hair shading widgets
		static LabelWidget hairShading("Hair Shading");
		GuiController::hairShadingDynamicWidgets.mDynamicProperties.emplace_back(&hairShading);

		static DropdownWidget hairColor("Hair Color", &gHairColor, hairColorNames, hairColorValues, HAIR_COLOR_COUNT);
		GuiController::hairShadingDynamicWidgets.mDynamicProperties.emplace_back(&hairColor);

#if HAIR_DEV_UI
		static DropdownWidget hairWidget("Hair type", &GuiController::currentHairType, hairNames, hairTypeValues, HAIR_TYPE_COUNT);
		GuiController::hairShadingDynamicWidgets.mDynamicProperties.emplace_back(&hairWidget);

		for (uint i = 0; i < HAIR_TYPE_COUNT; ++i)
		{
			for (size_t j = 0; j < gHairTypeIndices[i].size(); ++j)
			{
				uint                    k = gHairTypeIndices[i][j];
				CollapsingHeaderWidget* header =
					conf_placement_new<CollapsingHeaderWidget>(conf_malloc(sizeof(CollapsingHeaderWidget)), gHair[k].mName);
				GuiController::hairDynamicWidgets[i].hairWidgets.push_back(header);
				GuiController::hairDynamicWidgets[i].hairShading.mDynamicProperties.emplace_back(header);
				UniformDataHairShading* hair = &gHair[k].mUniformDataHairShading;

				ColorSliderWidget rootColor("Root Color", &hair->mRootColor);
				header->AddSubWidget(rootColor);

				ColorSliderWidget strandColor("Strand Color", &hair->mStrandColor);
				header->AddSubWidget(strandColor);

				SliderFloatWidget colorBias("Color Bias", &hair->mColorBias, 0.0f, 10.0f);
				header->AddSubWidget(colorBias);

				SliderFloatWidget diffuse("Kd", &hair->mKDiffuse, 0.0f, 1.0f);
				header->AddSubWidget(diffuse);

				SliderFloatWidget specular1("Ks1", &hair->mKSpecular1, 0.0f, 1.0f);
				header->AddSubWidget(specular1);

				SliderFloatWidget exponent1("Ex1", &hair->mKExponent1, 0.0f, 128.0f);
				header->AddSubWidget(exponent1);

				SliderFloatWidget specular2("Ks2", &hair->mKSpecular2, 0.0f, 1.0f);
				header->AddSubWidget(specular2);

				SliderFloatWidget exponent2("Ex2", &hair->mKExponent2, 0.0f, 128.0f);
				header->AddSubWidget(exponent2);

				SliderFloatWidget fiberRadius("Strand Radius", &gHair[k].mStrandRadius, 0.0f, 1.0f, 0.01f);
				header->AddSubWidget(fiberRadius);

				SliderFloatWidget fiberSpacing("Strand Spacing", &gHair[k].mStrandSpacing, 0.0f, 1.0f, 0.01f);
				header->AddSubWidget(fiberSpacing);
			}
		}
#endif

		// Hair simulation widgets
		static LabelWidget hairSimulation("Hair Simulation");
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&hairSimulation);

		static SliderFloat3Widget gravity("Gravity", (float3*)&gUniformDataHairGlobal.mGravity, float3(-10.0f), float3(10.0f));
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&gravity);

		static SliderFloat3Widget wind("Wind", (float3*)&gUniformDataHairGlobal.mWind, float3(-1024.0f), float3(1024.0f));
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&wind);

#if HAIR_MAX_CAPSULE_COUNT > 0
		static CheckboxWidget showCapsules("Show Collision Capsules", &gShowCapsules);
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&showCapsules);
#endif

#if HAIR_DEV_UI
		static CollapsingHeaderWidget transformHeader("Transforms");
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&transformHeader);

		for (size_t i = 0; i < gTransforms.size(); ++i)
		{
			CollapsingHeaderWidget header(gTransforms[i].mName);
			Transform*             transform = &gTransforms[i].mTransform;

			SliderFloat3Widget position("Position", (float3*)&transform->mPosition, float3(-10.0f), float3(10.0f));
			header.AddSubWidget(position);

			SliderFloat3Widget orientation("Orientation", (float3*)&transform->mOrientation, float3(-PI * 2.0f), float3(PI * 2.0f));
			header.AddSubWidget(orientation);

			SliderFloatWidget scale("Scale", &transform->mScale, 0.001f, 1.0f, 0.001f);
			header.AddSubWidget(scale);

			SliderIntWidget attachedBone("Attached To Bone", &gTransforms[i].mAttachedBone, -1, gAnimationRig[0].GetNumJoints() - 1);
			header.AddSubWidget(attachedBone);

			transformHeader.AddSubWidget(header);
		}

#if HAIR_MAX_CAPSULE_COUNT > 0
		static CollapsingHeaderWidget capsuleHeader("Capsules");
		GuiController::hairSimulationDynamicWidgets.mDynamicProperties.emplace_back(&capsuleHeader);

		for (size_t i = 0; i < gCapsules.size(); ++i)
		{
			CollapsingHeaderWidget header(gCapsules[i].mName);
			Capsule*               capsule = &gCapsules[i].mCapsule;

			SliderFloat3Widget center0("Center0", &capsule->mCenter0, float3(-10.0f), float3(10.0f));
			header.AddSubWidget(center0);

			SliderFloatWidget radius0("Radius0", &capsule->mRadius0, 0.0f, 10.0f);
			header.AddSubWidget(radius0);

			SliderFloat3Widget center1("Center1", &capsule->mCenter1, float3(-10.0f), float3(10.0f));
			header.AddSubWidget(center1);

			SliderFloatWidget radius1("Radius1", &capsule->mRadius1, 0.0f, 10.0f);
			header.AddSubWidget(radius1);

			SliderIntWidget attachedBone("Attached To Bone", &gCapsules[i].mAttachedBone, -1, gAnimationRig[0].GetNumJoints() - 1);
			header.AddSubWidget(attachedBone);

			capsuleHeader.AddSubWidget(header);
		}
#endif

		for (uint i = 0; i < HAIR_TYPE_COUNT; ++i)
		{
			for (size_t j = 0; j < gHairTypeIndices[i].size(); ++j)
			{
				uint                    k = gHairTypeIndices[i][j];
				CollapsingHeaderWidget* header =
					conf_placement_new<CollapsingHeaderWidget>(conf_malloc(sizeof(CollapsingHeaderWidget)), gHair[k].mName);
				GuiController::hairDynamicWidgets[i].hairWidgets.push_back(header);
				GuiController::hairDynamicWidgets[i].hairSimulation.mDynamicProperties.emplace_back(header);
				UniformDataHairSimulation* hair = &gHair[k].mUniformDataHairSimulation;

				SliderFloatWidget damping("Damping", &hair->mDamping, 0.0f, 0.1f, 0.001f);
				header->AddSubWidget(damping);

				SliderFloatWidget globalConstraintStiffness(
					"Global Constraint Stiffness", &hair->mGlobalConstraintStiffness, 0.0f, 0.1f, 0.001f);
				header->AddSubWidget(globalConstraintStiffness);

				SliderFloatWidget globalConstraintRange("Global Constraint Range", &hair->mGlobalConstraintRange, 0.0f, 1.0f);
				header->AddSubWidget(globalConstraintRange);

				SliderFloatWidget vspStrength("Shock Propagation Strength", &hair->mShockPropagationStrength, 0.0f, 1.0f);
				header->AddSubWidget(vspStrength);

				SliderFloatWidget vspAccelerationThreshold(
					"Shock Propagation Acceleration Threshold", &hair->mShockPropagationAccelerationThreshold, 0.0f, 10.0f);
				header->AddSubWidget(vspAccelerationThreshold);

				SliderFloatWidget localStiffness("Local Stiffness", &hair->mLocalStiffness, 0.0f, 1.0f);
				header->AddSubWidget(localStiffness);

				SliderUintWidget localConstraintIterations("Local Constraint Iterations", &hair->mLocalConstraintIterations, 0, 32);
				header->AddSubWidget(localConstraintIterations);

				SliderUintWidget lengthConstraintIterations("Length Constraint Iterations", &hair->mLengthConstraintIterations, 1, 32);
				header->AddSubWidget(lengthConstraintIterations);
			}
		}
#endif
	}

	if (gMaterialType == MaterialType::MATERIAL_HAIR)
	{
		GuiController::currentMaterialType = MaterialType::MATERIAL_HAIR;
		GuiController::hairShadingDynamicWidgets.ShowDynamicProperties(pGuiWindowMain);
		GuiController::hairSimulationDynamicWidgets.ShowDynamicProperties(pGuiWindowHairSimulation);
		GuiController::hairDynamicWidgets[gHairType].hairShading.ShowDynamicProperties(pGuiWindowMain);
		GuiController::hairDynamicWidgets[gHairType].hairSimulation.ShowDynamicProperties(pGuiWindowHairSimulation);
	}
}

void GuiController::Exit()
{
	for (size_t i = 0; i < hairDynamicWidgets.size(); ++i)
	{
		for (size_t j = 0; j < hairDynamicWidgets[i].hairWidgets.size(); ++j)
			conf_free(hairDynamicWidgets[i].hairWidgets[j]);
	}

	hairDynamicWidgets.clear();
}

DEFINE_APPLICATION_MAIN(MaterialPlayground)
