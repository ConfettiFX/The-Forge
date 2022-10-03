/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#include "../../Application/Interfaces/IApp.h"
#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

#if defined(QUEST_VR)
#include "../Quest/VrApi.h"
#endif

// NOTE: We have to define all functions so that their refernces in 
// WindowSystem are properly resolved.

IApp* pWindowAppRef = NULL;
WindowDesc gWindow;

bool windowReady = false;
bool isActive = false;
bool isLoaded = false;

// AndroidBase.cpp
extern CustomMessageProcessor sCustomProc;

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

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

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

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

void closeWindow(const WindowDesc* winDesc)
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

void toggleBorderless(WindowDesc* winDesc, unsigned width, unsigned height)
{
	// No-op
}

void toggleFullscreen(WindowDesc* window)
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

void* createCursor(const char* path)
{
	return NULL; 
}

void  setCursor(void* cursor)
{
	// No-op
}

void  showCursor(void)
{
	// No-op
}

void  hideCursor(void)
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

bool  isCursorInsideTrackingArea(void)
{
	return true; 
}

void  setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
	// No-op
}

void  setMousePositionAbsolute(int32_t x, int32_t y)
{
	// No-op
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

MonitorDesc* getMonitor(uint32_t index)
{
	return NULL; 
}

uint32_t getMonitorCount(void)
{
	return 1; 
}

void getDpiScale(float array[2])
{
	array[0] = metrics.scaledDensity;
	array[1] = metrics.scaledDensity;
}

void getRecommendedResolution(RectDesc* rect)
{
	*rect = { 0, 0, (int32_t)metrics.widthPixels, (int32_t)metrics.heightPixels };
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	// No-op
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	return false; 
}

//------------------------------------------------------------------------
// ANDROID PLATFORM WINDOW PROCEDURES
//------------------------------------------------------------------------

void handleMessages(WindowDesc* winDesc)
{
	return;
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

			//pApp->pWindow = gWindowDesc;
			if (!windowReady)
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

int32_t handle_input(struct android_app* app, AInputEvent* event)
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

