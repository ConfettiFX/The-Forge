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
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "../../Application/Config.h"

#ifdef _WINDOWS

#include <ctime>
#include <ntverp.h>

#include "../CPUConfig.h"

#if !defined(XBOX)
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"
#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../../Utilities/ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ITime.h"

#if defined(ENABLE_FORGE_REMOTE_UI)
#include "../../Tools/Network/Network.h"
#endif
#if defined(ENABLE_FORGE_RELOAD_SHADER)
#include "../../Tools/ReloadServer/ReloadClient.h"
#endif
#include "../../Utilities/Math/MathTypes.h"

#include "../../Utilities/Interfaces/IMemory.h"

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
#include "WindowsStackTraceDump.h"
#endif

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

// App Data
static IApp*       pApp = nullptr;
static WindowDesc* gWindowDesc = nullptr;
static bool        gShowPlatformUI = true;
static ResetDesc   gResetDescriptor = { RESET_TYPE_NONE };
static ReloadDesc  gReloadDescriptor = { RELOAD_TYPE_ALL };
/// CPU
static CpuInfo     gCpu;
static OSInfo      gOsInfo = {};

// UI
static UIComponent* pAPISwitchingComponent = NULL;
static UIComponent* pToggleVSyncComponent = NULL;
#if defined(ENABLE_FORGE_RELOAD_SHADER)
static UIComponent* pReloadShaderComponent = NULL;
#endif
static UIWidget* pSwitchComponentLabelWidget = NULL;
static UIWidget* pSelectApUIWidget = NULL;
static UIWidget* pSelectGraphicCardWidget = NULL;
static uint32_t  gSelectedApiIndex = 0;

// PickRenderingAPI.cpp
extern PlatformParameters gPlatformParameters;
extern bool               gD3D11Unsupported;

// WindowsWindow.cpp
extern IApp*        pWindowAppRef;
extern WindowDesc*  gWindow;
extern bool         gCursorVisible;
extern bool         gCursorInsideRectangle;
extern MonitorDesc* gMonitors;
extern uint32_t     gMonitorCount;

// WindowsLog.c
extern "C" HWND* gLogWindowHandle;

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

static inline float CounterToSecondsElapsed(int64_t start, int64_t end) { return (float)(end - start) / (float)1e6; }

ThermalStatus getThermalStatus() { return THERMAL_STATUS_NOT_SUPPORTED; }

//------------------------------------------------------------------------
// OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown() { PostQuitMessage(0); }

void requestReset(const ResetDesc* pResetDesc) { gResetDescriptor = *pResetDesc; }

void requestReload(const ReloadDesc* pReloadDesc) { gReloadDescriptor = *pReloadDesc; }

void errorMessagePopup(const char* title, const char* msg, WindowHandle* handle, errorMessagePopupCallbackFn callback)
{
    UNREF_PARAM(handle);
#if defined(AUTOMATED_TESTING)
    LOGF(eERROR, title);
    LOGF(eERROR, msg);
#else
    MessageBoxA((HWND)handle->window, msg, title, MB_OK);
#endif
    if (callback)
    {
        callback();
    }
}

CustomMessageProcessor sCustomProc = nullptr;
void                   setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

CpuInfo* getCpuInfo() { return &gCpu; }

OSInfo* getOsInfo() { return &gOsInfo; }

void getOsVersion(ULONG& majorVersion, ULONG& minorVersion, ULONG& buildNumber)
{
    void(WINAPI * pfnRtlGetNtVersionNumbers)(__out_opt ULONG * pNtMajorVersion, __out_opt ULONG * pNtMinorVersion,
                                             __out_opt ULONG * pNtBuildNumber);

    (FARPROC&)pfnRtlGetNtVersionNumbers = GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlGetNtVersionNumbers");

    if (pfnRtlGetNtVersionNumbers)
    {
        pfnRtlGetNtVersionNumbers(&majorVersion, &minorVersion, &buildNumber);
        buildNumber = buildNumber & ~0xF0000000;
    }
}
//------------------------------------------------------------------------
// PLATFORM LAYER CORE SUBSYSTEMS
//------------------------------------------------------------------------

