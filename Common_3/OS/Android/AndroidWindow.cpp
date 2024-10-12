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
#include <unistd.h>

#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#if defined(QUEST_VR)
#include "../Quest/VrApi.h"
#endif

// NOTE: We have to define all functions so that their refernces in
// WindowSystem are properly resolved.

IApp*      pWindowAppRef = NULL;
WindowDesc gWindow;

bool windowReady = false;
bool isActive = false;
bool isLoaded = false;

// AndroidBase.cpp
extern CustomMessageProcessor sCustomProc;

static MonitorDesc gMonitor;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

struct DisplayMetrics
{
    float    density;
    uint32_t densityDpi;
    float    scaledDensity;
};

DisplayMetrics metrics = {};

static void onFocusChanged(bool focused)
{
    if (pWindowAppRef == nullptr || !pWindowAppRef->mSettings.mInitialized)
    {
        return;
    }

    pWindowAppRef->mSettings.mFocused = focused;
}

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void openWindow(const char* app_name, WindowDesc* winDesc)
{
    // No-op
}

void closeWindow(WindowDesc* winDesc)
{
    // No-op
}

void setWindowRect(WindowDesc* winDesc, const RectDesc* rect)
{
    // No-op
}

void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    // No-op
}

void setWindowClientRect(WindowDesc* winDesc, const RectDesc* rect)
{
    // No-op
}

void setWindowClientSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
    // No-op
}

void setWindowed(WindowDesc* winDesc)
{
    // No-op
}

void setBorderless(WindowDesc* winDesc)
{
    // No-op
}

void toggleFullscreen(WindowDesc* pWindow)
{
    // No-op
}

void setFullscreen(WindowDesc* pWindow)
{
    // No-op
}

void showWindow(WindowDesc* winDesc)
{
    // No-op
}

void hideWindow(WindowDesc* winDesc)
{
    // No-op
}

void maximizeWindow(WindowDesc* winDesc)
{
    // No-op
}

void minimizeWindow(WindowDesc* winDesc)
{
    // No-op
}

void centerWindow(WindowDesc* winDesc)
{
    // No-op
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void* createCursor(const char* path) { return NULL; }

void setCursor(void* cursor)
{
    // No-op
}

void showCursor(void)
{
    // No-op
}

void hideCursor(void)
{
    // No-op
}

void captureCursor(WindowDesc* winDesc, bool bEnable)
{
    ASSERT(winDesc);

    if (winDesc->cursorCaptured != bEnable)
    {
        winDesc->cursorCaptured = bEnable;
    }
}

bool isCursorInsideTrackingArea(void) { return true; }

void setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
    // No-op
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
    // No-op
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

MonitorDesc* getMonitor(uint32_t index)
{
    ASSERT(index == 0);
    UNREF_PARAM(index);
    return &gMonitor;
}

uint32_t getMonitorCount(void) { return 1; }

uint32_t getActiveMonitorIdx() { return 0; }

void getDpiScale(float array[2])
{
    array[0] = metrics.scaledDensity;
    array[1] = metrics.scaledDensity;
}

void getMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2])
{
    ASSERT(monitorIndex == 0);
    UNREF_PARAM(monitorIndex);
    getDpiScale(dpiScale);
}

void getRecommendedWindowRect(WindowDesc*, RectDesc* rect) { getRecommendedResolution(rect); }

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
    // No-op
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes) { return false; }

//------------------------------------------------------------------------
// ANDROID PLATFORM WINDOW PROCEDURES
//------------------------------------------------------------------------

void handleMessages(WindowDesc* winDesc) { return; }

