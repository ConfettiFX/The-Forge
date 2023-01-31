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

#include <ctime>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrandr.h>

#include "../../Utilities/ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../../Utilities/Math/MathTypes.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/ITime.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Application/Interfaces/IProfiler.h"

#include "../../Game/Interfaces/IScripting.h"
#include "../../Application/Interfaces/IFont.h"
#include "../../Application/Interfaces/IUI.h"

#include "../../Graphics/Interfaces/IGraphics.h"

#include "../../Utilities/Interfaces/IMemory.h"

//------------------------------------------------------------------------
// STATIC STRUCTS
//------------------------------------------------------------------------

struct DeviceMode
{
	Screen* screen;
	RRCrtc  crtc;
	RRMode  mode;
};

struct MWMHints
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long          inputMode;
	unsigned long status;
};

#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)

#define MWM_FUNC_ALL (1L << 0)
#define MWM_FUNC_RESIZE (1L << 1)
#define MWM_FUNC_MOVE (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE (1L << 5)

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

//------------------------------------------------------------------------
// GLOBAL DATA
//------------------------------------------------------------------------

IApp* pWindowAppRef = NULL;

WindowDesc gWindow;
Display*      gDefaultDisplay;
float         gRetinaScale = 1.0f;
Cursor        gInvisibleCursor = {};
Cursor        gCursor = {};
bool          gCursorInsideRectangle = false;
bool          gHasXRandr;
int           gXRandrMajor;
int           gXRandrMinor;
DeviceMode*   gDirtyModes;
uint32_t      gDirtyModeCount;
MonitorDesc*  gMonitors;
uint32_t      gMonitorCount;

Atom wmState;
Atom wmStateHidden;
Atom wmStateFocused;

// LinuxMain.cpp
extern CustomMessageProcessor sCustomProc;

//------------------------------------------------------------------------
// Display server helpers
//------------------------------------------------------------------------
OSDisplayServer OS_DEFAULT_DISPLAYSERVER = OS_DISPLAYSERVER_XLIB;

int osDisplayServers() {
	return OS_DISPLAYSERVER_XLIB | OS_DISPLAYSERVER_WAYLAND;
}

int osDisplayServersCompiledIn() {
	int compiledIn;
#if defined(FORGE_ENABLE_DISPLAYSERVER_WAYLAND)
	compiledIn |= OS_DISPLAYSERVER_WAYLAND;
#endif
#if defined(FORGE_ENABLE_DISPLAYSERVER_XLIB)
	compiledIn |= OS_DISPLAYSERVER_XLIB;
#endif
#if defined(FORGE_ENABLE_DISPLAYSERVER_XCB)
	compiledIn |= OS_DISPLAYSERVER_XCB;
#endif
	return compiledIn;
}


//------------------------------------------------------------------------
// STATIC HELPER FUNCTIONS
//------------------------------------------------------------------------

static void UpdateWindowDescFullScreenRect(WindowDesc* winDesc)
{
	Screen* screen = XDefaultScreenOfDisplay(gDefaultDisplay);
	winDesc->fullscreenRect = { 0, 0, screen->width, screen->height };
}

