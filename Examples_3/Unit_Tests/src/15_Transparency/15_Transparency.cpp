/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../../Common_3/Utilities/Threading/ThreadSystem.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Math/ShaderUtilities.h"
#include "Shaders/Shared.h"

// input
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define FOREACH_SETTING(X)    X(EnableAOIT, 0)

#define GENERATE_ENUM(x, y)   x,
#define GENERATE_STRING(x, y) #x,
#define GENERATE_STRUCT(x, y) uint32_t m##x;
#define GENERATE_VALUE(x, y)  y,
#define INIT_STRUCT(s)        s = { FOREACH_SETTING(GENERATE_VALUE) }

typedef enum ESettings
{
    FOREACH_SETTING(GENERATE_ENUM) Count
} ESettings;

const char* gSettingNames[] = { FOREACH_SETTING(GENERATE_STRING) };

// Useful for using names directly instead of subscripting an array
struct ConfigSettings
{
    FOREACH_SETTING(GENERATE_STRUCT)
} gGpuSettings;

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;

typedef struct ParticleVertex
{
    float3   mPosition;
    uint32_t mNormal; // packed normal
    uint32_t mUV;     // packed uv
} ParticleVertex;

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
    Buffer* pParticleBuffer[gDataBufferCount];
    Object  mObject;

    vec3   mParticlePositions[MAX_NUM_PARTICLES];
    vec3   mParticleVelocities[MAX_NUM_PARTICLES];
    float  mParticleLifetimes[MAX_NUM_PARTICLES];
    size_t mLifeParticleCount;
} ParticleSystem;

typedef struct Scene
{
    uint32_t       mObjectCount;
    Object         mObjects[128];
    uint32_t       mParticleSystemCount;
    ParticleSystem mParticleSystems[2];
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
    CameraMatrix mViewProject;
} SkyboxUniformBlock;

typedef struct LightUniformBlock
{
    mat4 mLightViewProj;
    vec4 mLightDirection = { -1, -1, -1, 0 };
    vec4 mLightColor = { 1, 0, 0, 1 };
} LightUniformBlock;

typedef struct CameraUniform
{
    CameraMatrix mViewProject;
    mat4         mViewMat;
    vec4         mClipInfo;
    vec4         mPosition;
} CameraUniform;

typedef struct CameraLightUniform
{
    mat4 mViewProject;
    mat4 mViewMat;
    vec4 mClipInfo;
    vec4 mPosition;
} CameraLightUniform;

typedef struct AlphaBlendSettings
{
    bool mSortObjects = true;
    bool mSortParticles = true;
} AlphaBlendSettings;

typedef struct WBOITSettings
{
    float mColorResistance = 1.0f; // Increase if low-coverage foreground transparents are affecting background transparent color.
    float mRangeAdjustment = 0.3f; // Change to avoid saturating at the clamp bounds.
    float mDepthRange = 200.0f; // Decrease if high-opacity surfaces seem �too transparent�, increase if distant transparents are blending
                                // together too much.
    float mOrderingStrength = 4.0f; // Increase if background is showing through foreground too much.
    float mUnderflowLimit = 1e-2f;  // Increase to reduce underflow artifacts.
    float mOverflowLimit = 3e3f;    // Decrease to reduce overflow artifacts.
} WBOITSettings;

typedef struct WBOITVolitionSettings
{
    float mOpacitySensitivity = 3.0f; // Should be greater than 1, so that we only downweight nearly transparent things. Otherwise,
                                      // everything at the same depth should get equal weight. Can be artist controlled
    float mWeightBias = 5.0f; // Must be greater than zero. Weight bias helps prevent distant things from getting hugely lower weight than
                              // near things, as well as preventing floating point underflow
    float mPrecisionScalar = 10000.0f; // Adjusts where the weights fall in the floating point range, used to balance precision to combat
                                       // both underflow and overflow
    float mMaximumWeight = 20.0f; // Don't weight near things more than a certain amount to both combat overflow and reduce the "overpower"
                                  // effect of very near vs. very far things
    float mMaximumColorValue = 1000.0f;
    float mAdditiveSensitivity = 10.0f; // How much we amplify the emissive when deciding whether to consider this additively blended
    float mEmissiveSensitivity = 0.5f;  // Artist controlled value between 0.01 and 1
} WBOITVolitionSettings;

typedef enum WBOITRenderTargets
{
    WBOIT_RT_ACCUMULATION,
    WBOIT_RT_REVEALAGE,
    WBOIT_RT_COUNT
} WBOITRenderTargets;

TinyImageFormat gWBOITRenderTargetFormats[WBOIT_RT_COUNT] = { TinyImageFormat_R16G16B16A16_SFLOAT, TinyImageFormat_R8G8B8A8_UNORM };

typedef enum PTRenderTargets
{
    PT_RT_ACCUMULATION, // Shared with WBOIT
    PT_RT_MODULATION,
#if PT_USE_REFRACTION != 0
    PT_RT_REFRACTION,
#endif
    PT_RT_COUNT
} PTRenderTargets;

TinyImageFormat gPTRenderTargetFormats[3] = { TinyImageFormat_R16G16B16A16_SFLOAT, TinyImageFormat_R8G8B8A8_UNORM,
                                              TinyImageFormat_R16G16_SFLOAT };

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
Shader* pShaderAOITShade = NULL;
Shader* pShaderAOITComposite = NULL;
Shader* pShaderAOITClear = NULL;

/************************************************************************/
// Root signature
/************************************************************************/
RootSignature* pRootSignatureSkybox = NULL;
#if USE_SHADOWS != 0
RootSignature* pRootSignatureGaussianBlur = NULL;
uint32_t       gBlurAxisRootConstantIndex = 0;
#if PT_USE_CAUSTICS != 0
RootSignature* pRootSignaturePTDownsample = NULL;
RootSignature* pRootSignaturePTCopyShadowDepth = NULL;
#endif
#endif
RootSignature* pRootSignature = NULL;
RootSignature* pRootSignatureWBOITComposite = NULL;
RootSignature* pRootSignaturePTComposite = NULL;
#if PT_USE_DIFFUSION != 0
RootSignature* pRootSignaturePTCopyDepth = NULL;
RootSignature* pRootSignaturePTGenMips = NULL;
uint32_t       gMipSizeRootConstantIndex = 0;
#endif
RootSignature* pRootSignatureAOITShade = NULL;
RootSignature* pRootSignatureAOITComposite = NULL;
RootSignature* pRootSignatureAOITClear = NULL;

/************************************************************************/
// Descriptor sets
/************************************************************************/
#define VIEW_CAMERA          0
#define VIEW_SHADOW          1
#define GEOM_OPAQUE          0
#define GEOM_TRANSPARENT     1
#define UNIFORM_SET(f, v, g) (((f)*4) + ((v)*2 + (g)))

#define SHADE_FORWARD        0
#define SHADE_PT             1
#define SHADE_PT_SHADOW      2

DescriptorSet* pDescriptorSetSkybox[2] = { NULL };
DescriptorSet* pDescriptorSetGaussianBlur = { NULL };
DescriptorSet* pDescriptorSetUniforms = { NULL };
DescriptorSet* pDescriptorSetShade = { NULL };
DescriptorSet* pDescriptorSetPTGenMips = { NULL };
DescriptorSet* pDescriptorSetWBOITComposite = { NULL };
DescriptorSet* pDescriptorSetPTCopyDepth = { NULL };
DescriptorSet* pDescriptorSetPTComposite = { NULL };
#if PT_USE_CAUSTICS
DescriptorSet* pDescriptorSetPTCopyShadowDepth = { NULL };
DescriptorSet* pDescriptorSetPTDownsample = { NULL };
#endif
DescriptorSet* pDescriptorSetAOITClear = { NULL };
DescriptorSet* pDescriptorSetAOITShade[2] = { NULL };
DescriptorSet* pDescriptorSetAOITComposite = { NULL };

/************************************************************************/
// Pipelines
/************************************************************************/
Pipeline* pPipelineSkybox = NULL;
Pipeline* pPipelinePTSkybox = NULL;
#if USE_SHADOWS != 0
Pipeline* pPipelineShadow = NULL;
Pipeline* pPipelineGaussianBlur = NULL;
#if PT_USE_CAUSTICS != 0
Pipeline* pPipelinePTGaussianBlur = NULL;
Pipeline* pPipelinePTShadow = NULL;
Pipeline* pPipelinePTDownsample = NULL;
Pipeline* pPipelinePTCopyShadowDepth = NULL;
#endif
#endif
Pipeline* pPipelineForward = NULL;
Pipeline* pPipelinePTForward = NULL;
// With basic Alpha blending we have to render back faces first, then front faces
// to mitigate artifacts and inaccurate results
Pipeline* pPipelineTransparentForwardFront = NULL;
Pipeline* pPipelineTransparentForwardBack = NULL;
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
Pipeline* pPipelineAOITShade = NULL;
Pipeline* pPipelineAOITComposite = NULL;
Pipeline* pPipelineAOITClear = NULL;

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
RenderTarget* pRenderTargetPTShadowFinal[2][3] = { { NULL } };
#endif
#endif
/************************************************************************/
// AOIT Resources
/************************************************************************/
Texture*  pTextureAOITClearMask;
Buffer*   pBufferAOITDepthData;
Buffer*   pBufferAOITColorData;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler*  pSamplerPoint = NULL;
Sampler*  pSamplerPointClamp = NULL;
Sampler*  pSamplerBilinear = NULL;
Sampler*  pSamplerTrilinearAniso = NULL;
Sampler*  pSamplerSkybox = NULL;
Sampler*  pSamplerShadow = NULL; // Only created when USE_SHADOWS != 0
/************************************************************************/
// Resources
/************************************************************************/
Buffer*   pBufferSkyboxVertex = NULL;
Geometry* pMeshes[MESH_COUNT] = {};
Texture*  pTextures[TEXTURE_COUNT] = {};

/************************************************************************/
// Uniform buffers
/************************************************************************/
Buffer* pBufferMaterials[gDataBufferCount] = { NULL };
Buffer* pBufferOpaqueObjectTransforms[gDataBufferCount] = { NULL };
Buffer* pBufferTransparentObjectTransforms[gDataBufferCount] = { NULL };
Buffer* pBufferSkyboxUniform[gDataBufferCount] = { NULL };
Buffer* pBufferLightUniform[gDataBufferCount] = { NULL };
Buffer* pBufferCameraUniform[gDataBufferCount] = { NULL };
Buffer* pBufferCameraLightUniform[gDataBufferCount] = { NULL };
Buffer* pBufferWBOITSettings[gDataBufferCount] = { NULL };

typedef enum TransparencyType
{
    TRANSPARENCY_TYPE_ALPHA_BLEND,
    TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT,
    TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION,
    TRANSPARENCY_TYPE_PHENOMENOLOGICAL,
    TRANSPARENCY_TYPE_ADAPTIVE_OIT,
    TRANSPARENCY_TYPE_COUNT
} TransparencyType;

struct
{
    float3 mLightPosition = { 0, 10, 10 }; // light position, will be changed by GUI editor if not iOS
} gLightCpuSettings;

/************************************************************************/

// Constants
uint32_t gFrameIndex = 0;
float    gCurrentTime = 0.0f;

VertexLayout vertexLayoutDefault = {};

MaterialUniformBlock   gMaterialUniformData;
ObjectInfoUniformBlock gObjectInfoUniformData;
ObjectInfoUniformBlock gTransparentObjectInfoUniformData;
SkyboxUniformBlock     gSkyboxUniformData;
LightUniformBlock      gLightUniformData;
CameraUniform          gCameraUniformData;
CameraLightUniform     gCameraLightUniformData;
AlphaBlendSettings     gAlphaBlendSettings;
WBOITSettings          gWBOITSettingsData;
WBOITVolitionSettings  gWBOITVolitionSettingsData;

Scene    gScene = { 0 };
uint32_t gOpaqueDrawCallCount = 0;
DrawCall gOpaqueDrawCalls[MAX_NUM_OBJECTS * 2 + 1] = {};
uint32_t gTransparentDrawCallCount = 0;
DrawCall gTransparentDrawCalls[MAX_NUM_OBJECTS * 2 + 1] = {};
vec3     gObjectsCenter = { 0, 0, 0 };

ICameraController* pCameraController = NULL;
ICameraController* pLightView = NULL;

/// UI
UIComponent* pGuiWindow = NULL;
FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;
ProfileToken gCurrentGpuProfileTokens[TRANSPARENCY_TYPE_COUNT];
ProfileToken gCurrentGpuProfileToken;
HiresTimer   gCpuTimer;

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

uint32_t gTransparencyType = TRANSPARENCY_TYPE_PHENOMENOLOGICAL;

void AddObject(MeshResource mesh, const vec3& position, const vec4& color, const vec3& translucency = vec3(0.0f), float eta = 1.0f,
               float collimation = 0.0f, const vec3& scale = vec3(1.0f), const vec3& orientation = vec3(0.0f))
{
    ASSERT(gScene.mObjectCount < sizeof(gScene.mObjects) / sizeof(*gScene.mObjects));
    vec4 convert(pow(color.getX(), 2.2f), pow(color.getY(), 2.2f), pow(color.getZ(), 2.2f), color.getW());
    gScene.mObjects[gScene.mObjectCount++] = { position,
                                               scale,
                                               orientation,
                                               mesh,
                                               { v4ToF4(convert), float4(v3ToF3(translucency), 0.0f), eta, collimation } };
}

void AddObject(MeshResource mesh, const vec3& position, TextureResource texture, const vec3& scale = vec3(1.0f),
               const vec3& orientation = vec3(0.0f))
{
    ASSERT(gScene.mObjectCount < sizeof(gScene.mObjects) / sizeof(*gScene.mObjects));
    gScene.mObjects[gScene.mObjectCount++] = { position,
                                               scale,
                                               orientation,
                                               mesh,
                                               { float4(1.0f), float4(0.0f), 1.0f, 0.0f, float2(0.0f), 1, (uint)texture, 0, 0 } };
}

void AddParticleSystem(const vec3& position, const vec4& color, const vec3& translucency = vec3(0.0f), const vec3& scale = vec3(1.0f),
                       const vec3& orientation = vec3(0.0f))
{
    ASSERT(gScene.mParticleSystemCount < sizeof(gScene.mParticleSystems) / sizeof(*gScene.mParticleSystems));
    int particleSystemIndex = gScene.mParticleSystemCount++;
    gScene.mParticleSystems[particleSystemIndex] = ParticleSystem{
        { NULL },
        Object{ position, scale, orientation, MESH_PARTICLE_SYSTEM, { v4ToF4(color), float4(v3ToF3(translucency), 0.0f), 1.0f, 1.0f } },
        {},
        {},
        {},
        0
    };

    BufferLoadDesc particleBufferDesc = {};
    particleBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    particleBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    particleBufferDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    particleBufferDesc.mDesc.mSize = sizeof(ParticleVertex) * 6 * MAX_NUM_PARTICLES;
    for (uint32_t i = 0; i < gDataBufferCount; ++i)
    {
        particleBufferDesc.ppBuffer = &gScene.mParticleSystems[particleSystemIndex].pParticleBuffer[i];
        addResource(&particleBufferDesc, NULL);
    }
}

