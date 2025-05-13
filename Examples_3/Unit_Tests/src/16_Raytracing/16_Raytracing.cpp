/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

// Unit Test to create Bottom and Top Level Acceleration Structures using Raytracing API.

// Interfaces
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/OS/Interfaces/IInput.h"
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

// Raytracing
#include "../../../../Common_3/Graphics/Interfaces/IRay.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

// Geometry
#include "../../../Visibility_Buffer/src/SanMiguel.h"

// fsl

#define NO_FSL_DEFINITIONS
#include "../../../../Common_3/Graphics/FSL/defaults.h"
#include "Shaders/FSL/Shared.fsl.h"
#include "Shaders/FSL/Global.srt.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

#define USE_DENOISER  0
#define USE_RAY_QUERY 1

#define SCENE_SCALE   10.0f

bool gUseUavRwFallback = false;

ICameraController* pCameraController = NULL;

ProfileToken gGpuProfileToken;

enum RaytracingTechnique
{
    RAY_QUERY = 0,
    // #NOTE: Extend enum when you add new raytracing technique
    RAYTRACING_TECHNIQUE_COUNT
};
uint32_t gRaytracingTechnique = RAY_QUERY;
bool     gRaytracingTechniqueSupported[RAYTRACING_TECHNIQUE_COUNT] = {};

struct PropData
{
    Geometry* pGeom = NULL;
    uint32_t  mMaterialCount = 0;
    Buffer*   pIndexBufferOffsetStream = NULL; // one per geometry
    Texture** pTextureStorage = NULL;
    mat4      mWorldMatrix;
};

PropData SanMiguelProp;
Scene*   pScene;

uint32_t gFontID = 0;

struct PathTracingData
{
    mat4   mHistoryProjView;
    float3 mHistoryLightDirection;
    uint   mFrameIndex;
    uint   mHaltonIndex;
    uint   mLastCameraMoveFrame;
    mat4   mWorldToCamera;
    mat4   mProjMat;
    mat4   mProjectView;
    mat4   mCameraToWorld;
    float  mProjNear;
    float  mProjFarMinusNear;
    float2 mZ1PlaneSize;
    float  mRandomSeed;
};

static float haltonSequence(uint index, uint base)
{
    float f = 1.f;
    float r = 0.f;

    while (index > 0)
    {
        f /= (float)base;
        r += f * (float)(index % base);
        index /= base;
    }

    return r;
}

class UnitTest_NativeRaytracing: public IApp
{
public:
    UnitTest_NativeRaytracing()
    {
#ifdef TARGET_IOS
        mSettings.mContentScaleFactor = 1.f;
#endif
        ReadCmdArgs();
    }

