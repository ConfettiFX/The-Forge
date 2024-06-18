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

#include "../../Application/Config.h"

#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrandr.h>
#include <ctime>
#include <gtk/gtk.h>
#include <sys/utsname.h>

#include "../../Utilities/ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../../Application/Interfaces/IApp.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../Graphics/Interfaces/IGraphics.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../Interfaces/IOperatingSystem.h"

#if defined(ENABLE_FORGE_REMOTE_UI)
#include "../../Tools/Network/Network.h"
#endif
#if defined(ENABLE_FORGE_RELOAD_SHADER)
#include "../../Tools/ReloadServer/ReloadClient.h"
#endif
#include "../../Utilities/Math/MathTypes.h"
#include "../CPUConfig.h"

#include "../../Utilities/Interfaces/IMemory.h"

static IApp*       pApp = NULL;
static WindowDesc* gWindowDesc = NULL;

// LinuxWindow.cpp
extern IApp* pWindowAppRef;

// LinuxWindow.cpp
extern void collectMonitorInfo();
extern void destroyMonitorInfo();
extern void restoreResolutions();
extern void collectXRandrInfo();
extern bool handleMessages(WindowDesc*);
extern void linuxUnmaximizeWindow(WindowDesc* winDesc);

// LinuxWindow.cpp
extern WindowDesc gWindow;
extern Display*   gDefaultDisplay;
extern uint32_t   gMonitorCount;

static bool gQuit;

static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool       gShowPlatformUI = true;

/// CPU
static CpuInfo gCpu;
static OSInfo  gOsInfo = {};

/// VSync Toggle
static UIComponent* pToggleVSyncWindow = NULL;
#if defined(ENABLE_FORGE_RELOAD_SHADER)
static UIComponent* pReloadShaderComponent = NULL;
#endif

//------------------------------------------------------------------------
// OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------
CpuInfo* getCpuInfo() { return &gCpu; }

OSInfo* getOsInfo() { return &gOsInfo; }

ThermalStatus getThermalStatus() { return THERMAL_STATUS_NOT_SUPPORTED; }

void requestReload(const ReloadDesc* pReloadDesc) { gReloadDescriptor = *pReloadDesc; }

void requestShutdown() { gQuit = true; }

CustomMessageProcessor sCustomProc = nullptr;
void                   setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