// https://github.com/glfw/glfw/issues/1019
static double PlatformGetMonitorDPI(Display* display)
{
	char*       resourceString = XResourceManagerString(display);
	XrmDatabase db;
	XrmValue    value;
	char*       type = NULL;
	double      dpi = 96.0;

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

void collectXRandrInfo()
{
	int version;
	gHasXRandr = XQueryExtension(gDefaultDisplay, "RANDR", &version, &version, &version) &&
				 XRRQueryVersion(gDefaultDisplay, &gXRandrMajor, &gXRandrMinor);

	if (!gHasXRandr)
		LOGF(LogLevel::eWARNING, "XRANDR not supported on current platform");
}

static bool checkXRandrVersion(int major, int minor)
{ 
    return gXRandrMajor < major || (gXRandrMajor == major && gXRandrMinor < minor);
}

static bool requireXRandr()
{
	if (!gHasXRandr)
	{
		LOGF(LogLevel::eERROR, "XRANDR not found");
		return false;
	}

	return true;
}

static bool requireXRandrVersion(int major, int minor)
{
	if (!requireXRandr())
		return false;

	if (checkXRandrVersion(major, minor))
	{
		LOGF(LogLevel::eERROR, "XRANDR %i.%i required", major, minor);
		return false;
	}

	return true;
}

static RROutput getPrimaryOutput(Display* display, Window rootWindow, XRRScreenResources* screenRes)
{
	if (!requireXRandr())
		return None;

	RROutput output = screenRes->outputs[0];

	if (!checkXRandrVersion(1, 3))
	{
		RROutput primOutput = XRRGetOutputPrimary(display, rootWindow);

		if (primOutput != None)
			output = primOutput;
	}

	return output;
}

void collectMonitorInfo()
{
	gMonitorCount = XScreenCount(gDefaultDisplay);
	if (gMonitorCount == 0)
		return;

	gMonitors = (MonitorDesc*)tf_calloc(gMonitorCount, sizeof(MonitorDesc));

	if (gHasXRandr)
		gDirtyModes = (DeviceMode*)tf_calloc(gMonitorCount, sizeof(DeviceMode));

	for (uint32_t i = 0; i < gMonitorCount; ++i)
	{
		Screen* screen = XScreenOfDisplay(gDefaultDisplay, i);

		int width = WidthOfScreen(screen);
		int height = HeightOfScreen(screen);

		int physWidth = XWidthMMOfScreen(screen);
		int physHeight = XHeightMMOfScreen(screen);

		ASSERT(width >= 0 && height >= 0);
		ASSERT(physWidth >= 0 && physHeight >= 0);

		gMonitors[i].xlib_screen = screen;
		gMonitors[i].physicalSize[0] = (uint32_t)physWidth;
		gMonitors[i].physicalSize[1] = (uint32_t)physHeight;
		gMonitors[i].monitorRect = { 0, 0, width, height };
		gMonitors[i].workRect = { 0, 0, width, height };

		float vdpi = width * 25.4 / physWidth;
		float hdpi = height * 25.4 / physHeight;

		gMonitors[i].dpi[0] = (int)(vdpi + 0.5);
		gMonitors[i].dpi[1] = (int)(hdpi + 0.5);

		// TODO: Is it possible to get the names in X11?
		sprintf(gMonitors[i].adapterName, "Screen %u", (unsigned)i);
		sprintf(gMonitors[i].displayName, "Screen %u", (unsigned)i);
		sprintf(gMonitors[i].publicAdapterName, "Screen %u", (unsigned)i);
		sprintf(gMonitors[i].publicDisplayName, "Screen %u", (unsigned)i);

		if (gHasXRandr)
		{
			XRRScreenConfiguration* screenConfig = XRRGetScreenInfo(gDefaultDisplay, RootWindowOfScreen(screen));

			Rotation rotation;
			XRRConfigRotations(screenConfig, &rotation);

			bool flipped = rotation == RR_Rotate_90 || rotation == RR_Rotate_270;

			int            numScreenSizes;
			XRRScreenSize* screenSizes = XRRConfigSizes(screenConfig, &numScreenSizes);
			ASSERT(screenSizes && numScreenSizes > 0);

			gMonitors[i].resolutions = NULL;
			arrsetcap(gMonitors[i].resolutions, numScreenSizes);

			for (int j = 0; j < numScreenSizes; ++j)
			{
				ASSERT(screenSizes[j].width >= 0);
				ASSERT(screenSizes[j].height >= 0);

				Resolution resolution = { (uint32_t)screenSizes[j].width, (uint32_t)screenSizes[j].height };

				if (flipped)
				{
					uint32_t temp = resolution.mHeight;
					resolution.mHeight = resolution.mWidth;
					resolution.mWidth = temp;
				}
				
				bool exists = false;
				for (ptrdiff_t k = 0; k < arrlen(gMonitors[i].resolutions); ++k)
				{
					if (gMonitors[i].resolutions[k].mWidth == resolution.mWidth &&
						gMonitors[i].resolutions[k].mHeight == resolution.mHeight)
					{
						exists = true;
						break;
					}
				}
				
				if (!exists)
					arrpush(gMonitors[i].resolutions, resolution);
			}

			XRRFreeScreenConfigInfo(screenConfig);
		}
		else
		{
			gMonitors[i].resolutions = NULL;
			arrsetlen(gMonitors[i].resolutions, 1);
			gMonitors[i].resolutions[0] = { (uint32_t)width, (uint32_t)height };
		}

		for (ptrdiff_t j = 0; j < arrlen(gMonitors[i].resolutions); ++j)
		{
			Resolution res = gMonitors[i].resolutions[j];

			if (res.mWidth == (uint32_t)width && res.mHeight == (uint32_t)height)
			{
				gMonitors[i].defaultResolution = res;
				break;
			}
		}
	}
}

void destroyMonitorInfo()
{
	for (uint32_t i = 0; i < gMonitorCount; ++i)
		arrfree(gMonitors[i].resolutions);

	tf_free(gMonitors);
	tf_free(gDirtyModes);
}

void restoreResolutions()
{
	if (gDirtyModeCount > 0 && !requireXRandrVersion(1, 2))
		return;

	for (uint32_t i = 0; i < gDirtyModeCount; ++i)
	{
		DeviceMode* devMode = &gDirtyModes[i];

		Window rootWindow = RootWindowOfScreen(devMode->screen);

		XRRScreenResources* screenRes = XRRGetScreenResources(gDefaultDisplay, rootWindow);
		ASSERT(screenRes);

		XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(gDefaultDisplay, screenRes, devMode->crtc);
		ASSERT(crtcInfo);

		RROutput output = getPrimaryOutput(gDefaultDisplay, rootWindow, screenRes);
		ASSERT(output);

		XRRSetCrtcConfig(
			gDefaultDisplay, screenRes, devMode->crtc, CurrentTime, crtcInfo->x, crtcInfo->y, devMode->mode, crtcInfo->rotation, &output,
			1);

		XRRFreeCrtcInfo(crtcInfo);
		XRRFreeScreenResources(screenRes);
	}
}

static void onResize(WindowDesc* wnd, int32_t newSizeX, int32_t newSizeY)
{
	pWindowAppRef->mSettings.mWidth = newSizeX;
	pWindowAppRef->mSettings.mHeight = newSizeY;
	pWindowAppRef->mSettings.mFullScreen = wnd->fullScreen;

    ReloadDesc reloadDesc;
    reloadDesc.mType = RELOAD_TYPE_RESIZE;
    requestReload(&reloadDesc);
}

static void onFocusChanged(bool focused)
{
	if (pWindowAppRef == nullptr || !pWindowAppRef->mSettings.mInitialized)
	{
		return;
	}

	pWindowAppRef->mSettings.mFocused = focused;
}

void linuxUnmaximizeWindow(WindowDesc* winDesc)
{
    Atom wmState = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE", False);
	Atom maxHorz = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	Atom maxVert = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

	XEvent xev = {};
	xev.type = ClientMessage;
	xev.xclient.window = winDesc->handle.mXlib.window;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = maxHorz;
	xev.xclient.data.l[2] = maxVert;

	XSendEvent(winDesc->handle.mXlib.display, DefaultRootWindow(winDesc->handle.mXlib.display), False, SubstructureNotifyMask, &xev);
	
	winDesc->maximized = false;
}

//------------------------------------------------------------------------
// WINDOW HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void openWindow(const char* app_name, WindowDesc* winDesc)
{
	const char* display_envar = getenv("DISPLAY");
	if (display_envar == NULL || display_envar[0] == '\0')
	{
		printf("Environment variable DISPLAY requires a valid value.\nExiting ...\n");
		fflush(stdout);
		exit(1);
	}

	XInitThreads();
	XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

	winDesc->handle.mXlib.display = gDefaultDisplay;
	UpdateWindowDescFullScreenRect(winDesc);

	long        visualMask = VisualScreenMask;
	int         numberOfVisuals;
	XVisualInfo vInfoTemplate = {};
	vInfoTemplate.screen = DefaultScreen(winDesc->handle.mXlib.display);
	XVisualInfo* visualInfo = XGetVisualInfo(winDesc->handle.mXlib.display, visualMask, &vInfoTemplate, &numberOfVisuals);

	winDesc->handle.mXlib.colormap =
		XCreateColormap(winDesc->handle.mXlib.display, RootWindow(winDesc->handle.mXlib.display, vInfoTemplate.screen), visualInfo->visual, AllocNone);
	ASSERT(winDesc->handle.mXlib.colormap);

	XSetWindowAttributes windowAttributes = {};
	windowAttributes.colormap = winDesc->handle.mXlib.colormap;
	windowAttributes.background_pixel = 0xFFFFFFFF;
	windowAttributes.border_pixel = 0;
	windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask | FocusChangeMask | PropertyChangeMask;

	winDesc->handle.mXlib.window = XCreateWindow(
		winDesc->handle.mXlib.display, RootWindow(winDesc->handle.mXlib.display, vInfoTemplate.screen), winDesc->windowedRect.left,
		winDesc->windowedRect.top, winDesc->windowedRect.right - winDesc->windowedRect.left,
		winDesc->windowedRect.bottom - winDesc->windowedRect.top, 0, visualInfo->depth, InputOutput, visualInfo->visual,
		CWBackPixel | CWBorderPixel | CWEventMask | CWColormap, &windowAttributes);
	winDesc->clientRect = winDesc->windowedRect;
	ASSERT(winDesc->handle.mXlib.window);

	//Added
	//set window title name
	XStoreName(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, app_name);

	char windowName[200];
	sprintf(windowName, "%s", app_name);

	//set hint name for window
	XClassHint hint;
	hint.res_class = windowName;    //class name
	hint.res_name = windowName;     //application name
	XSetClassHint(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, &hint);

	XSelectInput(
		winDesc->handle.mXlib.display, winDesc->handle.mXlib.window,
		ExposureMask | KeyPressMask |    //Key press
			KeyReleaseMask |             //Key release
			ButtonPressMask |            //Mouse click
			ButtonReleaseMask |          //Mouse release
			StructureNotifyMask |        //Resize
			PointerMotionMask |          //Mouse movement
			LeaveWindowMask |            //Mouse leave window
			EnterWindowMask |            //Mouse enter window
			PropertyChangeMask           //Window Hide
	);
	XMapWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window);
	XFlush(winDesc->handle.mXlib.display);
	if (winDesc->centered)
	{
		centerWindow(winDesc);
	}
	winDesc->handle.mXlib.delete_window = XInternAtom(winDesc->handle.mXlib.display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, &winDesc->handle.mXlib.delete_window, 1);
	
	wmState = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE", False);
	wmStateHidden = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_HIDDEN", False);
	wmStateFocused = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_FOCUSED", False);
	
	Atom atoms[4] = {wmState, wmStateHidden, wmStateFocused, winDesc->handle.mXlib.delete_window};
	XSetWMProtocols(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, atoms, 4);

	// Restrict window min size
	XSizeHints* size_hints = XAllocSizeHints();
	size_hints->flags = PMinSize;
	size_hints->min_width = 128;
	size_hints->min_height = 128;
	XSetWMNormalHints(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, size_hints);
	XFree(size_hints);

	double baseDpi = 96.0;
	gRetinaScale = (float)(PlatformGetMonitorDPI(winDesc->handle.mXlib.display) / baseDpi);

	// Create invisible cursor that will be used when mouse is hidden
	Pixmap      bitmapEmpty = {};
	XColor      emptyColor = {};
	static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	emptyColor.red = emptyColor.green = emptyColor.blue = 0;
	bitmapEmpty = XCreateBitmapFromData(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, emptyData, 8, 8);
	gInvisibleCursor = XCreatePixmapCursor(winDesc->handle.mXlib.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);

	gCursor = XCreateFontCursor(winDesc->handle.mXlib.display, 68);
	setCursor(&gCursor);
}