    void ReadCmdArgs()
    {
        for (int i = 0; i < argc; i += 1)
        {
            if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
                mSettings.mWidth = min(max(atoi(argv[i + 1]), 64), 10000);
            else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc)
                mSettings.mHeight = min(max(atoi(argv[i + 1]), 64), 10000);
            else if (strcmp(argv[i], "-f") == 0)
            {
                mSettings.mFullScreen = true;
            }
        }
    }

    bool Init()
    {
        /************************************************************************/
        // 01 Init Raytracing
        /************************************************************************/
        RendererDesc settings = {};
        settings.mShaderTarget = SHADER_TARGET_6_3;
#if defined(SHADER_STATS_AVAILABLE)
        settings.mEnableShaderStats = true;
#endif
        initGPUConfiguration(settings.pExtendedSettings);
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
        {
            ShowUnsupportedMessage(getUnsupportedGPUMsg());
            return false;
        }
        setupGPUConfigurationPlatformParameters(pRenderer, settings.pExtendedSettings);

        /************************************************************************/
        // Raytracing setup
        /************************************************************************/
        initRaytracing(pRenderer, &pRaytracing);
        gRaytracingTechniqueSupported[RAY_QUERY] = pRenderer->pGpu->mRayQuerySupported;

        gUseUavRwFallback = !(pRenderer->pGpu->mFormatCaps[TinyImageFormat_R16G16B16A16_SFLOAT] & FORMAT_CAP_READ_WRITE);

        initResourceLoaderInterface(pRenderer);

        RootSignatureDesc rootDesc = {};
        INIT_RS_DESC(rootDesc, "default.rootsig", "compute.rootsig");
        initRootSignature(pRenderer, &rootDesc);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
        initQueue(pRenderer, &queueDesc, &pQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pQueue;
        cmdRingDesc.mPoolCount = gDataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        initGpuCmdRing(pRenderer, &cmdRingDesc, &mCmdRing);

        initSemaphore(pRenderer, &pImageAcquiredSemaphore);

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

        const char* ppGpuProfilerName[1] = { "Graphics" };

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.ppQueues = &pQueue;
        profiler.ppProfilerNames = ppGpuProfilerName;
        profiler.pProfileTokens = &gGpuProfileToken;
        profiler.mGpuProfilerCount = 1;
        initProfiler(&profiler);

        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            VertexLayout vertexLayout = {};
            vertexLayout.mBindingCount = 3;
            vertexLayout.mAttribCount = 3;

            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[0].mOffset = 0;

            // normals
            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32_UINT;
            vertexLayout.mAttribs[1].mLocation = 1;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mOffset = 0;

            // texture
            vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
            vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32_UINT;
            vertexLayout.mAttribs[2].mLocation = 2;
            vertexLayout.mAttribs[2].mBinding = 2;
            vertexLayout.mAttribs[2].mOffset = 0;

            SyncToken        token = {};
            GeometryLoadDesc loadDesc = {};
            loadDesc.pVertexLayout = &vertexLayout;
            loadDesc.mFlags = GEOMETRY_LOAD_FLAG_RAYTRACING_INPUT;
            pScene = initSanMiguel(&loadDesc, token, false);

            SanMiguelProp.mMaterialCount = pScene->geom->mDrawArgCount;
            SanMiguelProp.pGeom = pScene->geom;

            SanMiguelProp.pTextureStorage = (Texture**)tf_malloc(sizeof(Texture*) * SanMiguelProp.mMaterialCount);

            BufferLoadDesc desc = {};
            desc.mDesc.mDescriptors = DESCRIPTOR_TYPE_BUFFER_RAW;
            desc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            desc.mDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
            desc.mDesc.mSize = SanMiguelProp.pGeom->mDrawArgCount * sizeof(uint32_t);
            desc.mDesc.mElementCount = SanMiguelProp.pGeom->mDrawArgCount;
            desc.ppBuffer = &SanMiguelProp.pIndexBufferOffsetStream;
            addResource(&desc, NULL);

            BufferUpdateDesc updateDesc = {};
            updateDesc.pBuffer = SanMiguelProp.pIndexBufferOffsetStream;
            updateDesc.mCurrentState = desc.mDesc.mStartState;
            beginUpdateResource(&updateDesc);
            for (uint32_t i = 0, count = 0; i < SanMiguelProp.pGeom->mDrawArgCount; ++i)
            {
                ((uint32_t*)updateDesc.pMappedData)[count++] = SanMiguelProp.pGeom->pDrawArgs[i].mStartIndex;
            }
            endUpdateResource(&updateDesc);

            for (uint32_t i = 0; i < SanMiguelProp.mMaterialCount; ++i)
            {
                TextureLoadDesc texDesc = {};
                texDesc.pFileName = pScene->textures[i];
                texDesc.ppTexture = &SanMiguelProp.pTextureStorage[i];
                // Textures representing color should be stored in SRGB or HDR format
                texDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
                addResource(&texDesc, NULL);
            }

            waitForAllResourceLoads();

            /************************************************************************/
            // 02 Creation Acceleration Structure
            /************************************************************************/
            AccelerationStructureDesc         asDesc = {};
            AccelerationStructureGeometryDesc geomDescs[1024] = {};

            for (uint32_t i = 0; i < SanMiguelProp.pGeom->mDrawArgCount; i++)
            {
                IndirectDrawIndexArguments& drawArg = SanMiguelProp.pGeom->pDrawArgs[i];
                MaterialFlags               materialFlag = pScene->materialFlags[i];

                geomDescs[i].mFlags = (materialFlag & MATERIAL_FLAG_ALPHA_TESTED)
                                          ? ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION
                                          : ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE;
                geomDescs[i].pVertexBuffer = SanMiguelProp.pGeom->pVertexBuffers[0];
                geomDescs[i].mVertexCount = (uint32_t)SanMiguelProp.pGeom->mVertexCount;
                geomDescs[i].mVertexStride = SanMiguelProp.pGeom->mVertexStrides[0];
                geomDescs[i].mVertexFormat = TinyImageFormat_R32G32B32_SFLOAT;
                geomDescs[i].pIndexBuffer = SanMiguelProp.pGeom->pIndexBuffer;
                geomDescs[i].mIndexCount = drawArg.mIndexCount;
                geomDescs[i].mIndexOffset = drawArg.mStartIndex * sizeof(uint32_t);
                geomDescs[i].mIndexType = INDEX_TYPE_UINT32;
            }

            asDesc.mBottom.mDescCount = SanMiguelProp.pGeom->mDrawArgCount;
            asDesc.mBottom.pGeometryDescs = geomDescs;
            asDesc.mType = ACCELERATION_STRUCTURE_TYPE_BOTTOM;
            asDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            addAccelerationStructure(pRaytracing, &asDesc, &pSanMiguelBottomAS);

            // The transformation matrices for the instances
            SanMiguelProp.mWorldMatrix = mat4::scale(vec3(SCENE_SCALE));

            // Construct descriptions for Acceleration Structures Instances
            AccelerationStructureInstanceDesc instanceDesc = {};
            instanceDesc.mFlags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
            instanceDesc.mInstanceContributionToHitGroupIndex = 0;
            instanceDesc.mInstanceID = 0;
            instanceDesc.mInstanceMask = 1;
            memcpy(instanceDesc.mTransform, &SanMiguelProp.mWorldMatrix, sizeof(float[12]));
            instanceDesc.pBottomAS = pSanMiguelBottomAS;

            asDesc = {};
            asDesc.mType = ACCELERATION_STRUCTURE_TYPE_TOP;
            asDesc.mFlags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            asDesc.mTop.mDescCount = 1;
            asDesc.mTop.pInstanceDescs = &instanceDesc;
            addAccelerationStructure(pRaytracing, &asDesc, &pSanMiguelAS);

            GpuCmdRingElement elem = getNextGpuCmdRingElement(&mCmdRing, true, 1);
            resetCmdPool(pRenderer, elem.pCmdPool);

            // Build Acceleration Structures
            RaytracingBuildASDesc buildASDesc = {};
            buildASDesc.pAccelerationStructure = pSanMiguelBottomAS;
            buildASDesc.mIssueRWBarrier = true;
            beginCmd(elem.pCmds[0]);
            cmdBuildAccelerationStructure(elem.pCmds[0], pRaytracing, &buildASDesc);

            buildASDesc = {};
            buildASDesc.pAccelerationStructure = pSanMiguelAS;
            cmdBuildAccelerationStructure(elem.pCmds[0], pRaytracing, &buildASDesc);

            endCmd(elem.pCmds[0]);

            QueueSubmitDesc submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.ppCmds = elem.pCmds;
            submitDesc.pSignalFence = elem.pFence;
            submitDesc.mSubmitDone = true;
            queueSubmit(pQueue, &submitDesc);
            waitForFences(pRenderer, 1, &elem.pFence);

            removeAccelerationStructureScratch(pRaytracing, pSanMiguelBottomAS);
            removeAccelerationStructureScratch(pRaytracing, pSanMiguelAS);

            /************************************************************************/
            // 04 - Create Shader Binding Table to connect Pipeline with Acceleration Structure
            /************************************************************************/
            BufferLoadDesc ubDesc = {};
            ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            ubDesc.mDesc.mSize = sizeof(ShadersConfigBlock);
            for (uint32_t i = 0; i < gDataBufferCount; i++)
            {
                ubDesc.ppBuffer = &pRayGenConfigBuffer[i];
                addResource(&ubDesc, NULL);
            }

#if USE_DENOISER
            addSSVGFDenoiser(pRenderer, &pDenoiser);
#endif
        }

        CameraMotionParameters cmp{ 200.0f, 250.0f, 300.0f };
        vec3                   camPos{ 80.0f, 60.0f, 50.0f };
        vec3                   lookAt{ 1.0f, 0.5f, 0.0f };

        pCameraController = initFpsCameraController(camPos, lookAt);
        pCameraController->setMotionParameters(cmp);

        // App Actions
        AddCustomInputBindings();
        initScreenshotCapturer(pRenderer, pQueue, GetName());
        mFrameIdx = 0;

        waitForAllResourceLoads();

        return true;
    }

    void Exit()
    {
        exitScreenshotCapturer();

        exitCameraController(pCameraController);

        exitProfiler();

        exitUserInterface();

        exitFontSystem();

        if (pScene)
        {
            removeResource(pScene->geomData);
            pScene->geomData = NULL;
            exitSanMiguel(pScene);
        }

        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
#if USE_DENOISER
            removeSSVGFDenoiser(pDenoiser);
#endif

            for (uint32_t i = 0; i < SanMiguelProp.mMaterialCount; ++i)
            {
                removeResource(SanMiguelProp.pTextureStorage[i]);
            }
            tf_free(SanMiguelProp.pTextureStorage);

            for (uint32_t i = 0; i < gDataBufferCount; i++)
            {
                removeResource(pRayGenConfigBuffer[i]);
            }

            removeAccelerationStructure(pRaytracing, pSanMiguelAS);
            removeAccelerationStructure(pRaytracing, pSanMiguelBottomAS);
            removeResource(SanMiguelProp.pGeom);
            removeResource(SanMiguelProp.pIndexBufferOffsetStream);
        }

        exitSemaphore(pRenderer, pImageAcquiredSemaphore);
        exitGpuCmdRing(pRenderer, &mCmdRing);
        exitQueue(pRenderer, pQueue);
        exitRootSignature(pRenderer);
        exitResourceLoaderInterface(pRenderer);
        exitRaytracing(pRenderer, pRaytracing);
        exitRenderer(pRenderer);
        exitGPUConfiguration();
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        mPathTracingData = {};
        mFrameIdx = 0;

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            loadProfilerUI(mSettings.mWidth, mSettings.mHeight);

            UIComponentDesc guiDesc = {};
            guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
            uiAddComponent(GetName(), &guiDesc, &pGuiWindow);

            static const char* raytracingOptions[] = { "Ray Query" };
            COMPILE_ASSERT(TF_ARRAY_COUNT(raytracingOptions) == RAYTRACING_TECHNIQUE_COUNT);
            if (RAYTRACING_TECHNIQUE_COUNT > 1)
            {
                DropdownWidget raytracingDropdown;
                raytracingDropdown.mCount = TF_ARRAY_COUNT(raytracingOptions);
                raytracingDropdown.pNames = raytracingOptions;
                raytracingDropdown.pData = &gRaytracingTechnique;
                UIWidget* widget = uiAddComponentWidget(pGuiWindow, "Raytracing Technique", &raytracingDropdown, WIDGET_TYPE_DROPDOWN);
                luaRegisterWidget(widget);
            }
            else
            {
                LabelWidget raytracingLabel;
                uiAddComponentWidget(pGuiWindow, raytracingOptions[0], &raytracingLabel, WIDGET_TYPE_LABEL);
            }

            LabelWidget notSupportedLabel;
            uiAddDynamicWidgets(&mDynamicWidgets[0], "Raytracing technique is not supported on this GPU", &notSupportedLabel,
                                WIDGET_TYPE_LABEL);

            if (gRaytracingTechniqueSupported[RAY_QUERY])
            {
                SliderFloatWidget lightDirXSlider;
                lightDirXSlider.pData = &mLightDirection.x;
                lightDirXSlider.mMin = -2.0f;
                lightDirXSlider.mMax = 2.0f;
                lightDirXSlider.mStep = 0.001f;
                luaRegisterWidget(
                    uiAddDynamicWidgets(&mDynamicWidgets[1], "Light Direction X", &lightDirXSlider, WIDGET_TYPE_SLIDER_FLOAT));

                SliderFloatWidget lightDirYSlider;
                lightDirYSlider.pData = &mLightDirection.y;
                lightDirYSlider.mMin = -2.0f;
                lightDirYSlider.mMax = 2.0f;
                lightDirYSlider.mStep = 0.001f;
                luaRegisterWidget(
                    uiAddDynamicWidgets(&mDynamicWidgets[1], "Light Direction Y", &lightDirYSlider, WIDGET_TYPE_SLIDER_FLOAT));

                SliderFloatWidget lightDirZSlider;
                lightDirZSlider.pData = &mLightDirection.z;
                lightDirZSlider.mMin = -2.0f;
                lightDirZSlider.mMax = 2.0f;
                lightDirZSlider.mStep = 0.001f;
                luaRegisterWidget(
                    uiAddDynamicWidgets(&mDynamicWidgets[1], "Light Direction Z", &lightDirZSlider, WIDGET_TYPE_SLIDER_FLOAT));
            }

            if (!addSwapChain())
                return false;
        }

        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
            {
                addShaders();
                addDescriptorSets();
            }

