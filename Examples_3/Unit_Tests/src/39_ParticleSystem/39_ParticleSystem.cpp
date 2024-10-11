/*
 *
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

/********************************************************************************************************
 *
 * The Forge - PARTICLE SYSTEM UNIT TEST
 *
 * The purpose of this demo is to show GPU-driven particle system of The-Forge.
 *
 *********************************************************************************************************/

#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Renderer/Interfaces/IParticleSystem.h"
#include "../../../../Common_3/Renderer/Interfaces/IVisibilityBuffer.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Tools/Network/Network.h"

#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/RingBuffer.h"
#include "../../../Visibility_Buffer/src/SanMiguel.h"

#include "Shaders/FSL/shader_defs.h.fsl"
#include "../../../../Common_3/Utilities/Math/ShaderUtilities.h"
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h" // Must be the last include in a cpp file

#define SCENE_SCALE 1.0f
#if defined(ANDROID)
#define RESOLUTION_FACTOR 0.5f
#else
#define RESOLUTION_FACTOR 1.0f
#endif

#define DEFAULT_PARTICLE_SETS_COUNT 8
#define PARTICLE_TEXTURES_COUNT     2

#define pack2Floats(x, y)           (half(x).sh | (half(y).sh << 16))

#if defined(ENABLE_REMOTE_STREAMING)
#include "../../../../../Remote-Editor/RemoteServer/IStream.h"
#endif

#define SHADOWMAP_SIZE 2048
#if defined(ORBIS)
#define MAX_TRANSPARENCY_LAYERS 20
#elif (defined(PROSPERO) || defined(WINDOWS)) && defined(AUTOMATED_TESTING)
#define MAX_TRANSPARENCY_LAYERS 64
#else
#define MAX_TRANSPARENCY_LAYERS 24
#endif

#define FOREACH_SETTING(X)       \
    X(BindlessSupported, 1)      \
    X(AddGeometryPassThrough, 0) \
    X(OITSupported, 1)

#define GENERATE_ENUM(x, y)   x,
#define GENERATE_STRING(x, y) #x,
#define GENERATE_STRUCT(x, y) uint32_t m##x = y;

#if defined(ANDROID) || defined(AUTOMATED_TESTING)
#define DEFAULT_ASYNC_COMPUTE false
#else
#define DEFAULT_ASYNC_COMPUTE true
#endif

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

#define CUBE_SHADOWS_FACE_SIZE 128

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;
uint2          gAppResolution;

/*****************************************************/
/****************Particle System data*****************/
/*****************************************************/

// Particle system constant buffers
ParticleConstantBufferData pParticleSystemConstantData[gDataBufferCount];
Buffer*                    pParticleConstantBuffer[gDataBufferCount];

// Particle system buffers
Buffer* pParticlesBuffer = NULL;
Buffer* pParticleBitfields = NULL;
Buffer* pParticleSetsBuffer = NULL;
Buffer* pTransparencyListBuffer = NULL;
Buffer* pTransparencyListHeadBuffer = NULL;

// Default ParticleSets that are uploaded once to start the simulation and never touched again
#if !defined(ENABLE_REMOTE_STREAMING)
ParticleSet* pParticleSets = NULL;
#endif

// Particle textures
Texture* ppParticleTextures[PARTICLE_TEXTURES_COUNT] = { NULL };

// Depth cubemap for shadows
RenderTarget* pDepthCube = NULL;
// Shadow collector
Texture*      pShadowCollector = NULL;
Texture*      pFilteredShadowCollector = NULL;

/******************************************************/
// Fireflies
/******************************************************/
float gFirefliesFlockRadius = 4.0f;
float gFirefliesMinHeight = 1.5f;
float gFirefliesMaxHeight = 6.0f;

float gFirefliesElevationSpeed = 0.3f;
float gFirefliesWhirlSpeed = 0.3f;

float gFirefliesWhirlAngle = 0.5f;
float gFirefliesElevationT = 0.5f;

/******************************************************/
// Shadowmap filtering
/******************************************************/
Pipeline*      pPipelineFilterShadows = NULL;
Shader*        pShaderFilterShadows = NULL;
RootSignature* pRootSignatureFilterShadows = NULL;
DescriptorSet* pDescriptorSetFilterShadows = NULL;

float2 gSunControl = { -1.556f, -0.58f };
float  gESMControl = 50.0f;

uint32_t gShadowPushConstsIndex = 0;
uint32_t gShadowFilteringPushConstsIndex = 0;

UIComponent* pGuiWindow = NULL;
float4       gTextColor = { 1, 1, 1, 1 };

// True on the first frame
bool gJustLoaded = true;
bool gAsyncComputeEnabled = DEFAULT_ASYNC_COMPUTE;
// True after 10 seconds since the start of the app
bool gParticlesLoaded = false;

#if defined(FORGE_DEBUG)
bstring gParticleStatsText = bempty();
#endif

/************************************************************************/
// Per frame staging data
/************************************************************************/
struct PerFrameData
{
    // Stores the camera/eye position in object space for cluster culling
    vec3                    gEyeObjectSpace[NUM_CULLING_VIEWPORTS] = {};
    PerFrameConstantsData   gPerFrameUniformData = {};
    PerFrameVBConstantsData gPerFrameVBUniformData = {};

    uint32_t gDrawCount[NUM_GEOMETRY_SETS] = {};
};

/************************************************************************/
// Profiling
/************************************************************************/
ProfileToken      gGraphicsProfileToken;
ProfileToken      gComputeProfileToken;
/************************************************************************/
// Rendering data
/************************************************************************/
Renderer*         pRenderer = NULL;
VisibilityBuffer* pVisibilityBuffer = NULL;
/************************************************************************/
// Queues and Command buffers
/************************************************************************/
Queue*            pComputeQueue = NULL;
Queue*            pGraphicsQueue = NULL;
GpuCmdRing        gGraphicsCmdRing = {};
GpuCmdRing        gComputeCmdRing = {};
/************************************************************************/
// Swapchain
/************************************************************************/
SwapChain*        pSwapChain = NULL;
Semaphore*        pImageAcquiredSemaphore = NULL;
Semaphore*        pPresentSemaphore = NULL;
Semaphore*        gPrevGraphicsSemaphore = NULL;
Semaphore*        gComputeSemaphores[gDataBufferCount] = {};
/************************************************************************/
// Clear buffers pipeline
/************************************************************************/
Shader*           pShaderClearBuffers = NULL;
Pipeline*         pPipelineClearBuffers = NULL;
RootSignature*    pRootSignatureClearBuffers = NULL;
DescriptorSet*    pDescriptorSetClearBuffers = NULL;
/************************************************************************/
// Triangle filtering pipeline
/************************************************************************/
Shader*           pShaderTriangleFiltering = NULL;
Pipeline*         pPipelineTriangleFiltering = NULL;
RootSignature*    pRootSignatureTriangleFiltering = NULL;
DescriptorSet*    pDescriptorSetTriangleFiltering[2] = { NULL };
/************************************************************************/
// Clear light clusters pipeline
/************************************************************************/
Shader*           pShaderClearLightClusters = NULL;
Pipeline*         pPipelineClearLightClusters = NULL;
RootSignature*    pRootSignatureLightClusters = NULL;
DescriptorSet*    pDescriptorSetLightClusters[2] = { NULL };
/************************************************************************/
// "Downsample" depth buffer pipeline
/************************************************************************/
Shader*           pShaderDownsampleDepth = NULL;
Pipeline*         pPipelineDownsampleDepth = NULL;
DescriptorSet*    pDescriptorSetDownsampleDepth = NULL;
RenderTarget*     pRenderTargetDownsampleDepth = NULL;
RootSignature*    pRootSignatureDownsampleDepth = NULL;
uint32_t          gDownsamplePushConstantsIndex = 0;
/************************************************************************/
// Compute depth bounds pipeline
/************************************************************************/
Shader*           pShaderComputeDepthBounds = NULL;
Shader*           pShaderComputeDepthCluster = NULL;
Pipeline*         pPipelineComputeDepthBounds = NULL;
Pipeline*         pPipelineComputeDepthCluster = NULL;
Buffer*           pDepthBoundsBuffer = NULL;
uint32_t          gDepthBoundsPushConstsIndex = 0;
/************************************************************************/
// Compute light clusters pipeline
/************************************************************************/
Shader*           pShaderClusterLights = NULL;
Pipeline*         pPipelineClusterLights = NULL;
/************************************************************************/
// Shadow pass pipeline
/************************************************************************/
Shader*           pShaderShadowPass[NUM_GEOMETRY_SETS] = { NULL };
Pipeline*         pPipelineShadowPass[NUM_GEOMETRY_SETS] = { NULL };
/************************************************************************/
// VB pass pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferPass[NUM_GEOMETRY_SETS] = {};
Pipeline*         pPipelineVisibilityBufferPass[NUM_GEOMETRY_SETS] = {};
RootSignature*    pRootSignatureVBPass = NULL;
DescriptorSet*    pDescriptorSetVBPass[2] = { NULL };
/************************************************************************/
// VB shade pipeline
/************************************************************************/
Shader*           pShaderVisibilityBufferShade = NULL;
Pipeline*         pPipelineVisibilityBufferShadeSrgb = NULL;
RootSignature*    pRootSignatureVBShade = NULL;
DescriptorSet*    pDescriptorSetVBShade[2] = { NULL };
/************************************************************************/
// Clean texture pipeline
/************************************************************************/
Shader*           pShaderCleanTexture = NULL;
Pipeline*         pPipelineCleanTexture = NULL;
RootSignature*    pRootSignatureCleanTexture = NULL;
DescriptorSet*    pDescriptorSetCleanTexture = NULL;
/************************************************************************/
// Present pipeline
/************************************************************************/
Shader*           pShaderPresent = NULL;
Pipeline*         pPipelinePresent = NULL;
RootSignature*    pRootSignaturePresent = NULL;
DescriptorSet*    pDescriptorSetPresent = NULL;
uint32_t          gPresentPushConstantsIndex = 0;

/************************************************************************/
// Render targets
/************************************************************************/
RenderTarget*    pDepthBuffer = NULL;
RenderTarget*    pRenderTargetVBPass = NULL;
RenderTarget*    pRenderTargetShadow = NULL;
//  Finally, render the screen target to the swapchain target using the FSQ pipeline
RenderTarget*    pScreenRenderTarget = NULL;
/************************************************************************/
// Samplers
/************************************************************************/
Sampler*         pSamplerTrilinearAniso = NULL;
Sampler*         pSamplerBilinear = NULL;
Sampler*         pSamplerPointClamp = NULL;
Sampler*         pSamplerBilinearClamp = NULL;
Sampler*         pPointClampSampler = NULL;
/************************************************************************/
// Bindless texture array
/************************************************************************/
Texture**        gDiffuseMapsStorage = NULL;
Texture**        gNormalMapsStorage = NULL;
Texture**        gSpecularMapsStorage = NULL;
/************************************************************************/
// Vertex buffers for the scene
/************************************************************************/
Geometry*        pGeom = NULL;
VBMeshInstance*  pVBMeshInstances = NULL;
VBPreFilterStats gVBPreFilterStats[gDataBufferCount] = {};
/************************************************************************/
// Indirect buffers
/************************************************************************/
Buffer*          pPerFrameUniformBuffers[gDataBufferCount] = { NULL };
enum
{
    VB_UB_COMPUTE = 0,
    VB_UB_GRAPHICS,
    VB_UB_COUNT
};
Buffer* pPerFrameVBUniformBuffers[VB_UB_COUNT][gDataBufferCount] = {};
// Buffers containing all indirect draw commands per geometry set (no culling)
Buffer* pMeshConstantsBuffer = NULL;

/************************************************************************/
// Other buffers for lighting, point lights,...
/************************************************************************/
Buffer*      pLightClustersCount = NULL;
Buffer*      pLightClusters = NULL;
uint64_t     gFrameCount = 0;
uint32_t     gMeshCount = 0;
uint32_t     gMaterialCount = 0;
FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;
float        gCurrTime = 0.0f;

/************************************************************************/
ICameraController* pCameraController = NULL;
/************************************************************************/
// CPU staging data
/************************************************************************/
PerFrameData       gPerFrame[gDataBufferCount] = {};

/************************************************************************/
// Camera walking
/************************************************************************/
static float gCameraWalkingTime = 0.0f;
float3*      gCameraPathData;
uint         gCameraPoints;
float        gTotalElpasedTime;
bool         gCameraWalking = false;
float        gCameraWalkingSpeed = 1.0;

class Particle_System: public IApp
{
public:
    Particle_System() {}

    bool Init()
    {
        // Camera Walking
        loadCameraPath("cameraPath.txt", gCameraPoints, &gCameraPathData);
        gCameraPoints = (uint)29084 / 2;
        gTotalElpasedTime = (float)gCameraPoints * 0.00833f;

        /************************************************************************/
        // Initialize the Forge renderer with the appropriate parameters.
        /************************************************************************/
        ExtendedSettings extendedSettings = {};
        extendedSettings.mNumSettings = ESettings::Count;
        extendedSettings.pSettings = (uint32_t*)&gGpuSettings;
        extendedSettings.ppSettingNames = gSettingNames;

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.pExtendedSettings = &extendedSettings;
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage("Failed To Initialize renderer!");
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        if (!gGpuSettings.mBindlessSupported)
        {
            ShowUnsupportedMessage("Visibility Buffer does not run on this device. Doesn't support enough bindless texture entries");
            return false;
        }

        if (!gGpuSettings.mOITSupported)
        {
            ShowUnsupportedMessage(
                "Order Independent Transparency does not work on this device. Adreno doesn't support big enough storage buffers.");
            return false;
        }

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_COMPUTE;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pComputeQueue);

        queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pGraphicsQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGraphicsQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 2;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gGraphicsCmdRing);

        cmdRingDesc = {};
        cmdRingDesc.pQueue = pComputeQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &gComputeCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);
        initSemaphore(pRenderer, &pPresentSemaphore);

        /************************************************************************/
        // Initialize helper interfaces (resource loader, profiler)
        /************************************************************************/
        initResourceLoaderInterface(pRenderer);

        // Load fonts
        FontDesc font = {};
        font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
        fntDefineFonts(&font, 1, &gFontID);

        FontSystemDesc fontRenderDesc = {};
        fontRenderDesc.pRenderer = pRenderer;
        initFontSystem(&fontRenderDesc);

        // Initialize Forge User Interface Rendering
        UserInterfaceDesc uiRenderDesc = {};
        uiRenderDesc.pRenderer = pRenderer;
        initUserInterface(&uiRenderDesc);

        // Setup scripts
        LuaScriptDesc scriptDesc = {};
        scriptDesc.pScriptFileName = "39_ParticleSystem/Test_AllSets.lua";
        scriptDesc.pWaitCondition = &gParticlesLoaded;
        luaDefineScripts(&scriptDesc, 1);

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        initProfiler(&profiler);

        // Init Stream Server
#ifdef REMOTE_SERVER
        initStreamServer(pRenderer, pGraphicsQueue, REMOTE_CONNECTION_PORT);
