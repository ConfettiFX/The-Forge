/*
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

#ifdef __ANDROID__

#include <ctime>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/log.h>

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"

#include "../Input/InputSystem.h"
#include "../Input/InputMappings.h"
#include "AndroidFileSystem.cpp"
#include "../Interfaces/IMemory.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define GETX(l) (int(l & 0xFFFF))
#define GETY(l) (int(l) >> 16)

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static eastl::vector<MonitorDesc>                gMonitors;
static eastl::unordered_map<void*, WindowsDesc*> gHWNDMap;
static WindowsDesc                                 gWindow;

void adjustWindow(WindowsDesc* winDesc);

void getRecommendedResolution(RectDesc* rect) { *rect = { 0, 0, 1920, 1080 }; }

void requestShutdown() { LOGF(LogLevel::eERROR, "Cannot manually shutdown on Android"); }

/************************************************************************/
// App Entrypoint
/************************************************************************/
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"

static IApp* pApp = NULL;
ANativeActivity* android_activity = NULL;

struct DisplayMetrics
{
    uint32_t widthPixels;
    uint32_t heightPixels;
    float density;
    uint32_t densityDpi;
    float scaledDensity;
    float xdpi;
    float ydpi;
};

DisplayMetrics metrics = {};

float2 getDpiScale()
{
	float2 ret = {};
	ret.x = metrics.scaledDensity;
	ret.y = metrics.scaledDensity;

	return ret;
}

float getDensity()
{
    return metrics.density;
}

void getDisplayMetrics(struct android_app* _android_app)
{
    if (!_android_app || !_android_app->activity || !_android_app->activity->vm )
        return;

    JNIEnv* jni = 0;
    _android_app->activity->vm->AttachCurrentThread(&jni, NULL);
    if (!jni )
        return;

    // get all the classes we want to access from the JVM
    jclass classNativeActivity = jni->FindClass("android/app/NativeActivity");
    jclass classWindowManager = jni->FindClass("android/view/WindowManager");
    jclass classDisplay = jni->FindClass("android/view/Display");
    jclass classDisplayMetrics = jni->FindClass("android/util/DisplayMetrics");

    if (!classNativeActivity || !classWindowManager || !classDisplay || !classDisplayMetrics)
    {
        _android_app->activity->vm->DetachCurrentThread();
        return;
    }

    // Get all the methods we want to access from the JVM classes
    // Note: You can get the signatures (third parameter of GetMethodID) for all
    // functions of a class with the javap tool, like in the following example for class DisplayMetrics:
    // javap -s -classpath myandroidpath/adt-bundle-linux-x86_64-20131030/sdk/platforms/android-10/android.jar android/util/DisplayMetrics
    jmethodID idNativeActivity_getWindowManager = jni->GetMethodID( classNativeActivity
            , "getWindowManager"
            , "()Landroid/view/WindowManager;");
    jmethodID idWindowManager_getDefaultDisplay = jni->GetMethodID( classWindowManager
            , "getDefaultDisplay"
            , "()Landroid/view/Display;");
    jmethodID idDisplayMetrics_constructor = jni->GetMethodID( classDisplayMetrics
            , "<init>"
            , "()V");
    jmethodID idDisplay_getMetrics = jni->GetMethodID( classDisplay
            , "getMetrics"
            , "(Landroid/util/DisplayMetrics;)V");

    if (!idNativeActivity_getWindowManager || !idWindowManager_getDefaultDisplay || !idDisplayMetrics_constructor
        || !idDisplay_getMetrics)
    {
        _android_app->activity->vm->DetachCurrentThread();
        return;
    }

    jobject windowManager = jni->CallObjectMethod(_android_app->activity->clazz, idNativeActivity_getWindowManager);

    if (!windowManager)
    {
        _android_app->activity->vm->DetachCurrentThread();
        return;
    }
    jobject display = jni->CallObjectMethod(windowManager, idWindowManager_getDefaultDisplay);
    if (!display)
    {
        _android_app->activity->vm->DetachCurrentThread();
        return;
    }
    jobject displayMetrics = jni->NewObject( classDisplayMetrics, idDisplayMetrics_constructor);
    if (!displayMetrics)
    {
        _android_app->activity->vm->DetachCurrentThread();
        return;
    }
    jni->CallVoidMethod(display, idDisplay_getMetrics, displayMetrics);

    // access the fields of DisplayMetrics (we ignore the DENSITY constants)
    jfieldID idDisplayMetrics_widthPixels = jni->GetFieldID( classDisplayMetrics, "widthPixels", "I");
    jfieldID idDisplayMetrics_heightPixels = jni->GetFieldID( classDisplayMetrics, "heightPixels", "I");
    jfieldID idDisplayMetrics_density = jni->GetFieldID( classDisplayMetrics, "density", "F");
    jfieldID idDisplayMetrics_densityDpi = jni->GetFieldID( classDisplayMetrics, "densityDpi", "I");
    jfieldID idDisplayMetrics_scaledDensity = jni->GetFieldID( classDisplayMetrics, "scaledDensity", "F");
    jfieldID idDisplayMetrics_xdpi = jni->GetFieldID(classDisplayMetrics, "xdpi", "F");
    jfieldID idDisplayMetrics_ydpi = jni->GetFieldID(classDisplayMetrics, "ydpi", "F");

    if ( idDisplayMetrics_widthPixels )
        metrics.widthPixels = jni->GetIntField(displayMetrics, idDisplayMetrics_widthPixels);
    if ( idDisplayMetrics_heightPixels )
        metrics.heightPixels = jni->GetIntField(displayMetrics, idDisplayMetrics_heightPixels);
    if (idDisplayMetrics_density )
        metrics.density = jni->GetFloatField(displayMetrics, idDisplayMetrics_density);
    if (idDisplayMetrics_densityDpi)
        metrics.densityDpi = jni->GetIntField(displayMetrics, idDisplayMetrics_densityDpi);
    if (idDisplayMetrics_scaledDensity)
        metrics.scaledDensity = jni->GetFloatField(displayMetrics, idDisplayMetrics_scaledDensity);
    if ( idDisplayMetrics_xdpi )
        metrics.xdpi = jni->GetFloatField(displayMetrics, idDisplayMetrics_xdpi);
    if ( idDisplayMetrics_ydpi )
        metrics.ydpi = jni->GetFloatField(displayMetrics, idDisplayMetrics_ydpi);

    _android_app->activity->vm->DetachCurrentThread();
}