bool initBaseSubsystems()
{
    // Not exposed in the interface files / app layer
    extern bool platformInitFontSystem();
    extern bool platformInitUserInterface();
    extern void platformInitLuaScriptingSystem();
    extern void platformInitWindowSystem(WindowDesc*);

    platformInitWindowSystem(gWindowDesc);
    pApp->pWindow = gWindowDesc;

#ifdef ENABLE_FORGE_FONTS
    if (!platformInitFontSystem())
        return false;
#endif

#ifdef ENABLE_FORGE_UI
    if (!platformInitUserInterface())
        return false;
#endif

#ifdef ENABLE_FORGE_SCRIPTING
    platformInitLuaScriptingSystem();

#if defined(ENABLE_FORGE_SCRIPTING) && defined(AUTOMATED_TESTING)
    // Tests below are executed first, before any tests registered in IApp::Init
    const char*    sFirstTestScripts[] = { "Test_Default.lua" };
    const uint32_t numScripts = sizeof(sFirstTestScripts) / sizeof(sFirstTestScripts[0]);
    LuaScriptDesc  scriptDescs[numScripts] = {};
    for (uint32_t i = 0; i < numScripts; ++i)
    {
        scriptDescs[i].pScriptFileName = sFirstTestScripts[i];
    }
    luaDefineScripts(scriptDescs, numScripts);
#endif
#endif

#if defined(ENABLE_FORGE_REMOTE_UI)
    initNetwork();
#endif

    return true;
}

void updateBaseSubsystems(float deltaTime, bool appDrawn)
{
    // Not exposed in the interface files / app layer
    extern void platformUpdateLuaScriptingSystem(bool appDrawn);
    extern void platformUpdateUserInterface(float deltaTime);
    extern void platformUpdateWindowSystem();

    platformUpdateWindowSystem();

#ifdef ENABLE_FORGE_SCRIPTING
    platformUpdateLuaScriptingSystem(appDrawn);
#endif

#ifdef ENABLE_FORGE_UI
    platformUpdateUserInterface(deltaTime);
#endif
}

void exitBaseSubsystems()
{
    // Not exposed in the interface files / app layer
    extern void platformExitFontSystem();
    extern void platformExitUserInterface();
    extern void platformExitLuaScriptingSystem();
    extern void platformExitWindowSystem();

    platformExitWindowSystem();

#ifdef ENABLE_FORGE_UI
    platformExitUserInterface();
#endif

#ifdef ENABLE_FORGE_FONTS
    platformExitFontSystem();
#endif

#ifdef ENABLE_FORGE_SCRIPTING
    platformExitLuaScriptingSystem();
#endif

#if defined(ENABLE_FORGE_REMOTE_UI)
    exitNetwork();
#endif
}

//------------------------------------------------------------------------
// PLATFORM LAYER USER INTERFACE
//------------------------------------------------------------------------

