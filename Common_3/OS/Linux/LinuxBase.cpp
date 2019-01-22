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

#ifdef __linux__

#include <ctime>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

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

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_CURSOR_DELTA 200

#define GETX(l) (int(l & 0xFFFF))
#define GETY(l) (int(l) >> 16)

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

namespace {
bool isCaptured = false;
}

static bool gWindowClassInitialized = false;
//static WNDCLASSW  gWindowClass;

static int gCursorLastX = 0, gCursorLastY = 0;

static tinystl::vector<MonitorDesc>                gMonitors;
static tinystl::unordered_map<void*, WindowsDesc*> gHWNDMap;
static WindowsDesc                                 gWindow;
static float                                       gRetinaScale = 1.0f;

void adjustWindow(WindowsDesc* winDesc);

namespace PlatformEvents {
extern bool wantsMouseCapture;
extern bool skipMouseCapture;

extern void onWindowResize(const WindowResizeEventData* pData);
}    // namespace PlatformEvents

void getRecommendedResolution(RectDesc* rect) { *rect = { 0, 0, 1920, 1080 }; }

void requestShutdown()
{
	// #TODO: Test this
	XEvent event = {};
	event.type = ClientMessage;
	event.xclient.data.l[0] == gWindow.xlib_wm_delete_window;
	XSendEvent(gWindow.display, gWindow.xlib_window, false, 0, &event);
}

bool getKeyDown(int key) { return InputSystem::IsButtonPressed(key); }

bool getKeyUp(int key) { return InputSystem::IsButtonReleased(key); }

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

long getUSec()
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

float2 getDpiScale() { return { gRetinaScale, gRetinaScale }; }
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

// https://github.com/glfw/glfw/issues/1019
static double PlatformGetMonitorDPI(Display* display)
{
	char*       resourceString = XResourceManagerString(display);
	XrmDatabase db;
	XrmValue    value;
	char*       type = NULL;
	double      dpi = 0.0;

	XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

	db = XrmGetStringDatabase(resourceString);

	if (resourceString)
	{
		if (XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True)
		{
			if (value.addr)
			{
				dpi = atof(value.addr);
			}
		}
	}

	return dpi;
}

void openWindow(const char* app_name, WindowsDesc* winDesc)
{
	const char* display_envar = getenv("DISPLAY");
	if (display_envar == NULL || display_envar[0] == '\0')
	{
		printf("Environment variable DISPLAY requires a valid value.\nExiting ...\n");
		fflush(stdout);
		exit(1);
	}

	XInitThreads();
	winDesc->display = XOpenDisplay(NULL);
	long        visualMask = VisualScreenMask;
	int         numberOfVisuals;
	XVisualInfo vInfoTemplate = {};
	vInfoTemplate.screen = DefaultScreen(winDesc->display);
	XVisualInfo* visualInfo = XGetVisualInfo(winDesc->display, visualMask, &vInfoTemplate, &numberOfVisuals);

	Colormap colormap =
		XCreateColormap(winDesc->display, RootWindow(winDesc->display, vInfoTemplate.screen), visualInfo->visual, AllocNone);

	XSetWindowAttributes windowAttributes = {};
	windowAttributes.colormap = colormap;
	windowAttributes.background_pixel = 0xFFFFFFFF;
	windowAttributes.border_pixel = 0;
	windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

	winDesc->xlib_window = XCreateWindow(
		winDesc->display, RootWindow(winDesc->display, vInfoTemplate.screen), winDesc->windowedRect.left, winDesc->windowedRect.top,
		winDesc->windowedRect.right - winDesc->windowedRect.left, winDesc->windowedRect.bottom - winDesc->windowedRect.top, 0,
		visualInfo->depth, InputOutput, visualInfo->visual, CWBackPixel | CWBorderPixel | CWEventMask | CWColormap, &windowAttributes);

	//Added
	//set window title name
	XStoreName(winDesc->display, winDesc->xlib_window, app_name);

	char windowName[200];
	sprintf(windowName, "%s", app_name);

	//set hint name for window
	XClassHint hint;
	hint.res_class = windowName;    //class name
	hint.res_name = windowName;     //application name
	XSetClassHint(winDesc->display, winDesc->xlib_window, &hint);

	XSelectInput(
		winDesc->display, winDesc->xlib_window,
		ExposureMask | KeyPressMask |    //Key press
			KeyReleaseMask |             //Key release
			ButtonPressMask |            //Mouse click
			ButtonReleaseMask |          //Mouse release
			StructureNotifyMask |        //Resize
			PointerMotionMask            //Mouse movement
	);
	XMapWindow(winDesc->display, winDesc->xlib_window);
	XFlush(winDesc->display);
	winDesc->xlib_wm_delete_window = XInternAtom(winDesc->display, "WM_DELETE_WINDOW", False);

	double baseDpi = 96.0;
	gRetinaScale = (float)(PlatformGetMonitorDPI(winDesc->display) / baseDpi);
}