static void CreateScene()
{
    // Set plane
    AddObject(MESH_CUBE, vec3(0.0f), vec4(0.9f, 0.9f, 0.9f, 1.0f), vec3(0.0f), 1.0f, 1.0f, vec3(100.0f, 0.5f, 100.0f));

    // Set cubes
    const float cubeDist = 3.0f;
    vec3        curTrans = { -cubeDist * (CUBES_EACH_ROW - 1) / 2.f, 2.3f, -cubeDist * (CUBES_EACH_COL - 1) / 2.f };

    for (int i = 0; i < CUBES_EACH_ROW; ++i)
    {
        curTrans.setX(-cubeDist * (CUBES_EACH_ROW - 1) / 2.f);

        for (int j = 0; j < CUBES_EACH_COL; j++)
        {
            AddObject(MESH_CUBE, curTrans,
                      vec4(float(i + 1) / CUBES_EACH_ROW, 1.0f - float(i + 1) / CUBES_EACH_ROW, 0.0f, float(j + 1) / CUBES_EACH_COL),
                      vec3(0.0f), 1.0f, 1.0f, vec3(1.0f));
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
        AddObject(MESH_CUBE, vec3(i * 2.0f - 25.0f, 4.0f, 25.0f), vec4(1.0f, 1.0f, 10.0f, 0.1f), vec3(0.0f), 1.0f, 1.0f,
                  vec3(0.1f, 4.0f, 4.0f));

    AddObject(MESH_CUBE, vec3(1.0f, 5.0f, -22.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(0.5f, 0.5f, 0.01f));
    AddObject(MESH_CUBE, vec3(-1.0f, 5.0f, -35.0f), vec4(0.0f, 1.0f, 0.0f, 1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(1.0f, 1.0f, 0.005f));
    AddObject(MESH_SPHERE, vec3(0.0f, 5.0f, -25.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.5f, 0.0f, vec3(4.0f));

    AddObject(MESH_LION, vec3(10.0f, 0.0f, -25.0f), vec4(1.0f), vec3(0.0f), 1.0f, 0.0f, vec3(0.25f), vec3(0.0f, PI, 0.0f));
    AddObject(MESH_CUBE, vec3(7.0f, 5.0f, -22.0f), vec4(1.0f, 0.3f, 0.3f, 0.9f), vec3(1.0f, 0.3f, 0.3f), 1.0f, 0.0f,
              vec3(1.5f, 4.0f, 0.005f));
    AddObject(MESH_CUBE, vec3(10.0f, 5.0f, -22.0f), vec4(0.3f, 1.0f, 0.3f, 0.9f), vec3(0.3f, 1.0f, 0.3f), 1.0f, 0.5f,
              vec3(1.5f, 4.0f, 0.005f));
    AddObject(MESH_CUBE, vec3(13.0f, 5.0f, -22.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.0f, 0.9f,
              vec3(1.5f, 4.0f, 0.005f));

    AddParticleSystem(vec3(30.0f, 5.0f, 20.0f), vec4(1.0f, 0.0f, 0.0f, 0.5f));
    AddParticleSystem(vec3(30.0f, 5.0f, 25.0f), vec4(1.0f, 1.0f, 0.0f, 0.5f));

    AddObject(MESH_PLANE, vec3(-15.0f - 5.0f, 10.0f, -25.0f), TEXTURE_MEASURING_GRID, vec3(10.0f, 1.0f, 10.0f),
              vec3(-90.0f * (PI / 180.0f), PI, 0.0f));
    AddObject(MESH_SPHERE, vec3(-17.5f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.001f, 1.0f,
              vec3(1.0f));
    AddObject(MESH_SPHERE, vec3(-15.0f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.3f, 1.0f, vec3(1.0f));
    AddObject(MESH_SPHERE, vec3(-12.5f - 5.0f, 5.0f, -20.0f), vec4(0.3f, 0.3f, 1.0f, 0.9f), vec3(0.3f, 0.3f, 1.0f), 1.5f, 1.0f, vec3(1.0f));
}

int DistanceCompare(const float3& a, const float3& b)
{
    float dx = a.x - b.x;
    if (dx == 0)
    {
        float dy = a.y - b.y;
        if (dy == 0)
        {
            float dz = a.z - b.z;
            if (dz == 0)
                return 0;
            dx = dz;
        }
        else
        {
            dx = dy;
        }
    }

    if (dx < 0)
        return -1;
    else
        return 1;
}

int MeshCompare(float2 a, float2 b)
{
    float dx = a.x - b.x;
    if (dx == 0)
    {
        float dy = a.y - b.y;
        if (dy == 0)
            return 0;
        dx = dy;
    }

    if (dx < 0)
        return -1;
    else
        return 1;
}

int DistanceCompare_float3(const void* a, const void* b) { return DistanceCompare(*(const float3*)a, *(const float3*)b); }

int MeshCompare_float2(const void* a, const void* b) { return MeshCompare(*(const float2*)a, *(const float2*)b); }

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
    static void RemoveGui();
    static void UpdateDynamicUI();

    static DynamicUIWidgets alphaBlendDynamicWidgets;
    static DynamicUIWidgets weightedBlendedOitDynamicWidgets;
    static DynamicUIWidgets weightedBlendedOitVolitionDynamicWidgets;
    static DynamicUIWidgets AOITNotSupportedDynamicWidgets;

    static TransparencyType currentTransparencyType;
};
DynamicUIWidgets GuiController::alphaBlendDynamicWidgets;
DynamicUIWidgets GuiController::weightedBlendedOitDynamicWidgets;
DynamicUIWidgets GuiController::weightedBlendedOitVolitionDynamicWidgets;
DynamicUIWidgets GuiController::AOITNotSupportedDynamicWidgets;
TransparencyType GuiController::currentTransparencyType;

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

const char* gTestScripts[] = { "Test_AlphaBlend.lua", "Test_WeightedBlendedOIT.lua", "Test_WeightedBlendedOITVolition.lua",
                               "Test_Phenomenological.lua", "Test_AdaptiveOIT.lua" };
uint32_t    gCurrentScriptIndex = 0;
void        RunScript(void* pUserData)
{
    UNREF_PARAM(pUserData);
    LuaScriptDesc runDesc = {};
    runDesc.pScriptFileName = gTestScripts[gCurrentScriptIndex];
    luaQueueScriptToRun(&runDesc);
}

class Transparency: public IApp
{
public:
    bool Init() override
    {
        initHiresTimer(&gCpuTimer);

        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_MESHES, "Meshes");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        INIT_STRUCT(gGpuSettings);

        ExtendedSettings extendedSettings = {};
        extendedSettings.mNumSettings = ESettings::Count;
        extendedSettings.pSettings = (uint32_t*)&gGpuSettings;
        extendedSettings.ppSettingNames = gSettingNames;

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.pExtendedSettings = &extendedSettings;
        initRenderer(GetName(), &settings, &pRenderer);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;

        addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        addSemaphore(pRenderer, &pImageAcquiredSemaphore);

        initResourceLoaderInterface(pRenderer);

        addModels();

        addSamplers();
        addResources();
        addUniformBuffers();

        CreateScene();

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        if (!initFontSystem(&fontRenderDesc))
            return false; // report?

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        const uint32_t numScripts = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
        LuaScriptDesc  scriptDescs[numScripts] = {};
        for (uint32_t i = 0; i < numScripts; ++i)
            scriptDescs[i].pScriptFileName = gTestScripts[i];
        luaDefineScripts(scriptDescs, numScripts);

        /************************************************************************/
        // Add GPU profiler
        /************************************************************************/

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        for (uint32_t i = 0; i < TRANSPARENCY_TYPE_COUNT; ++i)
            gCurrentGpuProfileTokens[i] = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gCurrentGpuProfileToken = gCurrentGpuProfileTokens[gTransparencyType];

        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.25f);

        uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

        GuiController::AddGui();

        CameraMotionParameters cmp{ 16.0f, 60.0f, 20.0f };
        vec3                   camPos{ -40, 17, 34 };
        vec3                   lookAt{ 0, 5, 0 };

        pLightView = initGuiCameraController(camPos, lookAt);
        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(cmp);

        InputSystemDesc inputDesc = {};
        inputDesc.pRenderer = pRenderer;
        inputDesc.pWindow = pWindow;
        inputDesc.pJoystickTexture = "circlepad.tex";
        if (!initInputSystem(&inputDesc))
            return false;

        // App Actions
        InputActionDesc actionDesc = { DefaultInputActions::DUMP_PROFILE_DATA,
                                       [](InputActionContext* ctx)
                                       {
                                           dumpProfileData(((Renderer*)ctx->pUserData)->pName);
                                           return true;
                                       },
                                       pRenderer };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TOGGLE_FULLSCREEN,
                       [](InputActionContext* ctx)
                       {
                           WindowDesc* winDesc = ((IApp*)ctx->pUserData)->pWindow;
                           if (winDesc->fullScreen)
                               winDesc->borderlessWindow
                                   ? setBorderless(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect))
                                   : setWindowed(winDesc, getRectWidth(&winDesc->clientRect), getRectHeight(&winDesc->clientRect));
                           else
                               setFullscreen(winDesc);
                           return true;
                       },
                       this };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::EXIT, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           requestShutdown();
                           return true;
                       } };
        addInputAction(&actionDesc);
        InputActionCallback onUIInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }
            return true;
        };

        typedef bool (*CameraInputHandler)(InputActionContext * ctx, DefaultInputActions::DefaultInputAction action);
        static CameraInputHandler onCameraInput = [](InputActionContext* ctx, DefaultInputActions::DefaultInputAction action)
        {
            if (*(ctx->pCaptured))
            {
                float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
                switch (action)
                {
                case DefaultInputActions::ROTATE_CAMERA:
                    pCameraController->onRotate(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA:
                    pCameraController->onMove(delta);
                    break;
                case DefaultInputActions::TRANSLATE_CAMERA_VERTICAL:
                    pCameraController->onMoveY(delta[0]);
                    break;
                default:
                    break;
                }
            }
            return true;
        };
        actionDesc = { DefaultInputActions::CAPTURE_INPUT,
                       [](InputActionContext* ctx)
                       {
                           setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);
                           return true;
                       },
                       NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::ROTATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::TRANSLATE_CAMERA_VERTICAL,
                       [](InputActionContext* ctx) { return onCameraInput(ctx, DefaultInputActions::TRANSLATE_CAMERA_VERTICAL); }, NULL };
        addInputAction(&actionDesc);
        actionDesc = { DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx)
                       {
                           UNREF_PARAM(ctx);
                           if (!uiWantTextInput())
                               pCameraController->resetView();
                           return true;
                       } };
        addInputAction(&actionDesc);
        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onUIInput, this };
        setGlobalInputAction(&globalInputActionDesc);

        gFrameIndex = 0;

        return true;
    }

    void Exit() override
    {
        exitInputSystem();
        exitCameraController(pCameraController);
        exitCameraController(pLightView);

        GuiController::RemoveGui();

        for (uint32_t i = 0; i < TRANSPARENCY_TYPE_COUNT; ++i)
            removeGpuProfiler(gCurrentGpuProfileTokens[i]);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        for (size_t i = 0; i < gScene.mParticleSystemCount; ++i)
            for (size_t j = 0; j < gDataBufferCount; ++j)
                removeResource(gScene.mParticleSystems[i].pParticleBuffer[j]);

        removeSamplers();
        removeResources();
        removeUniformBuffers();

        removeSemaphore(pRenderer, pImageAcquiredSemaphore);
        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
        exitRenderer(pRenderer);
        pRenderer = NULL;

        gScene.mParticleSystemCount = 0;
        gScene.mObjectCount = 0;
        gOpaqueDrawCallCount = 0;
        gTransparentDrawCallCount = 0;
    }

    bool Load(ReloadDesc* pReloadDesc) override
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
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc) override
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
            removeRenderTargets();
            removeSwapChain(pRenderer, pSwapChain);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime) override
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        resetHiresTimer(&gCpuTimer);

        gCurrentTime += deltaTime;

        // Dynamic UI elements
        GuiController::UpdateDynamicUI();
        /************************************************************************/
        // Camera Update
        /************************************************************************/
        const float zNear = 1.0f;
        const float zFar = 4000.0f;
        pCameraController->update(deltaTime);
        mat4         viewMat = pCameraController->getViewMatrix();
        const float  aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float  horizontal_fov = PI / 2.0f;
        CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, zNear, zFar); // view matrix
        vec3         camPos = pCameraController->getViewPosition();
        CameraMatrix vpMatrix = projMat * viewMat;
        /************************************************************************/
        // Light Update
        /************************************************************************/
        const float  lightZNear = -100.0f;
        const float  lightZFar = 100.0f;
        vec3 lightPos = vec3(gLightCpuSettings.mLightPosition.x, gLightCpuSettings.mLightPosition.y, gLightCpuSettings.mLightPosition.z);
        vec3 lightDir = normalize(gObjectsCenter - lightPos);
        pLightView->moveTo(lightDir * lightZNear);
        pLightView->lookAt(gObjectsCenter);
        mat4 lightViewMat = pLightView->getViewMatrix();
        mat4 lightProjMat = mat4::orthographicLH(-50.0f, 50.0f, -50.0f, 50.0f, 0.0f, lightZFar - lightZNear);
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
    }

    void UpdateParticleSystems(float deltaTime, const mat4& viewMat, const vec3& camPos)
    {
        const float particleSize = 0.2f;
        const vec3  camRight = vec3((float)viewMat[0][0], viewMat[1][0], viewMat[2][0]) * particleSize;
        const vec3  camUp = vec3((float)viewMat[0][1], viewMat[1][1], viewMat[2][1]) * particleSize;

        for (size_t i = 0; i < gScene.mParticleSystemCount; ++i)
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
                pParticleSystem->mParticleVelocities[pi] =
                    normalize(vec3(sin(gCurrentTime + pi) * 0.97f, cos(gCurrentTime * gCurrentTime + pi), sin(gCurrentTime * pi)) *
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

            // After particle system update we want to query and update particles vertex data...
            // This is done after update to sync vertex data with newly added particles...
            BufferUpdateDesc particleBufferUpdateDesc = { pParticleSystem->pParticleBuffer[gFrameIndex], 0u };
            particleBufferUpdateDesc.mSize = sizeof(ParticleVertex) * 6 * pParticleSystem->mLifeParticleCount;
            beginUpdateResource(&particleBufferUpdateDesc);
            ParticleVertex* particleVertexData = (ParticleVertex*)particleBufferUpdateDesc.pMappedData;

            // Update vertex buffers
            static uint packedNorm = packFloat3DirectionToHalf2(float3(0.0f, 1.0f, 0.0f));
            static uint packedUV0 = packFloat2ToHalf2(float2(0.0f, 0.0f));
            static uint packedUV1 = packFloat2ToHalf2(float2(0.0f, 1.0f));
            static uint packedUV2 = packFloat2ToHalf2(float2(1.0f, 0.0f));
            static uint packedUV3 = packFloat2ToHalf2(float2(1.0f, 1.0f));
            static uint packedUV4 = packFloat2ToHalf2(float2(1.0f, 0.0f));
            static uint packedUV5 = packFloat2ToHalf2(float2(0.0f, 1.0f));

            if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortParticles)
            {
                float2* sortedArray = (float2*)tf_malloc(pParticleSystem->mLifeParticleCount * sizeof(float2));

                for (size_t j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
                    sortedArray[j] = { (float)distSqr(Point3(camPos), Point3(pParticleSystem->mParticlePositions[j])), (float)j };

                qsort(sortedArray, pParticleSystem->mLifeParticleCount, sizeof(float2), MeshCompare_float2);

                for (uint j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
                {
                    vec3 pos = pParticleSystem->mParticlePositions[(int)sortedArray[pParticleSystem->mLifeParticleCount - j - 1][1]];
                    particleVertexData[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), packedNorm, packedUV0 };
                    particleVertexData[j * 6 + 1] = { v3ToF3(pos + camUp - camRight), packedNorm, packedUV1 };
                    particleVertexData[j * 6 + 2] = { v3ToF3(pos - camUp + camRight), packedNorm, packedUV2 };
                    particleVertexData[j * 6 + 3] = { v3ToF3(pos + camUp + camRight), packedNorm, packedUV3 };
                    particleVertexData[j * 6 + 4] = { v3ToF3(pos - camUp + camRight), packedNorm, packedUV4 };
                    particleVertexData[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), packedNorm, packedUV5 };
                }

                tf_free(sortedArray);
            }
            else
            {
                for (uint j = 0; j < pParticleSystem->mLifeParticleCount; ++j)
                {
                    vec3 pos = pParticleSystem->mParticlePositions[j];
                    particleVertexData[j * 6 + 0] = { v3ToF3(pos - camUp - camRight), packedNorm, packedUV0 };
                    particleVertexData[j * 6 + 1] = { v3ToF3(pos + camUp - camRight), packedNorm, packedUV1 };
                    particleVertexData[j * 6 + 2] = { v3ToF3(pos - camUp + camRight), packedNorm, packedUV2 };
                    particleVertexData[j * 6 + 3] = { v3ToF3(pos + camUp + camRight), packedNorm, packedUV3 };
                    particleVertexData[j * 6 + 4] = { v3ToF3(pos - camUp + camRight), packedNorm, packedUV4 };
                    particleVertexData[j * 6 + 5] = { v3ToF3(pos + camUp - camRight), packedNorm, packedUV5 };
                }
            }

            endUpdateResource(&particleBufferUpdateDesc);
        }
    }

    void CreateDrawCalls(float* pSortedObjects, uint objectCount, uint sizeOfObject, ObjectInfoUniformBlock* pObjectUniformBlock,
                         MaterialUniformBlock* pMaterialUniformBlock, uint* pMaterialCount, DrawCall* pDrawCalls, uint32_t* drawCallCount)
    {
        const uint meshIndexOffset = sizeOfObject - 2;
        const uint objectIndexOffset = sizeOfObject - 1;

        uint         instanceCount = 0;
        uint         instanceOffset = 0;
        MeshResource prevMesh = (MeshResource)-1;
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
                    pDrawCalls[(*drawCallCount)++] = { 0, instanceCount - 1, instanceOffset, prevMesh };
                    instanceOffset += instanceCount - 1;
                    instanceCount = 1;
                }

                pDrawCalls[(*drawCallCount)++] = { (uint)index, instanceCount, instanceOffset, MESH_PARTICLE_SYSTEM };
                instanceOffset += instanceCount;
                instanceCount = 0;
            }
            else if (mesh != prevMesh && instanceCount > 1)
            {
                pDrawCalls[(*drawCallCount)++] = { 0, instanceCount - 1, instanceOffset, prevMesh };
                instanceOffset += instanceCount - 1;
                instanceCount = 1;
            }

            prevMesh = mesh;
        }

        if (instanceCount > 0)
            pDrawCalls[(*drawCallCount)++] = { 0, instanceCount, instanceOffset, prevMesh };
    }

    void UpdateScene(float deltaTime, const mat4& viewMat, const vec3& camPos)
    {
        uint materialCount = 0;

        UpdateParticleSystems(deltaTime, viewMat, camPos);

        // Create list of opaque objects
        gOpaqueDrawCallCount = 0;
        {
            uint32_t sortedCount = 0;
            float2   sortedArray[MAX_NUM_OBJECTS];

            for (size_t i = 0; i < gScene.mObjectCount; ++i)
            {
                const Object* pObj = &gScene.mObjects[i];
                if (pObj->mMaterial.mColor.getW() == 1.0f)
                {
                    sortedArray[sortedCount++] = { (float)pObj->mMesh, (float)i };
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                }
            }
            for (size_t i = 0; i < gScene.mParticleSystemCount; ++i)
            {
                const Object* pObj = &gScene.mParticleSystems[i].mObject;
                if (pObj->mMaterial.mColor.getW() == 1.0f)
                {
                    sortedArray[sortedCount++] = { (float)pObj->mMesh, (float)i };
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                }
            }

            qsort(sortedArray, sortedCount, sizeof(float2), MeshCompare_float2); // Sorts by mesh

            CreateDrawCalls(&sortedArray[0].x, sortedCount, sizeof(sortedArray[0]) / sizeof(float), &gObjectInfoUniformData,
                            &gMaterialUniformData, &materialCount, gOpaqueDrawCalls, &gOpaqueDrawCallCount);
        }

        // Create list of transparent objects
        gTransparentDrawCallCount = 0;
        if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND && gAlphaBlendSettings.mSortObjects)
        {
            uint32_t sortedCount = 0;
            float3   sortedArray[MAX_NUM_OBJECTS];

            for (size_t i = 0; i < gScene.mObjectCount; ++i)
            {
                const Object* pObj = &gScene.mObjects[i];
                if (pObj->mMaterial.mColor.getW() < 1.0f)
                {
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                    sortedArray[sortedCount++] = { (float)distSqr(Point3(camPos), Point3(pObj->mPosition)) -
                                                       (float)pow(maxElem(pObj->mScale), 2),
                                                   (float)pObj->mMesh, (float)i };
                }
            }
            for (size_t i = 0; i < gScene.mParticleSystemCount; ++i)
            {
                const Object* pObj = &gScene.mParticleSystems[i].mObject;
                if (pObj->mMaterial.mColor.getW() < 1.0f)
                {
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                    sortedArray[sortedCount++] = { (float)distSqr(Point3(camPos), Point3(pObj->mPosition)) -
                                                       (float)pow(maxElem(pObj->mScale), 2),
                                                   (float)pObj->mMesh, (float)i };
                }
            }

            qsort(sortedArray, sortedCount, sizeof(float3), DistanceCompare_float3); // Sorts by distance first, then by mesh

            CreateDrawCalls(&sortedArray[0].x, sortedCount, sizeof(sortedArray[0]) / sizeof(float), &gTransparentObjectInfoUniformData,
                            &gMaterialUniformData, &materialCount, gTransparentDrawCalls, &gTransparentDrawCallCount);
        }
        else
        {
            uint32_t sortedCount = 0;
            float2   sortedArray[MAX_NUM_OBJECTS];

            for (size_t i = 0; i < gScene.mObjectCount; ++i)
            {
                const Object* pObj = &gScene.mObjects[i];
                if (pObj->mMaterial.mColor.getW() < 1.0f)
                {
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                    sortedArray[sortedCount++] = { (float)pObj->mMesh, (float)i };
                }
            }
            for (size_t i = 0; i < gScene.mParticleSystemCount; ++i)
            {
                const Object* pObj = &gScene.mParticleSystems[i].mObject;
                if (pObj->mMaterial.mColor.getW() < 1.0f)
                {
                    ASSERT(sortedCount < MAX_NUM_OBJECTS);
                    sortedArray[sortedCount++] = { (float)pObj->mMesh, (float)i };
                }
            }

            qsort(sortedArray, sortedCount, sizeof(float2), MeshCompare_float2); // Sorts by mesh

            CreateDrawCalls(&sortedArray[0].x, sortedCount, sizeof(sortedArray[0]) / sizeof(float), &gTransparentObjectInfoUniformData,
                            &gMaterialUniformData, &materialCount, gTransparentDrawCalls, &gTransparentDrawCallCount);
        }
    }

    void DrawSkybox(Cmd* pCmd)
    {
        RenderTarget* rt = pRenderTargetScreen;
        if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
        {
            rt = pRenderTargetPTBackground;
            RenderTargetBarrier barrier = { rt, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &barrier);
        }

        cmdBeginDebugMarker(pCmd, 0, 0, 1, "Draw skybox");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Draw Skybox");

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { rt, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);

        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)rt->mWidth, (float)rt->mHeight, 1.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, rt->mWidth, rt->mHeight);

        const uint32_t skyboxStride = sizeof(float) * 4;
        if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
        {
            cmdBindPipeline(pCmd, pPipelinePTSkybox);
        }
        else
        {
            cmdBindPipeline(pCmd, pPipelineSkybox);
        }

        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetSkybox[0]);
        cmdBindDescriptorSet(pCmd, gFrameIndex, pDescriptorSetSkybox[1]);
        cmdBindVertexBuffer(pCmd, 1, &pBufferSkyboxVertex, &skyboxStride, NULL);
        cmdDraw(pCmd, 36, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)rt->mWidth, (float)rt->mHeight, 0.0f, 1.0f);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void ShadowPass(Cmd* pCmd)
    {
#if USE_SHADOWS != 0
        RenderTargetBarrier barriers[2] = {};
        barriers[0].pRenderTarget = pRenderTargetShadowVariance[0];
        barriers[0].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].mNewState = RESOURCE_STATE_RENDER_TARGET;
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers);

        // Draw the opaque objects.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw shadow map");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render shadow map");

        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetShadowVariance[0], LOAD_ACTION_CLEAR };
        bindRenderTargets.mDepthStencil = { pRenderTargetShadowDepth, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetShadowVariance[0]->mWidth, (float)pRenderTargetShadowVariance[0]->mHeight,
                       0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetShadowVariance[0]->mWidth, pRenderTargetShadowVariance[0]->mHeight);

        cmdBindPipeline(pCmd, pPipelineShadow);
        cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_SHADOW, GEOM_OPAQUE), pDescriptorSetUniforms);
        DrawObjects(pCmd, gOpaqueDrawCallCount, gOpaqueDrawCalls, pRootSignature);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        // Blur shadow map
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Blur shadow map0");
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Blur shadow map");

        for (uint32_t i = 0; i < 1; ++i)
        {
            float axis = 0.0f;

            cmdBindRenderTargets(pCmd, NULL);

            barriers[0].pRenderTarget = pRenderTargetShadowVariance[0];
            barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].pRenderTarget = pRenderTargetShadowVariance[1];
            barriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTargetShadowVariance[1], LOAD_ACTION_CLEAR };
            cmdBindRenderTargets(pCmd, &bindRenderTargets);

            cmdBindPipeline(pCmd, pPipelineGaussianBlur);
            cmdBindPushConstants(pCmd, pRootSignatureGaussianBlur, gBlurAxisRootConstantIndex, &axis);
            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetGaussianBlur);
            cmdDraw(pCmd, 3, 0);

            cmdBindRenderTargets(pCmd, NULL);

            cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
            cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Blur shadow map1");

            barriers[0].pRenderTarget = pRenderTargetShadowVariance[1];
            barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].pRenderTarget = pRenderTargetShadowVariance[0];
            barriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = { pRenderTargetShadowVariance[0], LOAD_ACTION_CLEAR };
            cmdBindRenderTargets(pCmd, &bindRenderTargets);
            cmdBindPipeline(pCmd, pPipelineGaussianBlur);

            axis = 1.0f;
            cmdBindPushConstants(pCmd, pRootSignatureGaussianBlur, gBlurAxisRootConstantIndex, &axis);
            cmdBindDescriptorSet(pCmd, 1, pDescriptorSetGaussianBlur);
            cmdDraw(pCmd, 3, 0);
        }

        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        barriers[0].pRenderTarget = pRenderTargetShadowVariance[0];
        barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
        barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers);