void closeWindow(const WindowDesc* winDesc)
{
	// NOTE: WM_DELETE_WINDOW atom forbids XDestroyWindow

	XEvent event = {};
	event.type = ClientMessage;
	event.xclient.window = winDesc->handle.mXlib.window;
	event.xclient.message_type = XInternAtom(winDesc->handle.mXlib.display, "WM_PROTOCOLS", true);
	event.xclient.format = 32;
	event.xclient.data.l[0] = winDesc->handle.mXlib.delete_window;
	event.xclient.data.l[1] = CurrentTime;

	XSendEvent(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, False, NoEventMask, &event);
}

void setWindowRect(WindowDesc* winDesc, const RectDesc* pRect)
{
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect = *pRect;

	winDesc->clientRect = *pRect;

	XResizeWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, pRect->right - pRect->left, pRect->bottom - pRect->top);
	if (winDesc->centered)
	{
		centerWindow(winDesc);
	}
	else
	{
		XMoveWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, pRect->left, pRect->top);
		XFlush(winDesc->handle.mXlib.display);
	}
}

void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height)
{
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect.right = currentRect.left + width;
	currentRect.bottom = currentRect.top + height;

	XResizeWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, width, height);
	XFlush(winDesc->handle.mXlib.display);
}

void toggleBorderless(WindowDesc* winDesc, unsigned width, unsigned height)
{
	if (winDesc->borderlessWindow)
	{
		Atom mwmAtom = XInternAtom(winDesc->handle.mXlib.display, "_MOTIF_WM_HINTS", 0);

		MWMHints hints;
		hints.flags = MWM_HINTS_DECORATIONS;
		hints.decorations = MWM_FUNC_ALL;

		XChangeProperty(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, mwmAtom, mwmAtom, 32, PropModeReplace, (unsigned char*)&hints, 5);
	}
	else
	{
		Atom mwmAtom = XInternAtom(winDesc->handle.mXlib.display, "_MOTIF_WM_HINTS", 0);

		MWMHints hints;
		hints.flags = MWM_HINTS_DECORATIONS;
		hints.decorations = 0;

		XChangeProperty(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, mwmAtom, mwmAtom, 32, PropModeReplace, (unsigned char*)&hints, 5);
	}

	setWindowSize(winDesc, width, height);
	winDesc->borderlessWindow = !winDesc->borderlessWindow;
}

