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
#include <X11/extensions/Xrandr.h>

#include "../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../ThirdParty/OpenSource/rmem/inc/rmem.h"

#include "../Interfaces/IOperatingSystem.h"
#include "../Interfaces/ILog.h"
#include "../Interfaces/ITime.h"
#include "../Interfaces/IThread.h"
#include "../Interfaces/IFileSystem.h"
#include "../Interfaces/IMemory.h"

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

struct MWMHints 
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
};

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)

#define MWM_FUNC_ALL        (1L << 0)
#define MWM_FUNC_RESIZE     (1L << 1)
#define MWM_FUNC_MOVE       (1L << 2)
#define MWM_FUNC_MINIMIZE   (1L << 3)
#define MWM_FUNC_MAXIMIZE   (1L << 4)
#define MWM_FUNC_CLOSE      (1L << 5)

struct DeviceMode
{
    Screen* screen;
    RRCrtc  crtc;
    RRMode  mode;
};

static Display*     gDefaultDisplay;
static WindowsDesc  gWindow;
static MonitorDesc* gMonitors;
static uint32_t     gMonitorCount;
static float        gRetinaScale = 1.0f;
static bool         gQuit;
static bool         gHasXRandr;
static int          gXRandrMajor;
static int          gXRandrMinor;
static DeviceMode*  gDirtyModes;
static uint32_t     gDirtyModeCount;
static bool 		gCursorInsideRectangle = false;
static Cursor      	gInvisibleCursor = {};
static Cursor      	gCursor = {};


void adjustWindow(WindowsDesc* winDesc);

void getRecommendedResolution(RectDesc* rect) 
{ 
    Screen* screen = XDefaultScreenOfDisplay(gDefaultDisplay);

	*rect = { 0, 0, min(1920, (int)(WidthOfScreen(screen)*0.75)), min(1080, (int)(HeightOfScreen(screen)*0.75)) };
}

void requestShutdown()
{
    gQuit = true;
}

void toggleFullscreen(WindowsDesc* window)
{
    Atom wmState = XInternAtom(window->handle.display, "_NET_WM_STATE", False);
    Atom wmStateFullscreen = XInternAtom(window->handle.display, "_NET_WM_STATE_FULLSCREEN", False);

    ASSERT(wmState);
    ASSERT(wmStateFullscreen);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = window->handle.window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = window->fullScreen ? _NET_WM_STATE_REMOVE : _NET_WM_STATE_ADD;
    xev.xclient.data.l[1] = wmStateFullscreen;
    xev.xclient.data.l[2] = 0;

    XSendEvent(window->handle.display, DefaultRootWindow(window->handle.display), False, SubstructureNotifyMask, &xev);

    window->fullScreen = !window->fullScreen;
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

static void collectXRandrInfo()
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

    if (checkXRandrVersion(1, 3))
    {
        RROutput primOutput = XRRGetOutputPrimary(display, rootWindow);

        if (primOutput != None)
            output = primOutput;
    }

    return output;
}

static void collectMonitorInfo()
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

        gMonitors[i].screen = screen;
        gMonitors[i].physicalSize = { (uint32_t)physWidth, (uint32_t)physHeight };
        gMonitors[i].monitorRect = { 0, 0, width, height };
        gMonitors[i].workRect = { 0, 0, width, height };

        float vdpi = width * 25.4 / physWidth;
        float hdpi = height * 25.4 / physHeight;

        gMonitors[i].dpi.x = (int)(vdpi + 0.5);
        gMonitors[i].dpi.y = (int)(hdpi + 0.5);

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

            int numScreenSizes;
            XRRScreenSize* screenSizes = XRRConfigSizes(screenConfig, &numScreenSizes);
            ASSERT(screenSizes && numScreenSizes > 0);

            gMonitors[i].resolutionCount = numScreenSizes;
            gMonitors[i].resolutions = (Resolution*)tf_calloc(numScreenSizes, sizeof(Resolution));

            int tail = 0;
            for (int j = 0; j < numScreenSizes; ++j)
            {
                ASSERT(screenSizes[j].width >= 0);
                ASSERT(screenSizes[j].height >= 0);

                gMonitors[i].resolutions[tail++] = { (uint32_t)screenSizes[j].width, (uint32_t)screenSizes[j].height };

                if (flipped)
                    eastl::swap(gMonitors[i].resolutions[j].mWidth, gMonitors[i].resolutions[j].mHeight);

                for (int k = 0; k < j; ++k)
                {
                    if (gMonitors[i].resolutions[k].mWidth == gMonitors[i].resolutions[j].mWidth &&
                            gMonitors[i].resolutions[k].mHeight == gMonitors[i].resolutions[j].mHeight)
                    {
                        tail--;
                        gMonitors[i].resolutionCount--;
                        break;
                    }
                }
            }

            XRRFreeScreenConfigInfo(screenConfig);
        }
        else
        {
            gMonitors[i].resolutionCount = 1;
            gMonitors[i].resolutions = (Resolution*)tf_calloc(1, sizeof(Resolution));
            gMonitors[i].resolutions[0] = { (uint32_t)width, (uint32_t)height };
        }

        for (uint32_t j = 0; j < gMonitors[i].resolutionCount; ++i)
        {
            Resolution res = gMonitors[i].resolutions[j];

            if (res.mWidth == width && res.mHeight == height)
            {
                gMonitors[i].defaultResolution = res;
                break;
            }
        }
    }
}