// Must be called after Graphics::initRenderer()
void setupPlatformUI(const IApp::Settings* pSettings)
{
    gSelectedApiIndex = gPlatformParameters.mSelectedRendererApi;

#ifdef ENABLE_FORGE_UI

    // WINDOW AND RESOLUTION CONTROL
    extern void platformSetupWindowSystemUI(IApp*);
    platformSetupWindowSystemUI(pApp);

    // VSYNC CONTROL
    UIComponentDesc uiDesc = {};
    uiDesc.mStartPosition = vec2(pSettings->mWidth * 0.7f, pSettings->mHeight * 0.8f);
    uiCreateComponent("VSync Control", &uiDesc, &pToggleVSyncComponent);

    CheckboxWidget checkbox;
    checkbox.pData = &pApp->mSettings.mVSyncEnabled;
    UIWidget* pCheckbox = uiCreateComponentWidget(pToggleVSyncComponent, "Toggle VSync\t\t\t\t\t", &checkbox, WIDGET_TYPE_CHECKBOX);
    REGISTER_LUA_WIDGET(pCheckbox);

    // MICROPROFILER UI
    toggleProfilerMenuUI(true);

#if defined(ENABLE_FORGE_RELOAD_SHADER)
    // RELOAD CONTROL
    uiDesc = {};
    uiDesc.mStartPosition = vec2(pSettings->mWidth * 0.7f, pSettings->mHeight * 0.9f);
    uiCreateComponent("Reload Control", &uiDesc, &pReloadShaderComponent);

    platformReloadClientAddReloadShadersButton(pReloadShaderComponent);
#endif

    // API SWITCHING
    uiDesc = {};
    uiDesc.mStartPosition = vec2(pSettings->mWidth * 0.6f, pSettings->mHeight * 0.01f);
    uiCreateComponent("API Switching", &uiDesc, &pAPISwitchingComponent);

    static const char* pApiNames[] = {
#if defined(DIRECT3D12)
        "D3D12",
#endif
#if defined(VULKAN)
        "Vulkan",
#endif
#if defined(DIRECT3D11)
        "D3D11",
#endif
    };

    // Select Api
    DropdownWidget selectApUIWidget = {};
    selectApUIWidget.pData = &gSelectedApiIndex;

    uint32_t apiCount = RENDERER_API_COUNT;
#ifdef DIRECT3D11
    if (gD3D11Unsupported)
    {
        --apiCount;
    }
#endif
    ASSERT(apiCount != 0 && "No supported Graphics API available!");
    selectApUIWidget.pNames = pApiNames;
    selectApUIWidget.mCount = apiCount;

    pSelectApUIWidget = uiCreateComponentWidget(pAPISwitchingComponent, "Select API", &selectApUIWidget, WIDGET_TYPE_DROPDOWN);
    pSelectApUIWidget->pOnEdited = [](void* pUserData)
    {
        UNREF_PARAM(pUserData);
        ResetDesc resetDescriptor{ RESET_TYPE_API_SWITCH };
        requestReset(&resetDescriptor);
    };
    REGISTER_LUA_WIDGET(pSelectApUIWidget);

    static const char* gpuNames[] = { gPlatformParameters.ppAvailableGpuNames[0], gPlatformParameters.ppAvailableGpuNames[1],
                                      gPlatformParameters.ppAvailableGpuNames[2], gPlatformParameters.ppAvailableGpuNames[3] };

    DropdownWidget selectGraphicCardUIWidget = {};
    selectGraphicCardUIWidget.pData = &gPlatformParameters.mSelectedGpuIndex;
    selectGraphicCardUIWidget.pNames = gpuNames;
    selectGraphicCardUIWidget.mCount = gPlatformParameters.mAvailableGpuCount;

    pSelectGraphicCardWidget =
        uiCreateComponentWidget(pAPISwitchingComponent, "Select Graphic Card", &selectGraphicCardUIWidget, WIDGET_TYPE_DROPDOWN);
    pSelectGraphicCardWidget->pOnEdited = [](void* pUserData)
    {
        UNREF_PARAM(pUserData);
        ResetDesc resetDescriptor{ RESET_TYPE_GRAPHIC_CARD_SWITCH };
        requestReset(&resetDescriptor);
    };
    REGISTER_LUA_WIDGET(pSelectGraphicCardWidget);

#if defined(ENABLE_FORGE_SCRIPTING) && defined(AUTOMATED_TESTING)
    // Tests below are executed last, after tests registered in IApp::Init have executed
    const char*    sLastTestScripts[] = { "Test_API_Switching.lua" };
    const uint32_t numScripts = sizeof(sLastTestScripts) / sizeof(sLastTestScripts[0]);
    LuaScriptDesc  scriptDescs[numScripts] = {};
    for (uint32_t i = 0; i < numScripts; ++i)
    {
        scriptDescs[i].pScriptFileName = sLastTestScripts[i];
    }
    luaDefineScripts(scriptDescs, numScripts);
#endif
#endif
}

void togglePlatformUI()
{
    gShowPlatformUI = pApp->mSettings.mShowPlatformUI;

#ifdef ENABLE_FORGE_UI
    extern void platformToggleWindowSystemUI(bool);
    platformToggleWindowSystemUI(gShowPlatformUI);

    uiSetComponentActive(pToggleVSyncComponent, gShowPlatformUI);
    uiSetComponentActive(pAPISwitchingComponent, gShowPlatformUI);
#if defined(ENABLE_FORGE_RELOAD_SHADER)
    uiSetComponentActive(pReloadShaderComponent, gShowPlatformUI);
#endif
#endif
}

//------------------------------------------------------------------------
// APP ENTRY POINT
//------------------------------------------------------------------------

// WindowsWindow.cpp
extern void initWindowClass();
extern void exitWindowClass();

int          IApp::argc;
const char** IApp::argv;