#endif

        gGraphicsProfileToken = initGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");
        gComputeProfileToken = initGpuProfiler(pRenderer, pComputeQueue, "Compute");
        /************************************************************************/
        // Start timing the scene load
        /************************************************************************/
        HiresTimer totalTimer;
        initHiresTimer(&totalTimer);
        /************************************************************************/
        // Setup sampler states
        /************************************************************************/
        // Create sampler for VB render target
        SamplerDesc trilinearDesc = { FILTER_LINEAR,
                                      FILTER_LINEAR,
                                      MIPMAP_MODE_LINEAR,
                                      ADDRESS_MODE_REPEAT,
                                      ADDRESS_MODE_REPEAT,
                                      ADDRESS_MODE_REPEAT,
                                      0.0f,
                                      false,
                                      0.0f,
                                      0.0f,
                                      8.0f };
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

        // Sampler
        SamplerDesc pointSamplerDesc = { FILTER_NEAREST,
                                         FILTER_NEAREST,
                                         MIPMAP_MODE_NEAREST,
                                         ADDRESS_MODE_CLAMP_TO_BORDER,
                                         ADDRESS_MODE_CLAMP_TO_BORDER,
                                         ADDRESS_MODE_CLAMP_TO_BORDER };

        addSampler(pRenderer, &trilinearDesc, &pSamplerTrilinearAniso);
        addSampler(pRenderer, &bilinearDesc, &pSamplerBilinear);
        addSampler(pRenderer, &pointDesc, &pSamplerPointClamp);
        addSampler(pRenderer, &bilinearClampDesc, &pSamplerBilinearClamp);
        addSampler(pRenderer, &pointSamplerDesc, &pPointClampSampler);

        /************************************************************************/
        // Load the scene
        /************************************************************************/
        Scene* loadedScene = LoadGeometry();
        // Finish the resource loading process since the next code depends on the loaded resources
        waitForAllResourceLoads();
        exitSanMiguel(loadedScene);

        HiresTimer setupBuffersTimer;
        initHiresTimer(&setupBuffersTimer);

        /************************************************************************/
        // Load buffers
        /************************************************************************/
        // camera constant buffer
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            BufferLoadDesc bufferLoadDesc = {};
            bufferLoadDesc.mDesc.mSize = sizeof(ParticleConstantBufferData);
            bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            bufferLoadDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            bufferLoadDesc.mDesc.pName = "ParticleConstantBuffer";
            bufferLoadDesc.pData = nullptr;
            bufferLoadDesc.mForceReset = true;
            bufferLoadDesc.ppBuffer = &pParticleConstantBuffer[i];
            addResource(&bufferLoadDesc, nullptr);
        }

        // Buffer containing data for all particles
        BufferLoadDesc bufferLoadDesc = {};
        bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
        bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferLoadDesc.mDesc.mFirstElement = 0;
        bufferLoadDesc.mDesc.mStructStride = sizeof(ParticleData);
        bufferLoadDesc.mDesc.mElementCount = MAX_PARTICLES_COUNT;
        bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * (uint64_t)bufferLoadDesc.mDesc.mElementCount;
        bufferLoadDesc.pData = NULL;
        bufferLoadDesc.mForceReset = false;
        bufferLoadDesc.mDesc.pName = "ParticlesDataBuffer";
        bufferLoadDesc.ppBuffer = &pParticlesBuffer;
        bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        addResource(&bufferLoadDesc, NULL);

        // Buffer containing the bitfields of the particles
        bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
        bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferLoadDesc.mDesc.mStructStride = sizeof(uint32_t);
        bufferLoadDesc.mDesc.mSize = sizeof(uint32_t) * (uint64_t)bufferLoadDesc.mDesc.mElementCount;
        bufferLoadDesc.mDesc.pName = "ParticlesBitfields";
        bufferLoadDesc.ppBuffer = &pParticleBitfields;
        bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        addResource(&bufferLoadDesc, NULL);

#if !defined(ENABLE_REMOTE_STREAMING)
        pParticleSets = (ParticleSet*)tf_malloc(sizeof(ParticleSet) * MAX_PARTICLE_SET_COUNT);
        initDefaultParticleSets(pParticleSets);
        bufferLoadDesc.pData = pParticleSets;
#endif
        bufferLoadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        bufferLoadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferLoadDesc.mDesc.mStructStride = sizeof(ParticleSet);
        bufferLoadDesc.mDesc.mElementCount = MAX_PARTICLE_SET_COUNT;
        bufferLoadDesc.mDesc.mSize = bufferLoadDesc.mDesc.mStructStride * (uint64_t)bufferLoadDesc.mDesc.mElementCount;
        bufferLoadDesc.mDesc.pName = "ParticleSetsBuffer";
        bufferLoadDesc.ppBuffer = &pParticleSetsBuffer;
        bufferLoadDesc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        addResource(&bufferLoadDesc, NULL);
        /************************************************************************/
        // Load textures
        /************************************************************************/
        // Particle textures
        TextureLoadDesc particleTexLoad = {};
        particleTexLoad.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        particleTexLoad.pFileName = "Particles/FireflyTmp.tex";
        particleTexLoad.ppTexture = &ppParticleTextures[0];
        addResource(&particleTexLoad, NULL);
        particleTexLoad.pFileName = "Particles/smoke_01.tex";
        particleTexLoad.ppTexture = &ppParticleTextures[1];
        addResource(&particleTexLoad, NULL);
        waitForAllResourceLoads();

        // Depth bounds buffer
        BufferLoadDesc bufferBoundsLoad = {};
        bufferBoundsLoad.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
        bufferBoundsLoad.mDesc.mElementCount =
            LIGHT_CLUSTER_WIDTH * VERTICAL_SUBCLUSTER_COUNT * LIGHT_CLUSTER_HEIGHT * HORIZONTAL_SUBCLUSTER_COUNT * DEPTH_BOUNDS_ENTRY_SIZE;
        bufferBoundsLoad.mDesc.mStructStride = sizeof(uint);
        bufferBoundsLoad.mDesc.mFirstElement = 0;
        bufferBoundsLoad.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        bufferBoundsLoad.mDesc.mSize = bufferBoundsLoad.mDesc.mElementCount * bufferBoundsLoad.mDesc.mStructStride;
        bufferBoundsLoad.mDesc.pName = "DepthBoundsBuffer";
        bufferBoundsLoad.ppBuffer = &pDepthBoundsBuffer;
        bufferBoundsLoad.pData = NULL;
        addResource(&bufferBoundsLoad, NULL);

        /************************************************************************/
        // Setup the fps camera for navigating through the scene
        /************************************************************************/
#if !defined(ENABLE_REMOTE_STREAMING)
        vec3 startPosition(8.765f, 4.544f, 12.593f);
#else
        vec3 startPosition(9.722f, 9.111f, 5.097f);
#endif
        vec3                   startLookAt = vec3(0.0f, 4.0f, 1.0f);
        CameraMotionParameters camParams;
        camParams.acceleration = 100;
        camParams.braking = 800;
        camParams.maxSpeed = 50;
        pCameraController = initFpsCameraController(startPosition, startLookAt);
        pCameraController->setMotionParameters(camParams);

        // App Actions
        AddCustomInputBindings();
        initScreenshotInterface(pRenderer, pGraphicsQueue);
        return true;
    }

#if !defined(ENABLE_REMOTE_STREAMING)
    void initDefaultParticleSets(ParticleSet* particleSets)
    {
        const float gSteeringStrength = 3.1f;
        const float gBoidsSeek = 2.0f;
        const float gBoidsAvoid = 1.0;
        const float gBoidsFlee = 0.0f;
        const float gBoidsSeparation = 0.05f;
        const float gBoidsCohesion = 4.5f;
        const float gBoidsAlignment = 0.25f;

        ParticleSet swarmBase = {};
        swarmBase.Position = float4(8, 6, 11, 0);
        swarmBase.ParticleSetBitfield =
            PARTICLE_BITFIELD_TYPE_BOIDS | PARTICLE_BITFIELD_LIGHTING_MODE_NONE | PARTICLE_BITFIELD_MODULATION_TYPE_SPEED;
#if defined(AUTOMATED_TESTING)
        swarmBase.ParticlesPerSecond = 400000;
        swarmBase.InitialAge = 2.5;
#else
        swarmBase.ParticlesPerSecond = 100000;
        swarmBase.InitialAge = 10.0;
#endif
        swarmBase.BoidsAvoidSeekStrength = pack2Floats(gBoidsAvoid, gBoidsSeek);
        swarmBase.BoidsCohesionAlignmentStrength = pack2Floats(gBoidsCohesion, gBoidsAlignment);
        swarmBase.BoidsSeparationFleeStrength = pack2Floats(gBoidsSeparation, gBoidsFlee);
        swarmBase.SteeringStrengthMinSpeed = pack2Floats(gSteeringStrength, 1.5f);
        swarmBase.SpawnVolume = uint2(pack2Floats(1.0f, 1.0f), pack2Floats(1.0f, 0.001f));
        swarmBase.MaxSizeAndSpeed = pack2Floats(0.014f, 1.5f);
        swarmBase.LightRadiusAndVelocityNoise = 0;
        swarmBase.StartSizeAndTime = pack2Floats(0.0f, 0.3f);
        swarmBase.EndSizeAndTime = pack2Floats(0.0f, 0.7f);
        swarmBase.TextureIndices = 0;
        swarmBase.Allocated = 1;
        swarmBase.AttractorIndex = (uint32_t)-1;
        swarmBase.MinAndMaxAlpha = pack2Floats(1.0f, 1.0f);

        ParticleSet lightSet = {};
        lightSet.ParticleSetBitfield = PARTICLE_BITFIELD_LIGHTING_MODE_LIGHT | PARTICLE_BITFIELD_MODULATION_TYPE_LIFETIME;
        lightSet.StartColor = packUnorm4x8(float4(1.0f, 1.0f, 0.2f, 0.1f));
        lightSet.EndColor = packUnorm4x8(float4(1.0f, 0.2f, 0.0f, 0.9f));
        lightSet.SteeringStrengthMinSpeed = pack2Floats(gSteeringStrength, 0.05f);
        lightSet.InitialAge = 10.0;
        lightSet.LightRadiusAndVelocityNoise = pack2Floats(0.5f, 0.01f);
        lightSet.LightPulseSpeedAndOffset = pack2Floats(1.0f, 0.5f);
        lightSet.SpawnVolume = uint2(pack2Floats(10, 6), pack2Floats(8.0f, 0.002f));
        lightSet.Position = float4(2.0f, 8.0f, 5.0f, 0.0f);
        lightSet.MaxSizeAndSpeed = pack2Floats(0.15f, 0.25f);
        lightSet.ParticlesPerSecond = 1500;
        lightSet.StartSizeAndTime = pack2Floats(0.0f, 0.1f);
        lightSet.EndSizeAndTime = pack2Floats(0.0f, 0.9f);
        lightSet.TextureIndices = 0;
        lightSet.MinAndMaxAlpha = pack2Floats(1.0f, 1.0f);

        for (uint32_t i = 0; i < DEFAULT_PARTICLE_SETS_COUNT; i++)
            particleSets[i] = swarmBase;

        uint32_t particleSetIdx = 0;
        // Red particles
        particleSets[particleSetIdx].Position = float4(6.0f, 6.0f, 10.0f, 0.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(1.0f, 0.1f, 0.0f, 0.2f));
        particleSets[particleSetIdx].EndColor = packUnorm4x8(float4(1.0f, 0.1f, 0.0f, 0.8f));
        particleSets[particleSetIdx].AttractorIndex = 0;
        particleSets[particleSetIdx++].SpawnVolume = uint2(pack2Floats(4.0f, 4.0f), pack2Floats(4.0f, 0.001f));

        // Green particles
        particleSets[particleSetIdx].Position = float4(-6.0f, 6.0f, 10.0f, 0.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(0.5f, 1.0f, 0.0f, 0.1f));
        particleSets[particleSetIdx].EndColor = packUnorm4x8(float4(0.5f, 1.0f, 0.0f, 0.9f));
        particleSets[particleSetIdx++].SteeringStrengthMinSpeed = pack2Floats(0.5f, 1.0f);

        // White particles
        particleSets[particleSetIdx].Position = float4(6.0f, 6.0f, -2.0f, 0.0f);
        particleSets[particleSetIdx].SteeringStrengthMinSpeed = pack2Floats(0.5f, 1.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(1.0f, 1.0f, 1.0f, 0.1f));
        particleSets[particleSetIdx++].EndColor = packUnorm4x8(float4(1.0f, 1.0f, 1.0f, 0.9f));

        // Yellow particles
        particleSets[particleSetIdx].Position = float4(-6.0f, 6.0f, -2.0f, 0.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(1.0f, 1.0f, 0.5f, 0.1f));
        particleSets[particleSetIdx].EndColor = packUnorm4x8(float4(1.0f, 1.0f, 0.5f, 0.9f));
        particleSets[particleSetIdx++].SteeringStrengthMinSpeed = pack2Floats(0.5f, 1.0f);

        // Fireflies
        particleSets[particleSetIdx++] = lightSet;

        // Smoke 1
        particleSets[particleSetIdx].Position = float4(4.0f, 3.0f, 4.0f, 0.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(1.0f, 0.5f, 0.8f, 0.3f));
        particleSets[particleSetIdx].EndColor = packUnorm4x8(float4(0.6f, 0.1f, 0.1f, 0.8f));
        particleSets[particleSetIdx].ParticleSetBitfield = PARTICLE_BITFIELD_MODULATION_TYPE_LIFETIME;
        particleSets[particleSetIdx].ParticlesPerSecond = 2;
        particleSets[particleSetIdx].InitialAge = 20.0f;
        particleSets[particleSetIdx].SteeringStrengthMinSpeed = pack2Floats(0.0f, 0.05f);
        particleSets[particleSetIdx].SpawnVolume = uint2(pack2Floats(4, 1), pack2Floats(4.0f, 1.5f));
        particleSets[particleSetIdx].MaxSizeAndSpeed = pack2Floats(2.0f, 0.1f);
        particleSets[particleSetIdx].StartSizeAndTime = pack2Floats(0.75f, 0.2f);
        particleSets[particleSetIdx].TextureIndices = 1 << 16;
        particleSets[particleSetIdx++].EndSizeAndTime = pack2Floats(0.75f, 0.8f);

        // Smoke 2
        particleSets[particleSetIdx] = particleSets[particleSetIdx - 1];
        particleSets[particleSetIdx].Position = float4(8.0f, 3.0f, 4.0f, 0.0f);
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(0.0f, 0.0f, 1.0f, 0.3f));
        particleSets[particleSetIdx++].EndColor = packUnorm4x8(float4(0.0f, 1.0f, 0.5f, 0.8f));

        // Shadow casting
        particleSets[particleSetIdx] = lightSet;
        particleSets[particleSetIdx].Position = float4(5.0f, 7.0f, 5.0f, 0.0f);
        particleSets[particleSetIdx].ParticleSetBitfield = PARTICLE_BITFIELD_LIGHTING_MODE_LIGHTNSHADOW;
        particleSets[particleSetIdx].StartColor = packUnorm4x8(float4(0.8f, 1.0f, 0.1f, 0.2f));
        particleSets[particleSetIdx].EndColor = packUnorm4x8(float4(0.8f, 1.0f, 0.1f, 0.8f));
        particleSets[particleSetIdx].LightRadiusAndVelocityNoise = pack2Floats(3.0f, 0.0f);
        particleSets[particleSetIdx].ParticlesPerSecond = float(8) / 10.0f;
        particleSets[particleSetIdx].InitialAge = 10.0f;
        particleSets[particleSetIdx].SpawnVolume = uint2(pack2Floats(10.0f, 6.0f), pack2Floats(8.0f, 0.1f));
        particleSets[particleSetIdx].SteeringStrengthMinSpeed = pack2Floats(gSteeringStrength, 1.0f);
        particleSets[particleSetIdx].LightPulseSpeedAndOffset = pack2Floats(0.0f, 1.0f);

        for (uint32_t i = 0; i < DEFAULT_PARTICLE_SETS_COUNT; i++)
            particleSets[i].Allocated = 1;
        for (uint32_t i = DEFAULT_PARTICLE_SETS_COUNT; i < MAX_PARTICLE_SET_COUNT; i++)
            particleSets[i].Allocated = 0;
    }
#endif
    Scene* LoadGeometry()
    {
        HiresTimer sceneLoadTimer;
        initHiresTimer(&sceneLoadTimer);

        SyncToken        token = {};
        GeometryLoadDesc sceneLoadDesc = {};
        sceneLoadDesc.mFlags = GEOMETRY_LOAD_FLAG_SHADOWED; // To compute CPU clusters
        Scene* pScene = initSanMiguel(&sceneLoadDesc, token, false);
        waitForToken(&token);

        LOGF(LogLevel::eINFO, "Load scene : %f ms", getHiresTimerUSec(&sceneLoadTimer, true) / 1000.0f);

        pGeom = pScene->geom;

        gMeshCount = pGeom->mDrawArgCount;
        gMaterialCount = pGeom->mDrawArgCount;

        pVBMeshInstances = (VBMeshInstance*)tf_calloc(gMeshCount, sizeof(VBMeshInstance));
        gDiffuseMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gNormalMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);
        gSpecularMapsStorage = (Texture**)tf_malloc(sizeof(Texture*) * gMaterialCount);

        for (uint32_t i = 0; i < gMaterialCount; ++i)
        {
            TextureLoadDesc desc = {};
            desc.pFileName = pScene->textures[i];
            desc.ppTexture = &gDiffuseMapsStorage[i];
            // Textures representing color should be stored in SRGB or HDR format
            desc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
            addResource(&desc, NULL);

            TextureLoadDesc descNormal = {};
            descNormal.pFileName = pScene->normalMaps[i];
            descNormal.ppTexture = &gNormalMapsStorage[i];
            addResource(&descNormal, NULL);

            TextureLoadDesc descSpec = {};
            descSpec.pFileName = pScene->specularMaps[i];
            descSpec.ppTexture = &gSpecularMapsStorage[i];
            addResource(&descSpec, NULL);
        }

        waitForAllResourceLoads();

        addTriangleFilteringBuffers(pScene);

        return pScene;
    }

    void Exit()
    {
#ifdef REMOTE_SERVER
        exitStreamServer();
#endif

        exitScreenshotInterface();
        exitCameraController(pCameraController);

        tf_free(gCameraPathData);

        removeTriangleFilteringBuffers();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pParticleConstantBuffer[i]);
        }

        removeResource(pParticlesBuffer);
        removeResource(pParticleBitfields);
        removeResource(pParticleSetsBuffer);
        for (uint32_t i = 0; i < PARTICLE_TEXTURES_COUNT; i++)
        {
            removeResource(ppParticleTextures[i]);
        }
        removeResource(pDepthBoundsBuffer);

        exitGpuProfiler(gGraphicsProfileToken);
        exitGpuProfiler(gComputeProfileToken);
        exitProfiler();

        exitUserInterface();
        exitFontSystem();

        /************************************************************************/
        // Remove loaded scene
        /************************************************************************/
        // Remove textures
        for (uint32_t i = 0; i < gMaterialCount; i++)
        {
            removeResource(gDiffuseMapsStorage[i]);
            removeResource(gNormalMapsStorage[i]);
            removeResource(gSpecularMapsStorage[i]);
        }

        // Destroy scene buffers
