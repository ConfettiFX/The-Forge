/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#include <ctime>
#include <unistd.h>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/log.h>

#if defined(GLES)
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#endif

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IProfiler.h"

#include "../Interfaces/IScripting.h"
#include "../Interfaces/IFont.h"
#include "../Interfaces/IUI.h"

#include "../../Renderer/IRenderer.h"

#if defined(QUEST_VR)
#include "../../../Quest/Common_3/OS/VR/VrApi.h"
#endif

#include "../Interfaces/IMemory.h"

static WindowsDesc gWindow;

static uint8_t gResetScenario = RESET_SCENARIO_NONE;

/// UI
static UIComponent* pAPISwitchingWindow = NULL;
UIWidget* pSwitchWindowLabel = NULL;
UIWidget* pSelectApUIWidget = NULL;

static uint32_t gSelectedApiIndex = 0;
extern RendererApi gSelectedRendererApi; // Renderer.cpp
extern bool gGLESUnsupported; // Renderer.cpp

void adjustWindow(WindowsDesc* winDesc);

void getRecommendedResolution(RectDesc* rect) { *rect = { 0, 0, 1920, 1080 }; }

void requestShutdown() { LOGF(LogLevel::eERROR, "Cannot manually shutdown on Android"); }

void toggleFullscreen(WindowsDesc* window) {}
/************************************************************************/
// App Entrypoint
/************************************************************************/
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"

static IApp* pApp = NULL;

struct DisplayMetrics
{
	uint32_t widthPixels;
	uint32_t heightPixels;
	float    density;
	uint32_t densityDpi;
	float    scaledDensity;
	float    xdpi;
	float    ydpi;
};

DisplayMetrics metrics = {};

void getDpiScale(float array[2])
{
	array[0] = metrics.scaledDensity;
	array[1] = metrics.scaledDensity;
}

float getDensity() { return metrics.density; }