// Process the next main command.
#define ANDROID_FLUSH_LOG(msg)                                                 \
    while (0 > __android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", msg)) \
    {                                                                          \
    }

void handle_cmd(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
    {
        ANDROID_FLUSH_LOG("init window")

#if !defined(QUEST_VR)
        int32_t screenWidth = ANativeWindow_getWidth(app->window);
        int32_t screenHeight = ANativeWindow_getHeight(app->window);
#else
        int32_t screenWidth = pQuest->mEyeTextureWidth;
        int32_t screenHeight = pQuest->mEyeTextureHeight;
#endif

        IApp::Settings* pSettings = &pWindowAppRef->mSettings;
        gWindow.windowedRect = { 0, 0, screenWidth, screenHeight };
        gWindow.fullScreen = pSettings->mFullScreen;
        gWindow.maximized = false;
        gWindow.handle.window = app->window;
        pSettings->mWidth = screenWidth;
        pSettings->mHeight = screenHeight;
        openWindow(pWindowAppRef->GetName(), &gWindow);

        // pApp->pWindow = gWindowDesc;
        if (!windowReady && !pSettings->mQuit)
        {
            ReloadDesc reloadDesc;
            reloadDesc.mType = RELOAD_TYPE_ALL;
            pWindowAppRef->Load(&reloadDesc);

            isLoaded = true;
        }

        // The window is being shown, mark it as ready.
        windowReady = true;

        break;
    }
    case APP_CMD_TERM_WINDOW:
    {
        ANDROID_FLUSH_LOG("term window")
        windowReady = false;

        // The window is being hidden or closed, clean it up.
        break;
    }
    case APP_CMD_WINDOW_RESIZED:
    {
#if !defined(QUEST_VR)
        int32_t screenWidth = ANativeWindow_getWidth(app->window);
        int32_t screenHeight = ANativeWindow_getHeight(app->window);
#else
        int32_t screenWidth = pQuest->mEyeTextureWidth;
        int32_t screenHeight = pQuest->mEyeTextureHeight;
#endif

        IApp::Settings* pSettings = &pWindowAppRef->mSettings;
        if (pSettings->mWidth != screenWidth || pSettings->mHeight != screenHeight)
        {
            gWindow.windowedRect = { 0, 0, screenWidth, screenHeight };
            pSettings->mWidth = screenWidth;
            pSettings->mHeight = screenHeight;
            ReloadDesc reloadDesc;
            reloadDesc.mType = RELOAD_TYPE_RESIZE;
            requestReload(&reloadDesc);
        }
        break;
    }
    case APP_CMD_START:
    {
        ANDROID_FLUSH_LOG("start app")
        extern JNIEnv* pMainJavaEnv;
        extern void    platformInputOnStart(JNIEnv*);
        platformInputOnStart(pMainJavaEnv);
        break;
    }
    case APP_CMD_GAINED_FOCUS:
    {
        isActive = true;
        onFocusChanged(true);
        ANDROID_FLUSH_LOG("resume app")
        break;
    }
    case APP_CMD_LOST_FOCUS:
    {
        isActive = false;
        onFocusChanged(false);
        ANDROID_FLUSH_LOG("pause app")
        break;
    }
    case APP_CMD_PAUSE:
    {
        isActive = false;
        ANDROID_FLUSH_LOG("pause app")
        break;
    }
    case APP_CMD_RESUME:
    {
        isActive = true;
        ANDROID_FLUSH_LOG("resume app")
        break;
    }
    case APP_CMD_STOP:
    {
        ANDROID_FLUSH_LOG("stop app")
        extern JNIEnv* pMainJavaEnv;
        extern void    platformInputOnStop(JNIEnv*);
        platformInputOnStop(pMainJavaEnv);
        break;
    }
    case APP_CMD_DESTROY:
    {
        // Activity is destroyed and waiting app to clean up. Request app to shut down.
        ANDROID_FLUSH_LOG("shutting down app")
    }
    default:
    {
    }
    }
}

//------------------------------------------------------------------------
// WINDOW SYSTEM INIT/EXIT
//------------------------------------------------------------------------

