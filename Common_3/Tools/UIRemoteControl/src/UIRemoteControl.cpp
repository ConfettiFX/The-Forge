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
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"

#include "../../../../Common_3/Utilities/RingBuffer.h"

// Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

// Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

// Remote Control - client functions
extern void remoteControlConnect(const char* hostName, uint16_t port);
extern void remoteControlDisconnect();
extern bool remoteControlIsConnected();
extern void remoteControlCollectInputData(uint32_t actionId, bool buttonPress, const float2* mousePos, const float2 stick);
extern void remoteControlSendInputData();
extern void remoteControlReceiveTexture();
extern UserInterfaceDrawData* remoteControlReceiveDrawData();

Renderer* pRenderer = NULL;

Queue*     pGraphicsQueue = NULL;
GpuCmdRing gGraphicsCmdRing = {};

SwapChain* pSwapChain = NULL;
Semaphore* pImageAcquiredSemaphore = NULL;

ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

UIComponent* pGuiConnect = NULL;
UIComponent* pGuiDisconnect = NULL;

const uint32_t gDataBufferCount = 1;
uint32_t       gFontID = 0;
uint32_t       gFrameIndex = 0;

static char    gHostNameBuffer[255];
static bstring gHostName = bemptyfromarr(gHostNameBuffer);
struct RemoteRenderTarget
{
    float2 mWindowSize;

    float2        mRemoteSize;
    float2        mRemoteDisplaySize;
    RenderTarget* pRenderTarget;
    UIWidget*     pWidget;

    bool mDestroySignal = false;
};

RemoteRenderTarget gRemoteRenderTarget = {};

void reloadRequest(void*)
{
    ReloadDesc reload{ RELOAD_TYPE_SHADER };
    requestReload(&reload);
}

void buttonRemoteConnect(void*) { remoteControlConnect((const char*)gHostName.data, 8889); }

void buttonRemoteDisconnect(void*)
{
    gRemoteRenderTarget.mDestroySignal = true;
    remoteControlDisconnect();
}
static void registerRemoteInput(InputActionContext* ctx, uint32_t actionId)
{
    float2              mousePos = {};
    RemoteRenderTarget* remoteRender = ((RemoteRenderTarget*)ctx->pUserData);

    if (ctx->pPosition && remoteRender->pWidget)
    {
        mousePos = *ctx->pPosition;
        mousePos.y -= remoteRender->pWidget->mDisplayPosition.y;
        mousePos.x -= remoteRender->pWidget->mDisplayPosition.x;

        mousePos /= remoteRender->mRemoteDisplaySize;
        mousePos *= remoteRender->mRemoteSize;
    }
    remoteControlCollectInputData(actionId, ctx->mBool, ctx->pPosition ? &mousePos : NULL, ctx->mFloat2);
}

class UIRemoteControl: public IApp
{
public:
    bool Init()
    {
        // FILE PATHS
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
        fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");
        fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_DEBUG, "Debug");

        // window and renderer setup
        RendererDesc settings;
        memset(&settings, 0, sizeof(settings));
        settings.mD3D11Supported = true;
        settings.mGLESSupported = true;
        settings.mDisableShaderServer = true;
        initRenderer(GetName(), &settings, &pRenderer);
        // check for init success
        if (!pRenderer)
            return false;

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
        uiRenderDesc.mEnableRemoteUI = false;
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
        UIComponentDesc connectDisconnectDesc = {};
        connectDisconnectDesc.mStartPosition = vec2(0.f, 0.f);
        connectDisconnectDesc.mStartSize = vec2(mSettings.mWidth, mSettings.mHeight);
        uiCreateComponent(GetName(), &connectDisconnectDesc, &pGuiConnect);
        uiSetComponentFlags(pGuiConnect, GUI_COMPONENT_FLAGS_NO_MOVE | GUI_COMPONENT_FLAGS_NO_COLLAPSE);

        uiCreateComponent(GetName(), &connectDisconnectDesc, &pGuiDisconnect);
        uiSetComponentFlags(pGuiDisconnect, GUI_COMPONENT_FLAGS_NO_MOVE | GUI_COMPONENT_FLAGS_NO_COLLAPSE);

        bassignliteral(&gHostName, "localhost");
        TextboxWidget textboxWidget;
        textboxWidget.pText = &gHostName;
        textboxWidget.mFlags = 0;
        luaRegisterWidget(uiCreateComponentWidget(pGuiConnect, "Host Name", &textboxWidget, WIDGET_TYPE_TEXTBOX));