int WindowsMain(int argc, char** argv, IApp* app)
{
    UNREF_PARAM(argc);
    UNREF_PARAM(argv);
    if (!initMemAlloc(app->GetName()))
        return EXIT_FAILURE;

    FileSystemInitDesc fsDesc = {};
    fsDesc.pAppName = app->GetName();
    if (!initFileSystem(&fsDesc))
        return EXIT_FAILURE;

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(VULKAN) && VK_OVERRIDE_LAYER_PATH
    // We are now shipping validation layer in the repo itself to remove dependency on Vulkan SDK to be installed
    // Set VK_LAYER_PATH to executable location so it can find the layer files that our application wants to use
    SetEnvironmentVariableA("VK_LAYER_PATH", pSystemFileIO->GetResourceMount(RM_DEBUG));
#endif

#ifdef ENABLE_MTUNER
    rmemInit(0);
#endif

    initLog(app->GetName(), DEFAULT_LOG_LEVEL);

    ULONG majorVersion = 0;
    ULONG minorVersion = 0;
    ULONG buildNumber = 0;
    getOsVersion(majorVersion, minorVersion, buildNumber);
    snprintf(gOsInfo.osName, 256, "Windows PC");
    snprintf(gOsInfo.osVersion, 256, "%lu.%lu (Build: %lu)", majorVersion, minorVersion, buildNumber);
    snprintf(gOsInfo.osDeviceName, 256, "Unknown");
    LOGF(LogLevel::eINFO, "Operating System: %s. Version: %s. Device Name: %s.", gOsInfo.osName, gOsInfo.osVersion, gOsInfo.osDeviceName);

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
    if (!WindowsStackTrace::Init())
        return EXIT_FAILURE;
#endif

    pApp = app;
    pWindowAppRef = app;

    initWindowClass();

    // Used for automated testing, if enabled app will exit after DEFAULT_AUTOMATION_FRAME_COUNT (240) frames
#if defined(AUTOMATED_TESTING)
    uint32_t frameCounter = 0;
    uint32_t targetFrameCount = DEFAULT_AUTOMATION_FRAME_COUNT;
#endif

    initCpuInfo(&gCpu);

    IApp::Settings* pSettings = &pApp->mSettings;
    WindowDesc      window = {};
    gWindow = &window;     // WindowsWindow.cpp
    gWindowDesc = &window; // WindowsBase.cpp
    gLogWindowHandle =
        (HWND*)&window.handle
            .window; // WindowsLog.c, save the address to this handle to avoid having to adding includes to WindowsLog.c to use WindowDesc*.

    if (pSettings->mMonitorIndex < 0 || pSettings->mMonitorIndex >= (int)gMonitorCount)
    {
        pSettings->mMonitorIndex = 0;
    }

    if (pSettings->mWidth <= 0 || pSettings->mHeight <= 0)
    {
        RectDesc rect = {};

        getRecommendedResolution(&rect);
        pSettings->mWidth = getRectWidth(&rect);
        pSettings->mHeight = getRectHeight(&rect);
    }

    MonitorDesc* monitor = getMonitor(pSettings->mMonitorIndex);
    ASSERT(monitor != nullptr);

    gWindow->clientRect = { (int)pSettings->mWindowX + monitor->monitorRect.left, (int)pSettings->mWindowY + monitor->monitorRect.top,
                            (int)pSettings->mWidth, (int)pSettings->mHeight };

    gWindow->windowedRect = gWindow->clientRect;
    gWindow->fullScreen = pSettings->mFullScreen;
    gWindow->maximized = false;
    gWindow->noresizeFrame = !pSettings->mDragToResize;
    gWindow->borderlessWindow = pSettings->mBorderlessWindow;
    gWindow->centered = false; // pSettings->mCentered;
    gWindow->forceLowDPI = pSettings->mForceLowDPI;
    gWindow->overrideDefaultPosition = true;
    gWindow->cursorCaptured = false;

    if (!pSettings->mExternalWindow)
        openWindow(pApp->GetName(), gWindow);

    pSettings->mWidth = gWindow->fullScreen ? getRectWidth(&gWindow->fullscreenRect) : getRectWidth(&gWindow->clientRect);
    pSettings->mHeight = gWindow->fullScreen ? getRectHeight(&gWindow->fullscreenRect) : getRectHeight(&gWindow->clientRect);

    pApp->pCommandLine = GetCommandLineA();

#ifdef AUTOMATED_TESTING
    bool paramRenderingAPIFound = false;
    char benchmarkOutput[1024] = { "\0" };
    // Check if benchmarking was given through command line
    for (int i = 0; i < argc; i += 1)
    {
        if (strcmp(argv[i], "-b") == 0)
        {
            pSettings->mBenchmarking = true;
            if (i + 1 < argc && isdigit(*argv[i + 1]))
                targetFrameCount = min(max(atoi(argv[i + 1]), 32), 512);
        }
        else if (strcmp(argv[i], "--request-recompile-after") == 0)
        {
            extern uint32_t gReloadServerRequestRecompileAfter;
            if (i + 1 < argc && isdigit(*argv[i + 1]))
                gReloadServerRequestRecompileAfter = atoi(argv[i + 1]);
        }
        // Run forever, this is useful when the app will control when the automated tests are over
        else if (strcmp(argv[i], "--no-auto-exit") == 0)
        {
            targetFrameCount = UINT32_MAX;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            strcpy(benchmarkOutput, argv[i + 1]);
        }
        // Allow to set renderer API through command line so that we are able to test the same build with differnt APIs
        // On the TheForge Jenkins setup we change APIs through a lua script that changes the selector variable in the UI,
        // but for projects where we compile without our lua interface we cannot do this.
#if defined(DIRECT3D11)
        else if (strcmp(argv[i], "--d3d11") == 0)
        {
            if (paramRenderingAPIFound)
            {
                LOGF(eERROR, "Two command line parameters are requesting the rendering API, only one is allowed.");
                ASSERT(false);
                return -1;
            }
            gPlatformParameters.mSelectedRendererApi = RENDERER_API_D3D11;
            paramRenderingAPIFound = true;
        }
#endif
#if defined(DIRECT3D12)
        else if (strcmp(argv[i], "--d3d12") == 0)
        {
            if (paramRenderingAPIFound)
            {
                LOGF(eERROR, "Two command line parameters are requesting the rendering API, only one is allowed.");
                ASSERT(false);
                return -1;
            }
            gPlatformParameters.mSelectedRendererApi = RENDERER_API_D3D12;
            paramRenderingAPIFound = true;
        }
#endif
#if defined(VULKAN)
        else if (strcmp(argv[i], "--vulkan") == 0)
        {
            if (paramRenderingAPIFound)
            {
                LOGF(eERROR, "Two command line parameters are requesting the rendering API, only one is allowed.");
                ASSERT(false);
                return -1;
            }
            gPlatformParameters.mSelectedRendererApi = RENDERER_API_VULKAN;
            paramRenderingAPIFound = true;
        }
#endif
    }
#endif

    {
        if (!initBaseSubsystems())
            return EXIT_FAILURE;

        Timer t;
        initTimer(&t);
        if (!pApp->Init())
        {
            const char* pRendererReason;
            if (hasRendererInitializationError(&pRendererReason))
            {
                pApp->ShowUnsupportedMessage(pRendererReason);
            }

            if (pApp->mUnsupported)
            {
                errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "",
                                  &pApp->pWindow->handle, NULL);
                exitLog();
                return 0;
            }

            return EXIT_FAILURE;
        }

        setupPlatformUI(pSettings);
        pSettings->mInitialized = true;

        if (!pApp->Load(&gReloadDescriptor))
            return EXIT_FAILURE;

        LOGF(LogLevel::eINFO, "Application Init+Load+Reload %fms", getTimerMSec(&t, false) / 1000.0f);
    }

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
        setAggregateFrames(targetFrameCount / 2);
