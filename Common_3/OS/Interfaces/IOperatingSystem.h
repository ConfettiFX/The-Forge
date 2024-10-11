/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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
#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#if defined(__APPLE__)
#if !defined(TARGET_IOS)
#import <Carbon/Carbon.h>
#else
#include <stdint.h>
typedef uint64_t uint64;
#endif

// Support fixed set of OS versions in order to minimize maintenance efforts
// Two runtimes. Using IOS in the name as iOS versions are easier to remember
// Try to match the macOS version with the relevant features in iOS version
#define IOS14_API     API_AVAILABLE(macos(11.0), ios(14.1))
#define IOS14_RUNTIME @available(macOS 11.0, iOS 14.1, *)

#define IOS17_API     API_AVAILABLE(macos(14.0), ios(17.0))
#define IOS17_RUNTIME @available(macOS 14.0, iOS 17.0, *)

#elif defined(__ANDROID__)
#include <android/log.h>
#include <android_native_app_glue.h>

#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// For time related functions such as getting localtime
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "../../Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.h"

#if defined(FORGE_DEBUG) && (defined(_WINDOWS) || (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__))
#define WINDOW_DETAILS 1
#else
#define WINDOW_DETAILS 0
#endif

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

typedef enum WindowHandleType
{
    WINDOW_HANDLE_TYPE_UNKNOWN,
    WINDOW_HANDLE_TYPE_WIN32,
    WINDOW_HANDLE_TYPE_XLIB,
    WINDOW_HANDLE_TYPE_XCB,
    WINDOW_HANDLE_TYPE_WAYLAND,
    WINDOW_HANDLE_TYPE_ANDROID,
    WINDOW_HANDLE_TYPE_VI_NN,
} WindowHandleType;

typedef struct WindowHandle
{
    WindowHandleType type;

#if defined(_WIN32)
    HWND window;
#elif defined(ANDROID)
    ANativeWindow*   window;
    ANativeActivity* activity;
    AConfiguration*  configuration;
#elif defined(__APPLE__) || defined(NX64) || defined(ORBIS) || defined(PROSPERO)
    void*   window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t*        connection;
    xcb_window_t             window;
    xcb_screen_t*            screen;
    xcb_intern_atom_reply_t* atom_wm_delete_window;
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    union
    {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        struct
        {
            Display* display;
            Window   window;
            RectDesc windowDecorations;
            Atom     xlib_wm_delete_window;
            Colormap colormap;
        };
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        struct
        {
            struct wl_display* wl_display;
            struct wl_surface* wl_surface;
        };
#endif
    };
#endif
} WindowHandle;

typedef struct WindowDesc
{
    WindowHandle handle;
    // Note: all rectangles use top-left corner as origin
    RectDesc     windowedRect;   // Window rectangle with title bar and border when in windowed mode
    RectDesc     fullscreenRect; // Window rectangle when in fullscreen mode
    RectDesc     clientRect;     // Rectangle that can be rendered to inside a window. Excludes border and title bar.
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
    bool         forceLowDPI;

    int32_t mWindowMode;

    // Client area x,y positions, width and height
    int32_t mWndX;
    int32_t mWndY;
    int32_t mWndW;
    int32_t mWndH;

    bool    mCursorHidden;
    int32_t mCursorInsideWindow;
    bool    mCursorCaptured;
    bool    mMinimizeRequested;

#if WINDOW_DETAILS
    bstring pWindowedRectLabel;
    bstring pFullscreenRectLabel;
    bstring pClientRectLabel;
    bstring pWndLabel;

    bstring pFullscreenLabel;
    bstring pCursorCapturedLabel;
    bstring pIconifiedLabel;
    bstring pMaximizedLabel;
    bstring pMinimizedLabel;
    bstring pNoResizeFrameLabel;
    bstring pBorderlessWindowLabel;
    bstring pOverrideDefaultPositionLabel;
    bstring pForceLowDPILabel;
    bstring pWindowModeLabel;
#endif

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
    Screen* screen;
    char    adapterName[32];
    char    displayName[32];
    char    publicAdapterName[64];
    char    publicDisplayName[64];
#else
    char                     adapterName[32];
    char                     displayName[32];
    char                     publicAdapterName[64];
    char                     publicDisplayName[64];
#endif
    // stb_ds array of Resolutions
    Resolution* resolutions;
    Resolution  defaultResolution;
    uint32_t    currentResolution;
    bool        modesPruned;
    bool        modeChanged;
#if defined(PROSPERO)
    bool supportsHDR;
#endif
} MonitorDesc;

typedef enum ThermalStatus
{
    THERMAL_STATUS_MIN = -2,

    THERMAL_STATUS_NOT_SUPPORTED = -2, // Current platform/device/build doesn't support ThermalStatus queries
    THERMAL_STATUS_ERROR = -1,         // There was an error retrieving thermal status

    // Note: These enum names are taken from Android, we might want to rename them or create our own
    THERMAL_STATUS_NONE = 0,     // Means we didn't get any thermal status data yet (maybe device doesn't support it)
    THERMAL_STATUS_LIGHT = 1,    // iOS Nominal
    THERMAL_STATUS_MODERATE = 2, // iOS Fair
    THERMAL_STATUS_SEVERE = 3,   // iOS Serious
    THERMAL_STATUS_CRITICAL = 4, // iOS Critical
    THERMAL_STATUS_EMERGENCY = 5,
    THERMAL_STATUS_SHUTDOWN = 6,

    THERMAL_STATUS_MAX,
} ThermalStatus;