#if defined(FORGE_DEBUG)
        bdestroy(&gParticleStatsText);
#endif
        removeResource(pGeom);
        tf_free(pVBMeshInstances);

        tf_free(gDiffuseMapsStorage);
        tf_free(gNormalMapsStorage);
        tf_free(gSpecularMapsStorage);
#if !defined(ENABLE_REMOTE_STREAMING)
        tf_free(pParticleSets);
#endif

        /************************************************************************/
        /************************************************************************/
        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitSemaphore(pRenderer, pPresentSemaphore);

        exitGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        exitGpuCmdRing(pRenderer, &gComputeCmdRing);
        exitQueue(pRenderer, pGraphicsQueue);
        exitQueue(pRenderer, pComputeQueue);

        removeSampler(pRenderer, pSamplerTrilinearAniso);
        removeSampler(pRenderer, pSamplerBilinear);
        removeSampler(pRenderer, pSamplerPointClamp);
        removeSampler(pRenderer, pSamplerBilinearClamp);
        removeSampler(pRenderer, pPointClampSampler);

        exitVisibilityBuffer(pVisibilityBuffer);

        exitResourceLoaderInterface(pRenderer);
        exitRenderer(pRenderer);
        exitGPUConfiguration();

        pRenderer = NULL;

        gFrameCount = 0;
    }

    // Setup the render targets used in this demo.
    // The only render target that is being currently used stores the results of the Visibility Buffer pass.
    // As described earlier, this render target uses 32 bit per pixel to store draw / triangle IDs that will be
    // loaded later by the shade step to reconstruct interpolated triangle data per pixel.
    bool Load(ReloadDesc* pReloadDesc)
    {
        gAppResolution = uint2((uint32_t)(mSettings.mWidth * RESOLUTION_FACTOR), (uint32_t)(mSettings.mHeight * RESOLUTION_FACTOR));

        gFrameCount = 0;

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            loadProfilerUI(gAppResolution.x, gAppResolution.y);

            // Setup the UI components
            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2((float)(gAppResolution.x - 500.0f), 0.0f);
            guiDesc.mStartSize = vec2(500, 150);
            uiAddComponent(GetName(), &guiDesc, &pGuiWindow);
            uiSetComponentFlags(pGuiWindow, GUI_COMPONENT_FLAGS_NO_COLLAPSE | GUI_COMPONENT_FLAGS_NO_RESIZE);

#if defined(FORGE_DEBUG)
            DynamicTextWidget textWidget;
            textWidget.pText = &gParticleStatsText;
            textWidget.pColor = &gTextColor;
            uiAddComponentWidget(pGuiWindow, "Particle System stats", &textWidget, WIDGET_TYPE_DYNAMIC_TEXT);
#endif
            CheckboxWidget asyncCompute;
            asyncCompute.pData = &gAsyncComputeEnabled;
            UIWidget* pAsyncCompute = uiAddComponentWidget(pGuiWindow, "Enable async compute", &asyncCompute, WIDGET_TYPE_CHECKBOX);
            REGISTER_LUA_WIDGET(pAsyncCompute);

            CheckboxWidget cameraWalkingCheckbox;
            cameraWalkingCheckbox.pData = &gCameraWalking;
            luaRegisterWidget(uiAddComponentWidget(pGuiWindow, "Cinematic Camera walking", &cameraWalkingCheckbox, WIDGET_TYPE_CHECKBOX));

            SliderFloatWidget cameraWalkingSpeedSlider;
            cameraWalkingSpeedSlider.pData = &gCameraWalkingSpeed;
            cameraWalkingSpeedSlider.mMin = 0.0f;
            cameraWalkingSpeedSlider.mMax = 20.0f;
            luaRegisterWidget(
                uiAddComponentWidget(pGuiWindow, "Cinematic Camera walking: Speed", &cameraWalkingSpeedSlider, WIDGET_TYPE_SLIDER_FLOAT));

            if (!addSwapChain())
                return false;
            addRenderTargets();

            // Shadow collector
            TextureLoadDesc shadowCollectorLoad = {};
            TextureDesc     shadowCollectorDesc = {};

            shadowCollectorDesc.mArraySize = 1;
            shadowCollectorDesc.mMipLevels = 1;
            shadowCollectorDesc.mDepth = 1;
            shadowCollectorDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
            // R: shadow data, G: distance from occluder, B: distance from camera, A: shadow mask
            shadowCollectorDesc.mFormat = TinyImageFormat_R16_SFLOAT; // TinyImageFormat_R16G16B16A16_SFLOAT;
            shadowCollectorDesc.mHeight = gAppResolution.y;
            shadowCollectorDesc.mWidth = gAppResolution.x;
            shadowCollectorDesc.mSampleCount = SAMPLE_COUNT_1;
            shadowCollectorDesc.mSampleQuality = 0;
            shadowCollectorDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            shadowCollectorDesc.pName = "ShadowCollector";

            shadowCollectorLoad.ppTexture = &pShadowCollector;
            shadowCollectorLoad.pDesc = &shadowCollectorDesc;
            addResource(&shadowCollectorLoad, NULL);

            shadowCollectorLoad.pDesc->pName = "FilteredShadowCollector";
            shadowCollectorLoad.ppTexture = &pFilteredShadowCollector;
            shadowCollectorLoad.pDesc->mStartState = RESOURCE_STATE_UNORDERED_ACCESS;
            addResource(&shadowCollectorLoad, NULL);

            // Setup lights cluster data
            BufferLoadDesc lightClustersCountBufferDesc = {};
            lightClustersCountBufferDesc.mDesc.mSize =
                LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * HORIZONTAL_SUBCLUSTER_COUNT * VERTICAL_SUBCLUSTER_COUNT * sizeof(uint32_t);
            lightClustersCountBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
            lightClustersCountBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            lightClustersCountBufferDesc.mDesc.mFirstElement = 0;
            lightClustersCountBufferDesc.mDesc.mElementCount =
                LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT * HORIZONTAL_SUBCLUSTER_COUNT * VERTICAL_SUBCLUSTER_COUNT;
            lightClustersCountBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
            lightClustersCountBufferDesc.pData = NULL;
            lightClustersCountBufferDesc.mDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            lightClustersCountBufferDesc.mDesc.pName = "Light Cluster Count Buffer Desc";
            lightClustersCountBufferDesc.ppBuffer = &pLightClustersCount;
            lightClustersCountBufferDesc.mForceReset = true;
            addResource(&lightClustersCountBufferDesc, NULL);

            BufferLoadDesc lightClustersDataBufferDesc = {};
            lightClustersDataBufferDesc.mDesc.mElementCount = MAX_LIGHTS_PER_CLUSTER * LIGHT_CLUSTER_WIDTH * LIGHT_CLUSTER_HEIGHT *
                                                              HORIZONTAL_SUBCLUSTER_COUNT * VERTICAL_SUBCLUSTER_COUNT;
            lightClustersDataBufferDesc.mDesc.mStructStride = sizeof(uint32_t);
            lightClustersDataBufferDesc.mDesc.mSize =
                lightClustersDataBufferDesc.mDesc.mElementCount * lightClustersDataBufferDesc.mDesc.mStructStride;
            lightClustersDataBufferDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
            lightClustersDataBufferDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            lightClustersDataBufferDesc.mDesc.mFirstElement = 0;
            lightClustersDataBufferDesc.mDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            lightClustersDataBufferDesc.pData = NULL;
            lightClustersDataBufferDesc.mDesc.pName = "Light Cluster Data Buffer Desc";
            lightClustersDataBufferDesc.ppBuffer = &pLightClusters;
            addResource(&lightClustersDataBufferDesc, NULL);

            // Per pixel linked lists for OIT
            BufferLoadDesc transparencyListDesc = {};
            transparencyListDesc.mDesc.mElementCount = gAppResolution.x * gAppResolution.y * MAX_TRANSPARENCY_LAYERS;
            transparencyListDesc.mDesc.mStructStride = sizeof(PackedParticleTransparencyNode);
            transparencyListDesc.mDesc.mSize = transparencyListDesc.mDesc.mElementCount * transparencyListDesc.mDesc.mStructStride;
            transparencyListDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER;
            transparencyListDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            transparencyListDesc.mDesc.mFirstElement = 0;
            transparencyListDesc.mDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            transparencyListDesc.pData = NULL;
            transparencyListDesc.mDesc.pName = "Transparency Linked List";
            transparencyListDesc.ppBuffer = &pTransparencyListBuffer;
            addResource(&transparencyListDesc, NULL);

            BufferLoadDesc transparencyListHeadDesc = transparencyListDesc;
            transparencyListHeadDesc.mDesc.mStructStride = sizeof(uint32_t);
            transparencyListHeadDesc.mDesc.mElementCount = gAppResolution.x * gAppResolution.y;
            transparencyListHeadDesc.mDesc.mSize =
                transparencyListHeadDesc.mDesc.mStructStride * transparencyListHeadDesc.mDesc.mElementCount;
            transparencyListHeadDesc.mDesc.pName = "Transparency List Heads";
            transparencyListHeadDesc.ppBuffer = &pTransparencyListHeadBuffer;
            addResource(&transparencyListHeadDesc, NULL);

            ParticleSystemInitDesc particleDesc = {};
            particleDesc.pRenderer = pRenderer;
            particleDesc.mSwapColorFormat = pScreenRenderTarget->mFormat;
            particleDesc.mSwapHeight = pScreenRenderTarget->mHeight;
            particleDesc.mSwapWidth = pScreenRenderTarget->mWidth;
            particleDesc.mFramesInFlight = gDataBufferCount;
            particleDesc.mDefaultParticleSetsCount = DEFAULT_PARTICLE_SETS_COUNT;
            particleDesc.mDepthFormat = pDepthBuffer->mFormat;
            particleDesc.mColorSampleQuality = pScreenRenderTarget->mSampleQuality;
            particleDesc.pColorBuffer = pScreenRenderTarget->pTexture;
            particleDesc.pDepthBuffer = pDepthBuffer->pTexture;
            particleDesc.pTransparencyListBuffer = pTransparencyListBuffer;
            particleDesc.pTransparencyListHeadsBuffer = pTransparencyListHeadBuffer;
            particleDesc.ppParticleConstantBuffer = pParticleConstantBuffer;
            particleDesc.pParticlesBuffer = pParticlesBuffer;
            particleDesc.pParticleSetsBuffer = pParticleSetsBuffer;
            particleDesc.pBitfieldBuffer = pParticleBitfields;
            particleDesc.ppParticleTextures = ppParticleTextures;
            particleDesc.mParticleTextureCount = PARTICLE_TEXTURES_COUNT;
            particleSystemInit(&particleDesc);
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            addShaders();
            addRootSignatures();
            addDescriptorSets();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

        prepareDescriptorSets();

        UserInterfaceLoadDesc uiLoad = {};
        uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        uiLoad.mWidth = gAppResolution.x;
        uiLoad.mHeight = gAppResolution.y;
        uiLoad.mDisplayWidth = mSettings.mWidth;
        uiLoad.mDisplayHeight = mSettings.mHeight;
        uiLoad.mLoadType = pReloadDesc->mType;
        loadUserInterface(&uiLoad);

        FontSystemLoadDesc fontLoad = {};
        fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        fontLoad.mHeight = mSettings.mHeight;
        fontLoad.mWidth = mSettings.mWidth;
        fontLoad.mLoadType = pReloadDesc->mType;
        loadFontSystem(&fontLoad);

#ifdef REMOTE_SERVER
        StreamServerLoadDesc loadDesc{};
        loadDesc.mLoadType = pReloadDesc->mType;
        loadDesc.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
        loadDesc.mHeight = gAppResolution.y;
        loadDesc.mWidth = gAppResolution.x;
        loadDesc.mBitrate = DECODER_DEFAULT_BITRATE;
        addStreamServer(&loadDesc);
#endif

        gJustLoaded = true;
        gParticlesLoaded = false;

        gCurrTime = 0.0f;
        gFirefliesWhirlAngle = 0.5;
        gFirefliesElevationT = 0.5;

        LuaScriptDesc runDesc = {};
        runDesc.pScriptFileName = "39_ParticleSystem/Test_AllSets.lua";
        runDesc.pWaitCondition = &gParticlesLoaded;
        luaQueueScriptToRun(&runDesc);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);
        waitQueueIdle(pComputeQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

#ifdef REMOTE_SERVER
        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removeStreamServer();
        }
#endif

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            removePipelines();
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeRenderTargets();
            removeResource(pShadowCollector);
            removeResource(pFilteredShadowCollector);

            removeResource(pLightClusters);
            removeResource(pLightClustersCount);
            removeResource(pTransparencyListBuffer);
            removeResource(pTransparencyListHeadBuffer);

            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();

            particleSystemExit();
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
        // Make screenshot testing deterministic and simulate at a stable 60 FPS
#if defined(AUTOMATED_TESTING)
        deltaTime = 0.016f;
#endif
        // On Apple platforms, Screenshots.cpp currently has to wait one frame after the capture is requested in order to take a
        // screenshot. Since the app is closed after 240 frames have passed and all scripts have been launched, if a script finishes
        // after 240 frames, then no screenshot is taken. For this reason we only wait 210 frames on Apple platforms.
#if defined(__APPLE__)
        if (gCurrTime >= 3.5f)
#else
        if (gCurrTime >= 10.0f)
#endif
        {
            gParticlesLoaded = true;
        }

#ifdef REMOTE_SERVER
        bool userInput = false;
#endif
        if (!uiIsFocused())
        {
            float2 moveInput = { inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) };
            float2 rotateInput = { inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) };
            float  mouseYInput = inputGetValue(0, CUSTOM_MOVE_UP);
#ifdef REMOTE_SERVER
            if (moveInput != float2(0, 0) || rotateInput != float2(0, 0) || mouseYInput != 0)
                userInput = true;
#endif
            pCameraController->onMove(moveInput);
            pCameraController->onRotate(rotateInput);
            pCameraController->onMoveY(mouseYInput);

            if (inputGetValue(0, CUSTOM_RESET_VIEW))
            {
                pCameraController->resetView();
            }
            if (inputGetValue(0, CUSTOM_TOGGLE_FULLSCREEN))
            {
                toggleFullscreen(pWindow);
            }
            if (inputGetValue(0, CUSTOM_TOGGLE_UI))
            {
                uiToggleActive();
            }
            if (inputGetValue(0, CUSTOM_DUMP_PROFILE))
            {
                dumpProfileData(GetName());
            }
            if (inputGetValue(0, CUSTOM_EXIT))
            {
                requestShutdown();
            }
        }

#ifdef REMOTE_SERVER
        if (streamServerIsConnected())
        {
            // streamServerGetLastReceiveInputData(pCameraController);
            //
            // RemoteStreamBufferType bufferType;
            // uint32_t               bufferSize;
            // uint32_t               bufferOffset;
            // void*                  bufferData = streamServerGetLastReceiveBufferData(&bufferType, &bufferSize, &bufferOffset);
            // if (bufferData && bufferType == REMOTE_STREAM_BUFFER_TYPE_PARTICLE_SETS)
            //{
            //    // update the GPU buffer using resource manager
            //    BufferUpdateDesc bufferUpdateDesc = {};
            //    bufferUpdateDesc.mDstOffset = bufferOffset;
            //    bufferUpdateDesc.mSize = bufferSize;
            //    bufferUpdateDesc.pBuffer = pParticleSetsBuffer;
            //
            //    beginUpdateResource(&bufferUpdateDesc);
            //    if (bufferUpdateDesc.pMappedData)
            //        memcpy(bufferUpdateDesc.pMappedData, bufferData, bufferSize);
            //    endUpdateResource(&bufferUpdateDesc);
            //}
            if (!userInput)
                streamServerReceiveInputData(pCameraController);
            streamServerReceiveParticleSetsData(pParticleSetsBuffer);
        }