bool getBenchmarkArguments(android_app* pAndroidApp, JNIEnv* pJavaEnv, int& frameCount, char * benchmarkOutput)
{
    if (!pAndroidApp || !pAndroidApp->activity || !pAndroidApp->activity->vm || !pJavaEnv)
        return false;

	jobject me = pAndroidApp->activity->clazz;

	jclass acl = pJavaEnv->GetObjectClass(me); //class pointer of NativeActivity
	jmethodID giid = pJavaEnv->GetMethodID(acl, "getIntent", "()Landroid/content/Intent;");
	jobject intent = pJavaEnv->CallObjectMethod(me, giid); //Got our intent

	jclass icl = pJavaEnv->GetObjectClass(intent); //class pointer of Intent
	jmethodID gseid = pJavaEnv->GetMethodID(icl, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

	bool argumentsPassed = false;

	jstring benchmarkParam = (jstring)pJavaEnv->CallObjectMethod(intent, gseid, pJavaEnv->NewStringUTF("-b"));
	if (benchmarkParam != 0x0)
	{
		//get c string for value of parameter
		const char *benchParamCstr = pJavaEnv->GetStringUTFChars(benchmarkParam, 0);
		//convert to int.
		frameCount = (int)strtol(benchParamCstr, NULL, 10);
		//When done with it, or when you've made a copy
		pJavaEnv->ReleaseStringUTFChars(benchmarkParam, benchParamCstr);
		argumentsPassed = true;
	}

	jstring outputParam = (jstring)pJavaEnv->CallObjectMethod(intent, gseid, pJavaEnv->NewStringUTF("-o"));
	if (outputParam != 0x0)
	{
		//get c string for value of parameter
		const char *benchParamCstr = pJavaEnv->GetStringUTFChars(outputParam, 0);
		strcpy(benchmarkOutput, benchParamCstr);
		//When done with it, or when you've made a copy
		pJavaEnv->ReleaseStringUTFChars(outputParam, benchParamCstr);
		argumentsPassed = true;
	}
	
	return argumentsPassed;
}

void getDisplayMetrics(android_app* pAndroidApp, JNIEnv* pJavaEnv)
{
	if (!pAndroidApp || !pAndroidApp->activity || !pAndroidApp->activity->vm || !pJavaEnv)
		return;

	// get all the classes we want to access from the JVM
	jclass classNativeActivity = pJavaEnv->FindClass("android/app/NativeActivity");
	jclass classWindowManager = pJavaEnv->FindClass("android/view/WindowManager");
	jclass classDisplay = pJavaEnv->FindClass("android/view/Display");
	jclass classDisplayMetrics = pJavaEnv->FindClass("android/util/DisplayMetrics");

	if (!classNativeActivity || !classWindowManager || !classDisplay || !classDisplayMetrics)
	{
        pAndroidApp->activity->vm->DetachCurrentThread();
		return;
	}

	// Get all the methods we want to access from the JVM classes
	// Note: You can get the signatures (third parameter of GetMethodID) for all
	// functions of a class with the javap tool, like in the following example for class DisplayMetrics:
	// javap -s -classpath myandroidpath/adt-bundle-linux-x86_64-20131030/sdk/platforms/android-10/android.jar android/util/DisplayMetrics
	jmethodID idNativeActivity_getWindowManager =
		pJavaEnv->GetMethodID(classNativeActivity, "getWindowManager", "()Landroid/view/WindowManager;");
	jmethodID idWindowManager_getDefaultDisplay = pJavaEnv->GetMethodID(classWindowManager, "getDefaultDisplay", "()Landroid/view/Display;");
	jmethodID idDisplayMetrics_constructor = pJavaEnv->GetMethodID(classDisplayMetrics, "<init>", "()V");
	jmethodID idDisplay_getMetrics = pJavaEnv->GetMethodID(classDisplay, "getMetrics", "(Landroid/util/DisplayMetrics;)V");

	if (!idNativeActivity_getWindowManager || !idWindowManager_getDefaultDisplay || !idDisplayMetrics_constructor || !idDisplay_getMetrics)
	{
        pAndroidApp->activity->vm->DetachCurrentThread();
		return;
	}

	jobject windowManager = pJavaEnv->CallObjectMethod(pAndroidApp->activity->clazz, idNativeActivity_getWindowManager);

	if (!windowManager)
	{
        pAndroidApp->activity->vm->DetachCurrentThread();
		return;
	}
	jobject display = pJavaEnv->CallObjectMethod(windowManager, idWindowManager_getDefaultDisplay);
	if (!display)
	{
        pAndroidApp->activity->vm->DetachCurrentThread();
		return;
	}
	jobject displayMetrics = pJavaEnv->NewObject(classDisplayMetrics, idDisplayMetrics_constructor);
	if (!displayMetrics)
	{
        pAndroidApp->activity->vm->DetachCurrentThread();
		return;
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
		metrics.widthPixels = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_widthPixels);
	if (idDisplayMetrics_heightPixels)
		metrics.heightPixels = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_heightPixels);
	if (idDisplayMetrics_density)
		metrics.density = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_density);
	if (idDisplayMetrics_densityDpi)
		metrics.densityDpi = pJavaEnv->GetIntField(displayMetrics, idDisplayMetrics_densityDpi);
	if (idDisplayMetrics_scaledDensity)
		metrics.scaledDensity = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_scaledDensity);
	if (idDisplayMetrics_xdpi)
		metrics.xdpi = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_xdpi);
	if (idDisplayMetrics_ydpi)
		metrics.ydpi = pJavaEnv->GetFloatField(displayMetrics, idDisplayMetrics_ydpi);
}

void openWindow(const char* app_name, WindowsDesc* winDesc) {}

void handleMessages(WindowsDesc* winDesc) { return; }

void onStart(ANativeActivity* activity) { printf("start\b"); }

static CustomMessageProcessor sCustomProc = nullptr;
void                          setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

static bool windowReady = false;
static bool isActive = false;
static bool isLoaded = false;

static int32_t handle_input(struct android_app* app, AInputEvent* event)
{
	if (AKeyEvent_getKeyCode(event) == AKEYCODE_BACK)
	{
		app->destroyRequested = 1;
		isActive = false;
		return 1;
	}

	if (sCustomProc != nullptr)
	{
		sCustomProc(&gWindow, event);
	}

	return 0;
}

static void onFocusChanged(bool focused)
{
	if (pApp == nullptr || !pApp->mSettings.mInitialized)
	{
		return;
	}

	pApp->mSettings.mFocused = focused;
}

void onRequestReload()
{
	gResetScenario |= RESET_SCENARIO_RELOAD;
}

void onDeviceLost()
{
	// NOT SUPPORTED ON THIS PLATFORM
}