#endif
    }

    void StochasticShadowPass(Cmd* pCmd)
    {
        UNREF_PARAM(pCmd);
#if PT_USE_CAUSTICS != 0
        RenderTargetBarrier barriers[3] = {};
        for (uint32_t i = 0; i < 3; ++i)
        {
            barriers[i].pRenderTarget = pRenderTargetPTShadowVariance[i];
            barriers[i].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 3, barriers);

        LoadActionsDesc loadActions = {};
        for (uint32_t i = 0; i < 3; ++i)
        {
            loadActions.mLoadActionsColor[i] = LOAD_ACTION_CLEAR;
            loadActions.mClearColorValues[i] = pRenderTargetPTShadowVariance[i]->mClearValue;
        }
        loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

        // Copy depth buffer to shadow maps
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render stochastic shadow map", true);
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Copy shadow map");

        for (uint32_t w = 0; w < 3; ++w)
        {
            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = pRenderTargetPTShadowVariance[w];
            bindRenderTargets.pLoadActions = &loadActions;
            cmdBindRenderTargets(pCmd, &bindRenderTargets);
            cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowVariance[0]->mWidth,
                           (float)pRenderTargetPTShadowVariance[0]->mHeight, 0.0f, 1.0f);
            cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowVariance[0]->mWidth, pRenderTargetPTShadowVariance[0]->mHeight);

            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPTCopyShadowDepth);
            cmdBindPipeline(pCmd, pPipelinePTCopyShadowDepth);

            cmdDraw(pCmd, 3, 0);
        }
        cmdEndDebugMarker(pCmd);

        // Start render pass and apply load actions
        for (int i = 0; i < 3; ++i)
            loadActions.mLoadActionsColor[i] = LOAD_ACTION_LOAD;
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 3;
        bindRenderTargets.ppRenderTargets = pRenderTargetPTShadowVariance;
        bindRenderTargets.pLoadActions = &loadActions;
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowVariance[0]->mWidth, (float)pRenderTargetPTShadowVariance[0]->mHeight,
                       0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowVariance[0]->mWidth, pRenderTargetPTShadowVariance[0]->mHeight);

        // Draw the opaque objects.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw stochastic shadow map");

        cmdBindPipeline(pCmd, pPipelinePTShadow);
        cmdBindDescriptorSet(pCmd, SHADE_PT_SHADOW, pDescriptorSetShade);
        cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_SHADOW, GEOM_TRANSPARENT), pDescriptorSetUniforms);
        DrawObjects(pCmd, gTransparentDrawCallCount, gTransparentDrawCalls, pRootSignature);
        cmdEndDebugMarker(pCmd);

        // Downsample shadow map
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Downsample shadow map");
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
        loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

        for (uint32_t w = 0; w < 3; ++w)
        {
            cmdBindRenderTargets(pCmd, NULL);

            barriers[0].pRenderTarget = pRenderTargetPTShadowVariance[w];
            barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].pRenderTarget = pRenderTargetPTShadowFinal[0][w];
            barriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = pRenderTargetPTShadowFinal[0][w];
            bindRenderTargets.pLoadActions = &loadActions;
            cmdBindRenderTargets(pCmd, &bindRenderTargets);
            cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPTShadowFinal[0][w]->mWidth,
                           (float)pRenderTargetPTShadowFinal[0][w]->mHeight, 0.0f, 1.0f);
            cmdSetScissor(pCmd, 0, 0, pRenderTargetPTShadowFinal[0][w]->mWidth, pRenderTargetPTShadowFinal[0][w]->mHeight);

            cmdBindPipeline(pCmd, pPipelinePTDownsample);
            cmdBindDescriptorSet(pCmd, w, pDescriptorSetPTDownsample);
            cmdDraw(pCmd, 3, 0);
        }
        cmdEndDebugMarker(pCmd);

        // Blur shadow map
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Blur shadow map");
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_DONTCARE;
        loadActions.mLoadActionDepth = LOAD_ACTION_DONTCARE;

        uint32_t rootConstantIndex = getDescriptorIndexFromName(pRootSignatureGaussianBlur, "RootConstant");

        for (uint32_t w = 0; w < 3; ++w)
        {
            float axis = 0.0f;

            cmdBindRenderTargets(pCmd, NULL);

            barriers[0].pRenderTarget = pRenderTargetPTShadowFinal[0][w];
            barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].pRenderTarget = pRenderTargetPTShadowFinal[1][w];
            barriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = pRenderTargetPTShadowFinal[1][w];
            bindRenderTargets.pLoadActions = &loadActions;
            cmdBindRenderTargets(pCmd, &bindRenderTargets);

            cmdBindPipeline(pCmd, pPipelinePTGaussianBlur);
            cmdBindPushConstants(pCmd, pRootSignatureGaussianBlur, gBlurAxisRootConstantIndex, &axis);
            cmdBindDescriptorSet(pCmd, 2 + (w * 2 + 0), pDescriptorSetGaussianBlur);
            cmdDraw(pCmd, 3, 0);

            cmdBindRenderTargets(pCmd, NULL);

            barriers[0].pRenderTarget = pRenderTargetPTShadowFinal[1][w];
            barriers[0].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].pRenderTarget = pRenderTargetPTShadowFinal[0][w];
            barriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 1;
            bindRenderTargets.mRenderTargets[0] = pRenderTargetPTShadowFinal[0][w];
            bindRenderTargets.pLoadActions = &loadActions;
            cmdBindRenderTargets(pCmd, &bindRenderTargets);
            cmdBindPipeline(pCmd, pPipelinePTGaussianBlur);

            axis = 1.0f;
            cmdBindPushConstants(pCmd, pRootSignatureGaussianBlur, gBlurAxisRootConstantIndex, &axis);
            cmdBindDescriptorSet(pCmd, 2 + (w * 2 + 1), pDescriptorSetGaussianBlur);
            cmdDraw(pCmd, 3, 0);
        }

        cmdBindRenderTargets(pCmd, NULL);

        for (uint32_t w = 0; w < 3; ++w)
        {
            barriers[w].pRenderTarget = pRenderTargetPTShadowFinal[0][w];
            barriers[w].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            barriers[w].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 3, barriers);

        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
#endif
    }

    void DrawObjects(Cmd* pCmd, uint32_t drawCallCount, const DrawCall* pDrawCalls, RootSignature* pRootSignature_)
    {
        static MeshResource boundMesh = (MeshResource)-1;
        static uint         vertexCount = 0;
        static uint         indexCount = 0;

        uint32_t rootConstantIndex = getDescriptorIndexFromName(pRootSignature_, "DrawInfoRootConstant");

        for (size_t i = 0; i < drawCallCount; ++i)
        {
            const DrawCall* dc = &pDrawCalls[i];
            cmdBindPushConstants(pCmd, pRootSignature_, rootConstantIndex, &dc->mInstanceOffset);

            if (dc->mMesh != boundMesh || dc->mMesh > MESH_COUNT)
            {
                if (dc->mMesh == MESH_PARTICLE_SYSTEM)
                {
                    const uint32_t stride = sizeof(ParticleVertex);
                    cmdBindVertexBuffer(pCmd, 1, &gScene.mParticleSystems[dc->mIndex].pParticleBuffer[gFrameIndex], &stride, NULL);
                    vertexCount = (uint)gScene.mParticleSystems[dc->mIndex].mLifeParticleCount * 6;
                    indexCount = 0;
                    boundMesh = MESH_PARTICLE_SYSTEM;
                }
                else
                {
                    cmdBindVertexBuffer(pCmd, 1, &pMeshes[dc->mMesh]->pVertexBuffers[0], &pMeshes[dc->mMesh]->mVertexStrides[0], NULL);
                    if (pMeshes[dc->mMesh]->pIndexBuffer)
                        cmdBindIndexBuffer(pCmd, pMeshes[dc->mMesh]->pIndexBuffer, pMeshes[dc->mMesh]->mIndexType, 0);
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

        // Draw the opaque objects.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw opaque geometry");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render opaque geometry");

        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { rt, LOAD_ACTION_LOAD };
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)rt->mWidth, (float)rt->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, rt->mWidth, rt->mHeight);

        if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
        {
            cmdBindPipeline(pCmd, pPipelinePTForward);
        }
        else
        {
            cmdBindPipeline(pCmd, pPipelineForward);
        }

        cmdBindDescriptorSet(pCmd, SHADE_FORWARD, pDescriptorSetShade);
        cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_CAMERA, GEOM_OPAQUE), pDescriptorSetUniforms);
        DrawObjects(pCmd, gOpaqueDrawCallCount, gOpaqueDrawCalls, pRootSignature);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);

        //        cmdBeginGpuFrameProfile(pCmd, gCurrentGpuProfileToken);

#if PT_USE_DIFFUSION != 0
        if (gTransparencyType == TRANSPARENCY_TYPE_PHENOMENOLOGICAL)
        {
            cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "PT Gen Mips");
            RenderTargetBarrier barrier = { rt, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_UNORDERED_ACCESS };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &barrier);

            uint32_t mipSizeX = 1 << (uint32_t)ceil(log2((float)rt->mWidth));
            uint32_t mipSizeY = 1 << (uint32_t)ceil(log2((float)rt->mHeight));

            cmdBindPipeline(pCmd, pPipelinePTGenMips);
            for (uint32_t i = 1; i < rt->mMipLevels; ++i)
            {
                mipSizeX >>= 1;
                mipSizeY >>= 1;
                uint mipSize[2] = { mipSizeX, mipSizeY };
                cmdBindPushConstants(pCmd, pRootSignaturePTGenMips, gMipSizeRootConstantIndex, mipSize);
                cmdBindDescriptorSet(pCmd, i - 1, pDescriptorSetPTGenMips);

                uint32_t groupCountX = mipSizeX / 16;
                uint32_t groupCountY = mipSizeY / 16;
                if (groupCountX == 0)
                    groupCountX = 1;
                if (groupCountY == 0)
                    groupCountY = 1;
                cmdDispatch(pCmd, groupCountX, groupCountY, pRenderTargetScreen->mArraySize);
                barrier = { rt, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                barrier.mSubresourceBarrier = true;
                barrier.mMipLevel = i;
                cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &barrier);
            }

            barrier = { rt, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &barrier);
            cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        }
#endif

        //        cmdEndGpuFrameProfile(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void AlphaBlendTransparentPass(Cmd* pCmd)
    {
        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render transparent geometry");

        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetScreen, LOAD_ACTION_LOAD };
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mWidth, (float)pRenderTargetScreen->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mWidth, pRenderTargetScreen->mHeight);

        Pipeline* pipelines[2] = { pPipelineTransparentForwardBack, pPipelineTransparentForwardFront };
        for (uint32_t i = 0; i < TF_ARRAY_COUNT(pipelines); ++i)
        {
            cmdBindPipeline(pCmd, pipelines[i]);
            cmdBindDescriptorSet(pCmd, SHADE_FORWARD, pDescriptorSetShade);
            cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_CAMERA, GEOM_TRANSPARENT), pDescriptorSetUniforms);
            DrawObjects(pCmd, gTransparentDrawCallCount, gTransparentDrawCalls, pRootSignature);
        }

        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void WeightedBlendedOrderIndependentTransparencyPass(Cmd* pCmd, bool volition)
    {
        Pipeline* pShadePipeline = volition ? pPipelineWBOITVShade : pPipelineWBOITShade;
        Pipeline* pCompositePipeline = volition ? pPipelineWBOITVComposite : pPipelineWBOITComposite;

        RenderTargetBarrier textureBarriers[WBOIT_RT_COUNT] = {};
        for (int i = 0; i < WBOIT_RT_COUNT; ++i)
        {
            textureBarriers[i].pRenderTarget = pRenderTargetWBOIT[i];
            textureBarriers[i].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            textureBarriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, WBOIT_RT_COUNT, textureBarriers);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (WBOIT)");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render transparent geometry (WBOIT)");

        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = WBOIT_RT_COUNT;
        for (int i = 0; i < WBOIT_RT_COUNT; ++i)
        {
            bindRenderTargets.mRenderTargets[i] = { pRenderTargetWBOIT[i], LOAD_ACTION_CLEAR };
        }
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetWBOIT[0]->mWidth, (float)pRenderTargetWBOIT[0]->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetWBOIT[0]->mWidth, pRenderTargetWBOIT[0]->mHeight);

        cmdBindPipeline(pCmd, pShadePipeline);
        cmdBindDescriptorSet(pCmd, SHADE_FORWARD, pDescriptorSetShade);
        cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_CAMERA, GEOM_TRANSPARENT), pDescriptorSetUniforms);
        DrawObjects(pCmd, gTransparentDrawCallCount, gTransparentDrawCalls, pRootSignature);

        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        // Composite WBOIT buffers
        for (int i = 0; i < WBOIT_RT_COUNT; ++i)
        {
            textureBarriers[i].pRenderTarget = pRenderTargetWBOIT[i];
            textureBarriers[i].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            textureBarriers[i].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, WBOIT_RT_COUNT, textureBarriers);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite WBOIT buffers");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Composite WBOIT buffers");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetScreen, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mWidth, (float)pRenderTargetScreen->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mWidth, pRenderTargetScreen->mHeight);

        cmdBindPipeline(pCmd, pCompositePipeline);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetWBOITComposite);
        cmdDraw(pCmd, 3, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void PhenomenologicalTransparencyPass(Cmd* pCmd)
    {
        RenderTargetBarrier textureBarriers[PT_RT_COUNT + 1] = {};

#if PT_USE_DIFFUSION != 0
        // Copy depth buffer
        textureBarriers[0].pRenderTarget = pRenderTargetDepth;
        textureBarriers[0].mCurrentState = RESOURCE_STATE_DEPTH_WRITE;
        textureBarriers[0].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        textureBarriers[1].pRenderTarget = pRenderTargetPTDepthCopy;
        textureBarriers[1].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        textureBarriers[1].mNewState = RESOURCE_STATE_RENDER_TARGET;
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, textureBarriers);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "PT Copy depth buffer");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "PT Copy depth buffer");

        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetPTDepthCopy, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPTDepthCopy->mWidth, (float)pRenderTargetPTDepthCopy->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetPTDepthCopy->mWidth, pRenderTargetPTDepthCopy->mHeight);

        cmdBindPipeline(pCmd, pPipelinePTCopyDepth);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPTCopyDepth);
        cmdDraw(pCmd, 3, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        textureBarriers[0].pRenderTarget = pRenderTargetDepth;
        textureBarriers[0].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        textureBarriers[0].mNewState = RESOURCE_STATE_DEPTH_READ | RESOURCE_STATE_DEPTH_WRITE;
        textureBarriers[1].pRenderTarget = pRenderTargetPTDepthCopy;
        textureBarriers[1].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
        textureBarriers[1].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, textureBarriers);
