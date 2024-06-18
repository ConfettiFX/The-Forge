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

#include <android/configuration.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <ctime>
#include <memory_advice/memory_advice.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include "../../Graphics/GraphicsConfig.h"

#if defined(GLES)
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif

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
#include "../CPUConfig.h"

#if defined(QUEST_VR)
#include "../Quest/VrApi.h"
#endif

#include "../../Utilities/Interfaces/IMemory.h"

static IApp*       pApp = NULL;
static WindowDesc* gWindowDesc = nullptr;
extern WindowDesc  gWindow;

static ResetDesc  gResetDescriptor = { RESET_TYPE_NONE };
static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool       gShutdownRequested = false;
static bool       gShowPlatformUI = true;

static ThermalStatus               gThermalStatus = THERMAL_STATUS_NONE;
static MemoryState                 gMemoryState = MEMORY_STATE_UNKNOWN;
static errorMessagePopupCallbackFn gErrorMessagePopupCallback = NULL;

/// CPU
static CpuInfo gCpu;
static OSInfo  gOsInfo = {};

/// UI
static UIComponent* pAPISwitchingWindow = NULL;
static UIComponent* pToggleVSyncWindow = NULL;
#if defined(ENABLE_FORGE_RELOAD_SHADER)
static UIComponent* pReloadShaderComponent = NULL;
#endif
UIWidget* pSwitchWindowLabel = NULL;
UIWidget* pSelectApUIWidget = NULL;

static uint32_t gSelectedApiIndex = 0;

// PickRenderingAPI.cpp
extern PlatformParameters gPlatformParameters;
extern bool               gGLESUnsupported;

// AndroidWindow.cpp
extern IApp* pWindowAppRef;
extern bool  windowReady;
extern bool  isActive;
extern bool  isLoaded;

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------
CpuInfo* getCpuInfo() { return &gCpu; }

OSInfo* getOsInfo() { return &gOsInfo; }

extern "C"
{
    JNIEXPORT void JNICALL TF_ANDROID_JAVA_NATIVE_EVENT(ForgeBaseActivity, nativeThermalEvent)(JNIEnv* env, jobject obj, jint status);
    JNIEXPORT void JNICALL TF_ANDROID_JAVA_NATIVE_EVENT(ForgeBaseActivity, nativeOnAlertClosed)(JNIEnv* env, jobject obj);
}

void JNICALL TF_ANDROID_JAVA_NATIVE_EVENT(ForgeBaseActivity, nativeThermalEvent)(JNIEnv* env, jobject obj, jint status)
{
    const ThermalStatus thermalStatus = (ThermalStatus)status;
    ASSERT(thermalStatus >= THERMAL_STATUS_MIN && thermalStatus < THERMAL_STATUS_MAX);

    LOGF(eINFO, "Thermal status event: %s (%d)", getThermalStatusString(thermalStatus), thermalStatus);
    gThermalStatus = thermalStatus;
}

void JNICALL TF_ANDROID_JAVA_NATIVE_EVENT(ForgeBaseActivity, nativeOnAlertClosed)(JNIEnv* env, jobject obj)
{
    if (gErrorMessagePopupCallback)
    {
        gErrorMessagePopupCallback();
    }
}

// this callback is called only for states other then MEMORYADVICE_STATE_OK.
void memoryStateWatcherCallback(MemoryAdvice_MemoryState state, void* userData)
{
    MemoryState tfState;
    switch (state)
    {
    case MEMORYADVICE_STATE_UNKNOWN:
        tfState = MEMORY_STATE_UNKNOWN;
        break;
    case MEMORYADVICE_STATE_OK:
        tfState = MEMORY_STATE_OK;
        break;
    case MEMORYADVICE_STATE_APPROACHING_LIMIT:
        tfState = MEMORY_STATE_APPROACHING_LIMIT;
        break;
    case MEMORYADVICE_STATE_CRITICAL:
        tfState = MEMORY_STATE_CRITICAL;
        break;
    default:
        tfState = MEMORY_STATE_UNKNOWN;
        break;
    }

    if (gMemoryState != tfState)
    {
        LOGF(eINFO, "Memory state change: %s (%d)", getMemoryStateString(tfState), tfState);
    }
    gMemoryState = tfState;
}