#endif

    bool    baseSubsystemAppDrawn = false;
    bool    quit = false;
    int64_t lastCounter = getUSec(false);
    while (!quit)
    {
        int64_t counter = getUSec(false);
        float   deltaTime = CounterToSecondsElapsed(lastCounter, counter);
        lastCounter = counter;

#ifdef FORGE_DEBUG
        // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
        if (deltaTime > 0.15f)
            deltaTime = 0.05f;
#endif

#if defined(AUTOMATED_TESTING)
        // Used to keep screenshot results consistent across CI runs
        deltaTime = AUTOMATION_FIXED_FRAME_TIME;
#endif

        bool lastMinimized = gWindow->minimized;

        extern bool handleMessages();
        quit = handleMessages() || pSettings->mQuit;

        // UPDATE BASE INTERFACES
        updateBaseSubsystems(deltaTime, baseSubsystemAppDrawn);
        baseSubsystemAppDrawn = false;

        if (gResetDescriptor.mType != RESET_TYPE_NONE)
        {
            if (gResetDescriptor.mType & RESET_TYPE_DEVICE_LOST)
            {
                errorMessagePopup(
                    "Graphics Device Lost",
                    "Connection to the graphics device has been lost.\nPlease verify the integrity of your graphics drivers.\nCheck the "
                    "logs for further details.",
                    &pApp->pWindow->handle, NULL);
            }

            if (gResetDescriptor.mType & RESET_TYPE_GRAPHIC_CARD_SWITCH)
            {
                ASSERT(gPlatformParameters.mSelectedGpuIndex < gPlatformParameters.mAvailableGpuCount);
                gPlatformParameters.mPreferedGpuId = gPlatformParameters.pAvailableGpuIds[gPlatformParameters.mSelectedGpuIndex];
            }

            gReloadDescriptor.mType = RELOAD_TYPE_ALL;
            pApp->Unload(&gReloadDescriptor);
            pApp->Exit();

            gPlatformParameters.mSelectedRendererApi = (RendererApi)gSelectedApiIndex;
            pSettings->mInitialized = false;

            closeWindow(app->pWindow);
            openWindow(app->GetName(), app->pWindow);

            exitBaseSubsystems();

            {
                if (!initBaseSubsystems())
                    return EXIT_FAILURE;

                Timer t;
                initTimer(&t);
                if (!pApp->Init())
                {
                    if (pApp->mUnsupported)
                    {
                        errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "",
                                          &pApp->pWindow->handle, NULL);
                        exitLog();
                        return 0;
                    }
                    return EXIT_FAILURE;
                }

                setupPlatformUI(pSettings);
                pSettings->mInitialized = true;

                if (!pApp->Load(&gReloadDescriptor))
                    return EXIT_FAILURE;

                LOGF(LogLevel::eINFO, "Application Reset %fms", getTimerMSec(&t, false) / 1000.0f);
            }

            gResetDescriptor.mType = RESET_TYPE_NONE;
            continue;
        }

        if (gReloadDescriptor.mType != RELOAD_TYPE_ALL)
        {
            Timer t;
            initTimer(&t);

            pApp->Unload(&gReloadDescriptor);
            if (!pApp->Load(&gReloadDescriptor))
                return EXIT_FAILURE;

            LOGF(LogLevel::eINFO, "Application Reload %fms", getTimerMSec(&t, false) / 1000.0f);
            gReloadDescriptor.mType = RELOAD_TYPE_ALL;
            continue;
        }

        // If window is minimized let other processes take over
        if (gWindow->minimized)
        {
            // Call update once after minimize so app can react.
            if (lastMinimized != gWindow->minimized)
            {
                pApp->Update(deltaTime);
            }
            threadSleep(1);
            continue;
        }

        // UPDATE APP
        pApp->Update(deltaTime);
        pApp->Draw();
        baseSubsystemAppDrawn = true;

        if (gShowPlatformUI != pApp->mSettings.mShowPlatformUI)
        {
            togglePlatformUI();
        }