void onAPISwitch()
{
	gResetScenario |= RESET_SCENARIO_API_SWITCH;
}

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd)
{
	switch (cmd)
	{
		case APP_CMD_INIT_WINDOW:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "init window");

#if !defined(QUEST_VR)
			int32_t screenWidth = ANativeWindow_getWidth(app->window);
			int32_t screenHeight = ANativeWindow_getHeight(app->window);
#else
            int32_t screenWidth = hook_window_width();
            int32_t screenHeight = hook_window_height();
#endif

			IApp::Settings* pSettings = &pApp->mSettings;
			gWindow.windowedRect = { 0, 0, screenWidth, screenHeight };
			gWindow.fullScreen = pSettings->mFullScreen;
			gWindow.maximized = false;
			gWindow.handle.window = app->window;
			pSettings->mWidth = screenWidth;
			pSettings->mHeight = screenHeight;
			openWindow(pApp->GetName(), &gWindow);

			pApp->pWindow = &gWindow;
			if (!windowReady)
			{
				pApp->Load();
				isLoaded = true;
			}

			// The window is being shown, mark it as ready.
			windowReady = true;

			break;
		}
		case APP_CMD_TERM_WINDOW:
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "term window");

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
            int32_t screenWidth = hook_window_width();
            int32_t screenHeight = hook_window_height();
#endif

			IApp::Settings* pSettings = &pApp->mSettings;
			if (pSettings->mWidth != screenWidth || pSettings->mHeight != screenHeight)
			{
				gWindow.windowedRect = { 0, 0, screenWidth, screenHeight };
				pSettings->mWidth = screenWidth;
				pSettings->mHeight = screenHeight;

				onRequestReload();
			}
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
			onFocusChanged(true);
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "resume app");
			break;
		}
		case APP_CMD_LOST_FOCUS:
		{
			isActive = false;
			onFocusChanged(false);
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "pause app");
			break;
		}
		case APP_CMD_PAUSE:
		{
			isActive = false;
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "pause app");
			break;
		}
		case APP_CMD_RESUME:
		{
			isActive = true;
			__android_log_print(ANDROID_LOG_VERBOSE, "the-forge-app", "resume app");
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

bool initBaseSubsystems(IApp* app)
{
	// Not exposed in the interface files / app layer
	extern bool platformInitFontSystem();
	extern bool platformInitUserInterface();
	extern void platformInitLuaScriptingSystem();

#ifdef USE_FORGE_FONTS
	if (!platformInitFontSystem())
		return false;
#endif

#ifdef USE_FORGE_UI
	if (!platformInitUserInterface())
		return false;
#endif

#ifdef USE_FORGE_SCRIPTING
	platformInitLuaScriptingSystem();
#endif

	return true;
}

void updateBaseSubsystems(float deltaTime)
{
	// Not exposed in the interface files / app layer
	extern void platformUpdateLuaScriptingSystem();
	extern void platformUpdateUserInterface(float deltaTime);

#ifdef USE_FORGE_SCRIPTING
	platformUpdateLuaScriptingSystem();
#endif

#ifdef USE_FORGE_UI
	platformUpdateUserInterface(deltaTime);
#endif
}

void exitBaseSubsystems()
{
	// Not exposed in the interface files / app layer
	extern void platformExitFontSystem();
	extern void platformExitUserInterface();
	extern void platformExitLuaScriptingSystem();

#ifdef USE_FORGE_UI
	platformExitUserInterface();
#endif

#ifdef USE_FORGE_FONTS
	platformExitFontSystem();
#endif

#ifdef USE_FORGE_SCRIPTING
	platformExitLuaScriptingSystem();
#endif
}

void setupAPISwitchingUI(int32_t width, int32_t height)
{
	gSelectedApiIndex = gSelectedRendererApi;

#ifdef USE_FORGE_UI
	static const char* pApiNames[] =
	{
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
		UIComponentDesc UIComponentDesc = {};
		UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.01f);
		uiCreateComponent("API Switching", &UIComponentDesc, &pAPISwitchingWindow);

		// Select Api 
		DropdownWidget selectApUIWidget;
		selectApUIWidget.pData = &gSelectedApiIndex;
		for (uint32_t i = 0; i < RENDERER_API_COUNT; ++i)
		{
			selectApUIWidget.mNames.push_back((char*)pApiNames[i]);
			selectApUIWidget.mValues.push_back(i);
		}
		pSelectApUIWidget = uiCreateComponentWidget(pAPISwitchingWindow, "Select API", &selectApUIWidget, WIDGET_TYPE_DROPDOWN);
		pSelectApUIWidget->pOnEdited = onAPISwitch;

#ifdef USE_FORGE_SCRIPTING
		luaRegisterWidget(pSelectApUIWidget);
		LuaScriptDesc apiScriptDesc = {};
		apiScriptDesc.pScriptFileName = "Test_API_Switching.lua";
		luaDefineScripts(&apiScriptDesc, 1);
#endif
	}
#endif
}

