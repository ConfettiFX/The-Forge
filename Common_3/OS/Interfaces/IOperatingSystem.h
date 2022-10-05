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

#pragma once

#include "../../Application/Config.h"

#include "../CPUConfig.h"

#if defined(_WINDOWS) || defined(XBOX)
#include <sys/stat.h>
#include <stdlib.h>
#include <windows.h>
#endif

#if defined(__APPLE__)
#if !defined(TARGET_IOS)
#import <Carbon/Carbon.h>
#else
#include <stdint.h>
typedef uint64_t uint64;
#endif
#elif defined(__ANDROID__)
#include <android_native_app_glue.h>
#include <android/log.h>
#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
#define VK_USE_PLATFORM_XLIB_KHR
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
// X11 defines primitive types which conflict with Forge libraries
#undef Bool
#endif
#elif defined(NX64)
#include "../../../Switch/Common_3/OS/NX/NXTypes.h"
#elif defined(ORBIS)
#define THREAD_STACK_SIZE_ORBIS (64u * TF_KB)
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// For time related functions such as getting localtime
#include <time.h>

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#define IMEMORY_FROM_HEADER
#include "../../Utilities/Interfaces/IMemory.h"

#if !defined(_WINDOWS) && !defined(XBOX)
#include <unistd.h>
#define stricmp(a, b) strcasecmp(a, b)
#if !defined(ORBIS) && !defined(PROSPERO)
#define vsprintf_s vsnprintf
#endif
#endif

#if defined(XBOX)
#define stricmp(a, b) _stricmp(a, b)
#endif

//------------------------------------------------------------------------
// WINDOW AND RESOLUTION
//------------------------------------------------------------------------

#define MAX_MONITOR_COUNT 32

typedef enum WindowMode
{
	WM_WINDOWED,
	WM_FULLSCREEN,
	WM_BORDERLESS
} WindowMode;

typedef struct RectDesc
{
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
} RectDesc;

typedef struct WindowHandle
{
	// TODO: Separate vulkan ext from choosing xlib vs xcb
#if defined(VK_USE_PLATFORM_XLIB_KHR)
	Display* display;
	Window   window;
	Atom     xlib_wm_delete_window;
	Colormap colormap;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	xcb_connection_t*        connection;
	xcb_window_t             window;
	xcb_screen_t*            screen;
	xcb_intern_atom_reply_t* atom_wm_delete_window;
#elif defined(__ANDROID__)
	ANativeWindow*           window;
	ANativeActivity*         activity;
	AConfiguration*			 configuration;
#else
	void* window;    //hWnd
#endif
} WindowHandle;

typedef struct WindowDesc
{

	WindowHandle handle;
	RectDesc     windowedRect;
	RectDesc     fullscreenRect;
	RectDesc     clientRect;
	uint32_t     windowsFlags;
	bool         fullScreen;
	bool         cursorCaptured;
	bool         iconified;
	bool         maximized;
	bool         minimized;
	bool         hide;
	bool         noresizeFrame;
	bool         borderlessWindow;
	bool         overrideDefaultPosition;
	bool         centered;
	bool         forceLowDPI;

	int32_t mWindowMode;

	int32_t pCurRes[MAX_MONITOR_COUNT];
	int32_t pLastRes[MAX_MONITOR_COUNT];

	int32_t mWndX;
	int32_t mWndY;
	int32_t mWndW;
	int32_t mWndH;

	bool    mCursorHidden;
	int32_t mCursorInsideWindow;
	bool    mCursorClipped;
	bool    mMinimizeRequested;

} WindowDesc;

typedef struct Resolution
{
	uint32_t mWidth;
	uint32_t mHeight;
} Resolution;

// Monitor data
//
typedef struct
{
	RectDesc     monitorRect;
	RectDesc     workRect;
	unsigned int dpi[2];
	unsigned int physicalSize[2];
	// This size matches the static size of DISPLAY_DEVICE.DeviceName
#if defined(_WINDOWS) || defined(XBOX)
	WCHAR adapterName[32];
	WCHAR displayName[32];
	WCHAR publicAdapterName[128];
	WCHAR publicDisplayName[128];
#elif defined(__APPLE__)
#if defined(TARGET_IOS)
#else
	CGDirectDisplayID displayID;
	char              publicAdapterName[64];
	char              publicDisplayName[64];
#endif
#elif defined(__linux__) && !defined(__ANDROID__)
	Screen*          screen;
	char             adapterName[32];
	char             displayName[32];
	char             publicAdapterName[64];
	char             publicDisplayName[64];
#else
	char  adapterName[32];
	char  displayName[32];
	char  publicAdapterName[64];
	char  publicDisplayName[64];
#endif
	// stb_ds array of Resolutions
	Resolution* resolutions;
	Resolution  defaultResolution;
	bool        modesPruned;
	bool        modeChanged;
} MonitorDesc;