#endif

        for (int i = 0; i < PT_RT_COUNT; ++i)
        {
            textureBarriers[i].pRenderTarget = pRenderTargetPT[i];
            textureBarriers[i].mCurrentState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            textureBarriers[i].mNewState = RESOURCE_STATE_RENDER_TARGET;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, PT_RT_COUNT, textureBarriers);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (PT)");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render transparent geometry (PT)");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = PT_RT_COUNT;
        for (int i = 0; i < PT_RT_COUNT; ++i)
        {
            bindRenderTargets.mRenderTargets[i] = { pRenderTargetPT[i], LOAD_ACTION_CLEAR };
        }
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetPT[0]->mWidth, (float)pRenderTargetPT[0]->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetPT[0]->mWidth, pRenderTargetPT[0]->mHeight);

        cmdBindPipeline(pCmd, pPipelinePTShade);
        cmdBindDescriptorSet(pCmd, SHADE_PT, pDescriptorSetShade);
        cmdBindDescriptorSet(pCmd, UNIFORM_SET(gFrameIndex, VIEW_CAMERA, GEOM_TRANSPARENT), pDescriptorSetUniforms);
        DrawObjects(pCmd, gTransparentDrawCallCount, gTransparentDrawCalls, pRootSignature);

        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        // Composite PT buffers
        for (uint32_t i = 0; i < PT_RT_COUNT; ++i)
        {
            textureBarriers[i].pRenderTarget = pRenderTargetPT[i];
            textureBarriers[i].mCurrentState = RESOURCE_STATE_RENDER_TARGET;
            textureBarriers[i].mNewState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, PT_RT_COUNT, textureBarriers);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite PT buffers");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Composite PT buffers");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetScreen, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mWidth, (float)pRenderTargetScreen->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mWidth, pRenderTargetScreen->mHeight);

        cmdBindPipeline(pCmd, pPipelinePTComposite);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetPTComposite);
        cmdDraw(pCmd, 3, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void AdaptiveOrderIndependentTransparency(Cmd* pCmd)
    {
        if (!gGpuSettings.mEnableAOIT)
        {
            return;
        }

        TextureBarrier textureBarrier = { pTextureAOITClearMask, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };
        BufferBarrier  bufferBarriers[] = {
			{ pBufferAOITColorData, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
#if AOIT_NODE_COUNT != 2
			{ pBufferAOITDepthData, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
#endif
		};
        cmdResourceBarrier(pCmd, sizeof(bufferBarriers) / sizeof(bufferBarriers[0]), bufferBarriers, 1, &textureBarrier, 0, NULL);

        // Draw fullscreen quad.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Clear AOIT buffers");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Clear AOIT buffers");

        // Clear AOIT buffers
        // Start render pass and apply load actions
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_DONTCARE };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mWidth, (float)pRenderTargetScreen->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mWidth, pRenderTargetScreen->mHeight);

        cmdBindPipeline(pCmd, pPipelineAOITClear);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetAOITClear);
        cmdDraw(pCmd, 3, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        // Draw the transparent geometry.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Draw transparent geometry (AOIT)");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Render transparent geometry (AOIT)");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mDepthStencil = { pRenderTargetDepth, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pSwapChain->ppRenderTargets[0]->mWidth, (float)pSwapChain->ppRenderTargets[0]->mHeight,
                       0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pSwapChain->ppRenderTargets[0]->mWidth, pSwapChain->ppRenderTargets[0]->mHeight);

        cmdBindPipeline(pCmd, pPipelineAOITShade);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetAOITShade[0]);
        cmdBindDescriptorSet(pCmd, gFrameIndex, pDescriptorSetAOITShade[1]);
        DrawObjects(pCmd, gTransparentDrawCallCount, gTransparentDrawCalls, pRootSignatureAOITShade);

        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);

        // Composite AOIT buffers
        textureBarrier = { pTextureAOITClearMask, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
        bufferBarriers[0] = { pBufferAOITColorData, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
#if AOIT_NODE_COUNT != 2
        bufferBarriers[1] = { pBufferAOITDepthData, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
#endif
        cmdResourceBarrier(pCmd, sizeof(bufferBarriers) / sizeof(bufferBarriers[0]), bufferBarriers, 1, &textureBarrier, 0, NULL);

        // Draw fullscreen quad.
        cmdBeginDebugMarker(pCmd, 1, 0, 1, "Composite AOIT buffers");
        cmdBeginGpuTimestampQuery(pCmd, gCurrentGpuProfileToken, "Composite AOIT buffers");

        // Start render pass and apply load actions
        bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetScreen, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTargetScreen->mWidth, (float)pRenderTargetScreen->mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, pRenderTargetScreen->mWidth, pRenderTargetScreen->mHeight);

        cmdBindPipeline(pCmd, pPipelineAOITComposite);
        cmdBindDescriptorSet(pCmd, 0, pDescriptorSetAOITComposite);
        cmdDraw(pCmd, 3, 0);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndGpuTimestampQuery(pCmd, gCurrentGpuProfileToken);
        cmdEndDebugMarker(pCmd);
    }

    void Draw() override
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        resetCmdPool(pRenderer, elem.pCmdPool);

        getHiresTimerUSec(&gCpuTimer, true);
        /************************************************************************/
        // Update uniform buffers
        /************************************************************************/
        BufferUpdateDesc materialBufferUpdateDesc = { pBufferMaterials[gFrameIndex] };
        beginUpdateResource(&materialBufferUpdateDesc);
        memcpy(materialBufferUpdateDesc.pMappedData, &gMaterialUniformData, sizeof(gMaterialUniformData));
        endUpdateResource(&materialBufferUpdateDesc);
        BufferUpdateDesc opaqueBufferUpdateDesc = { pBufferOpaqueObjectTransforms[gFrameIndex] };
        beginUpdateResource(&opaqueBufferUpdateDesc);
        memcpy(opaqueBufferUpdateDesc.pMappedData, &gObjectInfoUniformData, sizeof(gObjectInfoUniformData));
        endUpdateResource(&opaqueBufferUpdateDesc);
        BufferUpdateDesc transparentBufferUpdateDesc = { pBufferTransparentObjectTransforms[gFrameIndex] };
        beginUpdateResource(&transparentBufferUpdateDesc);
        *(ObjectInfoUniformBlock*)transparentBufferUpdateDesc.pMappedData = gTransparentObjectInfoUniformData;
        memcpy(transparentBufferUpdateDesc.pMappedData, &gTransparentObjectInfoUniformData, sizeof(gTransparentObjectInfoUniformData));
        endUpdateResource(&transparentBufferUpdateDesc);

        BufferUpdateDesc cameraCbv = { pBufferCameraUniform[gFrameIndex] };
        beginUpdateResource(&cameraCbv);
        memcpy(cameraCbv.pMappedData, &gCameraUniformData, sizeof(gCameraUniformData));
        endUpdateResource(&cameraCbv);

        BufferUpdateDesc cameraLightBufferCbv = { pBufferCameraLightUniform[gFrameIndex] };
        beginUpdateResource(&cameraLightBufferCbv);
        memcpy(cameraLightBufferCbv.pMappedData, &gCameraLightUniformData, sizeof(gCameraLightUniformData));
        endUpdateResource(&cameraLightBufferCbv);

        BufferUpdateDesc skyboxViewProjCbv = { pBufferSkyboxUniform[gFrameIndex] };
        beginUpdateResource(&skyboxViewProjCbv);
        memcpy(skyboxViewProjCbv.pMappedData, &gSkyboxUniformData, sizeof(gSkyboxUniformData));
        endUpdateResource(&skyboxViewProjCbv);

        BufferUpdateDesc lightBufferCbv = { pBufferLightUniform[gFrameIndex] };
        beginUpdateResource(&lightBufferCbv);
        memcpy(lightBufferCbv.pMappedData, &gLightUniformData, sizeof(gLightUniformData));
        endUpdateResource(&lightBufferCbv);
        /************************************************************************/
        // Update transparency settings
        /************************************************************************/
        if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
        {
            BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings[gFrameIndex] };
            beginUpdateResource(&wboitSettingsUpdateDesc);
            memcpy(wboitSettingsUpdateDesc.pMappedData, &gWBOITSettingsData, sizeof(gWBOITSettingsData));
            endUpdateResource(&wboitSettingsUpdateDesc);
        }
        else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
        {
            BufferUpdateDesc wboitSettingsUpdateDesc = { pBufferWBOITSettings[gFrameIndex] };
            beginUpdateResource(&wboitSettingsUpdateDesc);
            memcpy(wboitSettingsUpdateDesc.pMappedData, &gWBOITVolitionSettingsData, sizeof(gWBOITVolitionSettingsData));
            endUpdateResource(&wboitSettingsUpdateDesc);
        }
        /************************************************************************/
        // Rendering
        /************************************************************************/
        // Get command list to store rendering commands for this frame
        Cmd* pCmd = elem.pCmds[0];

        pRenderTargetScreen = pSwapChain->ppRenderTargets[swapchainImageIndex];
        beginCmd(pCmd);

        gCurrentGpuProfileToken = gCurrentGpuProfileTokens[gTransparencyType];

        cmdBeginGpuFrameProfile(pCmd, gCurrentGpuProfileToken);
        RenderTargetBarrier barriers1[] = {
            { pRenderTargetScreen, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers1);

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
        else if (gTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT)
            AdaptiveOrderIndependentTransparency(pCmd);
        else
            ASSERT(false && "Not implemented.");

        ////////////////////////////////////////////////////////
        //  Draw UIs

        cmdBeginDebugMarker(pCmd, 0, 1, 0, "Draw UI");
        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTargetScreen, LOAD_ACTION_LOAD };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);

        gFrameTimeDraw.mFontColor = 0xff00ffff;
        gFrameTimeDraw.mFontSize = 18.0f;
        gFrameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(pCmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        cmdDrawGpuProfile(pCmd, float2(8.0f, txtSize.y + 75.f), gCurrentGpuProfileToken, &gFrameTimeDraw);

        cmdDrawUserInterface(pCmd);
        cmdBindRenderTargets(pCmd, NULL);

        cmdEndDebugMarker(pCmd);
        ////////////////////////////////////////////////////////

        barriers1[0] = { pRenderTargetScreen, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, barriers1);

        cmdEndGpuFrameProfile(pCmd, gCurrentGpuProfileToken);
        endCmd(pCmd);

        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &pCmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() override { return "15_Transparency"; }

    /************************************************************************/
    // Init and Exit functions
    /************************************************************************/
    void addSamplers()
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
        samplerTrilinearAnisoDesc.mMipLodBias = 0.0f;
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

    void removeSamplers()
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

    void addShaders()
    {
        // Skybox shader
        ShaderLoadDesc skyboxShaderDesc = {};
        skyboxShaderDesc.mStages[0].pFileName = "skybox.vert";
        skyboxShaderDesc.mStages[1].pFileName = "skybox.frag";
        addShader(pRenderer, &skyboxShaderDesc, &pShaderSkybox);

#if USE_SHADOWS != 0
        // Shadow mapping shader
        ShaderLoadDesc shadowShaderDesc = {};
        shadowShaderDesc.mStages[0].pFileName = "shadow.vert";
        shadowShaderDesc.mStages[1].pFileName = "shadow.frag";
        addShader(pRenderer, &shadowShaderDesc, &pShaderShadow);

        // Gaussian blur shader
        ShaderLoadDesc blurShaderDesc = {};
        blurShaderDesc.mStages[0].pFileName = "fullscreen.vert";
        blurShaderDesc.mStages[1].pFileName = "gaussianBlur.frag";
        addShader(pRenderer, &blurShaderDesc, &pShaderGaussianBlur);

#if PT_USE_CAUSTICS != 0
        // Stochastic shadow mapping shader
        ShaderLoadDesc stochasticShadowShaderDesc = {};
        stochasticShadowShaderDesc.mStages[0].pFileName = "forward.vert";
        stochasticShadowShaderDesc.mStages[1].pFileName = "stochasticShadow.frag";
        addShader(pRenderer, &stochasticShadowShaderDesc, &pShaderPTShadow);

        // Downsample shader
        ShaderLoadDesc downsampleShaderDesc = {};
        downsampleShaderDesc.mStages[0].pFileName = "fullscreen.vert";
        downsampleShaderDesc.mStages[1].pFileName = "downsample.frag";
        addShader(pRenderer, &downsampleShaderDesc, &pShaderPTDownsample);

        // Shadow map copy shader
        ShaderLoadDesc copyShadowDepthShaderDesc = {};
        copyShadowDepthShaderDesc.mStages[0].pFileName = {
            "fullscreen.vert",
        };
        copyShadowDepthShaderDesc.mStages[1].pFileName = {
            "copy.frag",
        };
        addShader(pRenderer, &copyShadowDepthShaderDesc, &pShaderPTCopyShadowDepth);
#endif
#endif

        // Forward shading shader
        ShaderLoadDesc forwardShaderDesc = {};
        forwardShaderDesc.mStages[0].pFileName = "forward.vert";
        forwardShaderDesc.mStages[1].pFileName = "forward.frag";
        addShader(pRenderer, &forwardShaderDesc, &pShaderForward);

        // WBOIT shade shader
        ShaderLoadDesc wboitShadeShaderDesc = {};
        wboitShadeShaderDesc.mStages[0].pFileName = "forward.vert";
        wboitShadeShaderDesc.mStages[1].pFileName = "weightedBlendedOIT.frag";
        addShader(pRenderer, &wboitShadeShaderDesc, &pShaderWBOITShade);

        // WBOIT composite shader
        ShaderLoadDesc wboitCompositeShaderDesc = {};
        wboitCompositeShaderDesc.mStages[0] = { "fullscreen.vert" };
        wboitCompositeShaderDesc.mStages[1] = { "weightedBlendedOITComposite.frag" };
        addShader(pRenderer, &wboitCompositeShaderDesc, &pShaderWBOITComposite);

        // WBOIT Volition shade shader
        ShaderLoadDesc wboitVolitionShadeShaderDesc = {};
        wboitVolitionShadeShaderDesc.mStages[0] = { "forward.vert" };
        wboitVolitionShadeShaderDesc.mStages[1] = { "weightedBlendedOITVolition.frag" };
        addShader(pRenderer, &wboitVolitionShadeShaderDesc, &pShaderWBOITVShade);

        // WBOIT Volition composite shader
        ShaderLoadDesc wboitVolitionCompositeShaderDesc = {};
        wboitVolitionCompositeShaderDesc.mStages[0] = { "fullscreen.vert" };
        wboitVolitionCompositeShaderDesc.mStages[1] = { "weightedBlendedOITVolitionComposite.frag" };
        addShader(pRenderer, &wboitVolitionCompositeShaderDesc, &pShaderWBOITVComposite);

        // PT shade shader
        ShaderLoadDesc ptShadeShaderDesc = {};
        ptShadeShaderDesc.mStages[0] = { "forward.vert" };
        ptShadeShaderDesc.mStages[1] = { "phenomenologicalTransparency.frag" };
        addShader(pRenderer, &ptShadeShaderDesc, &pShaderPTShade);

        // PT composite shader
        ShaderLoadDesc ptCompositeShaderDesc = {};
        ptCompositeShaderDesc.mStages[0] = { "fullscreen.vert" };
        ptCompositeShaderDesc.mStages[1] = { "phenomenologicalTransparencyComposite.frag" };
        addShader(pRenderer, &ptCompositeShaderDesc, &pShaderPTComposite);

#if PT_USE_DIFFUSION != 0
        // PT copy depth shader
        ShaderLoadDesc ptCopyShaderDesc = {};
        ptCopyShaderDesc.mStages[0] = { "fullscreen.vert" };
        ptCopyShaderDesc.mStages[1] = { "copy.frag" };
        addShader(pRenderer, &ptCopyShaderDesc, &pShaderPTCopyDepth);

        // PT generate mips shader
        ShaderLoadDesc ptGenMipsShaderDesc = {};
        ptGenMipsShaderDesc.mStages[0] = { "generateMips.comp" };
        addShader(pRenderer, &ptGenMipsShaderDesc, &pShaderPTGenMips);
#endif

        if (gGpuSettings.mEnableAOIT)
        {
            // AOIT shade shader
            ShaderLoadDesc aoitShadeShaderDesc = {};
            aoitShadeShaderDesc.mStages[0] = { "forward.vert" };
            aoitShadeShaderDesc.mStages[1] = { "AdaptiveOIT.frag" };
            addShader(pRenderer, &aoitShadeShaderDesc, &pShaderAOITShade);

            // AOIT composite shader
            ShaderLoadDesc aoitCompositeShaderDesc = {};
            aoitCompositeShaderDesc.mStages[0] = { "fullscreen.vert" };
            aoitCompositeShaderDesc.mStages[1] = { "AdaptiveOITComposite.frag" };
            addShader(pRenderer, &aoitCompositeShaderDesc, &pShaderAOITComposite);

            // AOIT clear shader
            ShaderLoadDesc aoitClearShaderDesc = {};
            aoitClearShaderDesc.mStages[0] = { "fullscreen.vert" };
            aoitClearShaderDesc.mStages[1] = { "AdaptiveOITClear.frag" };
            addShader(pRenderer, &aoitClearShaderDesc, &pShaderAOITClear);
        }
    }

    void removeShaders()
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

        if (gGpuSettings.mEnableAOIT)
        {
            removeShader(pRenderer, pShaderAOITShade);
            removeShader(pRenderer, pShaderAOITComposite);
            removeShader(pRenderer, pShaderAOITClear);
        }
    }

    void addRootSignatures()
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
        RootSignatureDesc blurRootSignatureDesc = {};
        blurRootSignatureDesc.ppShaders = &pShaderGaussianBlur;
        blurRootSignatureDesc.mShaderCount = 1;
        blurRootSignatureDesc.ppStaticSamplers = staticSamplers;
        blurRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
        blurRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
        blurRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
        addRootSignature(pRenderer, &blurRootSignatureDesc, &pRootSignatureGaussianBlur);
        gBlurAxisRootConstantIndex = getDescriptorIndexFromName(pRootSignatureGaussianBlur, "RootConstant");