#if USE_DENOISER
            RenderTargetDesc rtDesc = {};
            rtDesc.mClearValue = { { FLT_MAX, 0, 0, 0 } };
            rtDesc.mWidth = mSettings.mWidth;
            rtDesc.mHeight = mSettings.mHeight;
            rtDesc.mDepth = 1;
            rtDesc.mSampleCount = SAMPLE_COUNT_1;
            rtDesc.mSampleQuality = 0;
            rtDesc.mArraySize = 1;
            rtDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;

            rtDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
            addRenderTarget(pRenderer, &rtDesc, &pDepthNormalRenderTarget[0]);
            addRenderTarget(pRenderer, &rtDesc, &pDepthNormalRenderTarget[1]);

            rtDesc.mFormat = TinyImageFormat_R16G16_SFLOAT;
            rtDesc.mClearValue = { { 0, 0 } };
            addRenderTarget(pRenderer, &rtDesc, &pMotionVectorRenderTarget);

            rtDesc.mStartState = RESOURCE_STATE_DEPTH_WRITE;
            rtDesc.mFormat = TinyImageFormat_D32_SFLOAT;
            rtDesc.mClearValue = { { 0.0f, 0 } };
            rtDesc.mFlags = TEXTURE_CREATION_FLAG_ON_TILE;
            addRenderTarget(pRenderer, &rtDesc, &pDepthRenderTarget);