void toggleFullscreen(WindowDesc* winDesc)
{
	winDesc->fullScreen = !winDesc->fullScreen;

	// Going fullscreen.
	if (winDesc->fullScreen)
	{
		Screen* screen = XDefaultScreenOfDisplay(gDefaultDisplay);
		toggleBorderless(winDesc, screen->width, screen->height);
		XMoveWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, 0, 0);
		XFlush(winDesc->handle.mXlib.display);
	}
	else
	{
		toggleBorderless(winDesc, getRectWidth(&winDesc->windowedRect), getRectHeight(&winDesc->windowedRect));
		setWindowRect(winDesc, &winDesc->windowedRect);
	}
}

void showWindow(WindowDesc* winDesc)
{
	winDesc->hide = false;
	XMapWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window);
}

void hideWindow(WindowDesc* winDesc)
{
	winDesc->hide = true;
	XUnmapWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window);
}

void maximizeWindow(WindowDesc* winDesc)
{
	Atom wmState = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE", False);
	Atom maxHorz = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	Atom maxVert = XInternAtom(winDesc->handle.mXlib.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

	XEvent xev = {};
	xev.type = ClientMessage;
	xev.xclient.window = winDesc->handle.mXlib.window;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	xev.xclient.data.l[1] = maxHorz;
	xev.xclient.data.l[2] = maxVert;

	XSendEvent(winDesc->handle.mXlib.display, DefaultRootWindow(winDesc->handle.mXlib.display), False, SubstructureNotifyMask, &xev);

    winDesc->minimized = false; 
	winDesc->maximized = true;
}

void minimizeWindow(WindowDesc* winDesc)
{
	linuxUnmaximizeWindow(winDesc); 
    
    /* Get screen number */
    XWindowAttributes attr;
    XGetWindowAttributes(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, &attr);
    int screen = XScreenNumberOfScreen(attr.screen);

    /* Minimize it */
    XIconifyWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, screen);
    
    winDesc->minimized = true;
}

