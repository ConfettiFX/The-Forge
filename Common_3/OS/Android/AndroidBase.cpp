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

#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/IPlatformEvents.h"
#include "../Interfaces/ILogManager.h"
#include "../Interfaces/ITimeManager.h"
#include "../Interfaces/IThread.h"

#include "../../../Middleware_3/Input/InputSystem.h"
#include "../../../Middleware_3/Input/InputMappings.h"
#include "../Interfaces/IMemoryManager.h"
#include "AndroidFileSystem.cpp"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_KEYS 256
#define MAX_CURSOR_DELTA 200

#define GETX(l) (int(l & 0xFFFF))
#define GETY(l) (int(l) >> 16)

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static tinystl::vector<MonitorDesc>                gMonitors;
static tinystl::unordered_map<void*, WindowsDesc*> gHWNDMap;
static WindowsDesc                                 gWindow;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents {
extern void onWindowResize(const WindowResizeEventData* pData);
}

void getRecommendedResolution(RectDesc* rect) { *rect = { 0, 0, 1920, 1080 }; }

void requestShutdown() { LOGERROR("Cannot manually shutdown on Android"); }

bool getKeyDown(int key) { return false; }

bool getKeyUp(int key) { return false; }

bool getJoystickButtonDown(int button)
{
	ASSERT(0);    // We don't support joystick
	return false;
}

bool getJoystickButtonUp(int button)
{
	ASSERT(0);    // We don't support joystick
	return false;
}

/************************************************************************/
// Time Related Functions
/************************************************************************/

unsigned getSystemTime()
{
	long            ms;    // Milliseconds
	time_t          s;     // Seconds
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);

	s = spec.tv_sec;
	ms = round(spec.tv_nsec / 1.0e6);    // Convert nanoseconds to milliseconds

	ms += s * 1000;

	return (unsigned int)ms;
}

int64_t getUSec()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long us = (ts.tv_nsec / 1000);
	us += ts.tv_sec * 1e6;
	return us;
}

unsigned getTimeSinceStart() { return (unsigned)time(NULL); }

int64_t getTimerFrequency()
{
	// This is us to s
	return 1000000LL;
}
/************************************************************************/
// App Entrypoint
/************************************************************************/
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"

static IApp* pApp = NULL;

static void onResize(const WindowResizeEventData* pData)
{
	pApp->mSettings.mWidth = getRectWidth(pData->rect);
	pApp->mSettings.mHeight = getRectHeight(pData->rect);
	pApp->mSettings.mFullScreen = pData->pWindow->fullScreen;
	pApp->Unload();
	pApp->Load();
}

float2 getDpiScale()
{
	float2 ret = {};
	ret.x = 1.0f;
	ret.y = 1.0f;

	return ret;
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

			gWindow.handle = reinterpret_cast<WindowHandle>(app->window);

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

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t       testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	// Set asset manager.
	_mgr = (android_app->activity->assetManager);

	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	IApp::Settings* pSettings = &pApp->mSettings;

	if (!pApp->Init())
		abort();

	InputSystem::Init(pSettings->mWidth, pSettings->mHeight);
	// Set the callback to process input events
	android_app->onInputEvent = handle_input;

	InputSystem::SetMouseCapture(true);

	Timer deltaTimer;
	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		RectDesc rect = {};
		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}

	registerWindowResizeEvent(onResize);

	bool quit = false;

	while (!quit)
	{
		// Used to poll the events in the main loop
		int                  events;
		android_poll_source* source;

		if (ALooper_pollAll(windowReady ? 1 : 0, nullptr, &events, (void**)&source) >= 0)
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
		if (android_app->destroy_requested)
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