#endif

            if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
            {
                TextureDesc uavDesc = {};
                uavDesc.mArraySize = 1;
                uavDesc.mDepth = 1;
                uavDesc.mFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
                uavDesc.mHeight = mSettings.mHeight;
                uavDesc.mMipLevels = 1;
                uavDesc.mSampleCount = SAMPLE_COUNT_1;
                uavDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                uavDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
                uavDesc.mWidth = mSettings.mWidth;
                uavDesc.pName = "gOutput";
                TextureLoadDesc loadDesc = {};
                loadDesc.pDesc = &uavDesc;
                loadDesc.ppTexture = &pComputeOutput;
                addResource(&loadDesc, NULL);

#if USE_DENOISER
                uavDesc.mFormat = TinyImageFormat_B10G10R10A2_UNORM;
                loadDesc.ppTexture = &pAlbedoTexture;
                addResource(&loadDesc, NULL);
#endif
            }

            if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
            {
                addPipelines();
            }

            prepareDescriptorSets();
        }

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

        updateUIVisibility();

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);

            uiRemoveDynamicWidgets(&mDynamicWidgets[0]);
            uiRemoveDynamicWidgets(&mDynamicWidgets[1]);
            uiRemoveComponent(pGuiWindow);
            unloadProfilerUI();
        }

        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
            {
                removePipelines();
            }

            if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
            {
#if USE_DENOISER
                removeRenderTarget(pRenderer, pMotionVectorRenderTarget);
                removeRenderTarget(pRenderer, pDepthNormalRenderTarget[0]);
                removeRenderTarget(pRenderer, pDepthNormalRenderTarget[1]);
                removeRenderTarget(pRenderer, pDepthRenderTarget);
#endif
            }

            if (pReloadDesc->mType & RELOAD_TYPE_RESIZE)
            {
                removeResource(pComputeOutput);
#if USE_DENOISER
                removeResource(pAlbedoTexture);
#endif
            }

            if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
            {
                removeDescriptorSets();
                removeShaders();
            }
        }
    }

    void Update(float deltaTime)
    {
        PROFILER_SET_CPU_SCOPE("Cpu Profile", "update", 0x222222);

        if (!uiIsFocused())
        {
            pCameraController->onMove({ inputGetValue(0, CUSTOM_MOVE_X), inputGetValue(0, CUSTOM_MOVE_Y) });
            pCameraController->onRotate({ inputGetValue(0, CUSTOM_LOOK_X), inputGetValue(0, CUSTOM_LOOK_Y) });
            pCameraController->onMoveY(inputGetValue(0, CUSTOM_MOVE_UP));
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

        pCameraController->update(deltaTime);

        static uint32_t prevRaytracingTechnique = UINT32_MAX;
        if (gRaytracingTechnique != prevRaytracingTechnique)
        {
            mFrameIdx = 0;
            prevRaytracingTechnique = gRaytracingTechnique;
            mPathTracingData = {};
            updateUIVisibility();
        }

        mat4 viewMat = pCameraController->getViewMatrix().mCamera;

        const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
        const float horizontalFOV = PI / 2.0f;
        const float nearPlane = 0.1f;
        const float farPlane = 1000.f;
        mat4        projMat = mat4::perspectiveLH_ReverseZ(horizontalFOV, aspectInverse, nearPlane, farPlane);
        mat4        projectView = projMat * viewMat;

        mPathTracingData.mWorldToCamera = viewMat;
        mPathTracingData.mProjMat = projMat;
        mPathTracingData.mProjectView = projectView;
        mPathTracingData.mCameraToWorld = inverse(viewMat);
        mPathTracingData.mProjNear = nearPlane;
        mPathTracingData.mProjFarMinusNear = farPlane - nearPlane;
        mPathTracingData.mZ1PlaneSize = float2(1.0f / projMat.getElem(0, 0), 1.0f / projMat.getElem(1, 1));

        mPathTracingData.mRandomSeed = randomFloat01();
    }

    void Draw()
    {
        if ((bool)pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        PROFILER_SET_CPU_SCOPE("Cpu Profile", "draw", 0xffffff);

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        GpuCmdRingElement elem = getNextGpuCmdRingElement(&mCmdRing, true, 1);
        FenceStatus       fenceStatus = {};
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* pCmd = elem.pCmds[0];
        beginCmd(pCmd);
        cmdBeginGpuFrameProfile(pCmd, gGpuProfileToken);

        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        const bool raytracingTechniqueSupported = gRaytracingTechniqueSupported[gRaytracingTechnique];

        if (raytracingTechniqueSupported)
        {
            bool cameraMoved = memcmp(&mPathTracingData.mProjectView, &mPathTracingData.mHistoryProjView, sizeof(mat4)) != 0;
            bool lightMoved = memcmp(&mLightDirection, &mPathTracingData.mHistoryLightDirection, sizeof(float3)) != 0; //-V1014

#if USE_DENOISER
            if (lightMoved)
            {
                clearSSVGFDenoiserTemporalHistory(pDenoiser);
            }
#else
            if (cameraMoved || lightMoved)
            {
                mPathTracingData.mFrameIndex = 0;
                mPathTracingData.mHaltonIndex = 0;
            }
#endif
            if (cameraMoved)
            {
                mPathTracingData.mLastCameraMoveFrame = mPathTracingData.mFrameIndex;
            }

            ShadersConfigBlock cb;
            cb.mCameraToWorld = mPathTracingData.mCameraToWorld;
            cb.mProjNear = mPathTracingData.mProjNear;
            cb.mProjFarMinusNear = mPathTracingData.mProjFarMinusNear;
            cb.mZ1PlaneSize = mPathTracingData.mZ1PlaneSize;
            cb.mLightDirection = v3ToF3(normalize(f3Tov3(mLightDirection)));

            cb.mRandomSeed = mPathTracingData.mRandomSeed;

            // Loop through the first 16 items in the Halton sequence.
            // The Halton sequence takes one-based indices.
            cb.mSubpixelJitter =
                float2(haltonSequence(mPathTracingData.mHaltonIndex + 1, 2), haltonSequence(mPathTracingData.mHaltonIndex + 1, 3));

            cb.mFrameIndex = mPathTracingData.mFrameIndex;

            cb.mFramesSinceCameraMove = mPathTracingData.mFrameIndex - mPathTracingData.mLastCameraMoveFrame;
            cb.mWidth = mSettings.mWidth;
            cb.mHeight = mSettings.mHeight;
            cb.mWorldToCamera = mPathTracingData.mWorldToCamera;
            cb.mCameraToProjection = mPathTracingData.mProjMat;
            cb.mWorldToProjectionPrevious = mPathTracingData.mHistoryProjView;
            cb.mRtInvSize = float2(1.0f / mSettings.mWidth, 1.0f / mSettings.mHeight);

            cb.mWorldMatrix = SanMiguelProp.mWorldMatrix;

            BufferUpdateDesc bufferUpdate = { pRayGenConfigBuffer[mFrameIdx] };
            beginUpdateResource(&bufferUpdate);
            memcpy(bufferUpdate.pMappedData, &cb, sizeof(cb));
            endUpdateResource(&bufferUpdate);

            mPathTracingData.mHistoryProjView = mPathTracingData.mProjectView;
            mPathTracingData.mHistoryLightDirection = mLightDirection;
            mPathTracingData.mFrameIndex += 1;
            mPathTracingData.mHaltonIndex = (mPathTracingData.mHaltonIndex + 1) % 16;

#if USE_DENOISER
            RenderTarget* depthNormalTarget = pDepthNormalRenderTarget[mPathTracingData.mFrameIndex & 0x1];

            RenderTargetBarrier barriers[] = {
                { depthNormalTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
                { pMotionVectorRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            RenderTarget*   denoiserRTs[] = { depthNormalTarget, pMotionVectorRenderTarget };
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
            loadActions.mClearColorValues[0] = { { FLT_MAX, 0, 0, 0 } };
            loadActions.mLoadActionsColor[1] = LOAD_ACTION_CLEAR;
            loadActions.mClearColorValues[1] = { { 0, 0, 0, 0 } };
            loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
            loadActions.mClearDepth = { { 0.f } };

            cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Generate Denoiser Inputs");
            BindRenderTargetsDesc bindRenderTargets = {};
            bindRenderTargets.mRenderTargetCount = 2;
            bindRenderTargets.ppRenderTargets = denoiserRTs;
            bindRenderTargets.pDepthStencil = pDepthRenderTarget;
            bindRenderTargets.pLoadActions = &loadActions;
            bindRenderTargets.mDepthArraySlice = 0;
            bindRenderTargets.mDepthMipSlice = 0;
            cmdBindRenderTargets(pCmd, &bindRenderTargets);
            cmdSetViewport(pCmd, 0.0f, 0.0f, (float)pDepthRenderTarget->mWidth, (float)pDepthRenderTarget->mHeight, 0.0f, 1.0f);
            cmdSetScissor(pCmd, 0, 0, pDepthRenderTarget->mWidth, pDepthRenderTarget->mHeight);

            cmdBindPipeline(pCmd, pDenoiserInputsPipeline);

            cmdBindDescriptorSet(pCmd, mFrameIdx, pDenoiserInputsDescriptorSet);

            cmdBindVertexBuffer(pCmd, 2, SanMiguelProp.pGeom->pVertexBuffers, SanMiguelProp.pGeom->mVertexStrides, NULL);

            cmdBindIndexBuffer(pCmd, SanMiguelProp.pGeom->pIndexBuffer, 0, (IndexType)SanMiguelProp.pGeom->mIndexType);
            cmdDrawIndexed(pCmd, SanMiguelProp.pGeom->mIndexCount, 0, 0);

            cmdBindRenderTargets(pCmd, NULL);

            barriers[0] = { depthNormalTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            barriers[1] = { pMotionVectorRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 2, barriers);

            cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
#endif
            /************************************************************************/
            // Transition UAV texture so raytracing shader can write to it
            /************************************************************************/
            cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Path Trace Scene");
            TextureBarrier uavBarriers[] = {
                { pComputeOutput, RESOURCE_STATE_PIXEL_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS },
            };
            cmdResourceBarrier(pCmd, 0, NULL, TF_ARRAY_COUNT(uavBarriers), uavBarriers, 0, NULL);
            /************************************************************************/
            // Perform raytracing
            /************************************************************************/
            cmdBindPipeline(pCmd, pPipeline[gRaytracingTechnique]);

            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetRayTracingPersistent[gRaytracingTechnique]);
            cmdBindDescriptorSet(pCmd, mFrameIdx, pDescriptorSetRayTracingPerFrame[gRaytracingTechnique]);
            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetRaytracingPerBatch[gRaytracingTechnique]);

            if (RAY_QUERY == gRaytracingTechnique)
            {
                const uint32_t* numThreads = pShaderRayQuery->mNumThreadsPerGroup;
                uint32_t        groupX = round_up(mSettings.mWidth, numThreads[0]) / numThreads[0];
                uint32_t        groupY = round_up(mSettings.mHeight, numThreads[1]) / numThreads[1];
                cmdDispatch(pCmd, groupX, groupY, 1);
            }
            /************************************************************************/
            // Transition UAV to be used as source and swapchain as destination in copy operation
            /************************************************************************/
            TextureBarrier copyBarriers[] = {
                { pComputeOutput, RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_PIXEL_SHADER_RESOURCE },
            };
            RenderTargetBarrier rtCopyBarriers[] = {
                { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(pCmd, 0, NULL, 1, copyBarriers, 1, rtCopyBarriers);

#if USE_DENOISER
            Texture* denoisedTexture = NULL;
            cmdSSVGFDenoise(pCmd, pDenoiser, pComputeOutput, pMotionVectorRenderTarget->pTexture,
                            pDepthNormalRenderTarget[mPathTracingData.mFrameIndex & 0x1]->pTexture,
                            pDepthNormalRenderTarget[(mPathTracingData.mFrameIndex + 1) & 0x1]->pTexture, &denoisedTexture);

            DescriptorData params[1] = {};
            params[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gDisplayTexture);
            params[0].ppTextures = &denoisedTexture;
            updateDescriptorSet(pRenderer, mFrameIdx, pDescriptorSetPerFrame, 1, params);

            removeResource(denoisedTexture);
#endif

            cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
        }
        else
        {
            RenderTargetBarrier rtCopyBarriers[] = {
                { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
            };
            cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, rtCopyBarriers);
        }
        /************************************************************************/
        // Present to screen
        /************************************************************************/
        if (raytracingTechniqueSupported)
        {
            cmdBeginGpuTimestampQuery(pCmd, gGpuProfileToken, "Render result");
        }

        BindRenderTargetsDesc bindRenderTargets = {};
        bindRenderTargets.mRenderTargetCount = 1;
        bindRenderTargets.mRenderTargets[0] = { pRenderTarget, LOAD_ACTION_CLEAR };
        cmdBindRenderTargets(pCmd, &bindRenderTargets);
        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)mSettings.mWidth, (float)mSettings.mHeight, 0.0f, 1.0f);
        cmdSetScissor(pCmd, 0, 0, mSettings.mWidth, mSettings.mHeight);

        if (raytracingTechniqueSupported)
        {
            /************************************************************************/
            // Perform copy
            /************************************************************************/
            // Draw computed results
            cmdBindPipeline(pCmd, pDisplayTexturePipeline);
            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetRayTracingPersistent[gRaytracingTechnique]);
            cmdBindDescriptorSet(pCmd, 0, pDescriptorSetRaytracingPerBatch[gRaytracingTechnique]);
            cmdBindDescriptorSet(pCmd, mFrameIdx, pDescriptorSetPerFrame);
            cmdDraw(pCmd, 3, 0);
            cmdEndGpuTimestampQuery(pCmd, gGpuProfileToken);
        }

        cmdBeginDebugMarker(pCmd, 0, 1, 0, "Draw UI");

        FontDrawDesc frameTimeDraw;
        frameTimeDraw.mFontColor = 0xff0080ff;
        frameTimeDraw.mFontSize = 18.0f;
        frameTimeDraw.mFontID = gFontID;
        float2 txtSize = cmdDrawCpuProfile(pCmd, float2(8.0f, 15.0f), &frameTimeDraw);
        cmdDrawGpuProfile(pCmd, float2(8.f, txtSize.y + 75.f), gGpuProfileToken, &frameTimeDraw);

        cmdDrawUserInterface(pCmd);
        cmdBindRenderTargets(pCmd, NULL);
        cmdEndDebugMarker(pCmd);

        RenderTargetBarrier presentBarrier = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(pCmd, 0, NULL, 0, NULL, 1, &presentBarrier);

        cmdEndGpuFrameProfile(pCmd, gGpuProfileToken);

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
        queueSubmit(pQueue, &submitDesc);
        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = (uint8_t)swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.mSubmitDone = true;
        queuePresent(pQueue, &presentDesc);
        flipProfiler();

        mFrameIdx = (mFrameIdx + 1) % gDataBufferCount;
        /************************************************************************/
        /************************************************************************/
    }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mColorClearValue = {};
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.ppPresentQueues = &pQueue;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        for (uint32_t t = 0; t < RAYTRACING_TECHNIQUE_COUNT; ++t)
        {
            DescriptorSetDesc setDesc = SRT_SET_DESC_LARGE_RW(SrtData, Persistent, 1, 0);
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetRayTracingPersistent[t]);
            setDesc = SRT_SET_DESC_LARGE_RW(SrtData, PerFrame, gDataBufferCount, 0);
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetRayTracingPerFrame[t]);
            setDesc = SRT_SET_DESC_LARGE_RW(SrtData, PerBatch, 1, 0);
            addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetRaytracingPerBatch[t]);
        }

        DescriptorSetDesc setDesc = SRT_SET_DESC_LARGE_RW(SrtData, PerFrame, gDataBufferCount, 0);
        addDescriptorSet(pRenderer, &setDesc, &pDescriptorSetPerFrame);