#if PT_USE_CAUSTICS != 0
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

        Shader* pShaders[] = {
			pShaderShadow,
			pShaderWBOITShade,
			pShaderWBOITVShade,
			pShaderForward,
			pShaderPTShade,
#if PT_USE_CAUSTICS
			pShaderPTShadow
#endif
		};
        // Forward shading root signature
        RootSignatureDesc forwardRootSignatureDesc = {};
        forwardRootSignatureDesc.ppShaders = pShaders;
        forwardRootSignatureDesc.mShaderCount = sizeof(pShaders) / sizeof(pShaders[0]);
        forwardRootSignatureDesc.ppStaticSamplers = staticSamplers;
        forwardRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
        forwardRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
        forwardRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
        addRootSignature(pRenderer, &forwardRootSignatureDesc, &pRootSignature);

        // WBOIT composite root signature
        Shader*           pShadersWBOITComposite[2] = { pShaderWBOITComposite, pShaderWBOITVComposite };
        RootSignatureDesc wboitCompositeRootSignatureDesc = {};
        wboitCompositeRootSignatureDesc.ppShaders = pShadersWBOITComposite;
        wboitCompositeRootSignatureDesc.mShaderCount = 2;
        wboitCompositeRootSignatureDesc.ppStaticSamplers = staticSamplers;
        wboitCompositeRootSignatureDesc.mStaticSamplerCount = numStaticSamplers;
        wboitCompositeRootSignatureDesc.ppStaticSamplerNames = staticSamplerNames;
        wboitCompositeRootSignatureDesc.mMaxBindlessTextures = TEXTURE_COUNT;
        addRootSignature(pRenderer, &wboitCompositeRootSignatureDesc, &pRootSignatureWBOITComposite);

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
        gMipSizeRootConstantIndex = getDescriptorIndexFromName(pRootSignaturePTGenMips, "RootConstant");
#endif

        if (gGpuSettings.mEnableAOIT)
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
    }

    void removeRootSignatures()
    {
        removeRootSignature(pRenderer, pRootSignatureSkybox);
#if USE_SHADOWS != 0
        removeRootSignature(pRenderer, pRootSignature);
        removeRootSignature(pRenderer, pRootSignatureGaussianBlur);
#if PT_USE_CAUSTICS != 0
        removeRootSignature(pRenderer, pRootSignaturePTDownsample);
        removeRootSignature(pRenderer, pRootSignaturePTCopyShadowDepth);
#endif
#endif
        removeRootSignature(pRenderer, pRootSignatureWBOITComposite);
        removeRootSignature(pRenderer, pRootSignaturePTComposite);
#if PT_USE_DIFFUSION != 0
        removeRootSignature(pRenderer, pRootSignaturePTCopyDepth);
        removeRootSignature(pRenderer, pRootSignaturePTGenMips);
#endif
        if (gGpuSettings.mEnableAOIT)
        {
            removeRootSignature(pRenderer, pRootSignatureAOITShade);
            removeRootSignature(pRenderer, pRootSignatureAOITComposite);
            removeRootSignature(pRenderer, pRootSignatureAOITClear);
        }
    }

    void addDescriptorSets()
    {
        // Skybox
        DescriptorSetDesc setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[0]);
        setDesc = { pRootSignatureSkybox, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetSkybox[1]);
        // Gaussian blur
        setDesc = { pRootSignatureGaussianBlur, DESCRIPTOR_UPDATE_FREQ_NONE, 2 + (3 * 2) };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetGaussianBlur);
        // Uniforms
        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 4 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetUniforms);
        // Forward
        setDesc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 3 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetShade);
        // Gen Mips
        setDesc = { pRootSignaturePTGenMips, DESCRIPTOR_UPDATE_FREQ_PER_DRAW, (1 << 5) };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPTGenMips);
        // WBOIT Composite
        setDesc = { pRootSignatureWBOITComposite, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetWBOITComposite);
        // PT Copy Depth
        setDesc = { pRootSignaturePTCopyDepth, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPTCopyDepth);
        // PT Composite
        setDesc = { pRootSignaturePTComposite, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPTComposite);
#if PT_USE_CAUSTICS
        // PT Copy Shadow Depth
        setDesc = { pRootSignaturePTCopyShadowDepth, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPTCopyShadowDepth);
        // PT Downsample
        setDesc = { pRootSignaturePTDownsample, DESCRIPTOR_UPDATE_FREQ_NONE, 3 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPTDownsample);
#endif
        if (gGpuSettings.mEnableAOIT)
        {
            // AOIT Clear
            setDesc = { pRootSignatureAOITClear, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetAOITClear);
            // AOIT Shade
            setDesc = { pRootSignatureAOITShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetAOITShade[0]);
            setDesc = { pRootSignatureAOITShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetAOITShade[1]);
            // AOIT Composite
            setDesc = { pRootSignatureAOITComposite, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetAOITComposite);
        }
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetSkybox[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetGaussianBlur);
        removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
        removeDescriptorSet(pRenderer, pDescriptorSetShade);
        removeDescriptorSet(pRenderer, pDescriptorSetPTGenMips);
        removeDescriptorSet(pRenderer, pDescriptorSetWBOITComposite);
        removeDescriptorSet(pRenderer, pDescriptorSetPTCopyDepth);
        removeDescriptorSet(pRenderer, pDescriptorSetPTComposite);
#if PT_USE_CAUSTICS
        removeDescriptorSet(pRenderer, pDescriptorSetPTCopyShadowDepth);
        removeDescriptorSet(pRenderer, pDescriptorSetPTDownsample);
#endif
        if (gGpuSettings.mEnableAOIT)
        {
            removeDescriptorSet(pRenderer, pDescriptorSetAOITClear);
            removeDescriptorSet(pRenderer, pDescriptorSetAOITShade[0]);
            removeDescriptorSet(pRenderer, pDescriptorSetAOITShade[1]);
            removeDescriptorSet(pRenderer, pDescriptorSetAOITComposite);
        }
    }

    void prepareDescriptorSets()
    {
        // Skybox
        {
            DescriptorData params[6] = {};
            params[0].pName = "RightText";
            params[0].ppTextures = &pTextures[TEXTURE_SKYBOX_RIGHT];
            params[1].pName = "LeftText";
            params[1].ppTextures = &pTextures[TEXTURE_SKYBOX_LEFT];
            params[2].pName = "TopText";
            params[2].ppTextures = &pTextures[TEXTURE_SKYBOX_UP];
            params[3].pName = "BotText";
            params[3].ppTextures = &pTextures[TEXTURE_SKYBOX_DOWN];
            params[4].pName = "FrontText";
            params[4].ppTextures = &pTextures[TEXTURE_SKYBOX_FRONT];
            params[5].pName = "BackText";
            params[5].ppTextures = &pTextures[TEXTURE_SKYBOX_BACK];
            updateDescriptorSet(pRenderer, 0, pDescriptorSetSkybox[0], 6, params);
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                params[0].pName = "SkyboxUniformBlock";
                params[0].ppBuffers = &pBufferSkyboxUniform[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetSkybox[1], 1, params);
            }
        }
        // Gaussian blur
        {
            DescriptorData params[2] = {};
            params[0].pName = "Source";
            params[0].ppTextures = &pRenderTargetShadowVariance[0]->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetGaussianBlur, 1, params);
            params[0].ppTextures = &pRenderTargetShadowVariance[1]->pTexture;
            updateDescriptorSet(pRenderer, 1, pDescriptorSetGaussianBlur, 1, params);
#if PT_USE_CAUSTICS
            for (uint32_t w = 0; w < 3; ++w)
            {
                params[0].ppTextures = &pRenderTargetPTShadowFinal[0][w]->pTexture;
                updateDescriptorSet(pRenderer, 2 + (w * 2 + 0), pDescriptorSetGaussianBlur, 1, params);
                params[0].ppTextures = &pRenderTargetPTShadowFinal[1][w]->pTexture;
                updateDescriptorSet(pRenderer, 2 + (w * 2 + 1), pDescriptorSetGaussianBlur, 1, params);
            }
#endif
        }
        // Shadow, Forward, WBOIT, PT, AOIT
        {
            uint32_t       updateCount = 1;
            DescriptorData params[9] = {};
            params[0].pName = "MaterialTextures";
            params[0].ppTextures = pTextures;
            params[0].mCount = TEXTURE_COUNT;

#if PT_USE_CAUSTICS
            updateDescriptorSet(pRenderer, SHADE_PT_SHADOW, pDescriptorSetShade, updateCount, params);
#endif

#if USE_SHADOWS != 0
            params[1].pName = "VSM";
            params[1].ppTextures = &pRenderTargetShadowVariance[0]->pTexture;
            ++updateCount;
#if PT_USE_CAUSTICS != 0
            params[2].pName = "VSMRed";
            params[2].ppTextures = &pRenderTargetPTShadowFinal[0][0]->pTexture;
            params[3].pName = "VSMGreen";
            params[3].ppTextures = &pRenderTargetPTShadowFinal[0][1]->pTexture;
            params[4].pName = "VSMBlue";
            params[4].ppTextures = &pRenderTargetPTShadowFinal[0][2]->pTexture;
            updateCount += 3;
#endif
#endif
            updateDescriptorSet(pRenderer, SHADE_FORWARD, pDescriptorSetShade, updateCount, params);

#if PT_USE_DIFFUSION != 0
            params[updateCount].pName = "DepthTexture";
            params[updateCount].ppTextures = &pRenderTargetPTDepthCopy->pTexture;
#endif
            updateDescriptorSet(pRenderer, SHADE_PT, pDescriptorSetShade, updateCount + 1, params);

            if (gGpuSettings.mEnableAOIT)
            {
                params[updateCount].pName = "AOITClearMaskUAV";
                params[updateCount].ppTextures = &pTextureAOITClearMask;
                params[updateCount + 1].pName = "AOITColorDataUAV";
                params[updateCount + 1].ppBuffers = &pBufferAOITColorData;
                updateCount += 2;
#if AOIT_NODE_COUNT != 2
                params[updateCount].pName = "AOITDepthDataUAV";
                params[updateCount].ppBuffers = &pBufferAOITDepthData;
                ++updateCount;
#endif

                updateDescriptorSet(pRenderer, 0, pDescriptorSetAOITShade[0], updateCount, params);
            }

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                // Opaque objects
                DescriptorData oParams[5] = {};
                oParams[0].pName = "ObjectUniformBlock";
                oParams[0].ppBuffers = &pBufferOpaqueObjectTransforms[i];
                oParams[1].pName = "CameraUniform";
                oParams[1].ppBuffers = &pBufferCameraLightUniform[i];
                oParams[2].pName = "MaterialUniform";
                oParams[2].ppBuffers = &pBufferMaterials[i];
                oParams[3].pName = "LightUniformBlock";
                oParams[3].ppBuffers = &pBufferLightUniform[i];
                oParams[4].pName = "WBOITSettings";
                oParams[4].ppBuffers = &pBufferWBOITSettings[i];

                // View Shadow Geom Opaque
                updateDescriptorSet(pRenderer, UNIFORM_SET(i, VIEW_SHADOW, GEOM_OPAQUE), pDescriptorSetUniforms, 5, oParams);
                // View Shadow Geom Transparent
                oParams[0].ppBuffers = &pBufferTransparentObjectTransforms[i];
                updateDescriptorSet(pRenderer, UNIFORM_SET(i, VIEW_SHADOW, GEOM_TRANSPARENT), pDescriptorSetUniforms, 5, oParams);
                oParams[0].ppBuffers = &pBufferOpaqueObjectTransforms[i];
                oParams[1].ppBuffers = &pBufferCameraUniform[i];
                // View Camera Geom Opaque
                updateDescriptorSet(pRenderer, UNIFORM_SET(i, VIEW_CAMERA, GEOM_OPAQUE), pDescriptorSetUniforms, 5, oParams);
                // View Camera Geom Transparent
                oParams[0].ppBuffers = &pBufferTransparentObjectTransforms[i];
                updateDescriptorSet(pRenderer, UNIFORM_SET(i, VIEW_CAMERA, GEOM_TRANSPARENT), pDescriptorSetUniforms, 5, oParams);

                if (gGpuSettings.mEnableAOIT)
                {
                    // AOIT
                    updateDescriptorSet(pRenderer, i, pDescriptorSetAOITShade[1], 4, oParams);
                }
            }
        }
        // Gen Mips
        {
            RenderTarget* rt = pRenderTargetPTBackground;
            for (uint32_t i = 1; i < rt->mMipLevels; ++i)
            {
                DescriptorData params[2] = {};
                params[0].pName = "Source";
                params[0].ppTextures = &rt->pTexture;
                params[0].mUAVMipSlice = (uint16_t)(i - 1);
                params[1].pName = "Destination";
                params[1].ppTextures = &rt->pTexture;
                params[1].mUAVMipSlice = (uint16_t)i;
                updateDescriptorSet(pRenderer, i - 1, pDescriptorSetPTGenMips, 2, params);
            }
        }
        // WBOIT Composite
        {
            DescriptorData compositeParams[2] = {};
            compositeParams[0].pName = "AccumulationTexture";
            compositeParams[0].ppTextures = &pRenderTargetWBOIT[WBOIT_RT_ACCUMULATION]->pTexture;
            compositeParams[1].pName = "RevealageTexture";
            compositeParams[1].ppTextures = &pRenderTargetWBOIT[WBOIT_RT_REVEALAGE]->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetWBOITComposite, 2, compositeParams);
        }
        // PT Copy Depth
        {
            DescriptorData copyParams[1] = {};
            copyParams[0].pName = "Source";
            copyParams[0].ppTextures = &pRenderTargetDepth->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetPTCopyDepth, 1, copyParams);
        }
        // PT Composite
        {
            uint32_t       compositeParamCount = 3;
            DescriptorData compositeParams[4] = {};
            compositeParams[0].pName = "AccumulationTexture";
            compositeParams[0].ppTextures = &pRenderTargetPT[PT_RT_ACCUMULATION]->pTexture;
            compositeParams[1].pName = "ModulationTexture";
            compositeParams[1].ppTextures = &pRenderTargetPT[PT_RT_MODULATION]->pTexture;
            compositeParams[2].pName = "BackgroundTexture";
            compositeParams[2].ppTextures = &pRenderTargetPTBackground->pTexture;
#if PT_USE_REFRACTION != 0
            compositeParams[3].pName = "RefractionTexture";
            compositeParams[3].ppTextures = &pRenderTargetPT[PT_RT_REFRACTION]->pTexture;
            ++compositeParamCount;
#endif
            updateDescriptorSet(pRenderer, 0, pDescriptorSetPTComposite, compositeParamCount, compositeParams);
        }
        // PT Shadows
#if PT_USE_CAUSTICS
        {
            DescriptorData params[1] = {};
            params[0].pName = "Source";
            params[0].ppTextures = &pRenderTargetShadowVariance[0]->pTexture;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetPTCopyShadowDepth, 1, params);

            for (uint32_t w = 0; w < 3; ++w)
            {
                DescriptorData params[1] = {};
                params[0].pName = "Source";
                params[0].ppTextures = &pRenderTargetPTShadowVariance[w]->pTexture;
                updateDescriptorSet(pRenderer, w, pDescriptorSetPTDownsample, 1, params);
            }
        }
