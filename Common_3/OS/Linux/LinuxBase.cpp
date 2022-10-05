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

#include "../../Application/Config.h"

#ifdef __linux__

#include "../CPUConfig.h"

#include <ctime>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrandr.h>

#include <gtk/gtk.h>

#include "../../Utilities/ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../../Utilities/Math/MathTypes.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Application/Interfaces/IProfiler.h"
#include "../../Application/Interfaces/IApp.h"
#include "../../Utilities/Interfaces/IFileSystem.h"

#include "../../Game/Interfaces/IScripting.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IUI.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#include "../../Utilities/Interfaces/IMemory.h"

static IApp* pApp = NULL;
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
extern Display*      gDefaultDisplay;

static bool         gQuit;

static ReloadDesc gReloadDescriptor = { RELOAD_TYPE_ALL };
static bool    gShowPlatformUI = true;

/// CPU
static CpuInfo gCpu;

/// VSync Toggle
static UIComponent* pToggleVSyncWindow = NULL;

//------------------------------------------------------------------------
// OPERATING SYSTEM INTERFACE FUNCTIONS
//------------------------------------------------------------------------
CpuInfo* getCpuInfo() {
	return &gCpu;
}

void requestReload(const ReloadDesc* pReloadDesc)
{
	gReloadDescriptor = *pReloadDesc;
}

void requestShutdown()
{
    gQuit = true;
}

CustomMessageProcessor sCustomProc = nullptr;
void setCustomMessageProcessor(CustomMessageProcessor proc) { sCustomProc = proc; }

void errorMessagePopup(const char* title, const char* msg, void* windowHandle)
{
#ifndef AUTOMATED_TESTING
	GtkDialogFlags flags = GTK_DIALOG_MODAL;
	GtkWidget*     dialog = gtk_message_dialog_new(NULL, flags, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(GTK_WIDGET(dialog));
	while (g_main_context_iteration(NULL, false))
		;
#endif
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
#endif
}

void togglePlatformUI()
{
	gShowPlatformUI = pApp->mSettings.mShowPlatformUI;

#ifdef ENABLE_FORGE_UI
	extern void platformToggleWindowSystemUI(bool);
	platformToggleWindowSystemUI(gShowPlatformUI); 

	uiSetComponentActive(pToggleVSyncWindow, gShowPlatformUI); 
#endif
}

//------------------------------------------------------------------------
// APP ENTRY POINT
//------------------------------------------------------------------------

int LinuxMain(int argc, char** argv, IApp* app)
{
	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = app->GetName();

	if (!initFileSystem(&fsDesc))
		return EXIT_FAILURE;

	fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

	if (!initMemAlloc(app->GetName()))
		return EXIT_FAILURE;

#if TF_USE_MTUNER
	rmemInit(0);
#endif

	initLog(app->GetName(), DEFAULT_LOG_LEVEL);

	pApp = app;
    pWindowAppRef = app; 

	//Used for automated testing, if enabled app will exit after 240 frames
#if defined(AUTOMATED_TESTING)
	uint32_t frameCounter = 0;
	uint32_t targetFrameCount = 240;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;

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
	//Check if benchmarking was given through command line
	for (int i = 0; i < argc; i += 1)
	{
		if (strcmp(argv[i], "-b") == 0)
		{
			pSettings->mBenchmarking = true;
			if (i + 1 < argc && isdigit(*argv[i + 1]))
				targetFrameCount = min(max(atoi(argv[i + 1]), 32), 512);
		}
		else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
		{
			strcpy(benchmarkOutput, argv[i + 1]);
		}
	}
#endif

	if (!pApp->Init())
		return EXIT_FAILURE;

    setupPlatformUI(pSettings->mWidth, pSettings->mHeight); 
	pApp->mSettings.mInitialized = true;

	if (!pApp->Load(&gReloadDescriptor))
		return EXIT_FAILURE;
            
#ifdef AUTOMATED_TESTING
	if (pSettings->mBenchmarking) setAggregateFrames(targetFrameCount / 2);
#endif

    initCpuInfo(&gCpu);

	gtk_init(&argc, &argv);

	int64_t lastCounter = getUSec(false);
	while (!gQuit)
	{

		int64_t counter = getUSec(false);
		float   deltaTime = (float)(counter - lastCounter) / (float)1e6;
		lastCounter = counter;

		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		bool lastMinimized = gWindow.minimized;

		gQuit = handleMessages(gWindowDesc);

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
		frameCounter++;
		if (frameCounter >= targetFrameCount)
			gQuit = true;
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