void openWindow(const char* app_name, WindowsDesc* winDesc) {}

void handleMessages(WindowsDesc* winDesc) { return; }

void onStart(ANativeActivity* activity) { printf("start\b"); }

static bool    windowReady = false;
static bool    isActive = false;
static int32_t handle_input(struct android_app* app, AInputEvent* event)
{
	// Forward input events to Gainput
	InputSystem::UpdateSize(ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
	return InputSystem::HandleMessage(event);
}

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd)
	{
		case APP_CMD_INIT_WINDOW:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "init window");

			IApp::Settings* pSettings = &pApp->mSettings;
			gWindow.windowedRect = { 0, 0, ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window) };
			gWindow.fullScreen = pSettings->mFullScreen;
			gWindow.maximized = false;
			openWindow(pApp->GetName(), &gWindow);

			gWindow.handle.window = reinterpret_cast<void*>(app->window);

			pSettings->mWidth = ANativeWindow_getWidth(app->window);
			pSettings->mHeight = ANativeWindow_getHeight(app->window);
			pApp->pWindow = &gWindow;

			// The window is being shown, mark it as ready.
			if (!windowReady)
				pApp->Load();
			windowReady = true;

			InputSystem::UpdateSize(pSettings->mWidth, pSettings->mHeight);
			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "term window");

			// losing window, remove swapchain
			if (windowReady)
				pApp->Unload();
			windowReady = false;
			// The window is being hidden or closed, clean it up.
			break;
		}
		case APP_CMD_START:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "start app");
			break;
		}
		case APP_CMD_GAINED_FOCUS:
		{
			isActive = true;
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "resume app");
			break;
		}
		case APP_CMD_LOST_FOCUS:
		{
			isActive = false;
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "pause app");
			break;
		}
		case APP_CMD_STOP:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "stop app");
			break;
		}
		case APP_CMD_DESTROY:
		{
			// Activity is destroyed and waiting app to clean up. Request app to shut down.
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "shutting down app");
		}
		default:
		{
		}
	}
}

int AndroidMain(void* param, IApp* app)
{
	struct android_app* android_app = (struct android_app*)param;

	// Set the callback to process system events
    android_app->onAppCmd = handle_cmd;

	pApp = app;
	android_activity = android_app->activity;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t       testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	// Set asset manager.
	_mgr = (android_app->activity->assetManager);

	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	IApp::Settings* pSettings = &pApp->mSettings;
	Timer deltaTimer;
	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		RectDesc rect = {};
		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}
	getDisplayMetrics(android_app);

	InputSystem::Init(pSettings->mWidth, pSettings->mHeight);
    InputSystem::SetMouseCapture(true);
    InputSystem::SetHideMouseCursorWhileCaptured(false);
	// Set the callback to process input events
    android_app->onInputEvent = handle_input;

	if (!pApp->Init())
		abort();

	InputSystem::SetMouseCapture(true);

	bool quit = false;

	while (!quit)
	{
		// Used to poll the events in the main loop
		int                  events;
		android_poll_source* source;

		if (ALooper_pollAll(windowReady ? 1 : 0, NULL, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(android_app, source);
		}
		if (!windowReady || !isActive)
		{
			usleep(1);
			continue;
		}
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		InputSystem::Update();
		handleMessages(&gWindow);

		pApp->Update(deltaTime);
		pApp->Draw();

#ifdef AUTOMATED_TESTING
		//used in automated tests only.
		testingFrameCount++;
		if (testingFrameCount >= testingDesiredFrameCount)
			quit = true;
#endif
		if (android_app->destroyRequested)
			quit = true;
	}
	if (windowReady)
		pApp->Unload();
	windowReady = false;
	pApp->Exit();

	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