static void destroyMonitorInfo()
{
    for (uint32_t i = 0; i < gMonitorCount; ++i)
        tf_free(gMonitors[i].resolutions);

    tf_free(gMonitors);
    tf_free(gDirtyModes);
}

static void restoreResolutions()
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

        XRRSetCrtcConfig(gDefaultDisplay, screenRes, devMode->crtc, CurrentTime, 
                crtcInfo->x, crtcInfo->y, devMode->mode, crtcInfo->rotation, &output, 1);

        XRRFreeCrtcInfo(crtcInfo);
        XRRFreeScreenResources(screenRes);
    }
}

static void hideWindowInTaskbar(WindowsDesc* winDesc)
{
    Atom wmState = XInternAtom(winDesc->handle.display, "_NET_WM_STATE", False);
    Atom hidden = XInternAtom(winDesc->handle.display, "_NET_WM_STATE_HIDDEN", False);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = winDesc->handle.window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
    xev.xclient.data.l[1] = hidden;

    XSendEvent(winDesc->handle.display, DefaultRootWindow(winDesc->handle.display), False, SubstructureNotifyMask, &xev);
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
	XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

    winDesc->handle.display = gDefaultDisplay;

	long        visualMask = VisualScreenMask;
	int         numberOfVisuals;
	XVisualInfo vInfoTemplate = {};
	vInfoTemplate.screen = DefaultScreen(winDesc->handle.display);
	XVisualInfo* visualInfo = XGetVisualInfo(winDesc->handle.display, visualMask, &vInfoTemplate, &numberOfVisuals);

	winDesc->handle.colormap =
		XCreateColormap(winDesc->handle.display, RootWindow(winDesc->handle.display, vInfoTemplate.screen), visualInfo->visual, AllocNone);
    ASSERT(winDesc->handle.colormap);

	XSetWindowAttributes windowAttributes = {};
	windowAttributes.colormap = winDesc->handle.colormap;
	windowAttributes.background_pixel = 0xFFFFFFFF;
	windowAttributes.border_pixel = 0;
	windowAttributes.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask;

	winDesc->handle.window = XCreateWindow(
		winDesc->handle.display, RootWindow(winDesc->handle.display, vInfoTemplate.screen), winDesc->windowedRect.left, winDesc->windowedRect.top,
		winDesc->windowedRect.right - winDesc->windowedRect.left, winDesc->windowedRect.bottom - winDesc->windowedRect.top, 0,
		visualInfo->depth, InputOutput, visualInfo->visual, CWBackPixel | CWBorderPixel | CWEventMask | CWColormap, &windowAttributes);
    ASSERT(winDesc->handle.window);

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
			PointerMotionMask   |        //Mouse movement
			LeaveWindowMask		|		 //Mouse leave window
			EnterWindowMask				 //Mouse enter window
	);
	XMapWindow(winDesc->handle.display, winDesc->handle.window);
	XFlush(winDesc->handle.display);
	if(winDesc->centered)
	{
		centerWindow(winDesc);
	}
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
	
	// Create invisible cursor that will be used when mouse is hidden
	Pixmap      bitmapEmpty = {};
	XColor      emptyColor = {};
	static char emptyData[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	emptyColor.red = emptyColor.green = emptyColor.blue = 0;
	bitmapEmpty = XCreateBitmapFromData(winDesc->handle.display, winDesc->handle.window, emptyData, 8, 8);
	gInvisibleCursor = XCreatePixmapCursor(winDesc->handle.display, bitmapEmpty, bitmapEmpty, &emptyColor, &emptyColor, 0, 0);
	
	gCursor = XCreateFontCursor(winDesc->handle.display, 68);
	setCursor(&gCursor);
}