#endif
        // AOIT
        if (gGpuSettings.mEnableAOIT)
        {
            DescriptorData clearParams[1] = {};
            clearParams[0].pName = "AOITClearMaskUAV";
            clearParams[0].ppTextures = &pTextureAOITClearMask;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetAOITClear, 1, clearParams);

            DescriptorData compositeParams[2] = {};
            compositeParams[0].pName = "AOITClearMaskSRV";
            compositeParams[0].ppTextures = &pTextureAOITClearMask;
            compositeParams[1].pName = "AOITColorDataSRV";
            compositeParams[1].ppBuffers = &pBufferAOITColorData;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetAOITComposite, 2, compositeParams);
        }
    }

    void addResources()
    {
        addTextures();

        static const float gSkyboxPointArray[] = {
            10.0f,  -10.0f, -10.0f, 6.0f, // -z
            -10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
            -10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

            -10.0f, -10.0f, 10.0f,  2.0f, //-x
            -10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
            -10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

            10.0f,  -10.0f, -10.0f, 1.0f, //+x
            10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
            10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

            -10.0f, -10.0f, 10.0f,  5.0f, // +z
            -10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
            10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

            -10.0f, 10.0f,  -10.0f, 3.0f, //+y
            10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
            10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

            10.0f,  -10.0f, 10.0f,  4.0f, //-y
            10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
            -10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
        };

        uint64_t       skyBoxDataSize = 4 * 6 * 6 * sizeof(float);
        BufferLoadDesc skyboxVbDesc = {};
        skyboxVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        skyboxVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        skyboxVbDesc.mDesc.mSize = skyBoxDataSize;
        skyboxVbDesc.pData = gSkyboxPointArray;
        skyboxVbDesc.ppBuffer = &pBufferSkyboxVertex;
        addResource(&skyboxVbDesc, NULL);

#if USE_SHADOWS != 0
        const uint shadowMapResolution = 1024;

        RenderTargetDesc renderTargetDesc = {};
        renderTargetDesc.mArraySize = 1;
        renderTargetDesc.mClearValue = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        renderTargetDesc.mDepth = 1;
        renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        renderTargetDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
        renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        renderTargetDesc.mWidth = shadowMapResolution;
        renderTargetDesc.mHeight = shadowMapResolution;
        renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
        renderTargetDesc.mSampleQuality = 0;
        renderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        renderTargetDesc.pName = "Shadow variance RT";
        for (uint32_t i = 0; i < 2; ++i)
        {
            addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetShadowVariance[i]);
        }

        RenderTargetDesc shadowRT = {};
        shadowRT.mArraySize = 1;
        shadowRT.mClearValue = { { 1.0f, 0.0f } };
        shadowRT.mDepth = 1;
        shadowRT.mFormat = TinyImageFormat_D32_SFLOAT;
        shadowRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        shadowRT.mWidth = shadowMapResolution;
        shadowRT.mHeight = shadowMapResolution;
        shadowRT.mSampleCount = SAMPLE_COUNT_1;
        shadowRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        shadowRT.mSampleQuality = 0;
        shadowRT.pName = "Shadow depth RT";
        addRenderTarget(pRenderer, &shadowRT, &pRenderTargetShadowDepth);

#if PT_USE_CAUSTICS != 0
        const uint ptShadowMapResolution = 4096;
        renderTargetDesc = {};
        renderTargetDesc.mArraySize = 1;
        renderTargetDesc.mClearValue = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        renderTargetDesc.mDepth = 1;
        renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        renderTargetDesc.mFormat = TinyImageFormat_R16G16_UNORM;
        renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        renderTargetDesc.mWidth = ptShadowMapResolution;
        renderTargetDesc.mHeight = ptShadowMapResolution;
        renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
        renderTargetDesc.mSampleQuality = 0;
        renderTargetDesc.pName = "PT shadow variance RT";
        for (uint32_t w = 0; w < 3; ++w)
        {
            addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTShadowVariance[w]);
        }

        renderTargetDesc = {};
        renderTargetDesc.mArraySize = 1;
        renderTargetDesc.mClearValue = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        renderTargetDesc.mDepth = 1;
        renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        renderTargetDesc.mFormat = TinyImageFormat_R16G16_UNORM;
        renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        renderTargetDesc.mWidth = ptShadowMapResolution / 4;
        renderTargetDesc.mHeight = ptShadowMapResolution / 4;
        renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
        renderTargetDesc.mSampleQuality = 0;
        renderTargetDesc.pName = "PT shadow final RT";
        for (uint32_t w = 0; w < 3; ++w)
        {
            for (uint32_t i = 0; i < 2; ++i)
            {
                addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTShadowFinal[i][w]);
            }
        }
#endif
#endif
    }

    void removeResources()
    {
        removeResource(pBufferSkyboxVertex);
#if USE_SHADOWS != 0
        for (uint32_t i = 0; i < 2; ++i)
        {
            removeRenderTarget(pRenderer, pRenderTargetShadowVariance[i]);
        }
        removeRenderTarget(pRenderer, pRenderTargetShadowDepth);
#if PT_USE_CAUSTICS != 0
        for (uint32_t w = 0; w < 3; ++w)
        {
            removeRenderTarget(pRenderer, pRenderTargetPTShadowVariance[w]);
            for (uint32_t i = 0; i < 2; ++i)
            {
                removeRenderTarget(pRenderer, pRenderTargetPTShadowFinal[i][w]);
            }
        }
#endif
#endif

        removeTextures();
        removeModels();
    }

    void addModel(size_t m)
    {
        static const char* modelNames[MESH_COUNT] = { "cube.bin", "sphere.bin", "plane.bin", "lion.bin" };

        GeometryLoadDesc loadDesc = {};
        loadDesc.pFileName = modelNames[m];
        loadDesc.ppGeometry = &pMeshes[m];
        loadDesc.pVertexLayout = &vertexLayoutDefault;
        addResource(&loadDesc, NULL);
    }

    void addModels()
    {
        vertexLayoutDefault.mBindingCount = 1;
        vertexLayoutDefault.mAttribCount = 3;
        vertexLayoutDefault.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutDefault.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayoutDefault.mAttribs[0].mBinding = 0;
        vertexLayoutDefault.mAttribs[0].mLocation = 0;
        vertexLayoutDefault.mAttribs[0].mOffset = 0;
        vertexLayoutDefault.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayoutDefault.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
        vertexLayoutDefault.mAttribs[1].mBinding = 0;
        vertexLayoutDefault.mAttribs[1].mLocation = 1;
        vertexLayoutDefault.mAttribs[1].mOffset = 3 * sizeof(float);
        vertexLayoutDefault.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayoutDefault.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
        vertexLayoutDefault.mAttribs[2].mBinding = 0;
        vertexLayoutDefault.mAttribs[2].mLocation = 2;
        vertexLayoutDefault.mAttribs[2].mOffset = 3 * sizeof(float) + sizeof(uint32_t);

        for (size_t i = 0; i < MESH_COUNT; i += 1)
        {
            addModel(i);
        }
    }

    void removeModels()
    {
        for (int i = 0; i < MESH_COUNT; ++i)
        {
            removeResource(pMeshes[i]);
            pMeshes[i] = nullptr;
        }
    }

    void addTextures()
    {
        const char* textureNames[TEXTURE_COUNT] = {
            "skybox/hw_sahara/sahara_rt.tex",
            "skybox/hw_sahara/sahara_lf.tex",
            "skybox/hw_sahara/sahara_up.tex",
            "skybox/hw_sahara/sahara_dn.tex",
            "skybox/hw_sahara/sahara_ft.tex",
            "skybox/hw_sahara/sahara_bk.tex",
            "grid.tex",
        };

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            TextureLoadDesc textureDesc = {};
            textureDesc.pFileName = textureNames[i];
            textureDesc.ppTexture = &pTextures[i];
            // Textures representing color should be stored in SRGB or HDR format
            textureDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&textureDesc, NULL);
        }
    }

    void removeTextures()
    {
        for (uint i = 0; i < TEXTURE_COUNT; ++i)
            removeResource(pTextures[i]);
    }

    void addUniformBuffers()
    {
        BufferLoadDesc materialUBDesc = {};
        materialUBDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialUBDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        materialUBDesc.mDesc.mSize = sizeof(MaterialUniformBlock);
        materialUBDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        materialUBDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            materialUBDesc.ppBuffer = &pBufferMaterials[i];
            addResource(&materialUBDesc, NULL);
        }

        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mSize = sizeof(ObjectInfoUniformBlock);
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pBufferOpaqueObjectTransforms[i];
            addResource(&ubDesc, NULL);
        }
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pBufferTransparentObjectTransforms[i];
            addResource(&ubDesc, NULL);
        }

        BufferLoadDesc skyboxDesc = {};
        skyboxDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        skyboxDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        skyboxDesc.mDesc.mSize = sizeof(SkyboxUniformBlock);
        skyboxDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        skyboxDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            skyboxDesc.ppBuffer = &pBufferSkyboxUniform[i];
            addResource(&skyboxDesc, NULL);
        }

        BufferLoadDesc camUniDesc = {};
        camUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        camUniDesc.mDesc.mSize = sizeof(CameraUniform);
        camUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            camUniDesc.ppBuffer = &pBufferCameraUniform[i];
            addResource(&camUniDesc, NULL);
        }

        BufferLoadDesc camLightUniDesc = {};
        camLightUniDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        camLightUniDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        camLightUniDesc.mDesc.mSize = sizeof(CameraLightUniform);
        camLightUniDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            camLightUniDesc.ppBuffer = &pBufferCameraLightUniform[i];
            addResource(&camLightUniDesc, NULL);
        }

        BufferLoadDesc lightUniformDesc = {};
        lightUniformDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightUniformDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        lightUniformDesc.mDesc.mSize = sizeof(LightUniformBlock);
        lightUniformDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        lightUniformDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            lightUniformDesc.ppBuffer = &pBufferLightUniform[i];
            addResource(&lightUniformDesc, NULL);
        }
        BufferLoadDesc wboitSettingsDesc = {};
        wboitSettingsDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        wboitSettingsDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        wboitSettingsDesc.mDesc.mSize = max(sizeof(WBOITSettings), sizeof(WBOITVolitionSettings));
        wboitSettingsDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        wboitSettingsDesc.pData = NULL;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            wboitSettingsDesc.ppBuffer = &pBufferWBOITSettings[i];
            addResource(&wboitSettingsDesc, NULL);
        }
    }

    void removeUniformBuffers()
    {
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
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
    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);

        // 1/11/24 - Modified from linear to SRGB.
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mColorClearValue = { { 1, 0, 1, 1 } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    bool addRenderTargets() const
    {
        const uint32_t width = mSettings.mWidth;
        const uint32_t height = mSettings.mHeight;

        const ClearValue depthClear = { { 0.0f, 0 } };
        const ClearValue colorClearBlack = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        const ClearValue colorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };
        const ClearValue colorClearTransparentWhite = { { 1.0f, 1.0f, 1.0f, 0.0f } };
        /************************************************************************/
        // Main depth buffer
        /************************************************************************/
        RenderTargetDesc depthRT = {};
        depthRT.mArraySize = 1;
        depthRT.mClearValue = depthClear;
        depthRT.mDepth = 1;
        depthRT.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mWidth = width;
        depthRT.mHeight = height;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
        depthRT.pName = "Depth RT";
        addRenderTarget(pRenderer, &depthRT, &pRenderTargetDepth);
#if PT_USE_DIFFUSION != 0
        depthRT.mFormat = TinyImageFormat_R32_SFLOAT;
        depthRT.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthRT.pName = "Depth RT PT";
        addRenderTarget(pRenderer, &depthRT, &pRenderTargetPTDepthCopy);
#endif

        /************************************************************************/
        // WBOIT render targets
        /************************************************************************/
        ClearValue  wboitClearValues[] = { colorClearBlack, colorClearWhite };
        const char* wboitNames[] = { "Accumulation RT", "Revealage RT" };
        for (int i = 0; i < WBOIT_RT_COUNT; ++i)
        {
            RenderTargetDesc renderTargetDesc = {};
            renderTargetDesc.mArraySize = 1;
            renderTargetDesc.mClearValue = wboitClearValues[i];
            renderTargetDesc.mDepth = 1;
            renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTargetDesc.mFormat = gWBOITRenderTargetFormats[i];
            renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            renderTargetDesc.mWidth = width;
            renderTargetDesc.mHeight = height;
            renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
            renderTargetDesc.mSampleQuality = 0;
            renderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            renderTargetDesc.pName = wboitNames[i];
            addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetWBOIT[i]);
        }

        /************************************************************************/
        // PT render targets
        /************************************************************************/
        ClearValue  ptClearValues[] = { colorClearBlack, colorClearTransparentWhite, colorClearBlack };
        const char* ptNames[] = { "Accumulation RT", "Modulation RT", "Refraction RT" };
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
            renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
            renderTargetDesc.mFormat = gPTRenderTargetFormats[i];
            renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            renderTargetDesc.mWidth = width;
            renderTargetDesc.mHeight = height;
            renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
            renderTargetDesc.mSampleQuality = 0;
            renderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            renderTargetDesc.pName = ptNames[i];
            addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPT[i]);
        }

        {
            RenderTargetDesc renderTargetDesc = {};
            renderTargetDesc.mArraySize = 1;
            renderTargetDesc.mClearValue = pSwapChain->ppRenderTargets[0]->mClearValue;
            renderTargetDesc.mDepth = 1;
            renderTargetDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
            renderTargetDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            renderTargetDesc.mWidth = width;
            renderTargetDesc.mHeight = height;
            renderTargetDesc.mSampleCount = SAMPLE_COUNT_1;
            renderTargetDesc.mSampleQuality = 0;
            renderTargetDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
            renderTargetDesc.mMipLevels = (uint)log2(width);
            renderTargetDesc.pName = "PT Background RT";
            renderTargetDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            addRenderTarget(pRenderer, &renderTargetDesc, &pRenderTargetPTBackground);
        }

        if (gGpuSettings.mEnableAOIT)
        {
            // Create AOIT resources
            TextureDesc aoitClearMaskTextureDesc = {};
            aoitClearMaskTextureDesc.mFormat = TinyImageFormat_R32_UINT;
            aoitClearMaskTextureDesc.mWidth = mSettings.mWidth;
            aoitClearMaskTextureDesc.mHeight = mSettings.mHeight;
            aoitClearMaskTextureDesc.mDepth = 1;
            aoitClearMaskTextureDesc.mArraySize = 1;
            aoitClearMaskTextureDesc.mSampleCount = SAMPLE_COUNT_1;
            aoitClearMaskTextureDesc.mSampleQuality = 0;
            aoitClearMaskTextureDesc.mMipLevels = 1;
            aoitClearMaskTextureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            aoitClearMaskTextureDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
            aoitClearMaskTextureDesc.mFlags = TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
            aoitClearMaskTextureDesc.pName = "AOIT Clear Mask";

            TextureLoadDesc aoitClearMaskTextureLoadDesc = {};
            aoitClearMaskTextureLoadDesc.pDesc = &aoitClearMaskTextureDesc;
            aoitClearMaskTextureLoadDesc.ppTexture = &pTextureAOITClearMask;
            addResource(&aoitClearMaskTextureLoadDesc, NULL);

#if AOIT_NODE_COUNT != 2
            BufferLoadDesc aoitDepthDataLoadDesc = {};
            aoitDepthDataLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            aoitDepthDataLoadDesc.mDesc.mFormat = TinyImageFormat_UNDEFINED;
            aoitDepthDataLoadDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight * pSwapChain->ppRenderTargets[0]->mArraySize;
            aoitDepthDataLoadDesc.mDesc.mStructStride = sizeof(uint32_t) * 4 * AOIT_RT_COUNT;
            aoitDepthDataLoadDesc.mDesc.mSize = aoitDepthDataLoadDesc.mDesc.mElementCount * aoitDepthDataLoadDesc.mDesc.mStructStride;
            aoitDepthDataLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
            aoitDepthDataLoadDesc.mDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            aoitDepthDataLoadDesc.mDesc.pName = "AOIT Depth Data";
            aoitDepthDataLoadDesc.ppBuffer = &pBufferAOITDepthData;
            addResource(&aoitDepthDataLoadDesc, NULL);
#endif

            BufferLoadDesc aoitColorDataLoadDesc = {};
            aoitColorDataLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            aoitColorDataLoadDesc.mDesc.mFormat = TinyImageFormat_UNDEFINED;
            aoitColorDataLoadDesc.mDesc.mElementCount = mSettings.mWidth * mSettings.mHeight * pSwapChain->ppRenderTargets[0]->mArraySize;
            aoitColorDataLoadDesc.mDesc.mStructStride = sizeof(uint32_t) * 4 * AOIT_RT_COUNT;
            aoitColorDataLoadDesc.mDesc.mSize = aoitColorDataLoadDesc.mDesc.mElementCount * aoitColorDataLoadDesc.mDesc.mStructStride;
            aoitColorDataLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER | DESCRIPTOR_TYPE_BUFFER;
            aoitColorDataLoadDesc.mDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            aoitColorDataLoadDesc.mDesc.pName = "AOIT Color Data";
            aoitColorDataLoadDesc.ppBuffer = &pBufferAOITColorData;
            addResource(&aoitColorDataLoadDesc, NULL);
        }

        return true;
    }

    void removeRenderTargets()
    {
        if (gGpuSettings.mEnableAOIT)
        {
            removeResource(pTextureAOITClearMask);
#if AOIT_NODE_COUNT != 2
            removeResource(pBufferAOITDepthData);
#endif
            removeResource(pBufferAOITColorData);
        }

        removeRenderTarget(pRenderer, pRenderTargetDepth);
#if PT_USE_DIFFUSION != 0
        removeRenderTarget(pRenderer, pRenderTargetPTDepthCopy);
#endif
        for (uint32_t i = 0; i < WBOIT_RT_COUNT; ++i)
        {
            removeRenderTarget(pRenderer, pRenderTargetWBOIT[i]);
        }
        for (uint32_t i = 0; i < PT_RT_COUNT; ++i)
        {
            if (i == PT_RT_ACCUMULATION)
            {
                continue; // Acculuation RT is shared with WBOIT and has already been removed
            }
            removeRenderTarget(pRenderer, pRenderTargetPT[i]);
        }
        removeRenderTarget(pRenderer, pRenderTargetPTBackground);
    }

    void addPipelines()
    {
        // Define vertex layouts
        VertexLayout vertexLayoutSkybox = {};
        vertexLayoutSkybox.mBindingCount = 1;
        vertexLayoutSkybox.mAttribCount = 1;
        vertexLayoutSkybox.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayoutSkybox.mAttribs[0].mFormat = TinyImageFormat_R32G32B32A32_SFLOAT;
        vertexLayoutSkybox.mAttribs[0].mBinding = 0;
        vertexLayoutSkybox.mAttribs[0].mLocation = 0;
        vertexLayoutSkybox.mAttribs[0].mOffset = 0;

        RasterizerStateDesc rasterStateBackDesc = {};
        rasterStateBackDesc.mCullMode = CULL_MODE_BACK;

        RasterizerStateDesc rasterStateFrontDesc = {};
        rasterStateFrontDesc.mCullMode = CULL_MODE_FRONT;

        RasterizerStateDesc rasterStateNoneDesc = {};
        rasterStateNoneDesc.mCullMode = CULL_MODE_NONE;

        DepthStateDesc depthStateEnabledDesc = {};
        depthStateEnabledDesc.mDepthFunc = CMP_LEQUAL;
        depthStateEnabledDesc.mDepthWrite = true;
        depthStateEnabledDesc.mDepthTest = true;

        DepthStateDesc depthStateDisabledDesc = {};
        depthStateDisabledDesc.mDepthWrite = false;
        depthStateDisabledDesc.mDepthTest = false;

        DepthStateDesc reverseDepthStateEnabledDesc = {};
        reverseDepthStateEnabledDesc.mDepthFunc = CMP_GEQUAL;
        reverseDepthStateEnabledDesc.mDepthWrite = true;
        reverseDepthStateEnabledDesc.mDepthTest = true;

        DepthStateDesc reverseDepthStateNoWriteDesc = {};
        reverseDepthStateNoWriteDesc.mDepthFunc = CMP_GEQUAL;
        reverseDepthStateNoWriteDesc.mDepthWrite = false;
        reverseDepthStateNoWriteDesc.mDepthTest = true;

        BlendStateDesc blendStateAlphaDesc = {};
        blendStateAlphaDesc.mSrcFactors[0] = BC_SRC_ALPHA;
        blendStateAlphaDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateAlphaDesc.mBlendModes[0] = BM_ADD;
        blendStateAlphaDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateAlphaDesc.mDstAlphaFactors[0] = BC_ZERO;
        blendStateAlphaDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateAlphaDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateAlphaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateAlphaDesc.mIndependentBlend = false;

        BlendStateDesc blendStateWBOITShadeDesc = {};
        blendStateWBOITShadeDesc.mSrcFactors[0] = BC_ONE;
        blendStateWBOITShadeDesc.mDstFactors[0] = BC_ONE;
        blendStateWBOITShadeDesc.mBlendModes[0] = BM_ADD;
        blendStateWBOITShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateWBOITShadeDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStateWBOITShadeDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateWBOITShadeDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateWBOITShadeDesc.mSrcFactors[1] = BC_ZERO;
        blendStateWBOITShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
        blendStateWBOITShadeDesc.mBlendModes[1] = BM_ADD;
        blendStateWBOITShadeDesc.mSrcAlphaFactors[1] = BC_ZERO;
        blendStateWBOITShadeDesc.mDstAlphaFactors[1] = BC_ONE_MINUS_SRC_ALPHA;
        blendStateWBOITShadeDesc.mBlendAlphaModes[1] = BM_ADD;
        blendStateWBOITShadeDesc.mColorWriteMasks[1] = COLOR_MASK_RED;
        blendStateWBOITShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
        blendStateWBOITShadeDesc.mIndependentBlend = true;

        BlendStateDesc blendStateWBOITVolitionShadeDesc = {};
        blendStateWBOITVolitionShadeDesc.mSrcFactors[0] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mDstFactors[0] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mBlendModes[0] = BM_ADD;
        blendStateWBOITVolitionShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateWBOITVolitionShadeDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateWBOITVolitionShadeDesc.mSrcFactors[1] = BC_ZERO;
        blendStateWBOITVolitionShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
        blendStateWBOITVolitionShadeDesc.mBlendModes[1] = BM_ADD;
        blendStateWBOITVolitionShadeDesc.mSrcAlphaFactors[1] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mDstAlphaFactors[1] = BC_ONE;
        blendStateWBOITVolitionShadeDesc.mBlendAlphaModes[1] = BM_ADD;
        blendStateWBOITVolitionShadeDesc.mColorWriteMasks[1] = COLOR_MASK_RED | COLOR_MASK_ALPHA;
        blendStateWBOITVolitionShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
        blendStateWBOITVolitionShadeDesc.mIndependentBlend = true;

        BlendStateDesc blendStatePTShadeDesc = {};
        blendStatePTShadeDesc.mSrcFactors[0] = BC_ONE;
        blendStatePTShadeDesc.mDstFactors[0] = BC_ONE;
        blendStatePTShadeDesc.mBlendModes[0] = BM_ADD;
        blendStatePTShadeDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStatePTShadeDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStatePTShadeDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStatePTShadeDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStatePTShadeDesc.mSrcFactors[1] = BC_ZERO;
        blendStatePTShadeDesc.mDstFactors[1] = BC_ONE_MINUS_SRC_COLOR;
        blendStatePTShadeDesc.mBlendModes[1] = BM_ADD;
        blendStatePTShadeDesc.mSrcAlphaFactors[1] = BC_ONE;
        blendStatePTShadeDesc.mDstAlphaFactors[1] = BC_ONE;
        blendStatePTShadeDesc.mBlendAlphaModes[1] = BM_ADD;
        blendStatePTShadeDesc.mColorWriteMasks[1] = COLOR_MASK_ALL;
#if PT_USE_REFRACTION != 0
        blendStatePTShadeDesc.mSrcFactors[2] = BC_ONE;
        blendStatePTShadeDesc.mDstFactors[2] = BC_ONE;
        blendStatePTShadeDesc.mBlendModes[2] = BM_ADD;
        blendStatePTShadeDesc.mSrcAlphaFactors[2] = BC_ONE;
        blendStatePTShadeDesc.mDstAlphaFactors[2] = BC_ONE;
        blendStatePTShadeDesc.mBlendAlphaModes[2] = BM_ADD;
        blendStatePTShadeDesc.mColorWriteMasks[2] = COLOR_MASK_RED | COLOR_MASK_GREEN;
        blendStatePTShadeDesc.mRenderTargetMask = BLEND_STATE_TARGET_2;
#endif
        blendStatePTShadeDesc.mRenderTargetMask |= BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1;
        blendStatePTShadeDesc.mIndependentBlend = true;

        BlendStateDesc blendStateAOITShadeaDesc = {};
        blendStateAOITShadeaDesc.mSrcFactors[0] = BC_ONE;
        blendStateAOITShadeaDesc.mDstFactors[0] = BC_SRC_ALPHA;
        blendStateAOITShadeaDesc.mBlendModes[0] = BM_ADD;
        blendStateAOITShadeaDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStateAOITShadeaDesc.mDstAlphaFactors[0] = BC_SRC_ALPHA;
        blendStateAOITShadeaDesc.mBlendAlphaModes[0] = BM_ADD;
        blendStateAOITShadeaDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
        blendStateAOITShadeaDesc.mRenderTargetMask = BLEND_STATE_TARGET_0;
        blendStateAOITShadeaDesc.mIndependentBlend = false;

        // Skybox pipeline
        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& skyboxPipelineDesc = desc.mGraphicsDesc;
        skyboxPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        skyboxPipelineDesc.pShaderProgram = pShaderSkybox;
        skyboxPipelineDesc.pRootSignature = pRootSignatureSkybox;
        skyboxPipelineDesc.mRenderTargetCount = 1;
        skyboxPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        skyboxPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        skyboxPipelineDesc.mSampleQuality = 0;
        skyboxPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        skyboxPipelineDesc.pVertexLayout = &vertexLayoutSkybox;
        skyboxPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        skyboxPipelineDesc.pDepthState = &depthStateDisabledDesc;
        skyboxPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelineSkybox);

        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& skyboxPTPipelineDesc = desc.mGraphicsDesc;
        skyboxPTPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        skyboxPTPipelineDesc.pShaderProgram = pShaderSkybox;
        skyboxPTPipelineDesc.pRootSignature = pRootSignatureSkybox;
        skyboxPTPipelineDesc.mRenderTargetCount = 1;
        skyboxPTPipelineDesc.pColorFormats = &pRenderTargetPTBackground->mFormat;
        skyboxPTPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        skyboxPTPipelineDesc.mSampleQuality = 0;
        skyboxPTPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        skyboxPTPipelineDesc.pVertexLayout = &vertexLayoutSkybox;
        skyboxPTPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        skyboxPTPipelineDesc.pDepthState = &depthStateDisabledDesc;
        skyboxPTPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTSkybox);

