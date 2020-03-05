/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"

#include "../Interfaces/IMemory.h"

#define CONFETTI_WINDOW_CLASS L"confetti"
#define MAX_CURSOR_DELTA 200

#define GETX(l) (int(l & 0xFFFF))
#define GETY(l) (int(l) >> 16)

#define elementsOf(a) (sizeof(a) / sizeof((a)[0]))

static bool gWindowClassInitialized = false;

static eastl::vector<MonitorDesc>                gMonitors;
static eastl::unordered_map<void*, WindowsDesc*> gHWNDMap;
static WindowsDesc                                 gWindow;
static float                                       gRetinaScale = 1.0f;

void adjustWindow(WindowsDesc* winDesc);

void getRecommendedResolution(RectDesc* rect) { *rect = { 0, 0, 1920, 1080 }; }

void requestShutdown()
{
	XEvent event = {};
	event.type = ClientMessage;
	event.xclient.format = 32;
	event.xclient.data.l[0] = gWindow.handle.xlib_wm_delete_window;
	if(XSendEvent(gWindow.handle.display, gWindow.handle.window, false, 0, &event) == 0)
		LOGF(LogLevel::eERROR, "Failed to sent shutdown event to window!");
}

void toggleFullscreen(WindowsDesc* window)
{
}

float2 getDpiScale() { return { gRetinaScale, gRetinaScale }; }
/************************************************************************/
// App Entrypoint
/************************************************************************/
#include "../Interfaces/IApp.h"
#include "../Interfaces/IFileSystem.h"

static IApp* pApp = NULL;

static void onResize(WindowsDesc* wnd, int32_t newSizeX, int32_t newSizeY)
{
	pApp->mSettings.mWidth = newSizeX;
	pApp->mSettings.mHeight = newSizeY;
	pApp->mSettings.mFullScreen = wnd->fullScreen;
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
	double      dpi = 96.0;

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
			XrmDestroyDatabase(db);
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
	winDesc->handle.display = XOpenDisplay(NULL);
	long        visualMask = VisualScreenMask;
	int         numberOfVisuals;
	XVisualInfo vInfoTemplate = {};
	vInfoTemplate.screen = DefaultScreen(winDesc->handle.display);
	XVisualInfo* visualInfo = XGetVisualInfo(winDesc->handle.display, visualMask, &vInfoTemplate, &numberOfVisuals);

	Colormap colormap =
		XCreateColormap(winDesc->handle.display, RootWindow(winDesc->handle.display, vInfoTemplate.screen), visualInfo->visual, AllocNone);

	XSetWindowAttributes windowAttributes = {};
	windowAttributes.colormap = colormap;
	windowAttributes.background_pixel = 0xFFFFFFFF;
	windowAttributes.border_pixel = 0;
	windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

	winDesc->handle.window = XCreateWindow(
		winDesc->handle.display, RootWindow(winDesc->handle.display, vInfoTemplate.screen), winDesc->windowedRect.left, winDesc->windowedRect.top,
		winDesc->windowedRect.right - winDesc->windowedRect.left, winDesc->windowedRect.bottom - winDesc->windowedRect.top, 0,
		visualInfo->depth, InputOutput, visualInfo->visual, CWBackPixel | CWBorderPixel | CWEventMask | CWColormap, &windowAttributes);

	//Added
	//set window title name
	XStoreName(winDesc->handle.display, winDesc->handle.window, app_name);

	char windowName[200];
	sprintf(windowName, "%s", app_name);

	//set hint name for window
	XClassHint hint;
	hint.res_class = windowName;    //class name
	hint.res_name = windowName;     //application name
	XSetClassHint(winDesc->handle.display, winDesc->handle.window, &hint);

	XSelectInput(
		winDesc->handle.display, winDesc->handle.window,
		ExposureMask | KeyPressMask |    //Key press
			KeyReleaseMask |             //Key release
			ButtonPressMask |            //Mouse click
			ButtonReleaseMask |          //Mouse release
			StructureNotifyMask |        //Resize
			PointerMotionMask            //Mouse movement
	);
	XMapWindow(winDesc->handle.display, winDesc->handle.window);
	XFlush(winDesc->handle.display);
	winDesc->handle.xlib_wm_delete_window = XInternAtom(winDesc->handle.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(winDesc->handle.display, winDesc->handle.window, &winDesc->handle.xlib_wm_delete_window, 1);
	
	// Restrict window min size
	XSizeHints* size_hints = XAllocSizeHints();
	size_hints->flags = PMinSize;
    size_hints->min_width = 128;
    size_hints->min_height = 128;
    XSetWMNormalHints(winDesc->handle.display, winDesc->handle.window, size_hints);
    XFree(size_hints);

	double baseDpi = 96.0;
	gRetinaScale = (float)(PlatformGetMonitorDPI(winDesc->handle.display) / baseDpi);
}

bool handleMessages(WindowsDesc* winDesc)
{
	bool quit = false;

	XEvent event;
	while (XPending(winDesc->handle.display) > 0)
	{
		XNextEvent(winDesc->handle.display, &event);
		
		if (winDesc->callbacks.onHandleMessage)
            winDesc->callbacks.onHandleMessage(winDesc, &event);
			
		switch (event.type)
		{
			case ClientMessage:
				if ((Atom)event.xclient.data.l[0] == winDesc->handle.xlib_wm_delete_window)
					quit = true;
				break;
			case DestroyNotify:
			{
				LOGF(LogLevel::eINFO, "Destroying the window");
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

					if (gWindow.callbacks.onResize)
						gWindow.callbacks.onResize(&gWindow, getRectWidth(rect), getRectHeight(rect));
				}
				break;
			}
			default: break;
		}
	}

	XFlush(winDesc->handle.display);

	return quit;
}

int LinuxMain(int argc, char** argv, IApp* app)
{
	extern bool MemAllocInit();
	extern void MemAllocExit();

	if (!MemAllocInit())
		return EXIT_FAILURE;

	if (!fsInitAPI())
		return EXIT_FAILURE;

	Log::Init();
	
	pApp = app;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t       testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;
	Timer           deltaTimer;

    RectDesc rect = {};
    getRecommendedResolution(&rect);

    //TODO find a way to set linux fullscreen mode
    gWindow.fullscreenRect = rect;

	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}

	gWindow.callbacks.onResize = onResize;

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

	bool quit = false;

	while (!quit)
	{
		float deltaTime = deltaTimer.GetMSec(true) / 1000.0f;
		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

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

	pApp->Unload();
	pApp->Exit();
	
	Log::Exit();
	fsDeinitAPI();
	MemAllocExit();

	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