#if defined(ENABLE_FORGE_RELOAD_SHADER)
        if (platformReloadClientShouldQuit())
            quit = true;
#endif

#ifdef AUTOMATED_TESTING
        extern bool gAutomatedTestingScriptsFinished;
        // wait for the automated testing if it hasn't managed to finish in time
        if (gAutomatedTestingScriptsFinished && frameCounter >= targetFrameCount)
            quit = true;
        frameCounter++;
#endif
    }

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
    {
        dumpBenchmarkData(pSettings, benchmarkOutput, pApp->GetName());
        dumpProfileData(benchmarkOutput, targetFrameCount);
    }
#endif

    gReloadDescriptor.mType = RELOAD_TYPE_ALL;
    pApp->mSettings.mQuit = true;
    pApp->Unload(&gReloadDescriptor);
    pApp->Exit();

    exitWindowClass();

#ifdef ENABLE_FORGE_STACKTRACE_DUMP
    WindowsStackTrace::Exit();
#endif

    exitLog();

    exitBaseSubsystems();

    exitFileSystem();

#ifdef ENABLE_MTUNER
    rmemUnload();
    rmemShutDown();
#endif

    exitMemAlloc();

    gWindow = NULL;
    gWindowDesc = NULL;
    gLogWindowHandle = NULL;
    return 0;
}
#endif