#if USE_DENOISER
        setDesc = SRT_SET_DESC(SrtData, PerFrame, gDataBufferCount, 0, pDescriptorSetRayTracingPersistent[0]);
        addDescriptorSet(pRenderer, &setDesc, &pDenoiserInputsDescriptorSet);
#endif
    }

    void removeDescriptorSets()
    {
#if USE_DENOISER
        removeDescriptorSet(pRenderer, pDenoiserInputsDescriptorSet);
#endif
        removeDescriptorSet(pRenderer, pDescriptorSetPerFrame);

        for (uint32_t t = 0; t < RAYTRACING_TECHNIQUE_COUNT; ++t)
        {
            removeDescriptorSet(pRenderer, pDescriptorSetRaytracingPerBatch[t]);
            removeDescriptorSet(pRenderer, pDescriptorSetRayTracingPersistent[t]);
            removeDescriptorSet(pRenderer, pDescriptorSetRayTracingPerFrame[t]);
        }
    }

    void addShaders()
    {
        /************************************************************************/
        // Create Raytracing Shaders
        /************************************************************************/
        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            ShaderLoadDesc desc = {};
            desc.mComp.pFileName = USE_DENOISER ? (gUseUavRwFallback ? "RayQuery_denoise_rw_fallback.comp" : "RayQuery_denoise.comp")
                                                : (gUseUavRwFallback ? "RayQuery_rw_fallback.comp" : "RayQuery.comp");
            addShader(pRenderer, &desc, &pShaderRayQuery);
        }

