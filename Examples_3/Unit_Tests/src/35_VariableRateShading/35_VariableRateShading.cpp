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

/*
 * Unit Test for Variable rate shading
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
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Demo structures
#define CUBES_COUNT 9 // Must match with the value in shaderDefs.h

// #NOTE: Two sets of resources (one in flight and one being used on CPU)
const uint32_t gDataBufferCount = 2;
const uint32_t gDivider = 2;
const uint32_t gCuboidVertexCount = 36;
bool           gClearHistoryTextures = false;

SampleLocations gLocations[4] = { { -4, -4 }, { 4, -4 }, { -4, 4 }, { 4, 4 } };

struct CubeInfoStruct
{
    vec4 mColor;
    mat4 mLocalMat;
} gCubesInfo[CUBES_COUNT];

struct UniformBlock
{
    mat4 mProjectView;
    mat4 mToWorldMat[CUBES_COUNT];
    vec4 mColor[CUBES_COUNT];
};

struct Matrices
{
    mat4 mProjectView;
    mat4 mPrevProjectView;
    mat4 mToWorldMat[CUBES_COUNT];
    mat4 mPrevToWorldMat[CUBES_COUNT];
};

struct ScreenSize
{
    int32_t mWidth;
    int32_t mHeight;
};

struct BackgroundData
{
    uint32_t   kernelSize;
    ScreenSize size;
};

BackgroundData gBGData = { 21, {} };

Renderer*  pRenderer = NULL;
Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Semaphore*    pImageAcquiredSemaphore = NULL;

uint32_t gShadingRateRootConstantIndex = 0;
uint32_t gScreenSettingsConstantIndex = 0;
uint32_t gKernelSizeConstantIndex = 0;
uint32_t gVelocityScreenSettingsConstantIndex = 0;

Shader*        pResolveShader = NULL;
Shader*        pPlaneShader = NULL;
Shader*        pCubeShader = NULL;
Shader*        pFillStencilShader = NULL;
Shader*        pResolveComputeShader = NULL;
Shader*        pVelocityShader = NULL;
Pipeline*      pCubePipeline = NULL;
Pipeline*      pPlanePipeline = NULL;
Pipeline*      pResolvePipeline = NULL;
Pipeline*      pResolveComputePipeline = NULL;
Pipeline*      pFillStencilPipeline = NULL;
Pipeline*      pVelocityPipeline = NULL;
Pipeline*      pVelocityPipelineClear = NULL;
RootSignature* pRootSignature = NULL;
RootSignature* pRootSignatureFillStencil = NULL;
RootSignature* pRootSignatureVelocity = NULL;
RootSignature* pRootSignatureResolveCompute = NULL;
RootSignature* pRootSignatureComposite = NULL;
RenderTarget*  pColorRenderTarget = NULL;
RenderTarget*  pHistoryRenderTarget[gDataBufferCount] = {};
RenderTarget*  pVelocityRenderTarget = NULL;
DescriptorSet* pDescriptorSetTextures = {};
DescriptorSet* pDescriptorSetCubes = {};
DescriptorSet* pDescriptorSetMatrices = {};
DescriptorSet* pResolveDescriptorSet = {};
DescriptorSet* pFillStencilDescriptorSet = {};
DescriptorSet* pResolveComputeDescriptorSet = {};
Buffer*        pUniformBuffer[gDataBufferCount] = {};
Buffer*        pMatricesBuffer[gDataBufferCount] = {};
Buffer*        pCuboidVertexBuffer = NULL;
Texture*       pPaletteTexture = NULL;
Texture*       pResolvedTexture[gDataBufferCount] = {};
Texture*       pTestTexture = NULL;
UniformBlock   gUniformData = {};
Matrices       gMatrices = {};
uint32_t       gDebugViewRootConstantIndex = 0;

Sampler*    pStaticSampler = { NULL };
const char* pStaticSamplerName = "uSampler";

uint32_t     gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

ICameraController* pCameraController = NULL;

UIComponent* pGuiWindow;
bool         bToggleVRS = true;
bool         bToggleDebugView = false;
bool         bDrawCubes = true;
/// UI
FontDrawDesc gFrameTimeDraw;
uint32_t     gFontID = 0;

void takeScreenshot(void* pUserData) { setCaptureScreenshot("35_VariableRateShading.png"); }

class VariableRateShading: public IApp
{
public:
    VariableRateShading() {}

    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mShaderTarget = SHADER_TARGET_6_4;
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

        if (!pRenderer->pGpu->mSettings.mSoftwareVRSSupported)
        {
            ShowUnsupportedMessage("GPU does not support programmable sample positions");
            return false;
        }

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
        // Load textures
        TextureLoadDesc loadDesc = {};
        loadDesc.pFileName = "Lion_Albedo.tex";
        loadDesc.ppTexture = &pPaletteTexture;
        // Textures representing color should be stored in SRGB or HDR format
        loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        addResource(&loadDesc, NULL);

        loadDesc = {};
        loadDesc.pFileName = "Lion_Albedo.tex";
        loadDesc.ppTexture = &pTestTexture;
        // Textures representing color should be stored in SRGB or HDR format
        loadDesc.mCreationFlag = TEXTURE_CREATION_FLAG_SRGB;
        addResource(&loadDesc, NULL);

        // Sampler
        SamplerDesc samplerDesc = {
            FILTER_NEAREST, FILTER_NEAREST, MIPMAP_MODE_NEAREST, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT,
        };
        addSampler(pRenderer, &samplerDesc, &pStaticSampler);

        // Color palette
        {
            BufferLoadDesc ubDesc = {};
            ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
            ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
            ubDesc.pData = NULL;
            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                ubDesc.mDesc.mSize = sizeof(UniformBlock);
                ubDesc.ppBuffer = &pUniformBuffer[i];
                addResource(&ubDesc, NULL);

                ubDesc.mDesc.mSize = sizeof(Matrices);
                ubDesc.ppBuffer = &pMatricesBuffer[i];
                addResource(&ubDesc, NULL);
            }
        }

        // Cubes
        {
            // scene data
            for (int i = 0; i < CUBES_COUNT; ++i)
            {
                gCubesInfo[i].mColor = vec4(float(i % 3), float((i + 1) % 3), float((i + 2) % 3), 1.0f);
                gUniformData.mColor[i] = gCubesInfo[i].mColor;
            }

            // Vertex buffer
            addCube(0.4f, 0.4f, 0.4f);
        }

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

        // Initialize micro profiler and its UI.
        ProfilerDesc profiler = {};
        profiler.pRenderer = pRenderer;
        profiler.mWidthUI = mSettings.mWidth;
        profiler.mHeightUI = mSettings.mHeight;
        initProfiler(&profiler);

        // Gpu profiler can only be added after initProfile.
        gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

        /************************************************************************/
        // GUI
        /************************************************************************/
        UIComponentDesc guiDesc = {};
        guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.2f);
        uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