#if USE_SHADOWS != 0
        // Shadow pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& shadowPipelineDesc = desc.mGraphicsDesc;
        shadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        shadowPipelineDesc.pShaderProgram = pShaderShadow;
        shadowPipelineDesc.pRootSignature = pRootSignature;
        shadowPipelineDesc.mRenderTargetCount = 1;
        shadowPipelineDesc.pColorFormats = &pRenderTargetShadowVariance[0]->mFormat;
        shadowPipelineDesc.mSampleCount = pRenderTargetShadowVariance[0]->mSampleCount;
        shadowPipelineDesc.mSampleQuality = pRenderTargetShadowVariance[0]->mSampleQuality;
        shadowPipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        shadowPipelineDesc.pVertexLayout = &vertexLayoutDefault;
        shadowPipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        shadowPipelineDesc.pDepthState = &depthStateEnabledDesc;
        shadowPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelineShadow);

        // Gaussian blur pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& blurPipelineDesc = desc.mGraphicsDesc;
        blurPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        blurPipelineDesc.pShaderProgram = pShaderGaussianBlur;
        blurPipelineDesc.pRootSignature = pRootSignatureGaussianBlur;
        blurPipelineDesc.mRenderTargetCount = 1;
        blurPipelineDesc.pColorFormats = &pRenderTargetShadowVariance[0]->mFormat;
        blurPipelineDesc.mSampleCount = pRenderTargetShadowVariance[0]->mSampleCount;
        blurPipelineDesc.mSampleQuality = pRenderTargetShadowVariance[0]->mSampleQuality;
        blurPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        blurPipelineDesc.pVertexLayout = NULL;
        blurPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        blurPipelineDesc.pDepthState = &depthStateDisabledDesc;
        blurPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelineGaussianBlur);

#if PT_USE_CAUSTICS != 0
        blurPipelineDesc.pColorFormats = &pRenderTargetPTShadowFinal[0][0]->mFormat;
        addPipeline(pRenderer, &desc, &pPipelinePTGaussianBlur);
        TinyImageFormat stochasticShadowColorFormats[] = { pRenderTargetPTShadowVariance[0]->mFormat,
                                                           pRenderTargetPTShadowVariance[1]->mFormat,
                                                           pRenderTargetPTShadowVariance[2]->mFormat };

        BlendStateDesc blendStatePTMinDesc = {};
        blendStatePTMinDesc.mSrcFactors[0] = BC_ONE;
        blendStatePTMinDesc.mDstFactors[0] = BC_ONE;
        blendStatePTMinDesc.mBlendModes[0] = BM_MIN;
        blendStatePTMinDesc.mSrcAlphaFactors[0] = BC_ONE;
        blendStatePTMinDesc.mDstAlphaFactors[0] = BC_ONE;
        blendStatePTMinDesc.mBlendAlphaModes[0] = BM_MIN;
        blendStatePTMinDesc.mColorWriteMasks[0] = COLOR_MASK_RED | COLOR_MASK_GREEN;
        blendStatePTMinDesc.mRenderTargetMask = BLEND_STATE_TARGET_0 | BLEND_STATE_TARGET_1 | BLEND_STATE_TARGET_2;
        blendStatePTMinDesc.mIndependentBlend = false;

        // Stochastic shadow pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& stochasticShadowPipelineDesc = desc.mGraphicsDesc;
        stochasticShadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        stochasticShadowPipelineDesc.pShaderProgram = pShaderPTShadow;
        stochasticShadowPipelineDesc.pRootSignature = pRootSignature;
        stochasticShadowPipelineDesc.mRenderTargetCount = 3;
        stochasticShadowPipelineDesc.pColorFormats = stochasticShadowColorFormats;
        stochasticShadowPipelineDesc.mSampleCount = pRenderTargetPTShadowVariance[0]->mSampleCount;
        stochasticShadowPipelineDesc.mSampleQuality = pRenderTargetPTShadowVariance[0]->mSampleQuality;
        stochasticShadowPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        stochasticShadowPipelineDesc.pVertexLayout = &vertexLayoutDefault;
        stochasticShadowPipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        stochasticShadowPipelineDesc.pDepthState = &depthStateDisabledDesc;
        stochasticShadowPipelineDesc.pBlendState = &blendStatePTMinDesc;
        addPipeline(pRenderer, &desc, &pPipelinePTShadow);

        // Downsample shadow pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& downsampleShadowPipelineDesc = desc.mGraphicsDesc;
        downsampleShadowPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        downsampleShadowPipelineDesc.pShaderProgram = pShaderPTDownsample;
        downsampleShadowPipelineDesc.pRootSignature = pRootSignaturePTDownsample;
        downsampleShadowPipelineDesc.mRenderTargetCount = 1;
        downsampleShadowPipelineDesc.pColorFormats = &pRenderTargetPTShadowFinal[0][0]->mFormat;
        downsampleShadowPipelineDesc.mSampleCount = pRenderTargetPTShadowFinal[0][0]->mSampleCount;
        downsampleShadowPipelineDesc.mSampleQuality = pRenderTargetPTShadowFinal[0][0]->mSampleQuality;
        downsampleShadowPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        downsampleShadowPipelineDesc.pVertexLayout = NULL;
        downsampleShadowPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        downsampleShadowPipelineDesc.pDepthState = &depthStateDisabledDesc;
        downsampleShadowPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTDownsample);

        // Copy shadow map pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& copyShadowDepthPipelineDesc = desc.mGraphicsDesc;
        copyShadowDepthPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        copyShadowDepthPipelineDesc.pShaderProgram = pShaderPTCopyShadowDepth;
        copyShadowDepthPipelineDesc.pRootSignature = pRootSignaturePTCopyShadowDepth;
        copyShadowDepthPipelineDesc.mRenderTargetCount = 1;
        copyShadowDepthPipelineDesc.pColorFormats = &pRenderTargetPTShadowVariance[0]->mFormat;
        copyShadowDepthPipelineDesc.mSampleCount = pRenderTargetPTShadowVariance[0]->mSampleCount;
        copyShadowDepthPipelineDesc.mSampleQuality = pRenderTargetPTShadowVariance[0]->mSampleQuality;
        copyShadowDepthPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        copyShadowDepthPipelineDesc.pVertexLayout = NULL;
        copyShadowDepthPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        copyShadowDepthPipelineDesc.pDepthState = &depthStateDisabledDesc;
        copyShadowDepthPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTCopyShadowDepth);

#endif
#endif

        // Forward shading pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& forwardPipelineDesc = desc.mGraphicsDesc;
        forwardPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        forwardPipelineDesc.pShaderProgram = pShaderForward;
        forwardPipelineDesc.pRootSignature = pRootSignature;
        forwardPipelineDesc.mRenderTargetCount = 1;
        forwardPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        forwardPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        forwardPipelineDesc.mSampleQuality = 0;
        forwardPipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        forwardPipelineDesc.pVertexLayout = &vertexLayoutDefault;
        forwardPipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        forwardPipelineDesc.pDepthState = &reverseDepthStateEnabledDesc;
        forwardPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelineForward);

        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& forwardPTPipelineDesc = desc.mGraphicsDesc;
        forwardPTPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        forwardPTPipelineDesc.pShaderProgram = pShaderForward;
        forwardPTPipelineDesc.pRootSignature = pRootSignature;
        forwardPTPipelineDesc.mRenderTargetCount = 1;
        forwardPTPipelineDesc.pColorFormats = &pRenderTargetPTBackground->mFormat;
        forwardPTPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        forwardPTPipelineDesc.mSampleQuality = 0;
        forwardPTPipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        forwardPTPipelineDesc.pVertexLayout = &vertexLayoutDefault;
        forwardPTPipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        forwardPTPipelineDesc.pDepthState = &reverseDepthStateEnabledDesc;
        forwardPTPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTForward);

        // Transparent forward shading pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& transparentForwardPipelineDesc = desc.mGraphicsDesc;
        transparentForwardPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        transparentForwardPipelineDesc.pShaderProgram = pShaderForward;
        transparentForwardPipelineDesc.pRootSignature = pRootSignature;
        transparentForwardPipelineDesc.mRenderTargetCount = 1;
        transparentForwardPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        transparentForwardPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        transparentForwardPipelineDesc.mSampleQuality = 0;
        transparentForwardPipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        transparentForwardPipelineDesc.pVertexLayout = &vertexLayoutDefault;
        transparentForwardPipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        transparentForwardPipelineDesc.pDepthState = &reverseDepthStateNoWriteDesc;
        transparentForwardPipelineDesc.pBlendState = &blendStateAlphaDesc;
        addPipeline(pRenderer, &desc, &pPipelineTransparentForwardFront);

        transparentForwardPipelineDesc.pRasterizerState = &rasterStateBackDesc;
        addPipeline(pRenderer, &desc, &pPipelineTransparentForwardBack);

        // WBOIT shading pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& wboitShadePipelineDesc = desc.mGraphicsDesc;
        wboitShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        wboitShadePipelineDesc.pShaderProgram = pShaderWBOITShade;
        wboitShadePipelineDesc.pRootSignature = pRootSignature;
        wboitShadePipelineDesc.mRenderTargetCount = WBOIT_RT_COUNT;
        wboitShadePipelineDesc.pColorFormats = gWBOITRenderTargetFormats;
        wboitShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        wboitShadePipelineDesc.mSampleQuality = 0;
        wboitShadePipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        wboitShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
        wboitShadePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        wboitShadePipelineDesc.pDepthState = &reverseDepthStateNoWriteDesc;
        wboitShadePipelineDesc.pBlendState = &blendStateWBOITShadeDesc;
        addPipeline(pRenderer, &desc, &pPipelineWBOITShade);

        // WBOIT composite pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& wboitCompositePipelineDesc = desc.mGraphicsDesc;
        wboitCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        wboitCompositePipelineDesc.pShaderProgram = pShaderWBOITComposite;
        wboitCompositePipelineDesc.pRootSignature = pRootSignatureWBOITComposite;
        wboitCompositePipelineDesc.mRenderTargetCount = 1;
        wboitCompositePipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        wboitCompositePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        wboitCompositePipelineDesc.mSampleQuality = 0;
        wboitCompositePipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        wboitCompositePipelineDesc.pVertexLayout = NULL;
        wboitCompositePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        wboitCompositePipelineDesc.pDepthState = &depthStateDisabledDesc;
        wboitCompositePipelineDesc.pBlendState = &blendStateAlphaDesc;
        addPipeline(pRenderer, &desc, &pPipelineWBOITComposite);

        // WBOIT Volition shading pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& wboitVolitionShadePipelineDesc = desc.mGraphicsDesc;
        wboitVolitionShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        wboitVolitionShadePipelineDesc.pShaderProgram = pShaderWBOITVShade;
        wboitVolitionShadePipelineDesc.pRootSignature = pRootSignature;
        wboitVolitionShadePipelineDesc.mRenderTargetCount = WBOIT_RT_COUNT;
        wboitVolitionShadePipelineDesc.pColorFormats = gWBOITRenderTargetFormats;
        wboitVolitionShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        wboitVolitionShadePipelineDesc.mSampleQuality = 0;
        wboitVolitionShadePipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        wboitVolitionShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
        wboitVolitionShadePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        wboitVolitionShadePipelineDesc.pDepthState = &reverseDepthStateNoWriteDesc;
        wboitVolitionShadePipelineDesc.pBlendState = &blendStateWBOITVolitionShadeDesc;
        addPipeline(pRenderer, &desc, &pPipelineWBOITVShade);

        // WBOIT Volition composite pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& wboitVolitionCompositePipelineDesc = desc.mGraphicsDesc;
        wboitVolitionCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        wboitVolitionCompositePipelineDesc.pShaderProgram = pShaderWBOITVComposite;
        wboitVolitionCompositePipelineDesc.pRootSignature = pRootSignatureWBOITComposite;
        wboitVolitionCompositePipelineDesc.mRenderTargetCount = 1;
        wboitVolitionCompositePipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        wboitVolitionCompositePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        wboitVolitionCompositePipelineDesc.mSampleQuality = 0;
        wboitVolitionCompositePipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        wboitVolitionCompositePipelineDesc.pVertexLayout = NULL;
        wboitVolitionCompositePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        wboitVolitionCompositePipelineDesc.pDepthState = &depthStateDisabledDesc;
        wboitVolitionCompositePipelineDesc.pBlendState = &blendStateAlphaDesc;
        addPipeline(pRenderer, &desc, &pPipelineWBOITVComposite);

        // PT shading pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& ptShadePipelineDesc = desc.mGraphicsDesc;
        ptShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        ptShadePipelineDesc.pShaderProgram = pShaderPTShade;
        ptShadePipelineDesc.pRootSignature = pRootSignature;
        ptShadePipelineDesc.mRenderTargetCount = PT_RT_COUNT;
        ptShadePipelineDesc.pColorFormats = gPTRenderTargetFormats;
        ptShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        ptShadePipelineDesc.mSampleQuality = 0;
        ptShadePipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
        ptShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
        ptShadePipelineDesc.pRasterizerState = &rasterStateFrontDesc;
        ptShadePipelineDesc.pDepthState = &reverseDepthStateNoWriteDesc;
        ptShadePipelineDesc.pBlendState = &blendStatePTShadeDesc;
        addPipeline(pRenderer, &desc, &pPipelinePTShade);

        // PT composite pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& ptCompositePipelineDesc = desc.mGraphicsDesc;
        ptCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        ptCompositePipelineDesc.pShaderProgram = pShaderPTComposite;
        ptCompositePipelineDesc.pRootSignature = pRootSignaturePTComposite;
        ptCompositePipelineDesc.mRenderTargetCount = 1;
        ptCompositePipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        ptCompositePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        ptCompositePipelineDesc.mSampleQuality = 0;
        ptCompositePipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        ptCompositePipelineDesc.pVertexLayout = NULL;
        ptCompositePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        ptCompositePipelineDesc.pDepthState = &depthStateDisabledDesc;
        ptCompositePipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTComposite);