#if USE_DENOISER
        ShaderLoadDesc denoiserShader = {};
        denoiserShader.mVert.pFileName = "DenoiserInputsPass.vert";
        denoiserShader.mFrag.pFileName = "DenoiserInputsPass.frag";
        addShader(pRenderer, &denoiserShader, &pDenoiserInputsShader);
#endif

        /************************************************************************/
        // Blit texture
        /************************************************************************/
        const char* displayTextureVertShader[2] = { "DisplayTexture.vert", "DisplayTexture_USE_DENOISER.vert" };

        const char* displayTextureFragShader[2] = { "DisplayTexture.frag", "DisplayTexture_USE_DENOISER.frag" };

        ShaderLoadDesc displayShader = {};
        displayShader.mVert.pFileName = displayTextureVertShader[USE_DENOISER];
        displayShader.mFrag.pFileName = displayTextureFragShader[USE_DENOISER];
        addShader(pRenderer, &displayShader, &pDisplayTextureShader);
    }

    void removeShaders()
    {
#if USE_DENOISER
        removeShader(pRenderer, pDenoiserInputsShader);
#endif
        removeShader(pRenderer, pDisplayTextureShader);

        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            removeShader(pRenderer, pShaderRayQuery);
        }
    }

    void addPipelines()
    {
        /************************************************************************/
        //  Create Raytracing Pipelines
        /************************************************************************/
        if (gRaytracingTechniqueSupported[RAY_QUERY])
        {
            PipelineDesc rtPipelineDesc = {};
            rtPipelineDesc.mType = PIPELINE_TYPE_COMPUTE;
            PIPELINE_LAYOUT_DESC(rtPipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame),
                                 SRT_LAYOUT_DESC(SrtData, PerBatch), NULL);
            ComputePipelineDesc& pipelineDesc = rtPipelineDesc.mComputeDesc;
            pipelineDesc.pShaderProgram = pShaderRayQuery;
            addPipeline(pRenderer, &rtPipelineDesc, &pPipeline[RAY_QUERY]);

#if defined(SHADER_STATS_AVAILABLE)
            {
                PipelineStats pipelineStats = {};
                addPipelineStats(pRenderer, pPipeline[RAY_QUERY], false, &pipelineStats);
                ShaderStats& stats = pipelineStats.mComp;
                if (stats.mValid)
                {
                    LOGF(eINFO,
                         "Ray query shader stats\n"
                         "    VGPRS         : Used %u / Physical %u / Available %u\n"
                         "    SGPRS         : Used %u / Physical %u / Available %u\n"
                         "    LDS size      : %u\n"
                         "    LDS usage     : %u\n"
                         "    Scratch usage : %u\n",
                         stats.mUsedVgprs, stats.mPhysicalVgprs, stats.mAvailableVgprs, stats.mUsedSgprs, stats.mPhysicalSgprs,
                         stats.mAvailableSgprs, stats.mLdsSizePerLocalWorkGroup, stats.mLdsUsageSizeInBytes, stats.mScratchMemUsageInBytes);
                }
                removePipelineStats(pRenderer, &pipelineStats);
            }
#endif
        }