void errorMessagePopup(const char* title, const char* msg, WindowHandle* windowHandle, errorMessagePopupCallbackFn callback)
{
#if defined(AUTOMATED_TESTING)
    LOGF(eERROR, title);
    LOGF(eERROR, msg);
#else
    GtkDialogFlags flags = GTK_DIALOG_MODAL;
    GtkWidget*     dialog = gtk_message_dialog_new(NULL, flags, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(GTK_WIDGET(dialog));
    while (g_main_context_iteration(NULL, false))
        ;
#endif
    if (callback)
    {
        callback();
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

void setupPlatformUI(int32_t width, int32_t height)
{
#ifdef ENABLE_FORGE_UI

    // WINDOW AND RESOLUTION CONTROL

    extern void platformSetupWindowSystemUI(IApp*);
    platformSetupWindowSystemUI(pApp);

    // VSYNC CONTROL
    UIComponentDesc UIComponentDesc = {};
    UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.90f);
    uiCreateComponent("VSync Control", &UIComponentDesc, &pToggleVSyncWindow);

    CheckboxWidget checkbox;
    checkbox.pData = &pApp->mSettings.mVSyncEnabled;
    UIWidget* pCheckbox = uiCreateComponentWidget(pToggleVSyncWindow, "Toggle VSync\t\t\t\t\t", &checkbox, WIDGET_TYPE_CHECKBOX);
    REGISTER_LUA_WIDGET(pCheckbox);

#if defined(ENABLE_FORGE_RELOAD_SHADER)
    // RELOAD CONTROL
    UIComponentDesc = {};
    UIComponentDesc.mStartPosition = vec2(width * 0.6f, height * 0.90f);
    uiCreateComponent("Reload Control", &UIComponentDesc, &pReloadShaderComponent);
    platformReloadClientAddReloadShadersButton(pReloadShaderComponent);
#endif

    // MICROPROFILER UI
    toggleProfilerMenuUI(true);

#endif

#if defined(ENABLE_FORGE_SCRIPTING) && defined(AUTOMATED_TESTING)
// Tests below are executed last, after tests registered in IApp::Init have executed
#if 0 // For now we don't have any tests that require running after UT tests for this platform
	const char* sLastTestScripts[] = { "" };
	const uint32_t numScripts = sizeof(sLastTestScripts) / sizeof(sLastTestScripts[0]);
	LuaScriptDesc scriptDescs[numScripts] = {};
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

    uiSetComponentActive(pToggleVSyncWindow, gShowPlatformUI);
#if defined(ENABLE_FORGE_RELOAD_SHADER)
    uiSetComponentActive(pReloadShaderComponent, gShowPlatformUI);
#endif
#endif
}

//------------------------------------------------------------------------
// APP ENTRY POINT
//------------------------------------------------------------------------

#if defined(ENABLE_GRAPHICS_DEBUG) && defined(VULKAN) && VK_OVERRIDE_LAYER_PATH
static bool strreplace(char* s, const char* s1, const char* s2)
{
    char* p = strstr(s, s1);
    if (!p)
        return false;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    if (len1 != len2)
        memmove(p + len2, p + len1, strlen(p + len1) + 1);
    memcpy(p, s2, len2);

    return true;
}
#endif

int          IApp::argc;
const char** IApp::argv;

int LinuxMain(int argc, char** argv, IApp* app)
{
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
    const char* debugPath = pSystemFileIO->GetResourceMount(RM_DEBUG);
    VERIFY(!setenv("VK_LAYER_PATH", debugPath, true));
    // HACK: we can't simply specify LD_LIBRARY_PATH at runtime,
    // so we have to patch the .json file to use the full path for the shared library.
    // This currently works only for one library, because we use only one library.
    char jsonPath[256] = { 0 };
    snprintf(jsonPath, sizeof(jsonPath), "%s/VkLayer_khronos_validation.json", debugPath);

    FILE* file;
    VERIFY(file = fopen(jsonPath, "r+"));
    fseek(file, 0, SEEK_END);
    long  size = ftell(file) + strlen(debugPath) + 1;
    char* buffer = (char*)tf_calloc(size, 1);
    fseek(file, 0, SEEK_SET);
    fread(buffer, 1, size, file);

    const char* libNameInQuotes = "\"libVkLayer_khronos_validation.so\"";
    char        libPathInQuotes[256] = { 0 };
    snprintf(libPathInQuotes, sizeof(libPathInQuotes), "\"%s/libVkLayer_khronos_validation.so\"", debugPath);
    if (strreplace(buffer, libNameInQuotes, libPathInQuotes))
    {
        fseek(file, 0, SEEK_SET);
        fwrite(buffer, 1, size, file);
    }

    fclose(file);
    tf_free(buffer);
#endif

#if TF_USE_MTUNER
    rmemInit(0);
#endif

    initLog(app->GetName(), DEFAULT_LOG_LEVEL);

    gtk_init(&argc, &argv);

    {
        struct utsname details;
        int            ret = uname(&details);

        if (ret == 0)
        {
            snprintf(gOsInfo.osName, 256, "%s %s", details.sysname, details.release);
            snprintf(gOsInfo.osVersion, 256, "%s", details.version);
            snprintf(gOsInfo.osDeviceName, 256, "%s", details.machine);
        }
        else
        {
            snprintf(gOsInfo.osName, 256, "Linux Unknown");
            snprintf(gOsInfo.osVersion, 256, "Unknown");
            snprintf(gOsInfo.osDeviceName, 256, "Unknown");
        }

        LOGF(LogLevel::eINFO, "Operating System: %s. Version: %s. Device Name: %s.", gOsInfo.osName, gOsInfo.osVersion,
             gOsInfo.osDeviceName);
    }

    pApp = app;
    pWindowAppRef = app;

    // Used for automated testing, if enabled app will exit after DEFAULT_AUTOMATION_FRAME_COUNT (240) frames
#if defined(AUTOMATED_TESTING)
    uint32_t frameCounter = 0;
    uint32_t targetFrameCount = DEFAULT_AUTOMATION_FRAME_COUNT;
#endif

    IApp::Settings* pSettings = &pApp->mSettings;

    if (pSettings->mMonitorIndex < 0 || pSettings->mMonitorIndex >= (int)gMonitorCount)
    {
        pSettings->mMonitorIndex = 0;
    }

    gDefaultDisplay = XOpenDisplay(NULL);
    ASSERT(gDefaultDisplay);

    collectXRandrInfo();
    collectMonitorInfo();

    RectDesc rect = {};
    getRecommendedResolution(&rect);

    if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
    {
        pSettings->mWidth = getRectWidth(&rect);
        pSettings->mHeight = getRectHeight(&rect);
    }

    gWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
    gWindow.clientRect = rect;
    gWindow.fullScreen = pSettings->mFullScreen;
    gWindow.cursorCaptured = false;
    openWindow(pApp->GetName(), &gWindow);
    gWindowDesc = &gWindow;

    // NOTE: Some window manaters set the window to maximized
    // if the window size matches the screen resolution.
    // This might result in move/resize requests to be ignored.
    linuxUnmaximizeWindow(gWindowDesc);

    pSettings->mWidth = gWindow.fullScreen ? getRectWidth(&gWindow.fullscreenRect) : getRectWidth(&gWindow.windowedRect);
    pSettings->mHeight = gWindow.fullScreen ? getRectHeight(&gWindow.fullscreenRect) : getRectHeight(&gWindow.windowedRect);

    if (!initBaseSubsystems())
        return EXIT_FAILURE;

#ifdef AUTOMATED_TESTING
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
        else if (strcmp(argv[i], "--no-auto-exit") == 0)
        {
            targetFrameCount = UINT32_MAX;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            strcpy(benchmarkOutput, argv[i + 1]);
        }
    }
#endif

    if (!pApp->Init())
    {
        const char* pRendererReason;
        if (hasRendererInitializationError(&pRendererReason))
        {
            pApp->ShowUnsupportedMessage(pRendererReason);
        }

        if (pApp->mUnsupported)
        {
            errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "", &pApp->pWindow->handle,
                              NULL);
            exitLog();
            return 0;
        }

        return EXIT_FAILURE;
    }

    setupPlatformUI(pSettings->mWidth, pSettings->mHeight);
    pApp->mSettings.mInitialized = true;

    if (!pApp->Load(&gReloadDescriptor))
        return EXIT_FAILURE;

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
        setAggregateFrames(targetFrameCount / 2);
#endif

    initCpuInfo(&gCpu);

    bool    baseSubsystemAppDrawn = false;
    int64_t lastCounter = getUSec(false);
    while (!gQuit)
    {
        int64_t counter = getUSec(false);
        float   deltaTime = (float)(counter - lastCounter) / (float)1e6;
        lastCounter = counter;

        // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
        if (deltaTime > 0.15f)
            deltaTime = 0.05f;

#if defined(AUTOMATED_TESTING)
        // Used to keep screenshot results consistent across CI runs
        deltaTime = AUTOMATION_FIXED_FRAME_TIME;
#endif

        bool lastMinimized = gWindow.minimized;

        gQuit = handleMessages(gWindowDesc);

        // UPDATE BASE INTERFACES
        updateBaseSubsystems(deltaTime, baseSubsystemAppDrawn);
        baseSubsystemAppDrawn = false;

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
        if (gWindow.minimized)
        {
            // Call update once after minimize so app can react.
            if (lastMinimized != gWindow.minimized)
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
            gQuit = true;
#endif

#ifdef AUTOMATED_TESTING
        extern bool gAutomatedTestingScriptsFinished;
        // wait for the automated testing if it hasn't managed to finish in time
        if (gAutomatedTestingScriptsFinished && frameCounter >= targetFrameCount)
            gQuit = true;

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

    pApp->mSettings.mQuit = true;
    restoreResolutions();

    gReloadDescriptor.mType = RELOAD_TYPE_ALL;
    pApp->Unload(&gReloadDescriptor);

    closeWindow(&gWindow);
    destroyMonitorInfo();
    XCloseDisplay(gDefaultDisplay);

    pApp->Exit();

    exitLog();

    exitBaseSubsystems();

#if TF_USE_MTUNER
    rmemUnload();
    rmemShutDown();
#endif

    exitMemAlloc();

    exitFileSystem();

    return 0;
}

#endif