void setWindowRect(WindowsDesc* winDesc, const RectDesc& rect)
{
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
	currentRect = rect;

    XResizeWindow(winDesc->handle.display, winDesc->handle.window, rect.right - rect.left, rect.bottom - rect.top);
    XMoveWindow(winDesc->handle.display, winDesc->handle.window, rect.left, rect.top);
    XFlush(winDesc->handle.display);
}

void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	RectDesc& currentRect = winDesc->fullScreen ? winDesc->fullscreenRect : winDesc->windowedRect;
    currentRect.right = currentRect.left + width;
    currentRect.bottom = currentRect.top + height; 

    XResizeWindow(winDesc->handle.display, winDesc->handle.window, width, height);
    XFlush(winDesc->handle.display);
}

void toggleBorderless(WindowsDesc* winDesc, unsigned width, unsigned height)
{
	if (winDesc->fullScreen)
        return;
   
	if (winDesc->borderlessWindow)
	{
        Atom mwmAtom = XInternAtom(winDesc->handle.display, "_MOTIF_WM_HINTS", 0);

        MWMHints hints;
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.decorations = MWM_FUNC_ALL;

        XChangeProperty(winDesc->handle.display, winDesc->handle.window, 
                mwmAtom, mwmAtom, 32, PropModeReplace, (unsigned char*)&hints, 5);       
    }
    else
    {
        Atom mwmAtom = XInternAtom(winDesc->handle.display, "_MOTIF_WM_HINTS", 0);

        MWMHints hints;
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.decorations = 0;

        XChangeProperty(winDesc->handle.display, winDesc->handle.window, 
                mwmAtom, mwmAtom, 32, PropModeReplace, (unsigned char*)&hints, 5);       

    }

    setWindowRect(winDesc, { 0, 0, (int32_t)width, (int32_t)height });
    winDesc->borderlessWindow = !winDesc->borderlessWindow;
}

void showWindow(WindowsDesc* winDesc)
{
    winDesc->hide = false;
	XMapWindow(winDesc->handle.display, winDesc->handle.window);
}

void hideWindow(WindowsDesc* winDesc)
{
    winDesc->hide = true;
    XUnmapWindow(winDesc->handle.display, winDesc->handle.window);
}