#if USE_DENOISER
        {
            RasterizerStateDesc rasterState = {};
            rasterState.mCullMode = CULL_MODE_BACK;
            rasterState.mFrontFace = FRONT_FACE_CW;

            DepthStateDesc depthStateDesc = {};
            depthStateDesc.mDepthTest = true;
            depthStateDesc.mDepthWrite = true;
            depthStateDesc.mDepthFunc = CMP_GEQUAL;

            PipelineDesc pipelineDesc = {};
            pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
            PIPELINE_LAYOUT_DESC(pipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame),
                                 SRT_LAYOUT_DESC(SrtData, PerBatch), NULL);

            TinyImageFormat rtFormats[] = { pDepthNormalRenderTarget[0]->mFormat, pMotionVectorRenderTarget->mFormat };

            VertexLayout vertexLayout = {};
            vertexLayout.mBindingCount = 2;
            vertexLayout.mAttribCount = 2;
            vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
            vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[0].mBinding = 0;
            vertexLayout.mAttribs[0].mLocation = 0;
            vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
            vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
            vertexLayout.mAttribs[1].mBinding = 1;
            vertexLayout.mAttribs[1].mLocation = 1;

            GraphicsPipelineDesc& pipelineSettings = pipelineDesc.mGraphicsDesc;
            pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
            pipelineSettings.pRasterizerState = &rasterState;
            pipelineSettings.mRenderTargetCount = 2;
            pipelineSettings.pColorFormats = rtFormats;
            pipelineSettings.mDepthStencilFormat = pDepthRenderTarget->mFormat;
            pipelineSettings.pDepthState = &depthStateDesc;
            pipelineSettings.mSampleCount = SAMPLE_COUNT_1;
            pipelineSettings.mSampleQuality = 0;
            pipelineSettings.pVertexLayout = &vertexLayout;
            pipelineSettings.pRootSignature = pDenoiserInputsRootSignature;
            pipelineSettings.pShaderProgram = pDenoiserInputsShader;

            addPipeline(pRenderer, &pipelineDesc, &pDenoiserInputsPipeline);
        }
#endif

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        PipelineDesc graphicsPipelineDesc = {};
        graphicsPipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;
        PIPELINE_LAYOUT_DESC(graphicsPipelineDesc, SRT_LAYOUT_DESC(SrtData, Persistent), SRT_LAYOUT_DESC(SrtData, PerFrame),
                             SRT_LAYOUT_DESC(SrtData, PerBatch), NULL);
        GraphicsPipelineDesc& pipelineSettings = graphicsPipelineDesc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pVertexLayout = NULL;
        pipelineSettings.pShaderProgram = pDisplayTextureShader;
        addPipeline(pRenderer, &graphicsPipelineDesc, &pDisplayTexturePipeline);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pDisplayTexturePipeline);
#if USE_DENOISER
        removePipeline(pRenderer, pDenoiserInputsPipeline);