bool initWindowSystem(android_app* pAndroidApp, JNIEnv* pJavaEnv)
{
    if (!pAndroidApp || !pAndroidApp->activity || !pAndroidApp->activity->vm || !pJavaEnv)
        return false;

    // get all the classes we want to access from the JVM
    jclass classNativeActivity = pJavaEnv->FindClass("android/app/NativeActivity");
    jclass classWindowManager = pJavaEnv->FindClass("android/view/WindowManager");
    jclass classDisplay = pJavaEnv->FindClass("android/view/Display");
    jclass classDisplayMetrics = pJavaEnv->FindClass("android/util/DisplayMetrics");

    if (!classNativeActivity || !classWindowManager || !classDisplay || !classDisplayMetrics)
    {
        pAndroidApp->activity->vm->DetachCurrentThread();
        return false;
    }

    // Get all the methods we want to access from the JVM classes
    // Note: You can get the signatures (third parameter of GetMethodID) for all
    // functions of a class with the javap tool, like in the following example for class DisplayMetrics:
    // javap -s -classpath myandroidpath/adt-bundle-linux-x86_64-20131030/sdk/platforms/android-10/android.jar android/util/DisplayMetrics
    jmethodID idNativeActivity_getWindowManager =
        pJavaEnv->GetMethodID(classNativeActivity, "getWindowManager", "()Landroid/view/WindowManager;");
    jmethodID idWindowManager_getDefaultDisplay =
        pJavaEnv->GetMethodID(classWindowManager, "getDefaultDisplay", "()Landroid/view/Display;");
    jmethodID idDisplayMetrics_constructor = pJavaEnv->GetMethodID(classDisplayMetrics, "<init>", "()V");
    jmethodID idDisplay_getMetrics = pJavaEnv->GetMethodID(classDisplay, "getMetrics", "(Landroid/util/DisplayMetrics;)V");

    if (!idNativeActivity_getWindowManager || !idWindowManager_getDefaultDisplay || !idDisplayMetrics_constructor || !idDisplay_getMetrics)
    {
        pAndroidApp->activity->vm->DetachCurrentThread();
        return false;
    }

    jobject windowManager = pJavaEnv->CallObjectMethod(pAndroidApp->activity->clazz, idNativeActivity_getWindowManager);

    if (!windowManager)
    {
        pAndroidApp->activity->vm->DetachCurrentThread();
        return false;
    }
    jobject display = pJavaEnv->CallObjectMethod(windowManager, idWindowManager_getDefaultDisplay);
    if (!display)
    {
        pAndroidApp->activity->vm->DetachCurrentThread();
        return false;
    }
    jobject displayMetrics = pJavaEnv->NewObject(classDisplayMetrics, idDisplayMetrics_constructor);
    if (!displayMetrics)
    {
        pAndroidApp->activity->vm->DetachCurrentThread();
        return false;
    }
    pJavaEnv->CallVoidMethod(display, idDisplay_getMetrics, displayMetrics);

    // access the fields of DisplayMetrics (we ignore the DENSITY constants)
    jfieldID idDisplayMetrics_widthPixels = pJavaEnv->GetFieldID(classDisplayMetrics, "widthPixels", "I");
    jfieldID idDisplayMetrics_heightPixels = pJavaEnv->GetFieldID(classDisplayMetrics, "heightPixels", "I");
    jfieldID idDisplayMetrics_density = pJavaEnv->GetFieldID(classDisplayMetrics, "density", "F");
    jfieldID idDisplayMetrics_densityDpi = pJavaEnv->GetFieldID(classDisplayMetrics, "densityDpi", "I");
    jfieldID idDisplayMetrics_scaledDensity = pJavaEnv->GetFieldID(classDisplayMetrics, "scaledDensity", "F");
    jfieldID idDisplayMetrics_xdpi = pJavaEnv->GetFieldID(classDisplayMetrics, "xdpi", "F");
    jfieldID idDisplayMetrics_ydpi = pJavaEnv->GetFieldID(classDisplayMetrics, "ydpi", "F");

    if (idDisplayMetrics_widthPixels)
        gMonitor.defaultResolution.mWidth = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_widthPixels);
    if (idDisplayMetrics_heightPixels)
        gMonitor.defaultResolution.mHeight = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_heightPixels);
    if (idDisplayMetrics_density)
        metrics.density = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_density);
    if (idDisplayMetrics_densityDpi)
        metrics.densityDpi = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_densityDpi);
    if (idDisplayMetrics_scaledDensity)
        metrics.scaledDensity = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_scaledDensity);
    if (idDisplayMetrics_xdpi)
        gMonitor.dpi[0] = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_xdpi);
    if (idDisplayMetrics_ydpi)
        gMonitor.dpi[1] = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_ydpi);

    arrsetlen(gMonitor.resolutions, 1);
    gMonitor.resolutions[0] = gMonitor.defaultResolution;

    gMonitor.monitorRect.left = 0;
    gMonitor.monitorRect.top = 0;
    gMonitor.monitorRect.right = gMonitor.defaultResolution.mWidth;
    gMonitor.monitorRect.top = gMonitor.defaultResolution.mHeight;
    gMonitor.workRect = gMonitor.monitorRect;

    return true;
}

void exitWindowSystem() { arrfree(gMonitor.resolutions); }