void maximizeWindow(WindowsDesc* winDesc)
{
    Atom wmState = XInternAtom(winDesc->handle.display, "_NET_WM_STATE", False);
    Atom maxHorz = XInternAtom(winDesc->handle.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom maxVert = XInternAtom(winDesc->handle.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = winDesc->handle.window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
    xev.xclient.data.l[1] = maxHorz;
    xev.xclient.data.l[2] = maxVert;

    XSendEvent(winDesc->handle.display, DefaultRootWindow(winDesc->handle.display), False, SubstructureNotifyMask, &xev);

    winDesc->maximized = true;
}

void minimizeWindow(WindowsDesc* winDesc)
{
    Atom wmState = XInternAtom(winDesc->handle.display, "_NET_WM_STATE", False);
    Atom maxHorz = XInternAtom(winDesc->handle.display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom maxVert = XInternAtom(winDesc->handle.display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    XEvent xev = {};
    xev.type = ClientMessage;
    xev.xclient.window = winDesc->handle.window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
    xev.xclient.data.l[1] = maxHorz;
    xev.xclient.data.l[2] = maxVert;

    XSendEvent(winDesc->handle.display, DefaultRootWindow(winDesc->handle.display), False, SubstructureNotifyMask, &xev);

    winDesc->maximized = false;
}

void centerWindow(WindowsDesc* winDesc)
{
	uint32_t fsHalfWidth = getRectWidth(winDesc->fullscreenRect) >> 1;
	uint32_t fsHalfHeight = getRectHeight(winDesc->fullscreenRect) >> 1;
	uint32_t windowHalfWidth = getRectWidth(winDesc->windowedRect) >> 1;
	uint32_t windowHalfHeight = getRectHeight(winDesc->windowedRect) >> 1;
	uint32_t X = fsHalfWidth - windowHalfWidth;
	uint32_t Y = fsHalfHeight - windowHalfHeight;
	
	XMoveWindow(winDesc->handle.display, winDesc->handle.window, X, Y);
	XFlush(winDesc->handle.display);
}

void* createCursor(const char* path)
{
	Pixmap      bitmap = {};
	unsigned int bitmap_width, bitmap_height;
	int hotspot_x, hotspot_y;
	XColor foregroundColor = {};
	foregroundColor.red = foregroundColor.green = foregroundColor.blue = 65535;
	XColor backgroundColor = {};
	backgroundColor.red = backgroundColor.green = backgroundColor.blue = 0;
	XReadBitmapFile(gWindow.handle.display, gWindow.handle.window, path, &bitmap_width, &bitmap_height, &bitmap, &hotspot_x, &hotspot_y);
	Cursor* cursor = (Cursor*)tf_malloc(sizeof(Cursor));
	*cursor = XCreatePixmapCursor(gWindow.handle.display, bitmap, bitmap, &backgroundColor, &foregroundColor, hotspot_x, hotspot_y);
	return cursor;
}

void setCursor(void* cursor)
{
	Cursor* linuxCursor = (Cursor*)cursor;
	XDefineCursor(gWindow.handle.display, gWindow.handle.window, *linuxCursor);
}

void showCursor()
{
	XUndefineCursor(gWindow.handle.display, gWindow.handle.window);
	XDefineCursor(gWindow.handle.display, gWindow.handle.window, gCursor);
}

void hideCursor()
{
	XDefineCursor(gWindow.handle.display, gWindow.handle.window, gInvisibleCursor);
}

bool isCursorInsideTrackingArea()
{
	return gCursorInsideRectangle;
}

void setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y)
{
    XWarpPointer(winDesc->handle.display, winDesc->handle.window, None, 0, 0, 0, 0, x, y);
    XFlush(winDesc->handle.display);
}

void setMousePositionAbsolute(int32_t x, int32_t y)
{
	// TODO
}

void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes)
{
    if (!requireXRandrVersion(1, 2))
        return;

    Window rootWindow = RootWindowOfScreen(pMonitor->screen);
    
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
            eastl::swap(screenRes->modes[i].width, screenRes->modes[i].height);

        if (screenRes->modes[i].width == pRes->mWidth &&
                screenRes->modes[i].height == pRes->mHeight)
        {
            mode = screenRes->modes[i].id;
            break;
        }
    }

    ASSERT(mode);

    uint32_t i = 0;
    for (; i < gDirtyModeCount; ++i)
    {
        if (pMonitor->screen == gDirtyModes[i].screen)
            break;
    }

    if (i == gDirtyModeCount)
    {
        DeviceMode* devMode = &gDirtyModes[gDirtyModeCount++];

        devMode->screen = pMonitor->screen;
        devMode->mode = crtcInfo->mode;
        devMode->crtc = outputInfo->crtc;
    }

    XRRSetCrtcConfig(gDefaultDisplay, screenRes, outputInfo->crtc, CurrentTime, 
            crtcInfo->x, crtcInfo->y, mode, crtcInfo->rotation, &output, 1);

    XRRFreeCrtcInfo(crtcInfo);
    XRRFreeOutputInfo(outputInfo);
    XRRFreeScreenResources(screenRes);
}

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes)
{
	for (uint32_t i = 0; i < pMonitor->resolutionCount; ++i)
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

static CustomMessageProcessor sCustomProc = nullptr;
void setCustomMessageProcessor(CustomMessageProcessor proc)
{
	sCustomProc = proc;
}

bool handleMessages(WindowsDesc* winDesc)
{
	bool quit = false;

	XEvent event;
	while (XPending(winDesc->handle.display) > 0)
	{
		XNextEvent(winDesc->handle.display, &event);
		
		if (sCustomProc != nullptr)
		{
			sCustomProc(winDesc, &event);
		}
			
		switch (event.type)
		{
			case ClientMessage:
				if ((Atom)event.xclient.data.l[0] == winDesc->handle.xlib_wm_delete_window)
                {
                    XFreeColormap(winDesc->handle.display, winDesc->handle.colormap);
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
				// Handle Resize event
                if (event.xconfigure.width != (int)pApp->mSettings.mWidth ||
                    event.xconfigure.height != (int)pApp->mSettings.mHeight)
				{
					RectDesc rect = { 0 };
					rect = { (int)event.xconfigure.x, (int)event.xconfigure.y, (int)event.xconfigure.width + (int)event.xconfigure.x,
							 (int)event.xconfigure.height + (int)event.xconfigure.y };
					winDesc->windowedRect = rect;

					onResize(winDesc, getRectWidth(rect), getRectHeight(rect));
				}
				break;
			}
			default: break;
		}
	}

	XFlush(winDesc->handle.display);

	return quit;
}

void closeWindow(const WindowsDesc* winDesc)
{
    // NOTE: WM_DELETE_WINDOW atom forbids XDestroyWindow

	XEvent event = {};
	event.type = ClientMessage;
    event.xclient.window = winDesc->handle.window;
    event.xclient.message_type = XInternAtom(winDesc->handle.display, "WM_PROTOCOLS", true);
	event.xclient.format = 32;
	event.xclient.data.l[0] = winDesc->handle.xlib_wm_delete_window;
	event.xclient.data.l[1] = CurrentTime;

	XSendEvent(winDesc->handle.display, winDesc->handle.window, False, NoEventMask, &event);
}

int LinuxMain(int argc, char** argv, IApp* app)
{
	extern bool MemAllocInit(const char*);
	extern void MemAllocExit();
	
	FileSystemInitDesc fsDesc = {};
	fsDesc.pAppName = app->GetName();
	
    if (!initFileSystem(&fsDesc))
        return EXIT_FAILURE;

    fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_LOG, "");

	if (!MemAllocInit(app->GetName()))
		return EXIT_FAILURE;
		
#if TF_USE_MTUNER
	rmemInit(0);
#endif

	Log::Init(app->GetName());
	
	pApp = app;

	//Used for automated testing, if enabled app will exit after 120 frames
#ifdef AUTOMATED_TESTING
	uint32_t       testingFrameCount = 0;
	const uint32_t testingDesiredFrameCount = 120;
#endif

	IApp::Settings* pSettings = &pApp->mSettings;
	Timer           deltaTimer;

	gDefaultDisplay = XOpenDisplay(NULL);
    ASSERT(gDefaultDisplay);

    collectXRandrInfo();
    collectMonitorInfo();

    RectDesc rect = {};
    getRecommendedResolution(&rect);

    gWindow.fullscreenRect = rect;

	if (pSettings->mWidth == -1 || pSettings->mHeight == -1)
	{
		pSettings->mWidth = getRectWidth(rect);
		pSettings->mHeight = getRectHeight(rect);
	}

	gWindow.windowedRect = { 0, 0, (int)pSettings->mWidth, (int)pSettings->mHeight };
	gWindow.fullScreen = pSettings->mFullScreen;
	openWindow(pApp->GetName(), &gWindow);

    // NOTE: Some window manaters set the window to maximized
    // if the window size matches the screen resolution.
    // This might result in move/resize requests to be ignored.
    minimizeWindow(&gWindow);

	pSettings->mWidth = gWindow.fullScreen ? getRectWidth(gWindow.fullscreenRect) : getRectWidth(gWindow.windowedRect);
	pSettings->mHeight = gWindow.fullScreen ? getRectHeight(gWindow.fullscreenRect) : getRectHeight(gWindow.windowedRect);
	pApp->pWindow = &gWindow;

	if (!pApp->Init())
		return EXIT_FAILURE;

	if (!pApp->Load())
		return EXIT_FAILURE;

	int64_t lastCounter = getUSec();
	while (!gQuit)
	{
		int64_t counter = getUSec();
		float deltaTime = (float)(counter - lastCounter) / (float)1e6;
		lastCounter = counter;

		// if framerate appears to drop below about 6, assume we're at a breakpoint and simulate 20fps.
		if (deltaTime > 0.15f)
			deltaTime = 0.05f;

		gQuit = handleMessages(&gWindow);

		pApp->Update(deltaTime);
		pApp->Draw();

#ifdef AUTOMATED_TESTING
		//used in automated tests only.
		testingFrameCount++;
		if (testingFrameCount >= testingDesiredFrameCount)
            gQuit = true;
#endif
	}

    restoreResolutions();

	pApp->Unload();

    closeWindow(&gWindow);
    destroyMonitorInfo();
    XCloseDisplay(gDefaultDisplay);

	pApp->Exit();
	
	Log::Exit();

#if TF_USE_MTUNER
	rmemUnload();
	rmemShutDown();
#endif

	MemAllocExit();

	exitFileSystem();
	
	return 0;
}
/************************************************************************/
/************************************************************************/
#endif