void centerWindow(WindowDesc* winDesc)
{
	uint32_t fsHalfWidth = getRectWidth(&winDesc->fullscreenRect) >> 1;
	uint32_t fsHalfHeight = getRectHeight(&winDesc->fullscreenRect) >> 1;
	uint32_t windowHalfWidth = getRectWidth(&winDesc->windowedRect) >> 1;
	uint32_t windowHalfHeight = getRectHeight(&winDesc->windowedRect) >> 1;
	uint32_t X = fsHalfWidth - windowHalfWidth;
	uint32_t Y = fsHalfHeight - windowHalfHeight;

	RectDesc newRect = { (int32_t)X, (int32_t)Y, (int32_t)(X + getRectWidth(&winDesc->windowedRect)),
						 (int32_t)(Y + getRectHeight(&winDesc->windowedRect)) };

	winDesc->windowedRect = newRect;
	winDesc->clientRect = newRect;

	XMoveWindow(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, X, Y);
	XFlush(winDesc->handle.mXlib.display);
}

//------------------------------------------------------------------------
// CURSOR AND MOUSE HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void* createCursor(const char* path)
{
	Pixmap       bitmap = {};
	unsigned int bitmap_width, bitmap_height;
	int          hotspot_x, hotspot_y;
	XColor       foregroundColor = {};
	foregroundColor.red = foregroundColor.green = foregroundColor.blue = 65535;
	XColor backgroundColor = {};
	backgroundColor.red = backgroundColor.green = backgroundColor.blue = 0;
	XReadBitmapFile(gWindow.handle.mXlib.display, gWindow.handle.mXlib.window, path, &bitmap_width, &bitmap_height, &bitmap, &hotspot_x, &hotspot_y);
	Cursor* cursor = (Cursor*)tf_malloc(sizeof(Cursor));
	*cursor = XCreatePixmapCursor(gWindow.handle.mXlib.display, bitmap, bitmap, &backgroundColor, &foregroundColor, hotspot_x, hotspot_y);
	return cursor;
}