inline int getRectWidth(const RectDesc* rect) { return rect->right - rect->left; }
inline int getRectHeight(const RectDesc* rect) { return rect->bottom - rect->top; }

// Window handling
FORGE_API void openWindow(const char* app_name, WindowDesc* winDesc);
FORGE_API void closeWindow(const WindowDesc* winDesc);
FORGE_API void setWindowRect(WindowDesc* winDesc, const RectDesc* rect);
FORGE_API void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height);
FORGE_API void toggleBorderless(WindowDesc* winDesc, unsigned width, unsigned height);
FORGE_API void toggleFullscreen(WindowDesc* winDesc);
FORGE_API void showWindow(WindowDesc* winDesc);
FORGE_API void hideWindow(WindowDesc* winDesc);
FORGE_API void maximizeWindow(WindowDesc* winDesc);
FORGE_API void minimizeWindow(WindowDesc* winDesc);
FORGE_API void centerWindow(WindowDesc* winDesc);
FORGE_API void captureCursor(WindowDesc* winDesc, bool bEnable);

// Mouse and cursor handling
FORGE_API void* createCursor(const char* path);
FORGE_API void  setCursor(void* cursor);
FORGE_API void  showCursor(void);
FORGE_API void  hideCursor(void);
FORGE_API bool  isCursorInsideTrackingArea(void);
FORGE_API void  setMousePositionRelative(const WindowDesc* winDesc, int32_t x, int32_t y);
FORGE_API void  setMousePositionAbsolute(int32_t x, int32_t y);

FORGE_API void getRecommendedResolution(RectDesc* rect);
// Sets video mode for specified display
FORGE_API void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes);

FORGE_API MonitorDesc* getMonitor(uint32_t index);
FORGE_API uint32_t     getMonitorCount(void);
// pArray pointer to array with at least 2 elements(x,y)
FORGE_API void getDpiScale(float array[2]);

FORGE_API bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes);

// Reset
#if defined(_WINDOWS) || defined(__ANDROID__)
typedef enum ResetType
{
	RESET_TYPE_NONE				= 0,
	RESET_TYPE_API_SWITCH,
#if defined(_WINDOWS)
	RESET_TYPE_DEVICE_LOST,
	RESET_TYPE_GPU_MODE_SWITCH,
#endif
	RESET_TYPE_COUNT,
} ResetType;
#if defined(_WINDOWS)
COMPILE_ASSERT(RESET_TYPE_COUNT == 4);
#else
COMPILE_ASSERT(RESET_TYPE_COUNT == 2);
#endif

typedef struct ResetDesc
{
	ResetType mType;
} ResetDesc;

FORGE_API void requestReset(const ResetDesc* pResetDesc);
#endif

// Reload
typedef enum ReloadType
{
	RELOAD_TYPE_RESIZE = 0x1,
	RELOAD_TYPE_SHADER = 0x2,
	RELOAD_TYPE_RENDERTARGET =0x4,
	RELOAD_TYPE_ALL	= UINT32_MAX,
	RELOAD_TYPE_COUNT = 3,
} ReloadType;
COMPILE_ASSERT(RELOAD_TYPE_COUNT == 3);

typedef struct ReloadDesc
{
	ReloadType mType;
} ReloadDesc;

//------------------------------------------------------------------------
// PLATFORM LAYER
//------------------------------------------------------------------------

FORGE_API void requestReload(const ReloadDesc* pReloadDesc);


// API functions
FORGE_API void requestShutdown(void);
FORGE_API void errorMessagePopup(const char* title, const char* msg, void* windowHandle);

// Custom processing of OS pipe messages
typedef int32_t (*CustomMessageProcessor)(WindowDesc* pWindow, void* msg);
FORGE_API void setCustomMessageProcessor(CustomMessageProcessor proc);

// Shell commands

/// @param stdOutFile The file to which the output of the command should be written. May be NULL.
FORGE_API int systemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile);

//
// failure research ...
//