#if PT_USE_DIFFUSION != 0
        TinyImageFormat ptCopyDepthFormat = pRenderTargetPTDepthCopy->mFormat;

        // PT copy depth pipeline
        desc.mGraphicsDesc = {};
        GraphicsPipelineDesc& ptCopyDepthPipelineDesc = desc.mGraphicsDesc;
        ptCopyDepthPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        ptCopyDepthPipelineDesc.pShaderProgram = pShaderPTCopyDepth;
        ptCopyDepthPipelineDesc.pRootSignature = pRootSignaturePTCopyDepth;
        ptCopyDepthPipelineDesc.mRenderTargetCount = 1;
        ptCopyDepthPipelineDesc.pColorFormats = &ptCopyDepthFormat;
        ptCopyDepthPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
        ptCopyDepthPipelineDesc.mSampleQuality = 0;
        ptCopyDepthPipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
        ptCopyDepthPipelineDesc.pVertexLayout = NULL;
        ptCopyDepthPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
        ptCopyDepthPipelineDesc.pDepthState = &depthStateDisabledDesc;
        ptCopyDepthPipelineDesc.pBlendState = NULL;
        addPipeline(pRenderer, &desc, &pPipelinePTCopyDepth);

        // PT generate mips pipeline
        desc.mType = PIPELINE_TYPE_COMPUTE;
        desc.mComputeDesc = {};
        ComputePipelineDesc& ptGenMipsPipelineDesc = desc.mComputeDesc;
        ptGenMipsPipelineDesc.pShaderProgram = pShaderPTGenMips;
        ptGenMipsPipelineDesc.pRootSignature = pRootSignaturePTGenMips;
        addPipeline(pRenderer, &desc, &pPipelinePTGenMips);
#endif
        if (gGpuSettings.mEnableAOIT)
        {
            // AOIT shading pipeline
            desc.mType = PIPELINE_TYPE_GRAPHICS;
            desc.mGraphicsDesc = {};
            GraphicsPipelineDesc& aoitShadePipelineDesc = desc.mGraphicsDesc;
            aoitShadePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            aoitShadePipelineDesc.pShaderProgram = pShaderAOITShade;
            aoitShadePipelineDesc.pRootSignature = pRootSignatureAOITShade;
            aoitShadePipelineDesc.mRenderTargetCount = 0;
            aoitShadePipelineDesc.pColorFormats = NULL;
            aoitShadePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
            aoitShadePipelineDesc.mSampleQuality = 0;
            aoitShadePipelineDesc.mDepthStencilFormat = TinyImageFormat_D32_SFLOAT;
            aoitShadePipelineDesc.pVertexLayout = &vertexLayoutDefault;
            aoitShadePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
            aoitShadePipelineDesc.pDepthState = &reverseDepthStateNoWriteDesc;
            aoitShadePipelineDesc.pBlendState = NULL;
            addPipeline(pRenderer, &desc, &pPipelineAOITShade);

            // AOIT composite pipeline
            desc.mGraphicsDesc = {};
            GraphicsPipelineDesc& aoitCompositePipelineDesc = desc.mGraphicsDesc;
            aoitCompositePipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            aoitCompositePipelineDesc.pShaderProgram = pShaderAOITComposite;
            aoitCompositePipelineDesc.pRootSignature = pRootSignatureAOITComposite;
            aoitCompositePipelineDesc.mRenderTargetCount = 1;
            aoitCompositePipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
            aoitCompositePipelineDesc.mSampleCount = SAMPLE_COUNT_1;
            aoitCompositePipelineDesc.mSampleQuality = 0;
            aoitCompositePipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
            aoitCompositePipelineDesc.pVertexLayout = NULL;
            aoitCompositePipelineDesc.pRasterizerState = &rasterStateNoneDesc;
            aoitCompositePipelineDesc.pDepthState = &depthStateDisabledDesc;
            aoitCompositePipelineDesc.pBlendState = &blendStateAOITShadeaDesc;
            addPipeline(pRenderer, &desc, &pPipelineAOITComposite);

            // AOIT clear pipeline
            desc.mGraphicsDesc = {};
            GraphicsPipelineDesc& aoitClearPipelineDesc = desc.mGraphicsDesc;
            aoitClearPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            aoitClearPipelineDesc.pShaderProgram = pShaderAOITClear;
            aoitClearPipelineDesc.pRootSignature = pRootSignatureAOITClear;
            aoitClearPipelineDesc.mRenderTargetCount = 0;
            aoitClearPipelineDesc.pColorFormats = NULL;
            aoitClearPipelineDesc.mSampleCount = SAMPLE_COUNT_1;
            aoitClearPipelineDesc.mSampleQuality = 0;
            aoitClearPipelineDesc.mDepthStencilFormat = pRenderTargetDepth->mFormat;
            aoitClearPipelineDesc.pVertexLayout = NULL;
            aoitClearPipelineDesc.pRasterizerState = &rasterStateNoneDesc;
            aoitClearPipelineDesc.pDepthState = &depthStateDisabledDesc;
            aoitClearPipelineDesc.pBlendState = NULL;
            addPipeline(pRenderer, &desc, &pPipelineAOITClear);
        }
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineSkybox);
        removePipeline(pRenderer, pPipelinePTSkybox);
#if USE_SHADOWS != 0
        removePipeline(pRenderer, pPipelineShadow);
        removePipeline(pRenderer, pPipelineGaussianBlur);
#if PT_USE_CAUSTICS != 0
        removePipeline(pRenderer, pPipelinePTGaussianBlur);
        removePipeline(pRenderer, pPipelinePTShadow);
        removePipeline(pRenderer, pPipelinePTDownsample);
        removePipeline(pRenderer, pPipelinePTCopyShadowDepth);
#endif
#endif
        removePipeline(pRenderer, pPipelineForward);
        removePipeline(pRenderer, pPipelinePTForward);
        removePipeline(pRenderer, pPipelineTransparentForwardFront);
        removePipeline(pRenderer, pPipelineTransparentForwardBack);
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
        if (gGpuSettings.mEnableAOIT)
        {
            removePipeline(pRenderer, pPipelineAOITShade);
            removePipeline(pRenderer, pPipelineAOITComposite);
            removePipeline(pRenderer, pPipelineAOITClear);
        }
    }
};

void GuiController::UpdateDynamicUI()
{
    if ((int)gTransparencyType != GuiController::currentTransparencyType)
    {
        if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
            uiHideDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, pGuiWindow);
        else if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
            uiHideDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, pGuiWindow);
        else if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
            uiHideDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, pGuiWindow);
        else if (GuiController::currentTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT)
            uiHideDynamicWidgets(&GuiController::AOITNotSupportedDynamicWidgets, pGuiWindow);

        if (gTransparencyType == TRANSPARENCY_TYPE_ALPHA_BLEND)
            uiShowDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, pGuiWindow);
        else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT)
            uiShowDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, pGuiWindow);
        else if (gTransparencyType == TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION)
            uiShowDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, pGuiWindow);
        if (gTransparencyType == TRANSPARENCY_TYPE_ADAPTIVE_OIT && !gGpuSettings.mEnableAOIT)
            uiShowDynamicWidgets(&GuiController::AOITNotSupportedDynamicWidgets, pGuiWindow);

        GuiController::currentTransparencyType = (TransparencyType)gTransparencyType;
    }
}

void GuiController::AddGui()
{
    static const char* transparencyTypeNames[] = { "Alpha blended", "(WBOIT) Weighted blended order independent transparency",
                                                   "(WBOIT) Weighted blended order independent transparency - Volition",
                                                   "(PT) Phenomenological transparency", "(AOIT) Adaptive order independent transparency" };

    uint32_t dropDownCount = sizeof(transparencyTypeNames) / sizeof(transparencyTypeNames[0]);

    DropdownWidget ddTestScripts;
    ddTestScripts.pData = &gCurrentScriptIndex;
    ddTestScripts.pNames = gTestScripts;
    ddTestScripts.mCount = sizeof(gTestScripts) / sizeof(gTestScripts[0]);
    luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Test Scripts", &ddTestScripts, WIDGET_TYPE_DROPDOWN));

    ButtonWidget bRunScript;
    UIWidget*    pRunScript = uiCreateComponentWidget(pGuiWindow, "Run", &bRunScript, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pRunScript, nullptr, RunScript);
    luaRegisterWidget(pRunScript);

    // TRANSPARENCY_TYPE_ADAPTIVE_OIT support widget
    {
        ColorLabelWidget notSupportedLabel;
        notSupportedLabel.mColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::AOITNotSupportedDynamicWidgets,
                                                 "GPU and API configuration does not support AOIT.", &notSupportedLabel,
                                                 WIDGET_TYPE_COLOR_LABEL));
    }

    DropdownWidget ddTransparency;
    ddTransparency.pData = &gTransparencyType;
    ddTransparency.pNames = transparencyTypeNames;
    ddTransparency.mCount = gGpuSettings.mEnableAOIT ? dropDownCount : dropDownCount - 1;
    luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Transparency Type", &ddTransparency, WIDGET_TYPE_DROPDOWN));
    // TRANSPARENCY_TYPE_ALPHA_BLEND Widgets
    {
        LabelWidget blendLabel;
        luaRegisterWidget(
            uiCreateDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, "Blend Settings", &blendLabel, WIDGET_TYPE_LABEL));

        CheckboxWidget checkbox;
        checkbox.pData = &gAlphaBlendSettings.mSortObjects;
        luaRegisterWidget(
            uiCreateDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, "Sort Objects", &checkbox, WIDGET_TYPE_CHECKBOX));

        checkbox.pData = &gAlphaBlendSettings.mSortParticles;
        luaRegisterWidget(
            uiCreateDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, "Sort Particles", &checkbox, WIDGET_TYPE_CHECKBOX));
    }
    // TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT Widgets
    {
        LabelWidget blendLabel;
        luaRegisterWidget(
            uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Blend Settings", &blendLabel, WIDGET_TYPE_LABEL));

        SliderFloatWidget floatSlider;
        floatSlider.pData = &gWBOITSettingsData.mColorResistance;
        floatSlider.mMin = 1.0f;
        floatSlider.mMax = 25.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Color Resistance", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITSettingsData.mRangeAdjustment;
        floatSlider.mMin = 0.0f;
        floatSlider.mMax = 1.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Range Adjustment", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITSettingsData.mDepthRange;
        floatSlider.mMin = 0.1f;
        floatSlider.mMax = 500.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Depth Range", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITSettingsData.mOrderingStrength;
        floatSlider.mMin = 0.1f;
        floatSlider.mMax = 25.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Ordering Strength", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITSettingsData.mUnderflowLimit;
        floatSlider.mMin = 1e-4f;
        floatSlider.mMax = 1e-1f;
        floatSlider.mStep = 1e-4f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Underflow Limit", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITSettingsData.mOverflowLimit;
        floatSlider.mMin = 3e1f;
        floatSlider.mMax = 3e4f;
        floatSlider.mStep = 0.01f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Overflow Limit", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        ButtonWidget resetButton;
        UIWidget*    pResetButton =
            uiCreateDynamicWidgets(&GuiController::weightedBlendedOitDynamicWidgets, "Reset", &resetButton, WIDGET_TYPE_BUTTON);
        WidgetCallback resetCallback = ([](void* pUserData) { UNREF_PARAM(pUserData); gWBOITSettingsData = WBOITSettings(); });
        uiSetWidgetOnEditedCallback(pResetButton, nullptr, resetCallback);
        luaRegisterWidget(pResetButton);
    }
    // TRANSPARENCY_TYPE_WEIGHTED_BLENDED_OIT_VOLITION Widgets
    {
        LabelWidget blendLabel;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Blend Settings", &blendLabel,
                                                 WIDGET_TYPE_LABEL));

        SliderFloatWidget floatSlider;
        floatSlider.pData = &gWBOITVolitionSettingsData.mOpacitySensitivity;
        floatSlider.mMin = 1.0f;
        floatSlider.mMax = 25.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Opacity Sensitivity",
                                                 &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mWeightBias;
        floatSlider.mMin = 0.0f;
        floatSlider.mMax = 25.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Weight Bias", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mPrecisionScalar;
        floatSlider.mMin = 100.0f;
        floatSlider.mMax = 100000.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Precision Scalar", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mMaximumWeight;
        floatSlider.mMin = 0.1f;
        floatSlider.mMax = 100.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Maximum Weight", &floatSlider,
                                                 WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mMaximumColorValue;
        floatSlider.mMin = 100.0f;
        floatSlider.mMax = 10000.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Maximum Color Value",
                                                 &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mAdditiveSensitivity;
        floatSlider.mMin = 0.1f;
        floatSlider.mMax = 25.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Additive Sensitivity",
                                                 &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

        floatSlider.pData = &gWBOITVolitionSettingsData.mEmissiveSensitivity;
        floatSlider.mMin = 0.01f;
        floatSlider.mMax = 1.0f;
        luaRegisterWidget(uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Emissive Sensitivity",
                                                 &floatSlider, WIDGET_TYPE_SLIDER_FLOAT));

        ButtonWidget resetButton;
        UIWidget*    pResetButton =
            uiCreateDynamicWidgets(&GuiController::weightedBlendedOitVolitionDynamicWidgets, "Reset", &resetButton, WIDGET_TYPE_BUTTON);
        WidgetCallback resetCallback = ([](void* pUserData) { UNREF_PARAM(pUserData); gWBOITVolitionSettingsData = WBOITVolitionSettings(); });
        uiSetWidgetOnEditedCallback(pResetButton, nullptr, resetCallback);
        luaRegisterWidget(pResetButton);
    }

    LabelWidget settingsLabel;
    luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Settings", &settingsLabel, WIDGET_TYPE_LABEL));

    const float3       lightPosBound(10.0f);
    SliderFloat3Widget lightPosSlider;
    lightPosSlider.pData = &gLightCpuSettings.mLightPosition;
    lightPosSlider.mMin = -lightPosBound;
    lightPosSlider.mMax = lightPosBound;
    lightPosSlider.mStep = float3(0.1f);
    luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Light Position", &lightPosSlider, WIDGET_TYPE_SLIDER_FLOAT3));

    GuiController::currentTransparencyType = TRANSPARENCY_TYPE_ALPHA_BLEND;
    uiShowDynamicWidgets(&GuiController::alphaBlendDynamicWidgets, pGuiWindow);
}

void GuiController::RemoveGui()
{
    uiDestroyDynamicWidgets(&alphaBlendDynamicWidgets);
    uiDestroyDynamicWidgets(&weightedBlendedOitDynamicWidgets);
    uiDestroyDynamicWidgets(&weightedBlendedOitVolitionDynamicWidgets);
    uiDestroyDynamicWidgets(&AOITNotSupportedDynamicWidgets);
}

DEFINE_APPLICATION_MAIN(Transparency)