bool getBenchmarkArguments(android_app* pAndroidApp, JNIEnv* pJavaEnv, int& frameCount, uint32_t& requestRecompileAfter,
                           char* benchmarkOutput)
{
    if (!pAndroidApp || !pAndroidApp->activity || !pAndroidApp->activity->vm || !pJavaEnv)
        return false;

    jobject me = pAndroidApp->activity->clazz;

    jclass    acl = pJavaEnv->GetObjectClass(me); // class pointer of NativeActivity
    jmethodID giid = pJavaEnv->GetMethodID(acl, "getIntent", "()Landroid/content/Intent;");
    jobject   intent = pJavaEnv->CallObjectMethod(me, giid); // Got our intent

    jclass    icl = pJavaEnv->GetObjectClass(intent); // class pointer of Intent
    jmethodID gseid = pJavaEnv->GetMethodID(icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

    bool argumentsPassed = false;

    jstring benchmarkParam = (jstring)pJavaEnv->CallObjectMethod(intent, gseid, pJavaEnv->NewStringUTF("-b"));
    if (benchmarkParam != 0x0)
    {
        // get c string for value of parameter
        const char* benchParamCstr = pJavaEnv->GetStringUTFChars(benchmarkParam, 0);
        // convert to int.
        frameCount = (int)strtol(benchParamCstr, NULL, 10);
        // When done with it, or when you've made a copy
        pJavaEnv->ReleaseStringUTFChars(benchmarkParam, benchParamCstr);
        argumentsPassed = true;
    }

    jstring recompileAfterParam = (jstring)pJavaEnv->CallObjectMethod(intent, gseid, pJavaEnv->NewStringUTF("--request-recompile-after"));
    if (recompileAfterParam != 0x0)
    {
        // get c string for value of parameter
        const char* benchParamCstr = pJavaEnv->GetStringUTFChars(recompileAfterParam, 0);
        // convert to int.
        requestRecompileAfter = (int)strtol(benchParamCstr, NULL, 10);
        // When done with it, or when you've made a copy
        pJavaEnv->ReleaseStringUTFChars(recompileAfterParam, benchParamCstr);
        argumentsPassed = true;
    }

    jstring outputParam = (jstring)pJavaEnv->CallObjectMethod(intent, gseid, pJavaEnv->NewStringUTF("-o"));
    if (outputParam != 0x0)
    {
        // get c string for value of parameter
        const char* benchParamCstr = pJavaEnv->GetStringUTFChars(outputParam, 0);
        strcpy(benchmarkOutput, benchParamCstr);
        // When done with it, or when you've made a copy
        pJavaEnv->ReleaseStringUTFChars(outputParam, benchParamCstr);
        argumentsPassed = true;
    }

    return argumentsPassed;
}

void onStart(ANativeActivity* activity) { printf("start\b"); }

//------------------------------------------------------------------------
// OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown() { gShutdownRequested = true; }

void requestReset(const ResetDesc* pResetDesc) { gResetDescriptor = *pResetDesc; }

void requestReload(const ReloadDesc* pReloadDesc) { gReloadDescriptor = *pReloadDesc; }

void errorMessagePopup(const char* title, const char* msg, WindowHandle* handle, errorMessagePopupCallbackFn callback)
{
#if defined(AUTOMATED_TESTING) || defined(QUEST_VR)
    LOGF(eERROR, title);
    LOGF(eERROR, msg);
    if (callback)
    {
        callback();
    }
#else
    ASSERT(handle);
    gErrorMessagePopupCallback = callback;

    JNIEnv* jni = 0;
    handle->activity->vm->AttachCurrentThread(&jni, NULL);
    if (!jni)
    {
        return;
    }

    jclass    clazz = jni->GetObjectClass(handle->activity->clazz);
    jmethodID methodID = jni->GetMethodID(clazz, "showAlert", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!methodID)
    {
        LOGF(LogLevel::eERROR, "Could not find method \'showAlert\' in activity class");
        return;
    }

    jstring jTitle = jni->NewStringUTF(title);
    jstring jMessage = jni->NewStringUTF(msg);

    jni->CallVoidMethod(handle->activity->clazz, methodID, jTitle, jMessage);

    jni->DeleteLocalRef(jTitle);
    jni->DeleteLocalRef(jMessage);

#endif
}

CustomMessageProcessor sCustomProc = nullptr;
void                   setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

//------------------------------------------------------------------------
// DEVICE STATUS
//------------------------------------------------------------------------

ThermalStatus getThermalStatus(void) { return gThermalStatus; }

MemoryState getMemoryState(void) { return gMemoryState; }

//------------------------------------------------------------------------
// PLATFORM LAYER CORE SUBSYSTEMS
//------------------------------------------------------------------------

bool initBaseSubsystems(IApp* app)
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

    gSelectedApiIndex = gPlatformParameters.mSelectedRendererApi;

    static const char* pApiNames[] = {
#if defined(GLES)
        "GLES",
#endif
#if defined(VULKAN)
        "Vulkan",
#endif
    };
    uint32_t apiNameCount = sizeof(pApiNames) / sizeof(*pApiNames);

    if (apiNameCount > 1 && !gGLESUnsupported)
    {
        UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.01f);
        uiCreateComponent("API Switching", &UIComponentDesc, &pAPISwitchingWindow);

        // Select Api
        DropdownWidget selectApUIWidget;
        selectApUIWidget.pData = &gSelectedApiIndex;
        selectApUIWidget.pNames = pApiNames;
        selectApUIWidget.mCount = RENDERER_API_COUNT;

        pSelectApUIWidget = uiCreateComponentWidget(pAPISwitchingWindow, "Select API", &selectApUIWidget, WIDGET_TYPE_DROPDOWN);
        pSelectApUIWidget->pOnEdited = [](void* pUserData)
        {
            ResetDesc resetDesc;
            resetDesc.mType = RESET_TYPE_API_SWITCH;
            requestReset(&resetDesc);
        };
    }

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

    uiSetComponentActive(pToggleVSyncWindow, gShowPlatformUI);