        ButtonWidget connectButtonWidget;
        UIWidget*    connectUiWidget = uiCreateComponentWidget(pGuiConnect, "Connect", &connectButtonWidget, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(connectUiWidget, nullptr, buttonRemoteConnect);
        luaRegisterWidget(connectUiWidget);

        ButtonWidget disconnectButtonWidget;
        UIWidget* disconnectUiWidget = uiCreateComponentWidget(pGuiDisconnect, "Disconnect", &disconnectButtonWidget, WIDGET_TYPE_BUTTON);
        uiSetWidgetOnEditedCallback(disconnectUiWidget, nullptr, buttonRemoteDisconnect);
        luaRegisterWidget(disconnectUiWidget);

        waitForAllResourceLoads();

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
        actionDesc = { DefaultInputActions::ROTATE_CAMERA,
                       [](InputActionContext* ctx)
                       {
                           registerRemoteInput(ctx, UISystemInputActions::UISystemInputAction::UI_ACTION_MOUSE_MOVE);
                           return true;
                       },
                       &gRemoteRenderTarget };
        addInputAction(&actionDesc);
        InputActionCallback onAnyInput = [](InputActionContext* ctx)
        {
            if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
            {
                registerRemoteInput(ctx, ctx->mActionId);
                uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
            }

            return true;
        };

        InputActionCallback onText = [](InputActionContext* ctx)
        {
            if (ctx->pText)
            {
                uiOnText(ctx->pText);
            }

            return true;
        };

        GlobalInputActionDesc globalInputActionDesc = { GlobalInputActionDesc::ANY_BUTTON_ACTION, onAnyInput, &gRemoteRenderTarget };
        setGlobalInputAction(&globalInputActionDesc);

        GlobalInputActionDesc globalTextInputActionDesc = { GlobalInputActionDesc::TEXT, onText, this };
        setGlobalInputAction(&globalTextInputActionDesc);

        gFrameIndex = 0;

        return true;
    }

    void Exit()
    {
        if (gRemoteRenderTarget.pWidget)
        {
            uiDestroyComponentWidget(pGuiDisconnect, gRemoteRenderTarget.pWidget);
        }

        if (gRemoteRenderTarget.pRenderTarget != NULL)
        {
            removeRenderTarget(pRenderer, gRemoteRenderTarget.pRenderTarget);
        }

        remoteControlDisconnect();

        exitInputSystem();

        exitUserInterface();

        exitFontSystem();

        // Exit profile
        exitProfiler();

        removeGpuCmdRing(pRenderer, &gGraphicsCmdRing);
        removeSemaphore(pRenderer, pImageAcquiredSemaphore);

        exitResourceLoaderInterface(pRenderer);

        removeQueue(pRenderer, pGraphicsQueue);

        exitRenderer(pRenderer);
        pRenderer = NULL;
    }

    bool Load(ReloadDesc* pReloadDesc)
    {
        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            if (!addSwapChain())
                return false;
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

        initScreenshotInterface(pRenderer, pGraphicsQueue);

        return true;
    }

    void Unload(ReloadDesc* pReloadDesc)
    {
        waitQueueIdle(pGraphicsQueue);

        unloadFontSystem(pReloadDesc->mType);
        unloadUserInterface(pReloadDesc->mType);

        if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
        {
            removeSwapChain(pRenderer, pSwapChain);
        }

        exitScreenshotInterface();
    }