#endif

        if (gCameraWalking)
        {
            if (gTotalElpasedTime - (0.033333f * gCameraWalkingSpeed) <= gCameraWalkingTime)
            {
                gCameraWalkingTime = 0.0f;
            }

            gCameraWalkingTime += deltaTime * gCameraWalkingSpeed;

            uint32_t currentCameraFrame = (uint32_t)(gCameraWalkingTime / 0.00833f);
            float    remind = gCameraWalkingTime - (float)currentCameraFrame * 0.00833f;

            float3 newPos = v3ToF3(
                lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame]), f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1)]), remind));
            pCameraController->moveTo(f3Tov3(newPos) * SCENE_SCALE);

            float3 newLookat = v3ToF3(lerp(f3Tov3(gCameraPathData[2 * currentCameraFrame + 1]),
                                           f3Tov3(gCameraPathData[2 * (currentCameraFrame + 1) + 1]), remind));
            pCameraController->lookAt(f3Tov3(newLookat) * SCENE_SCALE);
        }

        pCameraController->update(deltaTime);
        updateUniformData(gFrameCount % gDataBufferCount, deltaTime);
    }

    void Draw()
    {
        uint32_t presentIndex = 0;
        uint32_t frameIdx = gFrameCount % gDataBufferCount;

        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        /*****************************************/
        // ASYNC COMPUTE PASS
        /*****************************************/
        if (gAsyncComputeEnabled)
        {
            GpuCmdRingElement computeElem = getNextGpuCmdRingElement(&gComputeCmdRing, true, 1);
            // check to see if we can use the cmd buffer
            FenceStatus       fenceStatus;
            getFenceStatus(pRenderer, computeElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                waitForFences(pRenderer, 1, &computeElem.pFence);

            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
            endUpdateResource(&update);

            Cmd* computeCmd = computeElem.pCmds[0];

            resetCmdPool(pRenderer, computeElem.pCmdPool);
            beginCmd(computeCmd);

            // Filter triangles
            cmdBeginGpuFrameProfile(computeCmd, gComputeProfileToken);
            triangleFilteringPass(computeCmd, frameIdx, gComputeProfileToken);
            cmdEndGpuFrameProfile(computeCmd, gComputeProfileToken);

            endCmd(computeCmd);

            FlushResourceUpdateDesc flushUpdateDesc = {};
            flushResourceUpdates(&flushUpdateDesc);
            Semaphore* computeWaitSemaphores[] = { flushUpdateDesc.pOutSubmittedSemaphore,
                                                   gFrameCount > 1 ? gPrevGraphicsSemaphore : NULL };

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.mSignalSemaphoreCount = 1;
            submitDesc.mWaitSemaphoreCount = computeWaitSemaphores[1] ? TF_ARRAY_COUNT(computeWaitSemaphores) : 1;
            submitDesc.ppCmds = &computeCmd;
            submitDesc.ppSignalSemaphores = &computeElem.pSemaphore;
            submitDesc.ppWaitSemaphores = computeWaitSemaphores;
            submitDesc.pSignalFence = computeElem.pFence;
            submitDesc.mSubmitDone = (gFrameCount < 1);
            queueSubmit(pComputeQueue, &submitDesc);

            gComputeSemaphores[frameIdx] = computeElem.pSemaphore;
        }

        /************************************************************************/
        // GRAPHICS PASS
        /************************************************************************/
        if (!gAsyncComputeEnabled || gFrameCount > 0)
        {
            GpuCmdRingElement graphicsElem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 2);
            // check to see if we can use the cmd buffer
            FenceStatus       fenceStatus;
            getFenceStatus(pRenderer, graphicsElem.pFence, &fenceStatus);
            if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            {
                waitForFences(pRenderer, 1, &graphicsElem.pFence);
            }
            resetCmdPool(pRenderer, graphicsElem.pCmdPool);

#if defined(FORGE_DEBUG)
            ParticleSystemStats particleSystemStats = particleSystemGetStats();
            bassignliteral(&gParticleStatsText, "");
            bformat(&gParticleStatsText,
                    "Allocated particles\nShadow + light: %d, Light: %d, Standard: %d\nAlive particles\nShadow + light: %d, Light: %d, "
                    "Standard: %d\nVisible lights: %d\nVisible particles: %d\n",
                    particleSystemStats.AllocatedCount[0], particleSystemStats.AllocatedCount[1], particleSystemStats.AllocatedCount[2],
                    particleSystemStats.AliveCount[0], particleSystemStats.AliveCount[1], particleSystemStats.AliveCount[2],
                    particleSystemStats.VisibleLightsCount, particleSystemStats.VisibleParticlesCount);
#endif

            BufferUpdateDesc update = { pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
            endUpdateResource(&update);

            update = { pPerFrameUniformBuffers[frameIdx] };
            beginUpdateResource(&update);
            memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameUniformData, sizeof(gPerFrame[frameIdx].gPerFrameUniformData));
            endUpdateResource(&update);

            if (!gAsyncComputeEnabled)
            {
                update = { pPerFrameVBUniformBuffers[VB_UB_COMPUTE][frameIdx] };
                beginUpdateResource(&update);
                memcpy(update.pMappedData, &gPerFrame[frameIdx].gPerFrameVBUniformData, sizeof(gPerFrame[frameIdx].gPerFrameVBUniformData));
                endUpdateResource(&update);
            }

            particleSystemUpdateConstantBuffers(frameIdx, &pParticleSystemConstantData[frameIdx]);

            Cmd* graphicsCmd = graphicsElem.pCmds[0];
            beginCmd(graphicsCmd);
            cmdBeginGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);

            /************************/
            // Triangle filtering
            /************************/
            if (!gAsyncComputeEnabled)
            {
                triangleFilteringPass(graphicsCmd, frameIdx, gGraphicsProfileToken);
            }

            TextureBarrier texBarriers[] = { { pShadowCollector, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS } };
            BufferBarrier  bufBarriers[] = {
                { pTransparencyListBuffer, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                { pTransparencyListHeadBuffer, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
            };
            cmdResourceBarrier(graphicsCmd, 2, bufBarriers, 1, texBarriers, 0, NULL);

            // Clear shadow collector
            cmdBindPipeline(graphicsCmd, pPipelineCleanTexture);
            cmdBindDescriptorSet(graphicsCmd, 0, pDescriptorSetCleanTexture);
            cmdDispatch(graphicsCmd, (uint32_t)ceil((float)gAppResolution.x / TEXTURE_CLEAR_THREAD_COUNT),
                        (uint32_t)ceil((float)gAppResolution.y / TEXTURE_CLEAR_THREAD_COUNT), 1);

            /************************/
            // Draw Pass
            /************************/
            {
                const uint32_t      rtBarriersCount = 5;
                RenderTargetBarrier rtBarriers[rtBarriersCount] = {
                    { pScreenRenderTarget, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                    { pDepthBuffer, RESOURCE_STATE_DEPTH_READ, RESOURCE_STATE_DEPTH_WRITE },
                    { pRenderTargetVBPass, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                    { pRenderTargetShadow, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE },
                    { pDepthCube, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE }
                };

                // Sync resources
                const uint32_t maxNumBarriers = NUM_UNIQUE_GEOMETRIES + 5;
                uint32_t       barrierCount = 0;
                BufferBarrier  barriers2[maxNumBarriers] = {};
                {
                    // VB barriers
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                                  RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE };
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[frameIdx], RESOURCE_STATE_UNORDERED_ACCESS,
                                                  RESOURCE_STATE_SHADER_RESOURCE };
                    for (uint32_t i = 0; i < NUM_UNIQUE_GEOMETRIES; i++)
                    {
                        barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i], RESOURCE_STATE_UNORDERED_ACCESS,
                                                      RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE };
                    }
                }
                cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, rtBarriersCount, rtBarriers);

                /***********************************/
                // Shadow pass
                /***********************************/
                drawShadowMapPass(graphicsCmd, gGraphicsProfileToken, frameIdx);

                /***********************************/
                // VB filling pass
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "VB Filling Pass");
                {
                    drawVisibilityBufferPass(graphicsCmd, frameIdx);

                    rtBarriers[0] = { pRenderTargetVBPass, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    rtBarriers[1] = { pRenderTargetShadow, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    rtBarriers[2] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
                    rtBarriers[3] = { pDepthCube, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };

                    BufferBarrier bBarriers[3] = {
                        { pLightClustersCount, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                        { pLightClusters, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                        { pDepthBoundsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS }
                    };
                    cmdResourceBarrier(graphicsCmd, 3, bBarriers, 0, NULL, 4, rtBarriers);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                /***********************************/
                // Particle system simulation
                /***********************************/
                BufferBarrier particlesBarriers[] = {
                    { pParticleBitfields, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
                    { pParticlesBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS }
                };
                cmdResourceBarrier(graphicsCmd, 2, particlesBarriers, 0, NULL, 0, NULL);

                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "ParticleSystemBegin");
                {
                    particleSystemCmdBegin(graphicsCmd, frameIdx);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "ParticleSystemSimulate");
                {
                    particleSystemCmdSimulate(graphicsCmd, frameIdx);

                    RenderTargetBarrier depthBarrier = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_WRITE };
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 1, &depthBarrier);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                /***********************************/
                // Particle system rendering
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "ParticleSystemRender");
                {
                    // Render particle system
                    BindRenderTargetsDesc bindDesc = {};
                    bindDesc.mRenderTargetCount = 1;
                    bindDesc.mRenderTargets[0].pRenderTarget = pScreenRenderTarget;
                    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_LOAD;
                    bindDesc.mDepthStencil.pDepthStencil = pDepthBuffer;
                    bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_LOAD;
                    cmdBindRenderTargets(graphicsCmd, &bindDesc);

                    cmdSetViewport(graphicsCmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f,
                                   1.0f);
                    cmdSetScissor(graphicsCmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

                    particleSystemCmdRender(graphicsCmd, frameIdx);
                    cmdBindRenderTargets(graphicsCmd, NULL);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                BufferBarrier bufferBarriers[] = {
                    { pTransparencyListBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
                    { pTransparencyListHeadBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
                    { pParticleBitfields, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
                    { pParticlesBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE },
                };
                rtBarriers[0] = { pDepthBuffer, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_SHADER_RESOURCE };
                cmdResourceBarrier(graphicsCmd, 4, bufferBarriers, 0, NULL, 1, rtBarriers);

                /***********************************/
                // Light clustering
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Compute depth clusters");
                {
                    clearLightClusters(graphicsCmd, frameIdx);
                    computeDepthBounds(graphicsCmd, frameIdx);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Compute light clusters");
                {
                    // Wait for depth bounds
                    BufferBarrier depthBoundsBarrier = { pDepthBoundsBuffer, RESOURCE_STATE_UNORDERED_ACCESS,
                                                         RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(graphicsCmd, 1, &depthBoundsBarrier, 0, NULL, 0, NULL);
                    // Light clustering
                    computeLightClusters(graphicsCmd, frameIdx);

                    BufferBarrier lightBarriers[4];
                    lightBarriers[0] = { pLightClustersCount, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    lightBarriers[1] = { pLightClusters, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    lightBarriers[2] = { pParticleBitfields, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    lightBarriers[3] = { pParticlesBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                    cmdResourceBarrier(graphicsCmd, 4, lightBarriers, 0, NULL, 0, NULL);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                /***********************************/
                // VB shading
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "VB Shading Pass");
                {
                    drawVisibilityBufferShade(graphicsCmd, frameIdx);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                /***********************************/
                // Shadow filtering
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Point shadows filtering pass");
                {
                    struct FilterConsts
                    {
                        uint2    ScreenSize;
                        uint32_t Horizontal;
                        uint32_t Pad;
                    };

                    FilterConsts   constants = { { pShadowCollector->mWidth, pShadowCollector->mHeight }, 1, 0 };
                    TextureBarrier texBarrier = { pShadowCollector, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 1, &texBarrier, 0, NULL);

                    cmdBindPipeline(graphicsCmd, pPipelineFilterShadows);
                    cmdBindDescriptorSet(graphicsCmd, 0, pDescriptorSetFilterShadows);
                    cmdBindPushConstants(graphicsCmd, pRootSignatureFilterShadows, gShadowFilteringPushConstsIndex, &constants);
                    // Horizontal pass
                    cmdDispatch(graphicsCmd, (uint32_t)ceil((float)gAppResolution.x / SHADOWMAP_BLUR_THREAD_COUNT),
                                (uint32_t)ceil((float)gAppResolution.y / SHADOWMAP_BLUR_THREAD_COUNT), 1);

                    texBarrier = { pFilteredShadowCollector, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 1, &texBarrier, 0, NULL);

                    // Vertical pass, write on the shadow collector
                    constants.Horizontal = 0;

                    cmdBindPushConstants(graphicsCmd, pRootSignatureFilterShadows, gShadowFilteringPushConstsIndex, &constants);
                    cmdBindDescriptorSet(graphicsCmd, 0, pDescriptorSetFilterShadows);
                    cmdDispatch(graphicsCmd, (uint32_t)ceil((float)gAppResolution.x / SHADOWMAP_BLUR_THREAD_COUNT),
                                (uint32_t)ceil((float)gAppResolution.y / SHADOWMAP_BLUR_THREAD_COUNT), 1);
                    cmdResourceBarrier(graphicsCmd, 0, NULL, 1, &texBarrier, 0, NULL);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);
                cmdBindRenderTargets(graphicsCmd, NULL);

                /***********************************/
                // Synchronization
                /***********************************/
                {
                    // VB barriers
                    barrierCount = 0;
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx],
                                                  RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_SHADER_RESOURCE,
                                                  RESOURCE_STATE_UNORDERED_ACCESS };
                    barriers2[barrierCount++] = { pVisibilityBuffer->ppIndirectDataBuffer[frameIdx], RESOURCE_STATE_SHADER_RESOURCE,
                                                  RESOURCE_STATE_UNORDERED_ACCESS };
                    for (uint32_t i = 0; i < NUM_UNIQUE_GEOMETRIES; ++i)
                    {
                        barriers2[barrierCount++] = { pVisibilityBuffer->ppFilteredIndexBuffer[i],
                                                      RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE,
                                                      RESOURCE_STATE_UNORDERED_ACCESS };
                    }

                    barriers2[barrierCount++] = { pParticleBitfields, RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                  RESOURCE_STATE_SHADER_RESOURCE };
                    barriers2[barrierCount++] = { pParticlesBuffer, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_SHADER_RESOURCE };
                    cmdResourceBarrier(graphicsCmd, barrierCount, barriers2, 0, NULL, 0, NULL);
                }

                /***********************************/
                // Resolve transparency
                /***********************************/
                acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &presentIndex);
                RenderTarget* finalTarget = pSwapChain->ppRenderTargets[presentIndex];
                rtBarriers[0] = { finalTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET };
                rtBarriers[1] = { pScreenRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
                rtBarriers[2] = { pDepthBuffer, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_DEPTH_READ };

                TextureBarrier texBarrier[] = { { pShadowCollector, RESOURCE_STATE_UNORDERED_ACCESS,
                                                  RESOURCE_STATE_PIXEL_SHADER_RESOURCE } };

                cmdResourceBarrier(graphicsCmd, 0, NULL, 1, texBarrier, 3, rtBarriers);
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Full Screen Quad");
                // Draw full screen quad
                {
                    BindRenderTargetsDesc bindDesc = {};

                    bindDesc.mRenderTargetCount = 1;
                    bindDesc.mRenderTargets[0].pRenderTarget = finalTarget;
                    bindDesc.mRenderTargets[0].mClearValue = finalTarget->mClearValue;
                    bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;

                    cmdBindRenderTargets(graphicsCmd, &bindDesc);
                    cmdSetViewport(graphicsCmd, 0.0f, 0.0f, (float)finalTarget->mWidth, (float)finalTarget->mHeight, 0.0f, 1.0f);
                    cmdSetScissor(graphicsCmd, 0, 0, finalTarget->mWidth, finalTarget->mHeight);

                    struct PushConstants
                    {
                        uint2 size;
                    } pushConsts;
                    pushConsts.size = uint2(finalTarget->mWidth, finalTarget->mHeight);

                    cmdBindPipeline(graphicsCmd, pPipelinePresent);
                    cmdBindDescriptorSet(graphicsCmd, 0, pDescriptorSetPresent);
                    cmdBindPushConstants(graphicsCmd, pRootSignaturePresent, gPresentPushConstantsIndex, &pushConsts);
                    cmdDraw(graphicsCmd, 3, 0);
                    cmdBindRenderTargets(graphicsCmd, NULL);
                }
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                /***********************************/
                // Draw UI
                /***********************************/
                cmdBeginGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken, "Draw UI");
                BindRenderTargetsDesc bindDesc = {};
                bindDesc.mRenderTargetCount = 1;
                bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_LOAD;
                bindDesc.mRenderTargets[0].pRenderTarget = finalTarget;

                cmdBindRenderTargets(graphicsCmd, &bindDesc);
                cmdSetViewport(graphicsCmd, 0.0f, 0.0f, (float)finalTarget->mWidth, (float)finalTarget->mHeight, 0.0f, 1.0f);
                cmdSetScissor(graphicsCmd, 0, 0, finalTarget->mWidth, finalTarget->mHeight);
                drawGUI(graphicsCmd);
                cmdEndGpuTimestampQuery(graphicsCmd, gGraphicsProfileToken);

                RenderTargetBarrier rtBarrier = { finalTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
                cmdResourceBarrier(graphicsCmd, 0, NULL, 0, NULL, 1, &rtBarrier);

                cmdEndGpuFrameProfile(graphicsCmd, gGraphicsProfileToken);
                endCmd(graphicsCmd);

                FlushResourceUpdateDesc flushUpdateDesc = {};
                flushResourceUpdates(&flushUpdateDesc);

                Semaphore* waitSemaphores[] = { pImageAcquiredSemaphore, flushUpdateDesc.pOutSubmittedSemaphore,
                                                gComputeSemaphores[frameIdx] };
                Semaphore* signalSemaphores[] = { graphicsElem.pSemaphore, pPresentSemaphore };

                QueueSubmitDesc submitDesc = {};
                submitDesc.mCmdCount = 1;
                submitDesc.mSignalSemaphoreCount = TF_ARRAY_COUNT(signalSemaphores);
                submitDesc.ppCmds = &graphicsCmd;
                submitDesc.ppSignalSemaphores = signalSemaphores;
                submitDesc.pSignalFence = graphicsElem.pFence;
                submitDesc.ppWaitSemaphores = waitSemaphores;
                submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores) - (gAsyncComputeEnabled ? 0 : 1);
                queueSubmit(pGraphicsQueue, &submitDesc);

                gPrevGraphicsSemaphore = graphicsElem.pSemaphore;

                QueuePresentDesc presentDesc = {};
                presentDesc.mIndex = (uint8_t)presentIndex;
                presentDesc.mWaitSemaphoreCount = 1;
                presentDesc.ppWaitSemaphores = &pPresentSemaphore;
                presentDesc.pSwapChain = pSwapChain;
                presentDesc.mSubmitDone = true;

#ifdef REMOTE_SERVER
                // streamServerEncodeAndPackFrame(pSwapChain, presentIndex);
                streamFrame(pSwapChain, presentIndex);
#endif

                queuePresent(pGraphicsQueue, &presentDesc);
                flipProfiler();

                gJustLoaded = false;
            }
        }

        ++gFrameCount;
    }

    const char* GetName() { return "39_ParticleSystem"; }

    bool addDescriptorSets()
    {
        // Clear Buffers
        DescriptorSetDesc setDesc = { pRootSignatureClearBuffers, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetClearBuffers);
        // Triangle Filtering
        setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[0]);
        setDesc = { pRootSignatureTriangleFiltering, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetTriangleFiltering[1]);
        // Light Clustering
        setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[0]);
        setDesc = { pRootSignatureLightClusters, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetLightClusters[1]);
        // VB, Shadow
        setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[0]);
        setDesc = { pRootSignatureVBPass, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBPass[1]);

        setDesc = { pRootSignatureFilterShadows, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetFilterShadows);
        setDesc = { pRootSignatureCleanTexture, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetCleanTexture);

        // VB Shade
        setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[0]);
        setDesc = { pRootSignatureVBShade, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetVBShade[1]);

        // Downsample depth
        setDesc = { pRootSignatureDownsampleDepth, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetDownsampleDepth);
        // Present
        setDesc = { pRootSignaturePresent, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPresent);

        return true;
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetVBPass[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBPass[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBShade[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetVBShade[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetLightClusters[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetLightClusters[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetClearBuffers);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[0]);
        removeDescriptorSet(pRenderer, pDescriptorSetTriangleFiltering[1]);
        removeDescriptorSet(pRenderer, pDescriptorSetFilterShadows);
        removeDescriptorSet(pRenderer, pDescriptorSetCleanTexture);
        removeDescriptorSet(pRenderer, pDescriptorSetDownsampleDepth);
        removeDescriptorSet(pRenderer, pDescriptorSetPresent);
    }

    void prepareDescriptorSets()
    {
        // Particle ranges
        DescriptorDataRange bitfieldLightRange = { 0, sizeof(uint) * MAX_LIGHT_COUNT, sizeof(uint) };
        DescriptorDataRange particleLightRange = { 0, sizeof(ParticleData) * MAX_LIGHT_COUNT, sizeof(ParticleData) };

        DescriptorDataRange bitfieldShadowRange = { 0, sizeof(uint) * MAX_SHADOW_COUNT, sizeof(uint) };
        DescriptorDataRange particleShadowRange = { 0, sizeof(ParticleData) * MAX_SHADOW_COUNT, sizeof(ParticleData) };

        // Clear Buffers
        {
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                uint32_t       clearParamsCount = 0;
                DescriptorData clearParams[2] = {};
                clearParams[clearParamsCount].pName = "indirectDrawArgs";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                clearParams[clearParamsCount].pName = "VBConstantBuffer";
                clearParams[clearParamsCount++].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
                updateDescriptorSet(pRenderer, i, pDescriptorSetClearBuffers, clearParamsCount, clearParams);
            }
        }
        // Triangle Filtering
        {
            DescriptorData filterParams[7] = {};
            uint32_t       paramsCount = 0;
            filterParams[paramsCount].pName = "vertexPositionBuffer";
            filterParams[paramsCount++].ppBuffers = &pGeom->pVertexBuffers[0];
            filterParams[paramsCount].pName = "indexDataBuffer";
            filterParams[paramsCount++].ppBuffers = &pGeom->pIndexBuffer;
            filterParams[paramsCount].pName = "meshConstantsBuffer";
            filterParams[paramsCount++].ppBuffers = &pMeshConstantsBuffer;
            filterParams[paramsCount].pName = "VBConstantBuffer";
            filterParams[paramsCount++].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            filterParams[paramsCount].pName = "ParticlesDataBuffer";
            filterParams[paramsCount].ppBuffers = &pParticlesBuffer;
            filterParams[paramsCount++].pRanges = &particleShadowRange;
            filterParams[paramsCount].pName = "BitfieldBuffer";
            filterParams[paramsCount].ppBuffers = &pParticleBitfields;
            filterParams[paramsCount++].pRanges = &bitfieldShadowRange;
            filterParams[paramsCount].pName = "ParticleSetsBuffer";
            filterParams[paramsCount++].ppBuffers = &pParticleSetsBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetTriangleFiltering[0], paramsCount, filterParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData filterParamsPerFrame[7] = {};
                filterParamsPerFrame[0].pName = "filteredIndicesBuffer";
                filterParamsPerFrame[0].mCount = NUM_CULLING_VIEWPORTS;
                filterParamsPerFrame[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[0];
                filterParamsPerFrame[1].pName = "filterDispatchGroupDataBuffer";
                filterParamsPerFrame[1].ppBuffers = &pVisibilityBuffer->ppFilterDispatchGroupDataBuffer[i];
                filterParamsPerFrame[2].pName = "PerFrameVBConstants";
                filterParamsPerFrame[2].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
                filterParamsPerFrame[3].pName = "indirectDrawArgs";
                filterParamsPerFrame[3].ppBuffers = &pVisibilityBuffer->ppIndirectDrawArgBuffer[i];
                filterParamsPerFrame[4].pName = "indirectDataBuffer";
                filterParamsPerFrame[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetTriangleFiltering[1], 5, filterParamsPerFrame);
            }
        }

        // Light Clustering
        {
            DescriptorData params[9] = {};
            params[0].pName = "lightClustersCount";
            params[0].ppBuffers = &pLightClustersCount;
            params[1].pName = "lightClusters";
            params[1].ppBuffers = &pLightClusters;
            params[2].pName = "DownsampledDepthBuffer";
            params[2].ppTextures = &pRenderTargetDownsampleDepth->pTexture;
            params[3].pName = "BitfieldBuffer";
            params[3].ppBuffers = &pParticleBitfields;
            params[3].pRanges = &bitfieldLightRange;
            params[4].pName = "ParticlesDataBuffer";
            params[4].ppBuffers = &pParticlesBuffer;
            params[4].pRanges = &particleLightRange;
            params[5].pName = "DepthBuffer";
            params[5].ppTextures = &pDepthBuffer->pTexture;
            params[6].pName = "DepthBoundsBuffer";
            params[6].ppBuffers = &pDepthBoundsBuffer;
            params[7].pName = "ParticleSetsBuffer";
            params[7].ppBuffers = &pParticleSetsBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetLightClusters[0], 8, params);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                params[0].pName = "PerFrameVBConstants";
                params[0].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                params[1].pName = "ParticleConstantBuffer";
                params[1].ppBuffers = &pParticleConstantBuffer[i];

                updateDescriptorSet(pRenderer, i, pDescriptorSetLightClusters[1], 2, params);
            }
        }
        // Texture cleaning
        {
            DescriptorData params[3] = {};
            params[0].pName = "shadowCollector";
            params[0].ppTextures = &pShadowCollector;
            params[1].pName = "transparencyListHeads";
            params[1].ppBuffers = &pTransparencyListHeadBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetCleanTexture, 2, params);
        }
        // VB, Shadow
        {
            DescriptorData params[6] = {};
            params[0].pName = "diffuseMaps";
            params[0].mCount = gMaterialCount;
            params[0].ppTextures = gDiffuseMapsStorage;
            params[1].pName = "vertexPositionBuffer";
            params[1].ppBuffers = &pGeom->pVertexBuffers[0];
            params[2].pName = "vertexTexCoordBuffer";
            params[2].ppBuffers = &pGeom->pVertexBuffers[1];
            params[3].pName = "BitfieldBuffer";
            params[3].ppBuffers = &pParticleBitfields;
            params[3].pRanges = &bitfieldShadowRange;
            params[4].pName = "ParticlesDataBuffer";
            params[4].ppBuffers = &pParticlesBuffer;
            params[4].pRanges = &particleShadowRange;
            params[5].pName = "ParticleSetsBuffer";
            params[5].ppBuffers = &pParticleSetsBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBPass[0], 6, params);

            params[0] = {};
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                params[0].pName = "indirectDataBuffer";
                params[0].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                params[1].pName = "PerFrameVBConstants";
                params[1].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];

                updateDescriptorSet(pRenderer, i, pDescriptorSetVBPass[1], 2, params);
            }
        }
        // VB Shade
        {
            DescriptorData vbShadeParams[17] = {};
            DescriptorData vbShadeParamsPerFrame[12] = {};
            vbShadeParams[0].pName = "vbTex";
            vbShadeParams[0].ppTextures = &pRenderTargetVBPass->pTexture;
            vbShadeParams[1].pName = "vertexPos";
            vbShadeParams[1].ppBuffers = &pGeom->pVertexBuffers[0];
            vbShadeParams[2].pName = "vertexTexCoord";
            vbShadeParams[2].ppBuffers = &pGeom->pVertexBuffers[1];
            vbShadeParams[3].pName = "vertexNormal";
            vbShadeParams[3].ppBuffers = &pGeom->pVertexBuffers[2];
            vbShadeParams[4].pName = "shadowMap";
            vbShadeParams[4].ppTextures = &pRenderTargetShadow->pTexture;
            vbShadeParams[5].pName = "meshConstantsBuffer";
            vbShadeParams[5].ppBuffers = &pMeshConstantsBuffer;
            vbShadeParams[6].pName = "shadowCollector";
            vbShadeParams[6].ppTextures = &pShadowCollector;
            vbShadeParams[7].pName = "diffuseMaps";
            vbShadeParams[7].mCount = gMaterialCount;
            vbShadeParams[7].ppTextures = gDiffuseMapsStorage;
            vbShadeParams[8].pName = "normalMaps";
            vbShadeParams[8].mCount = gMaterialCount;
            vbShadeParams[8].ppTextures = gNormalMapsStorage;
            vbShadeParams[9].pName = "specularMaps";
            vbShadeParams[9].mCount = gMaterialCount;
            vbShadeParams[9].ppTextures = gSpecularMapsStorage;
            vbShadeParams[10].pName = "VBConstantBuffer";
            vbShadeParams[10].ppBuffers = &pVisibilityBuffer->pVBConstantBuffer;
            vbShadeParams[11].pName = "BitfieldBuffer";
            vbShadeParams[11].ppBuffers = &pParticleBitfields;
            vbShadeParams[11].pRanges = &bitfieldLightRange;
            vbShadeParams[12].pName = "ParticlesDataBuffer";
            vbShadeParams[12].ppBuffers = &pParticlesBuffer;
            vbShadeParams[12].pRanges = &particleLightRange;
            vbShadeParams[13].pName = "lightClustersCount";
            vbShadeParams[13].ppBuffers = &pLightClustersCount;
            vbShadeParams[14].pName = "lightClusters";
            vbShadeParams[14].ppBuffers = &pLightClusters;
            vbShadeParams[15].pName = "ParticleSetsBuffer";
            vbShadeParams[15].ppBuffers = &pParticleSetsBuffer;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetVBShade[0], 16, vbShadeParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                vbShadeParamsPerFrame[0].pName = "filteredIndexBuffer";
                vbShadeParamsPerFrame[0].ppBuffers = &pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
                vbShadeParamsPerFrame[1].pName = "depthCube";
                vbShadeParamsPerFrame[1].ppTextures = &pDepthCube->pTexture;
                vbShadeParamsPerFrame[2].pName = "PerFrameConstants";
                vbShadeParamsPerFrame[2].ppBuffers = &pPerFrameUniformBuffers[i];
                vbShadeParamsPerFrame[3].pName = "PerFrameVBConstants";
                vbShadeParamsPerFrame[3].ppBuffers = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
                vbShadeParamsPerFrame[4].pName = "indirectDataBuffer";
                vbShadeParamsPerFrame[4].ppBuffers = &pVisibilityBuffer->ppIndirectDataBuffer[i];
                vbShadeParamsPerFrame[5].pName = "ParticleConstantBuffer";
                vbShadeParamsPerFrame[5].ppBuffers = &pParticleConstantBuffer[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetVBShade[1], 6, vbShadeParamsPerFrame);
            }
        }

        {
            Texture*       filteringTextures[] = { pShadowCollector, pFilteredShadowCollector };
            DescriptorData params[2] = {};
            params[0].pName = "Textures";
            params[0].ppTextures = filteringTextures;
            params[0].mCount = 2;
            updateDescriptorSet(pRenderer, 0, pDescriptorSetFilterShadows, 1, params);
        }

        {
            DescriptorData params[2] = {};
            params[0].pName = "depthBuffer";
            params[0].ppTextures = &pDepthBuffer->pTexture;
            params[1].pName = "pointClampSampler";
            params[1].ppSamplers = &pPointClampSampler;

            updateDescriptorSet(pRenderer, 0, pDescriptorSetDownsampleDepth, 2, params);
        }

        {
            DescriptorData params[6] = {};
            params[0].pName = "bilinearSampler";
            params[0].ppSamplers = &pSamplerBilinearClamp;
            params[1].pName = "g_inputTexture";
            params[1].ppTextures = &pScreenRenderTarget->pTexture;
            params[2].pName = "shadowCollector";
            params[2].ppTextures = &pShadowCollector;
            params[3].pName = "transparencyListHeads";
            params[3].ppBuffers = &pTransparencyListHeadBuffer;
            params[4].pName = "transparencyList";
            params[4].ppBuffers = &pTransparencyListBuffer;
            params[5].pName = "pointClampSampler";
            params[5].ppSamplers = &pPointClampSampler;

            updateDescriptorSet(pRenderer, 0, pDescriptorSetPresent, 6, params);
        }
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
        swapChainDesc.mWidth = gAppResolution.x;
        swapChainDesc.mHeight = gAppResolution.y;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;

        swapChainDesc.mColorClearValue = { { 1, 1, 1, 1 } };
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);
        return pSwapChain != NULL;
    }

    void addRenderTargets()
    {
        const uint32_t width = gAppResolution.x;
        const uint32_t height = gAppResolution.y;
        /************************************************************************/
        /************************************************************************/
        ClearValue     optimizedDepthClear = { { 0.0f, 0 } };
        ClearValue     optimizedColorClearWhite = { { 1.0f, 1.0f, 1.0f, 1.0f } };

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
        depthRT.mStartState = RESOURCE_STATE_DEPTH_READ;
        depthRT.mHeight = height;
        depthRT.mSampleCount = SAMPLE_COUNT_1;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = width;
        depthRT.pName = "Depth Buffer RT";
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        /************************************************************************/
        // Shadow pass render target
        /************************************************************************/
        RenderTargetDesc shadowRTDesc = {};
        shadowRTDesc.mArraySize = 1;
        shadowRTDesc.mClearValue = optimizedDepthClear;
        shadowRTDesc.mDepth = 1;
        shadowRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        shadowRTDesc.mFormat = TinyImageFormat_D16_UNORM;
        shadowRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        shadowRTDesc.mWidth = SHADOWMAP_SIZE;
        shadowRTDesc.mSampleCount = SAMPLE_COUNT_1;
        shadowRTDesc.mSampleQuality = 0;
        shadowRTDesc.mHeight = SHADOWMAP_SIZE;
        shadowRTDesc.pName = "Shadow Map RT";
        addRenderTarget(pRenderer, &shadowRTDesc, &pRenderTargetShadow);

        // Cube depth faces
        RenderTargetDesc faceDesc = {};
        faceDesc.mArraySize = 6 * MAX_SHADOW_COUNT;
        faceDesc.mClearValue = optimizedDepthClear;
        faceDesc.mDepth = 1;
        faceDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_TEXTURE_CUBE | DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES;
        faceDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        faceDesc.mWidth = CUBE_SHADOWS_FACE_SIZE;
        faceDesc.mHeight = CUBE_SHADOWS_FACE_SIZE;
        faceDesc.mFormat = TinyImageFormat_D16_UNORM;
        faceDesc.pName = "DepthCubemap";
        faceDesc.mSampleCount = SAMPLE_COUNT_1;
        faceDesc.mSampleQuality = 0;
        addRenderTarget(pRenderer, &faceDesc, &pDepthCube);

        /************************************************************************/
        // Visibility buffer pass render target
        /************************************************************************/
        RenderTargetDesc vbRTDesc = {};
        vbRTDesc.mArraySize = 1;
        vbRTDesc.mClearValue = optimizedColorClearWhite;
        vbRTDesc.mDepth = 1;
        vbRTDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        vbRTDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        vbRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        vbRTDesc.mHeight = height;
        vbRTDesc.mSampleCount = SAMPLE_COUNT_1;
        vbRTDesc.mSampleQuality = 0;
        vbRTDesc.mWidth = width;
        vbRTDesc.pName = "VB RT";
        addRenderTarget(pRenderer, &vbRTDesc, &pRenderTargetVBPass);

        /************************************************************************/
        // Intermediate render target
        /************************************************************************/
        RenderTargetDesc postProcRTDesc = {};
        postProcRTDesc.mArraySize = 1;
        postProcRTDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        postProcRTDesc.mDepth = 1;
        postProcRTDesc.mMipLevels = 1;
        postProcRTDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        postProcRTDesc.mFormat = TinyImageFormat_R10G10B10A2_UNORM;
        postProcRTDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        postProcRTDesc.mHeight = height;
        postProcRTDesc.mWidth = width;
        postProcRTDesc.mSampleCount = SAMPLE_COUNT_1;
        postProcRTDesc.mSampleQuality = 0;
        postProcRTDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        postProcRTDesc.pName = "Intermediate target";
        addRenderTarget(pRenderer, &postProcRTDesc, &pScreenRenderTarget);

        RenderTargetDesc downsampledDesc = {};
        downsampledDesc.mArraySize = 1;
        downsampledDesc.mClearValue = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        downsampledDesc.mDepth = 1;
        downsampledDesc.mMipLevels = 1;
        downsampledDesc.mDescriptors = DESCRIPTOR_TYPE_RW_TEXTURE | DESCRIPTOR_TYPE_TEXTURE;
        downsampledDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
        downsampledDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        downsampledDesc.mHeight = height / DOWNSAMPLED_DEPTH_DIVISOR;
        downsampledDesc.mWidth = width / DOWNSAMPLED_DEPTH_DIVISOR;
        downsampledDesc.mSampleCount = SAMPLE_COUNT_1;
        downsampledDesc.mSampleQuality = 0;
        downsampledDesc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        downsampledDesc.pName = "DownsampledDepth";
        addRenderTarget(pRenderer, &downsampledDesc, &pRenderTargetDownsampleDepth);
    }

    void removeRenderTargets()
    {
        removeRenderTarget(pRenderer, pDepthBuffer);
        removeRenderTarget(pRenderer, pRenderTargetVBPass);
        removeRenderTarget(pRenderer, pRenderTargetShadow);
        removeRenderTarget(pRenderer, pScreenRenderTarget);
        removeRenderTarget(pRenderer, pDepthCube);
        removeRenderTarget(pRenderer, pRenderTargetDownsampleDepth);
    }
    /************************************************************************/
    // Load all the shaders needed for the demo
    /************************************************************************/
    void addRootSignatures()
    {
        // Graphics root signatures
        const char* pTextureSamplerName = "textureFilter";
        const char* pShadingSamplerNames[] = { "depthSampler", "textureSampler", "pointSampler" };
        Sampler*    pShadingSamplers[] = { pSamplerBilinearClamp, pSamplerBilinear, pSamplerPointClamp };

        Shader* pShaders[NUM_GEOMETRY_SETS * 2] = {};
        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            pShaders[i * 2] = pShaderVisibilityBufferPass[i];
            pShaders[i * 2 + 1] = pShaderShadowPass[i];
        }
        RootSignatureDesc vbRootDesc = { pShaders, NUM_GEOMETRY_SETS * 2 };
        vbRootDesc.mMaxBindlessTextures = gMaterialCount;
        vbRootDesc.ppStaticSamplerNames = &pTextureSamplerName;
        vbRootDesc.ppStaticSamplers = &pSamplerPointClamp;
        vbRootDesc.mStaticSamplerCount = 1;
        addRootSignature(pRenderer, &vbRootDesc, &pRootSignatureVBPass);

        RootSignatureDesc shadeRootDesc = { &pShaderVisibilityBufferShade, 1 };
        // Set max number of bindless textures in the root signature
        shadeRootDesc.mMaxBindlessTextures = gMaterialCount;
        shadeRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
        shadeRootDesc.ppStaticSamplers = pShadingSamplers;
        shadeRootDesc.mStaticSamplerCount = 3;
        addRootSignature(pRenderer, &shadeRootDesc, &pRootSignatureVBShade);

        // Clear buffers root signature
        RootSignatureDesc clearBuffersRootDesc = { &pShaderClearBuffers, 1 };
        addRootSignature(pRenderer, &clearBuffersRootDesc, &pRootSignatureClearBuffers);

        // Triangle filtering root signatures
        RootSignatureDesc triangleFilteringRootDesc = { &pShaderTriangleFiltering, 1 };
        addRootSignature(pRenderer, &triangleFilteringRootDesc, &pRootSignatureTriangleFiltering);

        Shader*           pClusterShaders[] = { pShaderClearLightClusters, pShaderClusterLights, pShaderComputeDepthBounds,
                                      pShaderComputeDepthCluster };
        RootSignatureDesc clearLightRootDesc = { pClusterShaders, 4 };
        addRootSignature(pRenderer, &clearLightRootDesc, &pRootSignatureLightClusters);

        RootSignatureDesc filterShadowsRootDesc = { &pShaderFilterShadows, 1 };
        addRootSignature(pRenderer, &filterShadowsRootDesc, &pRootSignatureFilterShadows);

        RootSignatureDesc clearTexRootDesc = { &pShaderCleanTexture, 1 };
        addRootSignature(pRenderer, &clearTexRootDesc, &pRootSignatureCleanTexture);

        Shader*           pFSQShaders[] = { pShaderPresent };
        RootSignatureDesc presentRootDesc = { pFSQShaders, 1 };
        presentRootDesc.ppStaticSamplerNames = pShadingSamplerNames;
        presentRootDesc.ppStaticSamplers = pShadingSamplers;
        presentRootDesc.mStaticSamplerCount = 2;
        addRootSignature(pRenderer, &presentRootDesc, &pRootSignaturePresent);

        Shader*           pDownsampleShaders[] = { pShaderDownsampleDepth };
        RootSignatureDesc downsampleRootDesc = { pDownsampleShaders, 1 };
        addRootSignature(pRenderer, &downsampleRootDesc, &pRootSignatureDownsampleDepth);

        gPresentPushConstantsIndex = getDescriptorIndexFromName(pRootSignaturePresent, "fsqRootConstants");
        gShadowPushConstsIndex = getDescriptorIndexFromName(pRootSignatureVBPass, "ShadowRootConstants");
        gShadowFilteringPushConstsIndex = getDescriptorIndexFromName(pRootSignatureFilterShadows, "RootConstant");
        gDownsamplePushConstantsIndex = getDescriptorIndexFromName(pRootSignatureDownsampleDepth, "downsampleRootConstants");
    }

    void removeRootSignatures()
    {
        removeRootSignature(pRenderer, pRootSignatureLightClusters);
        removeRootSignature(pRenderer, pRootSignatureClearBuffers);
        removeRootSignature(pRenderer, pRootSignatureTriangleFiltering);
        removeRootSignature(pRenderer, pRootSignatureVBShade);
        removeRootSignature(pRenderer, pRootSignatureVBPass);
        removeRootSignature(pRenderer, pRootSignaturePresent);
        removeRootSignature(pRenderer, pRootSignatureDownsampleDepth);
        removeRootSignature(pRenderer, pRootSignatureFilterShadows);
        removeRootSignature(pRenderer, pRootSignatureCleanTexture);
    }

    void addShaders()
    {
        ShaderLoadDesc shadowPass = {};
        ShaderLoadDesc shadowPassAlpha = {};

        ShaderLoadDesc vbPass = {};
        ShaderLoadDesc vbPassAlpha = {};

        ShaderLoadDesc clearBuffer = {};
        ShaderLoadDesc triangleCulling = {};
        ShaderLoadDesc clearLights = {};
        ShaderLoadDesc clusterLights = {};
        ShaderLoadDesc clearTexture = {};

        ShaderLoadDesc vbShade = {};
        ShaderLoadDesc filterShadows = {};
        ShaderLoadDesc depthBounds = {};
        ShaderLoadDesc depthClusters = {};
        ShaderLoadDesc downsampleDepth = {};
        ShaderLoadDesc present = {};

        shadowPass.mVert.pFileName = "shadow_pass.vert";
        shadowPass.mFrag.pFileName = "shadow_pass.frag";
        shadowPassAlpha.mVert.pFileName = "shadow_pass_alpha.vert";
        shadowPassAlpha.mFrag.pFileName = "shadow_pass_alpha.frag";

        vbPass.mVert.pFileName = "visibilityBuffer_pass.vert";
        vbPass.mFrag.pFileName = "visibilityBuffer_pass.frag";
        vbPassAlpha.mVert.pFileName = "visibilityBuffer_pass_alpha.vert";
        vbPassAlpha.mFrag.pFileName = "visibilityBuffer_pass_alpha.frag";

        // Some vulkan driver doesn't generate glPrimitiveID without a geometry pass (steam deck as 03/30/2023)
        bool addGeometryPassThrough = gGpuSettings.mAddGeometryPassThrough;
        if (addGeometryPassThrough)
        {
            vbPass.mGeom.pFileName = "visibilityBuffer_pass.geom";
            vbPassAlpha.mGeom.pFileName = "visibilityBuffer_pass_alpha.geom";
        }

        vbShade.mVert.pFileName = "visibilityBuffer_shade.vert";
        vbShade.mFrag.pFileName = "visibilityBuffer_shade_SAMPLE_1.frag";

        // Triangle culling compute shader
        triangleCulling.mComp.pFileName = "triangle_filtering.comp";
        // Clear buffers compute shader
        clearBuffer.mComp.pFileName = "clear_buffers.comp";
        // Clear light clusters compute shader
        clearLights.mComp.pFileName = "clear_light_clusters.comp";
        // Cluster lights compute shader
        clusterLights.mComp.pFileName = "cluster_lights.comp";
        // Shadow filtering compute shader
        filterShadows.mComp.pFileName = "shadow_filtering.comp";
        // Texture clearing shader
        clearTexture.mComp.pFileName = "clear_texture.comp";
        // Depth bounds shader
        depthBounds.mComp.pFileName = "compute_depth_bounds.comp";
        // Depth clusters shader
        depthClusters.mComp.pFileName = "compute_depth_clusters.comp";
        // Downsample depth shader
        downsampleDepth.mVert.pFileName = "fsq.vert";
        downsampleDepth.mFrag.pFileName = "downsample_depth.frag";

        // Present shader
        present.mVert.pFileName = "fsq.vert";
#if defined(AUTOMATED_TESTING)
        present.mFrag.pFileName = "fsq_hq.frag";
#else
        present.mFrag.pFileName = "fsq.frag";
#endif

        addShader(pRenderer, &shadowPass, &pShaderShadowPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &shadowPassAlpha, &pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbPass, &pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        addShader(pRenderer, &vbPassAlpha, &pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        addShader(pRenderer, &vbShade, &pShaderVisibilityBufferShade);
        addShader(pRenderer, &clearBuffer, &pShaderClearBuffers);
        addShader(pRenderer, &triangleCulling, &pShaderTriangleFiltering);
        addShader(pRenderer, &clearLights, &pShaderClearLightClusters);
        addShader(pRenderer, &clusterLights, &pShaderClusterLights);
        addShader(pRenderer, &present, &pShaderPresent);
        addShader(pRenderer, &filterShadows, &pShaderFilterShadows);
        addShader(pRenderer, &clearTexture, &pShaderCleanTexture);
        addShader(pRenderer, &depthBounds, &pShaderComputeDepthBounds);
        addShader(pRenderer, &depthClusters, &pShaderComputeDepthCluster);
        addShader(pRenderer, &downsampleDepth, &pShaderDownsampleDepth);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderShadowPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_OPAQUE]);
        removeShader(pRenderer, pShaderVisibilityBufferPass[GEOMSET_ALPHA_CUTOUT]);
        removeShader(pRenderer, pShaderVisibilityBufferShade);
        removeShader(pRenderer, pShaderTriangleFiltering);
        removeShader(pRenderer, pShaderClearBuffers);
        removeShader(pRenderer, pShaderClusterLights);
        removeShader(pRenderer, pShaderClearLightClusters);
        removeShader(pRenderer, pShaderFilterShadows);
        removeShader(pRenderer, pShaderPresent);
        removeShader(pRenderer, pShaderCleanTexture);
        removeShader(pRenderer, pShaderComputeDepthBounds);
        removeShader(pRenderer, pShaderComputeDepthCluster);
        removeShader(pRenderer, pShaderDownsampleDepth);
    }

    void addPipelines()
    {
        /************************************************************************/
        // Setup compute pipelines for triangle filtering
        /************************************************************************/
        PipelineDesc pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
        ComputePipelineDesc& compPipelineSettings = pipelineDesc.mComputeDesc;
        compPipelineSettings.pShaderProgram = pShaderClearBuffers;
        compPipelineSettings.pRootSignature = pRootSignatureClearBuffers;
        pipelineDesc.pName = "Clear Filtering Buffers";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearBuffers);

        // Create the compute pipeline for GPU triangle filtering
        pipelineDesc.pName = "Triangle Filtering";
        compPipelineSettings.pShaderProgram = pShaderTriangleFiltering;
        compPipelineSettings.pRootSignature = pRootSignatureTriangleFiltering;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineTriangleFiltering);

        // Setup the clearing light clusters pipeline
        pipelineDesc.pName = "Clear Light Clusters";
        compPipelineSettings.pShaderProgram = pShaderClearLightClusters;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClearLightClusters);

        // Setup the compute light clusters pipeline
        pipelineDesc.pName = "Cluster Lights";
        compPipelineSettings.pShaderProgram = pShaderClusterLights;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineClusterLights);

        // Setup the shadowmap filtering pipeline
        pipelineDesc.pName = "Shadowmap filtering";
        compPipelineSettings.pShaderProgram = pShaderFilterShadows;
        compPipelineSettings.pRootSignature = pRootSignatureFilterShadows;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineFilterShadows);

        // Setup the texture clearing pipeline
        pipelineDesc.pName = "Clear texture";
        compPipelineSettings.pShaderProgram = pShaderCleanTexture;
        compPipelineSettings.pRootSignature = pRootSignatureCleanTexture;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineCleanTexture);

        // Setup the depth bounds pipeline
        pipelineDesc.pName = "Compute depth bounds";
        compPipelineSettings.pShaderProgram = pShaderComputeDepthBounds;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineComputeDepthBounds);

        // Setup the depth clusters pipeline
        pipelineDesc.pName = "Compute depth clusters";
        compPipelineSettings.pShaderProgram = pShaderComputeDepthCluster;
        compPipelineSettings.pRootSignature = pRootSignatureLightClusters;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineComputeDepthCluster);

        /************************************************************************/
        /************************************************************************/
        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = true;
        depthStateDesc.mDepthFunc = CMP_GEQUAL;
        DepthStateDesc depthStateDisableDesc = {};

        RasterizerStateDesc rasterizerStateCullNoneDesc = { CULL_MODE_NONE };
        RasterizerStateDesc rasterizerStateCullFrontDesc = { CULL_MODE_FRONT };

        /************************************************************************/
        // Setup the Shadow Pass Pipeline
        /************************************************************************/
        // Setup pipeline settings
        pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& shadowPipelineSettings = pipelineDesc.mGraphicsDesc;
        shadowPipelineSettings = { 0 };
        shadowPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        shadowPipelineSettings.pDepthState = &depthStateDesc;
        shadowPipelineSettings.mDepthStencilFormat = pRenderTargetShadow->mFormat;
        shadowPipelineSettings.mSampleCount = pRenderTargetShadow->mSampleCount;
        shadowPipelineSettings.mSampleQuality = pRenderTargetShadow->mSampleQuality;
        shadowPipelineSettings.pRootSignature = pRootSignatureVBPass;

        shadowPipelineSettings.pRasterizerState = &rasterizerStateCullFrontDesc;
        shadowPipelineSettings.pVertexLayout = NULL;
        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[0];
        pipelineDesc.pName = "Shadow Opaque";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[0]);

        shadowPipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        shadowPipelineSettings.pShaderProgram = pShaderShadowPass[1];
        pipelineDesc.pName = "Shadow AlphaTested";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineShadowPass[1]);

        /************************************************************************/
        // Setup the Visibility Buffer Pass Pipeline
        /************************************************************************/
        // Setup pipeline settings
        GraphicsPipelineDesc& vbPassPipelineSettings = pipelineDesc.mGraphicsDesc;
        vbPassPipelineSettings = { 0 };
        vbPassPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbPassPipelineSettings.mRenderTargetCount = 1;
        vbPassPipelineSettings.pDepthState = &depthStateDesc;
        vbPassPipelineSettings.pColorFormats = &pRenderTargetVBPass->mFormat;
        vbPassPipelineSettings.mSampleCount = pRenderTargetVBPass->mSampleCount;
        vbPassPipelineSettings.mSampleQuality = pRenderTargetVBPass->mSampleQuality;
        vbPassPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
        vbPassPipelineSettings.pRootSignature = pRootSignatureVBPass;

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            vbPassPipelineSettings.pRasterizerState =
                i == GEOMSET_ALPHA_CUTOUT ? &rasterizerStateCullNoneDesc : &rasterizerStateCullFrontDesc;
            vbPassPipelineSettings.pShaderProgram = pShaderVisibilityBufferPass[i];