#if defined(ENABLE_FORGE_RELOAD_SHADER)
    uiSetComponentActive(pReloadShaderComponent, gShowPlatformUI);
#endif
    if (pAPISwitchingWindow)
        uiSetComponentActive(pAPISwitchingWindow, gShowPlatformUI);
#endif
}

void processAllEvents(android_app* app, bool* windowReady, int* cancelationToken)
{
    ASSERT(app);
    ASSERT(windowReady);
    ASSERT(cancelationToken);

    while (!(*cancelationToken))
    {
        // Used to poll the events in the main loop
        int                  events;
        android_poll_source* source;
        if (ALooper_pollAll(*windowReady ? 1 : 0, NULL, &events, (void**)&source) >= 0)
        {
            if (source != NULL)
                source->process(app, source);
        }
    }
}

//------------------------------------------------------------------------
// APP ENTRY POINT
//------------------------------------------------------------------------

// AndroidWindow.cpp
extern void    handleMessages(WindowDesc*);
extern void    getDisplayMetrics(android_app*, JNIEnv*);
extern void    handle_cmd(android_app*, int32_t);
extern int32_t handle_input(struct android_app*, AInputEvent*);

int          IApp::argc;
const char** IApp::argv;

int AndroidMain(void* param, IApp* app)
{
    if (!initMemAlloc(app->GetName()))
    {
        while (0 > __android_log_print(ANDROID_LOG_ERROR, "The-Forge", "Error starting application"))
            ;
        return EXIT_FAILURE;
    }

    struct android_app* android_app = (struct android_app*)param;

    FileSystemInitDesc fsDesc = {};
    fsDesc.pPlatformData = android_app->activity;
    fsDesc.pAppName = app->GetName();
    if (!initFileSystem(&fsDesc))
    {
        while (0 > __android_log_print(ANDROID_LOG_ERROR, "The-Forge", "Error starting application"))
            ;
        return EXIT_FAILURE;
    }

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");
    fsSetPathForResourceDir(pSystemFileIO, RM_SYSTEM, RD_SYSTEM, "");

    initLog(app->GetName(), DEFAULT_LOG_LEVEL);

    snprintf(gOsInfo.osName, 256, "Android");
    bool knownVersion = __system_property_get("ro.build.version.sdk", gOsInfo.osVersion);
    bool knownModel = __system_property_get("ro.product.model", gOsInfo.osDeviceName);
    LOGF(LogLevel::eINFO, "Operating System: %s. Version: %s. Device Name: %s.", gOsInfo.osName,
         knownVersion ? gOsInfo.osVersion : "Unknown Version", knownModel ? gOsInfo.osDeviceName : "Unknown Model");

    JNIEnv* pJavaEnv;
    android_app->activity->vm->AttachCurrentThread(&pJavaEnv, NULL);

    // Set the callback to process system events
    gWindow.handle.type = WINDOW_HANDLE_TYPE_ANDROID;
    gWindow.handle.activity = android_app->activity;
    gWindow.handle.configuration = android_app->config;
    gWindow.cursorCaptured = false;
    gWindowDesc = &gWindow;

    android_app->onAppCmd = handle_cmd;
    pApp = app;
    pWindowAppRef = app;

    MemoryAdvice_ErrorCode code = MemoryAdvice_init(pJavaEnv, gWindow.handle.activity->clazz);
    if (code == MEMORYADVICE_ERROR_OK)
    {
        code = MemoryAdvice_registerWatcher(1000.0f, memoryStateWatcherCallback, NULL);
        gMemoryState = MEMORY_STATE_OK;
        if (code != MEMORYADVICE_ERROR_OK)
        {
            LOGF(eWARNING, "Unable to register Memory Advice watcher.");
        }
    }

    // Used for automated testing, if enabled app will exit after DEFAULT_AUTOMATION_FRAME_COUNT (240) frames
#ifdef AUTOMATED_TESTING
    uint32_t testingFrameCount = 0;
    uint32_t targetFrameCount = DEFAULT_AUTOMATION_FRAME_COUNT;
#endif

    initCpuInfo(&gCpu, pJavaEnv);

    IApp::Settings* pSettings = &pApp->mSettings;
    HiresTimer      deltaTimer;
    initHiresTimer(&deltaTimer);

    getDisplayMetrics(android_app, pJavaEnv);

#if defined(QUEST_VR)
    initVrApi(android_app, pJavaEnv);
    ASSERT(pQuest);
    pSettings->mWidth = pQuest->mEyeTextureWidth;
    pSettings->mHeight = pQuest->mEyeTextureHeight;
#else
    RectDesc rect = {};
    getRecommendedResolution(&rect);
    pSettings->mWidth = getRectWidth(&rect);
    pSettings->mHeight = getRectHeight(&rect);
#endif

#ifdef AUTOMATED_TESTING
    int             frameCountArgs;
    extern uint32_t gReloadServerRequestRecompileAfter;
    char            benchmarkOutput[1024] = { "\0" };
    bool            benchmarkArgs =
        getBenchmarkArguments(android_app, pJavaEnv, frameCountArgs, gReloadServerRequestRecompileAfter, &benchmarkOutput[0]);
    if (benchmarkArgs)
    {
        pSettings->mBenchmarking = true;
        targetFrameCount = frameCountArgs;
    }
#endif

    // Set the callback to process input events
    android_app->onInputEvent = handle_input;

    if (!initBaseSubsystems(pApp))
    {
        abort();
    }

    if (!pApp->Init())
    {
        const char* pRendererReason;
        if (hasRendererInitializationError(&pRendererReason))
        {
            pApp->ShowUnsupportedMessage(pRendererReason);
        }

        if (pApp->mUnsupported)
        {
            android_app->onAppCmd = NULL;
            android_app->onInputEvent = NULL;
            static int popupClosed = 0;
            errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "", &pApp->pWindow->handle,
                              []() { popupClosed = 1; });
            processAllEvents(android_app, &windowReady, &popupClosed);

#ifdef AUTOMATED_TESTING
            sleep(5); // logcat may "reorder" messages, this can break testing with apps that very quickly open and exit
#endif

            ANativeActivity_finish(android_app->activity);
            processAllEvents(android_app, &windowReady, &android_app->destroyRequested);

            exitLog();
#ifdef AUTOMATED_TESTING
            while (0 > __android_log_print(ANDROID_LOG_INFO, "The-Forge", "Success terminating application"))
                ;
#endif
            exit(0);
        }
        abort();
    }

    setupPlatformUI(pSettings->mWidth, pSettings->mHeight);
    pSettings->mInitialized = true;