bool handleMessages(WindowsDesc* winDesc)
{
	bool quit = false;
	//this needs to be done before updating the events
	//that way current frame data will be delta after resetting mouse position
	if (InputSystem::IsMouseCaptured())
	{
		ButtonData button = InputSystem::GetButtonData(KEY_UI_MOVE);
		gCursorLastX = button.mValue[0];
		gCursorLastY = button.mValue[1];
		{
			float x = 0;
			float y = 0;
			x = (gWindow.windowedRect.right - gWindow.windowedRect.left) / 2;
			y = (gWindow.windowedRect.bottom - gWindow.windowedRect.top) / 2;
			XWarpPointer(gWindow.display, None, gWindow.xlib_window, 0, 0, 0, 0, x, y);
			InputSystem::WarpMouse(x, y);
			XFlush(winDesc->display);
		}
	}

	XEvent event;
	while (XPending(winDesc->display) > 0)
	{
		XNextEvent(winDesc->display, &event);
		InputSystem::HandleMessage(event);
		switch (event.type)
		{
			case ClientMessage:
				if ((Atom)event.xclient.data.l[0] == winDesc->xlib_wm_delete_window)
					quit = true;
				break;
			case DestroyNotify:
			{
				LOGINFO("Destroying the window");
				break;
			}
			case ConfigureNotify:
			{
				// Handle Resize event
				{
					RectDesc rect = { 0 };
					rect = { (int)event.xconfigure.x, (int)event.xconfigure.y, (int)event.xconfigure.width + (int)event.xconfigure.x,
							 (int)event.xconfigure.height + (int)event.xconfigure.y };
					gWindow.windowedRect = rect;

					WindowResizeEventData eventData = { rect, &gWindow };
					PlatformEvents::onWindowResize(&eventData);
					InputSystem::UpdateSize(event.xconfigure.width, event.xconfigure.height);
				}
				break;
			}
			default: break;
		}
	}

	XFlush(winDesc->display);

	if (InputSystem::IsButtonTriggered(KEY_CANCEL))
	{
		if (!isCaptured)
		{
			quit = true;
		}
		else
		{
			XUngrabPointer(gWindow.display, CurrentTime);
			isCaptured = false;
			InputSystem::SetMouseCapture(false);
		}
	}

	if (InputSystem::IsButtonPressed(KEY_CONFIRM) && !PlatformEvents::skipMouseCapture && !isCaptured)
	{
		// Create invisible cursor that will be used when mouse is captured
		Cursor      invisibleCursor;
		Pixmap      bitmapEmpty;
		XColor      emptyColor;
		static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		emptyColor.red = emptyColor.green = emptyColor.blue = 0;
		bitmapEmpty = XCreateBitmapFromData(gWindow.display, gWindow.xlib_window, emptyData, 8, 8);
		invisibleCursor = XCreatePixmapCursor(gWindow.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);
		// Capture mouse
		unsigned int masks = PointerMotionMask |    //Mouse movement
							 ButtonPressMask |      //Mouse click
							 ButtonReleaseMask;     // Mouse release
		int XRes = XGrabPointer(
			gWindow.display, gWindow.xlib_window, 1 /*reports with respect to the grab window*/, masks, GrabModeAsync, GrabModeAsync, None,
			invisibleCursor, CurrentTime);

		isCaptured = true;
		InputSystem::SetMouseCapture(true);
	}

	return quit;
}

int LinuxMain(int argc, char** argv, IApp* app)
{
	pApp = app;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t       testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	FileSystem::SetCurrentDir(FileSystem::GetProgramDir());

	IApp::Settings* pSettings = &pApp->mSettings;
	Timer           deltaTimer;

	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		RectDesc rect = {};
		getRecommendedResolution(&rect);
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}

	gWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
	gWindow.fullScreen = pSettings->mFullScreen;
	gWindow.maximized = false;
	openWindow(pApp->GetName(), &gWindow);

	pSettings->mWidth = gWindow.fullScreen ? getRectWidth(gWindow.fullscreenRect) : getRectWidth(gWindow.windowedRect);
	pSettings->mHeight = gWindow.fullScreen ? getRectHeight(gWindow.fullscreenRect) : getRectHeight(gWindow.windowedRect);
	pApp->pWindow = &gWindow;

	if (!pApp->Init())
		return EXIT_FAILURE;

	if (!pApp->Load())
		return EXIT_FAILURE;

	InputSystem::Init(pSettings->mWidth, pSettings->mHeight);

	registerWindowResizeEvent(onResize);

	bool quit = false;

	while (!quit)
	{
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		InputSystem::Update();

		quit = handleMessages(&gWindow);

		pApp->Update(deltaTime);
		pApp->Draw();

#ifdef AUTOMATED_TESTING
		//used in automated tests only.
		testingFrameCount++;
		if (testingFrameCount >= testingDesiredFrameCount)
			quit = true;
#endif
	}

	InputSystem::Shutdown();
	pApp->Unload();
	pApp->Exit();

	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