void setCursor(void* cursor)
{
	Cursor* linuxCursor = (Cursor*)cursor;
	XDefineCursor(gWindow.handle.mXlib.display, gWindow.handle.mXlib.window, *linuxCursor);
}

void showCursor()
{
	XUndefineCursor(gWindow.handle.mXlib.display, gWindow.handle.mXlib.window);
	XDefineCursor(gWindow.handle.mXlib.display, gWindow.handle.mXlib.window, gCursor);
}

void hideCursor()
{
    XDefineCursor(gWindow.handle.mXlib.display, gWindow.handle.mXlib.window, gInvisibleCursor);
}


void captureCursor(WindowDesc* winDesc, bool bEnable)
{
	ASSERT(winDesc);

	if (winDesc->cursorCaptured != bEnable)
	{
		if(bEnable)
		{
			// Create invisible cursor that will be used when mouse is captured
			Cursor      invisibleCursor = {};
			Pixmap      bitmapEmpty = {};
			XColor      emptyColor = {};
			static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
			emptyColor.red = emptyColor.green = emptyColor.blue = 0;
			bitmapEmpty = XCreateBitmapFromData(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, emptyData, 8, 8);
			invisibleCursor = XCreatePixmapCursor(winDesc->handle.mXlib.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);
			// Capture mouse
			unsigned int masks = PointerMotionMask |    //Mouse movement
									ButtonPressMask |      //Mouse click
									ButtonReleaseMask;     // Mouse release
			int XRes = XGrabPointer(
				winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, 1 /*reports with respect to the grab window*/, masks, GrabModeAsync, GrabModeAsync, None,
				invisibleCursor, CurrentTime);
			ASSERT(XRes == GrabSuccess);
		}
		else
		{
			XUngrabPointer(winDesc->handle.mXlib.display, CurrentTime);
		}

		winDesc->cursorCaptured = bEnable;
	}
}