#if defined(GFX_EXTENDED_PSO_OPTIONS)
            ExtendedGraphicsPipelineDesc edescs[2] = {};
            edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
            initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
            edescs[0].shaderLimitsDesc.maxWavesWithLateAllocParameterCache = 16;

            edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
            edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
            edescs[1].pixelShaderOptions.depthBeforeShader =
                !i ? PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE : PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_DEFAULT;

            pipelineDesc.pPipelineExtensions = edescs;
            pipelineDesc.mExtensionCount = sizeof(edescs) / sizeof(edescs[0]);
#endif
            pipelineDesc.pName = GEOMSET_OPAQUE == i ? "VB Opaque" : "VB AlphaTested";
            addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferPass[i]);

            pipelineDesc.mExtensionCount = 0;
        }
        /************************************************************************/
        // Setup the resources needed for the Visibility Buffer Shade Pipeline
        /************************************************************************/
        GraphicsPipelineDesc& vbShadePipelineSettings = pipelineDesc.mGraphicsDesc;
        vbShadePipelineSettings = { 0 };
        vbShadePipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        vbShadePipelineSettings.mRenderTargetCount = 1;
        vbShadePipelineSettings.pDepthState = &depthStateDisableDesc;
        vbShadePipelineSettings.pRasterizerState = &rasterizerStateCullNoneDesc;
        vbShadePipelineSettings.pRootSignature = pRootSignatureVBShade;

        vbShadePipelineSettings.pShaderProgram = pShaderVisibilityBufferShade;
        vbShadePipelineSettings.mSampleCount = SAMPLE_COUNT_1;
        vbShadePipelineSettings.pColorFormats = &pScreenRenderTarget->mFormat;
        vbShadePipelineSettings.mSampleQuality = pScreenRenderTarget->mSampleQuality;