inline int getRectWidth(const RectDesc* rect) { return rect->right - rect->left; }
inline int getRectHeight(const RectDesc* rect) { return rect->bottom - rect->top; }

// Window handling
FORGE_API void openWindow(const char* app_name, WindowDesc* winDesc);
FORGE_API void closeWindow(WindowDesc* winDesc);
// Note: All window rectangle and window size functions are not meant to be used in fullscreen mode
// Sets window rectangle. The dimensions of rectangle include window border and title bar.
FORGE_API void setWindowRect(WindowDesc* winDesc, const RectDesc* rect);
// Sets window size. The size of the window includes window border and title bar
FORGE_API void setWindowSize(WindowDesc* winDesc, unsigned width, unsigned height);
// Sets window client rectangle. The dimensions of rectangle don't include window border or title bar.
FORGE_API void setWindowClientRect(WindowDesc* winDesc, const RectDesc* rect);
// Sets window client rectangle. The size of the client doesn't include window border or title bar.
FORGE_API void setWindowClientSize(WindowDesc* winDesc, unsigned width, unsigned height);

// Set Window mode
// Restores to previously saved client size
FORGE_API void setWindowed(WindowDesc* winDesc);
FORGE_API void setBorderless(WindowDesc* winDesc);
FORGE_API void setFullscreen(WindowDesc* winDesc);
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

// Returns recommended windowed client rectangle for active monitor
FORGE_API void getRecommendedResolution(RectDesc* rect);
FORGE_API void getRecommendedWindowRect(WindowDesc* winDesc, RectDesc* rect);
// Sets video mode for specified display
FORGE_API void setResolution(const MonitorDesc* pMonitor, const Resolution* pRes);

FORGE_API MonitorDesc* getMonitor(uint32_t index);
FORGE_API uint32_t     getMonitorCount(void);
FORGE_API uint32_t     getActiveMonitorIdx(void);
// pArray pointer to array with at least 2 elements(x,y)
FORGE_API void         getMonitorDpiScale(uint32_t monitorIndex, float dpiScale[2]);

FORGE_API bool getResolutionSupport(const MonitorDesc* pMonitor, const Resolution* pRes);

FORGE_API ThermalStatus getThermalStatus(void);

inline const char* getThermalStatusString(ThermalStatus thermalStatus)
{
    switch (thermalStatus)
    {
    case THERMAL_STATUS_NOT_SUPPORTED:
        return "NotSupported";
    case THERMAL_STATUS_ERROR:
        return "Error";
    case THERMAL_STATUS_NONE:
        return "None";
    case THERMAL_STATUS_LIGHT:
        return "Light";
    case THERMAL_STATUS_MODERATE:
        return "Moderate";
    case THERMAL_STATUS_SEVERE:
        return "Severe";
    case THERMAL_STATUS_CRITICAL:
        return "Critical";
    case THERMAL_STATUS_EMERGENCY:
        return "Emergency";
    case THERMAL_STATUS_SHUTDOWN:
        return "Shutdown";
    default:
        return "Invalid";
    }
}

// Reset
#if defined(_WINDOWS) || defined(__ANDROID__)
typedef enum ResetType
{
    RESET_TYPE_NONE = 0,
    RESET_TYPE_API_SWITCH = 0x1,
    RESET_TYPE_GRAPHIC_CARD_SWITCH = 0x2,
#if defined(_WINDOWS)
    RESET_TYPE_DEVICE_LOST = 0x4,
    RESET_TYPE_GPU_MODE_SWITCH = 0x8,
#endif
} ResetType;

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
    RELOAD_TYPE_RENDERTARGET = 0x4,
    RELOAD_TYPE_ALL = UINT32_MAX,
    RELOAD_TYPE_COUNT = 3,
} ReloadType;
COMPILE_ASSERT(RELOAD_TYPE_COUNT == 3);

typedef struct ReloadDesc
{
    ReloadType mType;
} ReloadDesc;

#if defined(ANDROID)
#define TF_MEMORY_STATE_API
typedef enum MemoryState
{
    MEMORY_STATE_UNKNOWN,
    MEMORY_STATE_OK,
    MEMORY_STATE_APPROACHING_LIMIT,
    MEMORY_STATE_CRITICAL,
} MemoryState;

inline const char* getMemoryStateString(MemoryState state)
{
    switch (state)
    {
    case MEMORY_STATE_UNKNOWN:
        return "Unknown";
    case MEMORY_STATE_OK:
        return "Okay";
    case MEMORY_STATE_APPROACHING_LIMIT:
        return "Approaching Limit";
    case MEMORY_STATE_CRITICAL:
        return "Critical";
    default:
        return "Invalid";
    }
}

FORGE_API MemoryState getMemoryState(void);
#endif

typedef struct OSInfo
{
    char osName[256];
    char osVersion[256];
    char osDeviceName[256];
} OSInfo;

FORGE_API OSInfo* getOsInfo(void);

//------------------------------------------------------------------------
// PLATFORM LAYER
//------------------------------------------------------------------------

FORGE_API void requestReload(const ReloadDesc* pReloadDesc);

// API functions
FORGE_API void requestShutdown(void);
typedef void (*errorMessagePopupCallbackFn)(void);
FORGE_API void errorMessagePopup(const char* title, const char* msg, WindowHandle* windowHandle, errorMessagePopupCallbackFn callback);

// Custom processing of OS pipe messages
typedef int32_t (*CustomMessageProcessor)(WindowDesc* pWindow, void* msg);
FORGE_API void setCustomMessageProcessor(CustomMessageProcessor proc);