#if defined(FORGE_DEBUG) || defined(AUTOMATED_TESTING)
        // Screenshots aren't supported on non-debug builds, since they require the RM_DEBUG mounting point
        ButtonWidget screenshot;
        UIWidget*    pScreenshot = uiCreateComponentWidget(pGuiWindow, "Screenshot", &screenshot, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(pScreenshot, nullptr, takeScreenshot);
        luaRegisterWidget(pScreenshot);
#endif

        CheckboxWidget toggleVRSCheckbox;
        toggleVRSCheckbox.pData = &bToggleVRS;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Toggle VRS\t\t\t\t\t", &toggleVRSCheckbox, WIDGET_TYPE_CHECKBOX));

        CheckboxWidget toggleDrawCubesCheckbox;
        toggleDrawCubesCheckbox.pData = &bDrawCubes;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Draw Cubes\t\t\t\t\t", &toggleDrawCubesCheckbox, WIDGET_TYPE_CHECKBOX));

        CheckboxWidget toggleDebugCheckbox;
        toggleDebugCheckbox.pData = &bToggleDebugView;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Toggle Debug View\t\t\t\t\t", &toggleDebugCheckbox, WIDGET_TYPE_CHECKBOX));

        SliderUintWidget kernelSizeSlider;
        kernelSizeSlider.pData = &gBGData.kernelSize;
        kernelSizeSlider.mMin = 1;
        kernelSizeSlider.mMax = 21;
        kernelSizeSlider.mStep = 2;
        luaRegisterWidget(uiCreateComponentWidget(pGuiWindow, "Blur kernel Size", &kernelSizeSlider, WIDGET_TYPE_SLIDER_UINT));

        waitForAllResourceLoads();

        CameraMotionParameters cmp{ 60.0f, 100.0f, 20.0f };
        vec3                   camPos{ 48.0f, 48.0f, 20.0f };
        vec3                   lookAt{ vec3(0) };
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

        typedef bool (*CameraInputHandler)(InputActionContext* ctx, DefaultInputActions::DefaultInputAction action);
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

    void Exit()
    {
        exitInputSystem();

        exitCameraController(pCameraController);

        exitUserInterface();

        exitFontSystem();

        exitProfiler();

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            removeResource(pUniformBuffer[i]);
            removeResource(pMatricesBuffer[i]);
        }

        removeResource(pPaletteTexture);
        removeResource(pTestTexture);
        removeResource(pCuboidVertexBuffer);

        removeSampler(pRenderer, pStaticSampler);

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer);
        removeQueue(pRenderer, pGraphicsQueue);
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
            gBGData.size.mWidth = mSettings.mWidth;
            gBGData.size.mHeight = mSettings.mHeight;

            if (!addSwapChain())
                return false;

            if (!addDepthBuffer())
                return false;

            if (!addColorRenderTarget())
                return false;

            if (!addHistoryRenderTarget())
                return false;

            if (!addVelocityRenderTarget())
                return false;

            if (!addResolvedTexture())
                return false;
        }

        if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
        {
            addPipelines();
        }

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
            removeRenderTarget(pRenderer, pDepthBuffer);
            removeRenderTarget(pRenderer, pColorRenderTarget);
            removeRenderTarget(pRenderer, pVelocityRenderTarget);

            for (uint32_t i = 0; i < gDataBufferCount; ++i)
            {
                removeRenderTarget(pRenderer, pHistoryRenderTarget[i]);
                removeResource(pResolvedTexture[i]);
            }
        }

        if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
        {
            removeDescriptorSets();
            removeRootSignatures();
            removeShaders();
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);
        pCameraController->update(deltaTime);

        // Update Scene
        {
            // camera
            mat4 viewMat = pCameraController->getViewMatrix();

            const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
            const float horizontal_fov = PI / 2.0f;
            mat4        projMat = mat4::perspectiveLH_ReverseZ(horizontal_fov, aspectInverse, 1000.0f, 0.1f);
            gUniformData.mProjectView = projMat * viewMat;

            // cubes
            static float rot = 0.0f;
            rot += deltaTime * 2.0f;

            for (int i = 0; i < CUBES_COUNT; ++i)
            {
                gCubesInfo[i].mLocalMat = mat4::translation(vec3(25.0f - i * 15.f, 0, -30 + (i % 3) * 30.f)) * mat4::rotationY(rot) *
                                          mat4::scale(vec3(25.0f)) * mat4::identity();
                gUniformData.mToWorldMat[i] = gCubesInfo[i].mLocalMat;
            }

            gMatrices.mPrevProjectView = gMatrices.mProjectView;
            gMatrices.mProjectView = gUniformData.mProjectView;
            for (int i = 0; i < CUBES_COUNT; ++i)
            {
                gMatrices.mPrevToWorldMat[i] = gMatrices.mToWorldMat[i];
                gMatrices.mToWorldMat[i] = gCubesInfo[i].mLocalMat;
            }
        }

        bToggleDebugView &= bToggleVRS;
    }

    void Draw()
    {
        if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
        {
            waitQueueIdle(pGraphicsQueue);
            ::toggleVSync(pRenderer, &pSwapChain);
        }

        uint32_t swapchainImageIndex;
        acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

        RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);
        FenceStatus       fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        // Update uniform buffers
        BufferUpdateDesc uniformBuffer = { pUniformBuffer[gFrameIndex] };
        beginUpdateResource(&uniformBuffer);
        *(UniformBlock*)uniformBuffer.pMappedData = gUniformData;
        endUpdateResource(&uniformBuffer);

        // Update uniform buffers
        uniformBuffer = { pMatricesBuffer[gFrameIndex] };
        beginUpdateResource(&uniformBuffer);
        *(Matrices*)uniformBuffer.pMappedData = gMatrices;
        endUpdateResource(&uniformBuffer);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
            { pVelocityRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
            { pHistoryRenderTarget[gFrameIndex], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 3, barriers);
        {
            // Note:
            // setSamplePositions in Metal is incosistent with other APIs: it cannot be called inside the renderpass, so has to be set
            // upfront without calling setSampleLocations upfront in D3D12 validation error will be issued:
            //     "Sample positions set on command list do not match sample positions last used to clear, fill, transition or resolve the
            //     depth stencil resource."
            // it is harmless for other APIs
            cmdSetSampleLocations(cmd, SAMPLE_COUNT_4, 1, 1, gLocations);
            {
                // Depth and velocity prepass
                {
                    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Depth and velocity prepass");
                    LoadActionsDesc loadActions = {};
                    loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                    loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
                    loadActions.mLoadActionStencil = LOAD_ACTION_CLEAR;
                    loadActions.mClearDepth.depth = 1.0f;
                    loadActions.mClearDepth.stencil = 0;

                    RenderTarget* renderTargets[3] = {};
                    uint32_t      rtCount = 1;
                    renderTargets[0] = pVelocityRenderTarget;
                    Pipeline* pFillVelocityPipeline = pVelocityPipeline;

                    if (gClearHistoryTextures)
                    {
                        barriers[0] = { pHistoryRenderTarget[1 - gFrameIndex], RESOURCE_STATE_SHADER_RESOURCE,
                                        RESOURCE_STATE_RENDER_TARGET };
                        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
                        pFillVelocityPipeline = pVelocityPipelineClear;
                        for (uint32_t i = 0; i < 2; ++i)
                        {
                            loadActions.mLoadActionsColor[1 + i] = LOAD_ACTION_CLEAR;
                            loadActions.mClearColorValues[1 + i] = { { 0xff, 0, 0, 0 } };
                            renderTargets[1 + i] = pHistoryRenderTarget[i];
                        }
                        rtCount += 2;
                    }

                    cmdBindRenderTargets(cmd, rtCount, renderTargets, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
                    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight, 0.0f, 1.0f);
                    cmdSetScissor(cmd, 0, 0, pDepthBuffer->mWidth, pDepthBuffer->mHeight);
                    cmdSetSampleLocations(cmd, SAMPLE_COUNT_4, 1, 1, gLocations);
                    if (bDrawCubes)
                    {
                        cmdBindPipeline(cmd, pFillVelocityPipeline);
                        cmdBindPushConstants(cmd, pRootSignatureVelocity, gVelocityScreenSettingsConstantIndex, &gBGData.size);
                        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetMatrices);
                        static uint32_t stride = sizeof(float) * 8;
                        cmdBindVertexBuffer(cmd, 1, &pCuboidVertexBuffer, &stride, NULL);

                        cmdDrawInstanced(cmd, gCuboidVertexCount, 0, CUBES_COUNT, 0);
                    }
                    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                }
                cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                barriers[0] = { pVelocityRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
                uint32_t bCount = 1;
                if (gClearHistoryTextures)
                {
                    barriers[bCount++] = { pHistoryRenderTarget[1 - gFrameIndex], RESOURCE_STATE_RENDER_TARGET,
                                           RESOURCE_STATE_SHADER_RESOURCE };
                    gClearHistoryTextures = false;
                }
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, bCount, barriers);
                // Generate the shading rate image
                {
                    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Generate the shading rate image");
                    LoadActionsDesc loadActions = {};
                    loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
                    loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
                    loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
                    cmdBindRenderTargets(cmd, 1, &pHistoryRenderTarget[gFrameIndex], pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
                    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pDepthBuffer->mWidth, (float)pDepthBuffer->mHeight, 0.0f, 1.0f);
                    cmdSetScissor(cmd, 0, 0, pDepthBuffer->mWidth, pDepthBuffer->mHeight);
                    cmdSetSampleLocations(cmd, SAMPLE_COUNT_4, 1, 1, gLocations);

                    if (bToggleVRS)
                    {
                        cmdBindPipeline(cmd, pFillStencilPipeline);
                        cmdBindDescriptorSet(cmd, 1 - gFrameIndex, pFillStencilDescriptorSet);
                        cmdBindPushConstants(cmd, pRootSignatureFillStencil, gScreenSettingsConstantIndex, &gBGData.size);

                        cmdSetStencilReferenceValue(cmd, 1);
                        cmdDraw(cmd, 3, 0);
                    }
                    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                }
                cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                // Draw
                {
                    barriers[0] = { pColorRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
                    cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

                    LoadActionsDesc loadActions = {};
                    loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                    loadActions.mClearColorValues[0] = pColorRenderTarget->mClearValue;
                    loadActions.mLoadActionDepth = LOAD_ACTION_LOAD;
                    loadActions.mLoadActionStencil = LOAD_ACTION_LOAD;
                    cmdBindRenderTargets(cmd, 1, &pColorRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
                    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pColorRenderTarget->mWidth, (float)pColorRenderTarget->mHeight, 0.0f, 1.0f);
                    cmdSetScissor(cmd, 0, 0, pColorRenderTarget->mWidth, pColorRenderTarget->mHeight);
                    cmdSetSampleLocations(cmd, SAMPLE_COUNT_4, 1, 1, gLocations);

                    if (bDrawCubes)
                    {
                        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw the cubes");
                        cmdBindPipeline(cmd, pCubePipeline);
                        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTextures);
                        cmdBindDescriptorSet(cmd, gFrameIndex, pDescriptorSetCubes);

                        cmdSetStencilReferenceValue(cmd, bToggleVRS ? 1 : 0);
                        static uint32_t stride = sizeof(float) * 8;
                        cmdBindVertexBuffer(cmd, 1, &pCuboidVertexBuffer, &stride, NULL);

                        cmdDrawInstanced(cmd, gCuboidVertexCount, 0, CUBES_COUNT, 0);

                        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                    }

                    // draw the background
                    {
                        cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw the background");
                        cmdBindPipeline(cmd, pPlanePipeline);
                        cmdBindDescriptorSet(cmd, 0, pDescriptorSetTextures);
                        cmdBindPushConstants(cmd, pRootSignature, gKernelSizeConstantIndex, &gBGData);
                        cmdSetStencilReferenceValue(cmd, bToggleVRS ? 1 : 0);
                        cmdDraw(cmd, 3, 0);
                        cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                    }
                    cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                }
                {
                    barriers[0] = { pHistoryRenderTarget[gFrameIndex], RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
                    barriers[1] = { pColorRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
                    float toggleVRS = bToggleVRS ? 1.0f : 0.0f;

                    TextureBarrier uav = { pResolvedTexture[gFrameIndex], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS };

                    cmdResourceBarrier(cmd, 0, NULL, 1, &uav, 2, barriers);

                    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Resolve");
                    cmdBindPipeline(cmd, pResolveComputePipeline);
                    cmdBindPushConstants(cmd, pRootSignatureResolveCompute, gShadingRateRootConstantIndex, &toggleVRS);

                    cmdBindDescriptorSet(cmd, gFrameIndex, pResolveComputeDescriptorSet);
                    const uint32_t* pThreadGroupSize = pResolveComputeShader->pReflection->mStageReflections[0].mNumThreadsPerGroup;
                    cmdDispatch(cmd, mSettings.mWidth / (gDivider * pThreadGroupSize[0]) + 1,
                                mSettings.mHeight / (gDivider * pThreadGroupSize[1]) + 1, pThreadGroupSize[2]);
                    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                }

                // Draw Screen
                {
                    float          toggleDebugView = (bToggleDebugView && bToggleVRS) ? 1.0f : 0.0f;
                    TextureBarrier uav = { pResolvedTexture[gFrameIndex], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE };
                    cmdResourceBarrier(cmd, 0, NULL, 1, &uav, 0, NULL);
                    cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw screen");

                    LoadActionsDesc loadActions = {};
                    loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                    cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
                    cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
                    cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);

                    cmdBindPipeline(cmd, pResolvePipeline);
                    cmdBindDescriptorSet(cmd, gFrameIndex, pResolveDescriptorSet);
                    cmdBindPushConstants(cmd, pRootSignatureComposite, gDebugViewRootConstantIndex, &toggleDebugView);

                    cmdDraw(cmd, 3, 0);
                    cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
                    cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
                }
            }
        }
        // UI and Other Overlays
        {
            LoadActionsDesc loadActions = {};
            loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
            cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
            cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
            cmdBindRenderTargets(cmd, 1, &pRenderTarget, NULL, &loadActions, NULL, NULL, -1, -1);
            cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

            const float txtIndent = 8.f;

            gFrameTimeDraw.mFontColor = bToggleDebugView ? 0xff000000 : 0xff00ffff;
            gFrameTimeDraw.mFontSize = 18.0f;
            gFrameTimeDraw.mFontID = gFontID;
            float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(txtIndent, 15.f), &gFrameTimeDraw);
            cmdDrawGpuProfile(cmd, float2(txtIndent, txtSizePx.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

            cmdDrawUserInterface(cmd);

            cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
            cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);
            barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
            cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
            cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
        }
        endCmd(cmd);

        // Sumbit and Present
        FlushResourceUpdateDesc flushUpdateDesc = {};
        flushUpdateDesc.mNodeIndex = 0;
        flushResourceUpdates(&flushUpdateDesc);
        Semaphore* waitSemaphores[2] = { flushUpdateDesc.pOutSubmittedSemaphore, pImageAcquiredSemaphore };

        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.mWaitSemaphoreCount = TF_ARRAY_COUNT(waitSemaphores);
        submitDesc.ppCmds = &cmd;
        submitDesc.ppSignalSemaphores = &elem.pSemaphore;
        submitDesc.ppWaitSemaphores = waitSemaphores;
        submitDesc.pSignalFence = elem.pFence;
        queueSubmit(pGraphicsQueue, &submitDesc);

        QueuePresentDesc presentDesc = {};
        presentDesc.mIndex = swapchainImageIndex;
        presentDesc.mWaitSemaphoreCount = 1;
        presentDesc.pSwapChain = pSwapChain;
        presentDesc.ppWaitSemaphores = &elem.pSemaphore;
        presentDesc.mSubmitDone = true;

#ifndef AUTOMATED_TESTING
        // Captures a screenshot if takeScreenshot() was called, otherwise this is a no-op
        captureScreenshot(pSwapChain, swapchainImageIndex, true, false);
#endif

        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;
    }

    const char* GetName() { return "35_VariableRateShading"; }

    bool addColorRenderTarget()
    {
        RenderTargetDesc desc = {};
        desc.mWidth = mSettings.mWidth / gDivider;
        desc.mHeight = mSettings.mHeight / gDivider;
        desc.mDepth = 1;
        desc.mArraySize = 1;
        desc.mMipLevels = 1;
        desc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        desc.mSampleCount = SAMPLE_COUNT_4;
        desc.mFormat = pSwapChain->mFormat;
        desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        desc.mClearValue.r = 0.0f;
        desc.mClearValue.g = 0.0f;
        desc.mClearValue.b = 0.0f;
        desc.mClearValue.a = 0.0f;
        desc.mSampleQuality = 0;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        desc.pName = "Color RT";
        addRenderTarget(pRenderer, &desc, &pColorRenderTarget);

        return NULL != pColorRenderTarget;
    }

    bool addHistoryRenderTarget()
    {
        RenderTargetDesc desc = {};
        desc.mWidth = mSettings.mWidth / gDivider;
        desc.mHeight = mSettings.mHeight / gDivider;
        desc.mDepth = 1;
        desc.mArraySize = 1;
        desc.mMipLevels = 1;
        desc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        desc.mSampleCount = SAMPLE_COUNT_4;
        desc.mFormat = TinyImageFormat_R8_UINT;
        desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        desc.mClearValue.r = 0.0f;
        desc.mSampleQuality = 0;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        desc.pName = "Color History RT";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            addRenderTarget(pRenderer, &desc, &pHistoryRenderTarget[i]);
        }

        gClearHistoryTextures = true;

        return NULL != pHistoryRenderTarget[0];
    }
    bool addVelocityRenderTarget()
    {
        RenderTargetDesc desc = {};
        desc.mWidth = mSettings.mWidth / gDivider;
        desc.mHeight = mSettings.mHeight / gDivider;
        desc.mDepth = 1;
        desc.mArraySize = 1;
        desc.mMipLevels = 1;
        desc.mFlags = TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        desc.mSampleCount = SAMPLE_COUNT_4;
        desc.mFormat = TinyImageFormat_R16G16_SFLOAT;
        desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        desc.mClearValue.r = 0.0f;
        desc.mClearValue.g = 0.0f;
        desc.mSampleQuality = 0;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        desc.pName = "Velocity RT";
        addRenderTarget(pRenderer, &desc, &pVelocityRenderTarget);
        return NULL != pVelocityRenderTarget;
    }

    bool addResolvedTexture()
    {
        TextureDesc texture = {};
        texture.mArraySize = 1;
        texture.mMipLevels = 1;
        texture.mDepth = 1;
        texture.mDescriptors = DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE;
        texture.mWidth = mSettings.mWidth;
        texture.mHeight = mSettings.mHeight;
        texture.mSampleCount = SAMPLE_COUNT_1;
        texture.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
        texture.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        texture.pName = "Resolved Texture";

        TextureLoadDesc textureDesc = {};
        textureDesc.pDesc = &texture;

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            textureDesc.ppTexture = &pResolvedTexture[i];
            addResource(&textureDesc, NULL);
        }

        return NULL != pResolvedTexture[0];
    }

    bool addSwapChain()
    {
        SwapChainDesc swapChainDesc = {};
        swapChainDesc.mWindowHandle = pWindow->handle;
        swapChainDesc.mPresentQueueCount = 1;
        swapChainDesc.ppPresentQueues = &pGraphicsQueue;
        swapChainDesc.mWidth = mSettings.mWidth;
        swapChainDesc.mHeight = mSettings.mHeight;
        swapChainDesc.mImageCount = getRecommendedSwapchainImageCount(pRenderer, &pWindow->handle);
        swapChainDesc.mColorFormat = getSupportedSwapchainFormat(pRenderer, &swapChainDesc, COLOR_SPACE_SDR_SRGB);
        swapChainDesc.mColorSpace = COLOR_SPACE_SDR_SRGB;
        swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }

    void addDescriptorSets()
    {
        DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetTextures);

        desc = { pRootSignatureComposite, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &desc, &pResolveDescriptorSet);

        desc = { pRootSignatureFillStencil, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &desc, &pFillStencilDescriptorSet);

        desc = { pRootSignatureResolveCompute, DESCRIPTOR_UPDATE_FREQ_NONE, gDataBufferCount };
        addDescriptorSet(pRenderer, &desc, &pResolveComputeDescriptorSet);

        desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetCubes);

        desc = { pRootSignatureVelocity, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gDataBufferCount * 2 };
        addDescriptorSet(pRenderer, &desc, &pDescriptorSetMatrices);
    }

    void removeDescriptorSets()
    {
        removeDescriptorSet(pRenderer, pDescriptorSetTextures);
        removeDescriptorSet(pRenderer, pDescriptorSetCubes);
        removeDescriptorSet(pRenderer, pDescriptorSetMatrices);
        removeDescriptorSet(pRenderer, pResolveDescriptorSet);
        removeDescriptorSet(pRenderer, pFillStencilDescriptorSet);
        removeDescriptorSet(pRenderer, pResolveComputeDescriptorSet);
    }

    void addRootSignatures()
    {
        Shader* shaders[] = { pPlaneShader, pCubeShader };

        RootSignatureDesc rootDesc = {};
        rootDesc.mStaticSamplerCount = 1;
        rootDesc.ppStaticSamplerNames = &pStaticSamplerName;
        rootDesc.ppStaticSamplers = &pStaticSampler;
        rootDesc.mShaderCount = 2;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &pRootSignature);
        gKernelSizeConstantIndex = getDescriptorIndexFromName(pRootSignature, "cbRootConstant");

        rootDesc = {};
        rootDesc.mStaticSamplerCount = 0;
        rootDesc.ppStaticSamplerNames = NULL;
        rootDesc.ppStaticSamplers = NULL;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pFillStencilShader;
        addRootSignature(pRenderer, &rootDesc, &pRootSignatureFillStencil);
        gScreenSettingsConstantIndex = getDescriptorIndexFromName(pRootSignatureFillStencil, "cbRootConstant");

        rootDesc = {};
        rootDesc.mStaticSamplerCount = 0;
        rootDesc.ppStaticSamplerNames = NULL;
        rootDesc.ppStaticSamplers = NULL;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pVelocityShader;
        addRootSignature(pRenderer, &rootDesc, &pRootSignatureVelocity);
        gVelocityScreenSettingsConstantIndex = getDescriptorIndexFromName(pRootSignatureVelocity, "cbRootConstant");

        rootDesc = {};
        rootDesc.mStaticSamplerCount = 0;
        rootDesc.ppStaticSamplerNames = NULL;
        rootDesc.ppStaticSamplers = NULL;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pResolveComputeShader;
        addRootSignature(pRenderer, &rootDesc, &pRootSignatureResolveCompute);
        gShadingRateRootConstantIndex = getDescriptorIndexFromName(pRootSignatureResolveCompute, "cbRootConstant");

        rootDesc = {};
        rootDesc.mStaticSamplerCount = 0;
        rootDesc.ppStaticSamplerNames = NULL;
        rootDesc.ppStaticSamplers = NULL;
        rootDesc.mShaderCount = 1;
        rootDesc.ppShaders = &pResolveShader;
        addRootSignature(pRenderer, &rootDesc, &pRootSignatureComposite);
        gDebugViewRootConstantIndex = getDescriptorIndexFromName(pRootSignatureComposite, "cbRootConstant");
    }

    void removeRootSignatures()
    {
        removeRootSignature(pRenderer, pRootSignature);
        removeRootSignature(pRenderer, pRootSignatureFillStencil);
        removeRootSignature(pRenderer, pRootSignatureVelocity);
        removeRootSignature(pRenderer, pRootSignatureResolveCompute);
        removeRootSignature(pRenderer, pRootSignatureComposite);
    }

    void addShaders()
    {
        ShaderLoadDesc shader = {};

        shader.mStages[0].pFileName = "basic.vert";
        shader.mStages[1].pFileName = "basic.frag";
        addShader(pRenderer, &shader, &pPlaneShader);

        shader.mStages[0].pFileName = "resolve.vert";
        shader.mStages[1].pFileName = "resolve.frag";
        addShader(pRenderer, &shader, &pResolveShader);

        shader.mStages[0].pFileName = "cube.vert";
        shader.mStages[1].pFileName = "cube.frag";
        addShader(pRenderer, &shader, &pCubeShader);

        shader.mStages[0].pFileName = "fillStencil.vert";
        shader.mStages[1].pFileName = "fillStencil.frag";
        addShader(pRenderer, &shader, &pFillStencilShader);

        shader.mStages[0].pFileName = "fillVelocity.vert";
        shader.mStages[1].pFileName = "fillVelocity.frag";
        addShader(pRenderer, &shader, &pVelocityShader);

        shader = {};
        shader.mStages[0].pFileName = "resolve.comp";
        addShader(pRenderer, &shader, &pResolveComputeShader);
    }

    void removeShaders()
    {
        removeShader(pRenderer, pResolveShader);
        removeShader(pRenderer, pPlaneShader);
        removeShader(pRenderer, pCubeShader);
        removeShader(pRenderer, pFillStencilShader);
        removeShader(pRenderer, pVelocityShader);
        removeShader(pRenderer, pResolveComputeShader);
    }

    void addPipelines()
    {
        PipelineDesc pipelineComputeDesc = {};
        pipelineComputeDesc.pName = "Resolve Pipeline";
        pipelineComputeDesc.mType = PIPELINE_TYPE_COMPUTE;

        ComputePipelineDesc& computePipelineDesc = pipelineComputeDesc.mComputeDesc;
        computePipelineDesc.pRootSignature = pRootSignatureResolveCompute;
        computePipelineDesc.pShaderProgram = pResolveComputeShader;
        addPipeline(pRenderer, &pipelineComputeDesc, &pResolveComputePipeline);

        // Color palette
        PipelineDesc pipelineDesc = {};
        pipelineDesc.pName = "Color Palette Pipeline";
        pipelineDesc.mType = PIPELINE_TYPE_GRAPHICS;

        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        GraphicsPipelineDesc& graphicsPipelineDesc = pipelineDesc.mGraphicsDesc;
        graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        graphicsPipelineDesc.mRenderTargetCount = 1;
        graphicsPipelineDesc.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
        graphicsPipelineDesc.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
        graphicsPipelineDesc.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
        graphicsPipelineDesc.pVertexLayout = NULL;
        graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;
        graphicsPipelineDesc.mUseCustomSampleLocations = true;

        graphicsPipelineDesc.pRootSignature = pRootSignatureComposite;

        graphicsPipelineDesc.pShaderProgram = pResolveShader;
        addPipeline(pRenderer, &pipelineDesc, &pResolvePipeline);

        DepthStateDesc depthStateDesc = {};
        depthStateDesc.mDepthTest = true;
        depthStateDesc.mDepthWrite = false;
        depthStateDesc.mDepthFunc = CMP_LEQUAL;
        depthStateDesc.mStencilWriteMask = 0x00;
        depthStateDesc.mStencilReadMask = 0xFF;
        depthStateDesc.mStencilTest = true;
        depthStateDesc.mStencilFrontFunc = CMP_EQUAL;
        depthStateDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateDesc.mStencilFrontPass = STENCIL_OP_KEEP;
        depthStateDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateDesc.mStencilBackFunc = CMP_EQUAL;
        depthStateDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateDesc.mStencilBackPass = STENCIL_OP_KEEP;
        depthStateDesc.mDepthBackFail = STENCIL_OP_KEEP;

        graphicsPipelineDesc.pRootSignature = pRootSignature;
        graphicsPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
        graphicsPipelineDesc.pDepthState = &depthStateDesc;
        graphicsPipelineDesc.pShaderProgram = pPlaneShader;
        graphicsPipelineDesc.mSampleCount = pColorRenderTarget->mSampleCount;
        addPipeline(pRenderer, &pipelineDesc, &pPlanePipeline);

        VertexLayout vertexLayout = {};
        vertexLayout.mBindingCount = 1;
        vertexLayout.mAttribCount = 3;
        vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[0].mBinding = 0;
        vertexLayout.mAttribs[0].mLocation = 0;
        vertexLayout.mAttribs[0].mOffset = 0;
        vertexLayout.mAttribs[1].mSemantic = SEMANTIC_NORMAL;
        vertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        vertexLayout.mAttribs[1].mBinding = 0;
        vertexLayout.mAttribs[1].mLocation = 1;
        vertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);
        vertexLayout.mAttribs[2].mSemantic = SEMANTIC_TEXCOORD0;
        vertexLayout.mAttribs[2].mFormat = TinyImageFormat_R32G32_SFLOAT;
        vertexLayout.mAttribs[2].mBinding = 0;
        vertexLayout.mAttribs[2].mLocation = 2;
        vertexLayout.mAttribs[2].mOffset = 6 * sizeof(float);

        graphicsPipelineDesc.pShaderProgram = pCubeShader;
        graphicsPipelineDesc.pVertexLayout = &vertexLayout;
        addPipeline(pRenderer, &pipelineDesc, &pCubePipeline);

        DepthStateDesc depthStateRWStencilDesc = {};
        depthStateRWStencilDesc.mDepthTest = false;
        depthStateRWStencilDesc.mDepthWrite = false;
        depthStateRWStencilDesc.mStencilWriteMask = 0xFF;
        depthStateRWStencilDesc.mStencilReadMask = 0xFF;
        depthStateRWStencilDesc.mStencilTest = true;
        depthStateRWStencilDesc.mStencilFrontFunc = CMP_ALWAYS;
        depthStateRWStencilDesc.mStencilFrontFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilFrontPass = STENCIL_OP_REPLACE;
        depthStateRWStencilDesc.mDepthFrontFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilBackFunc = CMP_ALWAYS;
        depthStateRWStencilDesc.mStencilBackFail = STENCIL_OP_KEEP;
        depthStateRWStencilDesc.mStencilBackPass = STENCIL_OP_REPLACE;
        depthStateRWStencilDesc.mDepthBackFail = STENCIL_OP_KEEP;

        graphicsPipelineDesc = {};
        graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        graphicsPipelineDesc.mRenderTargetCount = 1;
        graphicsPipelineDesc.pColorFormats = &pHistoryRenderTarget[0]->mFormat;
        graphicsPipelineDesc.mSampleCount = pDepthBuffer->mSampleCount;
        graphicsPipelineDesc.mSampleQuality = pDepthBuffer->mSampleQuality;
        graphicsPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
        graphicsPipelineDesc.pVertexLayout = NULL;
        graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;
        graphicsPipelineDesc.pBlendState = NULL;
        graphicsPipelineDesc.pRootSignature = pRootSignatureFillStencil;
        graphicsPipelineDesc.pDepthState = &depthStateRWStencilDesc;
        graphicsPipelineDesc.pShaderProgram = pFillStencilShader;
        graphicsPipelineDesc.mUseCustomSampleLocations = true;
        addPipeline(pRenderer, &pipelineDesc, &pFillStencilPipeline);

        DepthStateDesc depthStateRWDepthDesc = {};
        depthStateRWDepthDesc.mDepthTest = true;
        depthStateRWDepthDesc.mDepthWrite = true;
        depthStateRWDepthDesc.mDepthFunc = CMP_LEQUAL;

        graphicsPipelineDesc = {};
        graphicsPipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        graphicsPipelineDesc.mRenderTargetCount = 1;
        graphicsPipelineDesc.pColorFormats = &pVelocityRenderTarget->mFormat;
        graphicsPipelineDesc.mSampleCount = pDepthBuffer->mSampleCount;
        graphicsPipelineDesc.mSampleQuality = pDepthBuffer->mSampleQuality;
        graphicsPipelineDesc.mDepthStencilFormat = pDepthBuffer->mFormat;
        graphicsPipelineDesc.pVertexLayout = &vertexLayout;
        graphicsPipelineDesc.pRasterizerState = &rasterizerStateDesc;
        graphicsPipelineDesc.pBlendState = NULL;
        graphicsPipelineDesc.pRootSignature = pRootSignatureVelocity;
        graphicsPipelineDesc.pDepthState = &depthStateRWDepthDesc;
        graphicsPipelineDesc.pShaderProgram = pVelocityShader;
        graphicsPipelineDesc.mUseCustomSampleLocations = true;
        addPipeline(pRenderer, &pipelineDesc, &pVelocityPipeline);

        TinyImageFormat formats[3] = {};
        formats[0] = pVelocityRenderTarget->mFormat;
        formats[1] = pHistoryRenderTarget[0]->mFormat;
        formats[2] = pHistoryRenderTarget[1]->mFormat;

        graphicsPipelineDesc.mRenderTargetCount = 3;
        graphicsPipelineDesc.pColorFormats = formats;
        addPipeline(pRenderer, &pipelineDesc, &pVelocityPipelineClear);
    }

    void removePipelines()
    {
        removePipeline(pRenderer, pResolvePipeline);
        removePipeline(pRenderer, pPlanePipeline);
        removePipeline(pRenderer, pCubePipeline);
        removePipeline(pRenderer, pFillStencilPipeline);
        removePipeline(pRenderer, pVelocityPipeline);
        removePipeline(pRenderer, pVelocityPipelineClear);
        removePipeline(pRenderer, pResolveComputePipeline);
    }

    void prepareDescriptorSets()
    {
        DescriptorData params[3] = {};

        // Prepare descriptor sets
        params[0].pName = "historyTex";
        params[1].pName = "msaaSource";
        params[1].ppTextures = &pColorRenderTarget->pTexture;
        params[2].pName = "resolvedTex";

        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].ppTextures = &pHistoryRenderTarget[i]->pTexture;
            params[2].ppTextures = &pResolvedTexture[i];
            updateDescriptorSet(pRenderer, i, pResolveComputeDescriptorSet, 3, params);
        }

        params[0].pName = "uTexture";
        params[0].ppTextures = &pPaletteTexture;
        params[1].pName = "uTexture1";
        params[1].ppTextures = &pTestTexture;
        updateDescriptorSet(pRenderer, 0, pDescriptorSetTextures, 2, params);

        params[0].pName = "uniformBlock";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].ppBuffers = &pUniformBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetCubes, 1, params);
            params[0].ppBuffers = &pMatricesBuffer[i];
            updateDescriptorSet(pRenderer, i, pDescriptorSetMatrices, 1, params);
        }

        params[0].pName = "uTexture";
        params[1].pName = "uTextureDebug";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].ppTextures = &pResolvedTexture[i];
            params[1].ppTextures = &pHistoryRenderTarget[i]->pTexture;
            updateDescriptorSet(pRenderer, i, pResolveDescriptorSet, 2, params);
        }

        params[0].pName = "prevFrameTex";
        params[1].pName = "velocityTex";
        params[1].ppTextures = &pVelocityRenderTarget->pTexture;
        params[2].pName = "prevHistoryTex";
        for (uint32_t i = 0; i < gDataBufferCount; ++i)
        {
            params[0].ppTextures = &pResolvedTexture[1 - i];
            params[2].ppTextures = &pHistoryRenderTarget[i]->pTexture;
            updateDescriptorSet(pRenderer, i, pFillStencilDescriptorSet, 3, params);
        }
    }

    bool addDepthBuffer()
    {
        // Add depth buffer
        RenderTargetDesc depthRT = {};
        depthRT.pName = "DepthBuffer";
        depthRT.mArraySize = 1;
        depthRT.mClearValue.depth = 1.0f;
        depthRT.mClearValue.stencil = 0;
        depthRT.mDepth = 1;
        depthRT.mFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
        depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
        depthRT.mHeight = mSettings.mHeight / gDivider;
        depthRT.mSampleCount = SAMPLE_COUNT_4;
        depthRT.mSampleQuality = 0;
        depthRT.mWidth = mSettings.mWidth / gDivider;
        addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

        return pDepthBuffer != NULL;
    }

    void addCube(float width, float height, float depth)
    {
        static float cubePoints[] = {
            // Position				     //Normals			    //TexCoords
            -width, -height, -depth, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f, width,  -height, -depth, 0.0f, 0.0f,  -1.0f, 1.0f, 0.0f,
            width,  height,  -depth, 0.0f, 0.0f,  -1.0f, 1.0f, 1.0f, width,  height,  -depth, 0.0f, 0.0f,  -1.0f, 1.0f, 1.0f,
            -width, height,  -depth, 0.0f, 0.0f,  -1.0f, 0.0f, 1.0f, -width, -height, -depth, 0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,

            -width, -height, depth,  0.0f, 0.0f,  1.0f,  0.0f, 0.0f, width,  -height, depth,  0.0f, 0.0f,  1.0f,  1.0f, 0.0f,
            width,  height,  depth,  0.0f, 0.0f,  1.0f,  1.0f, 1.0f, width,  height,  depth,  0.0f, 0.0f,  1.0f,  1.0f, 1.0f,
            -width, height,  depth,  0.0f, 0.0f,  1.0f,  0.0f, 1.0f, -width, -height, depth,  0.0f, 0.0f,  1.0f,  0.0f, 0.0f,

            -width, height,  depth,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f, -width, height,  -depth, 1.0f, 0.0f,  0.0f,  1.0f, 1.0f,
            -width, -height, -depth, 1.0f, 0.0f,  0.0f,  0.0f, 1.0f, -width, -height, -depth, 1.0f, 0.0f,  0.0f,  0.0f, 1.0f,
            -width, -height, depth,  1.0f, 0.0f,  0.0f,  0.0f, 0.0f, -width, height,  depth,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f,

            width,  height,  depth,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f, width,  height,  -depth, 1.0f, 0.0f,  0.0f,  1.0f, 1.0f,
            width,  -height, -depth, 1.0f, 0.0f,  0.0f,  0.0f, 1.0f, width,  -height, -depth, 1.0f, 0.0f,  0.0f,  0.0f, 1.0f,
            width,  -height, depth,  1.0f, 0.0f,  0.0f,  0.0f, 0.0f, width,  height,  depth,  1.0f, 0.0f,  0.0f,  1.0f, 0.0f,

            -width, -height, -depth, 0.0f, -1.0f, 0.0f,  0.0f, 1.0f, width,  -height, -depth, 0.0f, -1.0f, 0.0f,  1.0f, 1.0f,
            width,  -height, depth,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f, width,  -height, depth,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f,
            -width, -height, depth,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f, -width, -height, -depth, 0.0f, -1.0f, 0.0f,  0.0f, 1.0f,

            -width, height,  -depth, 0.0f, 1.0f,  0.0f,  0.0f, 1.0f, width,  height,  -depth, 0.0f, 1.0f,  0.0f,  1.0f, 1.0f,
            width,  height,  depth,  0.0f, 1.0f,  0.0f,  1.0f, 0.0f, width,  height,  depth,  0.0f, 1.0f,  0.0f,  1.0f, 0.0f,
            -width, height,  depth,  0.0f, 1.0f,  0.0f,  0.0f, 0.0f, -width, height,  -depth, 0.0f, 1.0f,  0.0f,  0.0f, 1.0f
        };

        BufferLoadDesc cuboidVbDesc = {};
        cuboidVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        cuboidVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        cuboidVbDesc.mDesc.mSize = uint64_t(288 * sizeof(float));
        cuboidVbDesc.pData = cubePoints;
        cuboidVbDesc.ppBuffer = &pCuboidVertexBuffer;
        addResource(&cuboidVbDesc, NULL);
    }
};

DEFINE_APPLICATION_MAIN(VariableRateShading)