int AndroidMain(void* param, IApp* app)
{
	struct android_app* android_app = (struct android_app*)param;

	if (!initMemAlloc(app->GetName()))
	{
		__android_log_print(ANDROID_LOG_ERROR, "The-Forge", "Error starting application");
		return EXIT_FAILURE;
	}

	FileSystemInitDesc fsDesc = {};
	fsDesc.pPlatformData = android_app->activity;
	fsDesc.pAppName = app->GetName();
	if (!initFileSystem(&fsDesc))
	{
		__android_log_print(ANDROID_LOG_ERROR, "The-Forge", "Error starting application");
		return EXIT_FAILURE;
	}

	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

	initLog(app->GetName(), eALL);

    JNIEnv* pJavaEnv;
    android_app->activity->vm->AttachCurrentThread(&pJavaEnv, NULL);

	// Set the callback to process system events
	gWindow.handle.activity = android_app->activity;
	gWindow.handle.configuration = android_app->config;
	android_app->onAppCmd = handle_cmd;
	pApp = app;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t	testingFrameCount = 0;
	uint32_t	targetFrameCount = 120;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;
	Timer           deltaTimer;
	initTimer(&deltaTimer);
	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		RectDesc rect = {};
		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(&rect);
		pSettings->mHeight = getRectHeight(&rect);
	}
	getDisplayMetrics(android_app, pJavaEnv);
#ifdef AUTOMATED_TESTING
	int frameCountArgs;
	char benchmarkOutput[1024] = { "\0" };
	bool benchmarkArgs = getBenchmarkArguments(android_app, pJavaEnv, frameCountArgs, &benchmarkOutput[0]);
	if (benchmarkArgs)
	{
		pSettings->mBenchmarking = true;
		targetFrameCount = frameCountArgs;
	}
#endif

	pApp->pWindow = &gWindow;
	// Set the callback to process input events
	android_app->onInputEvent = handle_input;

#if defined(QUEST_VR)
    initVrApi(android_app, pJavaEnv);
#endif

    if (!initBaseSubsystems(pApp))
    {
        abort();
    }

    if (!pApp->Init())
    {
        abort();
    }

	setupAPISwitchingUI(pSettings->mWidth, pSettings->mHeight);
	pSettings->mInitialized = true;

#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking) setAggregateFrames(targetFrameCount / 2);
#endif

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

#if defined(QUEST_VR)
            hook_poll_events(isActive, windowReady, pApp->pWindow->handle.window);
#endif
		}

		if (isActive && gResetScenario != RESET_SCENARIO_NONE)
		{
			if (gResetScenario & RESET_SCENARIO_RELOAD)
			{
				pApp->Unload();

                if (!pApp->Load())
                {
                    abort();
                }

				gResetScenario &= ~RESET_SCENARIO_RELOAD;
				continue;
			}

			pApp->Unload();
			pApp->Exit();

			exitBaseSubsystems();

			gSelectedRendererApi = (RendererApi)gSelectedApiIndex;
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
                    abort();
                }

				setupAPISwitchingUI(pSettings->mWidth, pSettings->mHeight);
				pSettings->mInitialized = true;

                if (!pApp->Load())
                {
                    abort();
                }

				LOGF(LogLevel::eINFO, "Application Reset %f", getTimerMSec(&t, false) / 1000.0f);
			}

			gResetScenario = RESET_SCENARIO_NONE;
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
				pApp->Unload();
				isLoaded = false;
			}

			usleep(1);
			continue;
		}

#if defined(QUEST_VR)
        if (!isHeadsetReady())
            continue;

        updateVrApi();
#endif

		float deltaTime = getTimerMSec(&deltaTimer, true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		handleMessages(&gWindow);

		// UPDATE BASE INTERFACES
		updateBaseSubsystems(deltaTime);

		// UPDATE APP
		pApp->Update(deltaTime);
		pApp->Draw();

#ifdef AUTOMATED_TESTING
		//used in automated tests only.
		testingFrameCount++;
		if (testingFrameCount >= targetFrameCount)
		{
			ANativeActivity_finish(android_app->activity);
			pApp->mSettings.mQuit = true;
		}
#endif
	}

#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking)
	{
		dumpBenchmarkData(pSettings, benchmarkOutput, pApp->GetName());
		dumpProfileData(benchmarkOutput, targetFrameCount);
	}
#endif

	if (isLoaded)
		pApp->Unload();

	pApp->Exit();

	exitLog();

	exitBaseSubsystems();

#if defined(QUEST_VR)
    exitVrApi();
#endif

	exitFileSystem();

	exitMemAlloc();

#ifdef AUTOMATED_TESTING
	__android_log_print(ANDROID_LOG_INFO, "The-Forge", "Success terminating application");
#endif

	exit(0);
}
/************************************************************************/
/************************************************************************/
