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

#include "../../Graphics/GraphicsConfig.h"

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

#include "../CPUConfig.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/IFileSystem.h"

#include "../../Game/Interfaces/IScripting.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IUI.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#if defined(QUEST_VR)
#include "../Quest/VrApi.h"
#endif

#include "../../Utilities/Interfaces/IMemory.h"


static IApp* pApp = NULL;
static WindowDesc* gWindowDesc = nullptr;
extern WindowDesc gWindow;

static ResetDesc gResetDescriptor = { RESET_TYPE_NONE };
static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool    gShowPlatformUI = true;

/// CPU
static CpuInfo gCpu;

/// UI
static UIComponent* pAPISwitchingWindow = NULL;
static UIComponent* pToggleVSyncWindow = NULL;
UIWidget* pSwitchWindowLabel = NULL;
UIWidget* pSelectApUIWidget = NULL;

static uint32_t gSelectedApiIndex = 0;

// Renderer.cpp
extern RendererApi gSelectedRendererApi;
extern bool gGLESUnsupported;

// AndroidWindow.cpp
extern IApp* pWindowAppRef;
extern bool windowReady;
extern bool isActive;
extern bool isLoaded;

//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------
CpuInfo* getCpuInfo() {
	return &gCpu;
}

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

void onStart(ANativeActivity* activity)
{ 
	printf("start\b");
}

//------------------------------------------------------------------------
// OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void requestShutdown()
{
	LOGF(LogLevel::eERROR, "Cannot manually shutdown on Android");
}

void requestReset(const ResetDesc* pResetDesc)
{
	gResetDescriptor = *pResetDesc;
}

void requestReload(const ReloadDesc* pReloadDesc)
{
	gReloadDescriptor = *pReloadDesc;
}

void errorMessagePopup(const char* title, const char* msg, void* windowHandle)
{
#if !defined(QUEST_VR)
	ASSERT(windowHandle);
	
	WindowHandle* handle = (WindowHandle*)windowHandle;
	JNIEnv* jni = 0;
	handle->activity->vm->AttachCurrentThread(&jni, NULL);
	if (!jni)
		return;
	
	jclass clazz = jni->GetObjectClass(handle->activity->clazz);
	jmethodID methodID = jni->GetMethodID(clazz, "showAlert", "(Ljava/lang/String;Ljava/lang/String;)V");
	if (!methodID)
	{
		LOGF(LogLevel::eERROR, "Could not find method \'showAlert\' in activity class");
		handle->activity->vm->DetachCurrentThread();
		return;
	}
	
	jstring jTitle = jni->NewStringUTF(title);
	jstring jMessage = jni->NewStringUTF(msg);
	
	jni->CallVoidMethod(handle->activity->clazz, methodID, jTitle, jMessage);
	
	jni->DeleteLocalRef(jTitle);
	jni->DeleteLocalRef(jMessage);
	
	handle->activity->vm->DetachCurrentThread();
#endif
}

CustomMessageProcessor sCustomProc = nullptr;
void setCustomMessageProcessor(CustomMessageProcessor proc)
{
	sCustomProc = proc;
}

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
#endif

	return true;
}