#endif
        for (uint32_t t = 0; t < RAYTRACING_TECHNIQUE_COUNT; ++t)
        {
            removePipeline(pRenderer, pPipeline[t]);
        }
    }

    void prepareDescriptorSets()
    {
        DescriptorData perFrameParams[7] = {};
        DescriptorData perBatchParams[5] = {};

        perFrameParams[0].mIndex = SRT_RES_IDX(SrtData, Persistent, gRtScene);
        perFrameParams[0].ppAccelerationStructures = &pSanMiguelAS;
        perFrameParams[1].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexDataBuffer);
        perFrameParams[1].ppBuffers = &SanMiguelProp.pGeom->pIndexBuffer;
        perFrameParams[2].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexPositionBuffer);
        perFrameParams[2].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[0];
        perFrameParams[3].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexNormalBuffer);
        perFrameParams[3].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[1];
        perFrameParams[4].mIndex = SRT_RES_IDX(SrtData, Persistent, gVertexTexCoordBuffer);
        perFrameParams[4].ppBuffers = &SanMiguelProp.pGeom->pVertexBuffers[2];
        perFrameParams[5].mIndex = SRT_RES_IDX(SrtData, Persistent, gIndexOffsets);
        perFrameParams[5].ppBuffers = &SanMiguelProp.pIndexBufferOffsetStream;
        perFrameParams[6].mIndex = SRT_RES_IDX(SrtData, Persistent, gMaterialTextures);
        perFrameParams[6].ppTextures = SanMiguelProp.pTextureStorage;
        perFrameParams[6].mCount = SanMiguelProp.mMaterialCount;

        perBatchParams[0].mIndex = SRT_RES_IDX(SrtData, PerBatch, gOutput);
        perBatchParams[0].ppTextures = &pComputeOutput;

        uint32_t paramIndex = 1;
        if (gUseUavRwFallback)
        {
            perBatchParams[paramIndex].mIndex = SRT_RES_IDX(SrtData, PerBatch, gInput);
            perBatchParams[paramIndex].ppTextures = &pComputeOutput;
            ++paramIndex;
        }
#if USE_DENOISER
        perBatchParams[paramIndex].mIndex = SRT_RES_IDX(SrtData, PerBatch, gAlbedoOutput);
        perBatchParams[paramIndex].ppTextures = &pAlbedoTexture;
        ++paramIndex;
        if (gUseUavRwFallback)
        {
            perBatchParams[paramIndex].mIndex = SRT_RES_IDX(SrtData, PerBatch, gAlbedoInput);
            perBatchParams[paramIndex].ppTextures = &pAlbedoTexture;
            ++paramIndex;
        }
#endif
        for (uint32_t t = 0; t < RAYTRACING_TECHNIQUE_COUNT; ++t)
        {
            updateDescriptorSet(pRenderer, 0, pDescriptorSetRayTracingPersistent[t], 7, perFrameParams);
            updateDescriptorSet(pRenderer, 0, pDescriptorSetRaytracingPerBatch[t], paramIndex, perBatchParams);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                DescriptorData uParams[1] = {};
                uParams[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gSettings);
                uParams[0].ppBuffers = &pRayGenConfigBuffer[i];
                updateDescriptorSet(pRenderer, i, pDescriptorSetRayTracingPerFrame[t], 1, uParams);
            }
        }
        DescriptorData params[7] = {};
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].mIndex = SRT_RES_IDX(SrtData, PerFrame, gDisplayTexture);
            params[0].ppTextures = &pComputeOutput;
#if USE_DENOISER
            params[1].mIndex = SRT_RES_IDX(SrtData, PerFrame, gAlbedoTex);
            params[1].ppTextures = &pAlbedoTexture;
#endif
            updateDescriptorSet(pRenderer, i, pDescriptorSetPerFrame, 1 + USE_DENOISER, params);
        }

#if USE_DENOISER
        params[0].pName = "gSettings";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].ppBuffers = &pRayGenConfigBuffer[i];
            updateDescriptorSet(pRenderer, i, pDenoiserInputsDescriptorSet, 1, params);
        }
#endif
    }

    void updateUIVisibility()
    {
        uiHideDynamicWidgets(&mDynamicWidgets[0], pGuiWindow);
        uiHideDynamicWidgets(&mDynamicWidgets[1], pGuiWindow);

        const bool raytracingTechniqueSupported = gRaytracingTechniqueSupported[gRaytracingTechnique];
        uiShowDynamicWidgets(&mDynamicWidgets[raytracingTechniqueSupported], pGuiWindow);
        uiHideDynamicWidgets(&mDynamicWidgets[!raytracingTechniqueSupported], pGuiWindow);
    }

    const char* GetName() { return "16_Raytracing"; }
    /************************************************************************/
    // Data
    /************************************************************************/
private:
    // Two sets of resources (one in flight and one being used on CPU)
    static const uint32_t gDataBufferCount = 2;

    Renderer*              pRenderer = NULL;
    Raytracing*            pRaytracing = NULL;
    Queue*                 pQueue = NULL;
    GpuCmdRing             mCmdRing = {};
    Buffer*                pRayGenConfigBuffer[gDataBufferCount] = {};
    AccelerationStructure* pSanMiguelBottomAS = NULL;
    AccelerationStructure* pSanMiguelAS = NULL;
    Shader*                pShaderRayQuery = NULL;
    Shader*                pDisplayTextureShader = NULL;
    DescriptorSet*         pDescriptorSetRayTracingPersistent[RAYTRACING_TECHNIQUE_COUNT] = {};
    DescriptorSet*         pDescriptorSetRaytracingPerBatch[RAYTRACING_TECHNIQUE_COUNT] = {};
    DescriptorSet*         pDescriptorSetRayTracingPerFrame[RAYTRACING_TECHNIQUE_COUNT] = {};
    DescriptorSet*         pDescriptorSetPerFrame = NULL;
    Pipeline*              pPipeline[RAYTRACING_TECHNIQUE_COUNT] = {};
    Pipeline*              pDisplayTexturePipeline = NULL;
    SwapChain*             pSwapChain = NULL;
    Texture*               pComputeOutput = NULL;
    Semaphore*             pImageAcquiredSemaphore = NULL;
    uint32_t               mFrameIdx = 0;
    PathTracingData        mPathTracingData = {};
    UIComponent*           pGuiWindow = NULL;
    DynamicUIWidgets       mDynamicWidgets[2] = {};
    float3                 mLightDirection = float3(0.2f, 1.8f, 0.1f);

#if USE_DENOISER
    Texture*       pAlbedoTexture = NULL;
    DescriptorSet* pDenoiserInputsDescriptorSet = NULL;
    RenderTarget*  pDepthNormalRenderTarget[2] = {};
    RenderTarget*  pMotionVectorRenderTarget = NULL;
    RenderTarget*  pDepthRenderTarget = NULL;
    RootSignature* pDenoiserInputsRootSignature = NULL;
    Shader*        pDenoiserInputsShader = NULL;
    Pipeline*      pDenoiserInputsPipeline = NULL;
    SSVGFDenoiser* pDenoiser = NULL;
#endif
};

DEFINE_APPLICATION_MAIN(UnitTest_NativeRaytracing)