    void Update(float deltaTime)
    {
        updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

        /************************************************************************/
        // Scene Update
        /************************************************************************/
        static float currentTime = 0.0f;
        currentTime += deltaTime * 1000.0f;

        /************************************************************************/
        // UI Update
        /************************************************************************/
        bool isRemoteControlConnected = remoteControlIsConnected();
        uiSetComponentActive(pGuiConnect, !isRemoteControlConnected);
        uiSetComponentActive(pGuiDisconnect, isRemoteControlConnected);
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

        RenderTarget*     pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
        GpuCmdRingElement elem = getNextGpuCmdRingElement(&gGraphicsCmdRing, true, 1);

        // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
        FenceStatus fenceStatus;
        getFenceStatus(pRenderer, elem.pFence, &fenceStatus);
        if (fenceStatus == FENCE_STATUS_INCOMPLETE)
            waitForFences(pRenderer, 1, &elem.pFence);

        // Reset cmd pool for this frame
        resetCmdPool(pRenderer, elem.pCmdPool);

        Cmd* cmd = elem.pCmds[0];
        beginCmd(cmd);

        RenderTargetBarrier barriers[] = {
            { pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
        };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        LoadActionsDesc loadActions = {};
        loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
        cmdBindRenderTargets(cmd, 1, &pRenderTarget, nullptr, &loadActions, NULL, NULL, -1, -1);

        cmdDrawUserInterface((Cmd*)cmd);
        UserInterfaceDrawData* drawData = remoteControlReceiveDrawData();
        if (drawData)
        {
            gRemoteRenderTarget.mRemoteSize = drawData->mDisplaySize;
            if (gRemoteRenderTarget.pRenderTarget)
            {
                barriers[0] = { gRemoteRenderTarget.pRenderTarget, RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_RENDER_TARGET };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

                LoadActionsDesc loadActions = {};
                loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
                loadActions.mClearColorValues[0] = gRemoteRenderTarget.pRenderTarget->mClearValue;
                cmdBindRenderTargets(cmd, 1, &gRemoteRenderTarget.pRenderTarget, nullptr, &loadActions, NULL, NULL, -1, -1);

                cmdDrawUserInterface((Cmd*)cmd, drawData);

                barriers[0] = { gRemoteRenderTarget.pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE };
                cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);
            }
        }
        remoteControlSendInputData();

        cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
        barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
        cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

        endCmd(cmd);

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

        queuePresent(pGraphicsQueue, &presentDesc);
        flipProfiler();

        gFrameIndex = (gFrameIndex + 1) % gDataBufferCount;

        remoteControlReceiveTexture();
        // Update remote render target
        gRemoteRenderTarget.mWindowSize = { mSettings.mWidth, mSettings.mHeight };

        if (gRemoteRenderTarget.mDestroySignal)
        {
            if (gRemoteRenderTarget.pWidget)
            {
                uiDestroyComponentWidget(pGuiDisconnect, gRemoteRenderTarget.pWidget);
                gRemoteRenderTarget.pWidget = NULL;
                return;
            }

            if (gRemoteRenderTarget.pRenderTarget)
            {
                removeRenderTarget(pRenderer, gRemoteRenderTarget.pRenderTarget);
            }

            gRemoteRenderTarget = {};
        }

        if (gRemoteRenderTarget.mRemoteSize.x > 0 && gRemoteRenderTarget.mRemoteSize.y > 0)
        {
            if (gRemoteRenderTarget.pRenderTarget == NULL ||
                (gRemoteRenderTarget.pRenderTarget->mWidth != gRemoteRenderTarget.mRemoteSize.x &&
                 gRemoteRenderTarget.pRenderTarget->mHeight != gRemoteRenderTarget.mRemoteSize.y))
            {
                if (gRemoteRenderTarget.pWidget)
                {
                    uiDestroyComponentWidget(pGuiDisconnect, gRemoteRenderTarget.pWidget);
                    gRemoteRenderTarget.pWidget = NULL;
                    return;
                }

                if (gRemoteRenderTarget.pRenderTarget)
                {
                    removeRenderTarget(pRenderer, gRemoteRenderTarget.pRenderTarget);
                }
                const ClearValue colorClearBlack = { { 0.0f, 0.0f, 0.0f, 1.0f } };

                RenderTargetDesc rtDesc = {};
                rtDesc.mArraySize = 1;
                rtDesc.mClearValue = colorClearBlack;
                rtDesc.mDepth = 1;
                rtDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
                rtDesc.mFormat = pSwapChain->mFormat;
                rtDesc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
                rtDesc.mWidth = gRemoteRenderTarget.mRemoteSize.x;
                rtDesc.mHeight = gRemoteRenderTarget.mRemoteSize.y;
                rtDesc.mSampleCount = SAMPLE_COUNT_1;
                rtDesc.mSampleQuality = 0;
                rtDesc.mFlags = TEXTURE_CREATION_FLAG_ESRAM;
                rtDesc.pName = "Remote RT";
                addRenderTarget(pRenderer, &rtDesc, &gRemoteRenderTarget.pRenderTarget);

                if (gRemoteRenderTarget.pWidget)
                {
                    uiDestroyComponentWidget(pGuiDisconnect, gRemoteRenderTarget.pWidget);
                }

                float ar = gRemoteRenderTarget.mRemoteSize.x / gRemoteRenderTarget.mRemoteSize.y;
                float height = (mSettings.mHeight - 150);
                float width = height * ar;
                gRemoteRenderTarget.mRemoteDisplaySize = { width, height };

                DebugTexturesWidget widget;
                widget.pTextures = &gRemoteRenderTarget.pRenderTarget->pTexture;
                widget.mTexturesCount = 1;
                widget.mTextureDisplaySize = gRemoteRenderTarget.mRemoteDisplaySize;
                gRemoteRenderTarget.pWidget = uiCreateComponentWidget(pGuiDisconnect, "Remote RT", &widget, WIDGET_TYPE_DEBUG_TEXTURES);
            }
        }
    }

    const char* GetName() { return "UIRemoteControl"; }

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
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
        ::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

        return pSwapChain != NULL;
    }
};

DEFINE_APPLICATION_MAIN(UIRemoteControl)