bool isCursorInsideTrackingArea()
{
    return gCursorInsideRectangle; 
}

void setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y)
{
	XWarpPointer(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window, None, 0, 0, 0, 0, x, y);
	XFlush(winDesc->handle.mXlib.display);
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
	// TODO
}

//------------------------------------------------------------------------
// MONITOR AND RESOLUTION HANDLING INTERFACE FUNCTIONS
//------------------------------------------------------------------------

void getRecommendedResolution(RectDesc* rect)
{
	Screen* screen = XDefaultScreenOfDisplay(gDefaultDisplay);

	*rect = { 0, 0, min(1920, (int)(WidthOfScreen(screen) * 0.75)), min(1080, (int)(HeightOfScreen(screen) * 0.75)) };
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	if (!requireXRandrVersion(1, 2))
		return;

	Window rootWindow = RootWindowOfScreen(pMonitor->xlib_screen);

	XRRScreenResources* screenRes = XRRGetScreenResources(gDefaultDisplay, rootWindow);
	ASSERT(screenRes);

	RROutput output = getPrimaryOutput(gDefaultDisplay, rootWindow, screenRes);
	ASSERT(output);

	XRROutputInfo* outputInfo = XRRGetOutputInfo(gDefaultDisplay, screenRes, output);
	ASSERT(output && outputInfo->connection != RR_Disconnected);

	XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(gDefaultDisplay, screenRes, outputInfo->crtc);
	ASSERT(crtcInfo);

	RRMode mode = None;

	for (int i = 0; i < screenRes->nmode; ++i)
	{
		if (crtcInfo->rotation == RR_Rotate_90 || crtcInfo->rotation == RR_Rotate_270)
		{
			uint32_t tmp = screenRes->modes[i].width;
			screenRes->modes[i].width = screenRes->modes[i].height;
			screenRes->modes[i].height = tmp;
		}

		if (screenRes->modes[i].width == pRes->mWidth && screenRes->modes[i].height == pRes->mHeight)
		{
			mode = screenRes->modes[i].id;
			break;
		}
	}

	ASSERT(mode);

	uint32_t i = 0;
	for (; i < gDirtyModeCount; ++i)
	{
		if (pMonitor->xlib_screen == gDirtyModes[i].screen)
			break;
	}

	if (i == gDirtyModeCount)
	{
		DeviceMode* devMode = &gDirtyModes[gDirtyModeCount++];

		devMode->screen = pMonitor->xlib_screen;
		devMode->mode = crtcInfo->mode;
		devMode->crtc = outputInfo->crtc;
	}

	XRRSetCrtcConfig(
		gDefaultDisplay, screenRes, outputInfo->crtc, CurrentTime, crtcInfo->x, crtcInfo->y, mode, crtcInfo->rotation, &output, 1);

	XRRFreeCrtcInfo(crtcInfo);
	XRRFreeOutputInfo(outputInfo);
	XRRFreeScreenResources(screenRes);
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	for (ptrdiff_t i = 0; i < arrlen(pMonitor->resolutions); ++i)
	{
		if (pMonitor->resolutions[i].mWidth == pRes->mWidth && pMonitor->resolutions[i].mHeight == pRes->mHeight)
			return true;
	}

	return false;
}
MonitorDesc* getMonitor(uint32_t index)
{
	ASSERT(index < gMonitorCount);
	return &gMonitors[index];
}