#if defined(GFX_EXTENDED_PSO_OPTIONS)
        ExtendedGraphicsPipelineDesc edescs[2] = {};
        edescs[0].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_SHADER_LIMITS;
        initExtendedGraphicsShaderLimits(&edescs[0].shaderLimitsDesc);
        // edescs[0].ShaderLimitsDesc.MaxWavesWithLateAllocParameterCache = 22;

        edescs[1].type = EXTENDED_GRAPHICS_PIPELINE_TYPE_DEPTH_STENCIL_OPTIONS;
        edescs[1].pixelShaderOptions.outOfOrderRasterization = PIXEL_SHADER_OPTION_OUT_OF_ORDER_RASTERIZATION_ENABLE_WATER_MARK_7;
        edescs[1].pixelShaderOptions.depthBeforeShader = PIXEL_SHADER_OPTION_DEPTH_BEFORE_SHADER_ENABLE;

        pipelineDesc.pPipelineExtensions = edescs;
        pipelineDesc.mExtensionCount = sizeof(edescs) / sizeof(edescs[0]);
#endif
        pipelineDesc.pName = "VB Shade";
        addPipeline(pRenderer, &pipelineDesc, &pPipelineVisibilityBufferShadeSrgb);
        pipelineDesc.mExtensionCount = 0;

        pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
        pipelineDesc.mGraphicsDesc.pDepthState = &depthStateDisableDesc;
        pipelineDesc.mGraphicsDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineDesc.mGraphicsDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineDesc.mGraphicsDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineDesc.mGraphicsDesc.pRootSignature = pRootSignaturePresent;
        pipelineDesc.mGraphicsDesc.pShaderProgram = pShaderPresent;
        pipelineDesc.mGraphicsDesc.pVertexLayout = NULL;
        pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerStateCullNoneDesc;
        addPipeline(pRenderer, &pipelineDesc, &pPipelinePresent);

        TinyImageFormat downsampleDepthFormat = TinyImageFormat_R16G16_SFLOAT;
        pipelineDesc = {};
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        pipelineDesc.mGraphicsDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineDesc.mGraphicsDesc.mRenderTargetCount = 1;
        pipelineDesc.mGraphicsDesc.pDepthState = &depthStateDisableDesc;
        pipelineDesc.mGraphicsDesc.pColorFormats = &downsampleDepthFormat;
        pipelineDesc.mGraphicsDesc.mSampleCount = SAMPLE_COUNT_1;
        pipelineDesc.mGraphicsDesc.mSampleQuality = 0;
        pipelineDesc.mGraphicsDesc.pRootSignature = pRootSignatureDownsampleDepth;
        pipelineDesc.mGraphicsDesc.pShaderProgram = pShaderDownsampleDepth;
        pipelineDesc.mGraphicsDesc.pVertexLayout = NULL;
        pipelineDesc.mGraphicsDesc.pRasterizerState = &rasterizerStateCullNoneDesc;
        addPipeline(pRenderer, &pipelineDesc, &pPipelineDownsampleDepth);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pPipelineVisibilityBufferShadeSrgb);

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
            removePipeline(pRenderer, pPipelineVisibilityBufferPass[i]);

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
            removePipeline(pRenderer, pPipelineShadowPass[i]);

        // Destroy triangle filtering pipelines
        removePipeline(pRenderer, pPipelineClusterLights);
        removePipeline(pRenderer, pPipelineClearLightClusters);
        removePipeline(pRenderer, pPipelineTriangleFiltering);
        removePipeline(pRenderer, pPipelineClearBuffers);
        removePipeline(pRenderer, pPipelinePresent);
        removePipeline(pRenderer, pPipelineFilterShadows);
        removePipeline(pRenderer, pPipelineCleanTexture);
        removePipeline(pRenderer, pPipelineComputeDepthCluster);
        removePipeline(pRenderer, pPipelineComputeDepthBounds);
        removePipeline(pRenderer, pPipelineDownsampleDepth);
    }

    void addTriangleFilteringBuffers(Scene* pScene)
    {
        /************************************************************************/
        // Mesh constants
        /************************************************************************/
        uint32_t visibilityBufferFilteredIndexCount[NUM_GEOMETRY_SETS] = {};

        MeshConstants* meshConstants = (MeshConstants*)tf_malloc(gMeshCount * sizeof(MeshConstants));
        // Calculate mesh constants and filter containers
        for (uint32_t i = 0; i < gMeshCount; ++i)
        {
            MaterialFlags materialFlag = pScene->materialFlags[i];
            uint32_t      geomSet = materialFlag & MATERIAL_FLAG_ALPHA_TESTED ? GEOMSET_ALPHA_CUTOUT : GEOMSET_OPAQUE;
            visibilityBufferFilteredIndexCount[geomSet] += (pScene->geom->pDrawArgs + i)->mIndexCount;
            pVBMeshInstances[i].mGeometrySet = geomSet;
            pVBMeshInstances[i].mMeshIndex = i;
            pVBMeshInstances[i].mTriangleCount = (pScene->geom->pDrawArgs + i)->mIndexCount / 3;
            pVBMeshInstances[i].mInstanceIndex = INSTANCE_INDEX_NONE;

            meshConstants[i].indexOffset = pGeom->pDrawArgs[i].mStartIndex;
            meshConstants[i].vertexOffset = pGeom->pDrawArgs[i].mVertexOffset;
            meshConstants[i].materialID = i;
            meshConstants[i].twoSided = (pScene->materialFlags[i] & MATERIAL_FLAG_TWO_SIDED) ? 1 : 0;
        }

        removeResource(pScene->geomData);

        // Init visibility buffer
        VisibilityBufferDesc vbDesc = {};
        vbDesc.mNumFrames = gDataBufferCount;
        vbDesc.mNumBuffers = gDataBufferCount;
        vbDesc.mNumGeometrySets = NUM_GEOMETRY_SETS;
        vbDesc.mNumViews = NUM_CULLING_VIEWPORTS;
        vbDesc.mComputeThreads = VB_COMPUTE_THREADS;
        vbDesc.pMaxIndexCountPerGeomSet = visibilityBufferFilteredIndexCount;
        initVisibilityBuffer(pRenderer, &vbDesc, &pVisibilityBuffer);

        BufferLoadDesc meshConstantDesc = {};
        meshConstantDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER;
        meshConstantDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        meshConstantDesc.mDesc.mElementCount = gMeshCount;
        meshConstantDesc.mDesc.mStructStride = sizeof(MeshConstants);
        meshConstantDesc.mDesc.mSize = meshConstantDesc.mDesc.mElementCount * meshConstantDesc.mDesc.mStructStride;
        meshConstantDesc.pData = meshConstants;
        meshConstantDesc.ppBuffer = &pMeshConstantsBuffer;
        meshConstantDesc.mDesc.pName = "Mesh Constant Desc";
        addResource(&meshConstantDesc, NULL);

        UpdateVBMeshFilterGroupsDesc updateVBMeshFilterGroupsDesc = {};
        updateVBMeshFilterGroupsDesc.mNumMeshInstance = gMeshCount;
        updateVBMeshFilterGroupsDesc.pVBMeshInstances = pVBMeshInstances;
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            updateVBMeshFilterGroupsDesc.mFrameIndex = i;
            gVBPreFilterStats[i] = updateVBMeshFilterGroups(pVisibilityBuffer, &updateVBMeshFilterGroupsDesc);
        }

        for (uint32_t frameIdx = 0; frameIdx < gDataBufferCount; ++frameIdx)
        {
            gPerFrame[frameIdx].gDrawCount[GEOMSET_OPAQUE] = gVBPreFilterStats[frameIdx].mGeomsetMaxDrawCounts[GEOMSET_OPAQUE];
            gPerFrame[frameIdx].gDrawCount[GEOMSET_ALPHA_CUTOUT] = gVBPreFilterStats[frameIdx].mGeomsetMaxDrawCounts[GEOMSET_ALPHA_CUTOUT];
        }

        /************************************************************************/
        // Per Frame Constant Buffers
        /************************************************************************/
        uint64_t       size = sizeof(PerFrameConstantsData);
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mSize = size;
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = NULL;
        ubDesc.mDesc.pName = "Uniform Buffer Desc";

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pPerFrameUniformBuffers[i];
            addResource(&ubDesc, NULL);
        }

        ubDesc.mDesc.mSize = sizeof(PerFrameVBConstantsData);
        ubDesc.mDesc.pName = "PerFrameVBConstants Uniform Buffer Desc";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            ubDesc.ppBuffer = &pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i];
            addResource(&ubDesc, NULL);
            ubDesc.ppBuffer = &pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i];
            addResource(&ubDesc, NULL);
        }

        waitForAllResourceLoads();
        tf_free(meshConstants);
    }

    void removeTriangleFilteringBuffers()
    {
        /************************************************************************/
        // Mesh constants
        /************************************************************************/
        removeResource(pMeshConstantsBuffer);

        /************************************************************************/
        // Per Frame Constant Buffers
        /************************************************************************/
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPerFrameUniformBuffers[i]);
        }

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pPerFrameVBUniformBuffers[VB_UB_COMPUTE][i]);
            removeResource(pPerFrameVBUniformBuffers[VB_UB_GRAPHICS][i]);
        }
    }

    /************************************************************************/
    // Scene update
    /************************************************************************/
    // Updates uniform data for the given frame index.
    void updateUniformData(uint currentFrameIdx, float deltaTime)
    {
        const uint32_t width = gAppResolution.x;
        const uint32_t height = gAppResolution.y;
        const float    aspectRatioInv = (float)height / width;
        const uint32_t frameIdx = currentFrameIdx;
        PerFrameData*  currentFrame = &gPerFrame[frameIdx];

        CameraMatrix cameraProj = CameraMatrix::perspectiveReverseZ(PI / 2.0f, aspectRatioInv, CAMERA_NEAR, CAMERA_FAR);

        /*********************************************************/
        // Update particle system uniforms
        /*********************************************************/
        mat4 cameraView = pCameraController->getViewMatrix();

        pParticleSystemConstantData[currentFrameIdx].ViewTransform = cameraView;
        pParticleSystemConstantData[currentFrameIdx].ProjTransform = cameraProj.mCamera;
        pParticleSystemConstantData[currentFrameIdx].ViewProjTransform = cameraProj.mCamera * cameraView;
        pParticleSystemConstantData[currentFrameIdx].CameraPosition = float4(v3ToF3(pCameraController->getViewPosition()), 1.0f);
        pParticleSystemConstantData[currentFrameIdx].ScreenSize = uint2(gAppResolution.x, gAppResolution.y);

        float firefliesX = cos(gFirefliesWhirlAngle) * gFirefliesFlockRadius;
        float firefliesZ = 6.0f + sin(gFirefliesWhirlAngle) * gFirefliesFlockRadius;

        static int elevationDirection = 1;
        float      firefliesY = lerp(gFirefliesMinHeight, gFirefliesMaxHeight, ((float)sin(gFirefliesElevationT) + 1.0f) / 2.0f);

        gFirefliesWhirlAngle += deltaTime * gFirefliesWhirlSpeed;
        gFirefliesElevationT += deltaTime * gFirefliesElevationSpeed * elevationDirection;
        if ((gFirefliesElevationT >= 1.0f && elevationDirection == 1) || (gFirefliesElevationT <= 0.0f && elevationDirection == -1))
            elevationDirection *= -1;

        pParticleSystemConstantData[currentFrameIdx].ResetParticles = gJustLoaded ? 1 : 0;
        pParticleSystemConstantData[currentFrameIdx].Time = gCurrTime;
        pParticleSystemConstantData[currentFrameIdx].SeekPosition = float4(firefliesX, firefliesY, firefliesZ, 1.0f);
        pParticleSystemConstantData[currentFrameIdx].CameraPlanes = float2(CAMERA_NEAR, CAMERA_FAR);
#if defined(AUTOMATED_TESTING)
        pParticleSystemConstantData[currentFrameIdx].Seed = INT_MAX / 2;
        pParticleSystemConstantData[currentFrameIdx].TimeDelta = 1.0f / 60.0f;
#else
        pParticleSystemConstantData[currentFrameIdx].TimeDelta = deltaTime;
        pParticleSystemConstantData[currentFrameIdx].Seed = randomInt(0, INT_MAX);
#endif

        /***********************************/
        // Update visibility buffer uniforms
        /***********************************/
        mat4 vbCameraModel = mat4::identity();
        mat4 vbCameraView = pCameraController->getViewMatrix();

        Point3 lightSourcePos(10.f, 000.0f, 10.f);
        lightSourcePos[0] += (20.f);
        mat4 rotation = mat4::rotationXY(gSunControl.x, gSunControl.y);
        mat4 translation = mat4::translation(-vec3(lightSourcePos));

        vec3         lightDir = vec4(inverse(rotation) * vec4(0, 0, 1, 0)).getXYZ();
        CameraMatrix lightProj = CameraMatrix::orthographicReverseZ(-50, 30, -90, 30, -15, 1);
        mat4         lightView = rotation * translation;

        float2 twoOverRes = { 2.0f / float(width), 2.0f / float(height) };
        /************************************************************************/
        // Lighting data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.camPos = v4ToF4(vec4(pCameraController->getViewPosition()));
        currentFrame->gPerFrameUniformData.lightDir = v4ToF4(vec4(lightDir));
        currentFrame->gPerFrameUniformData.twoOverRes = twoOverRes;
        currentFrame->gPerFrameUniformData.esmControl = gESMControl;
        /************************************************************************/
        // Matrix data
        /************************************************************************/

        // Camera view
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp = cameraProj * vbCameraView;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp);
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].projection = cameraProj;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].mvp =
            currentFrame->gPerFrameVBUniformData.transform[VIEW_CAMERA].vp * vbCameraModel;

        // Shadow map view
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp = lightProj * lightView;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].invVP =
            CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp);
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].projection = lightProj;
        currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].mvp = currentFrame->gPerFrameVBUniformData.transform[VIEW_SHADOW].vp;

        // Point light cubemaps view

        CameraMatrix cubeProj = CameraMatrix::perspectiveReverseZ(degToRad(90), 1.0f, CAMERA_NEAR, CAMERA_FAR);
        for (uint32_t i = 0; i < 6; i++)
        {
            mat4     cubeView = mat4::identity();
            uint32_t viewportIdx = 2 + i;

            switch (i)
            {
            case 0: // POSITIVE_X
                cubeView = mat4::rotation(degToRad(90.0f), vec3(0.0f, 1.0f, 0.0f));
                break;
            case 1: // NEGATIVE_X
                cubeView = mat4::rotation(degToRad(-90.0f), vec3(0.0f, 1.0f, 0.0f));
                break;
            case 2: // POSITIVE_Y
                cubeView = mat4::rotation(degToRad(90.0f), vec3(-1.0f, 0.0f, 0.0f));
                break;
            case 3: // NEGATIVE_Y
                cubeView = mat4::rotation(degToRad(-90.0f), vec3(-1.0f, 0.0f, 0.0f));
                break;
            case 4: // POSITIVE_Z
                cubeView = mat4::identity();
                break;
            case 5: // NEGATIVE_Z
                cubeView = mat4::rotation(degToRad(180.0f), vec3(0.0f, 1.0f, 0.0f));
                break;
            }

            cubeView = inverse(cubeView);

            currentFrame->gPerFrameVBUniformData.transform[viewportIdx].vp = cubeProj * cubeView;
            currentFrame->gPerFrameVBUniformData.transform[viewportIdx].invVP =
                CameraMatrix::inverse(currentFrame->gPerFrameVBUniformData.transform[viewportIdx].vp);
            currentFrame->gPerFrameVBUniformData.transform[viewportIdx].projection = cubeProj;
            currentFrame->gPerFrameVBUniformData.transform[viewportIdx].mvp =
                currentFrame->gPerFrameVBUniformData.transform[viewportIdx].vp;
        }

        /************************************************************************/
        // Culling data
        /************************************************************************/
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_SHADOW].sampleCount = SAMPLE_COUNT_1;
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_SHADOW].windowSize = { (float)SHADOWMAP_SIZE, (float)SHADOWMAP_SIZE };

        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_CAMERA].sampleCount = SAMPLE_COUNT_1;
        currentFrame->gPerFrameVBUniformData.cullingViewports[VIEW_CAMERA].windowSize = { (float)width, (float)height };

        for (uint32_t i = 2; i < NUM_CULLING_VIEWPORTS; i++)
        {
            currentFrame->gPerFrameVBUniformData.cullingViewports[i].sampleCount = SAMPLE_COUNT_1;
            currentFrame->gPerFrameVBUniformData.cullingViewports[i].windowSize = { CUBE_SHADOWS_FACE_SIZE, CUBE_SHADOWS_FACE_SIZE };
        }

        // Cache eye position in object space for cluster culling on the CPU
        currentFrame->gEyeObjectSpace[VIEW_SHADOW] = (inverse(lightView) * vec4(0, 0, 0, 1)).getXYZ();
        currentFrame->gEyeObjectSpace[VIEW_CAMERA] =
            (inverse(vbCameraView * vbCameraModel) * vec4(0, 0, 0, 1)).getXYZ(); // vec4(0,0,0,1) is the camera position in eye space
        /************************************************************************/
        // Shading data
        /************************************************************************/
        currentFrame->gPerFrameUniformData.lightColor = float4(1.0f, 1.0f, 0.9f, 1.0f);
        currentFrame->gPerFrameUniformData.outputMode = 0;
        currentFrame->gPerFrameUniformData.CameraPlane = { CAMERA_NEAR, CAMERA_FAR };

        gCurrTime += deltaTime;
    }

    void triangleFilteringPass(Cmd* cmd, uint32_t frameIndex, ProfileToken profileToken)
    {
        /************************************************************************/
        // Triangle filtering pass
        /************************************************************************/
        TriangleFilteringPassDesc triangleFilteringDesc = {};
        triangleFilteringDesc.pPipelineClearBuffers = pPipelineClearBuffers;
        triangleFilteringDesc.pPipelineTriangleFiltering = pPipelineTriangleFiltering;
        triangleFilteringDesc.pDescriptorSetClearBuffers = pDescriptorSetClearBuffers;
        triangleFilteringDesc.pDescriptorSetTriangleFiltering = pDescriptorSetTriangleFiltering[0];
        triangleFilteringDesc.pDescriptorSetTriangleFilteringPerFrame = pDescriptorSetTriangleFiltering[1];
        triangleFilteringDesc.mFrameIndex = frameIndex;
        triangleFilteringDesc.mBuffersIndex = frameIndex;
        triangleFilteringDesc.mGpuProfileToken = profileToken;
        triangleFilteringDesc.mVBPreFilterStats = gVBPreFilterStats[frameIndex];
        cmdVBTriangleFilteringPass(pVisibilityBuffer, cmd, &triangleFilteringDesc);
    }

    void drawShadowGeometry(Cmd* cmd, uint32_t frameIdx, bool alphaTested, uint32_t cubeIndex, uint32_t particleIndex)
    {
        uint32_t resourceIndex = alphaTested ? 1 : 0;
        uint32_t geomSets[] = { GEOMSET_OPAQUE, GEOMSET_ALPHA_CUTOUT };
        uint2    shadowConsts = { cubeIndex, particleIndex };

        cmdBindPipeline(cmd, pPipelineShadowPass[resourceIndex]);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);
        cmdBindPushConstants(cmd, pRootSignatureVBPass, gShadowPushConstsIndex, &shadowConsts);

        uint32_t view = cubeIndex == 0 ? VIEW_SHADOW : VIEW_POINT_SHADOW + particleIndex;
        uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(view, geomSets[resourceIndex], 0) * sizeof(uint32_t);
        Buffer*  pIndirectDrawBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
        cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectDrawBuffer, indirectBufferByteOffset, NULL, 0);
    }

    /************************************************************************/
    // Rendering
    /************************************************************************/
    // Render the shadow mapping pass. This pass updates the shadow map texture and the depth cubemap
    void drawShadowMapPass(Cmd* cmd, ProfileToken pGpuProfiler, uint32_t frameIdx)
    {
        BindRenderTargetsDesc bindDesc = {};
        bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_CLEAR;
        bindDesc.mDepthStencil.pDepthStencil = pRenderTargetShadow;
        bindDesc.mDepthStencil.mClearValue = pRenderTargetShadow->mClearValue;

        // Start render pass and apply load actions
        cmdBindRenderTargets(cmd, &bindDesc);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetShadow->mWidth, (float)pRenderTargetShadow->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetShadow->mWidth, pRenderTargetShadow->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_SHADOW];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

        /************************************************/
        // Opaque + alpha tested directional shadow map
        /************************************************/
        cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Shadow Map");
        for (uint32_t i = 0; i < 2; i++)
        {
            drawShadowGeometry(cmd, frameIdx, i == 1, 0, 0);
        }
        cmdBindRenderTargets(cmd, NULL);
        cmdEndGpuTimestampQuery(cmd, pGpuProfiler);

        /************************************************/
        // Opaque + alpha tested point light shadow maps
        /************************************************/
        // For each shadow casting particle
        cmdBeginGpuTimestampQuery(cmd, pGpuProfiler, "Point Light Shadows");

        bindDesc.mDepthStencil.pDepthStencil = pDepthCube;
        bindDesc.mDepthStencil.mClearValue = pDepthCube->mClearValue;
        bindDesc.mDepthStencil.mUseArraySlice = true;

        for (uint32_t s = 0; s < MAX_SHADOW_COUNT; s++)
        {
            // Bind cubemap face
            for (uint32_t c = 0; c < 6; c++)
            {
                bindDesc.mDepthStencil.mArraySlice = s * 6 + c;

                cmdBindRenderTargets(cmd, &bindDesc);
                cmdSetViewport(cmd, 0.0f, 0.0f, CUBE_SHADOWS_FACE_SIZE, CUBE_SHADOWS_FACE_SIZE, 0.0f, 1.0f);
                cmdSetScissor(cmd, 0, 0, CUBE_SHADOWS_FACE_SIZE, CUBE_SHADOWS_FACE_SIZE);

                pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_POINT_SHADOW + s];
                cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

                // Draw opaque and alpha tested geometry
                for (uint32_t t = 0; t < 2; t++)
                {
                    drawShadowGeometry(cmd, frameIdx, t == 1, c + 1, s);
                }
            }
        }

        cmdEndGpuTimestampQuery(cmd, pGpuProfiler);
        cmdBindRenderTargets(cmd, NULL);
    }

    // Render the scene to perform the Visibility Buffer pass. In this pass the (filtered) scene geometry is rendered
    // into a 32-bit per pixel render target. This contains triangle information (batch Id and triangle Id) that allows
    // to reconstruct all triangle attributes per pixel. This is faster than a typical Deferred Shading pass, because
    // less memory bandwidth is used.
    void drawVisibilityBufferPass(Cmd* cmd, uint32_t frameIdx)
    {
        // Render target is cleared to (1,1,1,1) because (0,0,0,0) represents the first triangle of the first draw batch
        BindRenderTargetsDesc bindDesc = {};
        bindDesc.mRenderTargetCount = 1;
        bindDesc.mRenderTargets[0].pRenderTarget = pRenderTargetVBPass;
        bindDesc.mRenderTargets[0].mClearValue = pRenderTargetVBPass->mClearValue;
        bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;
        bindDesc.mDepthStencil.pDepthStencil = pDepthBuffer;
        bindDesc.mDepthStencil.mLoadAction = LOAD_ACTION_CLEAR;

        // Start render pass and apply load actions
        cmdBindRenderTargets(cmd, &bindDesc);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTargetVBPass->mWidth, (float)pRenderTargetVBPass->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetVBPass->mWidth, pRenderTargetVBPass->mHeight);

        Buffer* pIndexBuffer = pVisibilityBuffer->ppFilteredIndexBuffer[VIEW_CAMERA];
        cmdBindIndexBuffer(cmd, pIndexBuffer, INDEX_TYPE_UINT32, 0);

        for (uint32_t i = 0; i < NUM_GEOMETRY_SETS; ++i)
        {
            cmdBindPipeline(cmd, pPipelineVisibilityBufferPass[i]);
            cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBPass[0]);
            cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBPass[1]);

            uint64_t indirectBufferByteOffset = GET_INDIRECT_DRAW_ELEM_INDEX(VIEW_CAMERA, i, 0) * sizeof(uint32_t);
            Buffer*  pIndirectDrawBuffer = pVisibilityBuffer->ppIndirectDrawArgBuffer[frameIdx];
            cmdExecuteIndirect(cmd, INDIRECT_DRAW_INDEX, 1, pIndirectDrawBuffer, indirectBufferByteOffset, NULL, 0);
        }
        cmdBindRenderTargets(cmd, NULL);
    }

    // Render a fullscreen triangle to evaluate shading for every pixel. This render step uses the render target generated by
    // DrawVisibilityBufferPass to get the draw / triangle IDs to reconstruct and interpolate vertex attributes per pixel. This method
    // doesn't set any vertex/index buffer because the triangle positions are calculated internally using vertex_id.
    void drawVisibilityBufferShade(Cmd* cmd, uint32_t frameIdx)
    {
        // Set load actions to clear the screen to black
        BindRenderTargetsDesc bindDesc = {};
        bindDesc.mRenderTargetCount = 1;
        bindDesc.mRenderTargets[0].pRenderTarget = pScreenRenderTarget;
        bindDesc.mRenderTargets[0].mClearValue = pScreenRenderTarget->mClearValue;
        bindDesc.mRenderTargets[0].mLoadAction = LOAD_ACTION_CLEAR;

        cmdBindRenderTargets(cmd, &bindDesc);
        cmdSetViewport(cmd, 0.0f, 0.0f, (float)pScreenRenderTarget->mWidth, (float)pScreenRenderTarget->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pScreenRenderTarget->mWidth, pScreenRenderTarget->mHeight);

        cmdBindPipeline(cmd, pPipelineVisibilityBufferShadeSrgb);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetVBShade[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetVBShade[1]);
        // A single triangle is rendered without specifying a vertex buffer (triangle positions are calculated internally using vertex_id)
        cmdDraw(cmd, 3, 0);

        cmdBindRenderTargets(cmd, NULL);
    }

    void downsampleDepthBuffer(Cmd* cmd)
    {
        BindRenderTargetsDesc bindDesc = {};
        bindDesc.mRenderTargetCount = 1;
        bindDesc.mRenderTargets[0].pRenderTarget = pRenderTargetDownsampleDepth;

        RenderTargetBarrier rtBarrier = { pRenderTargetDownsampleDepth, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &rtBarrier);

        uint2 targetSize = uint2(pRenderTargetDownsampleDepth->mWidth, pRenderTargetDownsampleDepth->mHeight);

        cmdBindRenderTargets(cmd, &bindDesc);
        cmdSetViewport(cmd, 0, 0, (float)pRenderTargetDownsampleDepth->mWidth, (float)pRenderTargetDownsampleDepth->mHeight, 0.0f, 1.0f);
        cmdSetScissor(cmd, 0, 0, pRenderTargetDownsampleDepth->mWidth, pRenderTargetDownsampleDepth->mHeight);
        cmdBindPipeline(cmd, pPipelineDownsampleDepth);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetDownsampleDepth);
        cmdBindPushConstants(cmd, pRootSignatureDownsampleDepth, gDownsamplePushConstantsIndex, &targetSize);
        cmdDraw(cmd, 3, 0);
        cmdBindRenderTargets(cmd, NULL);

        rtBarrier = { pRenderTargetDownsampleDepth, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, &rtBarrier);
    }

    void computeDepthBounds(Cmd* cmd, uint32_t frameIdx)
    {
        downsampleDepthBuffer(cmd);

        uint2 dispatchSize = uint2((uint32_t)ceil((float)pRenderTargetDownsampleDepth->mWidth / DEPTH_BOUNDS_GROUP_AMOUNT_X),
                                   (uint32_t)ceil((float)pRenderTargetDownsampleDepth->mHeight / DEPTH_BOUNDS_GROUP_AMOUNT_Y));

        cmdBindPipeline(cmd, pPipelineComputeDepthBounds);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetLightClusters[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetLightClusters[1]);
        cmdDispatch(cmd, dispatchSize.x, dispatchSize.y, 1);

        BufferBarrier bb = { pDepthBoundsBuffer, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_UNORDERED_ACCESS };
        cmdResourceBarrier(cmd, 1, &bb, 0, NULL, 0, NULL);

        dispatchSize = uint2((uint32_t)ceil((float)pDepthBuffer->mWidth / (DEPTH_BOUNDS_GROUP_AMOUNT_X * DEPTH_ClUSTERS_KERNEL_SIZE)),
                             (uint32_t)ceil((float)pDepthBuffer->mHeight / (DEPTH_BOUNDS_GROUP_AMOUNT_Y * DEPTH_ClUSTERS_KERNEL_SIZE)));
        cmdBindPipeline(cmd, pPipelineComputeDepthCluster);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetLightClusters[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetLightClusters[1]);
        cmdDispatch(cmd, dispatchSize.x, dispatchSize.y, 1);
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
        uint32_t dispatchCount = (uint32_t)ceil(sqrt((float)MAX_LIGHT_COUNT));

        cmdBindPipeline(cmd, pPipelineClusterLights);
        cmdBindDescriptorSet(cmd, 0, pDescriptorSetLightClusters[0]);
        cmdBindDescriptorSet(cmd, frameIdx, pDescriptorSetLightClusters[1]);
        cmdDispatch(cmd, dispatchCount, dispatchCount, 1);
    }

    // Draw GUI / 2D elements
    void drawGUI(Cmd* cmd)
    {
        gFrameTimeDraw.mFontColor = 0xff00ffff;
#if defined(ANDROID) || defined(IOS)
        gFrameTimeDraw.mFontSize = 10.0f;
        gFrameTimeDraw.mFontID = gFontID;
#else
        gFrameTimeDraw.mFontSize = 14.0f;
        gFrameTimeDraw.mFontID = gFontID;
#endif

        cmdDrawCpuProfile(cmd, float2(8.0f, 15.0f), &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.0f, 100.0f), gGraphicsProfileToken, &gFrameTimeDraw);
        cmdDrawGpuProfile(cmd, float2(8.0f, 700.0f), gComputeProfileToken, &gFrameTimeDraw);
        cmdDrawUserInterface(cmd);

        cmdBindRenderTargets(cmd, NULL);
    }
};
DEFINE_APPLICATION_MAIN(Particle_System)