#ifdef AUTOMATED_TESTING
    if (pSettings->mBenchmarking)
    {
        setAggregateFrames(targetFrameCount / 2);
    }
#endif

    bool baseSubsystemAppDrawn = false;
    bool quit = false;

    while (!quit)
    {
        // Used to poll the events in the main loop
        int                  events;
        android_poll_source* source;

        if (ALooper_pollAll(windowReady ? 1 : 0, NULL, &events, (void**)&source) >= 0)
        {
            if (source != NULL)
            {
#if defined(QUEST_VR)
                // Don't try to handle the Home, Volume Up and Volume Down buttons.
                if (source->process != android_app->inputPollSource.process)
                    source->process(android_app, source);
#else
                source->process(android_app, source);
#endif
            }
        }

#if defined(QUEST_VR)
        hook_poll_events(isActive, windowReady, pApp->pWindow->handle.window);
#endif

        float deltaTime = getHiresTimerSeconds(&deltaTimer, true);
        // if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
        if (deltaTime > 0.15f)
            deltaTime = 0.05f;

#if defined(AUTOMATED_TESTING)
        // Used to keep screenshot results consistent across CI runs
        deltaTime = AUTOMATION_FIXED_FRAME_TIME;
#endif

        handleMessages(&gWindow);

        if (gShutdownRequested && !android_app->destroyRequested)
        {
            ANativeActivity_finish(android_app->activity);
            pApp->mSettings.mQuit = true;
            continue;
        }

        // UPDATE BASE INTERFACES
        updateBaseSubsystems(deltaTime, baseSubsystemAppDrawn);
        baseSubsystemAppDrawn = false;

        if (isActive && isLoaded && gResetDescriptor.mType != RESET_TYPE_NONE)
        {
            gReloadDescriptor.mType = RELOAD_TYPE_ALL;

            pApp->Unload(&gReloadDescriptor);
            pApp->Exit();

            exitBaseSubsystems();

            gPlatformParameters.mSelectedRendererApi = (RendererApi)gSelectedApiIndex;
            pSettings->mInitialized = false;

            {
                if (!initBaseSubsystems(pApp))
                {
                    abort();
                }

                Timer t;
                initTimer(&t);
                if (!pApp->Init())
                {
                    if (pApp->mUnsupported)
                    {
                        android_app->onAppCmd = NULL;
                        android_app->onInputEvent = NULL;
                        static int popupClosed = 0;
                        errorMessagePopup("Application unsupported", pApp->pUnsupportedReason ? pApp->pUnsupportedReason : "",
                                          &pApp->pWindow->handle, []() { popupClosed = 1; });
                        processAllEvents(android_app, &windowReady, &popupClosed);

#ifdef AUTOMATED_TESTING
                        sleep(5); // logcat may "reorder" messages, this can break testing with apps that very quickly open and exit
#endif

                        ANativeActivity_finish(android_app->activity);
                        processAllEvents(android_app, &windowReady, &android_app->destroyRequested);

                        exitLog();
#ifdef AUTOMATED_TESTING
                        while (0 > __android_log_print(ANDROID_LOG_INFO, "The-Forge", "Success terminating application"))
                            ;
#endif
                        exit(0);
                    }
                    abort();
                }

                setupPlatformUI(pSettings->mWidth, pSettings->mHeight);
                pSettings->mInitialized = true;

                if (!pApp->Load(&gReloadDescriptor))
                {
                    abort();
                }

                LOGF(LogLevel::eINFO, "Application Reset %fms", getTimerMSec(&t, false) / 1000.0f);
            }

            gResetDescriptor.mType = RESET_TYPE_NONE;
            continue;
        }

        if (isActive && isLoaded && gReloadDescriptor.mType != RELOAD_TYPE_ALL)
        {
            Timer t;
            initTimer(&t);

            pApp->Unload(&gReloadDescriptor);

            if (!pApp->Load(&gReloadDescriptor))
            {
                abort();
            }

            LOGF(LogLevel::eINFO, "Application Reload %fms", getTimerMSec(&t, false) / 1000.0f);
            gReloadDescriptor.mType = RELOAD_TYPE_ALL;
            continue;
        }

        if (!windowReady || !isActive)
        {
            if (android_app->destroyRequested)
            {
                quit = true;
                pApp->mSettings.mQuit = true;
            }

            if (isLoaded && !windowReady)
            {
                gReloadDescriptor.mType = RELOAD_TYPE_ALL;
                pApp->Unload(&gReloadDescriptor);
                isLoaded = false;
            }

            usleep(1);
            continue;
        }

#if defined(QUEST_VR)
        if (pQuest->pOvr == NULL)
            continue;

        updateVrApi();
#endif

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
        {
            ANativeActivity_finish(android_app->activity);
            pApp->mSettings.mQuit = true;
        }
#endif

#ifdef AUTOMATED_TESTING
        extern bool gAutomatedTestingScriptsFinished;
        // wait for the automated testing if it hasn't managed to finish in time
        if (gAutomatedTestingScriptsFinished && testingFrameCount >= targetFrameCount)
        {
            ANativeActivity_finish(android_app->activity);
            pApp->mSettings.mQuit = true;
        }
        testingFrameCount++;
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
    if (isLoaded)
        pApp->Unload(&gReloadDescriptor);

    pApp->Exit();

    exitLog();

    exitBaseSubsystems();

#if defined(QUEST_VR)
    exitVrApi();
#endif

    exitFileSystem();

    exitMemAlloc();

#ifdef AUTOMATED_TESTING
    while (0 > __android_log_print(ANDROID_LOG_INFO, "The-Forge", "Success terminating application"))
        ;
#endif

    exit(0);
}