uint32_t getMonitorCount() 
{
    return gMonitorCount;
}

void getDpiScale(float array[2])
{
	array[0] = gRetinaScale;
	array[1] = gRetinaScale;
}

//------------------------------------------------------------------------
// LINUX PLATFORM WINDOW PROCEDURES
//------------------------------------------------------------------------

bool handleMessages(WindowDesc* winDesc)
{
	bool quit = false;

	XEvent event;
	while (XPending(winDesc->handle.mXlib.display) > 0)
	{
		XNextEvent(winDesc->handle.mXlib.display, &event);

		if (sCustomProc != nullptr)
		{
			sCustomProc(winDesc, &event);
		}

		switch (event.type)
		{
			case PropertyNotify:
				if (event.xclient.message_type == wmState)
				{
					Atom atomType;
					int format;
					unsigned long itemCount;
					unsigned long remainingBytes;
					Atom *atoms = NULL;

					XGetWindowProperty(winDesc->handle.mXlib.display, winDesc->handle.mXlib.window,
						event.xclient.message_type, 0, 1024, False, XA_ATOM,
						&atomType, &format, &itemCount, &remainingBytes, (unsigned char**)&atoms);

					for(unsigned long i = 0; i < itemCount; ++i)
					{
						/* _NET_WM_STATE_HIDDEN will be added when the window is minimized.
						   Because _NET_WM_STATE_HIDDEN is added when minimizing and not removed when unminimizing,
						   unminimize events are checked with _NET_WM_STATE_FOCUSED. */
						if(atoms[i]==wmStateHidden)
						{
							winDesc->minimized = true;
							onFocusChanged(false);
							break;
						}
						else if(atoms[i]==wmStateFocused)
						{
							if (winDesc->minimized == true)
							{
								winDesc->minimized = false;
							}
							onFocusChanged(true);
							break;
						}
					}

					XFree(atoms);
				}
				break;
			case ClientMessage:
				if ((Atom)event.xclient.data.l[0] == winDesc->handle.mXlib.delete_window)
				{
					XFreeColormap(winDesc->handle.mXlib.display, winDesc->handle.mXlib.colormap);
					quit = true;
				}
				break;
			case LeaveNotify:
			{
				gCursorInsideRectangle = false;
				break;
			}
			case EnterNotify:
			{
				gCursorInsideRectangle = true;
				break;
			}
			case DestroyNotify:
			{
				LOGF(LogLevel::eINFO, "Destroying the window");
				break;
			}
			case ConfigureNotify:
			{
				// Some window state has changed. Update the relevant attributes.
				RectDesc rect = { 0 };
				rect = { (int)event.xconfigure.x, (int)event.xconfigure.y, (int)event.xconfigure.width + (int)event.xconfigure.x,
						 (int)event.xconfigure.height + (int)event.xconfigure.y };

				if (!winDesc->fullScreen)
				{
					winDesc->clientRect = rect;
					winDesc->windowedRect = rect;
				}
				else
				{
					winDesc->fullscreenRect = rect;
				}

				// Handle Resize event
				if (event.xconfigure.width != (int)pWindowAppRef->mSettings.mWidth || event.xconfigure.height != (int)pWindowAppRef->mSettings.mHeight)
				{
					onResize(winDesc, getRectWidth(&rect), getRectHeight(&rect));
				}
				break;
			}
			case FocusIn: onFocusChanged(true); break;
			case FocusOut: onFocusChanged(false); break;
			default:
			{
				break;
			}
		}
	}

	XFlush(winDesc->handle.mXlib.display);

	return quit;
}

#endif
