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

#pragma once

#if defined(_WINDOWS) || defined(XBOX)
#include <sys/stat.h>
#include <stdlib.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#undef min
#undef max
#endif

#if defined(__APPLE__)
#if defined(__aarch64__)
#define TARGET_APPLE_ARM64
#endif
#if !defined(TARGET_IOS)
// Exclude threads header, because we use POSIX threads
#define __THREADS__
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

#include "../Core/Compiler.h"
#define IMEMORY_FROM_HEADER
#include "../Interfaces/IMemory.h"

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

// #TODO: Fix - FORGE_DEBUG is a toggle (either it is defined or not defined) Setting it to zero or one
// so it can be used with #if is not the right approach
#ifndef FORGE_DEBUG
#if defined(DEBUG) || defined(_DEBUG) || defined(AUTOMATED_TESTING)
#define FORGE_DEBUG
#endif
#endif

#ifndef FORGE_STACKTRACE_DUMP
#ifdef AUTOMATED_TESTING
#if defined(NX64) || (defined(_WINDOWS) && defined(_M_X64)) || defined(ORBIS)
#define FORGE_STACKTRACE_DUMP
#endif
#endif
#endif

typedef struct WindowHandle
{
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

typedef struct RectDesc
{
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
} RectDesc;

inline int getRectWidth(const RectDesc* rect) { return rect->right - rect->left; }

inline int getRectHeight(const RectDesc* rect) { return rect->bottom - rect->top; }

typedef struct
{
	WindowHandle handle;
	RectDesc     windowedRect;
	RectDesc     fullscreenRect;
	RectDesc     clientRect;
	uint32_t     windowsFlags;
	bool         fullScreen;
	bool         cursorTracked;
	bool         iconified;
	bool         maximized;
	bool         minimized;
	bool         hide;
	bool         noresizeFrame;
	bool         borderlessWindow;
	bool         overrideDefaultPosition;
	bool         centered;
	bool         forceLowDPI;
} WindowsDesc;

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
	Resolution* resolutions;
	Resolution  defaultResolution;
	uint32_t    resolutionCount;
	bool        modesPruned;
	bool        modeChanged;
} MonitorDesc;

#if defined(_WINDOWS)
typedef enum ResetScenario
{

	RESET_SCENARIO_NONE = 0x0,
	RESET_SCENARIO_RELOAD = 0x1,
	RESET_SCENARIO_DEVICE_LOST = 0x2,
	RESET_SCENARIO_API_SWITCH = 0x4,

} ResetScenario;

void onRequestReload();
void onDeviceLost();
#elif defined(__ANDROID__)
typedef enum ResetScenario
{

	RESET_SCENARIO_NONE = 0x0,
	RESET_SCENARIO_RELOAD = 0x1,
	RESET_SCENARIO_API_SWITCH = 0x2,

} ResetScenario;

void onRequestReload();
void onDeviceLost();
void onAPISwitch();
#else
typedef enum ResetScenario
{

	RESET_SCENARIO_NONE = 0x0,
	RESET_SCENARIO_RELOAD = 0x1,

} ResetScenario;

void onRequestReload(void);
void onDeviceLost(void);
void onAPISwitch(void);
#endif

// API functions
void requestShutdown(void);
void errorMessagePopup(const char* title, const char* msg, void* windowHandle);

// Custom processing of OS pipe messages
typedef int32_t (*CustomMessageProcessor)(WindowsDesc* pWindow, void* msg);
void setCustomMessageProcessor(CustomMessageProcessor proc);

// Window handling
void openWindow(const char* app_name, WindowsDesc* winDesc);
void closeWindow(const WindowsDesc* winDesc);
void setWindowRect(WindowsDesc* winDesc, const RectDesc* rect);
void setWindowSize(WindowsDesc* winDesc, unsigned width, unsigned height);
void toggleBorderless(WindowsDesc* winDesc, unsigned width, unsigned height);
void toggleFullscreen(WindowsDesc* winDesc);
void showWindow(WindowsDesc* winDesc);
void hideWindow(WindowsDesc* winDesc);
void maximizeWindow(WindowsDesc* winDesc);
void minimizeWindow(WindowsDesc* winDesc);
void centerWindow(WindowsDesc* winDesc);

// Mouse and cursor handling
void* createCursor(const char* path);
void  setCursor(void* cursor);
void  showCursor(void);
void  hideCursor(void);
bool  isCursorInsideTrackingArea(void);
void  setMousePositionRelative(const WindowsDesc* winDesc, int32_t x, int32_t y);
void  setMousePositionAbsolute(int32_t x, int32_t y);

void getRecommendedResolution(RectDesc* rect);
// Sets video mode for specified display
void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes);

MonitorDesc* getMonitor(uint32_t index);
uint32_t     getMonitorCount(void);
// pArray pointer to array with at least 2 elements(x,y)
void getDpiScale(float array[2]);

bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes);

// Shell commands

/// @param stdOutFile The file to which the output of the command should be written. May be NULL.
int systemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile);

//
// failure research ...
//