void updateBaseSubsystems(float deltaTime)
{
	// Not exposed in the interface files / app layer
	extern void platformUpdateLuaScriptingSystem();
	extern void platformUpdateUserInterface(float deltaTime);
	extern void platformUpdateWindowSystem();

	platformUpdateWindowSystem();

#ifdef ENABLE_FORGE_SCRIPTING
	platformUpdateLuaScriptingSystem();
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

	gSelectedApiIndex = gSelectedRendererApi;

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
		UIComponentDesc.mStartPosition = vec2(width * 0.4f, height * 0.01f);
		uiCreateComponent("API Switching", &UIComponentDesc, &pAPISwitchingWindow);

		// Select Api 
		DropdownWidget selectApUIWidget;
		selectApUIWidget.pData = &gSelectedApiIndex;
		selectApUIWidget.pNames = pApiNames;
		selectApUIWidget.mCount = RENDERER_API_COUNT;

		pSelectApUIWidget = uiCreateComponentWidget(pAPISwitchingWindow, "Select API", &selectApUIWidget, WIDGET_TYPE_DROPDOWN);
		pSelectApUIWidget->pOnEdited = [](void* pUserData){ ResetDesc resetDesc; resetDesc.mType = RESET_TYPE_API_SWITCH; requestReset(&resetDesc); };

#ifdef ENABLE_FORGE_SCRIPTING
		REGISTER_LUA_WIDGET(pSelectApUIWidget);
		LuaScriptDesc apiScriptDesc = {};
		apiScriptDesc.pScriptFileName = "Test_API_Switching.lua";
		luaDefineScripts(&apiScriptDesc, 1);
#endif
	}
#endif
}

void togglePlatformUI()
{
	gShowPlatformUI = pApp->mSettings.mShowPlatformUI;

#ifdef ENABLE_FORGE_UI
	extern void platformToggleWindowSystemUI(bool);
	platformToggleWindowSystemUI(gShowPlatformUI); 

	uiSetComponentActive(pToggleVSyncWindow, gShowPlatformUI); 
	uiSetComponentActive(pAPISwitchingWindow, gShowPlatformUI);
#endif
}

//------------------------------------------------------------------------
// APP ENTRY POINT
//------------------------------------------------------------------------

// AndroidWindow.cpp
extern void handleMessages(WindowDesc*);
extern void getDisplayMetrics(android_app*, JNIEnv*);
extern void handle_cmd(android_app*, int32_t);
extern int32_t handle_input(struct android_app*, AInputEvent*);

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
	fsSetPathForResourceDir(pSystemFileIO, RM_SYSTEM, RD_SYSTEM, "");

	initLog(app->GetName(), DEFAULT_LOG_LEVEL);

    JNIEnv* pJavaEnv;
    android_app->activity->vm->AttachCurrentThread(&pJavaEnv, NULL);

	// Set the callback to process system events
	gWindow.handle.activity = android_app->activity;
	gWindow.handle.configuration = android_app->config;
	gWindow.cursorCaptured = false;
	gWindowDesc = &gWindow;

	android_app->onAppCmd = handle_cmd;
	pApp = app;
	pWindowAppRef = app; 

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t	testingFrameCount = 0;
	uint32_t	targetFrameCount = 120;
#endif

	initCpuInfo(&gCpu, pJavaEnv);

	IApp::Settings* pSettings = &pApp->mSettings;
	HiresTimer           deltaTimer;
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
	int frameCountArgs;
	char benchmarkOutput[1024] = { "\0" };
	bool benchmarkArgs = getBenchmarkArguments(android_app, pJavaEnv, frameCountArgs, &benchmarkOutput[0]);
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
        abort();
    }

	setupPlatformUI(pSettings->mWidth, pSettings->mHeight);
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

		if (isActive && gResetDescriptor.mType != RESET_TYPE_NONE)
		{
			gReloadDescriptor.mType = RELOAD_TYPE_ALL;

			pApp->Unload(&gReloadDescriptor);
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

		if (isActive && gReloadDescriptor.mType != RELOAD_TYPE_ALL)
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

		float deltaTime = getHiresTimerSeconds(&deltaTimer, true);
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		handleMessages(&gWindow);

		// UPDATE BASE INTERFACES
		updateBaseSubsystems(deltaTime);

		// UPDATE APP
		pApp->Update(deltaTime);
		pApp->Draw();

		if (gShowPlatformUI != pApp->mSettings.mShowPlatformUI)
		{
			togglePlatformUI(); 
		}

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
	__android_log_print(ANDROID_LOG_INFO, "The-Forge", "Success terminating application");
#endif

	exit(0);
}
